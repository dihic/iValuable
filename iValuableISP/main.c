
#include "LPC11xx.h"
#include "canonchip.h"
#include "timer32.h"
#include "gpio.h"
#include "math.h"
#include <string.h>
#include "System.h"
#include "uart.h"
#include "delay.h"

extern uint32_t baudRateBak;

extern uint32_t UARTStatus;
extern uint8_t UARTBuffer[UART_BUFSIZE];
extern uint32_t UARTCount;

static uint32_t UARTPostion=0;

uint8_t led_status_flag;

extern CAN_MSG_OBJ *canRxMsg;

#define RECE_UART_FRAME_CHECKSUM_BYTE_LEN 2
#define RECE_UART_FRAME_FRONT_CODE_LEN 5

//#define MAX_UART_BUFF_LEN  0x400 //(6+256+2)*2
#define MAX_RECE_FRAME_LEN 256+1+1
#define MAX_RECE_CMD_FRAME_LEN MAX_RECE_FRAME_LEN+8
#define MAX_SEND_FRAME_LEN 20

#define FLASH_PAGE_SIZE 256

#define MAX_LPC11C24_FLASH_PAGE_NUMBER 96//128 //page=256byte,128Page*256=32K

typedef struct _tagUartRece
{
	uint32_t frame_head;
	uint32_t current_frame_len;
	uint32_t current_checksum;
	uint32_t frame_active_len;

	uint8_t is_vaild_frame;
	uint8_t frame_status;
	uint8_t curCMDbuff[MAX_RECE_CMD_FRAME_LEN];
	uint8_t sendbuff[MAX_SEND_FRAME_LEN];
} tagUartRece;
tagUartRece UartRece;

//UartRece.is_vaild_frame
enum
{
	IS_INVAILD,
	IS_VAILD
};

//UartRece.frame_status
enum
{
	START_BYTE_STATUS,
	FRAME_START_BYTE_0X5A_STATUS,
	FRAME_START_BYTE_0XA5_STATUS,
	CMD_BYTE_STATUS,
	LENGTH_LOW_BYTE_STATUS,
	LENGTH_HIGH_BYTE_STATUS
};

#define FRAME_START_BYTE_0X5A 0x5A
#define FRAME_START_BYTE_0XA5 0xA5

#define FRAME_CMD_BYTE_POS 2

enum
{
  FRAME_CMD_NULL,
  FRAME_LOOK_FOR_DEVICE_CMD, //0x01
  FRAME_DOWN_UPDATE_ERROR_CMD, //0x02  Slave
  FRAME_PROGRAM_DATA_CMD, //0x03
  FRAME_ERASE_CHIP_CMD, //0x04
  FRAME_MASTER_GO_CMD, //0x05
  FRAME_FIRMWARE_VERSION_CMD, //0x06
  FRAME_CMD_LIMIT
};

enum
{
	LED_OFF,
	LED_ON
};

const uint8_t JumpCodeTable[] = 
{
#if 0
0x40, 0xBA, 0x70, 0x47, 0xC0, 0xBA, 0x70, 0x47, 
0x13, 0x48, 0x12, 0x49, 0x02, 0x68, 0x01, 0x23, 
0x1B, 0x04, 0x1A, 0x43, 0x02, 0x60, 0x03, 0x68, 
0x40, 0x22, 0x13, 0x43, 0x03, 0x60, 0x0F, 0x48, 
0x03, 0x68, 0x13, 0x43, 0x03, 0x60, 0x02, 0x68, 
0xFF, 0x23, 0x01, 0x33, 0x1A, 0x43, 0x02, 0x60, 
0x0B, 0x4A, 0x00, 0x20, 0x10, 0x60, 0x0B, 0x4A, 
0x10, 0x60, 0x00, 0xE0, 0x00, 0xBF, 0x49, 0x1E, 
0x00, 0x29, 0xFB, 0xDC, 0xBF, 0xF3, 0x4F, 0x8F, 
0x08, 0x49, 0x07, 0x48, 0xC8, 0x60, 0xBF, 0xF3, 
0x4F, 0x8F, 0xFE, 0xE7, 0xF0, 0x49, 0x02, 0x00, 
0x80, 0x80, 0x04, 0x40, 0x00, 0x80, 0x02, 0x50, 
0x00, 0x01, 0x02, 0x50, 0x00, 0x04, 0x02, 0x50, 
0x04, 0x00, 0xFA, 0x05, 0x00, 0xED, 0x00, 0xE0
#else
0x40, 0xBA, 0x70, 0x47, 0xC0, 0xBA, 0x70, 0x47, 
0xBF, 0xF3, 0x4F, 0x8F, 0x03, 0x49, 0x02, 0x48, 
0xC8, 0x60, 0xBF, 0xF3, 0x4F, 0x8F, 0xFE, 0xE7, 
0x04, 0x00, 0xFA, 0x05, 0x00, 0xED, 0x00, 0xE0
#endif
};

