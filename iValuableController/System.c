#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "System.h"
#include <string.h>
#include <rt_heap.h>

#define TEMP_AVG_SLOPE_INV 		400	// 2.5mV/degree
#define TEMP_V25							0.76f	// unit:  V
#define TEMP_AVG_WINDOW				10

RNG_HandleTypeDef RNGHandle = { RNG, 
																HAL_UNLOCKED,
																HAL_RNG_STATE_RESET };

CRC_HandleTypeDef CRCHandle = { CRC, 
																HAL_UNLOCKED,
																HAL_CRC_STATE_RESET };

ADC_HandleTypeDef hAdc1;

const uint8_t *UserFlash = (const uint8_t *)USER_ADDR;

//SYSCLK       180000000
//AHB Clock    180000000
//APB1 Clock   45000000                  
//APB2 Clock   90000000
																
#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** System Clock Configuration
*/
void SystemClock_Config(void)
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
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  HAL_PWREx_ActivateOverDrive();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
	HAL_RCC_EnableCSS();
	SystemCoreClockUpdate();
}

void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */

  __GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
  __GPIOD_CLK_ENABLE();
  __GPIOE_CLK_ENABLE();
	__GPIOF_CLK_ENABLE();
	__GPIOG_CLK_ENABLE();
	//__GPIOH_CLK_ENABLE();

  //Configure PA8 as MCO1
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	//Configure PC8 as system heartbeat
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  FMC SDRAM Mode definition register defines
  */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000) 
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)     

/* FMC initialization function */
void MX_FMC_Init(void)
{
	SDRAM_HandleTypeDef hsdram1;
	FMC_SDRAM_TimingTypeDef SdramTiming;
	FMC_SDRAM_CommandTypeDef command;

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_2;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 2;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  HAL_SDRAM_Init(&hsdram1, &SdramTiming);
	
	/* Step 3 --------------------------------------------------------------------*/
  /* Configure a clock configuration enable command */
  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
	while (HAL_SDRAM_SendCommand(&hsdram1, &command, 1000)!=HAL_OK) {}
	
	/* Step 4 --------------------------------------------------------------------*/
  /* Insert 100 ms delay */
  osDelay(100);
	
	/* Step 5 --------------------------------------------------------------------*/
  /* Configure a PALL (precharge all) command */ 
  command.CommandMode = FMC_SDRAM_CMD_PALL;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  while (HAL_SDRAM_SendCommand(&hsdram1, &command, 1000)!=HAL_OK) {}
	
	/* Step 6 --------------------------------------------------------------------*/
  /* Configure a Auto-Refresh command */ 
	command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  command.AutoRefreshNumber = 4;
  command.ModeRegisterDefinition = 0;
	/* Send the  first command */
  while (HAL_SDRAM_SendCommand(&hsdram1, &command, 1000)!=HAL_OK) {}
	/* Send the second command */
	while (HAL_SDRAM_SendCommand(&hsdram1, &command, 1000)!=HAL_OK) {}
	
	/* Step 7 --------------------------------------------------------------------*/
  /* Configure a load Mode register command*/ 
  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 
				(uint32_t)SDRAM_MODEREG_BURST_LENGTH_2          |
                  SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                  ((hsdram1.Init.CASLatency == FMC_SDRAM_CAS_LATENCY_2) ? 
									SDRAM_MODEREG_CAS_LATENCY_2 : SDRAM_MODEREG_CAS_LATENCY_3) |
                  SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                  SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
  while (HAL_SDRAM_SendCommand(&hsdram1, &command, 1000)!=HAL_OK) {}
	
	
	/* Step 8 --------------------------------------------------------------------*/
  /* Set the refresh rate counter */
  /* (15.62 us x Freq) - 20 */
  /* Set the device refresh counter */
	HAL_SDRAM_ProgramRefreshRate(&hsdram1, 1386);
}

/* ADC1 init function */
void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig;

    /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
    */
  hAdc1.Instance = ADC1;
  hAdc1.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
  hAdc1.Init.Resolution = ADC_RESOLUTION12b;
  hAdc1.Init.ScanConvMode = DISABLE;
  hAdc1.Init.ContinuousConvMode = DISABLE;
  hAdc1.Init.DiscontinuousConvMode = DISABLE;
  hAdc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hAdc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hAdc1.Init.NbrOfConversion = 1;
  hAdc1.Init.DMAContinuousRequests = ENABLE;
  hAdc1.Init.EOCSelection = EOC_SINGLE_CONV;
  HAL_ADC_Init(&hAdc1);

    /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
    */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  HAL_ADC_ConfigChannel(&hAdc1, &sConfig);

}

volatile float CurrentTemperature = 0;


