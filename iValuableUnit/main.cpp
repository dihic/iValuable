#include <LPC11xx.h>
#include <cstring>
#include <cmath>
#include "gpio.h"
#include "uart.h"
#include "timer32.h"
#include "ssp.h"
#include "SensorArray.h"
#include "canonchip.h"
#include "fram.h"
#include "iap.h"
#include "Display.h"
#include "ScaleInfo.h"

#include "System.h"

#include "FastDelegate.h"

#include "main.h"
#include "DataProcessor.h"
#include "SuppliesDisplay.h"
#include "NoticeLogic.h"

#define FW_VERSION_INDEX				0x01
#define FW_VERSION_SUBINDEX			0x00

using namespace std; 
using namespace fastdelegate;

DataProcessor *Processor = NULL;

float WeightArray[SENSOR_NUM];
volatile WeightSet Weights;

#define LOCK_WAIT_SECONDS  5

extern "C" {

volatile uint32_t TickCount = 0;	
volatile uint16_t LockCount = 0xffff;
	
volatile bool DoorState = false;
//volatile bool DoorClosedEvent = false;
	
void TIMER32_0_IRQHandler()		//1000Hz
{
	static uint32_t counter1=0;
	
	if ( LPC_TMR32B0->IR & 0x01 )
  {    
		LPC_TMR32B0->IR = 1;			/* clear interrupt flag */
		
		++TickCount;
		
		//Timeout to re-lock after last unlock
		if (LockCount!=0xffff && IS_LOCKER_ON)
		{
			if (--LockCount == 0)
			{
				LOCKER_OFF;
				LockCount = 0xffff;
			}
		}
		
		if (counter1++>=HeartbeatInterval)
		{
			counter1=0;
			if (Connected)
			{
				if (Registered && !Gotcha)
					syncTriggered = true;
			}
			else
				CANEXHeartbeat(STATE_OPERATIONAL);
		}
	}
}

volatile bool Updating=0;

void UpdateWeight()
{
	Updating = true;

	SensorArray &sensorArray = SensorArray::Instance();
	
	float total = 0;
	bool allStable = true;
	
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
		{
			WeightArray[i] = 0;
			continue;
		}

		//Skip unstable sensors
		if (!sensorArray.IsStable(i))
		{
			allStable = false;
			continue;
		}

		total += WeightArray[i] = Processor->CalculateWeight(i, sensorArray.GetCurrent(i));
	}
	
	Weights.AllStable = allStable;
	if (allStable)
	{
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
		Weights.Total = total;
#else
		Weights.Total = total - Processor->GetTareWeight();
#endif
		Weights.Delta = Weights.Total - Weights.Inventory;
	}
	
	Updating = false;
}

void TIMER32_1_IRQHandler()		//100Hz
{
	static int counter=0;
	static int cd = 0;
	static bool refresh = false;
	
	if ( LPC_TMR32B1->IR & 0x01 )
  {    
		LPC_TMR32B1->IR = 1;			/* clear interrupt flag */
		SensorArray::Instance().Refresh();
	
		if (++counter >= 20)
		{
			counter = 0;
			UpdateWeight(); 
		}
		
		//Door and lock management
		if (IS_LOCKER_ON)
		{
			if (LockCount == 0xffff)
				LockCount = LOCK_WAIT_SECONDS*1000;
			if (IS_DOOR_OPEN)
			{
				LOCKER_OFF;
				LockCount = 0xffff;
				DoorState = true;
				//syncTriggered = true;
			}
		}
		else if (IS_DOOR_CLOSED && DoorState)
		{
			NoticeLogic::NoticeCommand = NOTICE_RECOVER;
			DoorState = false;
//			DoorClosedEvent = true;
			//syncTriggered = true;
		}
		
		//Logic of display into pages 
		if (Processor)
		{
			if (ForceDisplay)
			{
				cd = 0;
				ForceDisplay = false;
				refresh = Processor->UpdateDisplay(true);
			}
			else if (refresh && ++cd==300) // Interval 3s
			{
				cd = 0;
				refresh = Processor->UpdateDisplay();
			}
		}
	}
}

