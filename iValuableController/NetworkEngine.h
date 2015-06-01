#ifndef _NETWORK_ENGINE_H
#define _NETWORK_ENGINE_H

#include "TcpClient.h"
#include "CommStructures.h"
#include "UnitManager.h"
#include "StorageUnit.h"
#include "RfidUnit.h"
#include <boost/type_traits.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>

namespace IntelliStorage
{
	class NetworkEngine
	{
		private:
			TcpClient tcp;
			boost::scoped_ptr<UnitManager> &unitManager;
			void CommandResponse(StorageUnit &unit, SerializableObjects::CodeType code, bool result);
			void CommandResponse(StorageUnit &unit, DeviceAttribute attr, bool isWrite, bool result);
			void TcpClientCommandArrival(boost::shared_ptr<std::uint8_t[]> payload, std::size_t size);
			void WhoAmI();
			void SendRfidData(boost::shared_ptr<RfidUnit> &unit);
			void SendDoorData(uint8_t groupId, bool state);
		public:

			NetworkEngine(const std::uint8_t *endpoint, boost::scoped_ptr<UnitManager> &units);
			~NetworkEngine() {}
			void SendHeartBeat();
			void Process();
			bool IsConnected() const { return tcp.IsConnected(); }
			void ChangeServiceEndpoint(const std::uint8_t *endpoint)
			{
				tcp.ChangeServiceEndpoint(endpoint);
			}
			
			void DeviceReadResponse(CanDevice &device, std::uint16_t attr, const boost::shared_ptr<std::uint8_t[]> &data, std::size_t size);
			void DeviceWriteResponse(CanDevice &device, std::uint16_t attr, bool result);
	};
}

#endif
