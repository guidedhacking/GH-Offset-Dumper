#include "GHFileHelp.h"

void saveFile(nlohmann::json& config, const std::string& text, const std::string& extension)
{
	// get filename from JSON
	std::string filename = config["filename"];

	// format full filename
	std::stringstream ss;
	std::string outpath = filename + "/";
	ss << outpath << filename << "." << extension;

	// write text file
	std::ofstream file(ss.str());
	file << text;
}

void saveReclassFile(nlohmann::json& config, const std::string& xml)
{
	std::string fname = config["filename"];
	std::string reclassFilename =
		fname + "/" + fname;
	reclassFilename += ".rcnet";
	struct zip_t* zip = zip_open(reclassFilename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

	zip_entry_open(zip, "Data.xml");
	zip_entry_write(zip, xml.c_str(), xml.size());
	zip_entry_close(zip);

	zip_close(zip);
}

nlohmann::json parseConfig(std::string& path, bool* success)
{
	if (success == nullptr)
	{
		return {};
	}

	*success = true;
	
	std::ifstream ifs(path);
	std::ostringstream oss;
	oss << ifs.rdbuf();
	std::string entireFile = oss.str();

	if (!nlohmann::json::accept(entireFile))
	{
		*success = false;
		return {};
	}

	return nlohmann::json::parse(entireFile);
}
