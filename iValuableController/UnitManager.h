#ifndef _UNIT_MANAGER_H
#define _UNIT_MANAGER_H

#include <map>
#include <cstdint>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include "CanDevice.h"
#include "FastDelegate.h"

using namespace fastdelegate;

namespace IntelliStorage
{
	class CardListItem
  {
		public:
			CardListItem() {}
			~CardListItem() {}
      std::uint8_t Direction;
			std::uint8_t IndicatorData;
	};
	
	class UnitManager
	{
		private:
			std::map<std::uint16_t, boost::shared_ptr<CanDevice> > unitList;
//			std::map<std::string, boost::shared_ptr<CardListItem> > cardList;
			
		public:
			typedef std::map<std::uint16_t, boost::shared_ptr<CanDevice> >::iterator UnitIterator;
			typedef std::map<std::string, boost::shared_ptr<CardListItem> >::iterator CardIterator;
//			typedef FastDelegate1<boost::shared_ptr<StorageUnit> > SendDataCB;
//			SendDataCB OnSendData;
			UnitManager() {}
			~UnitManager() {}
			void Add(std::uint16_t id, boost::shared_ptr<CanDevice> unit);
			std::map<std::uint16_t, boost::shared_ptr<CanDevice> > &GetList() { return unitList; }
			
			void UpdateLatest(boost::shared_ptr<CanDevice> &unit);
			boost::shared_ptr<CanDevice> FindUnit(const std::string &cardId);
			boost::shared_ptr<CanDevice> FindUnit(std::uint16_t id);
			
//			bool SetLEDState(const std::string &cardId, std::uint8_t dir, std::uint8_t color, std::uint8_t status);
//			bool SetLEDState(const std::string &cardId, std::uint8_t color, std::uint8_t status);
//			
//			bool ClearLEDState(const std::string &cardId);
//			bool RemoveLEDState(const std::string &cardId, bool keepShown);
//			void RemoveAllLEDState();
//			bool GetLEDState(const std::string &cardId, std::uint8_t &data);
//			void RemoveSide(std::uint8_t dir);
	};
	
}

#endif
