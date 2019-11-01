#include "stdafx.h"
#include "SigData.h"

void SigData::Scan(ProcEx proc)
{
	//TODO: Remove this when we detach this project from RFW
	ModEx mod((char*)module.c_str(), proc);

	//Scan for the pattern
	result = (intptr_t)Pattern::Ex::ScanMod((char*)comboPattern.c_str(), mod);

	//first offset is relative to pattern location, different than FindDMAAddy, you must add offset first, then RPM
	if (offsets.size() != 0)
	{
		for (auto o : offsets)
		{
			result = result + o;
			ReadProcessMemory(proc.handle, (BYTE*)result, &result, sizeof(o), NULL);
		}
	}

	//offset into the resulting address, if necessary
	if (extra)
	{
		result = result + extra;
	}

	//if a relative offset, get the relative offset
	if (relative)
	{
		result = result - (uintptr_t)mod.modEntry.modBaseAddr;
	}
}