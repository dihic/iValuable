#ifndef _COMM_STRUCTURES_H
#define _COMM_STRUCTURES_H

#include "TypeInfoBase.h"
#include <cstdint>

namespace SerializableObjects
{
	enum CodeType
	{
		CodeZero						 	= 0x00,
		CodeSetCalWeight			= 0x01,
		CodeCalibration 			= 0x02,
		CodeQueryAD 					= 0x03,
		CodeQueryRamp 				= 0x04,
		CodeSetSensorEnable 	= 0x05,
		CodeSetSensorConfig 	= 0x06,
		CodeTare 							= 0x07,
		CodeHeartBeat 				= 0x08,
		CodeReportRfid				= 0x09,
		CodeReportDoor				= 0x0A,
		CodeQueryState				= 0x0B,
		CodeSetInventoryInfo		= 0x0C,
		CodeQueryInventoryInfo	= 0x0D,
		CodeSetInventory				= 0x0E,
		CodeLockControl					= 0x0F,
		CodeGuide								= 0x10,
		
		CodeQueryConfig					= 0xF0,
		CodeQuerySensorEnable   = 0xF1,
		CodeQueryInventory			= 0xF2,
		CodeWhoAmI							= 0xFD,	
		CodeCommandResult				= 0xFE,
		CodeError								= 0xFF,
	};

	DECLARE_CLASS(HeartBeat)
	{
		public:
			HeartBeat()
			{
				REGISTER_FIELD(Times);
			}
			virtual ~HeartBeat() {}
			std::uint64_t Times;
	};
	
	DECLARE_CLASS(DeviceDesc)
	{
		public:
			DeviceDesc()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
			}
			virtual ~DeviceDesc() {}
			int GroupIndex;
			int NodeIndex;
	};
	
	DECLARE_CLASS(CalWeight)
	{
		public:
			CalWeight()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(Weight);
			}
			virtual ~CalWeight() {}
			int GroupIndex;
			int NodeIndex;
			float Weight;
	};
	
	DECLARE_CLASS(SensorEnable)
	{
		public:
			SensorEnable()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(Flags);
			}
			virtual ~SensorEnable() {}
			int GroupIndex;
			int NodeIndex;
			int Flags;
	};
	
	DECLARE_CLASS(RawData)
	{
		public:
			RawData()
			{
				REGISTER_FIELD(SensorIndex);
				REGISTER_FIELD(ZeroAdCount);
				REGISTER_FIELD(TareAdCount);
				REGISTER_FIELD(CurrentAdCount);
			}
			virtual ~RawData() {}
			int SensorIndex;
			int ZeroAdCount;
			int TareAdCount;
			int CurrentAdCount;
	};
	
	DECLARE_CLASS(RawDataCollection)
	{
		public:
			RawDataCollection()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(AdCountValues);
			}
			virtual ~RawDataCollection() {}
			int GroupIndex;
			int NodeIndex;
			Array<RawData> AdCountValues;
	};
	
	DECLARE_CLASS(RampValue)
	{
		public:
			RampValue()
			{
				REGISTER_FIELD(SensorIndex);
				REGISTER_FIELD(Value);
			}
			virtual ~RampValue() {}
			int SensorIndex;
			float Value;
	};
	
	DECLARE_CLASS(RampCollection)
	{
		public:
			RampCollection()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(OneGramIncrements);
			}
			virtual ~RampCollection() {}
			int GroupIndex;
			int NodeIndex;
			Array<RampValue> OneGramIncrements;
	};
	
	DECLARE_CLASS(InventoryQuantity)
	{
		public:
			InventoryQuantity()
			{
				REGISTER_FIELD(MaterialId);
				REGISTER_FIELD(Quantity);
			}
			virtual ~InventoryQuantity() {}
			std::uint64_t MaterialId;
			int Quantity;
	};
	
	DECLARE_CLASS(InventoryCollection)
	{
		public:
			InventoryCollection()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(AllMaterialAmounts);
			}
			virtual ~InventoryCollection() {}
			int GroupIndex;
			int NodeIndex;
			Array<InventoryQuantity> AllMaterialAmounts;
	};
	
	DECLARE_CLASS(ScaleState)
	{
		public:
			ScaleState()
			{
				REGISTER_FIELD(SensorIndex);
				REGISTER_FIELD(Weight);
				REGISTER_FIELD(WeightState);
				REGISTER_FIELD(IsDamaged);
				REGISTER_FIELD(IsStable);
			}
			virtual ~ScaleState() {}
			int SensorIndex;
			float Weight;
			int WeightState;
			bool IsDamaged;
			bool IsStable;
	};
	
	DECLARE_CLASS(UnitEntry)
	{
		public:
			UnitEntry()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(Weight);
				REGISTER_FIELD(IsStable);
				REGISTER_FIELD(SensorStates);
			}
			virtual ~UnitEntry() {}
			int GroupIndex;
			int NodeIndex;
			float Weight;
			bool IsStable;
			Array<ScaleState> SensorStates;
	};
	
	DECLARE_CLASS(UnitEntryCollection)
	{
		public:
			UnitEntryCollection()
			{
				REGISTER_FIELD(AllUnits);
			}
			virtual ~UnitEntryCollection() {}
			Array<UnitEntry> AllUnits;
	};

	DECLARE_CLASS(RfidDataBson)
	{
		public:
			RfidDataBson()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(CardType);
				REGISTER_FIELD(CardId);
			}
			virtual ~RfidDataBson() {}
			int GroupIndex;
			int NodeIndex;
			int CardType;
			std::string CardId;
	};
	
	DECLARE_CLASS(CommandResult)
	{
		public:
			CommandResult()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(RequestDemandType);
				REGISTER_FIELD(IsOk);
			}
			virtual ~CommandResult() {}
			int GroupIndex;
			int NodeIndex;
			int RequestDemandType;
			bool IsOk;
	};
	
	DECLARE_CLASS(ScaleAttribute)
	{
		public:
			ScaleAttribute()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(Sensitivity);
				REGISTER_FIELD(TempDrift);
				REGISTER_FIELD(ZeroRange);
				REGISTER_FIELD(SafeOverload);
				REGISTER_FIELD(MaxOverload);
			}
			virtual ~ScaleAttribute() {}
			int GroupIndex;
			int NodeIndex;
			float Sensitivity;
			float TempDrift;
			float ZeroRange;
			float SafeOverload;
			float MaxOverload;
	};
	
	DECLARE_CLASS(SuppliesItem)
	{
		public:
			SuppliesItem()
			{
				REGISTER_FIELD(Index);
				REGISTER_FIELD(MaterialId);
				REGISTER_FIELD(UnitWeight);
				REGISTER_FIELD(ErrorRange);
				REGISTER_FIELD(MaterialName);
			}
			virtual ~SuppliesItem() {}

			int Index;
			std::uint64_t MaterialId;
			float UnitWeight;
			float ErrorRange;
			Binary MaterialName;
	};
	
	DECLARE_CLASS(SuppliesCollection)
	{
		public:
			SuppliesCollection()
			{
				REGISTER_FIELD(GroupIndex);
				REGISTER_FIELD(NodeIndex);
				REGISTER_FIELD(AllMaterialInfos);
			}
			virtual ~SuppliesCollection() {}
			int GroupIndex;
			int NodeIndex;
			Array<SuppliesItem> AllMaterialInfos;
	};
	
	class CommStructures
	{
		public:
			static void Register();
	};
}
#endif

