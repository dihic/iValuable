#include "StorageUnit.h"
#include "System.h"
#include "stm32f4xx_hal_conf.h" 
#include <cstring>

using namespace std;

namespace IntelliStorage
{

	StorageUnit::StorageUnit(CANExtended::CanEx &ex, uint16_t id)
		:	CanDevice(ex, id), lastCardType(0), cardChanged(false)
	{
		cardState = 0;
		cardId.clear();
		presId.clear();
	}

	void StorageUnit::UpdateCard()
	{
		cardChanged = false;
		if (cardState == CardLeft)
		{
			cardId.clear();
			presId.clear();
		}
	}
	
	string StorageUnit::GenerateId(const uint8_t *id, size_t len)
	{
		string result;
		char temp;
		for(int i=len-1;i>=0;--i)
		{
			temp = (id[i] & 0xf0) >>4;
			temp += (temp>9 ? 55 : 48);
			result+=temp;
			temp = id[i] & 0x0f;
			temp += (temp>9 ? 55 : 48);
			result+=temp;
		}
		return result;
	}
	
	void StorageUnit::SetNotice(uint8_t level)
	{
		boost::shared_ptr<std::uint8_t[]> data = boost::make_shared<std::uint8_t[]>(1);
		data[0]=level;
		WriteAttribute(DeviceAttribute::Notice, data, 1);
	}

	void StorageUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> entry)
	{
		CanDevice::ProcessRecievedEvent(entry);
		//Notice device that host has got the process data
		canex.Sync(DeviceId, SYNC_GOTCHA, CANExtended::Trigger);
		std::uint8_t *rawData = entry->GetVal().get();
		string id;
		if (rawData[0]!=0)
			id = GenerateId(rawData+1, 8);	//Generate temporary rfid id when got one
		if (lastCardType==rawData[0] && cardId==id) //identical
			return;
		lastCardType = rawData[0];
		switch (rawData[0])
		{
			case 0:
				cardState = CardLeft;
				break;
			case 1:
				cardState = CardArrival;
				cardId = id;
				presId.clear();
				break;
			case 2:
				cardState = CardArrival;
				cardId = id;
				presId.clear();
				presId.append(reinterpret_cast<char *>(rawData+10), rawData[9]);
				break;
			default:
				break;
		}
		cardChanged = true;
	}
}

