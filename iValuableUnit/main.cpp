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
#include "Display.h"
#include "ScaleInfo.h"

#include "System.h"

#include "FastDelegate.h"

#include "main.h"
#include "DataProcessor.h"

using namespace std; 
using namespace fastdelegate;

//#define INTRUSION_EXTENSION_TIME 500   //500ms

DataProcessor *Processor;

volatile CanResponse res;

volatile bool syncTriggered = false;
volatile bool responseTriggered = false;
volatile CAN_ODENTRY syncEntry;

volatile bool Connected = true;			
volatile bool Registered = false;		// Registered by host
volatile bool ForceSync = false;
volatile bool Gotcha = true;


float WeightArray[SENSOR_NUM];

extern "C" {

//volatile uint8_t lastState=0;	//0:intrusion 1:stable
volatile uint32_t TickCount = 0;
	
void TIMER32_0_IRQHandler()
{
	static uint32_t counter1=0;
//	static uint32_t counter2=0;
//	uint8_t currentState;
	
	if ( LPC_TMR32B0->IR & 0x01 )
  {    
		LPC_TMR32B0->IR = 1;			/* clear interrupt flag */
		
		++TickCount;
		
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
			continue;

		WeightArray[i] = Processor->CalculateWeight(i, sensorArray.GetCurrent(i));
	}

	Updating = false;
}

void TIMER32_1_IRQHandler()		//100Hz
{
	static int counter=0;
	if ( LPC_TMR32B1->IR & 0x01 )
  {    
		LPC_TMR32B1->IR = 1;			/* clear interrupt flag */
		SensorArray::Instance().Refresh();
	
		if (++counter >= 20)
		{
			counter = 0;
			UpdateWeight(); 
		}
	}
}

void CanexReceived(uint16_t sourceId, CAN_ODENTRY *entry)
{
	uint8_t i;
	float f;
	
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
//			disable_timer32(1);
			i = entry->val[1];
			if (Processor->SensorEnable(i))
			{
				//0 for set Zero, 1 for set Tare
				Processor->SetZero(i, entry->val[0]!=0);
				*(response->val)=0;
			}
//			reset_timer32(1);
//			enable_timer32(1);
			break;
		case OP_RAWDATA:
			response->val = const_cast<uint8_t *>(res.buffer);
			response->entrytype_len= Processor->PrepareRaw(response->val);
			break;
		case OP_AUTO_RAMPS:
			Processor->CalibrateSensors(entry->val[0]);
			*(response->val)=0;
			break;
		case OP_RAMPS: //R/W coeff
			if (entry->subindex==0)
			{
				if (entry->val[0]>=SENSOR_NUM)
					break;
				response->entrytype_len = sizeof(float);
				response->val = const_cast<uint8_t *>(res.buffer);
				f = Processor->GetRamp(entry->val[0]);
				memcpy(response->val, &f, sizeof(float));
			}
			else
			{
				if (entry->val[0]>=SENSOR_NUM)
					break;
				memcpy(&f, entry->val+1, sizeof(float));
				Processor->SetRamp(entry->val[0], f);
				*(response->val)=0;
			}
			break;
		case OP_TEMP:
			if (entry->entrytype_len<4)
				break;
			if (entry->subindex!=0)
			{
				memcpy(&f, entry->val, sizeof(float));
				Processor->SetTemperature(f);
				*(response->val)=0;
			}
			break;
		case OP_UNIT:
//			if (entry->subindex==0)
//			{
//				response->entrytype_len = 25+(*pMedNameLen);
//				response->val = pMedGuid;
//			}
//			else
//			{
//				if (entry->entrytype_len<25)
//					break;
//				memcpy(pMedGuid,entry->val,16);
//				memcpy(pUnit,entry->val+16,sizeof(float));
//				memcpy(pDeviation,entry->val+20,sizeof(float));
//				*pMedNameLen = entry->val[24] & 0x7f;
//				memcpy(pMedName,entry->val+25,*pMedNameLen);
//				FRAM::WriteMemory(NVINFO_ADDR,(uint8_t *)(MemBuffer+NVDATA_BASE+ADDR_MED_GUID),25+(*pMedNameLen));
//				UpdateScaleRange();
//				//Update Display
//				NeedUpdateDisplay = true;
//				*(response->val)=0;
//			}
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
		}
		responseTriggered = true;
}

uint8_t syncBuf[0x100];

int PrepareData()
{
	syncBuf[0] = 0;
	int base = 1;
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (!Processor->SensorEnable(i))
			continue;
		++syncBuf[0];
		syncBuf[base] = i;
		syncBuf[base+1] = SensorArray::Instance().IsStable(i);
		syncBuf[base+2] = SensorArray::Instance().GetStatus(i);
		memcpy(syncBuf+base+3, reinterpret_cast<void *>(&WeightArray[i]), sizeof(float));
		base += 3+sizeof(float);
	}
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
//		case SYNC_GOTCHA:
//			Gotcha = true;
//			break;
		case SYNC_DATA:
			ForceSync = true;
			if (Connected)
				syncTriggered = true;
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
	if (len>0 && ack[0] == 0xff)
	{
//		boot = true;
//		NeedUpdateDisplay = true;
	}
}


int main()
{
	SystemCoreClockUpdate();
	SystemSetup();
	
	Processor = DataProcessor::InstancePtr();
	
	DELAY(100000); 	//wait 100ms for voltage stable
	uint8_t firstUse = FRAM::Init();
	//firstUse=1; 	//Force to format
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
	
	while(1)
	{
		Display::UARTProcessor();
		
		if (responseTriggered)
		{
			CANEXResponse(res.sourceId, const_cast<CAN_ODENTRY *>(&(res.response)));
			responseTriggered = false;
		}
		
		if (syncTriggered)
		{
			while(Updating)
				__nop();
			syncEntry.entrytype_len = PrepareData();
			CANEXBroadcast(const_cast<CAN_ODENTRY *>(&syncEntry));
			syncTriggered = false;
		}
//		bool displayChanged = false;
		
	}
//	SensorArray::Release();
}
