#include "display.h"
#include "NoticeLogic.h"

#define COLOR_RED				0x010000ff
#define COLOR_GREEN			0x0100ff00
#define COLOR_YELLOW		0x0100ffff
#define COLOR_CLEAR			0x01000000

#define NOTICE_STATE_GUIDE 							0x01

volatile uint8_t NoticeLogic::NoticeCommand = NOTICE_NONE;

uint8_t NoticeLogic::currentState = 0;
uint32_t NoticeLogic::exColor = 0;

// half: 0 for whole, 1 for left, 2 for right
void NoticeLogic::DrawNoticeBar(uint32_t color, std::uint8_t half)
{
	Display::SetColor(color);
	Display::ClearRegion((half==2?(RES_X>>1):0), MAXLINE_Y_LIMIT, (half?(RES_X>>1):RES_X), RES_Y - MAXLINE_Y_LIMIT);
	Display::SetColor(COLOR_CLEAR);
}

bool NoticeLogic::NoticeUpdate()
{
	if (NoticeCommand == NOTICE_NONE)
		return (exColor && currentState);
	switch (NoticeCommand)
	{
		case NOTICE_CLEAR_INFO:
			if (exColor)
				DrawNoticeBar(exColor, 1);	//Clear with exception color
			else
				DrawNoticeBar(COLOR_CLEAR, 0);
			currentState = 0;
			break;
		case NOTICE_CLEAR_EXCEPTION:
			if (currentState)
				DrawNoticeBar(COLOR_GREEN, 2);	//Clear with state color
			else
				DrawNoticeBar(COLOR_CLEAR, 0);
			exColor = 0;
			break;
		case NOTICE_GUIDE:
			DrawNoticeBar(COLOR_GREEN, exColor?1:0);
			break;
		case NOTICE_WARNING:
			exColor = COLOR_YELLOW;
			break;
		case NOTICE_FAILURE:
			exColor = COLOR_RED;
			break;
		default:
			break;
	}
	if (exColor)
		DrawNoticeBar(exColor, currentState?2:0);
	NoticeCommand = NOTICE_NONE;
	return (exColor && currentState);
}

