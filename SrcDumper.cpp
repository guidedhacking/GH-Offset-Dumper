#include "stdafx.h"
#include "SrcDumper.h"

SrcDumper::SrcDumper() {}

SrcDumper::SrcDumper(jsonxx::Object* json)
{
	jsonConfig = json;

	std::string procName = jsonConfig->get<std::string>("executable");

	//Find proc & open handle
	proc = ProcEx((char*)procName.c_str());
}

intptr_t SrcDumper::GetdwGetAllClassesAddr()
{
	for (auto s : signatures)
	{
		if (s.name == "dwGetAllClasses")
		{
			return s.result;
		}
	}
	return 0;
}

HMODULE SrcDumper::LoadClientDLL(ProcEx proc)
{
	ModEx mod(_T("client_panorama.dll"), proc);
	std::filesystem::path p(mod.modEntry.szExePath);

	p = p.parent_path().parent_path().parent_path() / "bin";
	AddDllDirectory(p.wstring().c_str());

	return LoadLibraryEx(mod.modEntry.szExePath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

intptr_t GetOffset(RecvTable* table, const char* tableName, const char* netvarName)
{
	for (int i = 0; i < table->m_nProps; i++)
	{
		RecvProp prop = table->m_pProps[i];

		if (!_stricmp(prop.m_pVarName, netvarName))
		{
			return prop.m_Offset;
		}

		if (prop.m_pDataTable)
		{
			intptr_t offset = GetOffset(prop.m_pDataTable, tableName, netvarName);

			if (offset)
			{
				return offset + prop.m_Offset;
			}
		}
	}
	return 0;
}

intptr_t SrcDumper::GetNetVarOffset(const char* tableName, const char* netvarName, ClientClass* clientClass)
{
	ClientClass* currNode = clientClass;

	for (auto currNode = clientClass; currNode; currNode = currNode->m_pNext)
	{
		if (!_stricmp(tableName, currNode->m_pRecvTable->m_pNetTableName))
		{
			return GetOffset(currNode->m_pRecvTable, tableName, netvarName);
		}
	}

	return 0;
}

void SrcDumper::ProcessNetvars()
{
	jsonxx::Array netvars = jsonConfig->get<jsonxx::Array>("netvars");

	//Loop through JSON netvar array
	for (size_t i = 0; i < netvars.size(); i++)
	{
		jsonxx::Object curr = netvars.get<jsonxx::Object>(i);
		NetvarData currData;

		currData.name = curr.get<jsonxx::String>("name");
		currData.prop = curr.get<jsonxx::String>("prop");
		currData.table = curr.get<jsonxx::String>("table");

		//dump offsets from json into vector
		if (curr.has<jsonxx::Number>("offset"))
		{
			jsonxx::Number offset = curr.get<jsonxx::Number>("offset");
			currData.offset = (int)offset;
		}

		Netvars.push_back(currData);
	}

	//^ we parsed the json into Netvars

	//now we can locally load client_panorama.dll and get the netvar offsets internally
	HMODULE hMod = LoadClientDLL(proc);

	//Get First ClientClass in the linked list
	ClientClass* dwGetallClassesAddr = (ClientClass*)((intptr_t)hMod + GetdwGetAllClassesAddr());
	
	//for each netvar in netvars, get the offset
	
	for (NetvarData& n : Netvars)
	{
		n.result = GetNetVarOffset(n.table.c_str(), n.prop.c_str(), dwGetallClassesAddr);

		if (n.offset)
		{
			n.result += n.offset;
		}
	}
}

void SrcDumper::GenerateHeaderOuput()
{
	//TODO: convert to string output
	std::ofstream file;
	file.open(jsonConfig->get<std::string>("filename") + ".h");

	file << "#pragma once\n#include <cstdint>\n";
	//timestamp

	file << "//GuidedHacking.com r0x0rs ur b0x0rs\n";

	file << "namespace offsets\n{\n";

	file << "\n//signatures\n\n";

	for (auto s : signatures)
	{
		file << "constexpr ptrdiff_t " << s.name << " = 0x" << std::uppercase << std::hex << s.result << ";\n";
	}

	file << "\n//netvars\n\n";

	for (auto n : Netvars)
	{
		file << "constexpr ptrdiff_t " << n.name << " = 0x" << std::uppercase << std::hex << n.result << ";\n";
	}

	file << "\n}\n";

	file.close();
}

void SrcDumper::GenerateCheatEngineOutput()
{
	std::string output;

	//header
	output += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	output += "<CheatTable CheatEngineTableVersion=\"29\">\n";
	output += "<CheatEntries>\n";

	//Add all offsets as User Defined Symbols, these are aliases for addresses you can use throughout your table
	output += "<UserdefinedSymbols>";

	for (auto n : Netvars)
	{
		output += "<SymbolEntry>\n";
		output += "<Name>" + n.name + "</Name>\n";
		output += "<Address>";
		output += AddrToHexString(n.result);
		output += "</Address>\n";
		output += "</SymbolEntry>\n";
	}

	for (auto s : signatures)
	{
		output += "<SymbolEntry>\n";
		output += "<Name>" + s.name + "</Name>\n";
		output += "<Address>";
		output += AddrToHexString(s.result);
		output += "</Address>\n";
		output += "</SymbolEntry>\n";
	}

	output += "</UserdefinedSymbols>\n";
	//end symbol output

	//todo: add all vars to the cheat table in module.dll + symbol form
	//most are offset from client base address, some from clientstate I believe
	//most are prefixed with f, i, etc... to define the variable type, we can use substring search to pull this info maybe
	
	//Work In Progress: We are only adding symbol names and adding each sig/netvar as a relative offset
	//not currently displaying correct data type and the full address, just a basic CE Table

	int count = 0;
	
	for (auto n : Netvars)
	{
		std::string netvarOutput;

		netvarOutput += "<CheatEntry>\n<ID>";
		netvarOutput += std::to_string(count); //decimal
		netvarOutput += "</ID>\n<Description>";
		netvarOutput += n.name;
		netvarOutput += "</Description>\n";
		netvarOutput += n.GetCEVariableTypeString();

		//netvarOutput += "<LastState/>\n<Address>client_panorama.dll + ";
		//netvarOutput += AddrToHexString(n.result); //hex uppercase

		//temporary output for basic CE table
		netvarOutput += "<LastState/>\n<Address>" + n.name;

		//end temp output
		
		netvarOutput += "</Address>\n</CheatEntry>\n";;

		output += netvarOutput;
		count++;
	}

	for (auto s : signatures)
	{
		std::string sigOutput;

		sigOutput += "<CheatEntry>\n";
		sigOutput += "<ID>" + std::to_string(count);
		sigOutput += "</ID>\n";
		sigOutput += "<Description>" + s.name + "</Description>\n";

		//sigOutput += "<LastState/>\n";
		//sigOutput += "<Address>client_panorama.dll + ";
		//sigOutput += AddrToHexString(s.result);

		//temporary output for basic CE table
		sigOutput += "<LastState/>\n<Address>" + s.name;
		//end temp output

		sigOutput += "</Address>\n</CheatEntry>\n";

		output += sigOutput;
		count++;
	}

	output += "</CheatEntries>";

	//footer
	output += "</CheatTable>";

	std::ofstream file;
	file.open(jsonConfig->get<std::string>("filename") + ".CT");
	file << output;
	file.close();
}

void SrcDumper::Dump()
{
	ProcessSignatures();

	ProcessNetvars();

	//Generate header output
	GenerateHeaderOuput();

	//Generate Cheat Engine output
	GenerateCheatEngineOutput();

	//Generate ReClass.NET output
}

std::string SrcDumper::GetSigBase(SigData sigdata)
{
	std::string sigBase;

	if (sigdata.name.find("clientstate_") != std::string::npos)
	{
		sigBase = "[" + sigdata.module + "dwClientState]";
	}

	else
	{
		sigBase = sigdata.module;
	}

	return sigBase;
	//does sig.module define the base module of the offset as well as signature?  think so
}