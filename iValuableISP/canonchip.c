#include "canonchip.h"
#include <string.h>
#include "gpio.h"
#include "delay.h"
#include "System.h"
#include "uart.h"

#define HOST_ID 0x001

#define BEGIN_SEND_INDEX 3

#define INDEX_HB 3
#define INDEX_SYNC 4
#define INDEX_NORMAL 5

#ifdef __cplusplus
extern "C" {
#endif

//Reserved RAM for CAN-on-chip
static volatile uint8_t can_memory[0x70] __attribute__((at(0x10000050)))={0xff};

CanRom **rom = (CanRom **)0x1fff1ff8;
CAN_MSG_OBJ msg_objs[33];
CAN_MSG_OBJ *nmtMsg;
//CAN_MSG_OBJ filter_objs[2];

CAN_MSG_OBJ *canRxMsg;

const CAN_ODCONSTENTRY ispConstOD[] = 
{
	/* index, subindex,	length|control,	value */
	{ 0x5100, 0x01, 	0x40, 		0x00000000UL }, //read serial number 1
	{ 0x5100, 0x02, 	0x40, 		0x00000000UL }, //read serial number 2
	{ 0x5100, 0x03, 	0x40, 		0x00000000UL }, //read serial number 3
	{ 0x5100, 0x04, 	0x40, 		0x00000000UL }, //read serial number 4
	{ 0x5000, 0x00, 	0x2b, 		0x00005A5AUL }, //unlock
	{ 0x5020, 0x00, 	0x2b, 		0x00000500UL }, //prepare sectors 0~5 for write
	{ 0x5030, 0x00, 	0x2b, 		0x00000500UL }, //Erase Sectors 0~5 
	{ 0x5015, 0x00, 	0x23, 		0x10001000UL }, //RAM Write Address, ACK:60 15 50 00 00 00 00 00
	{ 0x1f50, 0x01, 	0x21, 		0x00000100UL }, //Program Area,DOMAIN, ACK:60 50 1f 00 00 00 00 00
	{ 0x5050, 0x01, 	0x23, 		0x00000000UL }, //23,50,50,01,00,10,00,00 copy ram to flash dst add 0x00001000
	{ 0x5050, 0x02, 	0x23, 		0x10001000UL }, //23,50,50,02,00,10,00,10 copy ram to flash src add 0x10001000
	{ 0x5050, 0x03, 	0x2b, 		0x00000100UL }, //2b,50,50,03,00,10,00,00 copy ram to flash len 0x1000
	//{ 0x5070, 0x01, 	0x23, 		0x00000000UL }, //23,70,50,01,00,00,00,00 write execution add 0x00001000
	{ 0x5070, 0x01, 	0x23, 		0x10001008UL }, //23,70,50,01,00,00,00,00 write execution add 0x00001000
	{ 0x1f51, 0x01, 	0x2f, 		0x00000001UL }, //2f,51,1f,01,01,00,00,00 write program control 0x01
	{ 0x5060, 0x02, 	0x23, 		0x10001000UL }, //23,60,50,02,00,00,00,00
	{ 0x5060, 0x03, 	0x23, 		0x00000100UL }, //23,60,50,03,00,00,00,00
	{ 0x5060, 0x04, 	0x40, 		0x00000100UL }, //40,60,50,04,00,00,00,00
};

CAN_ODCONSTENTRY ispOD;
CAN_FlashCountStruct ispFlashCount;

volatile uint32_t msgSignals=0;

//ReceiverEventHandler CANEXReceiverEvent = NULL;
//TriggerSyncEventHandler CANTEXTriggerSyncEvent = NULL;
//volatile uint16_t NodeId = 0x0FFF;
//uint16_t HeartbeatInterval = 20;
//uint16_t SyncInterval = 10;

uint32_t ClkInitTable[2];
uint32_t baudRateBak;

/* Global variables */
volatile uint32_t CANopen_Timeout;				/* CANopen timeout timer for SDO client */
volatile uint32_t CANopen_SDOC_BuffCount;			/* Number of bytes in SDO client buffer for segmented read/write */
volatile uint8_t* CANopen_SDOC_Buff;			/* Buffer for SDO client segmented read/write */
volatile uint32_t CANopen_SDOC_Seg_BuffSize;	/* Size of SDO client buffer in number of bytes */
volatile uint8_t CANopen_SDOC_State;			/* Present state of SDO client state machine for controlling SDO client transfers */


extern void uartSendByte(uint8_t ucDat);
extern void uartSendStr(uint8_t const *pucStr, uint32_t ulNum);

/* Publish CAN Callback Functions */
static CAN_CALLBACKS Callbacks = {
   CAN_rx,
   CAN_tx,
   CAN_error,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
};

void CAN_cfgBaudrate(uint32_t baudRateinK)  
{
	ClkInitTable[0]=(SystemCoreClock / 8000000UL) - 1;
	switch (baudRateinK)
	{
		case 1000:
			ClkInitTable[1]=0x1400;        /* 1MBit/s @ 8MHz CAN clk */
			break;
		case 800:
			ClkInitTable[1]=0x1600;        /* 800kBit/s @ 8MHz CAN clk */
			break;
		case 500:
			ClkInitTable[1]=0x1C00;        /* 500kBit/s @ 8MHz CAN clk */
			break;
		case 250:
			ClkInitTable[1]=0x1C01;        /* 250kBit/s @ 8MHz CAN clk */
			break;
		case 125:
			//ClkInitTable[1]=0x1C03;        /* 125kBit/s @ 8MHz CAN clk */
		  ClkInitTable[1]=0x010f;        /* 125kBit/s @ 8MHz CAN clk */
			break;
		case 100:
			ClkInitTable[1]=0x0113;        /* 100kBit/s @ 8MHz CAN clk */
		  //ClkInitTable[1]=0x1201;        /* 100kBit/s @ 8MHz CAN clk */
			break;
	}
}

void CAN_IRQHandler(void)
{
  (*rom)->pCAND->isr();
}

#if 0
uint8_t rxdata[0x100];
CAN_ODENTRY rxEntry;//={0,0,0,rxdata};

void SystemEvent(uint16_t sourceId, CAN_ODENTRY *entry)
{
	if (entry->index==0)
	{
		switch (entry->subindex)
		{
			case 0:
				HeartbeatInterval = (entry->val[1]<<8) | entry->val[0]; 
				break;
			case 1:
				SyncInterval = (entry->val[1]<<8) | entry->val[0];
				break;
		}
	}
}
#endif


void CAN_ISR_HANDLE(CAN_MSG_OBJ *msg)
{
    //uartSendStr(msg->data, 8);
	  //uint32_t i;
    if(CANopen_SDOC_State == CANopen_SDOC_Exp_Read_Busy)
		{
			CANopen_SDOC_State = CANopen_SDOC_Succes;
			canRxMsg = msg;
			//read data to load buff
		}
		else if(CANopen_SDOC_State == CANopen_SDOC_Exp_Write_Busy)
		{
			/* expedited write was initiated */
			if(msg->data[0] == 0x60)
				CANopen_SDOC_State = CANopen_SDOC_Succes;					/* received confirmation */
		}
		else if(CANopen_SDOC_State == CANopen_SDOC_Seg_Read_Busy)
		{
			//null
		}
		else if(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_Busy)
		{
			CANopen_SDOC_State = CANopen_SDOC_Seg_Write_OK;
			canRxMsg = msg;
	  }
}

void ispSegmentDown(void)
{
	 uint32_t i;
	//uartSendStr(canRxMsg->data, 1);
	/* segmented write was initiated */
	if((((canRxMsg->data[0] & (7<<5)) == 0x60) && 
			((canRxMsg->data[0] & (1<<1)) == 0x00)) || 
			((canRxMsg->data[0] & (7<<5)) == 0x20))
	{
		/* received acknowledge */
		CANopen_SDOC_State = CANopen_SDOC_Seg_Write_Busy;
		nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
		nmtMsg->mode_id = 0x67d;
		nmtMsg->dlc = 8;
		if((canRxMsg->data[0] & (7<<5)) == 0x60)
		{
			/* first frame */
			CANopen_SDOC_BuffCount = 0;			/* Clear buffer */
			nmtMsg->data[0] = 1<<4;		/* initialize for toggle */
		}
		//msg->data[0] = msg->data[0];
		nmtMsg->data[0] = ((nmtMsg->data[0] & (1<<4)) ^ (1<<4));		/* toggle */

		/* fill frame data */
		for(i=0; i<7; i++)
		{
			if(CANopen_SDOC_BuffCount < CANopen_SDOC_Seg_BuffSize)
				nmtMsg->data[i+1] = CANopen_SDOC_Buff[CANopen_SDOC_BuffCount++];
			else
				nmtMsg->data[i+1] = 0x00;
		}
		
		/* if end of buffer has been reached, then this is the last frame */
		if(CANopen_SDOC_BuffCount == CANopen_SDOC_Seg_BuffSize)
		{
			i=7-(CANopen_SDOC_Seg_BuffSize%7);		//bytes in last frame
			if(i==7)i=0;						//set all data bytes
			nmtMsg->data[0] |= (i<<1) | 0x01;		/* save length */
			CANopen_SDOC_State = CANopen_SDOC_Succes;		/* set state to success */
		}
		#ifdef to_CAN_ISP_Data
		//uartSendByte('B');
		uartSendStr(nmtMsg->data, 8);
		#endif
		CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
		(*rom)->pCAND->can_transmit(nmtMsg);
	}
	else
	{
		uartSendByte('A');
	}
}

void repeatISPDataSend(void)
{
	CANopen_SDOC_State = CANopen_SDOC_Seg_Write_Busy;
	nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
	nmtMsg->mode_id = 0x67d;
	nmtMsg->dlc = 8;
	CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
  (*rom)->pCAND->can_transmit(nmtMsg);
}


/*	CAN receive callback */
/*	Function is executed by the Callback handler after
	a CAN message has been received */
void CAN_rx(uint8_t msg_obj_num)
{
	CAN_MSG_OBJ *msg;
	//CAN_MSG_OBJ *msg = msg_objs+msg_obj_num;
	//msg->msg_obj_num = msg_obj_num;
  /* Determine which CAN message has been received */
  msg->msg_obj_num = msg_obj_num;
  /* Now load up the msg_obj structure with the CAN message */
  (*rom)->pCAND->can_receive(msg);
	
	if(msg_obj_num==CANISP_RECE_MSG_OBJ_NUM)
  {
		CAN_ISR_HANDLE(msg);
	}
	
  #if 0
	command = msg->mode_id & 0x0F000000;
	sourceId = (msg->mode_id>>12) & 0xFFF;
	switch (command)
	{
		case COMMAND_REQUEST:
			if (msg->dlc >= 4)
			{
				rxEntry.index    			= (msg->data[1]<<8) | msg->data[0];
				rxEntry.subindex 			=  msg->data[2];
				rxEntry.entrytype_len =  msg->data[3];
				if (rxEntry.entrytype_len <= 4)
				{
					memcpy(rxEntry.val,msg->data+4,rxEntry.entrytype_len);
					SystemEvent(sourceId,&rxEntry);
					if (CANEXReceiverEvent)
						CANEXReceiverEvent(sourceId,&rxEntry);
				}
				else
				{
					memcpy(rxEntry.val,msg->data+4,4);
					rxIndex = 4;
				}
			}
			break;
		case COMMAND_REQUEST | COMMAND_EXTENDED:
			memcpy(rxEntry.val+rxIndex,msg->data,msg->dlc);
			rxIndex+=msg->dlc;
			if (rxIndex>=rxEntry.entrytype_len)
			{
				SystemEvent(sourceId,&rxEntry);
				if (CANEXReceiverEvent)
					CANEXReceiverEvent(sourceId,&rxEntry);
			}
			break;
		case COMMAND_SYNC:
			if (msg->data[3]==TRIGGER && CANTEXTriggerSyncEvent)
			{
				CANTEXTriggerSyncEvent((msg->data[1]<<8) | msg->data[0],msg->data[2]);
			}
			break;
		case COMMAND_HEARTBEAT:
			break;
		case COMMAND_LED_DISP_VALUE:
			memcpy(output, msg->data, MAXO_NUM);
			break;
		case COMMAND_ENABLE_LED_REFLASH:
			if((msg->data[0] == 0x55)&&(msg->data[1] == 0xaa))
				modifyLEDReflashFlag(0);
			else if((msg->data[0] == 0xcc)&&(msg->data[1] == 0x33))
				modifyLEDReflashFlag(1);
			break;
	}
	#endif
}

/*	CAN transmit callback */
/*	Function is executed by the Callback handler after
	a CAN message has been transmitted */
void CAN_tx(uint8_t msg_obj_num)
{
	msgSignals &= ~(1<<msg_obj_num);
}

/*	CAN error callback */
/*	Function is executed by the Callback handler after
	an error has occured on the CAN bus */
void CAN_error(uint32_t error_info)
{
	if ((error_info & (CAN_ERROR_STUF | CAN_ERROR_PASS)) == (CAN_ERROR_STUF | CAN_ERROR_PASS))
	{
		NVIC_DisableIRQ(CAN_IRQn);
		CANInit(baudRateBak);	//Force to reset CAN bus
	}
}

#if 0
void CANChangeNodeId(uint16_t id)
{
	NodeId = id & 0x0FFF;
	
	//Receive NodeId Msg
	filter_objs[0].mode_id = NodeId | CAN_MSGOBJ_EXT;
	filter_objs[0].mask = 0x00000fff;
	filter_objs[0].msg_obj_num=2;
	CANSetFilter(&filter_objs[0]);
}
#endif

void CANInit(uint32_t baudRateinK)
{
	int i;
	for(i=0;i<33;++i)
		msg_objs[i].msg_obj_num=i;
	
	msgSignals=0;
	
	baudRateBak=baudRateinK;
	
	//nmtMsg = msg_objs + INDEX_HB;
	nmtMsg = msg_objs + CANISP_SEND_MSG_OBJ_NUM;
	
	//rxEntry.val=rxdata;
	
	//CAN_STB Disable
	GPIOSetDir(CAN_EN_PIN, E_IO_OUTPUT);
	CanSTBDisable();
	
	CAN_cfgBaudrate(baudRateinK);
	(*rom)->pCAND->init_can(ClkInitTable,1);
	(*rom)->pCAND->config_calb(&Callbacks);

	#ifdef to_old_mode
	filter_objs[0].mode_id = 0x05fd;
	filter_objs[0].mask = 0x0fff;
	//filter_objs[0].msg_obj_num = 0;
	filter_objs[0].msg_obj_num = CANISP_RECE_MSG_OBJ_NUM;
	CANSetFilter(&filter_objs[0]);
	#else
	msg_objs[1].mode_id = 0x05fd;
	msg_objs[1].mask = 0x0fff;
	//msg_objs[1].msg_obj_num = 0;
	msg_objs[1].msg_obj_num = CANISP_RECE_MSG_OBJ_NUM;
	CANSetFilter(&msg_objs[1]);
	#endif
	
	#if 0
	//Receive NodeId Msg
	filter_objs[0].mode_id = NodeId | CAN_MSGOBJ_EXT;
	filter_objs[0].mask = 0x00000fff;
	filter_objs[0].msg_obj_num = 0;
	CANSetFilter(&filter_objs[0]);
	
	//Receive Broadcast Msg
	filter_objs[1].mode_id = CAN_MSGOBJ_EXT | (HOST_ID<<12);
	filter_objs[1].mask = 0x00ffffff;
	filter_objs[1].msg_obj_num = 1;
	CANSetFilter(&filter_objs[1]);
	#endif

	NVIC_EnableIRQ(CAN_IRQn);
	NVIC_SetPriority(CAN_IRQn, 2);
}

void CANSend(CAN_MSG_OBJ *pMsg)
{
	uint32_t sync = 1<<pMsg->msg_obj_num;
	uint32_t counter=0;
	uint8_t num=pMsg->msg_obj_num;
	
	if ((LPC_CAN->STAT & 0x7)!=0) //Make sure no error on bus to send 
		return;
	
	while (msgSignals & sync)
	{
		DELAY(100);
		if (++counter>1000)
		{
			counter=0;
			if (pMsg->msg_obj_num == num)
				msgSignals &= ~sync;
			if (++pMsg->msg_obj_num >= 32)
				pMsg->msg_obj_num = BEGIN_SEND_INDEX;
			sync = 1<<pMsg->msg_obj_num;
			if (pMsg->msg_obj_num == num)	//all failed
			{
				NVIC_DisableIRQ(CAN_IRQn);
				CANInit(baudRateBak);	//Force to reset CAN bus
				return;  
			}
		}
	}
	
	msgSignals |= sync;
	(*rom)->pCAND->can_transmit(pMsg);
	pMsg->msg_obj_num = num;
}

void CANSetFilter(CAN_MSG_OBJ *pMsg)
{
	(*rom)->pCAND->config_rxmsgobj(pMsg);
}

#if 0
void CANEXHeartbeat(enum SystemState state)
{
	//nmtMsg->mode_id = CAN_MSGOBJ_EXT | COMMAND_HEARTBEAT | (NodeId & 0xFFF)<<12;
	nmtMsg->mode_id = 0x067d;
	nmtMsg->dlc = 1;
	nmtMsg->data[0]=state;
	CANSend(nmtMsg);
}

void CANEXSync(uint16_t syncId,uint16_t index,enum SyncMode mode)
{
	syncId &= 0xfff;
	msg_objs[INDEX_SYNC].mode_id = CAN_MSGOBJ_EXT | COMMAND_SYNC | syncId;
	msg_objs[INDEX_SYNC].dlc = 4;
	msg_objs[INDEX_SYNC].data[0] =  index     & 0xff;
	msg_objs[INDEX_SYNC].data[1] = (index>>8) & 0xff;
	msg_objs[INDEX_SYNC].data[2] =  0;
	msg_objs[INDEX_SYNC].data[3] = mode;
	CANSend(&msg_objs[INDEX_SYNC]);
}

void CANTransmit(uint32_t command,uint16_t targetId,CAN_ODENTRY *entry,uint16_t sourceId)
{
	uint8_t segment,remainder,i,offset,msgindex;
	CAN_MSG_OBJ *msg;
	
	NodeId &= 0xfff;
	targetId &= 0xfff;
	
	msgindex = INDEX_NORMAL;
	
	msg_objs[msgindex].mode_id= CAN_MSGOBJ_EXT | command | targetId;
	if (sourceId==0xffff)
		msg_objs[msgindex].mode_id |= NodeId<<12;
	else
		msg_objs[msgindex].mode_id |= ((sourceId & 0xfff)<<12);
	msg_objs[msgindex].data[0] =  entry->index     & 0xff;
	msg_objs[msgindex].data[1] = (entry->index>>8) & 0xff;
	msg_objs[msgindex].data[2] =  entry->subindex;
	msg_objs[msgindex].data[3] =  entry->entrytype_len;
	if (entry->entrytype_len<=4)
	{
		msg_objs[msgindex].dlc = 4 + entry->entrytype_len;
		memcpy(msg_objs[msgindex].data+4,entry->val,entry->entrytype_len);
		CANSend(&msg_objs[msgindex]);
		return;
	}

	msg_objs[msgindex].dlc = 8;
	memcpy(msg_objs[msgindex].data+4,entry->val,4);
	CANSend(msg_objs+msgindex);
	//DELAY(100);
	
	segment 	= (entry->entrytype_len-4) >> 3;
	remainder = (entry->entrytype_len-4) & 0x7;
	offset=4;
	if (remainder!=0)
		segment++;
	
	++msgindex;
	for(i=0; i<segment; ++i)
	{
		msg = &msg_objs[msgindex];
		msg->mode_id = CAN_MSGOBJ_EXT | command | COMMAND_EXTENDED | NodeId<<12 | targetId;
		msg->dlc = (i<segment-1 || remainder==0) ? 8 : remainder;
		memcpy(msg->data,entry->val+offset,msg->dlc);
		CANSend(msg);
		//DELAY(100);
		offset += msg->dlc;
		++msgindex;
		if (msgindex >= 32)
			msgindex = INDEX_NORMAL;
	}
}

void CANEXRequset(uint16_t serviceId,CAN_ODENTRY *entry)
{
	CANTransmit(COMMAND_REQUEST,serviceId,entry,NodeId);
}

void CANEXSetInterval(uint16_t serviceId,enum IntervalType type,uint16_t interval)
{
	CAN_ODENTRY config;
	config.index					=	0;
	config.subindex				=	type;
	config.entrytype_len	=	2;
	config.val						=	(uint8_t *)&interval;
	CANTransmit(COMMAND_REQUEST,serviceId,&config,NodeId);
}

void CANEXResponse(uint16_t clientId,CAN_ODENTRY *entry)
{
	CANTransmit(COMMAND_RESPONSE,clientId,entry,NodeId);
}

void CANEXBroadcast(CAN_ODENTRY *entry)
{
	CANTransmit(COMMAND_BROADCAST,0,entry,NodeId);
}
#endif

/*****************************************************************************
** Function name:		CANopen_10ms_tick
**
** Description:			CAN related functions executed every 10ms, e.g:
** 						SDO timeout, heartbeat consuming/generation.
** 						Function must be called from application every 10ms.
**
** Parameters:			None
** Returned value:		None
*****************************************************************************/
void CANopen_10ms_tick(void)
{
	/* timeout for SDO client functions */
	if(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail))
	{
		if(CANopen_Timeout-- == 0)
			CANopen_SDOC_State = CANopen_SDOC_Fail;		/* change status to fail on timeout */
	}
}

