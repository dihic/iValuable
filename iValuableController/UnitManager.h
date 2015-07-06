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
#include "ConfigComm.h"
#include "ISPProgram.h"

using namespace fastdelegate;

namespace IntelliStorage
{	
	class UnitManager
	{
		public:
			class LockGroup
			{
				private:
					std::map<std::uint8_t, bool> lockMap;
//					volatile std::uint8_t num = 0;
//					volatile std::uint8_t count = 0;
					volatile bool open = false;
					volatile bool last = false;
					volatile bool changed = false;
				public:
					bool LockOne(std::uint8_t node);
					bool UnlockOne(std::uint8_t node);
					void AddLock(std::uint8_t node);
					bool IsOpen() const { return open; }
					bool CheckChanged() 
					{ 
						bool result = changed;
						changed = false;
						return result; 
					}
			};
		private:
			enum CommandType
			{
				CommandAccess				= 0xc2,
				CommandWrite 				= 0xc4,
				CommandWriteInfo		=	0xce,
				CommandReadInfo 		= 0xcf,
				CommandUpdate				= 0xd0,
				CommandDevices			= 0xd1,
				CommandVersion			= 0xd2,
				CommandStatus				= 0xfe
			};
			enum ErrorType
			{
				NoError						= 0x00,
				ErrorUnknown			= 0x01,
				ErrorParameter		= 0x02,
				ErrorChecksum			= 0x03,
				ErrorFailure 			= 0x04,
				ErrorBusy   			= 0x05,
				ErrorLength   		= 0x06,
				ErrorGeneric			= 0xff
			};
			
			enum UpdateType
			{
				UpdateAll,
				UpdateType,
				UpdateSingle,
			};
			
			struct UpdateThreadArgs
			{
				UnitManager *Manager;
				std::uint8_t Routine;
				std::uint8_t UnitType;
				std::uint8_t Id;
			};
			const boost::shared_ptr<ConfigComm> &comm;
			const boost::shared_ptr<ISPProgram> &updater;
			std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > unitList;
			std::map<std::uint16_t, boost::shared_ptr<SerializableObjects::UnitEntry> > entryList;
			std::map<std::uint8_t, boost::shared_ptr<LockGroup> > groupList;

			boost::shared_ptr<SerializableObjects::UnitEntryCollection> dataCollection;
			
			boost::shared_ptr<LockGroup> ObtainGroup(std::uint8_t id);
			void OnDoorChanged(std::uint8_t groupId, std::uint8_t nodeId, bool open);
			void CommandArrival(std::uint8_t command, std::uint8_t *parameters, std::size_t len);
			static boost::scoped_ptr<osThreadDef_t> UpdateThreadDef;
			static volatile bool updating;
			static bool UpdateOne(UnitManager *manager, boost::shared_ptr<StorageUnit> &unit, std::uint8_t type=0x00);
			static void UpdateThread(void const *arg);
			osThreadId tid = NULL;
		public:
			static bool IsUpdating() { return UnitManager::updating; }
			typedef FastDelegate1<boost::shared_ptr<RfidUnit> &> ReportRfidDataHandler;
			typedef FastDelegate2<std::uint8_t, bool> ReportDoorDataHandler;
			ReportRfidDataHandler ReportRfidDataEvent;
			ReportDoorDataHandler ReportDoorDataEvent;
		
			UnitManager(ARM_DRIVER_USART &u, boost::shared_ptr<ISPProgram> &isp);
			~UnitManager() {}
			void SyncAllData();
			void SyncUpdate();
			void Recover(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit);
			void Add(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit);
			//std::map<std::uint16_t, boost::shared_ptr<StorageUnit> > &GetList() { return unitList; }
			//std::map<std::uint8_t, boost::shared_ptr<LockGroup> > &GetLockGroups() { return groupList; }
			
			void Traversal(bool forceReport);
		
			bool Unlock(std::uint8_t groupId, bool &isOpen);
			boost::shared_ptr<RfidUnit> FindUnit(const std::string &cardId);
			boost::shared_ptr<StorageUnit> FindUnit(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntry> FindUnitEntry(std::uint16_t id);
			boost::shared_ptr<SerializableObjects::UnitEntryCollection> &PrepareData();
	};
	
}

#endif
