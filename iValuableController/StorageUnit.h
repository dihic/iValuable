#ifndef _STORAGE_UNIT_H
#define _STORAGE_UNIT_H

#include "boost/tuple/tuple.hpp"
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
			NoticeInventory			= 0x800B,		//W
			Notice 							= 0x9000,   //W
			Locker							= 0x9002,   //W
	};
	
	enum DeviceSync : std::uint16_t
	{		
		SyncData 			= 0x0100,
		SyncRfid			= 0x0101,
		SyncDoor      = 0x0102,
		SyncISP 			= 0x01f0,
		SyncLive			= 0x01ff,
	};
	
	class StorageBasic
	{
		public:
			boost::weak_ptr<CANExtended::CanEx> CanEx;
			std::uint16_t DeviceId;
			std::uint8_t SensorNum;
			std::uint16_t Version;
			StorageBasic(boost::shared_ptr<CANExtended::CanEx> &ex)	:CanEx(ex) {}
			~StorageBasic() {}
	};
	
	class UnitManager;
	
	class StorageUnit : public CanDevice
	{
		public:
			friend class UnitManager;
			enum UpdateState
			{
				Idle,
				Updating,
				Updated,
			};
		protected:
			volatile bool isDoorOpen = false;
			volatile UpdateState updateStatus = UpdateState::Idle;
			StorageUnit(std::uint8_t typeCode, StorageBasic &basic);
		public:		
			const std::uint8_t TypeCode;
			const std::uint16_t Version;
		  #if ADDR_TYPE!=ADDR_SWTICH_10BITS
			const bool IsLockController : 1;
			const std::uint8_t GroupId 	: 4;
			const std::uint8_t NodeId  	: 3;
		  #else
		  const bool IsLockController : 1;
			const std::uint8_t GroupId 	: 4;
			const std::uint8_t NodeId  	: 5;
		  #endif
		
			typedef FastDelegate3<std::uint8_t, std::uint8_t, bool> DoorChangedHandler;
			DoorChangedHandler OnDoorChangedEvent;
		
			static void SetTemperature(CANExtended::CanEx &ex, float t);
			
			static std::uint16_t GetId(std::uint8_t groupId, std::uint8_t nodeId)
			{
				#if ADDR_TYPE!=ADDR_SWTICH_10BITS
				return ((groupId&0xf)<<3)|(nodeId&0x7);
				#else
				return ((groupId&0xf)<<5)|(nodeId&0x1f);
				#endif
			}
			
			virtual ~StorageUnit() {}		

			bool IsDoorOpen() const { return isDoorOpen; }			
			void LockControl(bool open);
			
			void EnterCanISP()
			{
				updateStatus = UpdateState::Updating;
				auto ex = canex.lock();
				if (ex != nullptr)
					ex->Sync(DeviceId, DeviceSync::SyncISP, CANExtended::Trigger);
			}
			
			UpdateState UpdateStatus() const { return updateStatus; }
			
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry) override;
	};
	
	class SensorUnit : public StorageUnit
	{
		protected:
			volatile std::uint8_t sensorFlags = 0xff;
			volatile bool allStable = false;
			volatile bool inventoryExpected = false;
			volatile float deltaWeight = 0;
			SensorUnit(std::uint8_t typeCode, StorageBasic &basic) 
				:StorageUnit(typeCode, basic),
				 SensorNum(basic.SensorNum)
			{}
		public:					
			const std::uint8_t SensorNum;
				
			virtual ~SensorUnit() {}
			
			bool IsEnable(std::uint8_t ch) const
			{
				return (ch<SensorNum) ? (sensorFlags & (1<<ch))!=0 : false;
			}				
		
			std::uint8_t GetSensorEnable() const { return sensorFlags; }
			bool GetAllStable() const { return allStable; }
			
			void SetRamp(std::uint8_t index, float val);
			void SetZero(std::uint8_t flags, bool tare);
			void SetAutoRamp(std::uint8_t flags);
			void SetSensorEnable(std::uint8_t flags);
			void SetCalWeight(float weight);
			void SetNotice(std::uint8_t level);
			void SetSensorConfig(boost::shared_ptr<SerializableObjects::ScaleAttribute> &attr);
			void ClearAllInventoryInfo();
			void SetInventoryInfo(SerializableObjects::SuppliesItem &info);
			void SetInventoryQuantities(Array<SerializableObjects::InventoryQuantity> &quantities);
			void NoticeInventoryById(std::uint64_t id, bool notice);
			
			void RequestSensorConfig()
			{
				ReadAttribute(DeviceAttribute::SensorConfig);
			}
				
			void RequestRamps()
			{
				ReadAttribute(DeviceAttribute::Ramp);
			}
			
			void RequestInventoryQuantities()
			{
				ReadAttribute(DeviceAttribute::InventoryQuantity);
			}
			
			void RequestSensorEnable()
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
				auto ex = canex.lock();
				if (ex != nullptr)
					ex->Sync(DeviceId, DeviceSync::SyncData, CANExtended::Trigger);
			}
			
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry) override;
	};
	
	template<class T>
	class WeightBase : public SensorUnit
	{
		protected:
			boost::shared_ptr<T[]> scaleList; 
			WeightBase<T>(std::uint8_t typeCode, StorageBasic &basic)
				:SensorUnit(typeCode, basic)
			{
				scaleList = boost::make_shared<T[]>(basic.SensorNum);
			}
		public:
			virtual ~WeightBase() {}
			boost::shared_ptr<T[]> &GetScaleList() { return scaleList; }
	};
}

#endif

