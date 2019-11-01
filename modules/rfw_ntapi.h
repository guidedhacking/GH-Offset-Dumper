#pragma once

#include <windows.h>
#include <winternl.h>
#include "rfw_ntdll.h"
#include "util.h"

typedef NTSTATUS(__stdcall* tNtQueryInformationProcess)
(
	HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcInformationClass,
	PVOID ProcInformation,
	ULONG ProcInformationLength,
	PULONG ReturnLength
	);

tNtQueryInformationProcess ImportNTQueryInfo();

PEB* GetPEB();

//RFW_LDR_DATA_TABLE_ENTRY GetLDREntryIn(TCHAR* modName);

//uintptr_t RFW_GetModuleHandle(TCHAR* modName);