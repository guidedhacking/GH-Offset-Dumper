#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <stdlib.h>
#include <vector>
#include <intrin.h>
#include "RFW_ntapi.h"
#include "proc.h"

class ProcIn : public IProc
{
public:
	ProcIn();
	~ProcIn();

	_RFW_PEB* peb{ nullptr };

	virtual bool Get();
	virtual bool GetPEB();
};
