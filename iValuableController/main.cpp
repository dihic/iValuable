#include <cstdint>
#include <cstring>
#include <iostream>
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

#include "System.h"
#include "CanEx.h"
#include "NetworkConfig.h"
#include "CommStructures.h"

#include "FastDelegate.h"

using namespace fastdelegate;
using namespace IntelliStorage;
using namespace std;

boost::shared_ptr<CANExtended::CanEx> CanEx;
boost::shared_ptr<NetworkConfig> ethConfig;
//boost::shared_ptr<NetworkEngine> ethEngine;

int *pTest = 0;
uint8_t v = 0;

namespace boost
{
	void throw_exception(const std::exception& ex) {}
}

void HeatbeatTimer_Callback(void const *arg)
{
	HAL_GPIO_TogglePin(STATUS_PIN);
//	memset(pTest, ++v, 0x10000);
//	if ((pTest[0]&0xff)!=v)
//		cout<<"Error!"<<endl;
}

osTimerDef(HeatbeatTimer, HeatbeatTimer_Callback);

static void UpdateWorker (void const *argument)  
{
	while(1)
	{
		if (ConfigComm::Instance().get()!=NULL)
			ConfigComm::Instance()->DataReceiver();
		osThreadYield();
	}
}
osThreadDef(UpdateWorker, osPriorityNormal, 1, 0);

static void UpdateUnits(void const *argument)  //Prevent missing status
{
	while(1)
	{
		CanEx->Poll();
//		unitManager.Traversal();	//Update all units
		osThreadYield();
	}
}
osThreadDef(UpdateUnits, osPriorityNormal, 1, 0);

void HeartbeatArrival(uint16_t sourceId, CANExtended::DeviceState state)
{
}

int main()
{
	HAL_MspInit();
	cout<<"Started..."<<endl;
	
	CommStructures::Register();
	
	ethConfig.reset(new NetworkConfig(Driver_USART3));
	
	//Ethernet Init
	net_initialize();
	osDelay(100);
	Driver_ETH_PHY0.SetMode(ARM_ETH_PHY_AUTO_NEGOTIATE);
#ifdef DEBUG_PRINT
	cout<<"Ethernet Inited"<<endl;
#endif
	
	//Initialize CAN
	CanEx.reset(new CANExtended::CanEx(Driver_CAN1, 0x001));
	CanEx->HeartbeatArrivalEvent.bind(&HeartbeatArrival);
#ifdef DEBUG_PRINT
	cout<<"CAN Inited"<<endl;
#endif
	
//	pTest = new int[0x10000];
	
	osTimerId heartbeat = osTimerCreate(osTimer(HeatbeatTimer), osTimerPeriodic, NULL);
	osTimerStart(heartbeat, 500);
	
	osThreadCreate(osThread(UpdateWorker), NULL);
	osThreadCreate(osThread(UpdateUnits), NULL);
	
	while (1)
	{
		net_main();
		osThreadYield();
	}
}
