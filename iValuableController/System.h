#ifndef _SYSTEM_H
#define _SYSTEM_H

#include "stm32f4xx_hal_conf.h"         // Keil::Device:STM32Cube Framework:Classic
#include "stm32f4xx.h"                  // Device header
#include "Driver_CAN.h"
#include "Driver_USART.h"               // ::CMSIS Driver:USART
#include "Driver_ETH_MAC.h"
#include "Driver_ETH_PHY.h"

#define FW_VERSION_MAJOR			0x01
#define FW_VERSION_MINOR			0x00

#define STATUS_PIN		GPIOC,GPIO_PIN_8

extern ARM_DRIVER_CAN 				Driver_CAN1;
extern ARM_DRIVER_USART 			Driver_USART1;
extern ARM_DRIVER_USART 			Driver_USART3;
extern ARM_DRIVER_ETH_PHY 		Driver_ETH_PHY0;

extern RNG_HandleTypeDef RNGHandle;
extern CRC_HandleTypeDef CRCHandle;

#define USER_ADDR 				0x08100000
#define BACK_ADDR 				0x08104000
#define USER_SIZE 				0x4000

#define SDRAM_BASE 				0xC0000000u
#define IS42S16400J_SIZE	0x00800000u

#define CANEX_HOST 				0x001

#define UNIT_TYPE_INDEPENDENT		0x80
#define UNIT_TYPE_UNITY					0x81
#define UNIT_TYPE_UNITY_RFID		0x82

extern const uint8_t *UserFlash;

#define CODE_TAG1		0xAA
#define CODE_TAG2		0xBB

static const uint32_t CODE_BASE[] = 
	{0x08180000, 0x081A0000, 0x081C0000, 0x081E0000};

#define CODE_SPACE_SIZE		0x00020000

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void HAL_MspInit(void);

void RequestTemperature(void);
	
void EraseCodeFlash(uint8_t num);
void EraseFlash(void);
void PrepareWriteFlash(uint32_t addr, uint32_t size);
	
void CRCReset();
//uint32_t CRCPush(uint32_t *pBuffer, uint32_t BufferLength);
uint32_t CRCPush(uint32_t word);
uint32_t GetCRCValue();
	
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#define GET_RANDOM_NUMBER HAL_RNG_GetRandomNumber(&RNGHandle)

#endif
