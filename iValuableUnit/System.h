#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <cstdint>
#include "UnitType.h"

#define PRECISION_DIGITS 17

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
	#define SENSOR1_SCK  PORT1,8
	#define SENSOR1_DOUT PORT2,6
	#define SENSOR2_SCK  PORT0,5
	#define SENSOR2_DOUT PORT2,1
	#define SENSOR3_SCK  PORT2,2
	#define SENSOR3_DOUT PORT2,10
	#define SENSOR4_SCK  PORT2,3
	#define SENSOR4_DOUT PORT2,11
	
	#define CON_LOCKER	PORT2,8
	#define CON_LED			PORT2,7
	#define FB_LOCKER		PORT0,11
	#define ENABLE_LOCKER  PORT3,2 //ADDR7
#else
//Unity and Indpt Unit I/O Configuration
	#define SENSOR1_SCK  PORT2,1
	#define SENSOR1_DOUT PORT2,0
	#define SENSOR2_SCK  PORT2,3
	#define SENSOR2_DOUT PORT2,2
	#define SENSOR3_SCK  PORT1,1
	#define SENSOR3_DOUT PORT1,0
	#define SENSOR4_SCK  PORT1,5
	#define SENSOR4_DOUT PORT1,4
	#define SENSOR5_SCK  PORT3,1
	#define SENSOR5_DOUT PORT3,0
	#define SENSOR6_SCK  PORT3,3
	#define SENSOR6_DOUT PORT3,2

	//#define LATCH_LE 		PORT0,2
	//#define LATCH_D 		PORT0,4
	//#define LATCH_Q1 		PORT0,1
	//#define LATCH_Q2 		PORT0,3

	#define CON_LOCKER	PORT0,5
	#define CON_LED			PORT0,11
	#define FB_LOCKER		PORT1,2

	#define ENABLE_LOCKER  PORT1,8   //ADDR7
#endif

#define IS_DOOR_OPEN 		(GPIOGetValue(FB_LOCKER)==0)
#define IS_DOOR_CLOSED 	(GPIOGetValue(FB_LOCKER)!=0)

#define LED_ON						GPIOSetValue(CON_LED, 1)
#define LED_OFF 					GPIOSetValue(CON_LED, 0)
#define LED_BLINK 				GPIOSetValue(CON_LED, GPIOGetValue(CON_LED)==0)

//x=1 Lock On and Door Open
//x=0 Lock Off
#define LOCKER_CONTROL(x) 	GPIOSetValue(CON_LOCKER, (x)==0)		
#define LOCKER_ON 				GPIOSetValue(CON_LOCKER, 0)
#define LOCKER_OFF 				GPIOSetValue(CON_LOCKER, 1)
#define IS_LOCKER_ON 			(GPIOGetValue(CON_LOCKER)==0)
#define IS_LOCKER_ENABLE	(GPIOGetValue(ENABLE_LOCKER)!=0)

void SystemSetup();
//void EnterISP();


#endif

