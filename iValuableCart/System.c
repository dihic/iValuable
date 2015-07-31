#include "stm32f4xx.h"                  // Device header
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "System.h"
#include "trf796x.h"
#include <string.h>



RNG_HandleTypeDef RNGHandle = { RNG, 
																HAL_UNLOCKED,
																HAL_RNG_STATE_RESET };

TIM_HandleTypeDef TimHandle;

const uint8_t *UserFlash = (const uint8_t *)USER_ADDR;

//SYSCLK       168000000
//AHB Clock    168000000
//APB1 Clock   42000000                  
//APB2 Clock   84000000

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */
	
uint8_t rfid_buf[300];
volatile uint8_t i_reg;
volatile uint8_t irq_flag;
volatile uint8_t rxtx_state;
volatile uint8_t rx_error_flag;
volatile uint8_t stand_alone_flag;
volatile uint8_t host_control_flag;	

void InitSystemClock()
{
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = HSE_VALUE/1000000;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

	HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1); //Use HSE as MCO Source with same frequency
	SystemCoreClockUpdate();
}

void IOSetup()
{
	GPIO_InitTypeDef gpioType = { GPIO_PIN_All, 
																GPIO_MODE_OUTPUT_PP,
																GPIO_NOPULL,
																GPIO_SPEED_HIGH,
																0 };
	
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOE_CLK_ENABLE();
	
	//Configure PE0 as WP
	//Configure PE1 as HOLD
	gpioType.Pin = GPIO_PIN_0 | GPIO_PIN_1;
	gpioType.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOE, &gpioType);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);	//Source WP Set
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);	//Source HOLD Set

	//Configure PA8 as MCO1
	gpioType.Pin = GPIO_PIN_8;
	gpioType.Mode = GPIO_MODE_AF_PP;
	gpioType.Alternate = GPIO_AF0_MCO;
	gpioType.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &gpioType);
	HAL_Delay(1);
	
	//Configure PC0 as LAN Reset
	gpioType.Pin = GPIO_PIN_0;
	gpioType.Mode = GPIO_MODE_OUTPUT_PP;
	gpioType.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOC, &gpioType);
	
	//Configure PC8 as system heartbeat
	gpioType.Pin = GPIO_PIN_8;
	gpioType.Mode = GPIO_MODE_OUTPUT_PP;
	gpioType.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &gpioType);
	
	//Reset Network
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
	osDelay(20);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
	osDelay(20);	// require >10ms
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
	
	HAL_NVIC_SetPriority(RFID_EXT_IRQ, 15, 0);
  HAL_NVIC_EnableIRQ(RFID_EXT_IRQ);
}

void RFID_EXT_HANDLER(void)
{
	HAL_GPIO_EXTI_IRQHandler(PIN_RFID_IRQ);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == PIN_RFID_IRQ)
		Trf796xIrqHandler();
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
	__TIM2_CLK_ENABLE();
	HAL_NVIC_SetPriority(TIM2_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}


void HAL_MspInit(void)
{
	InitSystemClock();
	IOSetup();
	
	__RNG_CLK_ENABLE();
	HAL_RNG_Init(&RNGHandle);
	
	TrfTimerHandle = &TimHandle;
	
	TimHandle.Instance = TIM2;
	TimHandle.Channel = HAL_TIM_ACTIVE_CHANNEL_1;
	TimHandle.Init.Prescaler = (SystemCoreClock>>1)/1000000;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.Period = 1000; //1ms
	TimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	TimHandle.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&TimHandle);
}

void TIM2_IRQHandler(void)
{
  if (__HAL_TIM_GET_ITSTATUS(&TimHandle, TIM_IT_CC1) != RESET)
  {
    __HAL_TIM_CLEAR_IT(&TimHandle, TIM_IT_CC1);
    Trf796xTimerHandler();
  }
  
}

void HAL_Delay(__IO uint32_t Delay)
{
	osDelay(Delay);
}

static const FLASH_EraseInitTypeDef EraseInitTypeBackup = {
	TYPEERASE_SECTORS,
	0,
	FLASH_SECTOR_10,
	1,
	VOLTAGE_RANGE_3
};

static const FLASH_EraseInitTypeDef EraseInitTypeCurrent = {
	TYPEERASE_SECTORS,
	0,
	FLASH_SECTOR_11,
	1,
	VOLTAGE_RANGE_3
};

void EraseFlash(void)
{
	uint32_t error;
	HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef *)(&EraseInitTypeCurrent), &error);
}

void PrepareWriteFlash(uint32_t addr, uint32_t size)
{
	uint32_t error;
	uint32_t i = 0;
	uint32_t limit = addr+size;
	const uint32_t *word;
	const uint8_t *byte;
	HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef *)(&EraseInitTypeBackup), &error);
	while (i<0x10000)
	{
		if (i<addr)
		{
			if (i+4<=addr)
			{
				word = (uint32_t *)(USER_ADDR+i);
				HAL_FLASH_Program(TYPEPROGRAM_WORD, BACK_ADDR+i, *word);
				i+=4;
				continue;
			}
			else
			{
				while(i<addr)
				{
					byte = (uint8_t *)(USER_ADDR+i);
					HAL_FLASH_Program(TYPEPROGRAM_BYTE, BACK_ADDR+i, *byte);
					++i;
				}
				i = limit & (~0x3);				
			}
		}
		else if (i==addr)
			i = limit & (~0x3);
		if (i<limit && i+4>limit)
		{
			error = i+4 - limit;
			i = limit;
			while(error>0)
			{
				byte = (uint8_t *)(USER_ADDR+i);
				HAL_FLASH_Program(TYPEPROGRAM_BYTE, BACK_ADDR+i, *byte);
				--error;
				++i;
			}
		}
		else
		{
			word = (uint32_t *)(USER_ADDR+i);
			HAL_FLASH_Program(TYPEPROGRAM_WORD, BACK_ADDR+i, *word);
			i+=4;
		}
	}
	HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef *)(&EraseInitTypeCurrent), &error);
	i = 0;
	while (i<0x10000)
	{
		word = (uint32_t *)(BACK_ADDR+i);
		HAL_FLASH_Program(TYPEPROGRAM_WORD, USER_ADDR+i, *word);
		i+=4;
	}
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */

