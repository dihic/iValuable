#include "RfidUnit.h"
#include "System.h"

using namespace std;

namespace IntelliStorage
{	

	RfidUnit::RfidUnit(StorageBasic &basic)
		: UnityUnit(UNIT_TYPE_UNITY_RFID, basic)
	{
	}
	
	void RfidUnit::AfterUpdate()
	{
		cardChanged = false;
		if (cardState == CardLeft)
			cardId.clear();
	}
	
	string RfidUnit::GenerateId(const uint8_t *id, size_t len)
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
	
	void RfidUnit::ProcessRecievedEvent(boost::shared_ptr<CANExtended::OdEntry> &entry)
	{
		const DeviceSync syncIndex = static_cast<DeviceSync>(entry->Index);
		if (syncIndex == DeviceSync::SyncRfid)
		{
			//Notice device that host has got the process data
			//canex.Sync(DeviceId, DeviceSync::Gotcha, CANExtended::Trigger);
			auto rawData = entry->GetVal().get();
			string id = GenerateId(rawData+1, 8);	//Generate temporary rfid id when got one
			if (lastCardType==(RfidType)rawData[0] && cardId==id) //identical
				return;
			lastCardType = (RfidType)rawData[0];
			switch (lastCardType)
			{
				case RfidNone:
					cardState = CardLeft;
					break;
				case RfidAvailable:
				case RfidExpanded:
					cardState = CardArrival;
					cardId = id;
					break;
				default:
					break;
			}
			cardChanged = true;
#ifdef DEBUG_PRINT
			cout<<"Rfid Card "<<(cardState==CardArrival?"Arrival":"Left")<<" ID: "<<id<<endl;
#endif
		}
		else
			UnityUnit::ProcessRecievedEvent(entry);
	}
}

