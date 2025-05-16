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
	
	// weird nlohmann bug when using ifstream twice
	auto file_bytes = FileScanner::ReadBytes(path);
	std::vector<char> file_content(file_bytes.size());
	std::transform(file_bytes.begin(), file_bytes.end(), file_content.begin(), [] (MappedType c) { return static_cast<char>(c); });
	std::string file_string = std::string(file_content.begin(), file_content.end());

	if (!nlohmann::json::accept(file_string))
	{
		*success = false;
		return {};
	}

	return nlohmann::json::parse(file_string);
}
