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

class ScaleInfo
{
	private:
		std::uint8_t channel;
		ScaleBasic *basic;
		std::uint16_t base;
	public:
		typedef FastDelegate3<std::uint16_t, std::uint8_t *, std::uint16_t> WriteNVHandler;
		static WriteNVHandler WriteNV;
		ScaleInfo(std::uint8_t ch, ScaleBasic *sb, std::uint32_t nvAddr)
			:channel(ch), basic(sb), base(nvAddr) {}
		~ScaleInfo() {}
		ScaleBasic *GetBasic() { return basic; }
		bool Calibrate(float w);
		int32_t SetZero();
		int32_t SetTare();
		void SetRamp(float ramp);
		//Not write to non-volatile memory when the initial phase
		void TemperatureCompensation(float coeff, float delta);
};

#endif
