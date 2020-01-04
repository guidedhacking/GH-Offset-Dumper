#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <stdlib.h>
#include <vector>
#include <intrin.h>
#include "RFW_ntapi.h"

extern tNtQueryInformationProcess NtQueryInfoProc;

class IMod;

class IProc
{
public:
	TCHAR* name{ nullptr };
	PROCESSENTRY32 procEntry{};
	HANDLE handle{ nullptr }; //optional in internal

	~IProc();

	virtual bool Get() = 0;
};

//basic functions
DWORD GetProcId(const TCHAR* procName);
char* GetModuleBaseAddress(const TCHAR* modName, DWORD procId);
bool IsWow64Proc(HANDLE hProc);
bool SetDebugPrivilege(bool Enable);