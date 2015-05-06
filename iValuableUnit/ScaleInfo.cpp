#include "ScaleInfo.h"
#include "SensorArray.h"
#include "FastDelegate.h"
#include "delay.h"


using namespace std; 
using namespace fastdelegate;

ScaleInfo::WriteNVHandler ScaleInfo::WriteNV = NULL;

bool ScaleInfo::Calibrate(float w)
{
	if (w<=0)
		return false;
	SensorArray &sensors = SensorArray::Instance();
	while(!sensors.IsStable(channel))
		DELAY(10000);	//Wait for 10ms
	basic->Ramp = (sensors.GetCurrent(channel) - basic->Tare)/w;
	if (WriteNV)
		WriteNV(base, (uint8_t *)basic, sizeof(ScaleBasic));
	return true;
}

int32_t ScaleInfo::SetZero()
{
	SensorArray &sensors = SensorArray::Instance();
	while(!sensors.IsStable(channel))
		DELAY(10000);	//Wait for 10ms
	basic->Zero = sensors.GetCurrent(channel);
	sensors.UpdateStateWithZero(channel, basic->Tare = basic->Zero);
	if (WriteNV)
		WriteNV(base, (uint8_t *)basic, sizeof(ScaleBasic));
	return basic->Zero;
}

int32_t ScaleInfo::SetTare()
{
	SensorArray &sensors = SensorArray::Instance();
	while(!sensors.IsStable(channel))
		DELAY(10000);	//Wait for 10ms
	basic->Tare = sensors.GetCurrent(channel);
	if (WriteNV)
		WriteNV(base, (uint8_t *)basic, sizeof(ScaleBasic));
	return basic->Tare;
}

void ScaleInfo::SetRamp(float ramp)
{
	basic->Ramp = ramp;
	if (WriteNV)
		WriteNV(base, (uint8_t *)basic, sizeof(ScaleBasic));
}

void ScaleInfo::TemperatureCompensation(float coeff, float delta)
{
	if (ABS(delta)<0.01f)
		return;
	float d = SensorArray::Instance().GetFullRange()*coeff*delta;
	basic->Zero += d;
	basic->Tare += d;
	if (WriteNV)
		WriteNV(base, (uint8_t *)basic, sizeof(ScaleBasic));
}

