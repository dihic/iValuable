#ifndef _INDENPENDENT_UNIT_H
#define _INDENPENDENT_UNIT_H

#include "StorageUnit.h"

namespace IntelliStorage
{			
	class ScaleData
	{
		public:
			bool IsStable;
			std::uint8_t Status;
			std::int8_t TempQuantity;
			float Weight;
			
	};
	
	class IndependentUnit : public WeightBase<ScaleData>
	{
		public:
			IndependentUnit(CANExtended::CanEx &ex, std::uint16_t id, std::uint8_t sensorNum);
			virtual ~IndependentUnit() {}
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry) override;
	};
}

#endif
