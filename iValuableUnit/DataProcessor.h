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
#define ADDR_CONFIG  				0x020
#define ADDR_SCALE  				0x040
#define ADDR_INFO						0x0C0

class DataProcessor
{
	private:
		std::int32_t currentAD[SENSOR_NUM];
		ScaleAttribute *pConfig;
		ScaleInfo *pScales[SENSOR_NUM];
		SuppliesInfo *pSupplies[10];
		float *pCalWeight;
		float *pCurrentTemp;
		std::uint8_t *pSensorEnable;
		DataProcessor();
		static DataProcessor *Singleton;
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
		
		~DataProcessor() {}
		//Parameter flags stands for enable to calibrate sensors bitwise
		void CalibrateSensors(std::uint8_t flags);
		float CalculateWeight(std::uint8_t ch, std::int32_t ad);
		void SetRamp(std::uint8_t ch, float ramp);
		void SetZero(std::uint8_t ch, bool tare);
		
		void SetConfig(const std::uint8_t *buf);
		void SetTemperature(float t);
		void SetCalWeight(const std::uint8_t *buf);
		void SetEnable(std::uint8_t en);
			
		bool SensorEnable(std::uint8_t ch) const;
		ScaleAttribute *GetConfig() { return pConfig; }
		float GetCalWeight() const;
		float GetRamp(std::uint8_t ch) const;
		std::uint8_t GetEnable() const { return *pSensorEnable; }
		
		int PrepareRaw(std::uint8_t *buf);
		
		
};

#endif
