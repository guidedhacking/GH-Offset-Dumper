#include "include/GHDumper.h"
#include <sstream>
#include <string>
#include "base64.hpp"
#include <GHFileHelp.h>

namespace gh
{
	namespace internal
	{
		BOOL GetProcessByName(const wchar_t* processName, PROCESSENTRY32W* out)
		{
			out->dwSize = sizeof(PROCESSENTRY32W);
			HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hSnap != INVALID_HANDLE_VALUE)
				if (Process32FirstW(hSnap, out))
					do
						if (!_wcsicmp(out->szExeFile, processName))
							return CloseHandle(hSnap);
			while (Process32NextW(hSnap, out));
			CloseHandle(hSnap);
			return FALSE;
		}

		BOOL GetModuleByName(DWORD pid, const wchar_t* moduleName, MODULEENTRY32W* out)
		{
			out->dwSize = sizeof(MODULEENTRY32W);
			HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
			if (hSnap != INVALID_HANDLE_VALUE)
				if (Module32FirstW(hSnap, out))
					do
						if (!_wcsicmp(out->szModule, moduleName))
							return CloseHandle(hSnap);
			while (Module32NextW(hSnap, out));
			CloseHandle(hSnap);
			return FALSE;
		}

		void SplitComboPattern(const char* combo, std::string& pattern, std::string& mask)
		{
			uint8_t byte;
			auto ishex = [] (const char c) -> bool { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); };
			auto hexchartoint = [] (const char c) -> uint8_t { return (c >= 'A') ? (c - 'A' + 10) : (c - '0'); };

