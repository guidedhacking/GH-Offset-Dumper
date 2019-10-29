#include "stdafx.h"
#include "patternscan.h"
//TODO Finish Internal Scans

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

//Internal Pattern Scan
//Should match Ex::Scan logic, any changes here, change there too
char* Pattern::In::Scan(char* pattern, char* mask, char* begin, intptr_t size)
{
	char* match{ nullptr };
	DWORD oldprotect = 0;
	MEMORY_BASIC_INFORMATION mbi{};

	for (char* curr = begin; curr < begin + size; curr += mbi.RegionSize)
	{
		if (!VirtualQuery(curr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) continue;

		if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
		{
			match = ScanBasic(pattern, mask, curr, mbi.RegionSize);

			VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);

			if (match != nullptr)
			{
				break;
			}
		}
	}
	return match;
}

std::vector<char*> Pattern::In::ScanMulti(char* pattern, char* mask, char* begin, intptr_t size)
{
	std::vector<char*> matches;
	intptr_t patternLen = strlen(mask);

	for (int i = 0; i < size; i++)
	{
		bool found = true;
		for (int j = 0; j < patternLen; j++)
		{
			if (mask[j] != '?' && pattern[j] != *(begin + i + j))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			matches.push_back(begin + i);
		}
	}
	return matches;
}

char* Pattern::In::ScanMod(char* pattern, char* mask, IMod& mod)
{
	mod.GetLDREntry();

	char* match = Pattern::In::Scan(pattern, mask, (char*)mod.ldr.DllBase, mod.ldr.SizeOfImage);

	return match;
}

//scans only code in modules, when the f would this be necessary...
char* Pattern::In::ScanAllMods(char* pattern, char* mask, ProcIn& proc)
{
	LIST_ENTRY head = proc.peb->Ldr->InMemoryOrderModuleList;
	LIST_ENTRY curr = head;

	while (curr.Flink != head.Blink)
	{
		RFW_LDR_DATA_TABLE_ENTRY* mod = (RFW_LDR_DATA_TABLE_ENTRY*)curr.Flink;

		char* cName = TO_CHAR(mod->BaseDllName.Buffer);//

		ModIn currMod(cName, proc);

		delete[] cName;

		char* match = In::ScanMod(pattern, mask, currMod);

		if (match)
		{
			return match;
		}
		curr = *curr.Flink;
	}
	return nullptr;
}

char* Pattern::In::ScanProc(char* pattern, char* mask)
{
	unsigned long long int kernelMemory = (sizeof(char*) == 4) ? 0x80000000 : 0x800000000000;

	return Scan(pattern, mask, 0x0, (intptr_t)kernelMemory); //returns nullptr if no match
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

//TODO: Fix issue with missing patterns that bridge regions
std::vector<char*> Pattern::In::ScanProcMulti(char* pattern, char* mask)
{
	std::vector<char*> matches;

	DWORD oldprotect;
	MEMORY_BASIC_INFORMATION mbi{};

	//GetCurrentModule filename
	TCHAR szCallingModule[MAX_PATH]{};
	char* callingModuleAllocationBase{ nullptr };

	if (VirtualQuery(&ModuleFinder, &mbi, sizeof(mbi)))
	{
		callingModuleAllocationBase = (char*)mbi.AllocationBase;
		GetModuleFileName((HMODULE)(mbi.AllocationBase), szCallingModule, MAX_PATH);
	}

	unsigned long long int kernelMemory = (sizeof(char*) == 4) ? 0x80000000 : 0x800000000000;

	for (char* curr = 0; curr < (char*)kernelMemory; curr += mbi.RegionSize)
	{
		//if (!VirtualQuery(curr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Protect & PAGE_NOACCESS) continue; ?
		if (!VirtualQuery(curr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) continue;

		//exclude current module
		if (mbi.AllocationBase == callingModuleAllocationBase)
		{
			continue;
		}

		if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
		{
			std::vector<char*> match = In::ScanMulti(pattern, mask, (char*)mbi.BaseAddress, mbi.RegionSize);
			VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);

			if (!match.empty())
			{
				for (auto m : match)
				{
					matches.push_back(m);
				}
			}
		}
	}
	return matches;
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

//combo
char* Pattern::In::ScanMod(char* combopattern, ModIn& mod)
{
	char pattern[100]{};
	char mask[100]{};
	Pattern::Parse(combopattern, pattern, mask);
	return Pattern::In::ScanMod(pattern, mask, mod);
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