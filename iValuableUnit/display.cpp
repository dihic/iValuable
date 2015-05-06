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

void Display::ShowFormatString(const uint8_t *str, uint16_t size, uint16_t posy, float scale)
{
	uint16_t index = 0;
	uint16_t tail = 0;
	uint16_t len = 0;
	bool more = true;
	bool sign = false;
	bool vertical = true;
	bool custom_w = false;
	
	int currentW = X_OFFSET;
	int xlimit = MAXLINE_X_LIMIT;
	
	int wx, wy;
	
	//bool newline = true;
	
	while (tail < size)
	{
		more = true;

		if (str[tail]==0 && str[tail+1]==0)
		{
			if (len>0)
			{
				if (sign)
				{
					int s=(int)(LINE_HEIGHT*WARNING_SCALE);
					if (vertical)
					{
						ClearRegion(wx, wy, s+WARNING_MARGIN_X, s*len+WARNING_MARGIN_Y);
						ShowStringVertical(str+index, len, wx+WARNING_MARGIN_X, wy+WARNING_MARGIN_Y, WARNING_SCALE);
					}
					else
					{
						ClearRegion(wx, wy, s*len, s+WARNING_MARGIN_Y);
						ShowString(str+index, len, wx+WARNING_MARGIN_X, wy, WARNING_SCALE);
					}
					sign = false;
				}
				else
				{
					ShowString(str+index, len, X_OFFSET, posy, scale);
					posy += LINE_HEIGHT;
				}
				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
			}
			index += 2;
			tail += 2;
			
			SetColor(255, 255, 255, 0);
			SetColor(0, 0, 0, 1);
			more = false;
			//newline = true;
			
		}
		else if ((str[tail]==1 || str[tail]==2) && str[tail+1]==0)
		{
			if (len>0)
			{
				ShowString(str+index, len, X_OFFSET, posy, scale);
				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
				posy += LINE_HEIGHT;
			}
			index += 8;
			
			vertical = (str[tail]==1);
			if (!custom_w)
			{
				if (vertical)
				{
					wx = WARNING_X_V;
					wy = WARNING_Y_V;
				}
				else
				{
					wx = WARNING_X_H;
					wy = WARNING_Y_H;
				}
				xlimit = wx;
			}
			
			SetColor(str[tail+2], str[tail+3], str[tail+4], 0);
			SetColor(str[tail+5], str[tail+6], str[tail+7], 1);
			more = false;
			sign = true;
			//newline = true;
			tail += 8;
		}
		else if (str[tail]==3  && str[tail+1]==0)
		{
			if (len>0)
			{
				ShowString(str+index, len, X_OFFSET, posy, scale);
				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
				posy += LINE_HEIGHT;
			}
			index += 6;
			custom_w = true;
			wx = str[tail+2] | (str[tail+3]<<8);
			wy = str[tail+4] | (str[tail+5]<<8);
			xlimit = wx;
			more = false;
			tail += 6;
		}
		else
		{
			++len;
			currentW += CHAR_WEIGHT;
			if (str[tail+1]!=0)
				currentW += CHAR_WEIGHT;
			if (currentW >= xlimit-CHAR_WEIGHT*2)
			{
				ShowString(str+index, len, X_OFFSET, posy, scale);
				more = false;
				index += (len<<1);
				len = 0;
				currentW = X_OFFSET;
				posy+=LINE_HEIGHT;
			}
			tail+=2;
		}
	}
	if (more)
	{
		ShowString(str+index, len, X_OFFSET, posy, scale);
	}
	//Reset color
	SetColor(255, 255, 255, 0);
	SetColor(0, 0, 0, 1);
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
