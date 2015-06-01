#ifndef _RFID_UNIT_H
#define _RFID_UNIT_H

#include <string>
#include "UnityUnit.h"

namespace IntelliStorage
{		
	enum RfidType : std::uint8_t
	{
		RfidNone = 0,
		RfidAvailable = 1,
    RfidExpanded = 2,
		RfidNA = 0xff
	};
	
	enum RfidStatus : std::uint8_t
	{
		CardArrival = 0x80,
		CardLeft = 0x81,
	};
	
	class RfidUnit : public UnityUnit
	{
		private:
			static string GenerateId(const uint8_t *id, size_t len);
			volatile RfidType lastCardType = RfidNone;
			volatile bool cardChanged = false;
			volatile RfidStatus cardState = CardLeft;
			std::string cardId;
		public:
			RfidUnit(StorageBasic &basic);
			virtual ~RfidUnit() {}
			bool CardChanged() const { return cardChanged; }
			bool IsEmpty() const { return (cardState == CardLeft); }
			RfidStatus GetCardState() const { return cardState; }
			std::string &GetCardId() { return cardId; }
			
			void AfterUpdate();
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry) override;
	};
}

#endif
