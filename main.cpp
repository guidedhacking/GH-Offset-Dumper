#include <fstream>
#include <sstream>
#include "json.hpp"
#include "GHDumper.h"
#include "zip.h"
#include "FileScanner.h"

void saveFile(nlohmann::json& config, const std::string& text, const std::string& extension)
{
	// get filename from JSON
	std::string filename = config["filename"];

	// format full filename
	std::stringstream ss;
	ss << "output/" << filename << "." << extension;
	
	// write text file
	std::ofstream file(ss.str());
	file << text;
}

void saveReclassFile(nlohmann::json& config, const std::string& xml)
{
	std::string reclassFilename = "output/" + config["filename"].get<std::string>();
	reclassFilename += ".rcnet";
	struct zip_t *zip = zip_open(reclassFilename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
	
	zip_entry_open(zip, "Data.xml");
	zip_entry_write(zip, xml.c_str(), xml.size());
	zip_entry_close(zip);
	
	zip_close(zip);
}

int main(int argc, const char** argv)
{
	// default color
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 15);

	// load json

	std::string config_file = "config.json";
	if (argc >= 2)
	{
		config_file = argv[1];
	}

	std::ifstream file(config_file);
	auto config = nlohmann::json::parse(file);
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
		scanner = FileScanner(config["exefile"].get<std::string>(), dynamicModules);
	}
	else
	{
		// setup scanner for module name
		scanner = FileScanner(config["executable"]);
	}

	// dump as std::unordered_map<std::string, ptrdiff_t>
	auto signatures = gh::DumpSignatures(config, scanner);
	auto netvars = gh::DumpNetvars(config, signatures);

	// format files as std::string
	auto hpp = gh::FormatHeader(config, signatures, scanner, netvars);
	auto ct = gh::FormatCheatEngine(config, signatures, scanner, netvars);
	auto xml = gh::FormatReclass(config, scanner, netvars);
	
	// create output directory
	std::filesystem::create_directory("output/");

	// save files
	saveFile(config, hpp, "hpp");
	saveFile(config, ct, "ct");
	saveReclassFile(config, xml);

	SetConsoleTextAttribute(hConsole, 15);
	scanner.decon();

	printf("Finished!\n");
	Sleep(3000);
	return 0;
}
