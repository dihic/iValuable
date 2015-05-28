#include "StorageUnit.h"
#include "System.h"
#include "stm32f4xx_hal_conf.h" 
#include <cstring>

using namespace std;

#define IS_LC(x)							(((x)&0x80)!=0)
#define OBTAIN_GROUP_ID(x) 		(((x)&0x78)>>3)
#define OBTAIN_NODE_ID(x)			((x)&0x07)


namespace IntelliStorage
{

	StorageUnit::StorageUnit(CANExtended::CanEx &ex, uint16_t id, uint8_t sensorNum)
		:	CanDevice(ex, id), SensorNum(sensorNum), IsLockController(IS_LC(id)), 
			GroupId(OBTAIN_GROUP_ID(id)), NodeId(OBTAIN_NODE_ID(id))
	{
	}
	
	void StorageUnit::SetZero(std::uint8_t flags, bool tare)
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(2);
		data[0] = flags;
		data[1] = tare;
    WriteAttribute(DeviceAttribute::Zero, data, 2);
	}
	
	void StorageUnit::SetRamp(std::uint8_t index, float val)
	{
		static constexpr uint8_t size = sizeof(float)+1;
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(size);
		data[0] = index;
		memcpy(data.get()+1, &val, sizeof(float));
    WriteAttribute(DeviceAttribute::Ramp, data, size);
	}
	
	void StorageUnit::SetAutoRamp(std::uint8_t flags)
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(1);
		data[0] = flags;
    WriteAttribute(DeviceAttribute::AutoRamp, data, 1);
	}
	
	void StorageUnit::SetSensorEnable(std::uint8_t flags)
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(1);
		data[0] = flags;
    WriteAttribute(DeviceAttribute::SensorEnable, data, 1);
	}
	
	void StorageUnit::SetCalWeight(float weight)
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(sizeof(float));
		memcpy(data.get(), &weight, sizeof(float));
    WriteAttribute(DeviceAttribute::CalWeight, data, sizeof(float));
	}
	
	void StorageUnit::SetTemperature(float t)
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(sizeof(float));
		memcpy(data.get(), &t, sizeof(float));
    WriteAttribute(DeviceAttribute::Temperature, data, sizeof(float));
	}
	
	void StorageUnit::SetSensorConfig(boost::shared_ptr<SerializableObjects::ScaleAttribute> &attr)
	{
		static constexpr uint8_t size = sizeof(float)*5;
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(size);
		uint8_t *ptr = data.get();
		memcpy(ptr, &attr->Sensitivity, sizeof(float));
		ptr+=4;
		memcpy(ptr, &attr->TempDrift, sizeof(float));
		ptr+=4;
		memcpy(ptr, &attr->ZeroRange, sizeof(float));
		ptr+=4;
		memcpy(ptr, &attr->SafeOverload, sizeof(float));
		ptr+=4;
		memcpy(ptr, &attr->MaxOverload, sizeof(float));
    WriteAttribute(DeviceAttribute::SensorConfig, data, size);
	}
	
	void StorageUnit::SetNotice(std::uint8_t level)
	{
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(1);
		data[0]=level;
		WriteAttribute(DeviceAttribute::Notice, data, 1);
	}
	
	void StorageUnit::LockControl(bool open)
	{
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(1);
		data[0]=open;
		WriteAttribute(DeviceAttribute::Locker, data, 1);
	}
	
	void StorageUnit::SetInventoryInfo(boost::shared_ptr<SerializableObjects::SuppliesItem> &info)
	{
		static constexpr size_t fixSize = sizeof(uint64_t)+sizeof(float)*2+1;
		size_t size = info->MaterialName.size() + fixSize;
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(size);
		data[0] = info->Index & 0xff;
		uint8_t *ptr = data.get()+1;
		memcpy(ptr, &info->MaterialId, sizeof(uint64_t));
		ptr += sizeof(uint64_t);
		memcpy(ptr, &info->UnitWeight, sizeof(float));
		ptr += sizeof(float);
		memcpy(ptr, &info->ErrorRange, sizeof(float));
		ptr += sizeof(float);
		if (info->MaterialName.size() > 0)
			std::copy(info->MaterialName.begin(), info->MaterialName.end(), ptr);
		WriteAttribute(DeviceAttribute::InventoryInfo, data, size);
	}
	
	void StorageUnit::SetInventoryQuantity(std::uint8_t index, std::uint16_t q)
	{
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(3);
		data[0] = index;
		data[1] = q&0xff;
		data[2] = q>>8;
		WriteAttribute(DeviceAttribute::Notice, data, 3);
	}
	
	void StorageUnit::QueryInventoryById(std::uint64_t id, bool notice)
	{
		static constexpr size_t size = sizeof(uint64_t)+1;
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(size);
		uint8_t *ptr = data.get();
		memcpy(ptr, &id, sizeof(uint64_t));
		data[sizeof(uint64_t)] = notice;
		WriteAttribute(DeviceAttribute::QueryInventory, data, size);
	}
	
	void StorageUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry)
	{
		const DeviceSync syncIndex = static_cast<DeviceSync>(entry->Index);
		auto val = entry->GetVal();
		auto len = entry->GetLen();
		switch (syncIndex)
		{
			case DeviceSync::SyncData:
				sensorFlags = val[0];
				allStable = val[2]!=0;
				inventoryExpected = val[3]!=0;
				memcpy(reinterpret_cast<void *>(&deltaWeight), val.get()+4, sizeof(float));
				break;
			default:
				break;
		}
	}
}

