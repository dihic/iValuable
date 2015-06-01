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
#include "IndependentUnit.h"
#include "UnityUnit.h"
#include "RfidUnit.h"

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

extern volatile float CurrentTemperature;

void HeatbeatTimer_Callback(void const *arg)
{
	static uint8_t hbcount = 20;
	
	HAL_GPIO_TogglePin(STATUS_PIN);
	
	//Send a heart beat per 10s
	if (++hbcount>20)	// 20*500ms = 10s/hb
	{
		RequestTemperature();
#ifdef DEBUG_PRINT
		cout<<"Current Temperature: "<<CurrentTemperature<<endl;
#endif
		if (CanEx != nullptr)
			StorageUnit::SetTemperature(*CanEx, CurrentTemperature);
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
		if (ConfigComm::Instance() != nullptr)
			ConfigComm::Instance()->DataReceiver();
		if (CanEx != nullptr)
			CanEx->Poll();
		osThreadYield();
	}
}
osThreadDef(UpdateWorker, osPriorityNormal, 1, 0);

static void Traversal(void const *argument)  //Prevent missing status
{
	static bool forceReport = true;
	while(1)
	{
		if (ethEngine==nullptr || !ethEngine->IsConnected())
		{
			forceReport = true;
			continue;
		}
		unitManager->Traversal(forceReport);	//Update all units
		forceReport = false;
		osThreadYield();
	}
}
osThreadDef(Traversal, osPriorityNormal, 1, 0);

void HeartbeatArrival(uint16_t sourceId, const std::uint8_t *data, std::uint8_t len)
{
	if (len<5)
		return;
	CANExtended::DeviceState state = static_cast<CANExtended::DeviceState>(data[0]);
	if (state != CANExtended::Operational)
		return;
	auto unit = unitManager->FindUnit(sourceId&0x7f);
	if (unit == nullptr)
	{
		StorageBasic basic(CanEx);
		basic.DeviceId = sourceId;
		basic.SensorNum = data[2];
		basic.Version = (data[3]<<8)|data[4];
		switch (data[1])
		{
			case UNIT_TYPE_INDEPENDENT:
				unit.reset(new IndependentUnit(basic));
				break;
			case UNIT_TYPE_UNITY:
				unit.reset(new UnityUnit(basic));
				break;
			case UNIT_TYPE_UNITY_RFID:
				unit.reset(new RfidUnit(basic));
				break;
			default:
				CanEx->Sync(sourceId, DeviceSync::SyncLive,  CANExtended::Trigger);
				return;
		}
		unit->ReadCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceReadResponse);
		unit->WriteCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceWriteResponse);
		unitManager->Add(sourceId&0x7f, unit);
	}
	CanEx->Sync(sourceId, DeviceSync::SyncLive, CANExtended::AutoSync); //Confirm & Start AutoSync
}


int main()
{
	HAL_MspInit();
	cout<<"System Started..."<<endl;
	
	SerializableObjects::CommStructures::Register();
	
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
	CanEx.reset(new CANExtended::CanEx(Driver_CAN1, CANEX_HOST));
	CanEx->HeartbeatArrivalEvent.bind(&HeartbeatArrival);
#ifdef DEBUG_PRINT
	cout<<"CANBus Initialized"<<endl;
#endif
	
	osTimerId heartbeat = osTimerCreate(osTimer(HeatbeatTimer), osTimerPeriodic, NULL);
	osTimerStart(heartbeat, 500);
	
	osThreadCreate(osThread(UpdateWorker), NULL);
	osThreadCreate(osThread(Traversal), NULL);
	
	//Start collecting all devices
	CanEx->SyncAll(DeviceSync::SyncLive, CANExtended::Trigger);
	
	while (1)
	{
		net_main();
		ethEngine->Process();
//		osThreadYield();
	}
}