void ReflashISPLED(uint8_t led_on_off)
{
	GPIOSetValue(ISP_STATUS_LED_PIN, led_status_flag);
}

void LEDinit(void)
{
  //ISP STATUS LED
	GPIOSetDir(ISP_STATUS_LED_PIN, E_IO_OUTPUT);
	led_status_flag = LED_ON;
	ReflashISPLED(led_status_flag);
}

const uint8_t SLAVE_ACK_UID_INFO[] = {0x33,0xCC,0x01,0x04,0x00,0x01,0x02,0x03,0x04,0x00,0x00};

const uint8_t SLAVE_ISP_TARGET_LOSS_INFO[]     = {0x33,0xCC,0x02,0x01,0x00,0xE1,0xE4,0x00};
const uint8_t SLAVE_CHECKSUM_ERROR_INFO[]      = {0x33,0xCC,0x02,0x01,0x00,0xE2,0xE5,0x00};
const uint8_t SLAVE_ISP_TARGET_TIMEOUT_INFO[]  = {0x33,0xCC,0x02,0x01,0x00,0xE3,0xE6,0x00};
const uint8_t SLAVE_ISP_TARGET_TIMEOUT_INFO1[]  = {0x33,0xCC,0x02,0x01,0x00,0xE5,0xE6,0x00};
const uint8_t SLAVE_ISP_TARGET_TIMEOUT_INFO2[]  = {0x33,0xCC,0x02,0x01,0x00,0xE6,0xE6,0x00};
const uint8_t SLAVE_ISP_TARGET_TIMEOUT_INFO3[]  = {0x33,0xCC,0x02,0x01,0x00,0xE7,0xE7,0x00};

const uint8_t SLAVE_FLASH_PAGE_OVERFLOW_INFO[] = {0x33,0xCC,0x02,0x01,0x00,0xE4,0xE7,0x00};

const uint8_t SLAVE_ACK_PAGE_DOWNLOAD_INFO[] = {0x33,0xCC,0x03,0x01,0x00,0x00,0xE6,0x00};
const uint8_t SLAVE_ACK_ERASE_CHIP_INFO[] = {0x33,0xCC,0x04,0x01,0x00,0x01,0x06,0x00};
const uint8_t SLAVE_ACK_GO_INFO[] = {0x33,0xCC,0x05,0x01,0x00,0x01,0x07,0x00};

//const uint8_t SLAVE_ACK_FIRMWARE_VERSION_INFO[] = {0x33,0xCC,0x06,0x04,0x00,'V', '1', '.', '0', 0x00,0x00};

void Buffercpy(uint8_t* pSBuffer, uint8_t* pDBuffer, uint16_t BufferLength)  
{  
  while(BufferLength--)  
  {
  	*pDBuffer = *pSBuffer;
  	pDBuffer++;
  	pSBuffer++;
  }  
}

