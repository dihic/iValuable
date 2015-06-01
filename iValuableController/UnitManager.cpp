#include "UnitManager.h"
#include "UnityUnit.h"
#include "IndependentUnit.h"

using namespace std;

namespace IntelliStorage
{
	bool UnitManager::LockGroup::LockOne() 
	{ 
		if (count==num) //error state
			return open; 
		open = (++count!=num);
		changed = (open != last);
		last = open;
		return open;
	}
	
	bool UnitManager::LockGroup::UnlockOne()
	{
		if (count==0) //error state
			return open; 
		open = (--count==0);
		changed = (open != last);
		last = open;
		return open;
	}
	
	void UnitManager::LockGroup::AddLock(bool locked)
	{ 
		++num; 
		if (locked)
			LockOne();
	}
	
	UnitManager::UnitManager()
	{
		dataCollection.reset(new SerializableObjects::UnitEntryCollection);
	}
	
	void UnitManager::Traversal(bool forceReport)
	{		
		for (auto it = unitList.begin(); it!= unitList.end(); ++it)
		{
			auto rfidUnit = boost::dynamic_pointer_cast<RfidUnit>(it->second);
			if (rfidUnit == nullptr)
				continue;
			if (forceReport)
			{
				if (rfidUnit->GetCardState() != RfidStatus::CardArrival)
					continue;
			}
			else if (!rfidUnit->CardChanged())
				continue;
			if (ReportRfidDataEvent)
				ReportRfidDataEvent(rfidUnit);
		}
		
		for(auto it = groupList.begin(); it!=groupList.end(); ++it)
		{
			if ((forceReport || it->second->IsChanged()) && ReportDoorDataEvent)
				ReportDoorDataEvent(it->first, it->second->IsOpen());
		}
	}
	
	boost::shared_ptr<RfidUnit> UnitManager::FindUnit(const std::string &cardId)
	{
		boost::shared_ptr<RfidUnit> result;
		for (auto it = unitList.begin(); it != unitList.end(); ++it)
		{
			auto unit = boost::dynamic_pointer_cast<RfidUnit>(it->second);
			if (unit == nullptr)
				continue;
			if (unit->IsEmpty())
				continue;
			if (unit->GetCardState()==RfidStatus::CardArrival && cardId == unit->GetCardId())
			{
				result = unit;
				break;
			}
		}
		return result;
	}
	
	boost::shared_ptr<StorageUnit> UnitManager::FindUnit(std::uint16_t id)
	{
		boost::shared_ptr<StorageUnit> unit;
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
	
	boost::shared_ptr<UnitManager::LockGroup> UnitManager::ObtainGroup(std::uint8_t id)
	{
		boost::shared_ptr<UnitManager::LockGroup> group;
		auto it = groupList.find(id);
		if (it == groupList.end())
		{
			//Create a group
			group.reset(new UnitManager::LockGroup);
			groupList[id] = group;
		}
		else
			group = it->second;
		return group;
	}
	
	void UnitManager::OnDoorChanged(std::uint8_t groupId, bool open)
	{
		if (open)
			groupList[groupId]->UnlockOne();
		else
			groupList[groupId]->LockOne();
	}
	
	void UnitManager::Add(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit) 
	{
		unit->OnDoorChangedEvent.bind(this, &UnitManager::OnDoorChanged);
		unitList[id] = unit; 
		bool find = false;
		
		auto group = ObtainGroup(unit->GroupId);
		
		if (unit->IsLockController)
			group->AddLock(!unit->IsDoorOpen());
		
		auto count = dataCollection->AllUnits.Count();
		
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
