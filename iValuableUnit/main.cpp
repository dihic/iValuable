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

using namespace std; 
using namespace fastdelegate;

#define INTRUSION_EXTENSION_TIME 500   //500ms

#define MEM_BUFSIZE 0x200

//#define NVDATA_ADDR 0x10
//#define NVINFO_ADDR 0x80

//#define NVDATA_BASE 0x30

#define SYNC_SENSOR     0x0101
#define SYNC_DATA				0x0100
#define SYNC_LIVE				0x01ff

#define OP_SET_ZERO 	0x00fe
#define OP_RAMPS			0x8000
#define OP_AUTO_RAMPS	0x8007
#define OP_UNIT     	0x8001
#define OP_CAL_UNIT   0x8002
#define OP_CONFIG     0x8003
#define OP_AD					0x8006
#define OP_ENABLE			0x8008


#define A_POS_X		5
#define A_POS_Y		170
#define B_POS_X 	170
#define B_POS_Y		170
#define DIGIT_W		150
#define DIGIT_H		70
#define DIGIT_SCALE 2.0f
#define AUX_SCALE   1.5f

#define TEXT_POS_Y	5
#define A_NUM_OFFSET	70
#define B_NUM_OFFSET	60	

bool boot = false;

__align(16) volatile uint8_t MemBuffer[MEM_BUFSIZE];

struct CanResponse
{
	uint16_t sourceId;
	CAN_ODENTRY response;
	uint8_t result;
};

volatile CanResponse res;

volatile bool syncTriggered = false;
volatile bool responseTriggered = false;
volatile CAN_ODENTRY syncEntry;

volatile bool Connected = true;			
volatile bool Registered = false;		// Registered by host
volatile bool ForceSync = false;
volatile bool Gotcha = true;

float BackupWeight=0;
int16_t BackupValue[SENSOR_NUM];
float WeightArray[SENSOR_NUM] = { 0, 0, 0, 0, 0, 0};	
int32_t CurrentAD[SENSOR_NUM];

#define ADDR_SENSOR_ENABLE	0x00F
#define ADDR_CAL_WEIGHT			0x010
#define ADDR_TEMP						0x014
#define ADDR_CONFIG  				0x020
#define ADDR_SCALE  				0x040
#define ADDR_INFO						0x0C0

ScaleAttribute *pConfig;
ScaleInfo *pScales[6];
SuppliesInfo *pSupplies[10];
float *pCalWeight;
float *pCurrentTemp;
uint8_t *pSensorEnable;

void DataInit()
{
	memset((void *)MemBuffer, 0, MEM_BUFSIZE);
	for(int i=0; i<6; ++i)
	{
		ScaleBasic *sb = (ScaleBasic *)(MemBuffer + ADDR_SCALE+ i*sizeof(ScaleBasic));
		pScales[i] = new ScaleInfo(i, sb, ADDR_SCALE+ i*sizeof(ScaleBasic));
	}
	for(int i=0; i<10; ++i)
	{
		pSupplies[i] = (SuppliesInfo *)(MemBuffer + ADDR_INFO+ i*sizeof(SuppliesInfo));
	}
	
	pConfig = (ScaleAttribute *)(MemBuffer + ADDR_CONFIG);
	//Default values for YZC-133
	pConfig->Sensitivity = 1.0f;
	pConfig->TempDrift = 0.0002f;
	pConfig->ZeroRange = 0.1f;
	pConfig->SafeOverload = 1.2f;
	pConfig->MaxOverload = 1.5f;
	
	pCalWeight = (float *)(MemBuffer + ADDR_CAL_WEIGHT);
	pCurrentTemp = (float *)(MemBuffer + ADDR_TEMP);
	
	pSensorEnable = (uint8_t *)(MemBuffer + ADDR_SENSOR_ENABLE);
	*pSensorEnable = 0x3f;	//Default all enable for channel 0-5
}

