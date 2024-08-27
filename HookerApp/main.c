#include <windows.h>
#include <stdio.h>
#include <winioctl.h>
#include <ntddkbd.h>
#include "../KeyboardHooker/AppInterface.h"

//#define DETAIL //세부 정보를 보고 싶으면 주석 해제

char* GetUserInput(int makecode) {
    switch (makecode) {
    case 0x01: return "ESC";
    case 0x02: return "1";
    case 0x03: return "2";
    case 0x04: return "3";
    case 0x05: return "4";
    case 0x06: return "5";
    case 0x07: return "6";
    case 0x08: return "7";
    case 0x09: return "8";
    case 0x0A: return "9";
    case 0x0B: return "0";
    case 0x0C: return "-";
    case 0x0D: return "=";
    case 0x0E: return "Backspace";
    case 0x0F: return "Tab";
    case 0x10: return "q";
    case 0x11: return "w";
    case 0x12: return "e";
    case 0x13: return "r";
    case 0x14: return "t";
    case 0x15: return "y";
    case 0x16: return "u";
    case 0x17: return "i";
    case 0x18: return "o";
    case 0x19: return "p";
    case 0x1A: return "[";
    case 0x1B: return "]";
    case 0x1C: return "Enter";
    case 0x1D: return "Left Control";
    case 0x1E: return "a";
    case 0x1F: return "s";
    case 0x20: return "d";
    case 0x21: return "f";
    case 0x22: return "g";
    case 0x23: return "h";
    case 0x24: return "j";
    case 0x25: return "k";
    case 0x26: return "l";
    case 0x27: return ";";
    case 0x28: return "'";
    case 0x29: return "`";
    case 0x2A: return "Left Shift";
    case 0x2B: return "\\";
    case 0x2C: return "z";
    case 0x2D: return "x";
    case 0x2E: return "c";
    case 0x2F: return "v";
    case 0x30: return "b";
    case 0x31: return "n";
    case 0x32: return "m";
    case 0x33: return ",";
    case 0x34: return ".";
    case 0x35: return "/";
    case 0x36: return "Right Shift";
    case 0x37: return "KP *";
    case 0x38: return "Left Alt";
    case 0x39: return "Space";
    case 0x3A: return "Caps Lock";
    case 0x3B: return "F1";
    case 0x3C: return "F2";
    case 0x3D: return "F3";
    case 0x3E: return "F4";
    case 0x3F: return "F5";
    case 0x40: return "F6";
    case 0x41: return "F7";
    case 0x42: return "F8";
    case 0x43: return "F9";
    case 0x44: return "F10";
    case 0x45: return "Num Lock";
    case 0x46: return "Scroll Lock";
    case 0x47: return "Home";
    case 0x48: return "Up Arrow";
    case 0x49: return "Page Up";
    case 0x4A: return "KP -";
    case 0x4B: return "Left Arrow";
    case 0x4C: return "5 (KP)";
    case 0x4D: return "Right Arrow";
    case 0x4E: return "KP +";
    case 0x4F: return "End";
    case 0x50: return "Down Arrow";
    case 0x51: return "Page Down";
    case 0x52: return "Insert";
    case 0x53: return "Delete";
    case 0x54: return "(KP) 0";
    case 0x55: return "(KP) .";
    case 0x57: return "F11";
    case 0x58: return "F12";
    case 0x5B: return "Windows";
    case 0x5C: return "Windows";
    case 0x5D: return "Applications";
    case 0x60: return "KP /";
    case 0xBA: return ",";
    case 0xBB: return "=";
    case 0xBC: return ".";
    case 0xBD: return "-";
    case 0xBE: return "/";
    case 0xBF: return "`";
    default: return "unknown";
    }
}


int main(void)
{
	HANDLE hDevice;
	ULONG dwRet;
	PRECORD_LIST_APP pLogData = NULL;
	BOOLEAN bRet = FALSE;

	hDevice = CreateFile(L"\\\\.\\SampleKeyboardHookerLink",
		FILE_READ_DATA | FILE_WRITE_DATA,
		NULL, NULL, OPEN_EXISTING, 0, NULL);

	if (hDevice == (HANDLE)-1)
	{
		DWORD errorCode = GetLastError();

		printf("Invalid device handle[%d]\n", errorCode);
		Sleep(10000);
		return 0;
	}

	// 현재까지의 로그를 지운다
	DeviceIoControl(hDevice, IOCTL_KEYBOARDHOOKER_CLEAR_LOG, NULL, 0, NULL, 0, &dwRet, NULL);

	pLogData = (PRECORD_LIST_APP)malloc(sizeof(RECORD_LIST_APP));

	// 로그를 가져온다
#define POLLINGTIME_MSEC	(100) // 100ms

	while (1)
	{
		Sleep(POLLINGTIME_MSEC);
		dwRet = 0;
		memset(pLogData, 0, sizeof(RECORD_LIST_APP));
		bRet = DeviceIoControl(hDevice, IOCTL_KEYBOARDHHOOKER_GET_LOG, NULL, 0, pLogData, sizeof(RECORD_LIST_APP), &dwRet, NULL);
		if (bRet == FALSE)
			break;

		if (dwRet == 0)
			continue;

		// 가져온 로그를 출력한다
		switch (pLogData->MajorFunction)
		{
		case 0x03:  //IRP_MJ_READ case
			if (pLogData->bIsStatus == TRUE)
			{
#ifdef  DETAIL
				printf("<Keyboard information>\n");
				printf("ExtraInformation : %d\n", pLogData->keyData.ExtraInformation);
				printf("Flags : %d\n", pLogData->keyData.Flags);
				printf("MakeCode : %d\n", pLogData->keyData.MakeCode);
				printf("Reserved : %d\n", pLogData->keyData.Reserved);
				printf("UnitId : %d\n", pLogData->keyData.UnitId);
				printf("UserValue : %s\n\n", GetUserInput(pLogData->keyData.MakeCode));
#else
                if(pLogData->keyData.Flags == 0)
                    printf("Inputed Value : %s\n", GetUserInput(pLogData->keyData.MakeCode));
#endif
			}
			break;
		}
	}
	CloseHandle(hDevice);

	return 0;
}