			for (; *combo; combo++)
			{
				if (ishex(*combo))
				{
					if (ishex(combo[1]))
						byte = hexchartoint(*combo) | (hexchartoint(*(combo++)) << 4);
					else
						byte = hexchartoint(*combo);
					pattern += byte;
					mask += 'x';
				}
				else if (*combo == '?')
				{
					pattern += '\x00';
					mask += '?';
					if (combo[1] == '?')
						combo++;
				}
			}
		}

		// https://guidedhacking.com/threads/external-internal-pattern-scanning-guide.14112/
		char* ScanBasic(const char* pattern, const char* mask, char* begin, intptr_t size)
		{
			intptr_t patternLen = strlen(mask);

			for (int i = 0; i < size; i++)
			{
				bool found = true;
				for (int j = 0; j < patternLen; j++)
				{
					if (mask[j] != '?' && pattern[j] != *(char*)((intptr_t)begin + i + j))
					{
						found = false;
						break;
					}
				}
				if (found)
				{
					return (begin + i);
				}
			}

			return nullptr;
		}

		// https://guidedhacking.com/threads/external-internal-pattern-scanning-guide.14112/
		char* ScanEx(const char* pattern, const char* mask, char* begin, intptr_t size, HANDLE hProc)
		{
			char* match { nullptr };
			SIZE_T bytesRead;
			DWORD oldprotect;
			char* buffer { nullptr };
			MEMORY_BASIC_INFORMATION mbi;
			mbi.RegionSize = 0x1000;

			VirtualQueryEx(hProc, (LPCVOID)begin, &mbi, sizeof(mbi));

			for (char* curr = begin; curr < begin + size; curr += mbi.RegionSize)
			{
				if (!VirtualQueryEx(hProc, curr, &mbi, sizeof(mbi))) continue;
				if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) continue;

				delete[] buffer;
				buffer = new char[mbi.RegionSize];

				if (VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
				{
					ReadProcessMemory(hProc, mbi.BaseAddress, buffer, mbi.RegionSize, &bytesRead);
					VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);

					char* internalAddr = ScanBasic(pattern, mask, buffer, (intptr_t)bytesRead);

					if (internalAddr != nullptr)
					{
						//calculate from internal to external
						match = curr + (internalAddr - buffer);
						break;
					}
				}
			}
			delete[] buffer;
			return match;
		}

		intptr_t FindNetvarInRecvTable(RecvTable* table, const char* netvarName)
		{
			for (int i = 0; i < table->m_nProps; i++)
			{
				RecvProp* prop = table->m_pProps + i;

				if (!_stricmp(prop->m_pVarName, netvarName))
					return prop->m_Offset;

				if (prop->m_pDataTable)
				{
					intptr_t offset = FindNetvarInRecvTable(prop->m_pDataTable, netvarName);

					if (offset)
						return offset + prop->m_Offset;
				}
			}
			return 0;
		}

		intptr_t FindNetvar(const char* tableName, const char* netvarName, ClientClass* firstClientClass)
		{
			for (auto node = firstClientClass; node; node = node->m_pNext)
				if (!_stricmp(tableName, node->m_pRecvTable->m_pNetTableName))
					return FindNetvarInRecvTable(node->m_pRecvTable, netvarName);
			return 0;
		}

		std::string FormatNowUTC()
		{
			time_t t = time(0);
			struct tm tm_info;
			gmtime_s(&tm_info, &t);

			std::stringstream ss;
			ss << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S UTC");
			return ss.str();
		}

		nlohmann::json FindSignatureJSON(nlohmann::json& config, std::string signatureName)
		{
			for (auto& e : config["signatures"])
				if (std::string(e["name"]) == signatureName)
					return e;
		}

		nlohmann::json FindNetvarJSON(nlohmann::json& config, std::string netvarName)
		{
			for (auto& e : config["netvars"])
				if (std::string(e["name"]) == netvarName)
					return e;
		}
	} // internal namespace

	Dump DumpSignatures(nlohmann::json& config, FileScanner& scanner)
	{
		Dump signatures;

		// Use "executable" string in config JSON to get process handle and PID
		PROCESSENTRY32W process;
		if (!scanner.Valid())
		{
			std::string exe = config["executable"];
			std::wstring wexe(exe.begin(), exe.end());
			if (!internal::GetProcessByName(wexe.c_str(), &process))
				return {};
		}

		DWORD pid = process.th32ProcessID;
		HANDLE hProcess = INVALID_HANDLE_VALUE;
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, 15);

		if (!scanner.Valid())
		{
			hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		}

		for (auto& signature : config["signatures"])
		{
			// make sure required JSON keys are present
			if (!(signature.contains("name") && signature.contains("pattern")))
				continue;

			if (!signature.contains("module"))
			{
				if (scanner.Valid())
				{
					// default to the main exe
					signature["module"] = scanner.getMainFileName();
				}
				else
				{
					signature["module"] = config["executable"];
				}
			}

			// get module where the signature is
			MODULEENTRY32W module;
			std::string moduleName = signature["module"];
			std::wstring wmoduleName(moduleName.begin(), moduleName.end());
			if (!scanner.Valid())
			{
				if (!internal::GetModuleByName(pid, wmoduleName.c_str(), &module))
					return {};
			}

			// do pattern scanning
			std::string comboPattern = signature["pattern"];
			std::string pattern, mask;
			internal::SplitComboPattern(comboPattern.c_str(), pattern, mask);
			size_t result {};
			
			if (!scanner.Valid())
			{
				result = (size_t)internal::ScanEx(pattern.c_str(), mask.c_str(), (char*)module.modBaseAddr, module.modBaseSize, hProcess);
			}
			else
			{
				module.modBaseAddr = (BYTE*)scanner.getFileByName(moduleName).getBytes();
				module.modBaseSize = scanner.getFileByName(moduleName).getSize();

				// default to executable module
				if (module.modBaseAddr == nullptr)
				{
					module.modBaseAddr = (BYTE*)scanner.getMainFile().getBytes();
					module.modBaseSize = scanner.getMainFile().getSize();
				}
				result = (size_t)internal::ScanBasic(pattern.c_str(), mask.c_str(), (char*)module.modBaseAddr, module.modBaseSize);
			}

			std::string pattern_name = signature["name"];
			if (result)
			{
				SetConsoleTextAttribute(hConsole, 10);
				printf("[+] Found pattern %s at %p\n", pattern_name.c_str(), result);

				if (signature.contains("rva"))
				{
					SetConsoleTextAttribute(hConsole, 13);
					printf("\t[?] Processing Relative Branch\n");
					bool is_call = signature["rva"];
					if (is_call)
					{
						int opcode = signature.contains("opLoc") ? signature["opLoc"] : 1;
						int oplength = signature.contains("opLength") ? signature["opLength"] : 5;

						// resolve the call (if we are reading from disk)
						if (scanner.Valid())
						{
							int32_t rva = *(int32_t*)(result + opcode);
							size_t jmp_location = rva + result + oplength;
							result = jmp_location;
						}
						else
						{
							// we need to read it from the process externally otherwise
							int32_t rva {};
							ReadProcessMemory(hProcess, (LPCVOID)(result + opcode), &rva, sizeof(rva), nullptr); 
							size_t jmp_location = rva + result + oplength;
							result = jmp_location;
						}

						SetConsoleTextAttribute(hConsole, 14);
						printf("\t[+] Resolved call location: %p\n", result);

					}
				}
			}
			else
			{
				SetConsoleTextAttribute(hConsole, 12);
				printf("\t[!] Failed to find pattern %s (%s)!\n", pattern_name.c_str(), comboPattern.c_str());
			}
			

			if (!result)
			{
				continue;
			}

			// read multi-level pointer (if present)
			// first offset is relative to pattern location, different than FindDMAAddy, you must add offset first, then RPM
			if (signature.contains("offsets"))
				for (auto offset : signature["offsets"])
				{
					result += offset;
					if (!scanner.Valid())
					{
						ReadProcessMemory(hProcess, (BYTE*)result, &result, sizeof(size_t), NULL);
					}
					else
					{
						result = *(intptr_t*)result;
					}
				}

			// add "extra" JSON value to result
			if (signature.contains("extra"))
			{
				int extra = signature["extra"];
				if (extra)
					result += extra;
			}

			if (signature.contains("relative"))
			{
				bool relative = signature["relative"];
				if (relative)
					result -= (ptrdiff_t)module.modBaseAddr;
			}
			else
			{
				if (config.contains("relativeByDefault"))
				{
					bool relativeByDefault = config["relativeByDefault"];
					if (relativeByDefault)
					{
						// default to relative address
						result -= (ptrdiff_t)module.modBaseAddr;
					}
				}

			}


			// store result in "signatures" map
			signatures[signature["name"]] = result;
		}

		CloseHandle(hProcess);
		return signatures;
	}

	Dump DumpNetvars(nlohmann::json& config, const Dump& signatures)
	{
		Dump netvars;

		if (!config.contains("netvars"))
			return {};

		// Use "executable" string in config JSON to get PID
		std::string exe = config["executable"];
		std::wstring wexe(exe.begin(), exe.end());
		PROCESSENTRY32W process;
		if (!internal::GetProcessByName(wexe.c_str(), &process))
			return {};
		DWORD pid = process.th32ProcessID;
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, 15);

		// return if dumper's architecture does match game's architecture
		BOOL wow64;
