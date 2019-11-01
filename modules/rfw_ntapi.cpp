#include "stdafx.h"
#include "rfw_ntapi.h"

tNtQueryInformationProcess NtQueryInfoProc{ nullptr };

tNtQueryInformationProcess ImportNTQueryInfo()
{
	NtQueryInfoProc = (tNtQueryInformationProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationProcess");

	if (NtQueryInfoProc)
	{
		return NtQueryInfoProc;
	}
	else return false;
}

//redundant, already included in IProc
PEB* GetPEB()
{
#ifdef _WIN64
	PEB* peb = (PEB*)__readgsword(0x60); //64bit

#else
	PEB* peb = (PEB*)__readfsdword(0x30); //32bit
#endif

	return peb;
}

/*
//redundant, already included in IProc
RFW_LDR_DATA_TABLE_ENTRY GetLDREntryIn(TCHAR* modName)
{
	RFW_LDR_DATA_TABLE_ENTRY ldr{ 0 };

	PEB* peb = GetPEB();

	LIST_ENTRY head = peb->Ldr->InMemoryOrderModuleList;
	LIST_ENTRY curr = head;

	while (curr.Flink != head.Blink)
	{
		RFW_LDR_DATA_TABLE_ENTRY* mod = (RFW_LDR_DATA_TABLE_ENTRY*)CONTAINING_RECORD(curr.Flink, RFW_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

		if (mod->BaseDllName.Buffer)
		{
			char* cName = TO_CHAR(mod->BaseDllName.Buffer);
			if (_tcsicmp(cName, modName) == 0)
			{
				ldr = *mod;
				return ldr;
			}
			delete[] cName;
		}
		curr = *curr.Flink;
	}
	return ldr;
}

uintptr_t RFW_GetModuleHandle(TCHAR* modName)
{
	RFW_LDR_DATA_TABLE_ENTRY mod = GetLDREntryIn(modName);

	return (uintptr_t)mod.DllBase;

}

//todo add getprocaddress internal like https://guidedhacking.com/threads/internal-safe-getmodulehandlew-getprocaddress.13565/

*/