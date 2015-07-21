
#include "LPC11xx.h"
#include "timer32.h"
#include "gpio.h"
#include "math.h"
#include <string.h>
#include "System.h"
#include "delay.h"

#define MODE_DELAY

#ifdef MODE_DELAY
#define SWITCH_DELAY 5*1000
#define LOCKER_ON_TIME 500*1000
#define LOCKER_OFF_TIME 500*1000
#endif

//enum
//{
//	KEY_INPUT_RELEASE,
//	KEY_INPUT_PUSH
//};

//enum
//{
//	SWITCH_UNLOCK,
//	SWITCH_LOCK
//};

//volatile uint8_t key_status=KEY_INPUT_RELEASE;
//volatile uint8_t switch_lock=SWITCH_UNLOCK;

#ifndef MODE_DELAY
void TIMER32_0_IRQHandler() //1ms
{
  if(LPC_TMR32B0->IR & 0x01)
  {    
    LPC_TMR32B0->IR = 1; /* clear interrupt flag */
  }
}
#endif

//void scanKey(void)
//{
//	if((GPIOGetValue(SWITCH_INPUT)==0) &&
//		 (switch_lock == SWITCH_UNLOCK))
//	{
//		DELAY(5);
//		if(GPIOGetValue(SWITCH_INPUT)==0)
//		{
//			key_status = KEY_INPUT_PUSH;
//			switch_lock = SWITCH_LOCK;
//		}
//		else
//		{
//			key_status = KEY_INPUT_RELEASE;
//			switch_lock = SWITCH_UNLOCK;
//		}
//	}
//}

#ifdef DEBUG_WATCHDOG
void WDTInit (void)
{
    LPC_SYSCON->WDTCLKSEL      = 0x01;                                  /* 选择WDT时钟源                */
    LPC_SYSCON->WDTCLKUEN      = 0x00;
    LPC_SYSCON->WDTCLKUEN      = 0x01;                                  /* 更新使能                     */
    LPC_SYSCON->WDTCLKDIV      = 0x01;                                  /* 分频为1                      */
    
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<15);                               /* 打开WDT模块                  */
    LPC_WDT->TC          = 0xFFFF;                                      /* 定时时间                     */
    LPC_WDT->MOD         = (0x01 << 0);                                 /* 使能WDT                      */
//                          |(0x01 << 1);                                 /* 使能WDT中断                  */
    LPC_WDT->FEED        = 0xAA;                                        /* 喂狗                         */
    LPC_WDT->FEED        = 0x55;
}

void WDTFeed(void)
{
    LPC_WDT->FEED        = 0xAA;                                        /* 喂狗                         */
    LPC_WDT->FEED        = 0x55;
}
#endif

#ifdef MODE_DELAY
void scanKey(void)
{
	if(GPIOGetValue(SWITCH_INPUT)==0)
	{
		DELAY(SWITCH_DELAY);
		if(GPIOGetValue(SWITCH_INPUT)==0)
		{
			GPIOSetValue(LOCKER01, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER01, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER02, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER02, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER03, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER03, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER04, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER04, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER05, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER05, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER06, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER06, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER07, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER07, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER08, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER08, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER09, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER09, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER10, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER10, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER11, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER11, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER12, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER12, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER13, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER13, 0);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER14, 1);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER14, 0);
			DELAY(LOCKER_OFF_TIME);
		}
	}
}
#endif

int main(void)
{
	SystemCoreClockUpdate();
	SystemSetup();
	
	#ifdef DEBUG_WATCHDOG
  WDTInit();
	#endif

//	//timeout,use timer work. 
//	init_timer32(0, TIME_INTERVAL(1000));	//	1ms
//	enable_timer32(0);
	
  while(1)
  {
		#ifdef MODE_DELAY
		scanKey();
		#endif
  }
}
