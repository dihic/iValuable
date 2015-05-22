#include "UnitManager.h"

using namespace std;

namespace IntelliStorage
{
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
		UnitIterator it = unitList.find(id);
		if (it != unitList.end())
			unit = it->second;
		return unit;
	}
	
	void UnitManager::Add(std::uint16_t id, boost::shared_ptr<CanDevice> unit) 
	{
		unitList[id] = unit; 
	}
}
