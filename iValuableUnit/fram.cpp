#include <LPC11xx.h>
#include <cstdint>
#include "gpio.h"
#include "ssp.h"
#include "fram.h"

using namespace std;

namespace FRAM
{
	uint8_t Init()
	{
		SSP_IOConfig(NVM_SPI);
		
		GPIOSetDir(NVM_CS_PIN,E_IO_OUTPUT);
		GPIOSetValue(NVM_CS_PIN,1);
		
		uint8_t data[4];
		uint8_t firstUse=0;
	
		ReadMemory(IDENTIFIER_ADDR,data,4);
		if (data[0]!=IDENTIFIER_0)
		{
			data[0]=IDENTIFIER_0;
			firstUse=1;
		}
		if (data[1]!=IDENTIFIER_1)
		{
			data[1]=IDENTIFIER_1;
			firstUse=1;
		}
		if (data[2]!=IDENTIFIER_2)
		{
			data[2]=IDENTIFIER_2;
			firstUse=1;
		}
		if (data[3]!=IDENTIFIER_3)
		{
			data[3]=IDENTIFIER_3;
			firstUse=1;
		}
		if (firstUse)
		{
			FRAM::WriteMemory(IDENTIFIER_ADDR, data, 4);
			return 1;
		}
		return 0;
	}
	
	void WriteAccess(uint8_t enable)
	{
		SSP_Init(NVM_SPI,0,NVM_CS_PIN);
		uint8_t val=enable ? WREN : WRDI;
		SSP_BeginTransaction(NVM_SPI);
		SSP_Write(NVM_SPI,&val,1);
		SSP_EndTransaction(NVM_SPI);
	}
	
	uint8_t ReadStatus()
	{
		uint8_t buf[4];
		buf[0]=RDSR;
		SSP_Init(NVM_SPI,0,NVM_CS_PIN);
		SSP_BeginTransaction(NVM_SPI);
		SSP_WriteRead(NVM_SPI,buf,buf+2,2);
		SSP_EndTransaction(NVM_SPI);
		return buf[3];
	}
	
	void WriteStatus(uint8_t status)
	{
		uint8_t buf[2]={WRSR,status};
		SSP_Init(NVM_SPI,0,NVM_CS_PIN);
		SSP_BeginTransaction(NVM_SPI);
		SSP_Write(NVM_SPI,buf,2);
		SSP_EndTransaction(NVM_SPI);
	}
	
	void WriteMemory(uint16_t address,uint8_t *data,uint16_t length)
	{
		WriteAccess(1);
		DELAY(1);
		SSP_BeginTransaction(NVM_SPI);
		uint8_t temp=((address&0x100)>>5)|WRITE;
		SSP_Write(NVM_SPI,&temp,1);
		SSP_Write(NVM_SPI,(uint8_t *)&address,1);
		SSP_Write(NVM_SPI,data,length);
		SSP_EndTransaction(NVM_SPI);
	}
	
	void ReadMemory(uint16_t address,uint8_t *data,uint16_t length)
	{
		SSP_Init(NVM_SPI,0,NVM_CS_PIN);
		SSP_BeginTransaction(NVM_SPI);
		uint8_t temp=((address&0x100)>>5)|READ;
		SSP_Write(NVM_SPI,&temp,1);
		SSP_Write(NVM_SPI,(uint8_t *)&address,1);
		SSP_Read(NVM_SPI,data,length);
		SSP_EndTransaction(NVM_SPI);
	}
}
