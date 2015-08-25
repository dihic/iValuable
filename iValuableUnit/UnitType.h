#ifndef _UNIT_TYPE_H
#define _UNIT_TYPE_H

#define UNIT_TYPE_INDEPENDENT		0x80
#define UNIT_TYPE_UNITY					0x81
#define UNIT_TYPE_UNITY_RFID		0x82
#define UNIT_TYPE_LOCKER   0x83

#ifdef UNIT_TYPE
#if (UNIT_TYPE != UNIT_TYPE_INDEPENDENT &&	\
		UNIT_TYPE != UNIT_TYPE_UNITY &&	\
		UNIT_TYPE != UNIT_TYPE_UNITY_RFID && \
		UNIT_TYPE != UNIT_TYPE_LOCKER)
#error "Invalid UNIT_TYPE!"		
#endif
#else
#error "Undefined UNIT_TYPE!"
#endif

#define SUPPLIES_NUM	10

#if UNIT_TYPE==UNIT_TYPE_UNITY_RFID
#define SENSOR_NUM	4
#else
#define SENSOR_NUM	6
#endif

#ifndef FW_VERSION_MAJOR
#error "Undefined FW_VERSION_MAJOR!"
#endif

#ifndef FW_VERSION_MINOR
#error "Undefined FW_VERSION_MINOR!"
#endif

#endif
