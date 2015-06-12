#include "UnitType.h"
#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID

#include "RfidProcessor.h"
#include "trf796x.h"
#include "iso15693.h"
#include "delay.h"
#include "DataProcessor.h"

#include <cstring>

using namespace std;
using namespace fastdelegate;

uint8_t rfid_buf[300];
volatile uint8_t i_reg 							= 0x01;						// interrupt register
volatile uint8_t irq_flag 					= 0x00;
volatile uint8_t rx_error_flag 			= 0x00;
volatile uint8_t rxtx_state 				= 1;							// used for transmit recieve byte count
volatile uint8_t host_control_flag 	= 0;
volatile uint8_t stand_alone_flag 	= 1;
extern uint8_t UID[8];


static const std::uint8_t CardHeader[] = { 0x44, 0x49, 0x48, 0x45 };

uint8_t RfidProcessor::UIDlast[8] = { 0,0,0,0,0,0,0,0 };
volatile std::uint8_t RfidProcessor::RfidCardType = 0;

RfidProcessor::RfidChangedHandler RfidProcessor::RfidChangedEvent;

void RfidProcessor::AfterRfidError()
{
	RfidCardType = 0x00;
	memset(UIDlast, 0, 8);
//	DataProcessor::InstancePtr()->SetCardId(NULL);
	Trf796xCommunicationSetup();
	Trf796xInitialSettings();
}

bool RfidProcessor::ReadSingleBlock(uint8_t index, uint8_t *data)
{
	uint8_t fail = 0;
	uint8_t result;
	do
	{
		result = Iso15693ReadSingleBlockWithAddress(index, UID, data);
	} while (result!=0 && ++fail<10);
	return (fail<10);
}

#define IS_FORMATED_CARD(x) 	(std::memcmp((x), CardHeader, 4)==0)
#define IS_NEW_CARD						(std::memcmp(UID, UIDlast, 8) != 0)

void RfidProcessor::UpdateRfid()
{	
	static uint32_t lostCount = 0;
	
	uint8_t block[4];

	DataProcessor *processor = DataProcessor::InstancePtr();
	
	Trf796xTurnRfOn();
	DELAY(2000);
	if (Iso15693FindTag())		//Card found
	{
		lostCount = 0;
		if IS_NEW_CARD	//New card found
		{
			memcpy(UIDlast, UID, 8);
//			processor->SetCardId(UID);
			
			if (ReadSingleBlock(0, block))
			{
				if IS_FORMATED_CARD(block)	//Is valid format
				{
					RfidCardType = 0x02;
					if (ReadSingleBlock(1, block))
						processor->SetBoxWeight(block);
					else
						AfterRfidError();
				}
				else
				{
					RfidCardType = 0x01;
					processor->SetBoxWeight(0);
				}
			}
			else
				AfterRfidError();
			if (RfidChangedEvent)
				RfidChangedEvent(RfidCardType, UIDlast);
		}

//		if (RfidCardType == 0x02 && cardInfo->NeedUpdate())
//		{
//			fail = 0;
//			do
//			{
//				result = Iso15693WriteSingleBlock(1, cardInfo->UID, cardInfo->GetData());
//			} while (result!=0 && ++fail<10);
//		}
	}
	else if (++lostCount>RFID_OFF_COUNT)
	{
		lostCount = RFID_OFF_COUNT;
		if (RfidCardType!=0)
		{
			RfidCardType = 0x00;
			memset(UIDlast, 0, 8);
			processor->SetBoxWeight(0);
//			processor->SetCardId(NULL);
			if (RfidChangedEvent)
				RfidChangedEvent(RfidCardType, UIDlast);
		}
	}
	Trf796xTurnRfOff();
	DELAY(2000);
}

#endif

