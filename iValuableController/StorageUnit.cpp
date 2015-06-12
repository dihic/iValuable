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

	StorageUnit::StorageUnit(std::uint8_t typeCode, StorageBasic &basic)
		:	CanDevice(*(basic.CanEx), basic.DeviceId), 
			TypeCode(typeCode), Version(basic.Version), 
			SensorNum(basic.SensorNum), IsLockController(IS_LC(basic.DeviceId)), 
			GroupId(OBTAIN_GROUP_ID(basic.DeviceId)), NodeId(OBTAIN_NODE_ID(basic.DeviceId))
	{
	}
	
	void StorageUnit::SetTemperature(CANExtended::CanEx &ex, float t)
	{
		CANExtended::OdEntry entry(DeviceAttribute::Temperature, 1);
		entry.SetVal(reinterpret_cast<uint8_t *>(&t), sizeof(float));
		ex.Broadcast(entry);
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
	
	void StorageUnit::SetInventoryInfo(SerializableObjects::SuppliesItem &info)
	{
		static constexpr size_t fixSize = sizeof(uint64_t)+sizeof(float)*2+1;
		size_t size = info.MaterialName.size() + fixSize;
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(size);
		data[0] = info.Index & 0xff;
		uint8_t *ptr = data.get()+1;
		memcpy(ptr, &info.MaterialId, sizeof(uint64_t));
		ptr += sizeof(uint64_t);
		memcpy(ptr, &info.UnitWeight, sizeof(float));
		ptr += sizeof(float);
		memcpy(ptr, &info.ErrorRange, sizeof(float));
		ptr += sizeof(float);
		if (info.MaterialName.size() > 0)
			std::copy(info.MaterialName.begin(), info.MaterialName.end(), ptr);
		WriteAttribute(DeviceAttribute::InventoryInfo, data, size);
	}
	
	void StorageUnit::ClearAllInventoryInfo()
	{
		boost::shared_ptr<uint8_t[]> data = boost::make_shared<uint8_t[]>(1);
		data[0] = 0xff;
		WriteAttribute(DeviceAttribute::InventoryInfo, data, 1);
	}
	
	void StorageUnit::SetInventoryQuantities(Array<SerializableObjects::InventoryQuantity> &quantities)
	{
		const size_t size = (sizeof(uint64_t)+2)*quantities.Count()+1;
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(size);
		data[0] = quantities.Count();
		uint8_t *ptr = data.get()+1;
		for(auto i=0; i<quantities.Count(); ++i)
		{
			memcpy(ptr, &quantities[i].MaterialId, sizeof(uint64_t));
			ptr += sizeof(uint64_t);
			ptr[0] = quantities[i].Quantity&0xff;
			ptr[1] = quantities[i].Quantity>>8;
			ptr += 2;
		}
		WriteAttribute(DeviceAttribute::InventoryQuantity, data, size);
	}
	
	
	void StorageUnit::NoticeInventoryById(std::uint64_t id, bool notice)
	{
		static constexpr size_t size = sizeof(uint64_t)+1;
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(size);
		uint8_t *ptr = data.get();
		memcpy(ptr, &id, sizeof(uint64_t));
		data[sizeof(uint64_t)] = notice;
		WriteAttribute(DeviceAttribute::NoticeInventory, data, size);
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
				allStable = val[3]!=0;
				inventoryExpected = val[4]!=0;
				memcpy(const_cast<float *>(&deltaWeight), val.get()+4, sizeof(float));
				break;
			case DeviceSync::SyncDoor:
#ifdef DEBUG_PRINT
			cout<<"#Device 0x"<<std::hex<<(((GroupId&0xf)<<3)|(NodeId&0x7))<<(val[0]!=0?" Unlocked":" Locked")<<std::dec<<endl;
#endif
				if (OnDoorChangedEvent)
					OnDoorChangedEvent(GroupId, NodeId, val[0]!=0);
				break;
			default:
				break;
		}
	}
}

