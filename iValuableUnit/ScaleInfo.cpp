#include "ScaleInfo.h"
#include "SensorArray.h"
#include "FastDelegate.h"
#include "delay.h"
#include <cmath>

using namespace std; 
using namespace fastdelegate;

bool ScaleInfo::Calibrate(float w)
{
	if (abs(w)<0.0001f)
		return false;
	SensorArray &sensors = SensorArray::Instance();
	int8_t tries = 10; 
	while(!sensors.IsStable(channel))
	{
		if (tries--<=0)
			return false;		//Timeout
		DELAY(10000);	//Wait for 10ms
	}
	basic->Ramp = (sensors.GetCurrent(channel) - basic->Tare)/w;
	return true;
}

int32_t ScaleInfo::SetZero(bool tare)
{
	SensorArray &sensors = SensorArray::Instance();
	int8_t tries = 10; 
	while(!sensors.IsStable(channel))
	{
		if (tries--<=0)
			return 0;		//Timeout
		DELAY(10000);	//Wait for 10ms
	}
	if (tare)
	{
		basic->Tare = sensors.GetCurrent(channel);
		return basic->Tare;
	}
	basic->Zero = sensors.GetCurrent(channel);
	sensors.UpdateStateWithZero(channel, basic->Tare = basic->Zero);
	return basic->Zero;
}

//int32_t ScaleInfo::SetTare()
//{
//	SensorArray &sensors = SensorArray::Instance();
//	while(!sensors.IsStable(channel))
//		DELAY(10000);	//Wait for 10ms
//	
//}

void ScaleInfo::SetRamp(float ramp)
{
	basic->Ramp = ramp;
}

void ScaleInfo::TemperatureCompensation(float coeff, float delta)
{
	if (abs(delta)<0.0001f)
		return;
	float d = SensorArray::Instance().GetFullRange()*coeff*delta;
	basic->Zero += d;
	basic->Tare += d;
}

