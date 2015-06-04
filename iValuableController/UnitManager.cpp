#include "UnitManager.h"
#include "UnityUnit.h"
#include "IndependentUnit.h"
#include "System.h"

using namespace std;

namespace IntelliStorage
{
	boost::scoped_ptr<osThreadDef_t> UnitManager::UpdateThreadDef;
	
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
	
	UnitManager::UnitManager(ARM_DRIVER_USART &u, boost::scoped_ptr<ISPProgram> &isp)
		:comm(ConfigComm::CreateInstance(u)), updater(isp)
	{
		dataCollection.reset(new SerializableObjects::UnitEntryCollection);
		if (UpdateThreadDef == nullptr)
			UpdateThreadDef.reset(new osThreadDef_t { UpdateThread, osPriorityNormal, 1, 0});
		comm->OnFileCommandArrivalEvent.bind(this, &UnitManager::CommandArrival);
		comm->Start();
	}
	
	bool UnitManager::UpdateOne(UnitManager *manager, boost::shared_ptr<StorageUnit> &unit, std::uint8_t type)
	{
		//Determine FW data and size
		const uint8_t *ptr = NULL;
		uint16_t size = 0;
		if (type==0)
			type = unit->TypeCode;
		uint8_t i;
		for(i=0; i<4; ++i)
		{
			ptr = (const uint8_t *)(CODE_BASE[i]);
			if (ptr[0]!=CODE_TAG1 || ptr[1]!=CODE_TAG2 || ptr[3]!=0x00)
				continue;
			if (ptr[2]==type)
			{
				size = ptr[6]|(ptr[7]<<8);
				ptr += 8;
				break;
			}
		}
		if (i>=4)		//Unfound
			return false;
		
		//Make unit entering ISP mode
		unit->EnterCanISP();
		osDelay(100);
		
		if (!manager->updater->FindDevice())
			return false;
		if (!manager->updater->Format())
			return false;
		
		//Program 
		for(i=0;i<(size>>8);++i)
		{
			if (!manager->updater->ProgramData(i, ptr, 0x100))
				return false;
			ptr+=0x100;
		}
		if (size&0xff)
			if (!manager->updater->ProgramData(i, ptr, size&0xff))
				return false;
		
		unit->updateStatus = StorageUnit::Updated;
		
		if (!manager->updater->RestartDevice())
			return false;
		osDelay(100);
		
		unit->canex.Sync(unit->DeviceId, DeviceSync::SyncLive, CANExtended::Trigger);
		return true;
	}
	
	void UnitManager::UpdateThread(void const *arg)
	{
		if (arg==NULL)
			return;
		auto updater = reinterpret_cast<UpdateThreadArgs *>(const_cast<void *>(arg));
		auto manager = updater->Manager;
		boost::shared_ptr<StorageUnit> unit;
		
		uint8_t status[2];
		switch (updater->Routine)
		{
			case UpdateAll:
				for(auto it=manager->unitList.begin() ; it!=manager->unitList.end(); ++it)
				{
					status[0] = it->second->DeviceId & 0x7f;
					status[1] = UpdateOne(manager, it->second);
					manager->comm->SendFileData(CommandUpdate, status, 2);
				}
				break;
			case UpdateType:
				for(auto it=manager->unitList.begin() ; it!=manager->unitList.end(); ++it)
				{
					if (it->second->TypeCode != updater->UnitType)
						continue;
					status[0] = it->second->DeviceId & 0x7f;
					status[1] = UpdateOne(manager, it->second);
					manager->comm->SendFileData(CommandUpdate, status, 2);
				}
				break;
			case UpdateSingle:
				status[0] = updater->Id;
				status[1] = 0;
				unit = manager->FindUnit(updater->Id);
				if (unit != nullptr)
					status[1] = UpdateOne(manager, unit, updater->UnitType);
				manager->comm->SendFileData(CommandUpdate, status, 2);
				break;
			default:
				break;
		}
		delete updater;
		status[0] = 0xff;
		status[1] = 0;
		manager->comm->SendFileData(CommandUpdate, status, 2);
	}
	