void moveReceTailPointer(uint32_t move_len)
{
	uint32_t i;
	for(i=0; i<move_len; i++)
	{
		UARTPostion++;
		if (UARTPostion == UART_BUFSIZE)
		{
			UARTPostion = 0;		/* buffer overflow */
		}
	}
}

void modifyVaildFrame(uint8_t is_flag)
{
	UartRece.is_vaild_frame=is_flag;
}

uint8_t getVaildFrame(void)
{
	return UartRece.is_vaild_frame;
}

void modifyReceFrameStatus(uint8_t status)
{
	UartRece.frame_status = status;
}

uint8_t getReceFrameStatus(void)
{
	return UartRece.frame_status;
}

void resetReceStatus(void)
{
	modifyReceFrameStatus(START_BYTE_STATUS);
	UARTPostion = UARTCount;
	UartRece.frame_head = UARTPostion;
}

uint16_t calcCheckSum(uint8_t *pbuff, uint32_t len)
{
	uint32_t i;
	uint16_t check_sum;
	uint8_t *pcheckData;
	
  check_sum = 0;
	pcheckData = pbuff;
	pcheckData += 2;
  for(i=0; i<len; i++)
  {
    check_sum += *pcheckData++;
  }
	return check_sum;
}

void copyCurrentCMDDataToBuff(uint32_t len)
{
	uint32_t i,length;
	uint16_t rece_checksum,calc_checksum;
	
	memset(UartRece.curCMDbuff, 0, MAX_RECE_CMD_FRAME_LEN);
	length = len+RECE_UART_FRAME_FRONT_CODE_LEN;
	length %= MAX_RECE_CMD_FRAME_LEN;
	UartRece.frame_active_len = length;
	for(i=0; i<length; i++)
	{
		UartRece.curCMDbuff[i] = UARTBuffer[UartRece.frame_head++];
		if(UartRece.frame_head == UART_BUFSIZE) UartRece.frame_head=0;
	}
	calc_checksum = calcCheckSum(UartRece.curCMDbuff, (UartRece.frame_active_len-4));
	rece_checksum = UartRece.curCMDbuff[UartRece.frame_active_len-1];
	rece_checksum <<= 8;
	rece_checksum |= UartRece.curCMDbuff[UartRece.frame_active_len-2];
	UartRece.curCMDbuff[UartRece.frame_active_len-1] = 0;
	UartRece.curCMDbuff[UartRece.frame_active_len-2] = 0;
	
	#ifdef to_debug_part
	if(calc_checksum==rece_checksum) uartSendByte('1');
	else uartSendByte('2');
	#else
	if(calc_checksum==rece_checksum) modifyVaildFrame(IS_VAILD);
	else uartSendStr(SLAVE_CHECKSUM_ERROR_INFO, sizeof(SLAVE_CHECKSUM_ERROR_INFO));
	#endif
}

void canErrorForceReset(void)
{
	uint32_t can_status;
	can_status = LPC_CAN->STAT;
	if((can_status&0x07)!=0)
	{
		NVIC_DisableIRQ(CAN_IRQn);
		CANInit(baudRateBak);	//Force to reset CAN bus
	}
}