uint8_t CANopen_SDOC_Exp_ReadWrite(uint8_t entry_index)
{
	uint32_t _temp;
	if(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail)
	{
		ispOD=ispConstOD[entry_index];
		/* only initiate when no SDO client transfer is ongoing */
		if((ispOD.len&0x40)==0x40)
			CANopen_SDOC_State = CANopen_SDOC_Exp_Read_Busy;
		else if(ispOD.len==0x21)
			CANopen_SDOC_State = CANopen_SDOC_Seg_Write_Busy;
	  else
		  CANopen_SDOC_State = CANopen_SDOC_Exp_Write_Busy;
		nmtMsg->mode_id = 0x67d;
		nmtMsg->mask = 0x00;
		nmtMsg->dlc = 8;
		nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
		nmtMsg->data[0] = ispOD.len;
		nmtMsg->data[1] = (uint8_t)(ispOD.index);
		nmtMsg->data[2] = (uint8_t)(ispOD.index>>8);
		nmtMsg->data[3] = ispOD.subindex;
		_temp = ispOD.val;
		nmtMsg->data[4] = (uint8_t)(_temp);
		_temp >>= 8;
		nmtMsg->data[5] = (uint8_t)(_temp);
		_temp >>= 8;
		nmtMsg->data[6] = (uint8_t)(_temp);
		_temp >>= 8;
		nmtMsg->data[7] = (uint8_t)(_temp);
		(*rom)->pCAND->can_transmit(nmtMsg); //CANSend(nmtMsg);
		CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
		return 1;
	}
	CANopen_SDOC_State = CANopen_SDOC_Fail;
	return 0;
}