void CanexReceived(uint16_t sourceId, CAN_ODENTRY *entry)
{
	uint8_t i;
	float f, d;
	uint64_t id;
	bool result;
	SuppliesInfo info;
	const uint8_t *data;
	
	CAN_ODENTRY *response = const_cast<CAN_ODENTRY *>(&(res.response));

	res.sourceId = sourceId;
	res.result=0xff;
	
	response->val = const_cast<uint8_t *>(&(res.result));
	response->index = entry->index;
	response->subindex = entry->subindex;
	response->entrytype_len = 1;
	
	switch (entry->index)
	{
		case 0:	//system config
			break;
		case OP_SET_ZERO:
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
			for(i=0;i<SENSOR_NUM;++i)
				if (Processor->SensorEnable(i) && ((1<<i) & entry->val[0]))
				{
					//0 for set Zero, 1 for set Tare
					Processor->SetZero(i, entry->val[1]!=0);
					*(response->val)=0;
				}
#else
			if (entry->val[1]==0)		//0 for set Zero, 1 for set Tare
			{
				for(i=0;i<SENSOR_NUM;++i)
					if (Processor->SensorEnable(i) && ((1<<i) & entry->val[0]))
						Processor->SetZero(i, false);
				*(response->val)=0;
			}
			else
			{
				//Set tare sum, so dump channel value
				Processor->SetZero(0xff, true);
				*(response->val)=0;
			}
#endif
			break;
		case OP_RAWDATA:
			response->val = const_cast<uint8_t *>(res.buffer);
			response->entrytype_len= Processor->PrepareRaw(response->val);
			break;
		case OP_AUTO_RAMPS:
			if (entry->entrytype_len<1)
				break;
			Processor->CalibrateSensors(entry->val[0]);
			*(response->val)=0;
			break;
		case OP_RAMPS: //R/W coeff
			if (entry->subindex==0)
			{
				response->entrytype_len = 1;
				response->val = const_cast<uint8_t *>(res.buffer);
				response->val[0] = 0;
				uint8_t *ptr = response->val+1;
				for(i=0;i<SENSOR_NUM;++i)
				{
					if (!Processor->SensorEnable(i))
						continue;
					++response->val[0];
					ptr[0] = i;
					f =  Processor->GetRamp(entry->val[0]);
					memcpy(ptr+1, &f, sizeof(float));
					ptr += sizeof(float)+1;
					response->entrytype_len += sizeof(float)+1;
				}
			}
			else
			{
				if (entry->entrytype_len<1)
					break;
				if (!Processor->SensorEnable(i))
					break;
				memcpy(&f, entry->val+1, sizeof(float));
				Processor->SetRamp(entry->val[0], f);
				*(response->val)=0;
			}
			break;
		case OP_TEMP:
			if (entry->entrytype_len<4)
				break;
			if (entry->subindex==1)
			{
				memcpy(&f, entry->val, sizeof(float));
				Processor->SetTemperature(f);
				*(response->val)=0;
			}
			break;
		case OP_UNIT_INFO:
			if (entry->entrytype_len<1 || entry->val[0]>=SUPPLIES_NUM)
					break;
			if (entry->subindex==0)	//Read
			{
				response->entrytype_len = 1;
				response->val = const_cast<uint8_t *>(res.buffer);
				response->val[0] = 0;
				uint8_t *ptr = response->val+1;
				for(i=0;i<SUPPLIES_NUM;++i)
				{
					info.Uid = Processor->GetSuppliesId(i);
					if (info.Uid==0)
						continue;
					++response->val[0];
					Processor->GetSuppliesUnit(i, info.Unit, info.Deviation);
					ptr[0] = i; 
					memcpy(ptr+1, &info, sizeof(SuppliesInfo));
					ptr += sizeof(SuppliesInfo)+1;
					response->entrytype_len += sizeof(SuppliesInfo)+1;
				}
			}
			else if (entry->subindex==1)	//Write
			{
				//Size of info
				i = sizeof(SuppliesInfo);
				memcpy(&info, entry->val+1, i);
				Processor->SetSupplies(entry->val[0], info);
				Processor->SetQuantity(entry->val[0], 0);
				SuppliesDisplay::ModifyString(entry->val[0], entry->val+i+2, entry->val[i+1]);
				*(response->val)=0;
				ForceDisplay = true;
			}
			else	//Remove above 2
			{
				Processor->RemoveSupplies(entry->val[0]);
				SuppliesDisplay::DeleteString(entry->val[0]);
				*(response->val)=0;
				ForceDisplay = true;
			}
			break;
		case OP_QUANTITY:
			if (entry->subindex==0)	//Read quantities
			{
				response->entrytype_len = 1;
				response->val = const_cast<uint8_t *>(res.buffer);
				response->val[0] = 0;
				uint8_t *ptr = response->val+1;
				for(i=0;i<SUPPLIES_NUM;++i)
				{
					id = Processor->GetSuppliesId(i);
					if (id==0)
						continue;
					++response->val[0];
					memcpy(ptr, &id, sizeof(uint64_t));
					ptr += sizeof(uint64_t);
					ptr[0] = Processor->GetQuantity(i);
					++ptr;
					response->entrytype_len += sizeof(uint64_t)+1;
				}
			}
			else	//Write quantities
			{
				if (entry->val[0]==0 || entry->val[0]>SUPPLIES_NUM)
					break;
				for(i=0;i<entry->val[0];++i)
					Processor->SetQuantity(entry->val[(i<<1)+1], entry->val[(i<<1)+2]);
				Weights.Inventory = Processor->CalculateWeight(f, d);
				Weights.Min = f;
				Weights.Max = d;
				Weights.Delta = Weights.Total - Weights.Inventory;
				*(response->val)=0;
			}
			break;
		case OP_CAL_UNIT:
			if (entry->subindex==0)
			{
				response->entrytype_len = sizeof(float);
				response->val = const_cast<uint8_t *>(res.buffer);
				f = Processor->GetCalWeight();
				memcpy(response->val, &f, sizeof(float));
			}
			else
			{
				if (entry->entrytype_len<sizeof(float))
					break;
				Processor->SetCalWeight(entry->val);
				*(response->val)=0;
			}
			break;
		case OP_CONFIG:
			if (entry->subindex==0)
			{
				response->entrytype_len = 20;
				response->val = reinterpret_cast<uint8_t *>(Processor->GetConfig());
			}
			else
			{
				if (entry->entrytype_len<20)
					break;
				Processor->SetConfig(entry->val);
				SensorArray::Instance().SetRange(Processor->GetConfig());
				*(response->val)=0;
			}
			break;
		case OP_ENABLE:
			if (entry->subindex==0)
			{
				response->entrytype_len = 1;
				*(response->val) = Processor->GetEnable();
			}
			else
			{
				if (entry->entrytype_len<1)
					break;
				Processor->SetEnable(entry->val[0]);
				*(response->val)=0;
			}
			break;
		case OP_NOTICE:
			if (entry->subindex==1)
			{
				NoticeLogic::NoticeCommand = NOTICE_GUIDE;
				*(response->val)=0;
			}
			break;
		case OP_LOCKER:
			if (entry->subindex==0 && IS_LOCKER_ENABLE)
			{
				if (entry->val[0])
					LOCKER_ON;
				else
					LOCKER_OFF;
				*(response->val)=0;
			}
			break;
		case OP_QUERY:
			if (entry->subindex==1 && entry->entrytype_len>5)
			{
				memcpy(&id, &entry->val, sizeof(uint64_t));
				result = Processor->FindSuppliesId(id, i);
				if (result)
				{
					if (entry->val[sizeof(uint64_t)])
						NoticeLogic::NoticeCommand = NOTICE_GUIDE;
					*(response->val) = i;
				}
			}
			break;
		case OP_VERSION:
			if (entry->subindex==0)
			{
				response->entrytype_len = 2;
				response->val = const_cast<uint8_t *>(res.buffer);
				response->val[0] = FW_VERSION_INDEX;
				response->val[1] = FW_VERSION_SUBINDEX;
			}
			break;
		default:
			break;
		}
//		responseTriggered = true;
		CANEXResponse(res.sourceId, const_cast<CAN_ODENTRY *>(&(res.response)));
}

