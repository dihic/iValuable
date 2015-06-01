#ifndef _UNIT_MANAGER_H
#define _UNIT_MANAGER_H

#include <map>
#include <cstdint>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include "CanDevice.h"
#include "StorageUnit.h"
#include "RfidUnit.h"
#include "FastDelegate.h"
#include "CommStructures.h"

using namespace fastdelegate;

namespace IntelliStorage
{	
	class UnitManager
	{
		public:
			class LockGroup
			{
				private:
					volatile std::uint8_t num = 0;
					volatile std::uint8_t count = 0;
					volatile bool open = false;
					volatile bool last = false;
					volatile bool changed = false;
				public:
					bool LockOne();
					bool UnlockOne();
					void AddLock(bool locked);
					bool IsOpen() const { return open; }
					bool IsChanged() const { return changed; }
			};
		private:
			std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > unitList;
			std::map<std::uint16_t, boost::shared_ptr<SerializableObjects::UnitEntry> > entryList;
			std::map<std::uint8_t, boost::shared_ptr<LockGroup> > groupList;

			boost::shared_ptr<SerializableObjects::UnitEntryCollection> dataCollection;
			
			boost::shared_ptr<LockGroup> ObtainGroup(std::uint8_t id);
			void OnDoorChanged(std::uint8_t groupId, bool open);
		public:
			typedef FastDelegate1<boost::shared_ptr<RfidUnit> &> ReportRfidDataHandler;
			typedef FastDelegate2<std::uint8_t, bool> ReportDoorDataHandler;
			ReportRfidDataHandler ReportRfidDataEvent;
			ReportDoorDataHandler ReportDoorDataEvent;
		
			UnitManager();
			~UnitManager() {}
			void Add(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit);
			//std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > &GetList() { return unitList; }
			//std::map<std::uint8_t, boost::shared_ptr<LockGroup> > &GetLockGroups() { return groupList; }
			
			void Traversal(bool forceReport);
			
			void UpdateLatest(boost::shared_ptr<CanDevice> &unit);
			boost::shared_ptr<RfidUnit> FindUnit(const std::string &cardId);
			boost::shared_ptr<StorageUnit> FindUnit(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntry> FindUnitEntry(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntryCollection> &PrepareData();
	};
	
}

#endif
