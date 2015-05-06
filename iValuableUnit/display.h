#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <cstdint>
#include "FastDelegate.h"

using namespace std; 
using namespace fastdelegate;

#define LINE_HEIGHT			 35
#define CHAR_WEIGHT			 15
#define X_OFFSET				 5
#define RES_X						 400
#define RES_Y 					 240

#define MAXLINE_X_LIMIT	 (RES_X-X_OFFSET)


class Display
{
	private:
		static uint8_t bytes[0x100];
		static uint8_t bytesLen;
		static uint8_t ackBuffer[0x100];
		static uint16_t ackLength;
	public:
		typedef FastDelegate2<std::uint8_t *, std::uint8_t> AckRecieverHandler;
		static AckRecieverHandler OnAckReciever;
		static void UARTProcessor();
		static void ShowFormatString(const uint8_t *str, uint16_t size, uint16_t posy, float scale=1.0f);
		static void ShowStringVertical(const uint8_t *str, uint16_t strlen, uint16_t posx, uint16_t posy, float scale=1.0f);
		static uint16_t ShowString(const uint8_t *str, uint16_t posx, uint16_t posy, float scale=1.0f);
		static void ShowString(const uint8_t *str, uint16_t strlen, uint16_t posx, uint16_t posy, float scale=1.0f);
		static void ShowString(uint32_t number, uint16_t posx, uint16_t posy, float scale=1.0f);
		static void ClearRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
		static void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bk);
		static void DisplayOnOff(uint8_t on);
		static void Brightness(uint8_t level);
};

#endif
