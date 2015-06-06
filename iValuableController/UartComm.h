#ifndef _UART_COMM_H
#define _UART_COMM_H

#include "ISerialComm.h"
#include "FastDelegate.h"

#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include <Driver_USART.h>
#include <queue>

using namespace fastdelegate;

class UartComm : public ISerialComm
{
	protected:
		static void WorkThread(void const *arg);
	
		const ARM_DRIVER_USART &uart;
		const std::size_t bufferSize;
		
		osThreadDef_t WorkThreadDef;
		osThreadId tid = NULL;
	
		bool isStarted = false;
		std::queue<boost::shared_ptr<uint8_t[]> > messageQueue;
		boost::shared_ptr<uint8_t[]> buffer;
		
		uint32_t head = 0;
		uint32_t tail = 0;
		
		UartComm(ARM_DRIVER_USART &u, ARM_USART_SignalEvent_t cb_event, std::uint32_t baudrate, std::size_t bufSize);
		void Sync();
	private:
		boost::shared_ptr<uint8_t[]> overflowBuffer;
	public:	
		typedef FastDelegate3<std::uint8_t, std::uint8_t *, std::size_t> CommandArrivalHandler;
		CommandArrivalHandler OnCommandArrivalEvent;
		
		virtual ~UartComm();
		virtual void Start() override;
		virtual void Stop() override;
		virtual void DataReceiver() override;
		virtual void DataProcess(std::uint8_t byte) =0;
		virtual void PostOverflow() =0;
};

#endif

