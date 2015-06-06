#include "ISPComm.h"

using namespace std;

const std::uint8_t ISPComm::dataReceHeader[2] = {0x33, 0xcc};
const std::uint8_t ISPComm::dataSendHeader[2] = {0x5a, 0xa5};

boost::shared_ptr<ISPComm> ISPComm::instance;

boost::shared_ptr<ISPComm> &ISPComm::CreateInstance(ARM_DRIVER_USART &u)
{
	if (instance == nullptr || &instance->uart != &u)
		instance.reset(new ISPComm(u));
	return instance;
}

extern "C"
{
	void ISPComm::UARTCallback(uint32_t event)
	{
		switch (event)    
		{    
			case ARM_USART_EVENT_RECEIVE_COMPLETE:
			case ARM_USART_EVENT_TRANSFER_COMPLETE:
				osSignalSet(instance->tid, 0x01);
				break;
			case ARM_USART_EVENT_SEND_COMPLETE:    
			case ARM_USART_EVENT_TX_COMPLETE:      
				break;     
			case ARM_USART_EVENT_RX_TIMEOUT:         
				break;     
			case ARM_USART_EVENT_RX_OVERFLOW:    
			case ARM_USART_EVENT_TX_UNDERFLOW:        
				break;    
		}
	}
}


ISPComm::ISPComm(ARM_DRIVER_USART &u)
	:UartComm(u, UARTCallback, 1000000, ISPCOMM_BUFFER_SIZE)
{
}

ISPComm::~ISPComm()
{
}

void ISPComm::DataProcess(std::uint8_t byte)
{
	switch (dataState)
	{
		case StateDelimiter1:
			if (byte==dataReceHeader[0])
				dataState = StateDelimiter2;
			break;
		case StateDelimiter2:
			if (byte == dataReceHeader[1])
				dataState = StateCommand;
			else
				dataState = StateDelimiter1;
			break;
		case StateCommand:
			checksum = command = byte;
			index = 0;
			parameterLen = 0;
			dataState = StateParameterLength;
			break;
		case StateParameterLength:
			checksum += byte;
			parameterLen |= (byte<<(index++*8));
			if (index>=2)
			{
				if (parameterLen == 0)
				{
					parameters.reset();
					dataState = StateDelimiter1;
					if (OnCommandArrivalEvent)
						OnCommandArrivalEvent(command, NULL, 0);
				}
				else
				{
					parameters = boost::make_shared<uint8_t[]>(parameterLen);
					parameterIndex = 0;
					dataState = StateParameters;
				}
			}
			break;
		case StateParameters:
			checksum += byte;
			parameters[parameterIndex++] = byte;
			if (parameterIndex >= parameterLen)
			{
				index = 0;
				parameterIndex = 0;
				dataState = StateChecksum;
			}
			break;
		case StateChecksum:
			parameterIndex |= (byte<<(index++*8));
			if (index>=2)
			{
				if (parameterIndex==checksum && OnCommandArrivalEvent)
						OnCommandArrivalEvent(command,parameters.get(),parameterLen);
				parameters.reset();
				dataState = StateDelimiter1;
			}
			break;
		default:
			break;
	}
}

void ISPComm::PostOverflow()
{
	parameters.reset();
	dataState = StateDelimiter1;
}

bool ISPComm::SendData(const uint8_t *data, size_t len)
{
	Sync();
	uart.Send(data, len);
	Sync();
	return true;
}

bool ISPComm::SendData(uint8_t command, const uint8_t *data, size_t len)
{
  uint8_t pre[5]={ dataSendHeader[0], dataSendHeader[1], command, (uint8_t)(len), (uint8_t)(len>>8) };
	uint16_t checksum = pre[2]+pre[3]+pre[4];
	for(auto i=0;i<len;++i)
		checksum+=data[i];
	Sync();
	uart.Send(pre,5);
	if (len>0)
	{
		Sync();
		uart.Send(data,len);
	}
	pre[0] = checksum & 0xff;
	pre[1] = checksum >> 8;
	Sync();
	uart.Send(pre, 2);
	Sync();
	return true;
}
