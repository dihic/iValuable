#include <cstdint>
#include <cstring>
#include <iostream>
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

#include "System.h"

using namespace std;

int *pTest = 0;
uint8_t v = 0;

void HeatbeatTimer_Callback(void const *arg)
{
	HAL_GPIO_TogglePin(STATUS_PIN);
	memset(pTest, ++v, 0x10000);
	if ((pTest[0]&0xff)!=v)
		cout<<"Error!"<<endl;
}

osTimerDef(HeatbeatTimer, HeatbeatTimer_Callback);

int main()
{
	HAL_MspInit();
	
	cout<<"System Inited."<<endl;
	
	pTest = (int *)(0xC0100000);
	
	pTest[0] = 0x12345678;
	pTest[1] = 0x23456789;
	pTest[2] = pTest[0]+pTest[1];
	
	pTest = new int[0x10000];
	
	osTimerId heartbeat = osTimerCreate(osTimer(HeatbeatTimer), osTimerPeriodic, NULL);
	osTimerStart(heartbeat, 10);
	
	while (1)
	{
		net_main();
	}
}
