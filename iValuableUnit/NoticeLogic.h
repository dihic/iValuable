#ifndef _NOTICE_LOGIC_H
#define _NOTICE_LOGIC_H

#include <cstdint>

#define NOTICE_RECOVER 						0x00
#define NOTICE_CLEAR 					 		0x01
#define NOTICE_GUIDE 							0x02
#define NOTICE_WARNING 						0x03
#define NOTICE_FAILURE 						0x04 
#define NOTICE_NONE 							0xff

class NoticeLogic
{
	private:
		static std::uint8_t lastState;
		static std::uint8_t currentState;	
		static void DrawNoticeBar(std::uint8_t r, std::uint8_t g, std::uint8_t b);
	public:
		static volatile std::uint8_t NoticeCommand;
		static void NoticeUpdate();
};

#endif
