#include "SensorArray.h"
#include "gpio.h"
#include <cstring>

using namespace std;

#define LIMIT_SPEED 			(1<<7)
#define STABLE_WINDOW 		5
#define AVG_WINDOW 				4

SensorArray *SensorArray::Singleton = NULL;

SensorArray &SensorArray::Instance(ScaleAttribute *attr)
{
	SensorArray &instance = Instance();
	instance.SetRange(attr);
	return instance;
}

SensorArray::SensorArray()
	:fullRange(0), safeRange(0), maxRange(0), zeroLimit(0)
{
	pSensors[0] = new HX711(SENSOR1_SCK, SENSOR1_DOUT);
	pSensors[1] = new HX711(SENSOR2_SCK, SENSOR2_DOUT);
	pSensors[2] = new HX711(SENSOR3_SCK, SENSOR3_DOUT);
	pSensors[3] = new HX711(SENSOR4_SCK, SENSOR4_DOUT);
#if UNIT_TYPE!=UNIT_TYPE_UNITY_RFID
	pSensors[4] = new HX711(SENSOR5_SCK, SENSOR5_DOUT);
	pSensors[5] = new HX711(SENSOR6_SCK, SENSOR6_DOUT);
#endif
	memset(currentData, 0, SENSOR_NUM*sizeof(int32_t));
	memset(lastData, 0, SENSOR_NUM*sizeof(int32_t));
	memset(tempData, 0, SENSOR_NUM*sizeof(int32_t));
	memset(count, 0, SENSOR_NUM);
	memset(tempCount, 0, SENSOR_NUM);
	memset(stable, 0, SENSOR_NUM*sizeof(bool));
}

SensorArray::~SensorArray()
{
	delete pSensors[0];
	delete pSensors[1];
	delete pSensors[2];
	delete pSensors[3];
#if UNIT_TYPE!=UNIT_TYPE_UNITY_RFID
	delete pSensors[4];
	delete pSensors[5];	
#endif
}

void SensorArray::SetRange(ScaleAttribute *attr)
{
	float t = (1<<(PRECISION_DIGITS-1)) * HX711::RangeCoeff;
	fullRange =  t * attr->Sensitivity;
	zeroLimit = t * attr->ZeroRange;
	safeRange = fullRange*attr->SafeOverload;
	maxRange = fullRange*attr->MaxOverload;
}

void SensorArray::UpdateStateWithZero(uint8_t ch, int32_t zero)
{
	if (ABS(zero) > zeroLimit)
		state[ch] |= SENSOR_FAULT;
	else
		state[ch] &= 0x0f;
}

void SensorArray::Update(uint8_t ch)
{
	tempData[ch] += pSensors[ch]->ReadData(PRECISION_DIGITS);
	if (++tempCount[ch] > AVG_WINDOW)
	{
		//Filter with average value
		currentData[ch] = tempData[ch]/AVG_WINDOW;
		tempCount[ch] = 0;
		tempData[ch] = 0;
		
		//Update sensor state
		if (ABS(currentData[ch]) > safeRange)
		{
			if (ABS(currentData[ch]) > maxRange)
				state[ch] = (state[ch]&0xf0)|SENSOR_DAMAGE;
			else
				state[ch] = (state[ch]&0xf0)|SENSOR_OVERWEIGHT;
		}
		else
			state[ch] = (state[ch]&0xf0)|SENSOR_NORMAL;
		
		//Determine if sensor stable
		if (stable[ch])
		{
			if (ABS(lastData[ch]-currentData[ch]) > LIMIT_SPEED)
			{
				stable[ch] = false;
				count[ch] = 0;
			}
		}
		else
		{
			if (ABS(lastData[ch]-currentData[ch]) > LIMIT_SPEED)
				count[ch] = 0;
			else if (++count[ch] > STABLE_WINDOW)
				stable[ch] = true;
		}
		
		//Record latest AD value
		lastData[ch] = currentData[ch];
	}
}

void SensorArray::Refresh()
{
	if (GPIOIntStatus(SENSOR1_DOUT))
		Update(0);
	if (GPIOIntStatus(SENSOR2_DOUT))
		Update(1);
	if (GPIOIntStatus(SENSOR3_DOUT))
		Update(2);
	if (GPIOIntStatus(SENSOR4_DOUT))
		Update(3);
#if UNIT_TYPE!=UNIT_TYPE_UNITY_RFID
	if (GPIOIntStatus(SENSOR5_DOUT))
		Update(4);
	if (GPIOIntStatus(SENSOR6_DOUT))
		Update(5);
#endif
}
