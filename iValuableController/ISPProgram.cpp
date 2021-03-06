#include "ISPProgram.h"
#include <cstring>
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX

#include "System.h"

#ifdef DEBUG_PRINT
#include <iostream>
#endif

osMessageQDef(MsgBox, 8, uint32_t);               // Define message queue

#define ISP_ERROR		0xffffffffu
#define ISP_SYNC_TIME			200
#define ISP_FORMAT_TIME		500

using namespace std;

osMessageQId ISPProgram::MsgId = NULL;

ISPProgram::ISPProgram(ARM_DRIVER_USART &u)
	:comm(ISPComm::CreateInstance(u))
{
	if (MsgId == NULL)
		MsgId = osMessageCreate(osMessageQ(MsgBox), NULL);  
	comm->OnCommandArrivalEvent.bind(this, &ISPProgram::CommandArrival);
	comm->Start();
#ifdef DEBUG_PRINT
	cout<<"CAN-ISP UART Initialized."<<endl;
#endif
}

void ISPProgram::CommandArrival(std::uint8_t command, std::uint8_t *parameters, std::size_t len)
{
	uint32_t val = 0;
#ifdef DEBUG_PRINT
		cout<<"CAN-ISP Response: "<<(int)command<<endl;
#endif
	switch (command)
	{
		case ISPFindDevice:
		case ISPPagePorgramData:
		case ISPSectorErase:
		case ISPGoCMD:
			memcpy(&val, parameters, len<4?len:4);
			break;
	  case ISPErrorCode:
#ifdef DEBUG_PRINT
		cout<<"CAN-ISP Error: "<<(int)parameters[0]<<endl;
#endif
			val = ISP_ERROR;
		default:
			break;
	}
	osMessagePut(MsgId, val, 0);
}

bool ISPProgram::FindDevice()
{
  static const uint8_t ISPFindDeviceString[8] = {0x5a, 0xa5, 0x01, 0x01, 0x00, 0x01, 0x03, 0x00};
	comm->SendData(ISPFindDeviceString, 8);
	osEvent result = osMessageGet(MsgId, ISP_SYNC_TIME);
	if (result.status != osEventMessage)
		return false;
	return (result.value.v != ISP_ERROR);
}

bool ISPProgram::Format()
{
	static const uint8_t ISPSectorEraseString[8] = {0x5a, 0xa5, 0x04, 0x01, 0x00, 0x01, 0x06, 0x00};
	comm->SendData(ISPSectorEraseString, 8);
	osEvent result = osMessageGet(MsgId, ISP_FORMAT_TIME);
	if (result.status != osEventMessage)
		return false;
	return (result.value.v != ISP_ERROR);
}

bool ISPProgram::RestartDevice()
{
  static const uint8_t ISPGoCMDString[8] = {0x5a, 0xa5, 0x05, 0x01, 0x00, 0x01, 0x07, 0x00};
	comm->SendData(ISPGoCMDString, 8);
	osEvent result = osMessageGet(MsgId, ISP_SYNC_TIME);
	if (result.status != osEventMessage)
		return false;
	return (result.value.v != ISP_ERROR);
}

bool ISPProgram::ProgramData(uint8_t page, const uint8_t *data, size_t len)
{
	buf[0] = page;
	memcpy(buf+1, data, len);
	for(auto j=0;j<3;++j)
	{
		comm->SendData(ISPPagePorgramData, buf, len+1);
		osEvent result = osMessageGet(MsgId, ISP_SYNC_TIME);
		if (result.status == osEventMessage)
			return (result.value.v != ISP_ERROR);
	}
	return false;
}
