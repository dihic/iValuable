#ifndef _MAIN_H
#define _MAIN_H

#include <cstdint>
#include "canonchip.h"


#define SYNC_GOTCHA			0x0180
#define SYNC_DATA				0x0100
#define SYNC_LIVE				0x01ff

#define OP_SET_ZERO 	0x00fe
#define OP_RAMPS			0x8000
#define OP_AUTO_RAMPS	0x8007
#define OP_UNIT_INFO  0x8001
#define OP_CAL_UNIT   0x8002
#define OP_CONFIG     0x8003
#define OP_RAWDATA		0x8006
#define OP_ENABLE			0x8008
#define OP_TEMP				0x8009
#define OP_QUANTITY		0x800A
#define OP_ISP				0x80ff


//#define A_POS_X		5
//#define A_POS_Y		170
//#define B_POS_X 	170
//#define B_POS_Y		170
//#define DIGIT_W		150
//#define DIGIT_H		70
//#define DIGIT_SCALE 2.0f
//#define AUX_SCALE   1.5f


//#define A_NUM_OFFSET	70
//#define B_NUM_OFFSET	60	

struct CanResponse
{
	uint16_t sourceId;
	uint8_t result;
	uint8_t buffer[0x80];
	CAN_ODENTRY response;
};

struct WeightSet
{
	float Total;
	float Inventory;
	float Delta;
	float Min;
	float Max;
	bool AllStable;
};

volatile CanResponse res;

volatile bool syncTriggered = false;
//volatile bool responseTriggered = false;
volatile CAN_ODENTRY syncEntry;

volatile bool Connected = true;			
volatile bool Registered = false;		// Registered by host
//volatile bool ForceSync = false;
volatile bool Gotcha = true;

volatile bool ForceDisplay = false;


#endif
