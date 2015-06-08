#ifndef _SYSTEM_H
#define _SYSTEM_H

//#define CAN_EN_PIN PORT1,8
#define CAN_EN_PIN PORT1,11

#define CanSTBEnable() GPIOSetValue(CAN_EN_PIN, 0)
#define CanSTBDisable() GPIOSetValue(CAN_EN_PIN, 1)

#define ISP_STATUS_LED_PIN PORT1,10

#if 0
#define ENABLE_LOCKER  PORT1,8   //ADDR7

#define IS_DOOR_OPEN 		(GPIOGetValue(FB_LOCKER)!=0)
#define IS_DOOR_CLOSED 	(GPIOGetValue(FB_LOCKER)==0)

#define LED_ON						GPIOSetValue(CON_LED, 1)
#define LED_OFF 					GPIOSetValue(CON_LED, 0)
#define LED_BLINK 				GPIOSetValue(CON_LED, GPIOGetValue(CON_LED)==0)
#define LOCKER_ON 				GPIOSetValue(CON_LOCKER, 0)
#define LOCKER_OFF 				GPIOSetValue(CON_LOCKER, 1)
#define IS_LOCKER_ON 			(GPIOGetValue(CON_LOCKER)==0)
#define IS_LOCKER_ENABLE	(GPIOGetValue(ENABLE_LOCKER)!=0)
#endif

#define ABS(x) ((x)>=0?(x):-(x))


#endif

