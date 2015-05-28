#include "UnityUnit.h"

using namespace std;

namespace IntelliStorage
{		
	UnityUnit::UnityUnit(CANExtended::CanEx &ex, std::uint16_t id, std::uint8_t sensorNum)
		: WeightBase<std::uint8_t>(ex, id, sensorNum)
	{
		
	}
	
	void UnityUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry)
	{
		StorageUnit::ProcessRecievedEvent(entry);
		const DeviceSync syncIndex = static_cast<DeviceSync>(entry->Index);
		auto val = entry->GetVal();
		auto len = entry->GetLen();
		int base;
		switch (syncIndex)
		{
			case DeviceSync::SyncData:
				base = 4+sizeof(float);
				memcpy(reinterpret_cast<void *>(&totalWeight), val.get()+base, sizeof(float));
				base += sizeof(float);
				for(auto i=0; i<val[1]; ++i)
					scaleList[val[base++]] = val[base++];		//Sensor Status
				break;
			default:
				break;
		}
	}
}

