
#include <LPC11xx.h>
#include "gpio.h"
#include "System.h"

void SystemSetup(void)
{
	GPIOInit();
	
	LPC_GPIO2->DATA &= 0x000;
	LPC_GPIO2->DIR |= 0xfff;
	
	LPC_GPIO3->DATA &= 0xffc;
	LPC_GPIO3->DIR |= 0x003;

//	GPIOSetDir(LOCKER01, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER02, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER03, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER04, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER05, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER06, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER07, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER08, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER09, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER10, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER11, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER12, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER13, E_IO_OUTPUT);
//	GPIOSetDir(LOCKER14, E_IO_OUTPUT);
	
//	GPIOSetValue(LOCKER01, 0);
//	GPIOSetValue(LOCKER02, 0);
//	GPIOSetValue(LOCKER03, 0);
//	GPIOSetValue(LOCKER04, 0);
//	GPIOSetValue(LOCKER05, 0);
//	GPIOSetValue(LOCKER06, 0);
//	GPIOSetValue(LOCKER07, 0);
//	GPIOSetValue(LOCKER08, 0);
//	GPIOSetValue(LOCKER09, 0);
//	GPIOSetValue(LOCKER10, 0);
//	GPIOSetValue(LOCKER11, 0);
//	GPIOSetValue(LOCKER12, 0);
//	GPIOSetValue(LOCKER13, 0);
//	GPIOSetValue(LOCKER14, 0);
	
	GPIOSetDir(SWITCH_INPUT, E_IO_INPUT);
}