#ifndef _IAP_H
#define _IAP_H

#include <stdint.h>

#define IAP_STATUS_CMD_SUCCESS 					0
#define IAP_STATUS_INVALID_COMMAND 			1		//Invalid command.
#define IAP_STATUS_SRC_ADDR_ERROR 			2		//Source address is not on a word boundary.
#define IAP_STATUS_DST_ADDR_ERROR 			3		//Destination address is not on a correct boundary.
#define IAP_STATUS_SRC_ADDR_NOT_MAPPED 	4		//Source address is not mapped in the memory map.
																						//Count value is taken in to consideration where applicable.
#define IAP_STATUS_DST_ADDR_NOT_MAPPED 	5		//Destination address is not mapped in the memory
																						//map. Count value is taken in to consideration where applicable.
#define IAP_STATUS_COUNT_ERROR 					6		//Byte count is not multiple of 4 or is not a permitted value.
#define IAP_STATUS_INVALID_SECTOR 			7		//Sector number is invalid.
#define IAP_STATUS_SECTOR_NOT_BLANK 		8		//Sector is not blank.
#define IAP_STATUS_SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION		9	//Command to prepare sector for write operation was not executed.
#define IAP_STATUS_COMPARE_ERROR 				10	//Source and destination data is not same.
#define IAP_STATUS_BUSY 								11	//Flash programming

#ifdef __cplusplus
extern "C" {
#endif

int CopyToFlash256(unsigned int dest, unsigned int source);
int EraseSectors(unsigned int start, unsigned int end);
void ReinvokeISP(int can);

#ifdef __cplusplus
}
#endif

#endif

