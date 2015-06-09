#ifndef _DATA_PROCESSOR_H
#define _DATA_PROCESSOR_H

#include <cstdint>
#include "SensorArray.h"
#include "FastDelegate.h"
using namespace fastdelegate;

#define MEM_BUFSIZE 0x200

#define ADDR_SENSOR_ENABLE	0x00F
#define ADDR_CAL_WEIGHT			0x010
#define ADDR_TEMP						0x014

#if UNIT_TYPE!=UNIT_TYPE_INDEPENDENT
	#define ADDR_TARE_SUM				0x018
#endif

#define ADDR_CONFIG  				0x020
#define ADDR_SCALE  				0x040

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
	#define ADDR_RFID						0x090
#endif

#define ADDR_QUANTITY				0x0A0
#define ADDR_INFO						0x0D0

class DataProcessor
{
	private:
		std::int32_t currentAD[SENSOR_NUM];
		ScaleAttribute *pConfig;
		ScaleInfo *pScales[SENSOR_NUM];
		SuppliesInfo *pSupplies[SUPPLIES_NUM];
		std::int32_t *pQuantity[SUPPLIES_NUM];
		float *pCalWeight;
		float *pCurrentTemp;
		std::uint8_t *pSensorEnable;
		DataProcessor();
		static DataProcessor *Singleton;
#if UNIT_TYPE!=UNIT_TYPE_INDEPENDENT
		float *pTareSum;
#endif
#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		uint8_t *pCardId;
#endif
		volatile float boxWeight;
	public:
		typedef FastDelegate3<std::uint16_t, std::uint8_t *, std::uint16_t> WriteNVHandler;
		static WriteNVHandler WriteNV;
	
		static DataProcessor *InstancePtr() 
		{ 
			return (Singleton == 0)?(Singleton = new DataProcessor):Singleton; 
		}
		
		static void Release() 
		{ 
			if (Singleton)
			{
				delete Singleton;
				Singleton = 0;
			}
		}

		static __align(16) std::uint8_t MemBuffer[MEM_BUFSIZE];
		
		~DataProcessor();
		//Parameter flags stands for enable to calibrate sensors bitwise
		void CalibrateSensors(std::uint8_t flags);
		//
		float CalculateWeight(std::uint8_t ch, std::int32_t ad);
		float CalculateInventoryWeight(float &min, float &max);
		void SetRamp(std::uint8_t ch, float ramp);
		void SetZero(std::uint8_t ch, bool tare);
		bool SetQuantity(std::uint8_t index, std::int32_t num);
		bool SetQuantityById(std::uint64_t id, std::int32_t num);
		
		void SetConfig(const std::uint8_t *buf);
		void SetTemperature(float t);
		void SetCalWeight(const std::uint8_t *buf);
		void SetEnable(std::uint8_t en);
		
		bool SensorEnable(std::uint8_t ch) const;
		ScaleAttribute *GetConfig() { return pConfig; }
		float GetCalWeight() const {	return *pCalWeight; }
		float GetRamp(std::uint8_t ch) const;
		std::uint8_t GetEnable() const { return *pSensorEnable; }
		std::int32_t GetQuantity(std::uint8_t index) const { return *pQuantity[index]; }
#if UNIT_TYPE!=UNIT_TYPE_INDEPENDENT
		float GetTareWeight() const { return *pTareSum; }
#endif

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
		void SetBoxWeight(const std::uint8_t *w) { memcpy((void *)(&boxWeight), w, sizeof(float)); }
		const std::uint8_t *GetCardId() { return pCardId; }
		void SetCardId(const std::uint8_t *id);
#endif
		
		float GetBoxWeight() const { return boxWeight; }
		
		std::uint64_t GetSuppliesId(std::uint8_t index) const { return pSupplies[index]->Uid; }
		bool FindSuppliesId(std::uint64_t id, std::uint8_t &index) const;
		bool GetSuppliesUnit(std::uint8_t index, float &unit, float &deviation) const;
		void SetSupplies(std::uint8_t index, const SuppliesInfo &info);
		bool AddSupplies(const SuppliesInfo &info);
		void RemoveSupplies(std::uint8_t index);
		
		int PrepareRaw(std::uint8_t *buf);
		bool UpdateDisplay(bool force=false);
};

#endif
