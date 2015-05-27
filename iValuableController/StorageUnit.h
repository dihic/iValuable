#ifndef _STORAGE_UNIT_H
#define _STORAGE_UNIT_H

#include <string>
#include "CanDevice.h"
#include "CommStructures.h"

namespace IntelliStorage
{			
	enum DeviceAttribute : std::uint16_t
	{		
			Zero 								= 0x00fe,   //W
			Ramp		 						= 0x8000,   //RW
			InventoryInfo				= 0x8001,   //RW
			CalWeight						= 0x8002,   //W
			SensorConfig				= 0x8003,		//W
			RawData							= 0x8006,   //R
			AutoRamp						= 0x8007,   //W
			SensorEnable				= 0x8008,   //RW
			Temperature					= 0x8009,   //W
			InventoryQuantity		= 0x800A,   //RW
			QueryInventory			= 0x800B,		//W
			Version 						= 0x80ff,		//R
			Notice 							= 0x9000,   //W
			Locker							= 0x9002,   //W
	};
	
	enum DeviceSync : std::uint16_t
	{		
		Data 			= 0x0100,
		Gotcha		= 0x0180,
		ISP 			= 0x01f0,
		Live			= 0x01ff,
	};
	
	class StorageUnit : public CanDevice
	{
		protected:
			const std::uint8_t SensorNum;
			const bool IsDoorController : 1;
		public:		
			const std::uint8_t GroupId : 4;
			const std::uint8_t NodeId  : 3;
			
			StorageUnit(CANExtended::CanEx &ex, std::uint16_t id, std::uint8_t sensorNum);
			virtual ~StorageUnit() {}
				
			
			void SetRamp(std::uint8_t index, float val);
			void SetZero(std::uint8_t flags, bool tare);
			void SetAutoRamp(std::uint8_t flags);
			void SetSensorEnable(std::uint8_t flags);
			void SetCalWeight(float weight);
			void SetTemperature(float t);
			void SetNotice(std::uint8_t level);
			void LockControl(bool open);
			void SetSensorConfig(boost::shared_ptr<SerializableObjects::ScaleAttribute> &attr);
			void SetInventoryInfo(boost::shared_ptr<SerializableObjects::SuppliesItem> &info);
			void SetInventoryQuantity(std::uint8_t index, std::uint16_t q);
			void QueryInventoryById(std::uint64_t id, bool notice);
			
			void RequestVersion()
			{
				ReadAttribute(DeviceAttribute::Version);
			}
				
			void RequestRamps()
			{
				ReadAttribute(DeviceAttribute::Ramp);
			}
			
			void RequestInventoryQuantities()
			{
				ReadAttribute(DeviceAttribute::InventoryQuantity);
			}
			
			void RequestSensorsEnable()
			{
				ReadAttribute(DeviceAttribute::SensorEnable);
			}
			
			void RequestRawData()
			{
				ReadAttribute(DeviceAttribute::RawData);
			}
			
			void RequestInventoryInfos()
			{
				ReadAttribute(DeviceAttribute::InventoryInfo);
			}
			
			void RequestData()
			{
				canex.Sync(DeviceId, DeviceSync::Data, CANExtended::Trigger);
			}
			
			void EnterCanISP()
			{
				canex.Sync(DeviceId, DeviceSync::ISP, CANExtended::Trigger);
			}
			
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> entry) override;
	};
}

#endif

