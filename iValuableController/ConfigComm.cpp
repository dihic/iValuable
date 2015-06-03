#include "ConfigComm.h"

using namespace std;

const std::uint8_t ConfigComm::dataHeader[5] = {0xfe, 0xfc, 0xfd, 0xfa, 0xfb};

boost::shared_ptr<ConfigComm> ConfigComm::instance;

boost::shared_ptr<ConfigComm> &ConfigComm::CreateInstance(ARM_DRIVER_USART &u)
{
	if (instance == nullptr || &instance->uart != &u)
		instance.reset(new ConfigComm(u));
	return instance;
}

extern "C"
{
	void ConfigComm::UARTCallback(uint32_t event)
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
			instance->uart.Receive(instance->buffer, CONFIG_BUFFER_SIZE);
		}
	}
}

ConfigComm::ConfigComm(ARM_DRIVER_USART &u)
	:uart(u), isStarted(false)
{
	uart.Initialize(UARTCallback);
	uart.PowerControl(ARM_POWER_FULL);
	uart.Control(ARM_USART_MODE_ASYNCHRONOUS |
							 ARM_USART_DATA_BITS_8 |
							 ARM_USART_PARITY_NONE |
							 ARM_USART_STOP_BITS_1 |
							 ARM_USART_FLOW_CONTROL_NONE, 115200);
}

ConfigComm::~ConfigComm()
{
	Stop();
	uart.Uninitialize();
}

void ConfigComm::Start()
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
	uart.Receive(buffer, CONFIG_BUFFER_SIZE);
	isStarted = true;
}
	
void ConfigComm::Stop()
{
	if (!isStarted)
		return;
	uart.Control(ARM_USART_CONTROL_TX, 0);
	uart.Control(ARM_USART_CONTROL_RX, 0);
	uart.Control(ARM_USART_ABORT_TRANSFER, 0);
	isStarted = false;
}

void ConfigComm::DataReceiver()
{
	if (!uart.GetStatus().rx_busy)
		return;
	
	tail = uart.GetRxCount();
	if (tail==CONFIG_BUFFER_SIZE)		//wait for next read to avoid misjudgment
		return;
	
	while (tail!=head)
	{
		switch (dataState)
		{
			case StateDelimiter1:
				if (buffer[head]==dataHeader[0])
					dataState = StateDelimiter2;
				break;
			case StateDelimiter2:
				if (buffer[head] == dataHeader[1])
					dataState = StateCommand;
				else if (buffer[head] == dataHeader[3])
				{
					checksum = dataHeader[0] + dataHeader[3];
					dataState = StateFileCommand;
				}
				else
					dataState = StateDelimiter1;
				break;
			case StateCommand:
				command = buffer[head];
				dataState = StateParameterLength;
				break;
			case StateParameterLength:
				parameterLen = buffer[head];
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
				break;
			case StateParameters:
				parameters[parameterIndex++] = buffer[head];
				if (parameterIndex >= parameterLen)
				{
					if (OnCommandArrivalEvent)
						OnCommandArrivalEvent(command,parameters.get(),parameterLen);
					parameters.reset();
					dataState = StateDelimiter1;
				}
				break;
			case StateFileCommand:
				checksum += (command = buffer[head]);
				lenIndex = 0;
				dataState = StateFileDataLength;
				parameterLen = 0;
				break;
			case StateFileDataLength:
				checksum += buffer[head];
				parameterLen |= (buffer[head]<<(lenIndex<<3));
				if (++lenIndex>1)
				{
					if (parameterLen == 0)
					{
						parameters.reset();
						dataState = StateChecksum;
					}
					else
					{
						parameters = boost::make_shared<uint8_t[]>(parameterLen);
						parameterIndex = 0;
						dataState = StateFileData;
					}
				}
				break;
			case StateFileData:
				checksum += (parameters[parameterIndex++] = buffer[head]);
				if (parameterIndex >= parameterLen)
					dataState = StateChecksum;
				break;
			case StateChecksum:
				if ((checksum == buffer[head]) && OnFileCommandArrivalEvent)
						OnFileCommandArrivalEvent(command,parameters.get(),parameterLen);
				parameters.reset();
				dataState = StateDelimiter1;
				return;
			default:
				break;
		}
		if (++head>=CONFIG_BUFFER_SIZE)
		{
			++processRound;
			head=0;
		}
	}
}

inline void ConfigComm::Sync()
{
	while (uart.GetStatus().tx_busy)
		__nop();
}

bool ConfigComm::SendData(const uint8_t *data, size_t len)
{
	uint8_t pre[3]={ dataHeader[0],dataHeader[2], static_cast<uint8_t>(len) };
	Sync();
	uart.Send(pre,3);
	Sync();
	uart.Send(data,len);
	return true;
}

bool ConfigComm::SendData(uint8_t command,const uint8_t *data,size_t len)
{
	uint8_t pre[4]={ dataHeader[0], dataHeader[2], command, static_cast<uint8_t>(len) };
	Sync();
	uart.Send(pre,4);
	if (len>0)
	{
		Sync();
		uart.Send(data,len);
	}
	return true;
}

bool ConfigComm::SendFileData(uint8_t command,const uint8_t *data, size_t len)
{
	if (len>0xffff)
		return false;
	uint8_t pre[5]={ dataHeader[0], dataHeader[4], command, 
									 static_cast<uint8_t>(len&0xff), static_cast<uint8_t>((len>>8)&0xff) };
	uint8_t checksum = 0;
	for(int i=0;i<5;i++)
		checksum += pre[i];
	for(int i=0;i<len;i++)
		checksum += data[i];
	Sync();
	uart.Send(pre, 5);
	if (len>0)
	{
		Sync();
		uart.Send(data, len);
	}
	Sync();
	uart.Send(&checksum, 1);
	return true;
}


