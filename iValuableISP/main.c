
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

#define FLASH_PAGE_SIZE 256

#define MAX_LPC11C24_FLASH_PAGE_NUMBER 96//128 //page=256byte,128Page*256=32K

enum StateType
{
	StateDelimiter1,
	StateDelimiter2,
	StateCommand,
	StateParameterLength,
	StateParameters,
	StateChecksum,
};

volatile uint8_t dataState = StateDelimiter1;
static uint8_t command=0;
static uint16_t parameterLen=0;
uint8_t parameters[300];

uint8_t uartCMDBuff[300];
static uint8_t uartcommand=0;
uint8_t is_vaild_frame;
uint8_t UARTSendbuff[100];

//is_vaild_frame
enum
{
	IS_INVAILD,
	IS_VAILD
};

#define FRAME_START_BYTE_0X5A 0x5A
#define FRAME_START_BYTE_0XA5 0xA5

enum
{
  FRAME_CMD_NULL,
  FRAME_LOOK_FOR_DEVICE_CMD, //0x01
  FRAME_DOWN_UPDATE_ERROR_CMD, //0x02  Slave
  FRAME_PROGRAM_DATA_CMD, //0x03
  FRAME_ERASE_CHIP_CMD, //0x04
  FRAME_MASTER_GO_CMD, //0x05
  FRAME_CMD_LIMIT
};

enum
{
	LED_OFF,
	LED_ON
};

const uint8_t JumpCodeTable[] = 
{
	0x40, 0xBA, 0x70, 0x47, 0xC0, 0xBA, 0x70, 0x47, 
	0xBF, 0xF3, 0x4F, 0x8F, 0x03, 0x49, 0x02, 0x48, 
	0xC8, 0x60, 0xBF, 0xF3, 0x4F, 0x8F, 0xFE, 0xE7, 
	0x04, 0x00, 0xFA, 0x05, 0x00, 0xED, 0x00, 0xE0
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

void modifyVaildFrame(uint8_t is_flag)
{
	is_vaild_frame=is_flag;
}

uint8_t getVaildFrame(void)
{
	return is_vaild_frame;
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
		_cmd = uartcommand;
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
				memcpy(UARTSendbuff, (uint8_t*)SLAVE_ACK_UID_INFO, sizeof(SLAVE_ACK_UID_INFO));
        UARTSendbuff[5] = canRxMsg->data[4];
        UARTSendbuff[6] = canRxMsg->data[5];
        UARTSendbuff[7] = canRxMsg->data[6];
        UARTSendbuff[8] = canRxMsg->data[7];
        calc_checksum = calcCheckSum(UARTSendbuff, 7);
        UARTSendbuff[9] = (uint8_t)(calc_checksum);
        UARTSendbuff[10] = (uint8_t)(calc_checksum>>8);
        uartSendStr(UARTSendbuff, sizeof(SLAVE_ACK_UID_INFO));
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
			CANopen_SDOC_Buff = uartCMDBuff+1;
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
			
			memcpy(UARTSendbuff, (uint8_t*)SLAVE_ACK_PAGE_DOWNLOAD_INFO, sizeof(SLAVE_ACK_PAGE_DOWNLOAD_INFO));
			UARTSendbuff[5] = ispFlashCount.page;
			calc_checksum = calcCheckSum(UARTSendbuff, 4);
			UARTSendbuff[6] = (uint8_t)(calc_checksum);
			UARTSendbuff[7] = (uint8_t)(calc_checksum>>8);
			uartSendStr(UARTSendbuff, sizeof(SLAVE_ACK_PAGE_DOWNLOAD_INFO));
			
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
			
			memcpy(uartCMDBuff, (uint8_t*)JumpCodeTable, sizeof(JumpCodeTable));
			CANopen_SDOC_BuffCount = 0;			/* Clear buffer */
			CANopen_SDOC_Buff = uartCMDBuff;
			CANopen_SDOC_Seg_BuffSize = sizeof(JumpCodeTable)+10;
			
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
	}
}

void uartReceDataAnalysis(void)
{
	uint8_t byte;
	static uint8_t index;
	static uint16_t parameterIndex;
	static uint16_t checksum;
	
	while(UARTPostion != UARTCount)
  {
		byte=UARTBuffer[UARTPostion++];
		if (UARTPostion>=UART_BUFSIZE)
			UARTPostion=0;
	  	switch (dataState)
			{
				case StateDelimiter1:
					if (byte==FRAME_START_BYTE_0X5A)
						dataState = StateDelimiter2;
					break;
				case StateDelimiter2:
					if (byte == FRAME_START_BYTE_0XA5)
						dataState = StateCommand;
					else
						dataState = StateDelimiter1;
					break;
				case StateCommand:
					checksum = byte;
				  command = byte;
					index = 0;
          parameterLen = 0;
					dataState = StateParameterLength;
					break;
				case StateParameterLength:
					checksum += byte;
					parameterLen |= (byte<<(index++*8));
					if (index>=2)
					{
						if (parameterLen == 0)
						{
							dataState = StateDelimiter1;
						}
						else
						{
							parameterIndex = 0;
							dataState = StateParameters;
						}
					}
					break;
				case StateParameters:
					checksum += byte;
					parameters[parameterIndex++] = byte;
					if (parameterIndex >= parameterLen)
					{
						index = 0;
						parameterIndex = 0;
						dataState = StateChecksum;
					}
					break;
				case StateChecksum:
					parameterIndex |= (byte<<(index++*8));
					if (index>=2)
					{
						if (parameterIndex==checksum)
						{
							memset(uartCMDBuff, 0, parameterLen+1);
							memcpy(uartCMDBuff, parameters, parameterLen);
							uartcommand=command;
							modifyVaildFrame(IS_VAILD);
						}
						else
						{
							uartSendStr(SLAVE_CHECKSUM_ERROR_INFO, sizeof(SLAVE_CHECKSUM_ERROR_INFO)); //check sum error
						}
						dataState = StateDelimiter1;
					}
					break;
				default:
					break;
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
	
	UARTInit(1000000);

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
