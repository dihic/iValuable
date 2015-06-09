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

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
#include "RfidProcessor.h"
static bool RfidTimeup = true;	// for power saving
static bool RfidPending = false;
#endif

using namespace std; 
using namespace fastdelegate;

DataProcessor *Processor = NULL;

float WeightArray[SENSOR_NUM];
volatile WeightSet Weights;

#define LOCK_WAIT_SECONDS  5

extern "C" {
	
volatile bool DoorState = false;
volatile bool DoorChangedEvent = false;
	
volatile uint16_t LockCount = 0xffff;
	
static SystemState UnitSystemState = STATE_BOOTUP;
	
void TIMER32_0_IRQHandler()		//1000Hz
{
	static uint32_t hbCount = HeartbeatInterval;
	static uint32_t syncCount = SyncInterval;

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
	static uint32_t RfidCount = 0;
#endif
	
	if ( LPC_TMR32B0->IR & 0x01 )
  {    
		LPC_TMR32B0->IR = 1;			/* clear interrupt flag */

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		if (!RfidTimeup)
			if (RfidCount++>=RFID_TIME_INTERVAL)
			{
				RfidTimeup =true;
				RfidCount = 0;
			}
#endif
		
		//Timeout to re-lock after last unlock
		if (LockCount!=0xffff && IS_LOCKER_ON)
			if (--LockCount == 0)
			{
				LOCKER_OFF;
				LockCount = 0xffff;
			}
		
		if (Connected)
		{
			hbCount = HeartbeatInterval;
			if (Registered && AutoSyncEnable)
			{
				if (syncCount++>=SyncInterval)
				{
					syncCount = 0;
					DataSyncTriggered = true;
				}
			}
		}
		else
		{
			syncCount = SyncInterval;
			if (hbCount++>=HeartbeatInterval)
			{
				hbCount = 0;
				CANEXHeartbeat(UnitSystemState);
			}
		}
	}
}

volatile bool Updating = false;

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
#elif UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		Weights.Total = total - Processor->GetTareWeight() - Processor->GetBoxWeight();
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
				DoorChangedEvent = true;
			}
		}
		else if (IS_DOOR_CLOSED && DoorState)
		{
			NoticeLogic::NoticeCommand = NOTICE_RECOVER;
			DoorState = false;
			DoorChangedEvent = true;
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
	uint16_t q;
	bool result;
	SuppliesInfo info;
//	const uint8_t *data;
	
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
			if (entry->subindex==1 && entry->entrytype_len>=1 && entry->val[0] == 0xff)	//Clear All
			{
				for(i=0;i<SUPPLIES_NUM;++i)
				{
					Processor->RemoveSupplies(i);
					SuppliesDisplay::DeleteString(i);
				}
				*(response->val)=0;
				ForceDisplay = true;
				break;
			}
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
					Processor->GetSuppliesUnit(i, f, d);
					info.Unit = f;
					info.Deviation = d;
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
				uint8_t *ptr = entry->val;
				for(i=0; i<entry->val[0]; ++i)
				{
					memcpy(&id, ptr, sizeof(uint64_t));
					ptr += sizeof(uint64_t);
					memcpy(&q, ptr, sizeof(uint16_t));
					ptr += sizeof(uint16_t);
					Processor->SetQuantityById(id, q);
				}
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
				if (entry->val[0] ==0)
					NoticeLogic::NoticeCommand = NOTICE_CLEAR;
				else if (entry->val[0] ==1)
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
		default:
			break;
		}
//		responseTriggered = true;
		CANEXResponse(res.sourceId, const_cast<CAN_ODENTRY *>(&(res.response)));
}

int PrepareData()
{
	int base = 5+sizeof(float);
	syncBuf[0] = Processor->GetEnable();	//Enabled channel flag
	syncBuf[1] = 0; //Channel count
	syncBuf[2] = IS_LOCKER_ENABLE & IS_DOOR_OPEN;
	syncBuf[3] = Weights.AllStable;
	syncBuf[4] = (Weights.Total>Weights.Min && Weights.Total<Weights.Max);	//If current total weight fits inventory expected
	memcpy(syncBuf+5, const_cast<float *>(&Weights.Delta), sizeof(float));
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
	float unit,dev;
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
			continue;
		++syncBuf[1];
		syncBuf[base++] = i;
		syncBuf[base++] = SensorArray::Instance().IsStable(i);
		syncBuf[base++] = SensorArray::Instance().GetStatus(i);
		syncBuf[base++] = (Processor->GetSuppliesUnit(i, unit, dev)) ? (uint8_t)(WeightArray[i]/unit):0;	//Quantity temp
		memcpy(syncBuf+base, reinterpret_cast<void *>(&WeightArray[i]), sizeof(float));
		base += sizeof(float);
	}
