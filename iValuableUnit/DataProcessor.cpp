#include "DataProcessor.h"
#include "display.h"
#include "SuppliesDisplay.h"
#include <cmath>

using namespace std;

__align(16) std::uint8_t DataProcessor::MemBuffer[MEM_BUFSIZE];

DataProcessor *DataProcessor::Singleton = NULL;
DataProcessor::WriteNVHandler DataProcessor::WriteNV = NULL;

DataProcessor::DataProcessor()
{
	memset(currentAD, 0, SENSOR_NUM*sizeof(int32_t));
	memset(reinterpret_cast<void *>(MemBuffer), 0, MEM_BUFSIZE);
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		ScaleBasic *sb = reinterpret_cast<ScaleBasic *>(MemBuffer + ADDR_SCALE+ i*sizeof(ScaleBasic));
		pScales[i] = new ScaleInfo(i, sb);
	}
	for(int i=0; i<SUPPLIES_NUM; ++i)
	{
		pSupplies[i] = reinterpret_cast<SuppliesInfo *>(MemBuffer + ADDR_INFO+ i*sizeof(SuppliesInfo));
		pQuantity[i] = reinterpret_cast<int32_t *>(MemBuffer + ADDR_QUANTITY+ i*sizeof(int32_t));
	}
	
	pConfig = reinterpret_cast<ScaleAttribute *>(MemBuffer + ADDR_CONFIG);
	//Default values for YZC-133
	pConfig->Sensitivity = 1.0f;
	pConfig->TempDrift = 0.0002f;
	pConfig->ZeroRange = 0.1f;
	pConfig->SafeOverload = 1.2f;
	pConfig->MaxOverload = 1.5f;
	
	pCalWeight = reinterpret_cast<float *>(MemBuffer + ADDR_CAL_WEIGHT);
	pCurrentTemp = reinterpret_cast<float *>(MemBuffer + ADDR_TEMP);

#if UNIT_TYPE!=UNIT_TYPE_INDEPENDENT
	pTareSum = reinterpret_cast<float *>(MemBuffer + ADDR_TARE_SUM);
#endif
	
	pSensorEnable = reinterpret_cast<uint8_t *>(MemBuffer + ADDR_SENSOR_ENABLE);
	*pSensorEnable = 0x3f;	//Default all enable for channel 0-5
}

DataProcessor::~DataProcessor()
{
	for(int i=0; i<SENSOR_NUM; ++i)
		delete pScales[i];
}

//Parameter flags stands for enable to calibrate sensors bitwise
void DataProcessor::CalibrateSensors(uint8_t flags)
{
	for(uint8_t i=0;i<SENSOR_NUM;++i)
	{
		if ((flags & (1<<i)) == 0)
			continue;
		pScales[i]->Calibrate(*pCalWeight);
		if (WriteNV)
		{
			uint16_t base = ADDR_SCALE+ i*sizeof(ScaleBasic);
			WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
		}
	}
}

float DataProcessor::CalculateWeight(uint8_t ch, int32_t ad)
{
	if (ch>=SENSOR_NUM)
		return 0;
	currentAD[ch] = ad;
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
	return (abs(pScales[ch]->GetBasic()->Ramp)>0.000001f)?
		(ad - pScales[ch]->GetBasic()->Tare)/pScales[ch]->GetBasic()->Ramp: 0;
#else
	return (abs(pScales[ch]->GetBasic()->Ramp)>0.000001f)?
		(ad - pScales[ch]->GetBasic()->Zero)/pScales[ch]->GetBasic()->Ramp: 0;
#endif
}

bool DataProcessor::SensorEnable(std::uint8_t ch) const
{
	return (ch<SENSOR_NUM) ? (*pSensorEnable & (1<<ch))!=0 : false;
}

void DataProcessor::SetConfig(const std::uint8_t *buf)
{
	memcpy(reinterpret_cast<void *>(&pConfig->Sensitivity), buf, sizeof(float));
	memcpy(reinterpret_cast<void *>(&pConfig->TempDrift), buf+4, sizeof(float));
	memcpy(reinterpret_cast<void *>(&pConfig->ZeroRange), buf+8, sizeof(float));
	memcpy(reinterpret_cast<void *>(&pConfig->SafeOverload), buf+12, sizeof(float));
	memcpy(reinterpret_cast<void *>(&pConfig->MaxOverload), buf+16, sizeof(float));
	if (WriteNV)
		WriteNV(ADDR_CONFIG, reinterpret_cast<uint8_t *>(pConfig), sizeof(ScaleAttribute));
}

