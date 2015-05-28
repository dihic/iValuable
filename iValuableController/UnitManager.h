#ifndef _UNIT_MANAGER_H
#define _UNIT_MANAGER_H

#include <map>
#include <cstdint>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include "CanDevice.h"
#include "StorageUnit.h"
#include "FastDelegate.h"
#include "CommStructures.h"

using namespace fastdelegate;

namespace IntelliStorage
{	
	class UnitManager
	{
		private:
			std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > unitList;
			std::map<std::uint16_t, boost::shared_ptr<SerializableObjects::UnitEntry> > entryList;
//			std::map<std::string, boost::shared_ptr<CardListItem> > cardList;
			boost::shared_ptr<SerializableObjects::UnitEntryCollection> dataCollection;
		public:
			typedef std::map<std::uint16_t, boost::shared_ptr<CanDevice> >::iterator UnitIterator;
//			typedef FastDelegate1<boost::shared_ptr<StorageUnit> > SendDataCB;
//			SendDataCB OnSendData;
			UnitManager();
			~UnitManager() {}
			void Add(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit);
			std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > &GetList() { return unitList; }
			
			void UpdateLatest(boost::shared_ptr<CanDevice> &unit);
			boost::shared_ptr<CanDevice> FindUnit(const std::string &cardId);
			boost::shared_ptr<CanDevice> FindUnit(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntry> FindUnitEntry(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntryCollection> &PrepareData();
	};
	
}

#endif
