#include <windows.h>
#include <tlhelp32.h>

#include <unordered_map>
#include <random>
#include <vector>
#include <algorithm>
#include <set>
#include <time.h>

#include "json.hpp"

/* Notes
- gh::DumpNetvars() crashes if the dumper's architecture (32-bit or 64-bit) is different than the target process's architecture.
- gh::DumpNetvars() only works for games using the source engine.
- gh::DumpSignatures() and gh::DumpNetvars() return a Dump which is a std::unordered_map<std::string, ptrdiff_t>. If an error occurs, an empty map is returned.
- gh::FormatReclass() returns the Data.xml text contents. A reclass file is a <name>.rcnet zip containing Data.xml.
*/

/* Example
#include <fstream>
#include "json.hpp"
#include "GHDumper.h"

int main()
{
	// load json
	std::ifstream file("config.json"); 
	auto config = nlohmann::json::parse(file);

	// dump as std::unordered_map<std::string, ptrdiff_t>
	auto signatures = gh::DumpSignatures(config);
	auto netvars = gh::DumpNetvars(config, signatures);
	
	// format files as std::string
	auto hpp = gh::FormatHeader(config, signatures, netvars);
	auto ct = gh::FormatCheatEngine(config, signatures, netvars);
	auto xml = gh::FormatReclass(config, netvars);
	
	// save files or do whatever
	// ...
}

*/

namespace gh
{
	namespace internal
	{
		__inline BOOL GetProcessByName(const wchar_t* processName, PROCESSENTRY32W* out)
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

		__inline BOOL GetModuleByName(DWORD pid, const wchar_t* moduleName, MODULEENTRY32W* out)
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

		__inline void SplitComboPattern(const char* combo, std::string& pattern, std::string& mask)
		{
			uint8_t byte;
			auto ishex = [](const char c) -> bool { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); };
			auto hexchartoint = [](const char c) -> uint8_t { return (c >= 'A') ? (c - 'A' + 10) : (c - '0'); };

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
		__inline char* ScanBasic(const char* pattern, const char* mask, char* begin, intptr_t size)
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
		__inline char* ScanEx(const char* pattern, const char* mask, char* begin, intptr_t size, HANDLE hProc)
		{
			char* match{ nullptr };
			SIZE_T bytesRead;
			DWORD oldprotect;
			char* buffer{ nullptr };
			MEMORY_BASIC_INFORMATION mbi;
			mbi.RegionSize = 0x1000;//

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

		struct RecvProp;

		struct RecvTable
		{
			RecvProp*	m_pProps;
			int			m_nProps;
			void*		m_pDecoder;
			char*		m_pNetTableName;
			bool		m_bInitialized;
			bool		m_bInMainList;
		};

		struct RecvProp
		{
			char*			m_pVarName;
			void*			m_RecvType;
			int         	m_Flags;
			int         	m_StringBufferSize;
			int         	m_bInsideArray;
			const void*		m_pExtraData;
			RecvProp*		m_pArrayProp;
			void*			m_ArrayLengthProxy;
			void*			m_ProxyFn;
			void*			m_DataTableProxyFn;
			RecvTable*  	m_pDataTable;
			int         	m_Offset;
			int         	m_ElementStride;
			int         	m_nElements;
			const char*		m_pParentArrayPropName;
		};

		struct ClientClass
		{
			void*			m_pCreateFn;
			void*			m_pCreateEventFn;
			char*			m_pNetworkName;
			RecvTable*		m_pRecvTable;
			ClientClass*	m_pNext;
			int				m_ClassID;
		};
		
		__inline intptr_t FindNetvarInRecvTable(RecvTable* table, const char* netvarName)
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
		
		__inline intptr_t FindNetvar(const char* tableName, const char* netvarName, ClientClass* firstClientClass)
		{
			for (auto node = firstClientClass; node; node = node->m_pNext)
				if (!_stricmp(tableName, node->m_pRecvTable->m_pNetTableName))
					return FindNetvarInRecvTable(node->m_pRecvTable, netvarName);
			return 0;
		}
		
		__inline std::string FormatNowUTC()
		{
			time_t t = time(0);
			struct tm tm_info;
			gmtime_s(&tm_info, &t);
			
			std::stringstream ss;
			ss << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S UTC");
			return ss.str();
		}
	
		__inline nlohmann::json FindSignatureJSON(nlohmann::json& config, std::string signatureName)
		{
			for (auto& e : config["signatures"])
				if (std::string(e["name"]) == signatureName)
					return e;
		}
		
		__inline nlohmann::json FindNetvarJSON(nlohmann::json& config, std::string netvarName)
		{
			for (auto& e : config["netvars"])
				if (std::string(e["name"]) == netvarName)
					return e;
		}
	
	} // namespace internal
		
	typedef std::unordered_map<std::string, ptrdiff_t> Dump;

