#ifndef _NOTICE_LOGIC_H
#define _NOTICE_LOGIC_H

#include <cstdint>


#define NOTICE_CLEAR_INFO 				0x00
#define NOTICE_CLEAR_EXCEPTION 		0x01
#define NOTICE_GUIDE 							0x02
#define NOTICE_WARNING 						0x03
#define NOTICE_FAILURE 						0x04
#define NOTICE_NONE 							0xff

class NoticeLogic
{
	private:
		static std::uint8_t currentState;
		static std::uint32_t exColor;
		static void DrawNoticeBar(std::uint32_t color, std::uint8_t half);
	public:
		static bool AnyShown() { return exColor||currentState; }
		static void NoticeUpdate(std::uint8_t command);
};

#endif
