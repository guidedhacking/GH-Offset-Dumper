#include "stdafx.h"
#include "patternscan.h"

constexpr int ModuleFinder = 1337;

//Splits combo pattern into mask/pattern, accepts all wildcard variations
void Pattern::Parse(char* combo, char* pattern, char* mask)
{
	char lastChar = ' ';
	int j = 0;

	for (unsigned int i = 0; i < strlen(combo); i++)
	{
		if ((combo[i] == '?' || combo[i] == '*') && (lastChar != '?' && lastChar != '*'))
		{
			pattern[j] = mask[j] = '?';
			j++;
		}

		else if (isspace(lastChar))
		{
			pattern[j] = lastChar = (char)strtol(&combo[i], 0, 16);
			mask[j] = 'x';
			j++;
		}
		lastChar = combo[i];
	}
	pattern[j] = mask[j] = '\0';
}

//Basic Scan
char* Pattern::ScanBasic(char* pattern, char* mask, char* begin, intptr_t size)
{
	intptr_t patternLen = strlen(mask);

	//for (int i = 0; i < size - patternLen; i++)
	for (int i = 0; i < size; i++)
	{
		bool found = true;
		for (int j = 0; j < patternLen; j++)
		{
			if (mask[j] != '?' && pattern[j] != *(char*)((intptr_t)begin + i + j))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			return (begin + i);
		}
	}
	return nullptr;
}

HMODULE GetModuleNameByAddress(char* addr)
{
	MEMORY_BASIC_INFORMATION mbi{};
	TCHAR szPath[MAX_PATH + 1]{};
	if (VirtualQuery(addr, &mbi, sizeof(mbi)))
	{
		GetModuleFileName((HMODULE)(mbi.AllocationBase), szPath, MAX_PATH);
	}

	return (HMODULE)mbi.AllocationBase;
}

//External Wrapper
//TODO: Fix issue with missing patterns that bridge regions
char* Pattern::Ex::Scan(char* pattern, char* mask, char* begin, intptr_t size, HANDLE hProc)
{
	char* match{ nullptr };
	SIZE_T bytesRead;
	DWORD oldprotect;
	char* buffer{ nullptr };
	MEMORY_BASIC_INFORMATION mbi;
	mbi.RegionSize = 0x1000;//

	VirtualQueryEx(hProc, (LPCVOID)begin, &mbi, sizeof(mbi));

	for (char* curr = begin; curr < begin + size; curr += mbi.RegionSize)
	{
		if (!VirtualQueryEx(hProc, curr, &mbi, sizeof(mbi))) continue;
		if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) continue;

		delete[] buffer;
		buffer = new char[mbi.RegionSize];

		if (VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
		{
			ReadProcessMemory(hProc, mbi.BaseAddress, buffer, mbi.RegionSize, &bytesRead);
			VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);

			char* internalAddr = ScanBasic(pattern, mask, buffer, (intptr_t)bytesRead);

			if (internalAddr != nullptr)
			{
				//calculate from internal to external
				match = curr + (internalAddr - buffer);
				break;
			}
		}
	}
	delete[] buffer;
	return match;
}

//Module wrapper for external pattern scan
char* Pattern::Ex::ScanMod(char* pattern, char* mask, ModEx& mod)
{
	return Scan(pattern, mask, (char*)mod.modEntry.modBaseAddr, mod.modEntry.modBaseSize, mod.proc->handle);
}

//combo
char* Pattern::Ex::ScanMod(char* combopattern, ModEx& mod)
{
	char pattern[100]{};
	char mask[100]{};
	Pattern::Parse(combopattern, pattern, mask);
	return Pattern::Ex::ScanMod(pattern, mask, mod);
}

//Loops through all modules and scans them
char* Pattern::Ex::ScanAllMods(char* pattern, char* mask, ProcEx& proc)
{
	MODULEENTRY32 modEntry{};
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, proc.procEntry.th32ProcessID);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnapshot, &modEntry))
		{
			do
			{
				char* match = Scan(pattern, mask, (char*)modEntry.modBaseAddr, modEntry.modBaseSize, proc.handle);
				if (match != nullptr)
				{
					CloseHandle(hSnapshot);
					return match;
				}
			} while (Module32Next(hSnapshot, &modEntry));
		}
		CloseHandle(hSnapshot);
	}
	return nullptr;
}

char* Pattern::Ex::ScanProc(char* pattern, char* mask, ProcEx& proc)
{
	//TODO: this should only scan good memory like the other one

	unsigned long long int kernelMemory = IsWow64Proc(proc.handle) ? 0x80000000 : 0x800000000000;

	return Scan(pattern, mask, 0x0, (intptr_t)kernelMemory, proc.handle);
}