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
		bool needUpdate = false;
		switch (event)    
		{    
			case ARM_USART_EVENT_RECEIVE_COMPLETE:
			case ARM_USART_EVENT_TRANSFER_COMPLETE:
				if (!instance->uart.GetStatus().rx_busy)
					needUpdate = true;
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
		
		if (needUpdate)
		{
			++instance->readRound;
			instance->tail = 0;
			instance->uart.Receive(instance->buffer, ISPCOMM_BUFFER_SIZE);
		}
	}
}

ISPComm::ISPComm(ARM_DRIVER_USART &u)
	:uart(u), isStarted(false)
{
	uart.Initialize(UARTCallback);
	uart.PowerControl(ARM_POWER_FULL);
	uart.Control(ARM_USART_MODE_ASYNCHRONOUS |
							 ARM_USART_DATA_BITS_8 |
							 ARM_USART_PARITY_NONE |
							 ARM_USART_STOP_BITS_1 |
							 ARM_USART_FLOW_CONTROL_NONE, 1000000);
}

ISPComm::~ISPComm()
{
	Stop();
	uart.Uninitialize();
}

void ISPComm::Start()
{
	if (isStarted)
		return;
	uart.Control(ARM_USART_CONTROL_TX, 1);
	uart.Control(ARM_USART_CONTROL_RX, 1);
	dataState = StateDelimiter1;
	command = 0; 
	head = tail = 0;
	readRound = 0;
	processRound = 0;
	uart.Receive(buffer, ISPCOMM_BUFFER_SIZE);
	isStarted = true;
}
	
void ISPComm::Stop()
{
	if (!isStarted)
		return;
	uart.Control(ARM_USART_CONTROL_TX, 0);
	uart.Control(ARM_USART_CONTROL_RX, 0);
	uart.Control(ARM_USART_ABORT_TRANSFER, 0);
	isStarted = false;
}

void ISPComm::DataReceiver()
{
	if (!uart.GetStatus().rx_busy)
		return;
	
	tail = uart.GetRxCount();
	if (tail==ISPCOMM_BUFFER_SIZE)		//wait for next read to avoid misjudgment
		return;
	
	while (tail!=head)
	{
		switch (dataState)
		{
			case StateDelimiter1:
				if (buffer[head]==dataReceHeader[0])
					dataState = StateDelimiter2;
				break;
			case StateDelimiter2:
				if (buffer[head] == dataReceHeader[1])
					dataState = StateCommand;
				else
					dataState = StateDelimiter1;
				break;
			case StateCommand:
				checksum = command = buffer[head];
				index = 0;
				parameterLen = 0;
				dataState = StateParameterLength;
				break;
			case StateParameterLength:
				checksum += buffer[head];
				parameterLen |= (buffer[head]<<(index++*8));
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
				checksum += buffer[head];
				parameters[parameterIndex++] = buffer[head];
				if (parameterIndex >= parameterLen)
				{
					index = 0;
					parameterIndex = 0;
					dataState = StateChecksum;
				}
				break;
			case StateChecksum:
				parameterIndex |= (buffer[head]<<(index++*8));
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
		if (++head>=ISPCOMM_BUFFER_SIZE)
		{
			++processRound;
			head=0;
		}
	}
}

inline void ISPComm::Sync()
{
	while (uart.GetStatus().tx_busy)
		__nop();
}

bool ISPComm::SendData(const uint8_t *data, size_t len)
{
	Sync();
	uart.Send(data, len);
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
	return true;
}