#ifdef _WIN64 // dumper is 64-bit
		IsWow64Process(hProcess, &wow64);
		if (wow64) // game is 32-bit on 64-bit OS
			return {};
#else // dumper is 32-bit
		IsWow64Process(hProcess, &wow64);
		if (!wow64) // game is 32-bit on 32-bit OS or game is 64-bit on 64-bit OS
		{
			SYSTEM_INFO systemInfo { 0 };
			GetNativeSystemInfo(&systemInfo);
			if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) // game is 64-bit on 64-bit OS
				return {};
		}
#endif // _WIN64

		// Find client.dll
		MODULEENTRY32W client_dll;
		if (!internal::GetModuleByName(pid, L"client.dll", &client_dll))
			return {};

		// Add directory path of client.dll's dependencies, otherwise client.dll won't load
		std::filesystem::path path(client_dll.szExePath);
		path = path.parent_path().parent_path().parent_path() / "bin"; // is this CS:GO specific?
		AddDllDirectory(path.wstring().c_str());

		// Load client.dll into the dumper process
		auto current_addy = LoadLibraryExW(client_dll.szExePath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!current_addy)
			return {};

		// Get first ClientClass in the linked list	
		ClientClass* firstClientClass = (ClientClass*)((size_t)current_addy + signatures.at("dwGetAllClasses"));

		for (auto& netvar : config["netvars"])
		{
			// make sure required JSON keys are present 
			if (!(netvar.contains("name") && netvar.contains("table") && netvar.contains("prop")))
				continue;

			std::string table = netvar["table"];
			std::string prop = netvar["prop"];
			auto result = internal::FindNetvar(table.c_str(), prop.c_str(), firstClientClass);

			// add "offset" (if any)
			if (netvar.contains("offset"))
			{
				int offset = netvar["offset"];
				if (offset)
					result += offset;
			}

			SetConsoleTextAttribute(hConsole, 11);
			printf("[=] Found Netvar %s at %p\n", netvar["name"].get<std::string>().c_str(), result);
			netvars[netvar["name"]] = result;
		}

		CloseHandle(hProcess);
		return netvars;
	}

	std::string FormatHeader(nlohmann::json& config, const Dump& signatures, FileScanner& scanner, const Dump& netvars)
	{
		std::stringstream hpp;

		hpp << "// Generated by GuidedHacking.com's Offset Dumper at " << internal::FormatNowUTC() << "\n";
		hpp << "#pragma once\n";
		hpp << "#include <cstdint>\n";
		hpp << "#include <cstddef>\n"; // vscode error
		hpp << "\n";

		hpp << "namespace ghdumper\n{\n";
		hpp << "\tconstexpr std::int64_t timestamp = " << std::time(0) << ";\n";

		hpp << "\tnamespace signatures\n\t{\n";
		for (auto e : signatures)
		{
			hpp << "\t\tconstexpr std::ptrdiff_t " << e.first << " = 0x" << std::uppercase << std::hex << e.second << ";";

			// only add module comment if the signature is an offset relative to a module base address
			// the comment shows how to use the offset: <module>+<offset>
			const auto sig_json = internal::FindSignatureJSON(config, e.first);
			bool relative = true;

			if (sig_json.contains("relative"))
			{
				relative = sig_json["relative"];	
			}
			else
			{
				if (config.contains("relativeByDefault"))
				{
					relative = config["relativeByDefault"];
				}
			}

			std::string module_name = config["executable"];
			if (sig_json.contains("module"))
			{
				module_name = sig_json["module"];
			}

			// get the original exe name
			if (module_name == scanner.getMainFileName())
			{
				module_name = config["executable"];
			}

			if (module_name.find_last_of(".") == std::string::npos)
			{
				module_name += scanner.getFileByName(module_name).getExt();
			}


			hpp << " // " << module_name << "+0x" << e.second;
			hpp << "\n";
		}
		hpp << "\t} // namespace signatures\n";

		hpp << "\tnamespace netvars\n\t{\n";
		for (auto e : netvars)
			hpp << "\t\tconstexpr ::std::ptrdiff_t " << e.first << " = 0x" << std::uppercase << std::hex << e.second << "; // " << std::string(internal::FindNetvarJSON(config, e.first)["table"]) << "\n";
		hpp << "\t} // namespace netvars\n";

		hpp << "} // namespace ghdumper\n";

		return hpp.str();
	}

	std::string FormatCheatEngine(nlohmann::json& config, const Dump& signatures, FileScanner& scanner, const Dump& netvars)
	{
		/* Cheat engine table structure
		Generated by GuidedHacking.com's Offset Dumper at <UTC date>
		dwGameDir
		Local Player
			DT_BasePlayer
				m_iHealth
				...
			DT_CSPlayer
				...
			DT_BaseEntity
				...
			DT_BaseAnimating
				...
		Entity 0
			DT_BasePlayer
				m_iHealth
				...
			DT_CSPlayer
				...
			DT_BaseEntity
				...
			DT_BaseAnimating
				...
		Entity 1
			...
		...
		Entity 31
			...
		Raw Data (prettified .hpp)
			Signatures (relative to module)
				<netvarName> <offset>
				...
			Signatures (not relative to module)
				<netvarName> <offset>
				...
			Netvars
				Table X
					<netvarName> <offset>
					...
				Table Y
					<netvarName> <offset>
					...
				...
		*/

		std::stringstream ct, rel, nrel, rawNetvars, rawData;

		std::string nowUTC = internal::FormatNowUTC();

		ct << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
		ct << "<!-- Generated by GuidedHacking.com's Offset Dumper at " << nowUTC << " -->\n";
		ct << "<CheatTable CheatEngineTableVersion=\"31\">\n";

		size_t id = 0;

		// A cheat entry is a row in the Cheat Engine table of addresses
		auto FormatCheatEntry = [&](const std::string& description, const std::string& childrenXML = {}, bool collapsible = true, bool hideChildren = false, const std::string& address = {}, const std::string& valueType = {}, int showAsHex = 1, int length = 0)
		{
			std::stringstream tmp;
			tmp << "<CheatEntry>\n";
			tmp << "<ID>" << std::dec << id++ << "</ID>\n";
			tmp << "<Description>\"" << description << "\"</Description>\n";

			if (address.empty())
				tmp << "<GroupHeader>1</GroupHeader>\n"; // makes Type and Value columns not appear
			else
			{
				tmp << "<Address>" << address << "</Address>\n";
				tmp << "<VariableType>" << valueType << "</VariableType>\n";
				if (valueType == "String")
					tmp << "<Length>" << length << "</Length>\n";
				tmp << "<ShowAsHex>" << showAsHex << "</ShowAsHex>\n";
			}

			if (!childrenXML.empty())
				tmp << "<CheatEntries>\n" << childrenXML << "</CheatEntries>\n"; // child entries

			if (hideChildren || collapsible)
			{
				tmp << "<Options";
				if (hideChildren)
					tmp << " moAlwaysHideChildren=\"1\""; // collapsed by default
				if (collapsible)
					tmp << " moManualExpandCollapse=\"1\""; // +/- button to collpase/expand
				tmp << "/>\n";
			}

			tmp << "</CheatEntry>\n";
			return tmp.str();
		};

		auto FormatPlayer = [&] (const std::string& base)
		{
			std::stringstream tmp;
			auto playerTables = { "DT_BasePlayer", "DT_CSPlayer", "DT_BaseEntity", "DT_BaseAnimating" };

			for (auto& playerTable : playerTables)
			{
				std::stringstream entries;

				for (auto& netvar : config["netvars"])
				{
					if (std::string(netvar["table"]) == playerTable)
					{
						std::string name = netvar["name"];
						std::stringstream address;
						int length = 0;
						address << base << "+" << name;

						std::string valueType = "4 Bytes";
						int showAsHex = 1;
						if (name.find("m_i") == 0 || name.find("m_n") == 0) // integer
							showAsHex = 0;
						else if (name.find("m_b") == 0) // boolean
						{
							showAsHex = 0;
							valueType = "Byte";
						}
						else if (name.find("m_fl") == 0 || name.find("m_ang") == 0) // float
						{
							showAsHex = 0;
							valueType = "Float";
						}
						else if (name.find("m_sz") == 0) // string
						{
							showAsHex = 0;
							valueType = "String";
							length = 100;
						}

						if (name.find("m_vec") == 0) // vector of 3 floats
						{
							std::stringstream floats;
							floats << FormatCheatEntry("x", {}, {}, {}, address.str() + "+0", "Float", 0);
							floats << FormatCheatEntry("y", {}, {}, {}, address.str() + "+4", "Float", 0);
							floats << FormatCheatEntry("z", {}, {}, {}, address.str() + "+8", "Float", 0);
							entries << FormatCheatEntry(name, floats.str(), true, false, address.str(), valueType, showAsHex, length);
						}
						else
							entries << FormatCheatEntry(name, {}, {}, {}, address.str(), valueType, showAsHex, length);
					}
				}

				if (entries.str().size() > 0)
					tmp << FormatCheatEntry(playerTable, entries.str());
			}

			return tmp.str();
		};

		ct << "<CheatEntries>\n";

		// Add green GH banner with timestamp at the top
		ct << "<CheatEntry>\n";
		ct << "<ID>0</ID>\n";
		ct << "<Description>\"Generated by GuidedHacking.com's Offset Dumper at " << nowUTC << "\"</Description>\n";
		ct << "<GroupHeader>1</GroupHeader>\n";
		ct << "<Color>0EF7C1</Color>\n";
		ct << "</CheatEntry>\n";
		id += 1;

		// Add game directory entry
		if (signatures.count("dwGameDir"))
			for (auto& e : config["signatures"])
				if (std::string(e["name"]) == "dwGameDir")
					ct << FormatCheatEntry("dwGameDir", {}, {}, {}, std::string(e["module"]) + "+dwGameDir", "String", 0, 256);

		// Add local player and entity list
		if (config.contains("netvars") && config["netvars"].size() > 0)
		{
			if (signatures.count("dwLocalPlayer"))
				ct << FormatCheatEntry("Local Player", FormatPlayer("[client.dll+dwLocalPlayer]"), true, false, "client.dll+dwLocalPlayer", "4 Bytes", 1);

			if (signatures.count("dwEntityList"))
				for (int i = 0; i < 32; i++)
				{
					std::stringstream base;
					base << "[client.dll+dwEntityList+" << std::hex << i * 0x10 << "]";

					std::stringstream groupName;
					groupName << "Entity " << std::dec << i;

					std::stringstream address;
					address << "client.dll+dwEntityList+" << std::hex << i * 0x10;

					ct << FormatCheatEntry(groupName.str(), FormatPlayer(base.str()), true, true, address.str(), "4 Bytes", 1);
				}
		}

		// format signatures
		for (auto e : signatures)
		{
			auto signature = internal::FindSignatureJSON(config, e.first);
			bool relative = true; // default to true
			if (signature.contains("relative"))
			{
				relative = signature["relative"];
			}
			else
			{
				if (config.contains("relativeByDefault"))
				{
					relative = config["relativeByDefault"];
				}
			}

			std::string module_name = scanner.getMainFileName();
			if (signature.contains("module"))
			{
				module_name = signature["module"].get<std::string>();
			}

			std::stringstream address;
			if (relative)
				address << module_name << "+";
			address << std::uppercase << std::hex << "0x" << e.second;

			auto entry = FormatCheatEntry(e.first, {}, {}, {}, address.str(), "4 Bytes", 1);

			if (relative)
				rel << entry;
			else
				nrel << entry;
		}

		// format netvars
		std::set<std::string> netvarTables;
		for (auto& netvar : config["netvars"])
			netvarTables.insert(netvar["table"]);
		for (auto& table : netvarTables)
		{
			std::stringstream tmp;

			for (auto& netvar : config["netvars"])
				if (std::string(netvar["table"]) == table)
				{
					std::string netvarName = netvar["name"];
					if (!netvars.count(netvarName)) // not present
						continue;
					std::stringstream address;
					address << std::uppercase << std::hex << "0x" << netvars.at(netvarName);
					tmp << FormatCheatEntry(std::string(netvar["name"]), {}, {}, {}, address.str(), "4 Bytes", 1);
				}

			rawNetvars << FormatCheatEntry(table, tmp.str());
		}

		// add signatures and netvars
		rawData << FormatCheatEntry("Signatures (relative to module base)", rel.str());
		rawData << FormatCheatEntry("Signatures (not relative to module base)", nrel.str());
		rawData << FormatCheatEntry("Netvars", rawNetvars.str());
		ct << FormatCheatEntry("Raw Data (prettified .hpp)", rawData.str());

		ct << "</CheatEntries>\n";

		// Add all offsets as user defined symbols, these are aliases for addresses you can use throughout your table

		ct << "<UserdefinedSymbols>\n";

		for (auto e : signatures)
			ct << "<SymbolEntry>\n"
			<< "<Name>" << e.first << "</Name>\n"
			<< "<Address>" << std::hex << "0x" << e.second << "</Address>\n"
			<< "</SymbolEntry>\n";

		for (auto e : netvars)
			ct << "<SymbolEntry>\n"
			<< "<Name>" << e.first << "</Name>\n"
			<< "<Address>" << std::hex << "0x" << e.second << "</Address>\n"
			<< "</SymbolEntry>\n";

		ct << "</UserdefinedSymbols>";

		ct << "</CheatTable>\n";

		return ct.str();
	}

	std::string FormatReclass(nlohmann::json& config, FileScanner& scanner, const Dump& netvars)
	{
		std::stringstream xml;

		xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
		xml << "<!-- Generated by GuidedHacking.com's Offset Dumper at " << internal::FormatNowUTC() << " -->\n";

		xml << "<reclass version=\"65537\" type=\"x86\">\n";
		xml << "<custom_data />\n";
		xml << "<type_mapping>\n";
		xml << "<TypeBool>bool</TypeBool>\n";
		xml << "<TypeInt8>int8_t</TypeInt8>\n";
		xml << "<TypeInt16>int16_t</TypeInt16>\n";
		xml << "<TypeInt32>int32_t</TypeInt32>\n";
		xml << "<TypeInt64>int64_t</TypeInt64>\n";
		xml << "<TypeNInt>ptrdiff_t</TypeNInt>\n";
		xml << "<TypeUInt8>uint8_t</TypeUInt8>\n";
		xml << "<TypeUInt16>uint16_t</TypeUInt16>\n";
		xml << "<TypeUInt32>uint32_t</TypeUInt32>\n";
		xml << "<TypeUInt64>uint64_t</TypeUInt64>\n";
		xml << "<TypeNUInt>size_t</TypeNUInt>\n";
		xml << "<TypeFloat>float</TypeFloat>\n";
		xml << "<TypeDouble>double</TypeDouble>\n";
		xml << "<TypeVector2>Vector2</TypeVector2>\n";
		xml << "<TypeVector3>Vector3</TypeVector3>\n";
		xml << "<TypeVector4>Vector4</TypeVector4>\n";
		xml << "<TypeMatrix3x3>Matrix3x3</TypeMatrix3x3>\n";
		xml << "<TypeMatrix3x4>Matrix3x4</TypeMatrix3x4>\n";
		xml << "<TypeMatrix4x4>Matrix4x4</TypeMatrix4x4>\n";
		xml << "<TypeUtf8Text>char</TypeUtf8Text>\n";
		xml << "<TypeUtf16Text>wchar_t</TypeUtf16Text>\n";
		xml << "<TypeUtf32Text>char32_t</TypeUtf32Text>\n";
		xml << "<TypeFunctionPtr>void*</TypeFunctionPtr>\n";
		xml << "</type_mapping>\n";
		xml << "<enums />\n";

		xml << "<classes>\n";

		auto generateUUIDv4 = [] ()
		{
			std::random_device rd;
			std::stringstream ss;
			ss << std::hex << std::setfill('0');
			ss << std::setw(8) << rd();
			ss << "-";
			ss << std::setw(4) << (rd() & 0xffff);
			ss << "-";
			ss << std::setw(0) << "4" << std::setw(3) << (rd() & 0xfff);
			ss << "-";
			ss << std::setw(0) << "9" << std::setw(3) << (rd() & 0xfff);
			ss << "-";
			ss << std::setw(8) << rd() << std::setw(4) << (rd() & 0xffff);
			
			// new versions of reclass expect base64 encoded uuids
			return base64::to_base64(ss.str());
		};

		size_t paddingID = 0;

		std::set<std::string> netvarTables;
		for (auto& netvar : config["netvars"])
			netvarTables.insert(netvar["table"]);

		for (auto& table : netvarTables)
		{
			std::vector<std::pair<std::string, size_t>> tableNetvars;

			xml << "<class uuid=\"" << generateUUIDv4() << "\" name=\"" << table << "\" comment=\"\" address=\"0\">\n";

			for (auto& netvar : config["netvars"])
				if (std::string(netvar["table"]) == table)
				{
					std::string netvarName = netvar["name"];
					if (!netvars.count(netvarName)) // not present
						continue;
					tableNetvars.push_back({ netvarName, netvars.at(netvarName) });
				}

			std::sort(tableNetvars.begin(), tableNetvars.end(),
				[] (const std::pair<std::string, size_t>& a, const std::pair<std::string, size_t>& b)
				{
					return a.second < b.second;
				}
			);

			size_t lastOffset = 0;
			size_t lastVarSize = 0;

			for (size_t i = 0; i < tableNetvars.size(); i++)
			{
				auto& pair = tableNetvars[i];
				size_t paddingSize = pair.second - (lastOffset + lastVarSize);
				if (paddingSize > 0)
					xml << "<node type=\"Utf8TextNode\" name=\"pad" << paddingID++ << "\" comment=\"\" hidden=\"true\" length=\"" << paddingSize << "\" />\n";

				std::string nodeType = "UInt32Node";
				lastVarSize = 4;

				std::string name = pair.first;

				if (name.find("m_i") == 0 || name.find("m_n") == 0) // integer
					;
				else if (name.find("m_b") == 0) // boolean
				{
					nodeType = "BoolNode";
					lastVarSize = 1;
				}
				else if (name.find("m_fl") == 0 || name.find("m_ang") == 0) // float
					nodeType = "FloatNode";
				else if (name.find("m_sz") == 0) // string
				{
					nodeType = "Utf8TextNode";
					lastVarSize = 32;
				}
				else if (name.find("m_vec") == 0) // vector of 3 floats
				{
					nodeType = "Vector3Node";
					lastVarSize = 12;
				}

				// validate lastVarSize
				if (
					i != tableNetvars.size() - 1 // not last entry
					&& pair.second + lastVarSize > tableNetvars[i + 1].second
					) // lastVarSize is wrong, assume one byte
				{
					nodeType = "UInt8Node";
					lastVarSize = 1;
				}

				xml << "<node type=\"" << nodeType << "\" name=\"" << pair.first << "\" comment=\"\" hidden=\"false\"";
				if (nodeType == "Utf8TextNode")
					xml << " length=\"" << lastVarSize << "\"";
				xml << "/>\n";

				lastOffset = pair.second;
			}

			xml << "</class>\n";
		}

		xml << "</classes>\n";

		xml << "</reclass>\n";

		return xml.str();
	}

	FileScanner InitScanner(nlohmann::json& config, bool* runtime)
	{
		FileScanner scanner {};
		if (config.contains("fileonly") && config.contains("exefile"))
		{
			DynamicMoudleArray dynamicModules {};
			if (config.contains("additionalModules"))
			{
				for (auto& mod : config["additionalModules"])
				{
					std::string name = mod["name"];
					std::string path = mod["path"];

					DynamicModule dynamicModule {};
					dynamicModule.compName = name;
					dynamicModule.filePath = path;

					dynamicModules.push_back(dynamicModule);
				}
			}

			scanner = FileScanner(config["exefile"], dynamicModules);
			*runtime = false;
		}
		else
		{
			// setup scanner for module name
			scanner = FileScanner(config["executable"]);
			*runtime = true;
		}

		return scanner;
	}

	bool ParseCommandLine(int argc, const char** argv)
	{
		// support drag and drop json files
		std::string config_file = "config.json";
		if (argc >= 2)
		{
			config_file = argv[1];
		}

		printf("%s\n", config_file.c_str());

		bool config_parsed = false;
		auto config = parseConfig(config_file, &config_parsed);
		
		// failed to parse config, invalid json
		if (config_parsed == false)
		{
			printf("[-] Invalid Config File\n");
			return false;
		}

		if (!config.contains("executable"))
		{
			printf("%s\n", config.dump(4).c_str());
			printf("[-] Invalid config, missing executable name\n");
			return false;
		}

		const std::string target_game = config["executable"];

		// parse config for on-disk files
		bool runtime_dump = false;
		FileScanner scanner = gh::InitScanner(config, &runtime_dump);

		printf("[~] Target: %s\n", target_game.c_str());

		if (runtime_dump)
		{
			printf("[!] Dumping From Runtime\n");

			std::string exe = target_game;
			std::wstring wexe(exe.begin(), exe.end());
			PROCESSENTRY32W out {};
			if (!internal::GetProcessByName(wexe.c_str(), &out))
			{
				printf("[-] Failed to find target process\n");
				return false;
			}
		}
		else
		{
			printf("[!] Dumping From Disk\n");
		}

		// dump into map
		Dump signatures = gh::DumpSignatures(config, scanner);
		Dump netvars = gh::DumpNetvars(config, signatures);

		// format files
		std::string hpp = gh::FormatHeader(config, signatures, scanner, netvars);
		std::string ct = gh::FormatCheatEngine(config, signatures, scanner, netvars);
		std::string xml = gh::FormatReclass(config, scanner, netvars);

		// create output directory
		std::string output_dir = config["filename"].get<std::string>() + "/";
		std::filesystem::create_directory(output_dir);
		
		// save files
		saveFile(config, hpp, "hpp");
		saveFile(config, ct, "ct");
		saveReclassFile(config, xml);

		scanner.decon();
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
		return true;
	}
}