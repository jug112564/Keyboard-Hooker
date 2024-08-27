#pragma once
#ifndef _APPINTERFACE_H_
#define _APPINTERFACE_H_

#define IOCTL_KEYBOARDHHOOKER_GET_LOG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARDHOOKER_CLEAR_LOG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _RECORD_LIST_APP
{
	//about record
	ULONG Length;
	ULONG SequenceNumber;
	unsigned char MajorFunction;
	BOOLEAN bIsStatus;

	//about status
	ULONG_PTR requsetOffset;
	ULONG requsetLength;

	//about keyboard data
	KEYBOARD_INPUT_DATA keyData;
} RECORD_LIST_APP, * PRECORD_LIST_APP;

typedef struct _RECORD_LIST {
	LIST_ENTRY List;
	RECORD_LIST_APP ListData;
} RECORD_LIST, *PRECORD_LIST;


#endif