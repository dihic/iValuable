#include "CommStructures.h"

namespace SerializableObjects
{
	CLAIM_CLASS(HeartBeat);
	CLAIM_CLASS(SyetemInfo);
	CLAIM_CLASS(DeviceDesc);
	CLAIM_CLASS(CalWeight);
	CLAIM_CLASS(SensorEnable);
	CLAIM_CLASS(RawData);
	CLAIM_CLASS(RawDataCollection);
	CLAIM_CLASS(RampValue);
	CLAIM_CLASS(RampCollection);
	CLAIM_CLASS(ScaleState);
	CLAIM_CLASS(UnitEntry);
	CLAIM_CLASS(UnitEntryCollection);
	CLAIM_CLASS(RfidData);
	CLAIM_CLASS(CommandResult);
	
	CLAIM_CLASS(ScaleAttribute);
	CLAIM_CLASS(SuppliesItem);
	CLAIM_CLASS(SuppliesCollection);
	CLAIM_CLASS(InventoryQuantity);
	CLAIM_CLASS(InventoryCollection);
	CLAIM_CLASS(DoorReport);
	CLAIM_CLASS(DirectGuide);

	void CommStructures::Register()
	{
		REGISTER_CLASS(HeartBeat);
		REGISTER_CLASS(SyetemInfo);
		REGISTER_CLASS(DeviceDesc);
		REGISTER_CLASS(CalWeight);
		REGISTER_CLASS(SensorEnable);
		REGISTER_CLASS(RawData);
		REGISTER_CLASS(RawDataCollection);
		REGISTER_CLASS(RampValue);
		REGISTER_CLASS(RampCollection);
		REGISTER_CLASS(ScaleState);
		REGISTER_CLASS(UnitEntry);
		REGISTER_CLASS(UnitEntryCollection);
		REGISTER_CLASS(RfidData);
		REGISTER_CLASS(CommandResult);
		
		REGISTER_CLASS(ScaleAttribute);
		REGISTER_CLASS(SuppliesItem);
		REGISTER_CLASS(SuppliesCollection);
		REGISTER_CLASS(InventoryQuantity);
		REGISTER_CLASS(InventoryCollection);
		REGISTER_CLASS(DoorReport);
		REGISTER_CLASS(DirectGuide);
	}
}
	
