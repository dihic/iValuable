#include "IndependentUnit.h"
#include "System.h"

using namespace std;

namespace IntelliStorage
{			
	IndependentUnit::IndependentUnit(StorageBasic &basic)
		: WeightBase<ScaleData>(UNIT_TYPE_INDEPENDENT, basic)
	{
		
	}
	
	void IndependentUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry)
	{
		SensorUnit::ProcessRecievedEvent(entry);
		const DeviceSync syncIndex = static_cast<DeviceSync>(entry->Index);
		auto val = entry->GetVal();
		auto len = entry->GetLen();
		int base, index;
		switch (syncIndex)
		{
			case DeviceSync::SyncData:
				base = 5+sizeof(float);
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

