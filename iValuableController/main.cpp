#include <cstdint>
#include <cstring>
#include <iostream>
#include <cmath>
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

#include "stm32f4xx_hal.h"
#include "System.h"
#include "CanEx.h"
#include "NetworkConfig.h"
#include "CommStructures.h"
#include "NetworkEngine.h"
#include "UnitManager.h"
#include "IndependentUnit.h"
#include "UnityUnit.h"
#include "RfidUnit.h"
#include "ISPProgram.h"

#include "FastDelegate.h"

using namespace fastdelegate;
using namespace IntelliStorage;
using namespace std;

boost::shared_ptr<CANExtended::CanEx> CanEx;
boost::shared_ptr<NetworkConfig> ethConfig;
boost::shared_ptr<NetworkEngine> ethEngine;
boost::shared_ptr<UnitManager> unitManager;
boost::shared_ptr<ISPProgram> ispUpdater;

osTimerId syncDataTimerId = NULL;

namespace boost
{
	void throw_exception(const std::exception& ex) {}
}

extern volatile float CurrentTemperature;

#define WARM_COEFF		0.2f
#define WARM_LIMIT		50

void HeatbeatTimerCallback(void const *arg)
{
	static uint8_t hbcount = 20;
	static int warmCount = 0; 
	
	HAL_GPIO_TogglePin(STATUS_PIN);
	
	//Send a heart beat per 10s
	if (++hbcount>20)	// 20*500ms = 10s/hb
	{
		RequestTemperature();
#ifdef DEBUG_PRINT
		cout<<"Current Temperature: "<<CurrentTemperature<<endl;
#endif
		if (warmCount<WARM_LIMIT)
			warmCount++;
		if (CanEx != nullptr && !UnitManager::IsUpdating())
		{
			float t=CurrentTemperature - warmCount*WARM_COEFF;
			StorageUnit::SetTemperature(*CanEx, t);
#ifdef DEBUG_PRINT
		cout<<"Calibrated Temperature: "<<t<<endl;
#endif		
		}
		hbcount = 0;
		if (ethEngine!= nullptr) 
			ethEngine->SendHeartBeat();
	}
}

osTimerDef(HeatbeatTimer, HeatbeatTimerCallback);

void SyncDataTimerCallback(void const *arg)
{
	if (unitManager != nullptr)
		unitManager->SyncAllData();
}

osTimerDef(SyncDataTimer, SyncDataTimerCallback);

void SyncDataLauncher(void const *argument)
{
	osDelay(3000);
	syncDataTimerId = osTimerCreate(osTimer(SyncDataTimer), osTimerPeriodic, NULL);
	osTimerStart(syncDataTimerId, 500);
}
osThreadDef(SyncDataLauncher, osPriorityNormal, 1, 0);

static void CanPollWorker(void const *argument)  
{
	if (CanEx == nullptr)
		return;
	while(1)
	{
		CanEx->Poll();
		//osDelay(5);
		osThreadYield();
	}
}
osThreadDef(CanPollWorker, osPriorityNormal, 1, 0);

static void Traversal(void const *argument)  //Prevent missing status
{
	static bool forceReport = true;
	while(1)
	{
		if (ethEngine!=nullptr && !ethEngine->IsConnected())
		{
			forceReport = true;
			continue;
		}
		unitManager->Traversal(forceReport);	//Update all units
		forceReport = false;
		//osDelay(5);
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
	bool updated = (unit!=nullptr) && (unit->UpdateStatus()==StorageUnit::Updated);
	
	if (unit == nullptr || updated)
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
		if (ethEngine!=nullptr)
		{
			unit->ReadCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceReadResponse);
			unit->WriteCommandResponse.bind(ethEngine.get(), &NetworkEngine::DeviceWriteResponse);
		}
		CanEx->RegisterDevice(unit);
		if (updated)
		{	
			unitManager->Recover(sourceId&0x7f, unit);
#ifdef DEBUG_PRINT
			cout<<"#Recovered Device 0x"<<std::hex<<sourceId<<std::dec<<endl;
#endif
		}
		else
		{
			unitManager->Add(sourceId&0x7f, unit);
#ifdef DEBUG_PRINT
			cout<<"#Added Device 0x"<<std::hex<<sourceId<<std::dec<<endl;
#endif
		}
	}
	//CanEx->Sync(sourceId, DeviceSync::SyncLive, CANExtended::AutoSync); //Confirm & Start AutoSync
	CanEx->Sync(sourceId, DeviceSync::SyncLive, CANExtended::Trigger);
	if (updated)
		unitManager->SyncUpdate();
}

int main()
{
	HAL_Init();
#ifdef DEBUG_PRINT	
	cout<<"System Started..."<<endl;
#endif	
	SerializableObjects::CommStructures::Register();
	
	ispUpdater.reset(new ISPProgram(Driver_USART1));
	ethConfig.reset(new NetworkConfig(Driver_USART3));
	unitManager.reset(new UnitManager(Driver_USART3, ispUpdater));
	
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
	
	osThreadCreate(osThread(CanPollWorker), NULL);
	osThreadCreate(osThread(Traversal), NULL);
	
	//Start collecting all devices
	CanEx->SyncAll(DeviceSync::SyncLive, CANExtended::Trigger);
	
	osThreadCreate(osThread(SyncDataLauncher), NULL);
	
	while (1)
	{
		net_main();
		ethEngine->Process();
		osThreadYield();
	}
}
