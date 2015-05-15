#include "uart.h"
#include <cstdint>
#include <cstring>
#include "display.h"

using namespace std;

#define COMMAND_DRAW_STRING  		0xA1
#define COMMAND_DRAW_DIGIT 			0xA4
#define COMMAND_SET_COLOR 			0xA2
#define COMMAND_CLEAR_REGION 		0xA3
#define COMMAND_DISPLAY_ON_OFF 	0xA5
#define COMMAND_BRIGHTNESS 			0xA6

#define WARNING_X_V        335
#define WARNING_Y_V        5
#define WARNING_X_H        240
#define WARNING_Y_H        180
#define WARNING_SCALE    1.5f

#define WARNING_MARGIN_X	10
#define WARNING_MARGIN_Y	5

extern uint32_t UARTStatus;
extern uint8_t UARTBuffer[UART_BUFSIZE];
extern uint32_t UARTCount;

void Display::UARTProcessor()
{
	static uint8_t phase=0;
	static uint8_t sumcheck=0;
	static uint16_t readIndex=0;
	static uint32_t UARTPostion=0;
	
	while (UARTPostion!=UARTCount)
	{
		uint8_t byte=UARTBuffer[UARTPostion++];
		if (UARTPostion>=UART_BUFSIZE)
			UARTPostion=0;
		switch (phase)
		{
			case 0:
				if (byte==REV_HEADER)
				{
					phase=1;
					readIndex=0;
					ackLength=0;
					sumcheck=REV_HEADER;
				}
				break;
			case 1:
				sumcheck+=byte;
				ackLength|=byte<<(readIndex*8);
				readIndex++;
				if (readIndex>=2)
				{
					phase=2;
					readIndex=0;
				}				
				break;
			case 2:
				sumcheck+=byte;
				ackBuffer[readIndex++]=byte;
				if (readIndex>=ackLength)
					phase=3;
				break;
			case 3:
				if (byte==sumcheck && OnAckReciever)
					OnAckReciever(ackBuffer, ackLength);
					//Ack=1;
				phase=0;
				break;
		}
	}
}

uint8_t Display::bytes[0x100];
uint8_t Display::bytesLen = 0;
uint8_t Display::ackBuffer[0x100];
uint16_t Display::ackLength = 0;

Display::AckRecieverHandler Display::OnAckReciever = NULL;

uint16_t Display::ShowFormatString(const uint8_t *str, uint16_t size, uint16_t &posy, float scale)
{
	uint16_t index = 0;
	uint16_t tail = 0;
	uint16_t len = 0;
	bool more = true;
	
	int currentW = X_OFFSET;
	
	while (tail < size)
	{
		more = true;

		if (str[tail]==0 && str[tail+1]==0)
		{
			if (len>0)
			{
				ShowString(str+index, len, X_OFFSET, posy, scale);
				posy += LINE_HEIGHT;
				if (posy+LINE_HEIGHT > MAXLINE_Y_LIMIT)
					return tail+2;	//Need to clear screen in next update

				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
			}
			index += 2;
			tail += 2;
			
			SetColor(255, 255, 255, 0);
			SetColor(0, 0, 0, 1);
			more = false;
		}
		else if (str[tail]==4 && str[tail+1]==0)	//Set colors
		{
			SetColor(str[tail+2], str[tail+3], str[tail+4], 0);
			SetColor(str[tail+5], str[tail+6], str[tail+7], 1);
			tail += 8;
		}
		else
		{
			++len;
			currentW += CHAR_WEIGHT;
			if (str[tail+1]!=0)
				currentW += CHAR_WEIGHT;
			if (currentW >= MAXLINE_X_LIMIT-CHAR_WEIGHT*2)
			{
				ShowString(str+index, len, X_OFFSET, posy, scale);
				posy+=LINE_HEIGHT;
				if (posy+LINE_HEIGHT > MAXLINE_Y_LIMIT)
					return tail+2;	//Need to clear screen in next update
				more = false;
				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
			}
			tail+=2;
		}
	}
	if (more)
	{
		ShowString(str+index, len, X_OFFSET, posy, scale);
		if (posy+2*LINE_HEIGHT > MAXLINE_Y_LIMIT)
		{
			//Reset color
			SetColor(255, 255, 255, 0);
			SetColor(0, 0, 0, 1);
			return 0;		//Just completed but need to clear screen in next update
		}
	}
	
	//Reset color
	SetColor(255, 255, 255, 0);
	SetColor(0, 0, 0, 1);
	return size;	//Just completed
}
	
