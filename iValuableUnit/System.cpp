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
	
	#if ADDR_TYPE!=ADDR_SWITCH_10BITS
	  uint8_t id = LPC_GPIO[PORT1]->MASKED_ACCESS[0x7]; //ADDR2 ADDR1 ADDR0
	#else
	  uint16_t id = LPC_GPIO[PORT1]->MASKED_ACCESS[0x7];
	  GPIOSetDir(PORT3, 2, E_IO_INPUT);	//ADDR7 P3.2
	  GPIOSetDir(PORT3, 1, E_IO_INPUT);	//ADDR8 P3.1
	  if (GPIOGetValue(PORT3,2)) //ADDR7
		  id|=0x080;
	  if (GPIOGetValue(PORT3,1)) //ADDR8
		  id|=0x100;
	#endif
		
		if (GPIOGetValue(PORT1,4)) //ADDR3
			id|=0x08;
		if (GPIOGetValue(PORT1,5)) //ADDR4
			id|=0x10;
	#else
		LPC_GPIO[PORT2]->DIR &= ~(0xDC0); //ADDR0-4
		GPIOSetDir(PORT1, 10, E_IO_INPUT);	//ADDR5
		GPIOSetDir(PORT1, 11, E_IO_INPUT);	//ADDR6
		
		#if ADDR_TYPE!=ADDR_SWITCH_10BITS
		uint8_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
		id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
		#else
		uint16_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
		id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
		GPIOSetDir(PORT1,  8, E_IO_INPUT);	//ADDR7
		GPIOSetDir(PORT0, 11, E_IO_INPUT);	//ADDR8
		if (GPIOGetValue(PORT1,8)) //ADDR7
		  id|=0x080;
	  if (GPIOGetValue(PORT0,11)) //ADDR8
		  id|=0x100;
		#endif
		
	#endif
		GPIOSetDir(ENABLE_LOCKER, E_IO_INPUT); //ADDR7-->ADDR9
		
#else
  LPC_GPIO[PORT2]->DIR &= ~(0xDC0); //ADDR0-4
	GPIOSetDir(PORT1, 10, E_IO_INPUT);	//ADDR5
	GPIOSetDir(PORT1, 11, E_IO_INPUT);	//ADDR6	
	
	#if ADDR_TYPE!=ADDR_SWITCH_10BITS
	uint8_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
	id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
	#else
	uint16_t id = (LPC_GPIO[PORT2]->MASKED_ACCESS[0x1C0])>>6;
	id |= (LPC_GPIO[PORT2]->MASKED_ACCESS[0xC00])>>7;
	GPIOSetDir(PORT1,  8, E_IO_INPUT);	//ADDR7
	GPIOSetDir(PORT0, 11, E_IO_INPUT);	//ADDR8
	if (GPIOGetValue(PORT1,8)) //ADDR7
		id|=0x080;
	if (GPIOGetValue(PORT0,11)) //ADDR8
		id|=0x100;
	#endif
	
	GPIOSetDir(ENABLE_LOCKER, E_IO_INPUT); //ADDR7-->ADDR9
#endif
		
	if (GPIOGetValue(PORT1,10)) //ADDR5
		id|=0x20;
	if (GPIOGetValue(PORT1,11)) //ADDR6
		id|=0x40;
	
	#if ADDR_TYPE!=ADDR_SWITCH_10BITS
	if (GPIOGetValue(ENABLE_LOCKER)) //ADDR7
		id|=0x80;
	NodeId = 0x100 | id;
	#else
	if (GPIOGetValue(ENABLE_LOCKER)) //ADDR9
		id|=0x200;
	NodeId = 0x400 | id;
	#endif
	
//	GPIOSetDir(LATCH_LE, E_IO_OUTPUT);
//	GPIOSetDir(LATCH_D, E_IO_OUTPUT);
//	GPIOSetDir(LATCH_Q1, E_IO_INPUT);
//	GPIOSetDir(LATCH_Q2, E_IO_INPUT);
//	//Normal start in next power cycle
//	SetDLatch();
	
	GPIOSetDir(CON_LOCKER, E_IO_OUTPUT);
	GPIOSetDir(CON_LED, E_IO_OUTPUT);
	GPIOSetDir(FB_LOCKER, E_IO_INPUT);

#if LOCKER_TYPE==SOUTHCO_LOCKER
	GPIOSetDir(FB_DOOR, E_IO_INPUT);
#endif
	
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



