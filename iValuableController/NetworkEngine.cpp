#include "NetworkEngine.h"
#include "MemStream.h"
#include "Bson.h"
#include <ctime>
#include <cstring>

using namespace std;

namespace IntelliStorage
{	
	NetworkEngine::NetworkEngine(const std::uint8_t *endpoint, boost::scoped_ptr<UnitManager> &units)
		:tcp(endpoint),unitManager(units)
	{
		tcp.CommandArrivalEvent.bind(this, &NetworkEngine::TcpClientCommandArrival);
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
																	 0x01, 0x00, 0x00, 0x00, 0x00};	//FW Version 1.00
		tcp.SendData(SerializableObjects::CodeWhoAmI, ME, 0x14);
	}
	
//	void NetworkEngine::InventoryRfid()
//	{
//		static bool needReport = true;
//		if (!tcp.IsConnected())
//		{
//			needReport = true;
//			return;
//		}
//		
//		for (map<uint16_t, boost::shared_ptr<StorageUnit> >::iterator it = unitList.begin(); 
//				 it!= unitList.end(); ++it)
//		{
//			if (needReport)
//			{
//				if (it->second->GetCardState() != StorageUnit::CardArrival)
//					continue;
//			}
//			else if (!it->second->CardChanged())
//				continue;
//			SendRfidData(it->second);
//		}
//		needReport = false;
//	}
//	
//	void NetworkEngine::SendRfidData(boost::shared_ptr<StorageUnit> unit)
//	{
//		if (unit.get() == NULL)
//			return;
//		size_t bufferSize = 0;
//		boost::shared_ptr<RfidDataBson> rfidBson(new RfidDataBson);
//		rfidBson->NodeId = unit->DeviceId;
//		rfidBson->State = unit->GetCardState();
//		rfidBson->PresId = unit->GetPresId();
//		rfidBson->CardId = unit->GetCardId();
//		boost::shared_ptr<uint8_t[]> buffer = BSON::Bson::Serialize(rfidBson, bufferSize);
//		unit->UpdateCard();
//		if (buffer.get()!=NULL && bufferSize>0)
//			tcp.SendData(RfidDataCode, buffer.get(), bufferSize);
//	}
	
	void NetworkEngine::Process()
	{		
		TcpClient::TcpProcessor(&tcp);
	}
	
	void NetworkEngine::DeviceWriteResponse(CanDevice &device,std::uint16_t attr, bool result)
	{
		StorageUnit &unit = dynamic_cast<StorageUnit &>(device);
		CommandResponse(unit, static_cast<DeviceAttribute>(attr), true, result);
	}
	
	void NetworkEngine::CommandResponse(StorageUnit &unit, SerializableObjects::CodeType code, bool result)
	{
		boost::shared_ptr<SerializableObjects::CommandResult> response(new SerializableObjects::CommandResult);
		response->GroupIndex = unit.GroupId;
		response->NodeIndex = unit.NodeId;
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
				code = isWrite ? 
					SerializableObjects::CodeSetInventoryInfo : SerializableObjects::CodeQueryInventoryInfo;
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
			default:
				return;
		}
		CommandResponse(unit, code, result);
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
	

//		CodeQueryState				= 0x0B,
//		CodeSetInventoryInfo		= 0x0C,
//		CodeQueryInventoryInfo	= 0x0D,
//		CodeSetInventory				= 0x0E,
//		CodeLockControl					= 0x0F,
//		CodeGuide								= 0x10,
//		
//		CodeQueryConfig					= 0xF0,
//		CodeQuerySensorEnable   = 0xF1,
//		CodeQueryInventory			= 0xF2,
//		CodeCommandResult				= 0xFE,
//		CodeWhoAmI							= 0xFF,	
	
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
//		map<uint16_t, boost::shared_ptr<StorageUnit> >::iterator it;
//		
		boost::shared_ptr<MemStream> stream(new MemStream(payload, size, 1));
