#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <stdlib.h>
#include <vector>
#include <intrin.h>
#include "RFW_ntapi.h"
#include "proc.h"

class ModEx;

class ProcEx : public IProc
{
public:
	RFW_PROCESS_BASIC_INFORMATION pbi{ 0 };
	_RFW_PEB peb{};
	ModEx* exeMod; //IMod* ?

	ProcEx();
	ProcEx(TCHAR* exeName);

	~ProcEx();

	virtual bool Get(); //Finds process and get the procEntry
	virtual bool Attach(); //Gets Handle to process with PROCESS_ALL_ACCESS
	virtual bool GetPEB(); //Use NTQueryInformation to get the PEB address and make a copy of it
};
