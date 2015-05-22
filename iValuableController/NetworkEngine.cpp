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
		tcp.SendData(HeartBeatCode, HB, 0x14);
	}
	
	void NetworkEngine::WhoAmI()
	{
		static const uint8_t ME[0x14]={0x14, 0x00, 0x00, 0x00, 0x12, 0x74, 0x69, 0x6d, 0x65, 0x73, 0x00,
																	 0x01, 0x03, 0x03, 0x01, 			// Fixed 0x01, iValuable 0x03, HV 0x03, 0x01 Pharmacy Products
																	 0x00, 0x00, 0x00, 0x00, 0x00};
		tcp.SendData(HeartBeatCode, ME, 0x14);
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
//		boost::shared_ptr<CommandResult> response(new CommandResult);
//		response->NodeId = device.DeviceId;
//		response->Result = result;
//		
//		switch (attr)
//		{
//			case DeviceAttribute::Notice:
//				response->Command = RfidDataCode;
//				break;
//			default:
//				return;
//		}
//		
//		boost::shared_ptr<uint8_t[]> buffer;
//		size_t bufferSize = 0;
//		buffer = BSON::Bson::Serialize(response, bufferSize);
//		if (buffer.get()!=NULL && bufferSize>0)
//			tcp.SendData(CommandResponse, buffer.get(), bufferSize);
	}
	
	void NetworkEngine::DeviceReadResponse(CanDevice &device, std::uint16_t attr, const boost::shared_ptr<std::uint8_t[]> &data, std::size_t size)
	{
//		boost::shared_ptr<CommandResult> response;
//		boost::shared_ptr<uint8_t[]> buffer;
//		size_t bufferSize = 0;
//		
//		if (data.get()==NULL || size==0)
//		{
//			response.reset(new CommandResult);
//			response->NodeId = device.DeviceId;
//			response->Result = false;
//			switch (attr)
//			{
//				default:
//					return;
//			}
//			buffer = BSON::Bson::Serialize(response, bufferSize);
//			if (buffer.get()!=NULL && bufferSize>0)
//				tcp.SendData(CommandResponse, buffer.get(), bufferSize);
//			return;
//		}
		
//		switch (attr)
//		{
//			default:
//				break;
//		}
	}
	
	void NetworkEngine::TcpClientCommandArrival(boost::shared_ptr<std::uint8_t[]> payload, std::size_t size)
	{
//		CodeType code = (CodeType)payload[0];
//		
//		boost::shared_ptr<uint8_t[]> buffer;
//		size_t bufferSize = 0;
		
//		boost::shared_ptr<NodeQuery> 								nodeQuery;
//		boost::shared_ptr<NodeList> 								nodeList;
//		boost::shared_ptr<CommandResult>  					response;
//		boost::shared_ptr<RfidDataBson> 						rfidBson;

//		map<uint16_t, boost::shared_ptr<StorageUnit> >::iterator it;
//		
//		boost::shared_ptr<MemStream> stream(new MemStream(payload, size, 1));
//		
//		
//		int id;
//		bool found = false;
//		switch (code)
//		{
//			case QueryNodeId:
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

