#ifndef _ISP_COMM_H
#define _ISP_COMM_H

#include "UartComm.h"

using namespace fastdelegate;

#define ISPCOMM_BUFFER_SIZE	0x400

class ISPComm : public UartComm
{
	protected:
		static const std::uint8_t dataReceHeader[2];
		static const std::uint8_t dataSendHeader[2];
		static boost::shared_ptr<ISPComm> instance;
		static void UARTCallback(uint32_t event);
		ISPComm(ARM_DRIVER_USART &u);
	private:
		
		enum StateType
		{
			StateDelimiter1,
			StateDelimiter2,
			StateCommand,
			StateParameterLength,
			StateParameters,
			StateChecksum,
		};
		
		volatile StateType dataState = StateDelimiter1;
		uint16_t checksum;
		uint8_t command;
		uint16_t parameterLen;
		uint16_t parameterIndex;
		boost::shared_ptr<uint8_t[]> parameters;
		uint8_t index;
		
	public:
		static boost::shared_ptr<ISPComm> &CreateInstance(ARM_DRIVER_USART &u);
		static boost::shared_ptr<ISPComm> &Instance() { return instance; }
	
		typedef FastDelegate3<std::uint8_t, std::uint8_t *, std::size_t> CommandArrivalHandler;
		CommandArrivalHandler OnCommandArrivalEvent;
		
		virtual ~ISPComm();

		virtual void DataProcess(std::uint8_t byte) override;
		virtual bool SendData(const std::uint8_t *data,std::size_t len) override;
		virtual bool SendData(std::uint8_t command, const std::uint8_t *data, std::size_t len) override;
};

#endif

