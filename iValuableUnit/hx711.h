#ifndef __HX711_H__
#define __HX711_H__

#include <cstdint>
#include "gpio.h"

class HX711
{
	private:
		const uint32_t sckPort;
		const uint32_t sckBit;
		const uint32_t doutPort;
		const uint32_t doutBit;
	public:
		static const float RangeCoeff;
		int32_t CurrentData;
		HX711(uint32_t sckPortNum, uint32_t sckBitPos,uint32_t doutPortNum, uint32_t doutBitPos);
		~HX711() {}
		int32_t ReadData(uint8_t num=0);
};

#endif