uint8_t CANopen_SDOC_Exp_WritePageAddress(uint8_t page)
{
	if(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail)
	{
		/* only initiate when no SDO client transfer is ongoing */
    CANopen_SDOC_State = CANopen_SDOC_Exp_Write_Busy;
		nmtMsg->mode_id = 0x67d;
		nmtMsg->mask = 0x00;
		nmtMsg->dlc = 8;
		nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
		nmtMsg->data[0] = 0x23;
		nmtMsg->data[1] = 0x15;
		nmtMsg->data[2] = 0x50;
		nmtMsg->data[3] = 0x00;
		nmtMsg->data[4] = 0x00;
		nmtMsg->data[5] = 0x10+page;
		nmtMsg->data[6] = 0x00;
		nmtMsg->data[7] = 0x10;
		(*rom)->pCAND->can_transmit(nmtMsg); //CANSend(nmtMsg);
		CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
		return 1;
	}
	CANopen_SDOC_State = CANopen_SDOC_Fail;
	return 0;
}

//23,50,50,01,00,10,00,00 copy ram to flash dst add 0x00001000
uint8_t CANopen_SDOC_Exp_WriteFlashAddress(uint8_t flash_page)
{
	if(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail)
	{
		/* only initiate when no SDO client transfer is ongoing */
    CANopen_SDOC_State = CANopen_SDOC_Exp_Write_Busy;
		nmtMsg->mode_id = 0x67d;
		nmtMsg->mask = 0x00;
		nmtMsg->dlc = 8;
		nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
		nmtMsg->data[0] = 0x23;
		nmtMsg->data[1] = 0x50;
		nmtMsg->data[2] = 0x50;
		nmtMsg->data[3] = 0x01;
		nmtMsg->data[4] = 0x00;
		nmtMsg->data[5] = flash_page;
		nmtMsg->data[6] = 0x00;
		nmtMsg->data[7] = 0x00;
		(*rom)->pCAND->can_transmit(nmtMsg); //CANSend(nmtMsg);
		CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
		return 1;
	}
	CANopen_SDOC_State = CANopen_SDOC_Fail;
	return 0;
}

