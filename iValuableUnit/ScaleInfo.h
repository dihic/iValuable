#ifndef _SCALE_INFO_H
#define _SCALE_INFO_H

#include <cstdint>

#include "FastDelegate.h"
using namespace fastdelegate;

__packed struct ScaleAttribute
{
	float Sensitivity;
	float TempDrift;
	float ZeroRange;
	float SafeOverload;
	float MaxOverload;
};

__packed struct ScaleBasic
{
	float Ramp;
	std::int32_t Zero;
	std::int32_t Tare;
};

__packed struct SuppliesInfo
{
	std::uint64_t Uid;
	float Unit;
	float Deviation;
};

class ScaleInfo
{
	private:
		std::uint8_t channel;
		ScaleBasic *basic;
	public:
		ScaleInfo(std::uint8_t ch, ScaleBasic *sb)
			:channel(ch), basic(sb) {}
		~ScaleInfo() {}
		ScaleBasic *GetBasic() { return basic; }
		bool Calibrate(float w);
		int32_t SetZero(bool tare);
		void SetRamp(float ramp);
		//Not write to non-volatile memory when the initial phase
		void TemperatureCompensation(float coeff, float delta);
};

#endif