void DataProcessor::SetTemperature(float t)
{
	float delta = t-*pCurrentTemp;
	if (abs(delta)<0.01f)
		return;
	for(uint8_t i=0;i<SENSOR_NUM;++i)
	{
		if (SensorEnable(i))
		{
			pScales[i]->TemperatureCompensation(pConfig->TempDrift, delta);
			if (WriteNV)
			{
				uint16_t base = ADDR_SCALE+ i*sizeof(ScaleBasic);
				WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
			}
		}
	}
	*pCurrentTemp = t;
	if (WriteNV)
		WriteNV(ADDR_TEMP, reinterpret_cast<uint8_t *>(&t), sizeof(float));
}

void DataProcessor::SetRamp(uint8_t ch, float ramp)
{
	if (ch>=SENSOR_NUM)
		return;
	pScales[ch]->SetRamp(ramp);
	if (WriteNV)
	{
		uint16_t base = ADDR_SCALE+ ch*sizeof(ScaleBasic);
		WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
	}
}

float DataProcessor::GetRamp(uint8_t ch) const
{
	if (ch>=SENSOR_NUM)
		return 0;
	return pScales[ch]->GetBasic()->Ramp;
}

void DataProcessor::SetCalWeight(const std::uint8_t *buf)
{
	memcpy(pCalWeight, buf, sizeof(float));
	if (WriteNV)
		WriteNV(ADDR_CAL_WEIGHT, reinterpret_cast<uint8_t *>(pCalWeight), sizeof(float));
}

float DataProcessor::GetCalWeight() const
{
	return *pCalWeight;
}

void DataProcessor::SetZero(uint8_t ch, bool tare)
{
#if UNIT_TYPE==UNIT_TYPE_INDEPENDENT
	if (ch>=SENSOR_NUM)
		return;
	if (tare)
		pScales[ch]->SetTare();
	else
		pScales[ch]->SetZero();
	if (WriteNV)
	{
		uint16_t base = ADDR_SCALE+ ch*sizeof(ScaleBasic);
		WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
	}
#else
	if (tare)
	{
		float w = 0;
		//Ignore ch when taring
		for(uint8_t i=0;i<SENSOR_NUM;++i)
			if (SensorEnable(i))
			{
				if (abs(pScales[ch]->GetBasic()->Ramp)<0.000001f)
				{
					w = 0;
					break;
				}
				pScales[i]->SetTare();
				w += (pScales[i]->GetBasic()->Tare - pScales[i]->GetBasic()->Zero)/
							pScales[ch]->GetBasic()->Ramp;
			}
		*pTareSum = w;
		if (WriteNV)
		{
			WriteNV(ADDR_TARE_SUM, reinterpret_cast<uint8_t *>(pTareSum), sizeof(float));
			for(uint8_t i=0;i<SENSOR_NUM;++i)
				if (SensorEnable(i))
				{
					uint16_t base = ADDR_SCALE+ i*sizeof(ScaleBasic);
					WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
				}
		}
	}
	else
	{
		if (ch>=SENSOR_NUM)
			return;
		*pTareSum = 0;
		//When set zero for any channel, tare sum must be cleared!
		pScales[ch]->SetZero();
		if (WriteNV)
		{
			WriteNV(ADDR_TARE_SUM, reinterpret_cast<uint8_t *>(pTareSum), sizeof(float));
			uint16_t base = ADDR_SCALE+ ch*sizeof(ScaleBasic);
			WriteNV(base, MemBuffer+base, sizeof(ScaleBasic));
		}
	}
#endif
	
	
}

void DataProcessor::SetEnable(std::uint8_t en)
{
	*pSensorEnable = en;
	for(int i=0; i<SENSOR_NUM; ++i)
		if (SensorEnable(i))
			currentAD[i] = 0;
	if (WriteNV)
		WriteNV(ADDR_SENSOR_ENABLE, pSensorEnable, 1);
}

