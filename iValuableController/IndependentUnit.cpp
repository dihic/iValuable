#include "IndependentUnit.h"

using namespace std;

namespace IntelliStorage
{			
	IndependentUnit::IndependentUnit(CANExtended::CanEx &ex, std::uint16_t id, std::uint8_t sensorNum)
		: WeightBase<ScaleData>(ex, id, sensorNum)
	{
		
	}
	
	void IndependentUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry)
	{
		StorageUnit::ProcessRecievedEvent(entry);
		const DeviceSync syncIndex = static_cast<DeviceSync>(entry->Index);
		auto val = entry->GetVal();
		auto len = entry->GetLen();
		int base, index;
		switch (syncIndex)
		{
			case DeviceSync::SyncData:
				base = 4+sizeof(float);
				for(auto i=0; i<val[1]; ++i)
				{
					index = val[base++];
					scaleList[index].IsStable = val[base++]!=0;
					scaleList[index].Status = val[base++];
					scaleList[index].TempQuantity = val[base++];
					memcpy(reinterpret_cast<void *>(&scaleList[index].Weight), val.get()+base, sizeof(float));
					base += sizeof(float);
				}
				break;
			default:
				break;
		}
	}
}

