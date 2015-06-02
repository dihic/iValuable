#ifndef _UNITY_UNIT_H
#define _UNITY_UNIT_H

#include "StorageUnit.h"

namespace IntelliStorage
{			
	class UnityUnit : public WeightBase<std::uint8_t>
	{
		protected:
			float totalWeight;
			UnityUnit(std::uint8_t typeCode, StorageBasic &basic);
		public:
			UnityUnit(StorageBasic &basic);
			virtual ~UnityUnit() {}
			float GetTotalWeight() const { return totalWeight; }
			virtual void ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry) override;
	};
}

#endif
