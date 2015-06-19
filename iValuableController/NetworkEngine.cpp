#include "NetworkEngine.h"
#include "MemStream.h"
#include "Bson.h"
#include "System.h"
#include <ctime>
#include <cstring>

using namespace std;

namespace IntelliStorage
{	
	NetworkEngine::NetworkEngine(const std::uint8_t *endpoint, boost::scoped_ptr<UnitManager> &units)
		:tcp(endpoint),unitManager(units)
	{
		tcp.CommandArrivalEvent.bind(this, &NetworkEngine::TcpClientCommandArrival);
		unitManager->ReportRfidDataEvent.bind(this, &NetworkEngine::SendRfidData);
		unitManager->ReportDoorDataEvent.bind(this, &NetworkEngine::SendDoorData);
	}
	
	void NetworkEngine::SendHeartBeat()
	{
		static uint8_t HB[0x14]={0x14, 0x00, 0x00, 0x00, 0x12, 0x74, 0x69, 0x6d, 0x65, 0x73, 0x00,
														 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		static uint64_t t = 0;
		++t;
		memcpy(HB+0x0b, &t, 8);
		tcp.SendData(SerializableObjects::CodeHeartBeat, HB, 0x14);
	}
	
	void NetworkEngine::WhoAmI()
	{
		static const uint8_t ME[0x14]={0x14, 0x00, 0x00, 0x00, 0x12, 0x74, 0x69, 0x6d, 0x65, 0x73, 0x00,
																	 0x01, 0x03, 0x03, 0x01, 			// Fixed 0x01, iValuable 0x03, HV 0x03, 0x01 Pharmacy Products
																	 FW_VERSION_MAJOR, FW_VERSION_MINOR, 0x00, 0x00, 0x00};	//FW Version 1.00
		tcp.SendData(SerializableObjects::CodeWhoAmI, ME, 0x14);
	}
	
	void NetworkEngine::SendDoorData(uint8_t groupId, bool state)
	{
		boost::shared_ptr<SerializableObjects::DoorReport> report(new SerializableObjects::DoorReport);
		report->GroupIndex = groupId;
		report->GateIsOpen = state;
		size_t bufferSize = 0;
		auto buffer = BSON::Bson::Serialize(report, bufferSize);
		if (buffer!=nullptr && bufferSize>0)
			tcp.SendData(SerializableObjects::CodeReportDoor, buffer.get(), bufferSize);
	}
	
	void NetworkEngine::SendRfidData(boost::shared_ptr<RfidUnit> &unit)
	{
		if (unit == nullptr)
			return;
		boost::shared_ptr<SerializableObjects::RfidData> rfid(new SerializableObjects::RfidData);
		rfid->GroupIndex = unit->GroupId;
		rfid->NodeIndex = unit->NodeId;
		rfid->CardType = unit->GetCardState();
		rfid->CardId = unit->GetCardId();
		size_t bufferSize = 0;
		auto buffer = BSON::Bson::Serialize(rfid, bufferSize);
		unit->AfterUpdate();
		if (buffer!=nullptr && bufferSize>0)
			tcp.SendData(SerializableObjects::CodeReportRfid, buffer.get(), bufferSize);
	}
	
	void NetworkEngine::Process()
	{		
		TcpClient::TcpProcessor(&tcp);
	}
	
	void NetworkEngine::DeviceWriteResponse(CanDevice &device,std::uint16_t attr, bool result)
	{
		StorageUnit &unit = dynamic_cast<StorageUnit &>(device);
		CommandResponse(unit, static_cast<DeviceAttribute>(attr), true, result);
	}
	
	void NetworkEngine::CommandResponse(uint8_t groupId, uint8_t nodeId, SerializableObjects::CodeType code, bool result)
	{
		boost::shared_ptr<SerializableObjects::CommandResult> response(new SerializableObjects::CommandResult);
		response->GroupIndex = groupId;
		response->NodeIndex = nodeId;
		response->RequestDemandType = code;
		response->IsOk = result;
		size_t bufferSize = 0;
		auto buffer = BSON::Bson::Serialize(response, bufferSize);
		if (buffer!=nullptr && bufferSize>0)
			tcp.SendData(SerializableObjects::CodeCommandResult, buffer.get(), bufferSize);
	}

	void NetworkEngine::CommandResponse(StorageUnit &unit, DeviceAttribute attr, bool isWrite, bool result)
	{
		SerializableObjects::CodeType code;
		switch (attr)
		{
			case DeviceAttribute::Zero:
				if (!isWrite)
					return;
				code = SerializableObjects::CodeZero;
				break;
			case DeviceAttribute::Ramp:
				if (isWrite)
					return;
				code = SerializableObjects::CodeQueryRamp;
				break;
			case DeviceAttribute::InventoryInfo:
				if (isWrite)	//No need response here for writing,
					return;			//Bacause reponse already just after sending 
				code = SerializableObjects::CodeQueryInventoryInfo;
//				code = isWrite ? 
//					SerializableObjects::CodeSetInventoryInfo : SerializableObjects::CodeQueryInventoryInfo;
				break;
			case DeviceAttribute::CalWeight:
				if (!isWrite)
					return;
				code = SerializableObjects::CodeSetCalWeight;
				break;
			case DeviceAttribute::SensorConfig:
				if (!isWrite)
					return;
				code = SerializableObjects::CodeSetSensorConfig;
				break;
			case DeviceAttribute::RawData:
				if (isWrite)
					return;
				code = SerializableObjects::CodeQueryAD;
				break;
			case DeviceAttribute::AutoRamp:
				if (!isWrite)
					return;
				code = SerializableObjects::CodeCalibration;
				break;
			case DeviceAttribute::SensorEnable:
				code = isWrite ? 
					SerializableObjects::CodeSetSensorEnable : SerializableObjects::CodeQuerySensorEnable;
				break;
			case DeviceAttribute::InventoryQuantity:
				code = isWrite ? 
					SerializableObjects::CodeSetInventory : SerializableObjects::CodeQueryInventory;
				break;
			case DeviceAttribute::Locker:
				code = SerializableObjects::CodeLockControl;
				break;
			case DeviceAttribute::Notice:
				code = SerializableObjects::CodeGuide;
				break;
			default:
				return;
		}
		CommandResponse(unit.GroupId, unit.NodeId, code, result);
	}
	
	void NetworkEngine::DeviceReadResponse(CanDevice &device, uint16_t attr, const boost::shared_ptr<uint8_t[]> &data, size_t size)
	{		
		StorageUnit &unit = dynamic_cast<StorageUnit &>(device);
		if (data.get()==NULL || size==0)
		{
			CommandResponse(unit, static_cast<DeviceAttribute>(attr), false, false);
			return;
		}
		boost::shared_ptr<uint8_t[]> buffer;
		size_t bufferSize = 0;
		
		boost::shared_ptr<SerializableObjects::RampCollection> 				rampList;
		boost::shared_ptr<SerializableObjects::RampValue>							ramp;
		boost::shared_ptr<SerializableObjects::RawDataCollection> 		rawDataList;
		boost::shared_ptr<SerializableObjects::RawData>								rawData;
		boost::shared_ptr<SerializableObjects::SensorEnable>					sensorEnable;
		boost::shared_ptr<SerializableObjects::InventoryQuantity>			quantity;
		boost::shared_ptr<SerializableObjects::InventoryCollection>		quantityList;
		boost::shared_ptr<SerializableObjects::SuppliesItem>					suppliesItem;
		boost::shared_ptr<SerializableObjects::SuppliesCollection>		suppliesList;
		
		int i;
		const uint8_t *ptr;
		
		switch (attr)
		{
			case DeviceAttribute::Ramp:
				rampList.reset(new SerializableObjects::RampCollection);
				rampList->GroupIndex = unit.GroupId;
				rampList->NodeIndex = unit.NodeId;
				ptr = data.get()+1;
				for(i=0;i<data[0];++i)
				{
					ramp.reset(new SerializableObjects::RampValue);
					ramp->SensorIndex = ptr[0];
					memcpy(&ramp->Value, ptr+1, sizeof(float));
					rampList->OneGramIncrements.Add(ramp);
					ptr += sizeof(float)+1;
				}
				buffer = BSON::Bson::Serialize(rampList, bufferSize);
				if (buffer!=nullptr && bufferSize>0)
					tcp.SendData(SerializableObjects::CodeQueryRamp, buffer.get(), bufferSize);
				break;
			case DeviceAttribute::RawData:
				rawDataList.reset(new SerializableObjects::RawDataCollection);
				rawDataList->GroupIndex = unit.GroupId;
				rawDataList->NodeIndex = unit.NodeId;
				ptr = data.get()+1;
				for(i=0;i<data[0];++i)
				{
					rawData.reset(new SerializableObjects::RawData);
					rawData->SensorIndex = ptr[0];
					++ptr;
					memcpy(&rawData->ZeroAdCount, ptr, sizeof(int));
					ptr += sizeof(int);
					memcpy(&rawData->TareAdCount, ptr, sizeof(int));
					ptr += sizeof(int);
					memcpy(&rawData->CurrentAdCount, ptr, sizeof(int));
					ptr += sizeof(int);
					rawDataList->AdCountValues.Add(rawData);
				}
				buffer = BSON::Bson::Serialize(rawDataList, bufferSize);
				if (buffer!=nullptr && bufferSize>0)
					tcp.SendData(SerializableObjects::CodeQueryAD, buffer.get(), bufferSize);
				break;
			case DeviceAttribute::SensorEnable:
				sensorEnable.reset(new SerializableObjects::SensorEnable);
				sensorEnable->GroupIndex = unit.GroupId;
				sensorEnable->NodeIndex = unit.NodeId;
				sensorEnable->Flags = data[0];
				buffer = BSON::Bson::Serialize(sensorEnable, bufferSize);
				if (buffer!=nullptr && bufferSize>0)
					tcp.SendData(SerializableObjects::CodeQuerySensorEnable, buffer.get(), bufferSize);
				break;
			case DeviceAttribute::InventoryInfo:
				suppliesList.reset(new SerializableObjects::SuppliesCollection);
				suppliesList->GroupIndex = unit.GroupId;
				suppliesList->NodeIndex = unit.NodeId;
				ptr = data.get()+1;
				for(i=0; i<data[0]; ++i)
				{
					suppliesItem.reset(new SerializableObjects::SuppliesItem);
					suppliesItem->Index = ptr[0];
					memcpy(&suppliesItem->MaterialId, ++ptr, sizeof(uint64_t));
					ptr += sizeof(uint64_t);
					memcpy(&suppliesItem->UnitWeight, ptr, sizeof(float));
					ptr += sizeof(float);
					memcpy(&suppliesItem->ErrorRange, ptr, sizeof(float));
					ptr += sizeof(float);
					//Push dumps
					suppliesItem->MaterialName.push_back(0);
					suppliesItem->MaterialName.push_back(0);
					suppliesList->AllMaterialInfos.Add(suppliesItem);
				}
				buffer = BSON::Bson::Serialize(suppliesList, bufferSize);
				if (buffer!=nullptr && bufferSize>0)
					tcp.SendData(SerializableObjects::CodeQueryInventoryInfo, buffer.get(), bufferSize);
				break;
			case DeviceAttribute::InventoryQuantity:
				quantityList.reset(new SerializableObjects::InventoryCollection);
				quantityList->GroupIndex = unit.GroupId;
				quantityList->NodeIndex = unit.NodeId;
				ptr = data.get()+1;
				for(i=0;i<data[0];++i)
				{
					quantity.reset(new SerializableObjects::InventoryQuantity);
					memcpy(&quantity->MaterialId, ptr, sizeof(uint64_t));
					ptr += sizeof(uint64_t);
					quantity->Quantity = ptr[0];
					++ptr;
					quantityList->AllMaterialAmounts.Add(quantity);
				}
				buffer = BSON::Bson::Serialize(quantityList, bufferSize);
				if (buffer!=nullptr && bufferSize>0)
					tcp.SendData(SerializableObjects::CodeQueryInventory, buffer.get(), bufferSize);
				break;
			default:
				break;
		}
	}
	

//		CodeSetInventoryInfo		= 0x0C,
//		CodeSetInventory				= 0x0E,
//		CodeQueryInventory			= 0xF2,
	
	void NetworkEngine::TcpClientCommandArrival(boost::shared_ptr<std::uint8_t[]> payload, std::size_t size)
	{
		const SerializableObjects::CodeType code = static_cast<SerializableObjects::CodeType>(payload[0]);
		
		boost::shared_ptr<uint8_t[]> buffer;
		size_t bufferSize = 0;
		
		boost::shared_ptr<SerializableObjects::RampCollection> 				rampList;
		boost::shared_ptr<SerializableObjects::RampValue>							ramp;
		boost::shared_ptr<SerializableObjects::RawDataCollection> 		rawDataList;
		boost::shared_ptr<SerializableObjects::RawData>								rawData;
		boost::shared_ptr<SerializableObjects::SensorEnable>					sensorEnable;
		boost::shared_ptr<SerializableObjects::InventoryQuantity>			quantity;
		boost::shared_ptr<SerializableObjects::InventoryCollection>		quantityList;
		boost::shared_ptr<SerializableObjects::SuppliesItem>					suppliesItem;
		boost::shared_ptr<SerializableObjects::SuppliesCollection>		suppliesList;
		boost::shared_ptr<SerializableObjects::CalWeight>							calWeight;
		boost::shared_ptr<SerializableObjects::DeviceDesc>						deviceDesc;
		boost::shared_ptr<SerializableObjects::ScaleAttribute>				scaleAttr;
		boost::shared_ptr<SerializableObjects::UnitEntryCollection>   dataCollection;
		boost::shared_ptr<SerializableObjects::DirectGuide>						directGuide;

		boost::shared_ptr<MemStream> stream(new MemStream(payload, size, 1));
    
		bool open;
		
		switch (code)
		{
		case SerializableObjects::CodeZero:
		case SerializableObjects::CodeTare:
		case SerializableObjects::CodeCalibration:
		case SerializableObjects::CodeSetSensorEnable:
			BSON::Bson::Deserialize(stream, sensorEnable);
			if (sensorEnable != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(sensorEnable->GroupIndex, sensorEnable->NodeIndex));
				if (unit != nullptr)
				{
					if (code == SerializableObjects::CodeZero)
						unit->SetZero(sensorEnable->Flags, false);
					else if (code == SerializableObjects::CodeTare)
						unit->SetZero(0xff, true);
					else if (code == SerializableObjects::CodeCalibration)
						unit->SetAutoRamp(sensorEnable->Flags);
					else if (code == SerializableObjects::CodeSetSensorEnable)
						unit->SetSensorEnable(sensorEnable->Flags);
				}
				else
					CommandResponse(sensorEnable->GroupIndex, sensorEnable->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeSetCalWeight:
			BSON::Bson::Deserialize(stream, calWeight);
			if (calWeight != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(calWeight->GroupIndex, calWeight->NodeIndex));
				if (unit != nullptr)
					unit->SetCalWeight(calWeight->Weight);
				else
					CommandResponse(calWeight->GroupIndex, calWeight->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeQueryAD:
		case SerializableObjects::CodeQueryRamp:
		case SerializableObjects::CodeQueryConfig:
		case SerializableObjects::CodeQuerySensorEnable:
		case SerializableObjects::CodeQueryInventoryInfo:
		case SerializableObjects::CodeQueryInventory:
			BSON::Bson::Deserialize(stream, deviceDesc);
			if (deviceDesc != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(deviceDesc->GroupIndex, deviceDesc->NodeIndex));
				if (unit != nullptr)
				{
					if (code == SerializableObjects::CodeQueryAD)
						unit->RequestRawData();
					else if (code == SerializableObjects::CodeQueryRamp)
						unit->RequestRamps();
					else if (code == SerializableObjects::CodeQueryConfig)
						unit->RequestSensorConfig();
					else if (code == SerializableObjects::CodeQuerySensorEnable)
						unit->RequestSensorEnable();
					else if (code == SerializableObjects::CodeQueryInventoryInfo)
						unit->RequestInventoryInfos();
					else if (code == SerializableObjects::CodeQueryInventory)
						unit->RequestInventoryQuantities();
				}
				else
					CommandResponse(deviceDesc->GroupIndex, deviceDesc->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		
		case SerializableObjects::CodeSetSensorConfig:
			BSON::Bson::Deserialize(stream, scaleAttr);
			if (scaleAttr != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(scaleAttr->GroupIndex, scaleAttr->NodeIndex));
				if (unit != nullptr)
					unit->SetSensorConfig(scaleAttr);
				else
					CommandResponse(scaleAttr->GroupIndex, scaleAttr->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeQueryState:
			dataCollection = unitManager->PrepareData();
			buffer = BSON::Bson::Serialize(dataCollection, bufferSize);
			if (buffer!=nullptr && bufferSize>0)
				tcp.SendData(code, buffer.get(), bufferSize);
			break;
		case SerializableObjects::CodeSetInventoryInfo:
			BSON::Bson::Deserialize(stream, suppliesList);
			if (suppliesList != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(suppliesList->GroupIndex, suppliesList->NodeIndex));
				if (unit == nullptr)
				{
					CommandResponse(suppliesList->GroupIndex, suppliesList->NodeIndex, code, false);
					break;
				}
				if (suppliesList->AllMaterialInfos.Count() == 0) //Clear
					unit->ClearAllInventoryInfo();
				else		//Set or modify
				{
					for (auto i=0; i<suppliesList->AllMaterialInfos.Count(); ++i)
						unit->SetInventoryInfo(suppliesList->AllMaterialInfos[i]);
					//Response once for all
					CommandResponse(suppliesList->GroupIndex, suppliesList->NodeIndex, code, true);	
				}
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeSetInventory:
			BSON::Bson::Deserialize(stream, quantityList);
			if (suppliesList != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(quantityList->GroupIndex, quantityList->NodeIndex));
				if (unit == nullptr || quantityList->AllMaterialAmounts.Count() == 0)
				{
					CommandResponse(quantityList->GroupIndex, quantityList->NodeIndex, code, false);
					break;
				}
				unit->SetInventoryQuantities(quantityList->AllMaterialAmounts);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeLockControl:
			BSON::Bson::Deserialize(stream, deviceDesc);
			if (deviceDesc != nullptr)
			{
				if (unitManager->Unlock(deviceDesc->GroupIndex, open))
				{	
					if (open)
						CommandResponse(deviceDesc->GroupIndex, deviceDesc->NodeIndex, code, false);
				}
				else
					CommandResponse(deviceDesc->GroupIndex, deviceDesc->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeGuide:
			BSON::Bson::Deserialize(stream, directGuide);
			if (directGuide != nullptr)
			{
				auto unit = unitManager->FindUnit(StorageUnit::GetId(directGuide->GroupIndex, directGuide->NodeIndex));
				if (unit != nullptr)
				{
					unit->SetNotice(directGuide->IsGuide? 1 : 0);
					if (directGuide->IsGuide)
						unitManager->Unlock(directGuide->GroupIndex, open);
				}
				else
					CommandResponse(directGuide->GroupIndex, directGuide->NodeIndex, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeWhoAmI:
			WhoAmI();
			break;
		default:
			break;
		}
	}
}

