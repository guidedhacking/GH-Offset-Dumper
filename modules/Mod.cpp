#include "stdafx.h"
#include "Mod.h"

ModIn::ModIn(TCHAR* modName)
{
	name = modName;
	Get();
}

ModIn::ModIn(TCHAR* modName, ProcIn& process)
{
	name = modName;
	proc = &process;
	Get();
}

ModEx::ModEx(TCHAR* modName) {}

ModEx::~ModEx()
{
	//CloseHandle(proc->handle);
}

ModEx::ModEx(TCHAR* modName, ProcEx& process)
{
	name = modName;
	proc = &process;
	if (!proc->handle)
	{
		proc->Get();
	}
	this->Get();
}

bool ModEx::Get()
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, proc->procEntry.th32ProcessID);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (!_tcsicmp(modEntry.szModule, name))
				{
					CloseHandle(hSnap);
					return true;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}

	CloseHandle(hSnap);
	modEntry = {};
	return false;
}

bool ModIn::Get()
{
	return GetLDREntry();
}

bool ModIn::GetLDREntry()
{
	proc->GetPEB();

	LIST_ENTRY head = proc->peb->Ldr->InMemoryOrderModuleList;

	LIST_ENTRY curr = head;

	while (curr.Flink != head.Blink)
	{
		RFW_LDR_DATA_TABLE_ENTRY* mod = (RFW_LDR_DATA_TABLE_ENTRY*)CONTAINING_RECORD(curr.Flink, RFW_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

		if (mod->FullDllName.Buffer)
		{
			char* cName = TO_CHAR(mod->BaseDllName.Buffer);

			if (_tcsicmp(cName, name) == 0)
			{
				ldr = *mod;
				return true;
			}
			delete[] cName;
		}
		curr = *curr.Flink;
	}
	return false;
}

IMod::~IMod() {}