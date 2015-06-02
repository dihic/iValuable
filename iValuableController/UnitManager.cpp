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
	
	UnitManager::UnitManager(ARM_DRIVER_USART &u)
		:comm(ConfigComm::CreateInstance(u))
	{
		dataCollection.reset(new SerializableObjects::UnitEntryCollection);
		if (UpdateThreadDef == nullptr)
			UpdateThreadDef.reset(new osThreadDef_t { UpdateThread, osPriorityNormal, 1, 0});
		comm->OnFileCommandArrivalEvent.bind(this, &UnitManager::CommandArrival);
		comm->Start();
	}
	
	void UnitManager::UpdateThread(void const *arg)
	{
		auto manager = reinterpret_cast<UnitManager *>(const_cast<void *>(arg));
		if (manager==NULL)
			return;
		for(auto it=manager->unitList.begin() ; it!=manager->unitList.end(); ++it)
		{
			it->second->EnterCanISP();
			//Request for updating
		}
	}
	
	void UnitManager::CommandArrival(std::uint8_t command, std::uint8_t *parameters, std::size_t len)
	{
		static uint16_t offset = 0;
		static uint8_t index = 0; 
		uint8_t status[2] = {command, NoError};
		boost::shared_ptr<uint8_t[]> data;
		uint16_t dataLen, temp;
		uint32_t word;
		uint8_t *ptr;
		osThreadId tid;
		
		switch (command)
		{
			case CommandAccess:
				if (len<1 || parameters[0]>=4)
					status[1] = ErrorParameter;
				else
				{
					index = parameters[0];
					offset = 0;
					EraseCodeFlash(index);
				}
				comm->SendFileData(CommandStatus, status, 2);
				break;
			case CommandWrite:
				if (len<=2)
				{
					status[1] = ErrorParameter;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				dataLen = parameters[0] | (parameters[1]<<8);
				ptr = (uint8_t *)(CODE_BASE[index]+6);
				memcpy(&temp, ptr, 2);	//File size
				if ((dataLen!=len-2) || (offset+dataLen-8>temp))	//If current transmit length mismatch or over total file size
				{
					status[1] = ErrorLength;
					comm->SendFileData(CommandStatus, status, 2);
					break;
				}
				status[0] = (offset+dataLen-8 == temp); //Is file completed
					
				ptr = parameters+2;
				for(auto i=0;i<(dataLen>>2);++i)
				{
					memcpy(&word, ptr, 4);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					offset+=4;
					ptr+=4;
				}
				dataLen&=0x3;
				if (dataLen)
				{
					word=0;
					memcpy(&word, ptr, dataLen);
					HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+offset, word);
					offset+=dataLen;
				}
				if (status[0])
					HAL_FLASH_Program(TYPEPROGRAM_BYTE, CODE_BASE[index]+3, 0x00);	//Tag for file completed
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
				}
				else
				{
					if (parameters[0]==CODE_TAG1 && parameters[1]==CODE_TAG2)
					{
						parameters[3] = 0xff;	//Incompleted
						memcpy(&word, parameters, 4);
						HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index], word);
						memcpy(&word, parameters+4, 4);
						HAL_FLASH_Program(TYPEPROGRAM_WORD, CODE_BASE[index]+4, word);
						offset = 8;
					}
					else
						status[1] = ErrorParameter;
				}
				comm->SendFileData(CommandStatus, status, 2);
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
					dataLen = 32;
					data = boost::make_shared<uint8_t[]>(dataLen);
					for(auto i=0; i<4; ++i)
					{
						ptr = (uint8_t *)(CODE_BASE[parameters[0]]);
						memcpy(data.get()+i*8, ptr, 8);
					}
					comm->SendFileData(command, data.get(), dataLen);
				}
				else
				{
					ptr = (uint8_t *)(CODE_BASE[parameters[0]]);
					comm->SendFileData(command, ptr, 8);
				}
				break;
			case CommandUpdate:
				tid = osThreadCreate(UpdateThreadDef.get(), this);
				if (tid==NULL)
				{
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