uint8_t syncBuf[0x100];

int PrepareData()
{
	int base = 4+sizeof(float);
	syncBuf[0] = IS_LOCKER_ENABLE & IS_DOOR_OPEN;
	syncBuf[1] = Processor->GetEnable();	//Enabled channel flag
	syncBuf[2] = 0; //Channel count
	syncBuf[3] = (Weights.Total>Weights.Min && Weights.Total<Weights.Max);	//If current total weight fits inventory expected
	memcpy(syncBuf+4, const_cast<float *>(&Weights.Delta), sizeof(float));
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
	float unit,dev;
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
			continue;
		++syncBuf[2];
		syncBuf[base++] = i;
		syncBuf[base++] = SensorArray::Instance().IsStable(i);
		syncBuf[base++] = SensorArray::Instance().GetStatus(i);
		syncBuf[base++] = (Processor->GetSuppliesUnit(i, unit, dev)) ? (uint8_t)(WeightArray[i]/unit):0;	//Quantity temp
		memcpy(syncBuf+base, reinterpret_cast<void *>(&WeightArray[i]), sizeof(float));
		base += sizeof(float);
	}
#else
	syncBuf[base] = Weights.AllStable;
	memcpy(syncBuf+base+1, const_cast<float *>(&Weights.Total), sizeof(float));
	base += sizeof(float)+1;
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
			continue;
		++syncBuf[2];
		syncBuf[base++] = i;
		syncBuf[base++] = SensorArray::Instance().GetStatus(i);
	}
