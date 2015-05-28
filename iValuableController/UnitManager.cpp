#include "UnitManager.h"
#include "UnityUnit.h"
#include "IndependentUnit.h"

#include <boost/foreach.hpp>

using namespace std;

namespace IntelliStorage
{
	UnitManager::UnitManager()
	{
		dataCollection.reset(new SerializableObjects::UnitEntryCollection);
	}
	
	void UnitManager::UpdateLatest(boost::shared_ptr<CanDevice> &unit)
	{
//		string cardId = unit->GetCardId();
//		for (UnitIterator it = unitList.begin(); it != unitList.end(); ++it)
//		{
//			if (it->second->IsEmpty() || it->second==unit)
//				continue;
//			if (it->second->IsLatest() && cardId == it->second->GetCardId())
//			{
//				it->second->latest = false;
//				it->second->IndicatorOff();
//			}
//		}
	}
	
	boost::shared_ptr<CanDevice> UnitManager::FindUnit(const std::string &cardId)
	{
		boost::shared_ptr<CanDevice> unit;
//		for (UnitIterator it = unitList.begin(); it != unitList.end(); ++it)
//		{
//			if (it->second->IsEmpty())
//				continue;
//			if (it->second->IsLatest() && cardId == it->second->GetCardId())
//			{
//				unit = it->second;
//				break;
//			}
//		}
		return unit;
	}
	
	boost::shared_ptr<CanDevice> UnitManager::FindUnit(std::uint16_t id)
	{
		boost::shared_ptr<CanDevice> unit;
		auto it = unitList.find(id);
		if (it != unitList.end())
			unit = it->second;
		return unit;
	}
	
	boost::shared_ptr<SerializableObjects::UnitEntry> UnitManager::FindUnitEntry(std::uint16_t id)
	{
		boost::shared_ptr<SerializableObjects::UnitEntry> entry;
		auto it = entryList.find(id);
		if (it != entryList.end())
			entry = it->second;
		return entry;
	}
	
	void UnitManager::Add(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit) 
	{
		unitList[id] = unit; 
		
		auto count = dataCollection->AllUnits.Count();
		bool find = false;
		for(auto i=0; i<count; ++i)
			if (dataCollection->AllUnits[i].GroupIndex==unit->GroupId && 
					dataCollection->AllUnits[i].NodeIndex==unit->NodeId)
			{
				find = true;
				break;
			}
		if (!find)
		{
			boost::shared_ptr<SerializableObjects::UnitEntry> element(new SerializableObjects::UnitEntry);
			element->GroupIndex = unit->GroupId;
			element->NodeIndex = unit->NodeId;
			element->Weight = 0;
			element->IsStable = false;
			entryList[id] = element;
			dataCollection->AllUnits.Add(element);
		}
	}
	
	boost::shared_ptr<SerializableObjects::UnitEntryCollection> &UnitManager::PrepareData()
	{
		boost::shared_ptr<SerializableObjects::ScaleState> state;
		for(auto it = unitList.begin(); it!=unitList.end(); ++it)
		{
			auto element = FindUnitEntry(it->first);
			element->IsStable = it->second->GetAllStable();
			element->SensorStates.Clear();
			auto unity = boost::dynamic_pointer_cast<UnityUnit>(it->second);
			if (unity!=nullptr)
			{
				element->Weight = unity->GetTotalWeight();
				auto scales = unity->GetScaleList();
				for(auto i=0; i<unity->SensorNum; ++i)
				{
					if (!unity->IsEnable(i))
						continue;
					state.reset(new SerializableObjects::ScaleState);
					state->SensorIndex = i;
					state->IsDamaged = (scales[i]&0x10)!=0;
					state->WeightState = scales[i]&0x0f;
					state->Weight = 0;
					state->IsStable = false;
					element->SensorStates.Add(state);
				}
			}
			else
			{
				auto integrity = boost::dynamic_pointer_cast<IndependentUnit>(it->second);
				if (integrity!=nullptr)
				{
					element->Weight = 0;
					auto scalesData = integrity->GetScaleList();
					for(auto i=0; i<integrity->SensorNum; ++i)
					{
						if (!integrity->IsEnable(i))
							continue;
						state.reset(new SerializableObjects::ScaleState);
						state->SensorIndex = i;
						state->IsDamaged = (scalesData[i].Status&0x10)!=0;
						state->WeightState = scalesData[i].Status&0x0f;
						state->Weight = scalesData[i].Weight;
						state->IsStable = scalesData[i].IsStable;
						element->SensorStates.Add(state);
					}
				}
			}
		}
		return dataCollection;
	}
}
