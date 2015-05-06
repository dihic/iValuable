#ifndef __SENSOR_ARRAY_H 
#define __SENSOR_ARRAY_H

#include "hx711.h"
#include "ScaleInfo.h"
#include "System.h"
#include "FastDelegate.h"

using namespace fastdelegate;

//#define SCALE_TEMP_DRIFT 		0.0002f
//#define SCALE_ZERO_RANGE 		0.1f
//#define SCALE_OW_RANGE 			1.2f
//#define SCALE_DAMAGE_RANGE	1.5f

#define SENSOR_NORMAL				0x00
#define SENSOR_OVERWEIGHT 	0x01
#define SENSOR_DAMAGE			 	0x02
#define SENSOR_FAULT				0x10

class SensorArray
{
	private:
		HX711 *pSensors[6];
		int32_t lastData[6];
		int32_t tempData[6];
		int32_t currentData[6];
		int8_t tempCount[6];
		int8_t count[6];
		bool stable[6];
		uint8_t state[6];
		float fullRange;
		float safeRange;
		float maxRange;
		float zeroLimit;
		void Update(uint8_t ch);
		SensorArray();
		static SensorArray *Singleton;
	public:
//		typedef FastDelegate2<std::uint8_t *, std::uint8_t> AckRecieverHandler;
//		static AckRecieverHandler OnAckReciever;
		static SensorArray &Instance() 
		{ 
			return *((Singleton == 0)?(Singleton = new SensorArray):Singleton); 
		}
		static SensorArray &Instance(ScaleAttribute *attr);
		
		static void Release() 
		{ 
			if (Singleton)
			{
				delete Singleton;
				Singleton = 0;
			}
		}
		~SensorArray();
		int32_t GetCurrent(uint8_t ch) const { return currentData[ch]; }
		float GetFullRange() const { return fullRange; }
		void SetRange(ScaleAttribute *attr);
		bool IsStable(uint8_t ch) const { return stable[ch]; }
		uint8_t GetStatus(uint8_t ch) const { return state[ch]; }
		void UpdateStateWithZero(uint8_t ch, int32_t zero);
		void Refresh();
};

#endif
