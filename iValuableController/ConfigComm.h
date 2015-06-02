#ifndef _CONFIG_COMM_H
#define _CONFIG_COMM_H

#include "ISerialComm.h"
#include "FastDelegate.h"

#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include <Driver_USART.h>

using namespace fastdelegate;

#define CONFIG_BUFFER_SIZE	0x400

class ConfigComm : public ISerialComm
{
	protected:
		static const std::uint8_t dataHeader[5];
		static boost::shared_ptr<ConfigComm> instance;
		static void UARTCallback(uint32_t event);
	
		const ARM_DRIVER_USART &uart;
		ConfigComm(ARM_DRIVER_USART &u);
		bool isStarted;
	private:
		
		enum StateType
		{
			StateDelimiter1,
			StateDelimiter2,
			StateCommand,
			StateParameterLength,
			StateParameters,
			StateFileCommand,
			StateFileDataLength,
			StateFileData,
			StateChecksum
		};
		
		uint8_t buffer[CONFIG_BUFFER_SIZE];
		uint32_t head;
		volatile uint32_t tail;
		uint8_t processRound;
		volatile uint8_t readRound;
		
		StateType dataState;
		uint8_t checksum;
		uint8_t lenIndex;
		uint8_t command;
		uint16_t parameterLen;
		uint16_t parameterIndex;
		boost::shared_ptr<uint8_t[]> parameters;
		
		void Sync();
	
	public:
		static boost::shared_ptr<ConfigComm> &CreateInstance(ARM_DRIVER_USART &u);
		static boost::shared_ptr<ConfigComm> &Instance() { return instance; }
	
		typedef FastDelegate3<std::uint8_t, std::uint8_t *, std::size_t> CommandArrivalHandler;
		CommandArrivalHandler OnCommandArrivalEvent;
		CommandArrivalHandler OnFileCommandArrivalEvent;
		
		virtual ~ConfigComm();
		virtual void Start() override;
		virtual void Stop() override;
		virtual void DataReceiver() override;
		virtual bool SendData(const std::uint8_t *data,std::size_t len) override;
		virtual bool SendData(std::uint8_t command, const std::uint8_t *data,std::size_t len) override;
		bool SendFileData(uint8_t command,const uint8_t *data, size_t len);
};

#endif

