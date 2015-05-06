#ifndef _FRAM_H_
#define _FRAM_H_

#include <cstdint>

using namespace std;

#define NVM_CS_PIN PORT0,7

#define WREN 0x06
#define WRDI 0x04
#define RDSR 0x05
#define WRSR 0x01
#define READ 0x03
#define WRITE 0x02

#define NVM_SPI 0

#define WP_ALL 0x06
#define WP_HALF 0x04
#define WP_QUARTER 0x02
#define WP_NONE 0

#define IDENTIFIER_ADDR 0x00
#define IDENTIFIER_0 0xAA
#define IDENTIFIER_1 0xAB
#define IDENTIFIER_2 0xAC
#define IDENTIFIER_3 0xAD

namespace FRAM
{
  uint8_t Init();
	void WriteAccess(uint8_t enable);
	void WriteStatus(uint8_t status);
	uint8_t ReadStatus();
	void WriteMemory(uint16_t address,uint8_t *data,uint16_t length);
	void ReadMemory(uint16_t address,uint8_t *data,uint16_t length);
}

#endif
