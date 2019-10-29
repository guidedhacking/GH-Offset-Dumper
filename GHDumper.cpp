#include "stdafx.h"
#include "GHDumper.h"

jsonxx::Object ParseConfig()
{
	//Open File Stream & parse JSON data into object
	std::ifstream f("config.json", std::ios::in);
	std::string jsonBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	jsonxx::Object o;
	o.parse(jsonBuffer);
	return o;
}

void SigData::Get(ProcEx proc)
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

Dumper::Dumper() {}

Dumper::Dumper(jsonxx::Object* json)
{
	jsonConfig = json;

	//Get & attach to process
	std::string procName = jsonConfig->get<std::string>("executable");

	//Find proc & open handle
	ProcEx proc((char*)procName.c_str());

}

void Dumper::ProcessSignatures()
{
	//select signature array in json
	jsonxx::Array sigs = jsonConfig->get<jsonxx::Array>("signatures");

	//Loop through json signature array
	for (size_t i = 0; i < sigs.size(); i++)
	{
		jsonxx::Object curr = sigs.get<jsonxx::Object>(i);
		SigData currData;

		currData.name = curr.get<std::string>("name");
		currData.extra = (int)curr.get<jsonxx::Number>("extra");
		currData.relative = curr.get<jsonxx::Boolean>("relative");
		currData.module = curr.get<std::string>("module");

		//dump offsets from json into vector, not all have offsets
		jsonxx::Array offsetArray;

		//only grab them if they exist
		if (curr.has<jsonxx::Array>("offsets"))
		{
			offsetArray = curr.get<jsonxx::Array>("offsets");

			//Despite most only have 1 offset, it's an arrays
			for (size_t j = 0; j < offsetArray.size(); j++)
			{
				currData.offsets.push_back((int)offsetArray.get<jsonxx::Number>(j));
			}
		}

		currData.comboPattern = curr.get<std::string>("pattern");
		signatures.push_back(currData);
	}

	for (auto& s : signatures)
	{
		//Scan for the pattern, process the relative & extra offsets
		s.Get(proc);
	}
}

void Dumper::Dump()
{
	//GetSigs before Netvars because we need dwGetAllclasses
	ProcessSignatures();

	//for SrcEngine you would ProcessNetvars also

	//Generate header output

	//Generate Cheat Engine output

	//Generate ReClass.NET output

	//Write Output files
}