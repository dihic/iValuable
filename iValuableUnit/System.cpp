#include "System.h"

#include <LPC11xx.h>
#include "gpio.h"
#include "delay.h"
#include "canonchip.h"

//void SetDLatch()
//{
//	DrvGPIO_ClrBit(LATCH_LE);
//	DrvGPIO_SetBit(LATCH_D);
//	DELAY(1000);
//	DrvGPIO_SetBit(LATCH_LE);
//}

//void ClearDLatch()
//{
//	DrvGPIO_ClrBit(LATCH_LE);
//	DrvGPIO_ClrBit(LATCH_D);
//	DELAY(1000);
//	DrvGPIO_SetBit(LATCH_LE);
//}

void SystemSetup()
{
	GPIOInit();
	
#if UNIT_TYPE!=UNIT_TYPE_LOCKER
	#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		LPC_GPIO[PORT1]->DIR &= ~(0xC37); //ADDR0-6
	#else
		LPC_GPIO[PORT2]->DIR &= ~(0xDC0); //ADDR0-4
		GPIOSetDir(PORT1, 10, E_IO_INPUT);	//ADDR5
		GPIOSetDir(PORT1, 11, E_IO_INPUT);	//ADDR6
	#endif
		
		GPIOSetDir(ENABLE_LOCKER, E_IO_INPUT); //ADDR7
		
		//Obtain NodeId
	#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		uint8_t id = LPC_GPIO[PORT1]->MASKED_ACCESS[0x7];
		if (GPIOGetValue(PORT1,4))
			id|=0x08;
		if (GPIOGetValue(PORT1,5))
			id|=0x10;
	#else
		uint8_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
		id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
	#endif
#else
  LPC_GPIO[PORT2]->DIR &= ~(0xDC0); //ADDR0-4
	GPIOSetDir(PORT1, 10, E_IO_INPUT);	//ADDR5
	GPIOSetDir(PORT1, 11, E_IO_INPUT);	//ADDR6
	
	GPIOSetDir(ENABLE_LOCKER, E_IO_INPUT); //ADDR7
	
	uint8_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
	id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
#endif
	if (GPIOGetValue(PORT1,10))
		id|=0x20;
	if (GPIOGetValue(PORT1,11))
		id|=0x40;
	if (GPIOGetValue(ENABLE_LOCKER))
		id|=0x80;
	NodeId = 0x100 | id;
	
//	GPIOSetDir(LATCH_LE, E_IO_OUTPUT);
//	GPIOSetDir(LATCH_D, E_IO_OUTPUT);
//	GPIOSetDir(LATCH_Q1, E_IO_INPUT);
//	GPIOSetDir(LATCH_Q2, E_IO_INPUT);
//	//Normal start in next power cycle
//	SetDLatch();
	
	GPIOSetDir(CON_LOCKER, E_IO_OUTPUT);
	GPIOSetDir(CON_LED, E_IO_OUTPUT);
	GPIOSetDir(FB_LOCKER, E_IO_INPUT);
	
	LED_OFF;
	LOCKER_OFF;
}


//void EnterISP()
//{
//	//Enter CAN-ISP in next power cycle
//	ClearDLatch();
//	DELAY(1000);
//	NVIC_SystemReset();
//}



