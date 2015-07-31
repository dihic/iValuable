//***************************************************************
//------------<02.Dec.2010 by Peter Reiser>----------------------
//***************************************************************

#ifndef _TRF796X_H_
#define _TRF796X_H_

//===============================================================

#include <stdint.h>
#include "stm32f4xx.h"                  // Device header
#include "stm32f4xx_hal.h"
#include "Driver_SPI.h"
//#include "stm32f4xx_hal_gpio.h"

#include "iso14443a.h"


//===============================================================

#define PORT_RFID_IRQ 		GPIOE
#define PORT_RFID_EN			GPIOE
#define PORT_RFID_CS			GPIOA

#define PIN_RFID_IRQ 		GPIO_PIN_1
#define PIN_RFID_EN			GPIO_PIN_0
#define PIN_RFID_CS			GPIO_PIN_4

//#define RFID_EXT_CH			1

#define RFID_EXT_HANDLER			EXTI1_IRQHandler
#define RFID_EXT_IRQ					EXTI1_IRQn

#define Driver_RFID		Driver_SPI1

#define DBG	0						// if DBG 1 interrupts are display

//==== TRF796x definitions ======================================

#define BIT0                   (0x0001)
#define BIT1                   (0x0002)
#define BIT2                   (0x0004)
#define BIT3                   (0x0008)
#define BIT4                   (0x0010)
#define BIT5                   (0x0020)
#define BIT6                   (0x0040)
#define BIT7                   (0x0080)
#define BIT8                   (0x0100)
#define BIT9                   (0x0200)
#define BITA                   (0x0400)
#define BITB                   (0x0800)
#define BITC                   (0x1000)
#define BITD                   (0x2000)
#define BITE                   (0x4000)
#define BITF                   (0x8000)

#define PREFIX_COMMAND 0x80
#define ADDR_READ 0x40
#define ADDR_WRITE 0x00
#define CONTINUOUS_ADDR_MODE 0x20

//---- Direct commands ------------------------------------------

#define IDLE						0x00
#define SOFT_INIT					0x03
#define INITIAL_RF_COLLISION		0x04
#define RESPONSE_RF_COLLISION_N		0x05
#define RESPONSE_RF_COLLISION_0		0x06
#define	RESET						0x0F
#define TRANSMIT_NO_CRC				0x10
#define TRANSMIT_CRC				0x11
#define DELAY_TRANSMIT_NO_CRC		0x12
#define DELAY_TRANSMIT_CRC			0x13
#define TRANSMIT_NEXT_SLOT			0x14
#define CLOSE_SLOT_SEQUENCE			0x15
#define STOP_DECODERS				0x16
#define RUN_DECODERS				0x17
#define CHECK_INTERNAL_RF			0x18
#define CHECK_EXTERNAL_RF			0x19
#define ADJUST_GAIN					0x1A

//---- Reader register ------------------------------------------

#define CHIP_STATE_CONTROL			0x00
#define ISO_CONTROL					0x01
#define ISO_14443B_OPTIONS			0x02
#define ISO_14443A_OPTIONS			0x03
#define TX_TIMER_EPC_HIGH			0x04
#define TX_TIMER_EPC_LOW			0x05
#define TX_PULSE_LENGTH_CONTROL		0x06
#define RX_NO_RESPONSE_WAIT_TIME	0x07
#define RX_WAIT_TIME				0x08
#define MODULATOR_CONTROL			0x09
#define RX_SPECIAL_SETTINGS			0x0A
#define REGULATOR_CONTROL			0x0B
#define IRQ_STATUS					0x0C	// IRQ Status Register
#define IRQ_MASK					0x0D	// Collision Position and Interrupt Mask Register
#define	COLLISION_POSITION			0x0E
#define RSSI_LEVELS					0x0F
#define SPECIAL_FUNCTION			0x10
#define RAM_START_ADDRESS			0x11	//RAM is 6 bytes long (0x11 - 0x16)
#define NFCID						0x17
#define NFC_TArGET_LEVEL			0x18
#define NFC_TARGET_PROTOCOL			0x19
#define TEST_SETTINGS_1				0x1A
#define TEST_SETTINGS_2				0x1B
#define FIFO_STATUS					0x1C
#define TX_LENGTH_BYTE_1			0x1D
#define TX_LENGTH_BYTE_2			0x1E
#define FIFO						0x1F

//===============================================================

#ifdef __cplusplus
extern "C" {
#endif

#define IRQ_ASSERT					__HAL_GPIO_EXTI_GET_IT(PIN_RFID_IRQ)
#define IRQ_CLR							__HAL_GPIO_EXTI_CLEAR_FLAG(PIN_RFID_IRQ)
#define IRQ_PIN_HIGH 				(HAL_GPIO_ReadPin(PORT_RFID_IRQ, PIN_RFID_IRQ)==GPIO_PIN_SET)
#define IRQ_ON 							EXTI->IMR |=  (uint32_t)PIN_RFID_IRQ
#define IRQ_OFF 						EXTI->IMR &=~((uint32_t)PIN_RFID_IRQ)

extern TIM_HandleTypeDef *TrfTimerHandle;
	
#define STOP_TIMER 			Trf796xStopTimer()
#define START_TIMER(ms) Trf796xStartTimer(ms)

void Trf796xStartTimer(uint32_t ms);
void Trf796xStopTimer(void);

void Trf796xTimerHandler(void);
void Trf796xIrqHandler(void);
	
void Trf796xCommunicationSetup(void);
void Trf796xDirectCommand(uint8_t *pbuf,int needDummy);
//void Trf796xDirectMode(void);
void Trf796xDisableSlotCounter(void);
void Trf796xEnableSlotCounter(void);
void Trf796xInitialSettings(void);
void Trf796xRawWrite(uint8_t *pbuf, uint8_t length);
void Trf796xReConfig(void);
void Trf796xReadCont(uint8_t *pbuf, uint8_t length);
void Trf796xReadIrqStatus(uint8_t *pbuf);
void Trf796xReadSingle(uint8_t *pbuf, uint8_t length);
void Trf796xReset(void);
void Trf796xResetIrqStatus(void);
void Trf796xRunDecoders(void);
void Trf796xStopDecoders(void);
void Trf796xTransmitNextSlot(void);
void Trf796xTurnRfOff(void);
void Trf796xTurnRfOn(void);
void Trf796xWriteCont(uint8_t *pbuf, uint8_t length);
void Trf796xWriteIsoControl(uint8_t iso_control);
void Trf796xWriteSingle(uint8_t *pbuf, uint8_t length);

#ifdef __cplusplus
}
#endif

//===============================================================

#endif
