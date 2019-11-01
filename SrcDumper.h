#pragma once
#include <Windows.h>
#include <filesystem>
#include "GHDumper.h"
#include "SrcSDK.h"
#include "Netvar.h"
#include "SigData.h"

class SrcDumper : public Dumper
{
public:
	std::vector<NetvarData> Netvars;

	SrcDumper();
	SrcDumper(jsonxx::Object* json);

	intptr_t GetdwGetAllClassesAddr();

	HMODULE LoadClientDLL(ProcEx proc);

	intptr_t GetNetVarOffset(const char* tableName, const char* netvarName, ClientClass* clientClass);

	void ProcessNetvars();

	virtual void Dump();

	void GenerateHeaderOuput();

	void GenerateCheatEngineOutput();

	std::string GetSigBase(SigData sigdata);
};