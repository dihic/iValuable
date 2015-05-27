#include "display.h"
#include "NoticeLogic.h"

#define COLOR_RED				255,0,0
#define COLOR_GREEN			0,255,0
#define COLOR_YELLOW		255,255,0
#define COLOR_CLEAR			0,0,0


volatile uint8_t NoticeLogic::NoticeCommand = NOTICE_NONE;

uint8_t NoticeLogic::lastState = NOTICE_CLEAR;
uint8_t NoticeLogic::currentState = NOTICE_CLEAR;


void NoticeLogic::DrawNoticeBar(uint8_t r, uint8_t g, uint8_t b)
{
	Display::SetColor(r, g, b, 1);
	Display::ClearRegion(0, MAXLINE_Y_LIMIT, RES_X, RES_Y - MAXLINE_Y_LIMIT);
	Display::SetColor(0, 0, 0, 1);
}

void NoticeLogic::NoticeUpdate()
{
	if (NoticeCommand == currentState)
	{
		NoticeCommand = NOTICE_NONE;
		return;
	}
	switch (NoticeCommand)
	{
		case NOTICE_RECOVER:
			NoticeCommand = lastState;
			break;
		case NOTICE_CLEAR:
			Display::SetColor(COLOR_CLEAR, 1);
			Display::ClearRegion(0, MAXLINE_Y_LIMIT, RES_X, RES_Y - MAXLINE_Y_LIMIT);
			currentState = lastState = NOTICE_CLEAR;
			NoticeCommand = NOTICE_NONE;
			break;
		case NOTICE_GUIDE:
			DrawNoticeBar(COLOR_GREEN);
			NoticeCommand = NOTICE_NONE;
			currentState = NOTICE_GUIDE;
			break;
		case NOTICE_WARNING:
			DrawNoticeBar(COLOR_YELLOW);
			currentState = lastState = NOTICE_WARNING;
			NoticeCommand = NOTICE_NONE;
			break;
		case NOTICE_FAILURE:
			DrawNoticeBar(COLOR_RED);
			currentState = lastState = NOTICE_FAILURE;
			NoticeCommand = NOTICE_NONE;
			break;
		default:
			break;
	}
	
}