//		
//		
//		int id;
//		bool found = false;
		switch (code)
		{
		case SerializableObjects::CodeZero:
		case SerializableObjects::CodeTare:
		case SerializableObjects::CodeCalibration:
		case SerializableObjects::CodeSetSensorEnable:
			BSON::Bson::Deserialize(stream, sensorEnable);
			if (sensorEnable != nullptr)
			{
				auto unit = boost::dynamic_pointer_cast<StorageUnit>(
					unitManager->FindUnit(StorageUnit::GetId(sensorEnable->GroupIndex, sensorEnable->NodeIndex)));
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
					CommandResponse(*unit, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeSetCalWeight:
			BSON::Bson::Deserialize(stream, calWeight);
			if (calWeight != nullptr)
			{
				auto unit = boost::dynamic_pointer_cast<StorageUnit>(
					unitManager->FindUnit(StorageUnit::GetId(calWeight->GroupIndex, calWeight->NodeIndex)));
				if (unit != nullptr)
					unit->SetCalWeight(calWeight->Weight);
				else
					CommandResponse(*unit, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeQueryAD:
		case SerializableObjects::CodeQueryRamp:
			BSON::Bson::Deserialize(stream, deviceDesc);
			if (sensorEnable != nullptr)
			{
				auto unit = boost::dynamic_pointer_cast<StorageUnit>(
					unitManager->FindUnit(StorageUnit::GetId(deviceDesc->GroupIndex, deviceDesc->NodeIndex)));
				if (unit != nullptr)
				{
					if (code == SerializableObjects::CodeQueryAD)
						unit->RequestRawData();
					else if (code == SerializableObjects::CodeQueryRamp)
						unit->RequestRamps();
				}
				else
					CommandResponse(*unit, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		
		case SerializableObjects::CodeSetSensorConfig:
			BSON::Bson::Deserialize(stream, scaleAttr);
			if (sensorEnable != nullptr)
			{
				auto unit = boost::dynamic_pointer_cast<StorageUnit>(
					unitManager->FindUnit(StorageUnit::GetId(scaleAttr->GroupIndex, scaleAttr->NodeIndex)));
				if (unit != nullptr)
					unit->SetSensorConfig(scaleAttr);
				else
					CommandResponse(*unit, code, false);
			}
			else
				tcp.SendData(SerializableObjects::CodeError, nullptr, 0);
			break;
		case SerializableObjects::CodeQueryState:
			dataCollection = unitManager->PrepareData();
			buffer = BSON::Bson::Serialize(dataCollection, bufferSize);
			if (buffer!=nullptr && bufferSize>0)
				tcp.SendData(SerializableObjects::CodeQueryInventory, buffer.get(), bufferSize);
			break;
		default:
			break;
		}
//				nodeList.reset(new NodeList);
//				for(it = unitList.begin(); it != unitList.end(); ++it)
//				{
//					id = it->first;
//					nodeList->NodeIds.Add(id);
//				}
//				buffer = BSON::Bson::Serialize(nodeList, bufferSize);
//				if (buffer.get()!=NULL && bufferSize>0)
//					tcp.SendData(code, buffer.get(), bufferSize);
//				
////				for (it = unitList.begin(); it!= unitList.end(); ++it)
////				{
////					if (it->second->GetCardState() != StorageUnit::CardArrival)
////						continue;
////					SendRfidData(it->second);
////				}
//				break;
//			case QueryRfid:
//				BSON::Bson::Deserialize(stream, nodeQuery);
//				if (nodeQuery.get()!=NULL)
//				{
//					it = unitList.find(nodeQuery->NodeId);
//					if (it != unitList.end())
//						SendRfidData(it->second);
//				}
//			case WhoAmICode:
//				WhoAmI();
//				break;
//			case RfidDataCode:
//				BSON::Bson::Deserialize(stream, rfidBson);
//				if (rfidBson.get()!=NULL)
//				{
//					for(it = unitList.begin(); it != unitList.end(); ++it)
//						if (rfidBson->PresId.compare(it->second->GetPresId())==0)
//						{
//							found = true;
//							break;
//						}
//					response.reset(new CommandResult);
//					response->Command = RfidDataCode;
//					if (found)
//					{
//						it->second->SetNotice(rfidBson->State);
//						response->NodeId = it->first;
//						response->Result = true;
//					}
//					else
//					{
//						response->NodeId = 0;
//						response->Result = false;
//					}
//					buffer = BSON::Bson::Serialize(response, bufferSize);
//					if (buffer.get()!=NULL && bufferSize>0)
//						tcp.SendData(CommandResponse, buffer.get(), bufferSize);
//				}
//				break;
//			default:
//				break;
//		}
	}
}