#else
	memcpy(syncBuf+base, const_cast<float *>(&Weights.Total), sizeof(float));
	base += sizeof(float);
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
			continue;
		++syncBuf[1];
		syncBuf[base++] = i;
		syncBuf[base++] = SensorArray::Instance().GetStatus(i);
	}
#endif
	return base;
}

void CanexSyncTrigger(uint16_t index, uint8_t mode)
{	
	switch(index)
	{
		case SYNC_DATA:
			AutoSyncEnable = (mode!=0);
			if (mode == 0)
				DataSyncTriggered = true;
			break;
		case SYNC_LIVE:
			Connected=!Connected;
			if (Connected)
			{
				Registered = true;
				AutoSyncEnable = (mode!=0);
			}
			break;
		case SYNC_ISP:
			ReinvokeISP(1);
		default:
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

void ReportDoorState()
{
	syncBuf[0] = DoorState;
	syncEntry.index = SYNC_DOOR;
	syncEntry.subindex = 0;
	syncEntry.val = syncBuf;
	syncEntry.entrytype_len = 1;
	CANEXBroadcast(&syncEntry);
}


#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID

uint8_t RfidBytes[9];

void ReportRfid()
{
	syncEntry.index = SYNC_RFID;
	syncEntry.subindex = 0;
	syncEntry.val = RfidBytes;
	syncEntry.entrytype_len = 9;
	CANEXBroadcast(&syncEntry);
}

void RfidChanged(uint8_t cardType, const uint8_t *id)
{
	RfidBytes[0] = cardType;
	memcpy(RfidBytes+1, id, 8);
	
	if (cardType==0)
	{
		//TBD: Remove current inventory config
	}
	if (Connected && Registered)
	{
		RfidPending = false;
		ReportRfid();
	}
	else
		RfidPending = true;
}
#endif

int main()
{
	SystemCoreClockUpdate();
	SystemSetup();
	
	//Canbus Init
	CANInit(500);
	CANEXReceiverEvent = CanexReceived;
	CANTEXTriggerSyncEvent = CanexSyncTrigger;
	init_timer32(0,TIME_INTERVAL(1000));	//	1000Hz
	DELAY(10);
	enable_timer32(0);
	UnitSystemState = STATE_PREOPERATIONAL;
	
	Processor = DataProcessor::InstancePtr();
	SuppliesDisplay::Init();
	
	//FRAM Init
	DELAY(100000); 	//wait 100ms for voltage stable
	uint8_t firstUse = FRAM::Init();
	//firstUse=1; 	//Force to format FRAM for debug
	if (firstUse)
		FRAM::WriteMemory(0x04, (uint8_t *)(DataProcessor::MemBuffer+4), MEM_BUFSIZE-4);		//Write default values
	else
		FRAM::ReadMemory(0, (uint8_t *)DataProcessor::MemBuffer, MEM_BUFSIZE);		//Read all values from NV memory
	
	DataProcessor::WriteNV.bind(&FRAM::WriteMemory);
	SensorArray::Instance(Processor->GetConfig());

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID	
	RfidProcessor::RfidChangedEvent.bind(&RfidChanged);
#endif
	
	UARTInit(230400);
	Display::OnAckReciever.bind(&AckReciever);
	
	UpdateWeight();
	
	init_timer32(1,TIME_INTERVAL(100));		//	100Hz
	DELAY(10);
	enable_timer32(1);
	
	ForceDisplay = true;
	
	UnitSystemState = STATE_OPERATIONAL;
	
	while(1)
	{
		Display::UARTProcessor();
		
//		if (responseTriggered)
//		{
//			CANEXResponse(res.sourceId, const_cast<CAN_ODENTRY *>(&(res.response)));
//			responseTriggered = false;
//		}
		
		NoticeLogic::NoticeUpdate();
		
		if (DataSyncTriggered)
		{
			while(Updating)
				__nop();
			syncEntry.index = SYNC_DATA;
			syncEntry.subindex = 0;
			syncEntry.val = syncBuf;
			syncEntry.entrytype_len = PrepareData();
			CANEXBroadcast(&syncEntry);
			DataSyncTriggered = false;
		}
		
		if (DoorChangedEvent)
		{
			ReportDoorState();
			DoorChangedEvent = false;
		}
		
#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		if (RfidTimeup)
		{
			RfidProcessor::UpdateRfid();
			RfidTimeup = false;
		}
		if (Connected && Registered && RfidPending)
		{
			ReportRfid();
			RfidPending = false;
		}
#endif
	}
}
