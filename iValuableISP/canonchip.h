/****************************************************************************
 *   $Id:: rom_driver_CAN.h 5034 2010-09-24 23:48:37Z nxp12832              $
 *   Project: NXP LPC11xx CAN example
 *
 *   Description:
 *     This file is part of the CAN and CANopen on-chip driver examples.
 *
 ****************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
****************************************************************************/
#ifndef __ROM_DRIVER_CAN_H__
#define __ROM_DRIVER_CAN_H__

#include "LPC11xx.h"


#ifndef NULL
#define NULL    ((void *)0)
#endif

extern volatile uint32_t CANopen_Timeout;				/* CANopen timeout timer for SDO client */
extern volatile uint32_t CANopen_SDOC_BuffCount;			/* Number of bytes in SDO client buffer for segmented read/write */
extern volatile uint8_t* CANopen_SDOC_Buff;			/* Buffer for SDO client segmented read/write */
extern volatile uint32_t CANopen_SDOC_Seg_BuffSize;	/* Size of SDO client buffer in number of bytes */
extern volatile uint8_t CANopen_SDOC_State;			/* Present state of SDO client state machine for controlling SDO client transfers */
extern volatile uint8_t CANopen_SDOC_SegWrite_State;

// error status bits
#define CAN_ERROR_NONE  0x00000000UL
#define CAN_ERROR_PASS  0x00000001UL
#define CAN_ERROR_WARN  0x00000002UL
#define CAN_ERROR_BOFF  0x00000004UL
#define CAN_ERROR_STUF  0x00000008UL
#define CAN_ERROR_FORM  0x00000010UL
#define CAN_ERROR_ACK   0x00000020UL
#define CAN_ERROR_BIT1  0x00000040UL
#define CAN_ERROR_BIT0  0x00000080UL
#define CAN_ERROR_CRC   0x00000100UL

// control bits for CAN_MSG_OBJ.mode_id
#define CAN_MSGOBJ_STD  0x00000000UL   // CAN 2.0a 11-bit ID
#define CAN_MSGOBJ_EXT  0x20000000UL   // CAN 2.0b 29-bit ID
#define CAN_MSGOBJ_DAT  0x00000000UL   // data frame
#define CAN_MSGOBJ_RTR  0x40000000UL   // rtr frame