void handleUartReceFrame(void)
{
	uint8_t _cmd;
	uint8_t fail;
	uint16_t calc_checksum;
	
	if(getVaildFrame()==IS_VAILD)
	{
		modifyVaildFrame(IS_INVAILD);
		_cmd = UartRece.curCMDbuff[FRAME_CMD_BYTE_POS];
		if(_cmd == FRAME_LOOK_FOR_DEVICE_CMD)
		{
			CanSTBEnable(); //enable CAN transmiter
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(READ_SERIAL_NUMBER4_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_LOSS_INFO, sizeof(SLAVE_ISP_TARGET_LOSS_INFO));
				return;
			}
      if(CANopen_SDOC_State == CANopen_SDOC_Succes)
      {
        Buffercpy((uint8_t*)SLAVE_ACK_UID_INFO, UartRece.sendbuff, sizeof(SLAVE_ACK_UID_INFO));
        UartRece.sendbuff[5] = canRxMsg->data[4];
        UartRece.sendbuff[6] = canRxMsg->data[5];
        UartRece.sendbuff[7] = canRxMsg->data[6];
        UartRece.sendbuff[8] = canRxMsg->data[7];
        calc_checksum = calcCheckSum(UartRece.sendbuff, 7);
        UartRece.sendbuff[9] = (uint8_t)(calc_checksum);
        UartRece.sendbuff[10] = (uint8_t)(calc_checksum>>8);
        uartSendStr(UartRece.sendbuff, sizeof(SLAVE_ACK_UID_INFO));
      }
      // init parameter
      ispFlashCount.page = 0;
			led_status_flag = LED_ON;
			ReflashISPLED(led_status_flag);
		}
		else if(_cmd == FRAME_PROGRAM_DATA_CMD)
    {
			CanSTBEnable(); //enable CAN transmiter
			if(led_status_flag == LED_ON) led_status_flag = LED_OFF;
			else led_status_flag = LED_ON;
			ReflashISPLED(led_status_flag);
			
			CANopen_SDOC_BuffCount = 0;			/* Clear buffer */
			CANopen_SDOC_Buff = UartRece.curCMDbuff+RECE_UART_FRAME_FRONT_CODE_LEN+1;
			//CANopen_SDOC_Seg_BuffSize = UartRece.current_frame_len - RECE_UART_FRAME_CHECKSUM_BYTE_LEN-1;
			CANopen_SDOC_Seg_BuffSize = FLASH_PAGE_SIZE+4;//TBD
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(WRITE_RAM_ADDRESS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(START_DOWNLOAD_DATA_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_OK || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			
			while(CANopen_SDOC_BuffCount < CANopen_SDOC_Seg_BuffSize)
			{
				while(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_Busy);
				ispSegmentDown();
				fail=0;
				while((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(fail++<3))
				{
					repeatISPDataSend();
					while(!(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_OK || CANopen_SDOC_State == CANopen_SDOC_Fail));
				}
				if(fail>=3)
				{
					canErrorForceReset();
					uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO3, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO3));
					return;
				}
				//DELAY(2000);
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(PREPARE_SECTORS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			//CANopen_SDOC_Exp_ReadWrite(COPY_RAM_TO_FLASH_DST_ADD_INDEX);
			fail=0;
			do
			{
				CANopen_SDOC_Exp_WriteFlashAddress(ispFlashCount.page);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(COPY_RAM_TO_FLASH_SRC_ADD_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(COPY_RAM_TO_FLASH_LENGTH_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
				DELAY(500);
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			DELAY(1000);
			
      Buffercpy((uint8_t*)SLAVE_ACK_PAGE_DOWNLOAD_INFO, UartRece.sendbuff, sizeof(SLAVE_ACK_PAGE_DOWNLOAD_INFO));
			UartRece.sendbuff[5] = ispFlashCount.page;
			calc_checksum = calcCheckSum(UartRece.sendbuff, 4);
			UartRece.sendbuff[6] = (uint8_t)(calc_checksum);
			UartRece.sendbuff[7] = (uint8_t)(calc_checksum>>8);
			uartSendStr(UartRece.sendbuff, sizeof(SLAVE_ACK_PAGE_DOWNLOAD_INFO));
			
			ispFlashCount.page++;
			if(ispFlashCount.page > MAX_LPC11C24_FLASH_PAGE_NUMBER)
				uartSendStr(SLAVE_FLASH_PAGE_OVERFLOW_INFO, sizeof(SLAVE_FLASH_PAGE_OVERFLOW_INFO));
		}
		else if(_cmd == FRAME_ERASE_CHIP_CMD)
		{
			CanSTBEnable(); //enable CAN transmiter
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(FLASH_UNLOCK_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO));
				return;
			}
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(PREPARE_SECTORS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(ERASE_SECTORS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
				DELAY(50000);
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO2, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO2));
				return;
			}
			uartSendStr(SLAVE_ACK_ERASE_CHIP_INFO, sizeof(SLAVE_ACK_ERASE_CHIP_INFO));
			// init parameter
      ispFlashCount.page = 0;
		}
		else if(_cmd == FRAME_MASTER_GO_CMD)
		{
			CanSTBEnable(); //enable CAN transmiter
			
			led_status_flag = LED_ON;
			ReflashISPLED(led_status_flag);
			
			memset(UartRece.curCMDbuff, 0, MAX_RECE_CMD_FRAME_LEN);
			Buffercpy((uint8_t*)JumpCodeTable, UartRece.curCMDbuff, sizeof(JumpCodeTable));
			CANopen_SDOC_BuffCount = 0;			/* Clear buffer */
			CANopen_SDOC_Buff = UartRece.curCMDbuff;//+RECE_UART_FRAME_FRONT_CODE_LEN+1;
			//CANopen_SDOC_Seg_BuffSize = UartRece.current_frame_len - RECE_UART_FRAME_CHECKSUM_BYTE_LEN-1;
			CANopen_SDOC_Seg_BuffSize = sizeof(JumpCodeTable)+10;//FLASH_PAGE_SIZE;//TBD
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(WRITE_RAM_ADDRESS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(START_DOWNLOAD_DATA_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_OK || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			
			while(CANopen_SDOC_BuffCount < CANopen_SDOC_Seg_BuffSize)
			{
				while(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_Busy);
				ispSegmentDown();
				fail=0;
				while((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(fail++<3))
				{
					repeatISPDataSend();
					while(!(CANopen_SDOC_State == CANopen_SDOC_Seg_Write_OK || CANopen_SDOC_State == CANopen_SDOC_Fail));
				}
				if(fail>=3)
				{
					canErrorForceReset();
					uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
					return;
				}
				//DELAY(500);
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(FLASH_UNLOCK_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(WRITE_EXECUTION_ADDRESS_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			
			fail=0;
			do
			{
				CANopen_SDOC_Exp_ReadWrite(WRITE_PROGRAM_CONTROL_INDEX);
				while(!(CANopen_SDOC_State == CANopen_SDOC_Succes || CANopen_SDOC_State == CANopen_SDOC_Fail));
			} while ((CANopen_SDOC_State == CANopen_SDOC_Fail)&&(++fail<3));
			if(fail>=3)
			{
				canErrorForceReset();
				uartSendStr(SLAVE_ISP_TARGET_TIMEOUT_INFO1, sizeof(SLAVE_ISP_TARGET_TIMEOUT_INFO1));
				return;
			}
			
			uartSendStr(SLAVE_ACK_GO_INFO, sizeof(SLAVE_ACK_GO_INFO));
			DELAY(1000);
			CanSTBDisable(); //disable CAN transmiter
		}
//		else if(_cmd == FRAME_FIRMWARE_VERSION_CMD)
//		{
//			Buffercpy((uint8_t*)SLAVE_ACK_FIRMWARE_VERSION_INFO, UartRece.sendbuff, sizeof(SLAVE_ACK_FIRMWARE_VERSION_INFO));
//      calc_checksum = calcCheckSum(UartRece.sendbuff, 7);
//      UartRece.sendbuff[9] = (uint8_t)(calc_checksum);
//      UartRece.sendbuff[10] = (uint8_t)(calc_checksum>>8);
//      uartSendStr(UartRece.sendbuff, sizeof(SLAVE_ACK_FIRMWARE_VERSION_INFO));
//		}
	}
}

void uartReceDataAnalysis(void)
{
	while(UARTPostion != UARTCount)
  {
  	if(getReceFrameStatus() == START_BYTE_STATUS)
    {
			if (UARTBuffer[UARTPostion] == FRAME_START_BYTE_0X5A)
			{
				UartRece.frame_head = UARTPostion;
			  moveReceTailPointer(1);
			  modifyReceFrameStatus(FRAME_START_BYTE_0X5A_STATUS);
			}
			else
			{
				resetReceStatus();
			}
	  }
	  else if(getReceFrameStatus() == FRAME_START_BYTE_0X5A_STATUS)
	  {
			if (UARTBuffer[UARTPostion] == FRAME_START_BYTE_0XA5)
			{
			  moveReceTailPointer(1);
				modifyReceFrameStatus(FRAME_START_BYTE_0XA5_STATUS);
			}
			else
			{
				resetReceStatus();
			}
		}
		else if(getReceFrameStatus() == FRAME_START_BYTE_0XA5_STATUS)
		{
			if (UARTBuffer[UARTPostion] < FRAME_CMD_LIMIT)
			{
				modifyReceFrameStatus(CMD_BYTE_STATUS);
				moveReceTailPointer(1);
			}
			else
			{
				resetReceStatus();
			}
		}
		else if(getReceFrameStatus() == CMD_BYTE_STATUS)
		{
			UartRece.current_frame_len = UARTBuffer[UARTPostion];
			moveReceTailPointer(1);
			modifyReceFrameStatus(LENGTH_LOW_BYTE_STATUS);
		}
		else if(getReceFrameStatus() == LENGTH_LOW_BYTE_STATUS)
		{
			UartRece.current_frame_len |= (UARTBuffer[UARTPostion]<<8);
			UartRece.current_frame_len += RECE_UART_FRAME_CHECKSUM_BYTE_LEN; //TBD
			if((UartRece.current_frame_len > 0) ||
				 (UartRece.current_frame_len < MAX_RECE_FRAME_LEN))
		  {
			  moveReceTailPointer(1);
			  modifyReceFrameStatus(LENGTH_HIGH_BYTE_STATUS);
			}
			else
			{
				resetReceStatus();
			}
		}
		else if(getReceFrameStatus() == LENGTH_HIGH_BYTE_STATUS) //receive, timeout
		{
			if(UARTCount > UARTPostion)
			{
				if(UARTCount >= (UARTPostion+UartRece.current_frame_len))
				{
					moveReceTailPointer(UartRece.current_frame_len);
					copyCurrentCMDDataToBuff(UartRece.current_frame_len);
					modifyReceFrameStatus(START_BYTE_STATUS);
				}
			}
			else
			{
				if((UARTCount+UART_BUFSIZE) >= (UARTPostion+UartRece.current_frame_len))
				{
					moveReceTailPointer(UartRece.current_frame_len);
					copyCurrentCMDDataToBuff(UartRece.current_frame_len);//TBD,last data is not handled
					modifyReceFrameStatus(START_BYTE_STATUS);
				}
			}
		}
	}
}

void TIMER32_0_IRQHandler() //10ms
{
  if(LPC_TMR32B0->IR & 0x01)
  {    
    LPC_TMR32B0->IR = 1; /* clear interrupt flag */
		CANopen_10ms_tick();
  }
}

int main(void)
{
	SystemCoreClockUpdate();
  LEDinit();
	
	UARTInit(1000000);//115200
	
	UartRece.frame_head=0;
	UartRece.current_frame_len=0;
	modifyReceFrameStatus(START_BYTE_STATUS);
	modifyVaildFrame(IS_INVAILD);

	CANInit(100); 
	LPC_SYSCON->PRESETCTRL |= (0x1<<0);
	
	//timeout,use timer work. 
	init_timer32(0, TIME_INTERVAL(100));	//	10ms
	enable_timer32(0);
	
  while(1)
  {
//		while (UARTPostion!=UARTCount) //TO debug Uart receive
//	  {
//			UARTSend(&UARTBuffer[UARTPostion++], 1);
//			if (UARTPostion == UART_BUFSIZE)
//      {
//        UARTPostion = 0;		/* buffer overflow */
//      }
//		} 
		uartReceDataAnalysis();
		handleUartReceFrame();
  }
}