void ADC_IRQHandler(void)
{
	HAL_ADC_IRQHandler(&hAdc1);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	static uint8_t tempIndex = 0xff;
	static uint32_t templist[TEMP_AVG_WINDOW];
	static uint32_t tempSum = 0;

	uint8_t i;
	uint32_t data = HAL_ADC_GetValue(hadc);
	
	if (tempIndex == 0xff)
	{
		for(i=0;i<TEMP_AVG_WINDOW;++i)
			tempSum+=(templist[i]=data);
		tempIndex=0;
	}
	else
	{
		tempSum-=templist[tempIndex];
		tempSum+=(templist[tempIndex++]=data);
		if (tempIndex>=TEMP_AVG_WINDOW)
			tempIndex = 0;
	}
	
	CurrentTemperature = (tempSum*3.3f/(0xfff*TEMP_AVG_WINDOW)-TEMP_V25)*TEMP_AVG_SLOPE_INV + 25;
}

void RequestTemperature()
{
	HAL_ADC_Start_IT(&hAdc1);
}

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{	
	SystemClock_Config();
	
	//Insert 100 ms delay, wait for voltage stable
	osDelay(100);
	
	HAL_RNG_Init(&RNGHandle);
	HAL_CRC_Init(&CRCHandle);
	
	MX_GPIO_Init();
	
	//SDRAM Init and Heap Realloction
	MX_FMC_Init();
	_init_alloc(SDRAM_BASE, SDRAM_BASE+IS42S16400J_SIZE);
	
	MX_ADC1_Init();
}

void HAL_Delay(__IO uint32_t Delay)
{
	osDelay(Delay);
}

void HAL_CRC_MspInit(CRC_HandleTypeDef *hcrc)
{
	if(hcrc->Instance==CRC)
		__CRC_CLK_ENABLE();
}

void HAL_RNG_MspInit(RNG_HandleTypeDef* hrng)
{
  if(hrng->Instance==RNG)
    /* Peripheral clock enable */
    __RNG_CLK_ENABLE();
}

void HAL_RNG_MspDeInit(RNG_HandleTypeDef* hrng)
{
  if(hrng->Instance==RNG)
    /* Peripheral clock disable */
    __RNG_CLK_DISABLE();
}

static int FMC_Initialized = 0;

static void HAL_FMC_MspInit(void){
  GPIO_InitTypeDef GPIO_InitStruct;
  if (FMC_Initialized)
    return;
  FMC_Initialized = 1;
  /* Peripheral clock enable */
  __FMC_CLK_ENABLE();
  
  /** FMC GPIO Configuration  
  PF0   ------> FMC_A0
  PF1   ------> FMC_A1
  PF2   ------> FMC_A2
  PF3   ------> FMC_A3
  PF4   ------> FMC_A4
  PF5   ------> FMC_A5
  PC0   ------> FMC_SDNWE
  PC2   ------> FMC_SDNE0
  PC3   ------> FMC_SDCKE0
  PF11   ------> FMC_SDNRAS
  PF12   ------> FMC_A6
  PF13   ------> FMC_A7
  PF14   ------> FMC_A8
  PF15   ------> FMC_A9
  PG0   ------> FMC_A10
  PG1   ------> FMC_A11
  PE7   ------> FMC_D4
  PE8   ------> FMC_D5
  PE9   ------> FMC_D6
  PE10   ------> FMC_D7
  PE11   ------> FMC_D8
  PE12   ------> FMC_D9
  PE13   ------> FMC_D10
  PE14   ------> FMC_D11
  PE15   ------> FMC_D12
  PD8   ------> FMC_D13
  PD9   ------> FMC_D14
  PD10   ------> FMC_D15
  PD14   ------> FMC_D0
  PD15   ------> FMC_D1
  PG4   ------> FMC_BA0
  PG5   ------> FMC_BA1
  PG8   ------> FMC_SDCLK
  PD0   ------> FMC_D2
  PD1   ------> FMC_D3
  PG15   ------> FMC_SDNCAS
  PE0   ------> FMC_NBL0
  PE1   ------> FMC_NBL1
  */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_11|GPIO_PIN_12 
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
	//GPIOC->OTYPER |= 1; //Set Open-Drain for C0

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5 
                          |GPIO_PIN_8|GPIO_PIN_15;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10 
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14 
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14 
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* Peripheral interrupt Init */
  HAL_NVIC_SetPriority(FMC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FMC_IRQn);
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef* hsdram){
  HAL_FMC_MspInit();
}

static int FMC_DeInitialized = 0;

