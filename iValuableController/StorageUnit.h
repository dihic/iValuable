#ifndef _STORAGE_UNIT_H
#define _STORAGE_UNIT_H

#include <string>
#include "CanDevice.h"

namespace IntelliStorage
{
#define SYNC_DATA				0x0100
#define SYNC_GOTCHA			0x0180
#define SYNC_LIVE				0x01ff
	
	enum RfidType
	{
		RfidUnknown = 0,
		RfidIso15693 = 0x01,
		RfidIso14443A = 0x02,
		RfidIso14443B = 0x03,
	};

	enum RfidStatus
	{
		RfidNone = 0,
    RfidExpanded = 1,
    RfidAvailable = 2,
		RfidNA = 0xff
	};
	
	enum RfidAction
	{
		CardLeave,
		CardArrival,
	};
	
	class DeviceAttribute
	{
		private:
			DeviceAttribute() {}
		public:
			static const std::uint8_t OpLatch 	= 0x00;
			static const std::uint8_t OpRead 		= 0x00;
			static const std::uint8_t OpWrite 	= 0x01;
			static const std::uint8_t OpDel 		= 0x02;
		
			static const std::uint16_t Zero 								= 0x00fe;   //L
			static const std::uint16_t Ramp		 							= 0x8000;   //R
			static const std::uint16_t UnitInfo							= 0x8001;   //RW
			static const std::uint16_t CalWeight						= 0x8003;   //RW
			static const std::uint16_t RawData							= 0x8006;   //R
			static const std::uint16_t AutoRamp							= 0x8007;   //L
			static const std::uint16_t SensorEnable					= 0x8008;   //RW
			static const std::uint16_t Temperature					= 0x8009;   //W
			static const std::uint16_t InventoryQuantity		= 0x800A;   //RW
			static const std::uint16_t Notice 							= 0x9000;   //W
			static const std::uint16_t Door									= 0x9002;   //W
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

