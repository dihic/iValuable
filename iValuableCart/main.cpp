#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

#include "System.h"
#include "CanEx.h"
//#include "NetworkConfig.h"
#include "UsbKeyboard.h"
//#include "UnitManager.h"
#include "FastDelegate.h"
//#include "NetworkEngine.h"

using namespace fastdelegate;
//using namespace IntelliStorage;
using namespace std;

namespace boost
{
	void throw_exception(const std::exception& ex) {}
}

//boost::shared_ptr<CANExtended::CanEx> CanEx;
//boost::shared_ptr<NetworkConfig> ethConfig;
//boost::shared_ptr<NetworkEngine> ethEngine;
//boost::shared_ptr<ConfigComm> configComm;

//UnitManager unitManager;

volatile uint8_t CommandState = 0;

void SystemHeartbeat(void const *argument)
{
	static uint8_t hbcount = 20;
	
	HAL_GPIO_TogglePin(STATUS_PIN);
	
	//Send a heart beat per 10s
//	if (ethEngine!=nullptr && ++hbcount>20)
//	{
//		hbcount = 0;
// 		ethEngine->SendHeartBeat();
//	}
}
osTimerDef(TimerHB, SystemHeartbeat);

bool ReadKBLine(string command)
{
	return true;
}

void HeartbeatArrival(uint16_t sourceId, const std::uint8_t *data, std::uint8_t len)
{
//	static int dc =0;
//	CANExtended::DeviceState state = static_cast<CANExtended::DeviceState>(data[0]);
//	if (state != CANExtended::Operational)
//		return;
//	CanEx->Sync(sourceId, SYNC_LIVE, CANExtended::Trigger); //Confirm & Stop
//	if (sourceId & 0x100)
//	{
//		auto unit = unitManager.FindUnit(sourceId);
//		if (unit==nullptr)
//		{
//#ifdef DEBUG_PRINT
//			cout<<"#"<<++dc<<" DeviceID: "<<(sourceId & 0xff)<<" Added"<<endl;
//#endif
//			unit.reset(new StorageUnit(CanEx, sourceId));
//			CanEx->RegisterDevice(unit);
//			unit->ReadCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceReadResponse);
//			unit->WriteCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceWriteResponse);
//			unitManager.Add(sourceId, unit);
//			unit->NoticeFromMemory();
//		}
//	}
}

//static void UpdateWorker (void const *argument)  
//{
//	while(1)
//	{
//		CanEx->Poll();
//		ethEngine->Process();
//		osThreadYield();
//	}
//}
//osThreadDef(UpdateWorker, osPriorityNormal, 1, 0);

//static void UpdateUnits(void const *argument)  //Prevent missing status
//{
//	while(1)
//	{
//		unitManager.Traversal();	//Update all units
//		osThreadYield();
//	}
//}
//osThreadDef(UpdateUnits, osPriorityNormal, 1, 0);

int main()
{
	HAL_Init();		/* Initialize the HAL Library    */
	osDelay(100);	//Wait for voltage stable
	
	//CommStructures::Register();
	
	cout<<"Started..."<<endl;
	
//	Keyboard::Init();
//	Keyboard::OnLineReadEvent.bind(&ReadKBLine);
//	
//	configComm = ConfigComm::CreateInstance(Driver_USART2);
//	configComm->Start();

//	
//	ethConfig.reset(new NetworkConfig(configComm.get()));
//	
//	//Initialize CAN
//	CanEx.reset(new CANExtended::CanEx(Driver_CAN1, 0x001));
//	CanEx->HeartbeatArrivalEvent.bind(&HeartbeatArrival);
//#ifdef DEBUG_PRINT
//	cout<<"CAN Inited"<<endl;
//#endif
	
	//Initialize Ethernet interface
	net_initialize();
	
//	ethEngine.reset(new NetworkEngine(ethConfig->GetIpConfig(IpConfigGetServiceEnpoint), unitManager.GetList()));
//	ethEngine->StrCommandDelegate.bind(&ReadKBLine);
//	ethConfig->ServiceEndpointChangedEvent.bind(ethEngine.get(),&NetworkEngine::ChangeServiceEndpoint);
//	unitManager.OnSendData.bind(ethEngine.get(),&NetworkEngine::SendRfidData);

	//Start to find unit devices
	//CanEx->SyncAll(SYNC_LIVE, CANExtended::Trigger);
	
	//Initialize system heatbeat
	osTimerId id = osTimerCreate(osTimer(TimerHB), osTimerPeriodic, NULL);
  osTimerStart(id, 500); 
	
//	osThreadCreate(osThread(UpdateWorker), NULL);
//	osThreadCreate(osThread(UpdateUnits), NULL);
	
	//Search all units
//	for(int8_t i=1;i<0x7f;++i)
//	{
//		CanEx->Sync(i|0x100, SYNC_LIVE, CANExtended::Trigger);
//		osDelay(100);
//	}
	
  while(1) 
	{
//		net_main();
		osThreadYield();
  }
}
