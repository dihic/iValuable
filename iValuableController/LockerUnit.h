#ifndef _LOCKER_UNIT_H
#define _LOCKER_UNIT_H

#include "StorageUnit.h"

namespace IntelliStorage
{			
	class LockerUnit : public StorageUnit
	{
		public:
			LockerUnit(StorageBasic &basic);
			virtual ~LockerUnit() {}
	};
}

#endif