	__inline Dump DumpSignatures(nlohmann::json& config)
	{
		Dump signatures;
		
		// Use "executable" string in config JSON to get process handle and PID
		std::string exe = config["executable"];
		std::wstring wexe(exe.begin(), exe.end());
		PROCESSENTRY32W process;
		if (!internal::GetProcessByName(wexe.c_str(), &process))
			return {};
		DWORD pid = process.th32ProcessID;
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

		for (auto& signature : config["signatures"])
		{
			// make sure required JSON keys are present
			if (!(signature.contains("name") && signature.contains("module") && signature.contains("pattern")))
				continue;
			
			// get module where the signature is
			MODULEENTRY32W module;
			std::string moduleName = signature["module"];
			std::wstring wmoduleName(moduleName.begin(), moduleName.end());
			if (!internal::GetModuleByName(pid, wmoduleName.c_str(), &module))
				return {};
			
			// do pattern scanning
			std::string comboPattern = signature["pattern"];
			std::string pattern, mask;
			internal::SplitComboPattern(comboPattern.c_str(), pattern, mask);
			intptr_t result = (intptr_t)internal::ScanEx(pattern.c_str(), mask.c_str(), (char*)module.modBaseAddr, module.modBaseSize, hProcess);
			
			if (!result)
				continue;

			// read multi-level pointer (if present)
			// first offset is relative to pattern location, different than FindDMAAddy, you must add offset first, then RPM
			if (signature.contains("offsets"))
				for (auto offset : signature["offsets"])
				{
					result += offset;
					ReadProcessMemory(hProcess, (BYTE*)result, &result, 4, NULL);
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

			// store result in "signatures" map
			signatures[signature["name"]] = result;
		}
		
		CloseHandle(hProcess);
		return signatures;
	}

	__inline Dump DumpNetvars(nlohmann::json& config, const Dump& signatures)
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
			SYSTEM_INFO systemInfo{0};
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
		if (!LoadLibraryExW(client_dll.szExePath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS))
			return {};
		
		// Get first ClientClass in the linked list
		internal::ClientClass* firstClientClass = (internal::ClientClass*)(client_dll.modBaseAddr + signatures.at("dwGetAllClasses"));

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
			
			netvars[netvar["name"]] = result;
		}
		
		CloseHandle(hProcess);
		return netvars;
	}
	
	__inline std::string FormatHeader(nlohmann::json& config, const Dump& signatures, const Dump& netvars = {})
	{
		std::stringstream hpp;
		
		hpp << "// Generated by GuidedHacking.com's Offset Dumper at " << internal::FormatNowUTC() << "\n";
		hpp << "#pragma once\n";
		hpp << "#include <cstdint>\n";
		hpp << "\n";
		
		hpp << "namespace ghdumper {\n";
		hpp << "constexpr ::std::int64_t timestamp = " << std::time(0) << ";\n";
		
		hpp << "namespace signatures {\n";
		for (auto e : signatures)
		{
			hpp << "constexpr ::std::ptrdiff_t " << e.first << " = 0x" << std::uppercase << std::hex << e.second << ";";
			
			// only add module comment if the signature is an offset relative to a module base address
			// the comment shows how to use the offset: <module>+<offset>
			if (internal::FindSignatureJSON(config, e.first)["relative"]) 
				hpp << " // " << std::string(internal::FindSignatureJSON(config, e.first)["module"]) << "+0x" << e.second;
			
			hpp << "\n";
		}
		hpp << "} // namespace signatures\n";
		
		hpp << "namespace netvars {\n";
		for (auto e : netvars)
			hpp << "constexpr ::std::ptrdiff_t " << e.first << " = 0x" << std::uppercase << std::hex << e.second << "; // " << std::string(internal::FindNetvarJSON(config, e.first)["table"]) << "\n";
		hpp << "} // namespace netvars\n";
		
		hpp << "} // namespace ghdumper\n";
		
		return hpp.str();
	}

	__inline std::string FormatCheatEngine(nlohmann::json& config, const Dump& signatures, const Dump& netvars = {})
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
		auto FormatCheatEntry = [&](const std::string& description, const std::string& childrenXML={}, bool collapsible=true, bool hideChildren=false, const std::string& address={}, const std::string& valueType={}, int showAsHex=1, int length=0)
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

		auto FormatPlayer = [&](const std::string& base)
		{
			std::stringstream tmp;
			auto playerTables = {"DT_BasePlayer", "DT_CSPlayer", "DT_BaseEntity", "DT_BaseAnimating"};
			
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
					base << "[client.dll+dwEntityList+" << std::hex << i*0x10 << "]";
					
					std::stringstream groupName;
					groupName << "Entity " << std::dec << i;
					
					std::stringstream address;
					address << "client.dll+dwEntityList+" << std::hex << i*0x10;
					
					ct << FormatCheatEntry(groupName.str(), FormatPlayer(base.str()), true, true, address.str(), "4 Bytes", 1);
				}
		}
		
		// format signatures
		for (auto e : signatures)
		{
			auto signature = internal::FindSignatureJSON(config, e.first);
			bool relative = signature["relative"];
			
			std::stringstream address;
			if (relative)
				address << std::string(signature["module"]) << "+";
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
	
	__inline std::string FormatReclass(nlohmann::json& config, const Dump& netvars = {})
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
		
		auto generateUUIDv4 = []()
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
			return ss.str();
		};
		
		size_t paddingID = 0;
		
		std::set<std::string> netvarTables;
		for (auto& netvar : config["netvars"])
			netvarTables.insert(netvar["table"]);
		
		for (auto& table : netvarTables)
		{
			std::vector<std::pair<std::string,size_t>> tableNetvars;
			
			xml << "<class uuid=\"" << generateUUIDv4() << "\" name=\"" << table << "\" comment=\"\" address=\"0\">\n";
			
			for (auto& netvar : config["netvars"])
				if (std::string(netvar["table"]) == table)
				{
					std::string netvarName = netvar["name"];
					if (!netvars.count(netvarName)) // not present
						continue;
					tableNetvars.push_back({netvarName, netvars.at(netvarName)});
				}
			
			std::sort(tableNetvars.begin(), tableNetvars.end(),
				[](const std::pair<std::string,size_t>& a, const std::pair<std::string,size_t>& b) {
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
					&& pair.second + lastVarSize > tableNetvars[i+1].second
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
	
} // namespace ghdumper