#ifdef __cplusplus
extern "C" {
#endif

//extern volatile uint16_t NodeId;
//extern uint16_t HeartbeatInterval;
//extern uint16_t SyncInterval;

typedef struct _CAN_MSG_OBJ {
  uint32_t  mode_id;
  uint32_t  mask;
  uint8_t   data[8];
  uint8_t   dlc;
  uint8_t   msg_obj_num;
}CAN_MSG_OBJ;

/**************************************************************************
SDO Abort Codes
**************************************************************************/
#define SDO_ABORT_TOGGLE          0x05030000UL  // Toggle bit not alternated
#define SDO_ABORT_SDOTIMEOUT      0x05040000UL  // SDO protocol timed out
#define SDO_ABORT_UNKNOWN_COMMAND 0x05040001UL  // Client/server command specifier not valid or unknown
#define SDO_ABORT_UNSUPPORTED     0x06010000UL  // Unsupported access to an object
#define SDO_ABORT_WRITEONLY       0x06010001UL  // Attempt to read a write only object
#define SDO_ABORT_READONLY        0x06010002UL  // Attempt to write a read only object
#define SDO_ABORT_NOT_EXISTS      0x06020000UL  // Object does not exist in the object dictionary
#define SDO_ABORT_PARAINCOMP      0x06040043UL  // General parameter incompatibility reason
#define SDO_ABORT_ACCINCOMP       0x06040047UL  // General internal incompatibility in the device
#define SDO_ABORT_TYPEMISMATCH    0x06070010UL  // Data type does not match, length of service parameter does not match
#define SDO_ABORT_UNKNOWNSUB      0x06090011UL  // Sub-index does not exist
#define SDO_ABORT_VALUE_RANGE     0x06090030UL  // Value range of parameter exceeded (only for write access)
#define SDO_ABORT_TRANSFER        0x08000020UL  // Data cannot be transferred or stored to the application
#define SDO_ABORT_LOCAL           0x08000021UL  // Data cannot be transferred or stored to the application because of local control
#define SDO_ABORT_DEVSTAT         0x08000022UL  // Data cannot be transferred or stored to the application because of the present device state

typedef struct _CAN_ODCONSTENTRY {
  uint16_t index;
  uint8_t  subindex;
  uint8_t  len;
  uint32_t val;
}CAN_ODCONSTENTRY;

// upper-nibble values for CAN_ODENTRY.entrytype_len
#define OD_NONE    0x00    // Object Dictionary entry doesn't exist
#define OD_EXP_RO  0x10    // Object Dictionary entry expedited, read-only
#define OD_EXP_WO  0x20    // Object Dictionary entry expedited, write-only
#define OD_EXP_RW  0x30    // Object Dictionary entry expedited, read-write
#define OD_SEG_RO  0x40    // Object Dictionary entry segmented, read-only
#define OD_SEG_WO  0x50    // Object Dictionary entry segmented, write-only
#define OD_SEG_RW  0x60    // Object Dictionary entry segmented, read-write

typedef struct _CAN_ODENTRY {
  uint16_t index;
  uint8_t  subindex;
  uint8_t  entrytype_len;
  uint8_t  *val;
}CAN_ODENTRY;

typedef struct _CAN_CANOPENCFG {
  uint8_t   node_id;
  uint8_t   msgobj_rx;
  uint8_t   msgobj_tx;
  uint8_t   isr_handled;
  uint32_t  od_const_num;
  CAN_ODCONSTENTRY *od_const_table;
  uint32_t  od_num;
  CAN_ODENTRY *od_table;
}CAN_CANOPENCFG;

// Return values for CANOPEN_sdo_req() callback
#define CAN_SDOREQ_NOTHANDLED     0  // process regularly, no impact
#define CAN_SDOREQ_HANDLED_SEND   1  // processed in callback, auto-send returned msg
#define CAN_SDOREQ_HANDLED_NOSEND 2  // processed in callback, don't send response

// Values for CANOPEN_sdo_seg_read/write() callback 'openclose' parameter
#define CAN_SDOSEG_SEGMENT        0  // segment read/write
#define CAN_SDOSEG_OPEN           1  // channel is opened
#define CAN_SDOSEG_CLOSE          2  // channel is closed

typedef void (*ReceiverEventHandler)(uint16_t sourceId, CAN_ODENTRY *entry);
typedef void (*TriggerSyncEventHandler)(uint16_t index,uint8_t mode);

//extern ReceiverEventHandler CANEXReceiverEvent;
//extern TriggerSyncEventHandler CANTEXTriggerSyncEvent;

extern void CAN_IRQHandler(void);

///* Callback function prototypes */
extern void CAN_rx(uint8_t msg_obj_num);
extern void CAN_tx(uint8_t msg_obj_num);
extern void CAN_error(uint32_t error_info);

typedef	struct _CAN_CALLBACKS {
  void (*CAN_rx)(uint8_t msg_obj_num);
  void (*CAN_tx)(uint8_t msg_obj_num);
  void (*CAN_error)(uint32_t error_info);
  uint32_t (*CANOPEN_sdo_read)(uint16_t index, uint8_t subindex);
  uint32_t (*CANOPEN_sdo_write)(uint16_t index, uint8_t subindex, uint8_t *dat_ptr);
  uint32_t (*CANOPEN_sdo_seg_read)(uint16_t index, uint8_t subindex, uint8_t openclose, uint8_t *length, uint8_t *data, uint8_t *last);
  uint32_t (*CANOPEN_sdo_seg_write)(uint16_t index, uint8_t subindex, uint8_t openclose, uint8_t length, uint8_t *data, uint8_t *fast_resp);
  uint8_t (*CANOPEN_sdo_req)(uint8_t length_req, uint8_t *req_ptr, uint8_t *length_resp, uint8_t *resp_ptr);
}CAN_CALLBACKS;

// API function prototypes
extern void init_can(uint32_t * can_cfg, uint8_t isr_ena);
extern void isr(void);
extern void config_rxmsgobj(CAN_MSG_OBJ * msg_obj);
extern uint8_t can_receive(CAN_MSG_OBJ * msg_obj);
extern void can_transmit(CAN_MSG_OBJ * msg_obj);
extern void config_canopen(CAN_CANOPENCFG * canopen_cfg);
extern void canopen_handler(void);
extern void config_calb(CAN_CALLBACKS * callback_cfg);

typedef	struct _CAND {
  void (*init_can)(uint32_t * can_cfg, uint8_t isr_ena);
  void (*isr)(void);
  void (*config_rxmsgobj)(CAN_MSG_OBJ * msg_obj);
  uint8_t (*can_receive)(CAN_MSG_OBJ * msg_obj);
  void (*can_transmit)(CAN_MSG_OBJ * msg_obj);
  void (*config_canopen)(CAN_CANOPENCFG * canopen_cfg);
  void (*canopen_handler)(void);
  void (*config_calb)(CAN_CALLBACKS * callback_cfg);
}	CAND;

typedef	struct _ROM {
   unsigned p_usbd;
   unsigned p_clib;
   CAND *pCAND;
   unsigned p_pwrd;
   unsigned p_dev1;
   unsigned p_dev2;
   unsigned p_dev3;
   unsigned p_dev4; 
}	CanRom;

extern CAN_MSG_OBJ msg_objs[33];

enum SyncMode
{ 
	TRIGGER    = (uint8_t)0,
	AUTO_START = (uint8_t)1,
	AUTO_STOP  = (uint8_t)2
};

enum IntervalType
{
	SYSTEM_HEARTBEAT	=	((uint8_t)0),
	SYSTEM_SYNC				=	((uint8_t)1)
};

enum SystemState
{
	STATE_BOOTUP  				= ((uint8_t)0x00),
	STATE_STOPPED 				= ((uint8_t)0x04),
	STATE_OPERATIONAL 		= ((uint8_t)0x05),
	STATE_PREOPERATIONAL 	= ((uint8_t)0x7F)
};

#define COMMAND_REQUEST					 				 0x1000000
#define COMMAND_RESPONSE				 				 0x2000000
#define COMMAND_BROADCAST								 0x3000000
#define COMMAND_SYNC								 		 0x4000000
#define COMMAND_HEARTBEAT 			 				 0x7000000
#define COMMAND_EXTENDED 0x8000000

#define CANOPEN_TIMEOUT_VAL		20			/* in 10 ms */

#define CANISP_RECE_MSG_OBJ_NUM 1
#define CANISP_SEND_MSG_OBJ_NUM 8

/* SDOC State */
#define CANopen_SDOC_Fail			0
#define CANopen_SDOC_Succes			1
#define CANopen_SDOC_Exp_Read_Busy	2
#define CANopen_SDOC_Exp_Write_Busy	3
#define CANopen_SDOC_Seg_Read_Busy	4
#define CANopen_SDOC_Seg_Write_Busy	5
#define CANopen_SDOC_Seg_Write_OK 6



void CANSend(CAN_MSG_OBJ *pMsg);
void CANSetFilter(CAN_MSG_OBJ *pMsg);
void CANInit(uint32_t baudRateinK);

void CANChangeNodeId(uint16_t id);

void CANEXRequset(uint16_t serviceId,CAN_ODENTRY *entry);
void CANEXResponse(uint16_t clientId,CAN_ODENTRY *entry);
void CANEXBroadcast(CAN_ODENTRY *entry);
void CANEX(uint16_t serviceId,enum IntervalType type,uint16_t interval);
void CANEXHeartbeat(enum SystemState state);
void CANEXSync(uint16_t syncId,uint16_t index,enum SyncMode mode);


enum
{
	READ_SERIAL_NUMBER1_INDEX,
	READ_SERIAL_NUMBER2_INDEX,
	READ_SERIAL_NUMBER3_INDEX,
	READ_SERIAL_NUMBER4_INDEX,
	FLASH_UNLOCK_INDEX,
	PREPARE_SECTORS_INDEX,
	ERASE_SECTORS_INDEX,
	WRITE_RAM_ADDRESS_INDEX,
	START_DOWNLOAD_DATA_INDEX,
	COPY_RAM_TO_FLASH_DST_ADD_INDEX,
	COPY_RAM_TO_FLASH_SRC_ADD_INDEX,
	COPY_RAM_TO_FLASH_LENGTH_INDEX,
	WRITE_EXECUTION_ADDRESS_INDEX,
	WRITE_PROGRAM_CONTROL_INDEX,
	WRITE_COMPARE_RAM_ADDRESS_INDEX,
	WRITE_COMPARE_LENGTH_INDEX,
	READ_COMPARE_VALUE_INDEX,
};

typedef struct _CAN_Flash_Str {
  uint8_t page;
  uint8_t sector;
}CAN_FlashCountStruct;

extern CAN_FlashCountStruct ispFlashCount;
#define SECTOR_PAGE_NUMBER 16 //4K=16*256Byte

extern CAN_MSG_OBJ *nmtMsg;

void CANopen_10ms_tick(void);
uint8_t CANopen_SDOC_Exp_Write(uint8_t node_id, uint16_t index, uint8_t subindex, uint8_t* data, uint8_t length);
uint8_t CANopen_SDOC_Exp_ReadWrite(uint8_t entry_index);

void CAN_ISR_HANDLE(CAN_MSG_OBJ *msg);
void ispSegmentDown(void);
uint8_t CANopen_SDOC_Exp_WritePageAddress(uint8_t page);
uint8_t CANopen_SDOC_Exp_WriteFlashAddress(uint8_t flash_page);
void repeatSendCurCANData(void);

uint8_t CANopen_SDOC_Exp_CompareAddress(uint8_t flash_page);
void repeatISPDataSend(void);

#ifdef __cplusplus
}
#endif
				   
#endif /* __ROM_DRIVER_CAN_H__ */
