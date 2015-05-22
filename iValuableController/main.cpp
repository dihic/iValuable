#include <cstdint>
#include <cstring>
#include <iostream>
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

#include "System.h"
#include "CanEx.h"
#include "NetworkConfig.h"
#include "CommStructures.h"
#include "NetworkEngine.h"
#include "UnitManager.h"

#include "FastDelegate.h"

using namespace fastdelegate;
using namespace IntelliStorage;
using namespace std;

boost::scoped_ptr<CANExtended::CanEx> CanEx;
boost::scoped_ptr<NetworkConfig> ethConfig;
boost::scoped_ptr<NetworkEngine> ethEngine;
boost::scoped_ptr<UnitManager> unitManager;

namespace boost
{
	void throw_exception(const std::exception& ex) {}
}

void HeatbeatTimer_Callback(void const *arg)
{
	static uint8_t hbcount = 20;
	
	HAL_GPIO_TogglePin(STATUS_PIN);
	
	//Send a heart beat per 10s
	if (++hbcount>20)	// 20*500ms = 10s/hb
	{
		RequestTemperature();
		cout<<"Current Temperature: "<<CurrentTemperature<<endl;
		hbcount = 0;
		if (ethEngine.get()!=NULL) 
			ethEngine->SendHeartBeat();
	}
}

osTimerDef(HeatbeatTimer, HeatbeatTimer_Callback);

static void UpdateWorker (void const *argument)  
{
	while(1)
	{
		if (ConfigComm::Instance().get()!=NULL)
			ConfigComm::Instance()->DataReceiver();
		CanEx->Poll();
		osThreadYield();
	}
}
osThreadDef(UpdateWorker, osPriorityNormal, 1, 0);

//static void UpdateUnits(void const *argument)  //Prevent missing status
//{
//	while(1)
//	{
////		CanEx->Poll();
////		unitManager.Traversal();	//Update all units
//		osThreadYield();
//	}
//}
//osThreadDef(UpdateUnits, osPriorityNormal, 1, 0);

void HeartbeatArrival(uint16_t sourceId, CANExtended::DeviceState state)
{
}


int main()
{
	HAL_MspInit();
	cout<<"System Started..."<<endl;
	
	CommStructures::Register();
	unitManager.reset(new UnitManager);
	
	ethConfig.reset(new NetworkConfig(Driver_USART3));
	
	//Ethernet Init
	net_initialize();
//	osDelay(100);
//	Driver_ETH_PHY0.SetMode(ARM_ETH_PHY_AUTO_NEGOTIATE);
	
	ethEngine.reset(new NetworkEngine(ethConfig->GetIpConfig(IpConfigGetServiceEnpoint), unitManager));
	ethConfig->ServiceEndpointChangedEvent.bind(ethEngine.get(),&NetworkEngine::ChangeServiceEndpoint);
	
#ifdef DEBUG_PRINT
	cout<<"Ethernet Initialized"<<endl;
#endif
	
	//Initialize CAN
	CanEx.reset(new CANExtended::CanEx(Driver_CAN1, 0x001));
	CanEx->HeartbeatArrivalEvent.bind(&HeartbeatArrival);
#ifdef DEBUG_PRINT
	cout<<"CANBus Initialized"<<endl;
#endif
	
	osTimerId heartbeat = osTimerCreate(osTimer(HeatbeatTimer), osTimerPeriodic, NULL);
	osTimerStart(heartbeat, 500);
	
	osThreadCreate(osThread(UpdateWorker), NULL);
//	osThreadCreate(osThread(UpdateUnits), NULL);
	
	while (1)
	{
		net_main();
		ethEngine->Process();
//		osThreadYield();
	}
}
