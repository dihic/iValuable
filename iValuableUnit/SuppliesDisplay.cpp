#include "SuppliesDisplay.h"
#include "iap.h"
#include "system.h"

#include <string>

using namespace std;

__align(4) std::uint8_t SuppliesDisplay::tempBuffer[0x100];

void SuppliesDisplay::Init()
{
	memset(tempBuffer, 0 ,0x100);
}

void SuppliesDisplay::CopyFromSource(int index, const uint8_t *source, uint8_t len)
{
#if UNIT_TYPE == UNIT_TYPE_INDEPENDENT
	++index;
	if (index<10)
	{
		tempBuffer[0] = len+4;
		tempBuffer[1] = 0;
		tempBuffer[2] = 0x30+index;
		tempBuffer[3] = 0;
		tempBuffer[4] = 0x2e;		//Char .
		tempBuffer[5] = 0;
		memcpy(tempBuffer+6, source, len);
	}
	else
	{
		tempBuffer[0] = len+6;
		tempBuffer[1] = 0;
		tempBuffer[2] = 0x30+index%10;
		tempBuffer[3] = 0;
		tempBuffer[4] = 0x30+index/10;
		tempBuffer[5] = 0;
		tempBuffer[6] = 0x2e;		//Char .
		tempBuffer[7] = 0;
		memcpy(tempBuffer+8, source, len);
	}
#else
	
	tempBuffer[0] = len+2;
	tempBuffer[1] = 0;
	tempBuffer[2] = 0x22;					//Char •
	tempBuffer[3] = 0x20;
	memcpy(tempBuffer+4, source, len);
//	tempBuffer[0] = len;
//	tempBuffer[1] = 0;
//	memcpy(tempBuffer+2, source, len);
#endif
}

bool SuppliesDisplay::ModifyString(int index, const uint8_t *source, uint8_t len)
{
	if (index>=SUPPLIES_NUM || len>200)
		return false;
	
	const uint8_t *status = (const uint8_t *)((SUPPLIES_FLASH_SECTOR<<12)|(index<<8));
	if (*status == 0xff)	//Current index available
	{
		CopyFromSource(index, source, len);
		CopyToFlash256((SUPPLIES_FLASH_SECTOR<<12)|(index<<8), (unsigned int)tempBuffer);
		CopyToFlash256((BACKUP_FLASH_SECTOR<<12)|(index<<8), (unsigned int)tempBuffer);
		return true;
	}
	
	EraseSectors(SUPPLIES_FLASH_SECTOR, SUPPLIES_FLASH_SECTOR);
	for(int i=0;i<SUPPLIES_NUM;++i)
	{
		if (i==index)
			CopyFromSource(i, source, len);
		else
		{
			status = (const uint8_t *)((BACKUP_FLASH_SECTOR<<12)|(i<<8));
			if (*status == 0x00) //Skip if deleted before
				continue;
			memcpy(tempBuffer, (const void *)((BACKUP_FLASH_SECTOR<<12)|(i<<8)), 0x100);
		}
		CopyToFlash256((SUPPLIES_FLASH_SECTOR<<12)|(i<<8), (unsigned int)tempBuffer);
	}
	
	//Backup
	EraseSectors(BACKUP_FLASH_SECTOR, BACKUP_FLASH_SECTOR);
	for(int i=0;i<SUPPLIES_NUM;++i)
	{
		memcpy(tempBuffer, (const void *)((SUPPLIES_FLASH_SECTOR<<12)|(i<<8)), 0x100);
		CopyToFlash256((BACKUP_FLASH_SECTOR<<12)|(i<<8), (unsigned int)tempBuffer);
	}
	return true;
}

bool SuppliesDisplay::DeleteString(int index)
{
	if (index>=SUPPLIES_NUM)
		return false;
	const uint8_t *status = (const uint8_t *)((SUPPLIES_FLASH_SECTOR<<12)|(index<<8));
	if (*status == 0xff || *status == 0x00)	//if available or deleted
		return true;
	memcpy(tempBuffer, (const void *)((SUPPLIES_FLASH_SECTOR<<12)|(index<<8)), 0x100);
	tempBuffer[0] = 0x00; //Deleted tag
	CopyToFlash256((SUPPLIES_FLASH_SECTOR<<12)|(index<<8), (unsigned int)tempBuffer);
	CopyToFlash256((BACKUP_FLASH_SECTOR<<12)|(index<<8), (unsigned int)tempBuffer);
	return true;
}

const uint8_t *SuppliesDisplay::GetString(int index, std::uint8_t &len)
{
	const uint8_t *status = (const uint8_t *)((SUPPLIES_FLASH_SECTOR<<12)|(index<<8));
	if (*status == 0xff || *status == 0x00)	//if empty or deleted
	{
		len = 0;
		return NULL;
	}
	len = *status;
	return (const uint8_t *)((SUPPLIES_FLASH_SECTOR<<12)|(index<<8)|2);
}