uint8_t CANopen_SDOC_Exp_CompareAddress(uint8_t flash_page)
{
	if(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail)
	{
		/* only initiate when no SDO client transfer is ongoing */
    CANopen_SDOC_State = CANopen_SDOC_Exp_Write_Busy;
		nmtMsg->mode_id = 0x67d;
		nmtMsg->mask = 0x00;
		nmtMsg->dlc = 8;
		nmtMsg->msg_obj_num = CANISP_SEND_MSG_OBJ_NUM;
		nmtMsg->data[0] = 0x23;
		nmtMsg->data[1] = 0x60;
		nmtMsg->data[2] = 0x50;
		nmtMsg->data[3] = 0x01;
		nmtMsg->data[4] = 0x00;
		nmtMsg->data[5] = flash_page;
		nmtMsg->data[6] = 0x00;
		nmtMsg->data[7] = 0x00;
		(*rom)->pCAND->can_transmit(nmtMsg); //CANSend(nmtMsg);
		CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
		return 1;
	}
	CANopen_SDOC_State = CANopen_SDOC_Fail;
	return 0;
}


void repeatSendCurCANData(void)
{
  CANopen_SDOC_State = CANopen_SDOC_Seg_Write_Busy;
  CANopen_Timeout = CANOPEN_TIMEOUT_VAL;
  (*rom)->pCAND->can_transmit(nmtMsg);
}


#ifdef __cplusplus
}
#endif

