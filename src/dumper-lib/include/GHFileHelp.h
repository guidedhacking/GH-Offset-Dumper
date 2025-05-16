#pragma once
#include <fstream>
#include <sstream>
#include "GHDumper.h"
#include "../json.hpp"
#include "../zip.h"
#include "../FileScanner.h"

void saveFile(nlohmann::json& config, const std::string& text, const std::string& extension);
void saveReclassFile(nlohmann::json& config, const std::string& xml);
nlohmann::json parseConfig(std::string& path, bool* success);
