
#include <LPC11xx.h>
#include "gpio.h"
#include "System.h"

void SystemSetup(void)
{
	GPIOInit();
	
//	LPC_GPIO2->DATA &= 0x000;
//	LPC_GPIO2->DIR |= 0xfff;

//	LPC_GPIO3->DATA &= 0xffc;
//	LPC_GPIO3->DIR |= 0x003;

	GPIOSetDir(LOCKER01, E_IO_OUTPUT);
	GPIOSetDir(LOCKER02, E_IO_OUTPUT);
	GPIOSetDir(LOCKER03, E_IO_OUTPUT);
	GPIOSetDir(LOCKER04, E_IO_OUTPUT);
	GPIOSetDir(LOCKER05, E_IO_OUTPUT);
	GPIOSetDir(LOCKER06, E_IO_OUTPUT);
	GPIOSetDir(LOCKER07, E_IO_OUTPUT);
	GPIOSetDir(LOCKER08, E_IO_OUTPUT);
	GPIOSetDir(LOCKER09, E_IO_OUTPUT);
	GPIOSetDir(LOCKER10, E_IO_OUTPUT);
	GPIOSetDir(LOCKER11, E_IO_OUTPUT);
	GPIOSetDir(LOCKER12, E_IO_OUTPUT);
	GPIOSetDir(LOCKER13, E_IO_OUTPUT);
	GPIOSetDir(LOCKER14, E_IO_OUTPUT);
	
	GPIOSetValue(LOCKER01, LOCK_OFF);
	GPIOSetValue(LOCKER02, LOCK_OFF);
	GPIOSetValue(LOCKER03, LOCK_OFF);
	GPIOSetValue(LOCKER04, LOCK_OFF);
	GPIOSetValue(LOCKER05, LOCK_OFF);
	GPIOSetValue(LOCKER06, LOCK_OFF);
	GPIOSetValue(LOCKER07, LOCK_OFF);
	GPIOSetValue(LOCKER08, LOCK_OFF);
	GPIOSetValue(LOCKER09, LOCK_OFF);
	GPIOSetValue(LOCKER10, LOCK_OFF);
	GPIOSetValue(LOCKER11, LOCK_OFF);
	GPIOSetValue(LOCKER12, LOCK_OFF);
	GPIOSetValue(LOCKER13, LOCK_OFF);
	GPIOSetValue(LOCKER14, LOCK_OFF);
	
	GPIOSetDir(SWITCH_INPUT, E_IO_INPUT);
}
