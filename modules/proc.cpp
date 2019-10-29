#include "stdafx.h"
#include "proc.h"
#include "rfw_ntapi.h"

IProc::~IProc()
{
	//if (handle != nullptr)
	//CloseHandle(handle);
}

bool IsWow64Proc(HANDLE hProc)
{
	BOOL bWow64;
	IsWow64Process(hProc, &bWow64);
	return bWow64;
}

//basic function for quick projects
DWORD GetProcId(const TCHAR* procName)
{
	DWORD procId = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);

		if (Process32First(hSnap, &procEntry))
		{
			do
			{
				if (!_tcsicmp(procEntry.szExeFile, procName))
				{
					procId = procEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnap, &procEntry));

		}
	}
	CloseHandle(hSnap);
	return procId;
}

char* GetModuleBaseAddress(const TCHAR* modName, DWORD procId)
{
	char* modBaseAddr{ nullptr };
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry{};
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (!_tcsicmp(modEntry.szModule, modName))
				{
					modBaseAddr = (char*)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}