#include "iap.h"
#include "gpio.h"

extern uint32_t SystemCoreClock;

#define IAP_LOCATION 0x1fff1ff1
	
unsigned int iap_command_param[5];
unsigned int iap_status_result[4];
	
typedef void (*IAP)(unsigned int [],unsigned int[]);
IAP iap_entry = (IAP)IAP_LOCATION;

#define IAP_COMMAND_PREPARE_SECTOR		50
#define IAP_COMMAND_COPY_TO_FLASH			51
#define IAP_COMMAND_ERASE_SECTOR			52
#define IAP_COMMAND_BLANK_CHECK				53
#define IAP_COMMAND_READ_ID						54
#define IAP_COMMAND_READ_VERSION			55
#define IAP_COMMAND_COMPARE						56
#define IAP_COMMAND_REINVOKE_ISP			57
#define IAP_COMMAND_READ_UID					58
//#define IAP_COMMAND_ERASE_PAGE				59

int PrepareSectors(unsigned int start, unsigned int end)
{
	iap_command_param[0] = IAP_COMMAND_PREPARE_SECTOR;
	iap_command_param[1] = start;
	iap_command_param[2] = end;
	iap_entry(iap_command_param, iap_status_result);
	return iap_status_result[0];
}

int CopyToFlash256(unsigned int dest, unsigned int source)
{
	dest &= 0xffffff00u;	//256 byte boundary
	PrepareSectors((dest>>12)&0xf, ((dest+0xff)>>12)&0xf);
	if (iap_status_result[0] == IAP_STATUS_CMD_SUCCESS)
	{
		iap_command_param[0] = IAP_COMMAND_COPY_TO_FLASH;
		iap_command_param[1] = dest;	
		iap_command_param[2] = source & 0xffffffc;	// Word boundary
		iap_command_param[3] = 256;
		iap_command_param[4] = SystemCoreClock/1000; // System clock in kHz
		iap_entry(iap_command_param, iap_status_result);
	}
	return iap_status_result[0];
}


int EraseSectors(unsigned int start, unsigned int end)
{
	PrepareSectors(start, end);
	if (iap_status_result[0] == IAP_STATUS_CMD_SUCCESS)
	{
		iap_command_param[0] = IAP_COMMAND_ERASE_SECTOR;
		iap_command_param[1] = start;
		iap_command_param[2] = end;
		iap_command_param[3] = SystemCoreClock/1000; // System clock in kHz
		iap_entry(iap_command_param, iap_status_result);
	}
	return iap_status_result[0];
}

void ReinvokeISP(int can)
{
	GPIOSetDir(PORT0, 3, E_IO_OUTPUT);
	GPIOSetValue(PORT0, 3, can!=0);
	iap_command_param[0] = IAP_COMMAND_REINVOKE_ISP;
	iap_entry(iap_command_param, iap_status_result);
}
