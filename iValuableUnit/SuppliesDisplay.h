#ifndef _SUPPLIES_DISPLAY_H
#define _SUPPLIES_DISPLAY_H

#include <cstdint>

#define SUPPLIES_FLASH_ADDR 		0x7000
#define SUPPLIES_FLASH_SECTOR 	0x7
#define BACKUP_FLASH_SECTOR 	0x6

#define SUPPLIES_STRING(i) 			(const volatile std::uint8_t *)(SUPPLIES_FLASH_ADDR|(i<<8)|2)
#define SUPPLIES_STRING_SIZE(i) (*(const volatile std::uint8_t *)(SUPPLIES_FLASH_ADDR|(i<<8)))

class SuppliesDisplay
{
	private:
		static __align(4) std::uint8_t tempBuffer[0x100];
		SuppliesDisplay() {}
	public:
		static void Init();
		~SuppliesDisplay() {}
		static bool ModifyString(int index, const std::uint8_t *source, std::uint8_t len);
		static bool DeleteString(int index);
		static const std::uint8_t *GetString(int index, std::uint8_t &len);
};

#endif