void Display::ShowString(const uint8_t *str, uint16_t strlen, uint16_t posx, uint16_t posy, float scale)
{
	uint16_t len = 10 + (strlen<<1);
	bytes[0] = COMMAND_DRAW_STRING;
	bytes[1] = strlen;
	bytes[2] = posx & 0xff;
	bytes[3] = posx >> 8;
	bytes[4] = posy & 0xff;
	bytes[5] = posy >> 8;
	memcpy(bytes+6,&scale,4);
	memcpy(bytes+10,str,strlen<<1);
	UARTSendSpecial(bytes, len);
}
	
void Display::ShowStringVertical(const uint8_t *str, uint16_t strlen, uint16_t posx, uint16_t posy, float scale)
{
	uint16_t len = 12;
	bytes[0] = COMMAND_DRAW_STRING;
	bytes[1] = 1;
	bytes[2] = posx & 0xff;
	bytes[3] = posx >> 8;
	memcpy(bytes+6,&scale,4);
	for(int i=0;i<strlen;++i)
	{
		bytes[4] = posy & 0xff;
		bytes[5] = posy >> 8;
		posy+=(uint16_t)(LINE_HEIGHT*scale-5);
		memcpy(bytes+10, str+(i<<1), 2);
		UARTSendSpecial(bytes, len);
	}
}
	
uint16_t Display::ShowString(const uint8_t *str, uint16_t posx, uint16_t posy, float scale)
{
	uint16_t len = 0;
	bytes[0] = COMMAND_DRAW_STRING;
	bytes[2] = posx & 0xff;
	bytes[3] = posx >> 8;
	bytes[4] = posy & 0xff;
	bytes[5] = posy >> 8;
	memcpy(bytes+6,&scale,4);
	while (str[len]!=0)
	{
		bytes[10+len] = str[len];
		bytes[11+len] = str[len+1];
		len+=2;
	}
	bytes[1] = len >> 1;
	UARTSendSpecial(bytes, len+10);
	return len;
}
	
void Display::ShowString(uint32_t number, uint16_t posx, uint16_t posy, float scale)
{
	bytesLen = 14;
	bytes[0] = COMMAND_DRAW_DIGIT;
	bytes[1] = 0;
	bytes[2] = posx & 0xff;
	bytes[3] = posx >> 8;
	bytes[4] = posy & 0xff;
	bytes[5] = posy >> 8;
	memcpy(bytes+6,&scale,4);
	memcpy(bytes+10,&number,4);
	UARTSendSpecial(bytes,bytesLen);
}

void Display::ClearRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	bytesLen=9;
	bytes[0] = COMMAND_CLEAR_REGION;
	bytes[1] = x & 0xff;
	bytes[2] = x >> 8;
	bytes[3] = y & 0xff;
	bytes[4] = y >> 8;
	bytes[5] = w & 0xff;
	bytes[6] = w >> 8;
	bytes[7] = h & 0xff;
	bytes[8] = h >> 8;
	UARTSendSpecial(bytes,bytesLen);
}

void Display::SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bk)
{
	bytesLen=5;
	bytes[0] = COMMAND_SET_COLOR;
	bytes[1] = bk ? 1 : 0;
	bytes[2] = b;
	bytes[3] = g;
	bytes[4] = r;
	//buffer[5] = 0;
	UARTSendSpecial(bytes,bytesLen);
}
	
void Display::DisplayOnOff(uint8_t on)
{
	bytesLen=2;
	bytes[0] = COMMAND_DISPLAY_ON_OFF;
	bytes[1] = on;
	UARTSendSpecial(bytes,bytesLen);
}
	
void Display::Brightness(uint8_t level)
{
	bytesLen=2;
	bytes[0] = COMMAND_BRIGHTNESS;
	bytes[1] = level;
	UARTSendSpecial(bytes,bytesLen);
}