extern "C" {

volatile uint8_t lastState=0;	//0:intrusion 1:stable
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

//Parameter flags stands for enable to calibrate sensors bitwise
void CalibrateSensors(uint8_t flags)
{
	for(uint8_t i=0;i<6;++i)
	{
		if ((flags & (1<<i)) == 0)
			continue;
		pScales[i]->Calibrate(*pCalWeight);
	}
}

uint8_t Updating=0;

void UpdateWeight()
{
	float weight=0;
	float zeroWeight=0 , zeroWeightTemp;
	
	static uint8_t extend = 0;
	
	Updating=1;
	
	bool fail = false;
	
	SensorArray &sensorArray = SensorArray::Instance();
	
	for(int i=0; i<6; ++i)
	{
		//Skip disabled sensors
		if ((*pSensorEnable & (1<<i)) == 0)
		{
			CurrentAD[i] = 0;
			continue;
		}
//	
//		//if (sensorArray.pSensors[i]->CurrentData)
		if (!sensorArray.IsStable(i))
			continue;
		CurrentAD[i] = sensorArray.GetCurrent(i);
		
		//Calculation...
		if (abs(pScales[i]->GetBasic()->Ramp)>0.000001f)
			WeightArray[i] = (CurrentAD[i] - pScales[i]->GetBasic()->Tare)/pScales[i]->GetBasic()->Ramp;
		else
			WeightArray[i] = 0;
//		//diff=*pCurrentValue[i] -*pZero[i];
//		//if (*pRamp[i]>0.0f && *pRamp[i]<10000.0f)
//		if (abs(*pRamp[i])>0.000001f)
//		{
//			WeightArray[i] = (*pCurrentValue[i]-*pZero[i])/(*pRamp[i]);
//			zeroWeight += (*pZero[i])/(*pRamp[i]);
//			weight += (*pCurrentValue[i])/(*pRamp[i]);
//		}
//		else
//		{
//			WeightArray[i] = 0;
//			fail = true;
//		}
	}
	
	if (fail)
	{
		Updating = 0;
		return;
	}
	
//	for(int i=0;i<4;++i)
//		*pWeightRatio[i] = (zeroWeight!=0)?weightArray[i]/zeroWeight:0;
	
	//Determine whether intrusion happen or not
	//zeroWeightTemp = weight - BackupWeight;
//zeroWeightTemp = weight - (BackupQuantity * (*pUnit) + zeroWeight);
//lastState = abs(zeroWeightTemp) < (*pDeviation) * (*pUnit);
	
	//Intrusion Detected, calculate quantity

		if (extend == 0)
			extend = 1;
		else
			if (++extend > 5)
				extend = 0;
	
	Updating=0;
}

void TIMER32_1_IRQHandler()		//100Hz
{
	static int counter=0;
	if ( LPC_TMR32B1->IR & 0x01 )
  {    
		LPC_TMR32B1->IR = 1;			/* clear interrupt flag */
		SensorArray::Instance().Refresh();
	
		if (++counter >= 25)
		{
			counter = 0;
			UpdateWeight(); 
		}
	}
}

void CanexReceived(uint16_t sourceId, CAN_ODENTRY *entry)
{
	uint8_t i;
	uint8_t buf[0x20];
	
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
			if (i<6 && (*pSensorEnable & (1<<i)))
			{
				//0 for set Zero
				//1 for set Tare
				if (entry->val[0]==0)
					pScales[i]->SetZero();
				else
					pScales[i]->SetTare();
				*(response->val)=0;
			}
//			reset_timer32(1);
//			enable_timer32(1);
			break;
//		case OP_RAWDATA:
//			response->entrytype_len=0x20;
//			response->val=buf;
//			memcpy(buf,pZero[0],0x10);
//			memcpy(buf+0x10,pCurrentValue[0],0x10);
//			break;
		case OP_AUTO_RAMPS:
			CalibrateSensors(entry->val[0]);
			*(response->val)=0;
			break;
		case OP_RAMPS: //R/W coeff
			if (entry->subindex==0)
			{
				response->entrytype_len = sizeof(float);
				response->val = (uint8_t *)(&pScales[entry->val[0]]->GetBasic()->Ramp);
			}
			else
			{
				if (entry->val[0]>=6)
					break;
				float ramp;
				memcpy(&ramp, entry->val+1, sizeof(float));
				pScales[entry->val[0]]->SetRamp(ramp);
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
				response->val = (uint8_t *)pCalWeight;
			}
			else
			{
				if (entry->entrytype_len<sizeof(float))
					break;
				memcpy(pCalWeight, entry->val, sizeof(float));
				FRAM::WriteMemory(ADDR_CAL_WEIGHT, (uint8_t *)pCalWeight, sizeof(float));
				*(response->val)=0;
			}
			break;
		case OP_CONFIG:
			if (entry->subindex==0)
			{
				response->entrytype_len = 20;
				response->val=(uint8_t *)pConfig;
			}
			else
			{
				if (entry->entrytype_len<20)
					break;
				memcpy(reinterpret_cast<void *>(&pConfig->Sensitivity), entry->val, 4);
				memcpy(reinterpret_cast<void *>(&pConfig->TempDrift), entry->val+4, 4);
				memcpy(reinterpret_cast<void *>(&pConfig->ZeroRange), entry->val+8, 4);
				memcpy(reinterpret_cast<void *>(&pConfig->SafeOverload), entry->val+12, 4);
				memcpy(reinterpret_cast<void *>(&pConfig->MaxOverload), entry->val+16, 4);
				FRAM::WriteMemory(ADDR_CONFIG, (uint8_t *)pConfig, sizeof(ScaleAttribute));
				SensorArray::Instance().SetRange(pConfig);
				*(response->val)=0;
			}
			break;
		case OP_ENABLE:
			if (entry->subindex==0)
			{
				response->entrytype_len = 1;
				response->val=(uint8_t *)pSensorEnable;
			}
			else
			{
				if (entry->entrytype_len<1)
					break;
				*pSensorEnable = entry->val[0];
				FRAM::WriteMemory(ADDR_SENSOR_ENABLE, pSensorEnable, sizeof(ScaleAttribute));
				*(response->val)=0;
			}
			break;
		}
//	CANEXResponse(sourceId,response);
		responseTriggered = true;
}

