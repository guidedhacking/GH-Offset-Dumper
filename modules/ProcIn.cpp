#include "stdafx.h"
#include "ProcIn.h"

ProcIn::ProcIn()
{
	TCHAR path[MAX_PATH]{};
	TCHAR file[MAX_PATH]{};
	TCHAR ext[MAX_PATH]{};
	TCHAR filename[MAX_PATH]{};
	GetModuleFileName(NULL, path, MAX_PATH);
	_tsplitpath_s(path, 0, 0, 0, 0, file, MAX_PATH, ext, MAX_PATH);
	_tcscpy_s(filename, file);
	_tcscat_s(filename, ext);
	name = filename;
	Get();
}

ProcIn::~ProcIn()
{}

//internal
bool ProcIn::GetPEB()
{
#ifdef _WIN64
	peb = (_RFW_PEB*)__readgsword(0x60);

#else
	peb = (_RFW_PEB*)__readfsdword(0x30);
#endif

	return peb != nullptr;
}

bool ProcIn::Get()
{
	return GetPEB();
}