#endif
	return base;
}

void CanexSyncTrigger(uint16_t index,uint8_t mode)
{	
	if (mode!=0)
		return;
	if (syncTriggered)
		return;
	
	syncEntry.index=index;
	syncEntry.subindex=0;
	syncEntry.val=syncBuf;

	switch(index)
	{
		case SYNC_GOTCHA:
			Gotcha = true;
			break;
		case SYNC_DATA:
			Gotcha = false;
			break;
		case SYNC_LIVE:
			Connected=!Connected;
			if (Connected)
				Registered = true;
			break;
	}
}

}

void AckReciever(uint8_t *ack, uint8_t len)
{
//	if (len>0 && ack[0] == 0xff)
//	{
//	}
}




int main()
{
	SystemCoreClockUpdate();
	SystemSetup();
	
	Processor = DataProcessor::InstancePtr();
	SuppliesDisplay::Init();
	
	DELAY(100000); 	//wait 100ms for voltage stable
	uint8_t firstUse = FRAM::Init();
	//firstUse=1; 	//Force to format FRAM for debug
	if (firstUse)
		FRAM::WriteMemory(0x04, (uint8_t *)(DataProcessor::MemBuffer+4), MEM_BUFSIZE-4);		//Write default values
	else
		FRAM::ReadMemory(0, (uint8_t *)DataProcessor::MemBuffer, MEM_BUFSIZE);		//Read all values from NV memory
	
	DataProcessor::WriteNV.bind(&FRAM::WriteMemory);
	SensorArray::Instance(Processor->GetConfig());
	
	UARTInit(230400);
	Display::OnAckReciever.bind(&AckReciever);
	
	CANInit(500);
	CANEXReceiverEvent = CanexReceived;
	CANTEXTriggerSyncEvent = CanexSyncTrigger;
	
	UpdateWeight();
	
	init_timer32(0,TIME_INTERVAL(1000));	//	1000Hz
	init_timer32(1,TIME_INTERVAL(100));		//	100Hz
	DELAY(10);
	enable_timer32(0);
	enable_timer32(1);
	
	ForceDisplay = true;
	
	while(1)
	{
		Display::UARTProcessor();
		
//		if (responseTriggered)
//		{
//			CANEXResponse(res.sourceId, const_cast<CAN_ODENTRY *>(&(res.response)));
//			responseTriggered = false;
//		}
		
		NoticeLogic::NoticeUpdate();
		
		if (syncTriggered)
		{
			while(Updating)
				__nop();
			syncEntry.entrytype_len = PrepareData();
			CANEXBroadcast(const_cast<CAN_ODENTRY *>(&syncEntry));
			syncTriggered = false;
		}
	}
}
