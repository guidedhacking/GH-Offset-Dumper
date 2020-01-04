#pragma once

#include "ProcEx.h"
#include "ProcIn.h"
#include "util.h"

//delete Mod, rename ModIn to Mod, have ModEx inherit from Mod ?

class IMod
{
public:
	TCHAR* name{ nullptr };
	MODULEENTRY32 modEntry{};
	RFW_LDR_DATA_TABLE_ENTRY ldr{};

	~IMod();

	virtual bool Get() = 0; //Find module and get the modEntry
};

class ModEx : public IMod
{
public:
	ProcEx* proc{ nullptr };

	ModEx(TCHAR* modName);
	ModEx(TCHAR* modName, ProcEx& process);

	~ModEx();

	virtual bool Get();
};

class ModIn : public IMod
{
public:
	ProcIn* proc{ nullptr };

	ModIn(TCHAR* modName);
	ModIn(TCHAR* modName, ProcIn& process);

	virtual bool Get();
	bool GetLDREntry();
};