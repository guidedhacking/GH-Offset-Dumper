#include "stdafx.h"
#include "ProcEx.h"

ProcEx::ProcEx() {}

ProcEx::~ProcEx()
{
	//procex proc = procEx() will close the handle = bad
	//CloseHandle(handle);
}

//Get ProcEx by name
bool ProcEx::Get()
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		procEntry.dwSize = sizeof(procEntry);

		if (Process32First(hSnapshot, &procEntry))
		{
			do
			{
				if (!_tcscmp(procEntry.szExeFile, this->name))
				{
					CloseHandle(hSnapshot);
					return true;
				}
			} while (Process32Next(hSnapshot, &procEntry));
		}
	}
	CloseHandle(hSnapshot);
	procEntry = {};
	return false;
}

ProcEx::ProcEx(TCHAR* exeName)
{
	this->name = exeName;
	Get();
	Attach();
	GetPEB();
	//exeMod = ModEx(exeName);
	//exeMod = new ModEx(exeName);
}

bool ProcEx::Attach()
{
	handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procEntry.th32ProcessID);

	if (handle)
	{
		return true;
	}
	else return false;
}

bool ProcEx::GetPEB()
{
	if (!NtQueryInfoProc) ImportNTQueryInfo();

	if (NtQueryInfoProc)
	{
		NTSTATUS status = NtQueryInfoProc(handle, ProcessBasicInformation, &pbi, sizeof(pbi), 0);
		if (NT_SUCCESS(status))
		{
			ReadProcessMemory(handle, pbi.PebBaseAddress, &peb, sizeof(peb), 0);
			return true;
		}
	}
	return false;
}

bool SetDebugPrivilege(bool Enable)
{
	HANDLE hToken{ nullptr };
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
		return false;

	TOKEN_PRIVILEGES TokenPrivileges{};
	TokenPrivileges.PrivilegeCount = 1;
	TokenPrivileges.Privileges[0].Attributes = Enable ? SE_PRIVILEGE_ENABLED : 0;

	if (!LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &TokenPrivileges.Privileges[0].Luid))
	{
		CloseHandle(hToken);
		return false;
	}

	if (!AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
	{
		CloseHandle(hToken);
		return false;
	}

	CloseHandle(hToken);

	return true;
}