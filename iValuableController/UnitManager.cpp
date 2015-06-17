#include "UnitManager.h"
#include "UnityUnit.h"
#include "IndependentUnit.h"
#include "System.h"

using namespace std;

#define FILE_HEADER_SIZE	12

namespace IntelliStorage
{
	boost::scoped_ptr<osThreadDef_t> UnitManager::UpdateThreadDef;
	
	volatile bool UnitManager::updating = false;
	
	bool UnitManager::LockGroup::LockOne(std::uint8_t node) 
	{ 
		auto it = lockMap.find(node);
		if (it == lockMap.end())
			return open;
		it->second = false;
			
		bool openAny = false;
		for(auto item = lockMap.begin(); item != lockMap.end(); ++item)
			if (item->second)
			{
				openAny = true;
				break;
			}
		open = openAny;
		changed = (open != last);
		last = open;
		return open;
	}
	
	bool UnitManager::LockGroup::UnlockOne(std::uint8_t node)
	{
		auto it = lockMap.find(node);
		if (it == lockMap.end())
			return open;
		it->second = true;	
		open = true;
		changed = (open != last);
		last = open;
		return open;
	}
	
	void UnitManager::LockGroup::AddLock(std::uint8_t node)
	{ 
		lockMap[node] = false;
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
		uint8_t status = 0;
		
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
				//First 8 bytes of basic info, and 4 bytes for CRC
				ptr += FILE_HEADER_SIZE;
				break;
			}
		}
		if (i>=4)		//Unfound
			return false;
		
		updating = true;
		
		//Make unit entering ISP mode
		unit->EnterCanISP();
		osDelay(100);
		
		if (!manager->updater->FindDevice())
		{
			updating = false;
			return false;
		}
		if (!manager->updater->Format())
		{
			updating = false;
			return false;
		}
		
		//Program 
		for(i=0;i<(size>>8);++i)
		{
			if (!manager->updater->ProgramData(i, ptr, 0x100))
			{
				updating = false;
				return false;
			}
			manager->comm->SendFileData(CommandWrite, &status, 1);
			ptr+=0x100;
		}
		if (size&0xff)
		{
			if (!manager->updater->ProgramData(i, ptr, size&0xff))
			{
				updating = false;
				return false;
			}
			status = 1;	//Program Completed
			manager->comm->SendFileData(CommandWrite, &status, 1);
		}
		
		unit->updateStatus = StorageUnit::Updated;
		
		if (!manager->updater->RestartDevice())
		{
			updating = false;
			return false;
		}
		
		updating = false;
		osDelay(100);
		
		osSignalClear(osThreadGetId(), 0x01);
		unit->canex.Sync(unit->DeviceId, DeviceSync::SyncLive, CANExtended::Trigger);
		osEvent evt = osSignalWait(0x01, 1000);
		return (evt.status == osEventSignal);
	}
	
	void UnitManager::SyncUpdate()
	{
		if (tid!=NULL)
			osSignalSet(tid, 0x01);
	}
	
	void UnitManager::UpdateThread(void const *arg)
	{
		if (arg==NULL)
			return;
		
		//Copy arg to local and release
		boost::shared_ptr<UpdateThreadArgs> updater(new UpdateThreadArgs);
		memcpy(updater.get(), arg, sizeof(UpdateThreadArgs));
		delete (UpdateThreadArgs *)(arg);
		
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
		
		status[0] = 0xff;
		status[1] = 0;
		manager->comm->SendFileData(CommandUpdate, status, 2);
	}
	
	void UnitManager::CommandArrival(std::uint8_t command, std::uint8_t *parameters, std::size_t len)
	{
		static uint16_t offset = 0;
		static uint8_t index = 0; 
		uint8_t status[5] = {command, NoError};
		boost::shared_ptr<uint8_t[]> data;
		uint16_t size;
		uint32_t word;
		const uint8_t *ptr;
		uint8_t *p;
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
					CRCReset();
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
				if (offset+len-FILE_HEADER_SIZE>size)	//If current transmit length over total file size
				{
					status[1] = ErrorLength;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				status[0] = (offset+len-FILE_HEADER_SIZE == size); //If file completed
					
				ptr = parameters;
				HAL_FLASH_Unlock();
				for(auto i=0;i<(len>>2);++i)
				{
					memcpy(&word, ptr, 4);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					CRCPush(__REV(word));
					offset+=4;
					ptr+=4;
				}
				len&=0x3;
				if (len)
				{
					word=0xffffffffu;
					memcpy(&word, ptr, len);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					CRCPush(__REV(word));
					offset+=len;
				}
				if (status[0])
				{
					HAL_FLASH_Program(TYPEPROGRAM_BYTE, CODE_BASE[index]+3, 0x00);	//Tag for file completed
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+8, GetCRCValue());	//Write CRC Value
					memcpy(status+1, (uint8_t *)(CODE_BASE[index]+8), 4);
					comm->SendFileData(command, status, 5);
				}
				else
					comm->SendFileData(command, status, 1);	//Return if file completed or not
				HAL_FLASH_Lock();
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
						//Reserve 4 bytes for CRC
						offset = FILE_HEADER_SIZE;
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
			case CommandDevices:
				size = unitList.size()*4;
				data = boost::make_shared<uint8_t[]>(size+1);
				data[0] = unitList.size();
				if (size==0)
				{
					comm->SendFileData(command, data.get(), 1);
					break;
				}
				p = data.get()+1;
				for(auto it=unitList.begin() ; it!=unitList.end(); ++it)
				{
					p[0] = it->second->DeviceId & 0xff;
					p[1] = it->second->TypeCode;
					p[2] = it->second->Version>>8;
					p[3] = it->second->Version&0xff;
					p += 4;
				}
				comm->SendFileData(command, data.get(), size+1);
				break;
			default:
				status[1] = ErrorUnknown;
				comm->SendFileData(CommandStatus, status, 2);
				break;
		}
	}
	
	void UnitManager::SyncAllData()
	{
		for (auto it = unitList.begin(); it!= unitList.end(); ++it)
		{
			if (!updating && it->second->UpdateStatus() != StorageUnit::Updating)
			{
				it->second->SyncUnitData();
				osDelay(20);
			}
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
			if ((forceReport || it->second->CheckChanged()) && ReportDoorDataEvent)
			{
				ReportDoorDataEvent(it->first, it->second->IsOpen());

//				//When close door, clear all notices on same group
//				if (!it->second->IsOpen())
//					for (auto uit = unitList.begin(); uit!= unitList.end(); ++uit)
//					{
//						if (uit->second->GroupId==it->first)
//							uit->second->SetNotice(0);
//					}
			}
		}
	}
	
	bool UnitManager::Unlock(std::uint8_t groupId, bool &isOpen)
	{
		auto groupIt = groupList.find(groupId);
		if (groupIt == groupList.end())
			return false;
		isOpen = groupIt->second->IsOpen();
		if (isOpen)
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
	
	void UnitManager::OnDoorChanged(std::uint8_t groupId, std::uint8_t nodeId, bool open)
	{
		if (open)
			groupList[groupId]->UnlockOne(nodeId);
		else
			groupList[groupId]->LockOne(nodeId);
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
			group->AddLock(unit->NodeId);
		
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
