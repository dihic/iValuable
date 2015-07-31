#include "LPC11xx.h"
#include "timer32.h"
#include "gpio.h"
#include "math.h"
#include <string.h>
#include "System.h"
#include "delay.h"

#define POWERON_DELAY 50*1000
#define SWITCH_DELAY 5*1000

#define LOCKER_ON_TIME 200*1000
#define LOCKER_OFF_TIME 200*1000

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

void scanKey(void)
{
	if(GPIOGetValue(SWITCH_INPUT)==0)
	{
		DELAY(SWITCH_DELAY);
		if(GPIOGetValue(SWITCH_INPUT)==0)
		{
			GPIOSetValue(LOCKER01, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER01, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER02, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER02, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER03, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER03, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER04, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER04, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER05, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER05, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER06, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER06, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER07, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER07, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER08, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER08, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER09, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER09, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER10, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER10, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER11, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER11, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER12, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER12, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER13, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER13, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
			
			GPIOSetValue(LOCKER14, LOCK_ON);
			DELAY(LOCKER_ON_TIME);
			GPIOSetValue(LOCKER14, LOCK_OFF);
			DELAY(LOCKER_OFF_TIME);
		}
	}
}

int main(void)
{
	SystemSetup();
	SystemCoreClockUpdate();
	DELAY(POWERON_DELAY);
	
	#ifdef DEBUG_WATCHDOG
  WDTInit();
	#endif
  while(1)
  {
		scanKey();
  }
}
