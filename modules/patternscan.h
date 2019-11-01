#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include "proc.h"
#include "Mod.h"
#include <vector>
#include "util.h"

namespace Pattern
{
	//Split combo pattern into mask/pattern
	void Parse(char* combo, char* pattern, char* mask);

	//Pattern Scan
	char* ScanBasic(char* pattern, char* mask, char* begin, intptr_t size);

	namespace Ex
	{
		//External Wrapper
		char* Scan(char* pattern, char* mask, char* begin, intptr_t size, HANDLE hProc);

		//Scan a single module
		char* ScanMod(char* pattern, char* mask, ModEx& module);

		char* ScanMod(char* combopattern, ModEx& mod);

		//Scan all modules from process
		char* ScanAllMods(char* pattern, char* mask, ProcEx& proc);

		//Scan entire process
		char* ScanProc(char* pattern, char* mask, ProcEx& proc);
	}
}