uint8_t syncBuf[0x30];

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
//		case SYNC_RFID:
//			ForceSync = true;
//			break;
		case SYNC_SENSOR:
			if (Connected)
			{
				syncEntry.entrytype_len=0x20;
//				memcpy(syncBuf,pZero[0],0x10);
//				memcpy(syncBuf+0x10,pCurrentValue[0],0x10);
				syncTriggered = true;
			}
			break;
		case SYNC_DATA:
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
		boot = true;
//		NeedUpdateDisplay = true;
	}
}


int main()
{
	SystemCoreClockUpdate();
	SystemSetup();
	
	DataInit();
	
	DELAY(100000); 	//wait 100ms for voltage stable
	uint8_t firstUse = FRAM::Init();
	//firstUse=1; 	//Force to format
	if (firstUse)
		FRAM::WriteMemory(0x04, (uint8_t *)(MemBuffer+4), MEM_BUFSIZE-4);		//Write default values
	else
		FRAM::ReadMemory(0, (uint8_t *)MemBuffer, MEM_BUFSIZE);		//Read all values from NV memory
	
	ScaleInfo::WriteNV.bind(&FRAM::WriteMemory);
	SensorArray::Instance().SetRange(pConfig);
	
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
			CANEXBroadcast(const_cast<CAN_ODENTRY *>(&syncEntry));
			syncTriggered = false;
		}
		bool displayChanged = false;
		
	}
//	SensorArray::Release();
}
