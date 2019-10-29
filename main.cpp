#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "jsonxx\jsonxx.h"
#include "modules\ProcEx.h"
#include "GHDumper.h"
#include "SrcDumper.h"

const std::string csgo = "csgo.exe"; //to include hl2 etc... to detect src engine and scan netvars

int main()
{
	SetDebugPrivilege(true);

	Dumper* dumper;
	bool bSrcEngine = false;

	jsonxx::Object jsonConfig = ParseConfig();

	if (jsonConfig.get<std::string>("executable") == csgo)
	{
		bSrcEngine = true;
		dumper = new SrcDumper(&jsonConfig);
	}
	else dumper = new Dumper(&jsonConfig);

	dumper->Dump();

	return 0;
}