int DataProcessor::PrepareRaw(std::uint8_t *buf)
{
	buf[0] = 0;
	int base = 1;
	for(int i=0; i<SENSOR_NUM; ++i)
	{
		//Skip disabled sensors
		if (SensorEnable(i))
			continue;
		++buf[0];
		buf[base++] = i;
		memcpy(buf+base, reinterpret_cast<void *>(&pScales[i]->GetBasic()->Zero), sizeof(float));
		base += sizeof(float);
		memcpy(buf+base, reinterpret_cast<void *>(&pScales[i]->GetBasic()->Tare), sizeof(float));
		base += sizeof(float);
		memcpy(buf+base, &currentAD[i], sizeof(int32_t));
		base += sizeof(float);
	}
	return base;
}

bool DataProcessor::FindSuppliesId(std::uint64_t id, std::uint8_t &index) const
{
	for(int i=0;i<SUPPLIES_NUM;++i)
		if (pSupplies[i]->Uid == id)
		{
			index = i;
			return true;
		}
	return false;
}

bool DataProcessor::GetSuppliesUnit(std::uint8_t index, float &unit, float &deviation) const 
{ 
	if (pSupplies[index]->Uid == 0)
		return false;
	unit = pSupplies[index]->Unit; 
	deviation = pSupplies[index]->Deviation;
	return (abs(unit)>0.000001f);
}

bool DataProcessor::SetQuantity(std::uint8_t index, std::int32_t num)
{
	if (pSupplies[index]->Uid == 0)
		return false;
	*pQuantity[index] = num;
	if (WriteNV)
		WriteNV(ADDR_QUANTITY+index*sizeof(int32_t), reinterpret_cast<uint8_t *>(pSupplies[index]), sizeof(int32_t));
	return true;
}

float DataProcessor::CalculateInventoryWeight(float &min, float &max)
{
	float total = 0;
	min = max = 0;
	for(int i=0;i<SUPPLIES_NUM;++i)
		if (pSupplies[i]->Uid != 0)
		{
			total += pSupplies[i]->Unit**pQuantity[i];
			min += pSupplies[i]->Unit*(*pQuantity[i] - pSupplies[i]->Deviation);
			max += pSupplies[i]->Unit*(*pQuantity[i] + pSupplies[i]->Deviation);
		}
	return total;
}

void DataProcessor::SetSupplies(std::uint8_t index, const SuppliesInfo &info)
{
	memcpy(pSupplies[index], &info, sizeof(SuppliesInfo));
	if (WriteNV)
		WriteNV(ADDR_INFO+ index*sizeof(SuppliesInfo), reinterpret_cast<uint8_t *>(pSupplies[index]), sizeof(SuppliesInfo));
}

bool DataProcessor::AddSupplies(const SuppliesInfo &info)
{
	for(int i=0;i<SUPPLIES_NUM;++i)
		if (pSupplies[i]->Uid == 0)
		{
			SetSupplies(i, info);
			return true;
		}
	return false;
}

void DataProcessor::RemoveSupplies(std::uint8_t index)
{
	memset(pSupplies[index], 0 ,sizeof(SuppliesInfo));
	if (WriteNV)
		WriteNV(ADDR_INFO + index*sizeof(SuppliesInfo), reinterpret_cast<uint8_t *>(pSupplies[index]), sizeof(SuppliesInfo));
}

#define TEXT_POS_Y	5

bool DataProcessor::UpdateDisplay(bool force)
{
	static uint8_t index = 0;
	static const uint8_t *data = NULL;
	static uint8_t len = 0;
	
	if (force)
	{
		index = 0;
		data = NULL;
		len = 0;
	}

	uint16_t posY = TEXT_POS_Y;
	bool needUpdate = (index!=0);
	bool needClear = false;
	
	//Clear screen
	Display::ClearRegion(0, 0, RES_X, MAXLINE_Y_LIMIT);
	
	for(int i=index; i<SUPPLIES_NUM; ++i)
	{
		if (pSupplies[i]->Uid == 0)	//Skip empty
			continue;
		if (data == NULL)
		{
			data = SuppliesDisplay::GetString(i, len);		//Load name string
			if (len==0)		//Skip supplies without name string
			{
				data = NULL;
				continue;
			}
		}
		
		if (needClear)
		{
			index = i;
			data = NULL;
			len = 0;
			return true;
		}
		//Display string
		uint8_t end = Display::ShowFormatString(data, len, posY);
		if (end==0)		//Just completed but need to clear screen in next update
		{
			needClear = true;
			continue;
		}
		else if (end<len)
		{
			//Record current position for next display
			index = i;
			data = data+end;
			len -= end;
			return true;
		}
	}

	index = 0;
	data = NULL;
	len = 0;
	return needUpdate;
}
