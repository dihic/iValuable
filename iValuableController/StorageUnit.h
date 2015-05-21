#ifndef _STORAGE_UNIT_H
#define _STORAGE_UNIT_H

#include <string>
#include "Guid.h"
#include "CanDevice.h"

namespace IntelliStorage
{
#define SYNC_DATA				0x0100
#define SYNC_GOTCHA			0x0180
#define SYNC_LIVE				0x01ff
	
	
	class DeviceAttribute
	{
		private:
			DeviceAttribute() {}
		public:
			static const std::uint16_t RawData 				= 0x8006;   //R
			static const std::uint16_t Notice 				= 0x9000;   //W
	};
	
	class StorageUnit : public CanDevice
	{
		private:
			static string GenerateId(const uint8_t *id, size_t len);
			uint8_t lastCardType;
			volatile bool cardChanged;
			volatile std::uint8_t cardState;
			std::string cardId;
			std::string presId;
		public:
			static const std::uint8_t CardArrival = 0x80;
			static const std::uint8_t CardLeft    = 0x81;
		
			StorageUnit(CANExtended::CanEx &ex, std::uint16_t id);
			virtual ~StorageUnit() {}
			
			void SetNotice(uint8_t level);
			
			void RequestRawData()
			{
				ReadAttribute(DeviceAttribute::RawData);
			}
			
			bool CardChanged() const { return cardChanged; }
			bool IsEmpty() const { return (cardState == CardLeft) || presId.empty(); }
			
			void UpdateCard();
			
			uint8_t GetCardState() const { return cardState; }
			std::string &GetPresId() { return presId; }
			std::string &GetCardId() { return cardId; }
			
			void RequestData()
			{
				canex.Sync(DeviceId, SYNC_DATA, CANExtended::Trigger);
			}
			
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> entry);
	};
}

#endif

