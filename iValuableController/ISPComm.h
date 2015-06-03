#ifndef _ISP_COMM_H
#define _ISP_COMM_H

#include "ISerialComm.h"
#include "FastDelegate.h"

#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include <Driver_USART.h>

using namespace fastdelegate;

#define ISPCOMM_BUFFER_SIZE	0x200

class ISPComm : public ISerialComm
{
	protected:
		static const std::uint8_t dataReceHeader[2];
		static const std::uint8_t dataSendHeader[2];
		static boost::shared_ptr<ISPComm> instance;
		static void UARTCallback(uint32_t event);
	
		const ARM_DRIVER_USART &uart;
		bool isStarted;
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
		
		uint8_t buffer[ISPCOMM_BUFFER_SIZE];
		uint32_t head;
		volatile uint32_t tail;
		uint8_t processRound;
		volatile uint8_t readRound;
		
		StateType dataState;
		uint16_t checksum;
		uint8_t command;
		uint16_t parameterLen;
		uint16_t parameterIndex;
		boost::shared_ptr<uint8_t[]> parameters;
		uint8_t index;
		
		void Sync();
	  
	public:
		static boost::shared_ptr<ISPComm> &CreateInstance(ARM_DRIVER_USART &u);
		static boost::shared_ptr<ISPComm> &Instance() { return instance; }
	
		typedef FastDelegate3<std::uint8_t, std::uint8_t *, std::size_t> CommandArrivalHandler;
		CommandArrivalHandler OnCommandArrivalEvent;
		
		virtual ~ISPComm();
		virtual void Start() override;
		virtual void Stop() override;
		virtual void DataReceiver() override;
		virtual bool SendData(const std::uint8_t *data,std::size_t len) override;
		virtual bool SendData(std::uint8_t command, const std::uint8_t *data, std::size_t len) override;
};

#endif

