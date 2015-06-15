#include "display.h"
#include "NoticeLogic.h"

#define COLOR_RED				0xff000001u
#define COLOR_GREEN			0x00ff0001u
#define COLOR_YELLOW		0xffff0001u
#define COLOR_CLEAR			0x00000001u

#define NOTICE_STATE_GUIDE 							0x01

uint8_t NoticeLogic::currentState = 0;
uint32_t NoticeLogic::exColor = 0;

// half: 0 for whole, 1 for left, 2 for right
void NoticeLogic::DrawNoticeBar(uint32_t color, std::uint8_t half)
{
	Display::SetColor(color);
	Display::ClearRegion((half==2?(RES_X>>1):0), MAXLINE_Y_LIMIT, (half?(RES_X>>1):RES_X), RES_Y - MAXLINE_Y_LIMIT);
	//Display::ClearRegion(0, MAXLINE_Y_LIMIT, RES_X, RES_Y - MAXLINE_Y_LIMIT);
	Display::SetColor(COLOR_CLEAR);
}

void NoticeLogic::NoticeUpdate(std::uint8_t command)
{
	switch (command)
	{
		case NOTICE_CLEAR_INFO:
			if (currentState == 0)
				break;
			if (exColor)
				DrawNoticeBar(exColor, 1);	//Clear with exception color
			else
				DrawNoticeBar(COLOR_CLEAR, 0);
			currentState = 0;
			break;
		case NOTICE_CLEAR_EXCEPTION:
			if (exColor == 0)
				break;
			if (currentState)
				DrawNoticeBar(COLOR_GREEN, 2);	//Clear with state color
			else
				DrawNoticeBar(COLOR_CLEAR, 0);
			exColor = 0;
			break;
		case NOTICE_GUIDE:
			if (currentState == NOTICE_STATE_GUIDE)
				break;
			DrawNoticeBar(COLOR_GREEN, exColor?1:0);
			currentState = NOTICE_STATE_GUIDE;
			break;
		case NOTICE_WARNING:
			if (exColor == COLOR_YELLOW)
				break;
			exColor = COLOR_YELLOW;
			DrawNoticeBar(exColor, currentState?2:0);
			break;
		case NOTICE_FAILURE:
			if (exColor == COLOR_RED)
				break;
			exColor = COLOR_RED;
			DrawNoticeBar(exColor, currentState?2:0);
			break;
		default:
			break;
	}
}

