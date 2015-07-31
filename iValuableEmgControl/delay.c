#include "delay.h"

__asm void SysCtlDelay(unsigned long ulCount)
{
    subs    r0, #1;
		nop;
	  nop;
    bne     SysCtlDelay;
    bx      lr;
}
