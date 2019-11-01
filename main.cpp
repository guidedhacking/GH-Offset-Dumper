#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "jsonxx\jsonxx.h"
#include "modules\ProcEx.h"
#include "GHDumper.h"
#include "SrcDumper.h"

const std::string srcEngineGames[] = { "csgo.exe", "hl2.exe" };

int main()
{
	SetDebugPrivilege(true);

	Dumper* dumper;
	bool bSrcEngine = false;

	jsonxx::Object jsonConfig = ParseConfig();

	if (std::find(std::begin(srcEngineGames), std::end(srcEngineGames), jsonConfig.get<std::string>("executable")) != std::end(srcEngineGames))
	{
		bSrcEngine = true;
		dumper = new SrcDumper(&jsonConfig);
	}
	else dumper = new Dumper(&jsonConfig);

	dumper->Dump();

	return 0;
}