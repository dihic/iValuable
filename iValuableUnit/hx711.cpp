#include <cstdint>
#include <LPC11xx.h>
#include "gpio.h"
#include "hx711.h"
#include "delay.h"

using namespace std;

#define HX_SCK  sckPort,sckBit
#define HX_DOUT doutPort,doutBit

const float HX711::RangeCoeff = 0.256f;

HX711::HX711(uint32_t sckPortNum, uint32_t sckBitPos,uint32_t doutPortNum, uint32_t doutBitPos)
	:sckPort(sckPortNum),sckBit(sckBitPos),doutPort(doutPortNum),doutBit(doutBitPos)
{
	GPIOSetDir(HX_SCK,E_IO_OUTPUT);
	DrvGPIO_ClrBit(HX_SCK);
	GPIOSetDir(HX_DOUT,E_IO_INPUT);
	GPIOSetInterrupt(HX_DOUT,1,0,0);
	GPIOIntEnable(HX_DOUT);
	CurrentData=0;
}

int32_t HX711::ReadData(uint8_t num)
{
	GPIOIntClear(HX_DOUT);
	GPIOIntDisable(HX_DOUT);
	uint32_t val=0;
	for (int8_t i=23;i>=0;i--)
	{
		GPIOSetValue(HX_SCK,1);
		DELAY(10);
		GPIOSetValue(HX_SCK,0);
		if (GPIOGetValue(HX_DOUT))
			val|=(1<<i);
		DELAY(5);
	}
	//25th SCK for ChA 128 gain
	GPIOSetValue(HX_SCK,1);
	DELAY(10);
	GPIOSetValue(HX_SCK,0);
	DELAY(10);
	
	if (num>0 && num<24)
		val>>=(24-num);
	
	GPIOIntEnable(HX_DOUT);
	
	if (val & (1<<(num-1)))
	{
		uint32_t mask=0;
		for(int8_t i=num; i<32; ++i)
			mask |= (1<<i);
		val |= mask;
	}
	return CurrentData=val;
}			
