#ifndef _RFID_PROCESSOR_H
#define _RFID_PROCESSOR_H

#define RFID_OFF_COUNT    3
#define RFID_TIME_INTERVAL 200

#include <cstdint>
#include "FastDelegate.h"

class RfidProcessor
{
	private:
		static volatile std::uint8_t RfidCardType;
		static uint8_t UIDlast[8];
		static void AfterRfidError();
		static bool ReadSingleBlock(std::uint8_t index, std::uint8_t *data);
	public:
		typedef fastdelegate::FastDelegate2<std::uint8_t, const std::uint8_t *> RfidChangedHandler;
		static RfidChangedHandler RfidChangedEvent;
		static void UpdateRfid();
		static std::uint8_t GetCardType() { return RfidCardType; }
};

#endif

