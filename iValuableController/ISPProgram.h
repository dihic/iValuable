#ifndef _ISP_PROGRAM_H
#define _ISP_PROGRAM_H

#include "ISPComm.h"
#include "FastDelegate.h"

using namespace fastdelegate;

class ISPProgram
{
	private:
		enum ISPUpdateItem
		{
			ISPFindDevice=1,
			ISPErrorCode,
			ISPPagePorgramData,
			ISPSectorErase,
			ISPGoCMD,
		};
		static osMessageQId MsgId;
		boost::shared_ptr<ISPComm> &comm;
		uint8_t buf[0x200];
		void CommandArrival(std::uint8_t command,std::uint8_t *parameters,std::size_t len);
	public:
	  bool FindDevice();
	  bool Format();
	  bool RestartDevice();
	  bool ProgramData(uint8_t page, const uint8_t *data, size_t len);
	
		ISPProgram(ARM_DRIVER_USART &u);
		~ISPProgram() {}
};

#endif