	void UnitManager::CommandArrival(std::uint8_t command, std::uint8_t *parameters, std::size_t len)
	{
		static uint16_t offset = 0;
		static uint8_t index = 0; 
		uint8_t status[2] = {command, NoError};
		boost::shared_ptr<uint8_t[]> data;
		uint16_t size;
		uint32_t word;
		const uint8_t *ptr;
		osThreadId tid;
		UpdateThreadArgs *args;
		
		switch (command)
		{
			case CommandAccess:
				if (len<1 || parameters[0]>=4)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
				}
				else
				{
					index = parameters[0];
					offset = 0;
					EraseCodeFlash(index);
					status[0] = index;
					comm->SendFileData(command, status, 1);
				}
				break;
			case CommandWrite:
				if (len<=2)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				ptr = (const uint8_t *)(CODE_BASE[index]+6);
				memcpy(&size, ptr, 2);	//File size
				if (offset+len-8>size)	//If current transmit length over total file size
				{
					status[1] = ErrorLength;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				status[0] = (offset+len-8 == size); //If file completed
					
				ptr = parameters+2;
				HAL_FLASH_Unlock();
				for(auto i=0;i<(len>>2);++i)
				{
					memcpy(&word, ptr, 4);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					offset+=4;
					ptr+=4;
				}
				len&=0x3;
				if (len)
				{
					word=0;
					memcpy(&word, ptr, len);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					offset+=len;
				}
				if (status[0])
					HAL_FLASH_Program(TYPEPROGRAM_BYTE, CODE_BASE[index]+3, 0x00);	//Tag for file completed
				HAL_FLASH_Lock();
				comm->SendFileData(command, status, 1);	//Return if file completed or not
				break;
			case CommandWriteInfo:
				//Firmware tags 0xAA 0xBB
				//Unit Type
				//Completed byte (0xff : false 0x00 : true)
				//Version major byte
				//Version minor byte
				//File size	2 byte
				if (len<8)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
				}
				else
				{
					if (parameters[0]==CODE_TAG1 && parameters[1]==CODE_TAG2)
					{
						HAL_FLASH_Unlock();
						parameters[3] = 0xff;	//Incompleted
						memcpy(&word, parameters, 4);
						HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index], word);
						memcpy(&word, parameters+4, 4);
						HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+4, word);
						HAL_FLASH_Lock();
						offset = 8;
						comm->SendFileData(command, status+1, 1);
					}
					else
					{
						status[1] = ErrorParameter;
						comm->SendFileData(CommandStatus, status, 2);
					}
				}
				break;
			case CommandReadInfo:
				if (len>0 && parameters[0]>=4)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				if (len==0)
				{
					size = 32;
					data = boost::make_shared<uint8_t[]>(size);
					for(auto i=0; i<4; ++i)
					{
						ptr = (uint8_t *)(CODE_BASE[i]);
						memcpy(data.get()+i*8, ptr, 8);
					}
					comm->SendFileData(command, data.get(), size);
				}
				else
				{
					ptr = (uint8_t *)(CODE_BASE[parameters[0]]);
					comm->SendFileData(command, ptr, 8);
				}
				break;
			case CommandUpdate:
				if (len<3)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				args = new UpdateThreadArgs { this, parameters[0], parameters[1], parameters[2] };
				
				tid = osThreadCreate(UpdateThreadDef.get(), args);
				if (tid==NULL)
				{
					delete args;
					status[1] = ErrorBusy;
					comm->SendFileData(CommandStatus, status, 2);
				}
				
				break;
			default:
				status[1] = ErrorUnknown;
				comm->SendFileData(CommandStatus, status, 2);
				break;
		}
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
	
	bool UnitManager::Unlock(std::uint8_t groupId)
	{
		auto groupIt = groupList.find(groupId);
		if (groupIt == groupList.end())
			return false;
		if (groupIt->second->IsOpen())
			return true;
		bool result = false;
		for (auto it = unitList.begin(); it!= unitList.end(); ++it)
			if (it->second->IsLockController && it->second->GroupId == groupId)
			{
				it->second->LockControl(true);
				result = true;
			}
		return result;
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
	
	void UnitManager::Recover(std::uint16_t id, boost::shared_ptr<StorageUnit> &unit) 
	{
		unitList.erase(id);
		unit->OnDoorChangedEvent.bind(this, &UnitManager::OnDoorChanged);
		unitList[id] = unit; 
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
