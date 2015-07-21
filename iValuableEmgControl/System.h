#ifndef _SYSTEM_H
#define _SYSTEM_H

#define LOCKER01    PORT2,0
#define LOCKER02    PORT2,1
#define LOCKER03    PORT2,2
#define LOCKER04    PORT2,3
#define LOCKER05    PORT2,4
#define LOCKER06    PORT2,5
#define LOCKER07    PORT2,6
#define LOCKER08    PORT2,7
#define LOCKER09    PORT2,8
#define LOCKER10    PORT2,9
#define LOCKER11    PORT2,10
#define LOCKER12    PORT2,11
#define LOCKER13    PORT3,0
#define LOCKER14    PORT3,1

#define SWITCH_INPUT PORT3,4

#define IS_DOOR_OPEN 		(GPIOGetValue(FB_LOCKER)==0)
#define IS_DOOR_CLOSED 	(GPIOGetValue(FB_LOCKER)!=0)

void SystemSetup(void);

#endif

