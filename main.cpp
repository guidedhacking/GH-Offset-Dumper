#include <fstream>
#include <sstream>
#include "json.hpp"
#include "GHDumper.h"
#include "zip.h"

void saveFile(nlohmann::json& config, const std::string& text, const std::string& extension)
{
	// get filename from JSON
	std::string filename = config["filename"];
	
	// format full filename
	std::stringstream ss;
	ss << filename << "." << extension;
	
	// write text file
	std::ofstream file(ss.str());
	file << text;
}

void saveReclassFile(nlohmann::json& config, const std::string& xml)
{
	std::string reclassFilename = config["filename"];
	reclassFilename += ".rcnet";
	struct zip_t *zip = zip_open(reclassFilename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
	
	zip_entry_open(zip, "Data.xml");
	zip_entry_write(zip, xml.c_str(), xml.size());
	zip_entry_close(zip);
	
	zip_close(zip);
}

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
	
	// save files
	saveFile(config, hpp, "hpp");
	saveFile(config, ct, "ct");
	saveReclassFile(config, xml);
}