static void HAL_FMC_MspDeInit(void){
  if (FMC_DeInitialized)
    return;
  FMC_DeInitialized = 1;
  /* Peripheral clock enable */
  __FMC_CLK_DISABLE();
  
  /** FMC GPIO Configuration  
  PF0   ------> FMC_A0
  PF1   ------> FMC_A1
  PF2   ------> FMC_A2
  PF3   ------> FMC_A3
  PF4   ------> FMC_A4
  PF5   ------> FMC_A5
  PC0   ------> FMC_SDNWE
  PC2   ------> FMC_SDNE0
  PC3   ------> FMC_SDCKE0
  PF11   ------> FMC_SDNRAS
  PF12   ------> FMC_A6
  PF13   ------> FMC_A7
  PF14   ------> FMC_A8
  PF15   ------> FMC_A9
  PG0   ------> FMC_A10
  PG1   ------> FMC_A11
  PE7   ------> FMC_D4
  PE8   ------> FMC_D5
  PE9   ------> FMC_D6
  PE10   ------> FMC_D7
  PE11   ------> FMC_D8
  PE12   ------> FMC_D9
  PE13   ------> FMC_D10
  PE14   ------> FMC_D11
  PE15   ------> FMC_D12
  PD8   ------> FMC_D13
  PD9   ------> FMC_D14
  PD10   ------> FMC_D15
  PD14   ------> FMC_D0
  PD15   ------> FMC_D1
  PG4   ------> FMC_BA0
  PG5   ------> FMC_BA1
  PG8   ------> FMC_SDCLK
  PD0   ------> FMC_D2
  PD1   ------> FMC_D3
  PG15   ------> FMC_SDNCAS
  PE0   ------> FMC_NBL0
  PE1   ------> FMC_NBL1
  */
  HAL_GPIO_DeInit(GPIOF, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_11|GPIO_PIN_12 
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15);

  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3);

  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5 
                          |GPIO_PIN_8|GPIO_PIN_15);

  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10 
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14 
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1);

  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14 
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1);

  /* Peripheral interrupt DeInit */
  HAL_NVIC_DisableIRQ(FMC_IRQn);
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef* hsdram){
  HAL_FMC_MspDeInit();
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance==ADC1)
  {
    /* Peripheral clock enable */
    __ADC1_CLK_ENABLE();
  }
  HAL_NVIC_EnableIRQ(ADC_IRQn);
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance==ADC1)
  {
    /* Peripheral clock disable */
    __ADC1_CLK_DISABLE();
	}
	HAL_NVIC_DisableIRQ(ADC_IRQn);
}

static const FLASH_EraseInitTypeDef EraseInitTypeBackup = {
	TYPEERASE_SECTORS,
	FLASH_BANK_2,
	FLASH_SECTOR_13,
	1,
	VOLTAGE_RANGE_3
};

static const FLASH_EraseInitTypeDef EraseInitTypeCurrent = {
	TYPEERASE_SECTORS,
	FLASH_BANK_2,
	FLASH_SECTOR_12,
	1,
	VOLTAGE_RANGE_3
};

void EraseFlash(void)
{
	uint32_t error;
	HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef *)(&EraseInitTypeCurrent), &error);
}

static const uint32_t CODE_SECTORS[] = 
	{FLASH_SECTOR_20, FLASH_SECTOR_21, FLASH_SECTOR_22, FLASH_SECTOR_23};

void EraseCodeFlash(uint8_t num)
{
	uint32_t error;
	FLASH_EraseInitTypeDef eraseInit = {
		TYPEERASE_SECTORS,
		FLASH_BANK_2,
		CODE_SECTORS[num],
		1,
		VOLTAGE_RANGE_3
	};
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&eraseInit, &error);
	HAL_FLASH_Lock();
}

void PrepareWriteFlash(uint32_t addr, uint32_t size)
{
	uint32_t error;
	uint32_t i = 0;
	uint32_t limit = addr+size;
	const uint32_t *word;
	const uint8_t *byte;
	HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef *)(&EraseInitTypeBackup), &error);
	while (i<USER_SIZE)
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
	while (i<USER_SIZE)
	{
		word = (uint32_t *)(BACK_ADDR+i);
		HAL_FLASH_Program(TYPEPROGRAM_WORD, USER_ADDR+i, *word);
		i+=4;
	}
}

void CRCReset()
{
	HAL_CRC_Calculate(&CRCHandle, NULL, 0);
}

//uint32_t CRCPush(uint32_t *pBuffer, uint32_t BufferLength)
//{
//	 return HAL_CRC_Accumulate(&CRCHandle, pBuffer, BufferLength);
//}

uint32_t CRCPush(uint32_t word)
{
	 return HAL_CRC_Accumulate(&CRCHandle, &word, 1);
}

uint32_t GetCRCValue()
{
	return CRCHandle.Instance->DR ^ 0x0u;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */

