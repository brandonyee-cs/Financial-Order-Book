#include "orderbook/Utilities/Config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace orderbook {

Config::Config(const std::string& filename) {
    std::ifstream file(filename);
    std::string line, currentSection;
    
    while (std::getline(file, line)) {

        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        if(line.empty() || line[0] == ';') continue;
        
        if(line[0] == '[') {
            currentSection = line.substr(1, line.find(']') - 1);
        } else {
            size_t eqPos = line.find('=');
            if(eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                config_[currentSection][key] = value;
            }
        }
    }
}

std::string Config::getString(const std::string& section, 
                             const std::string& key,
                             const std::string& defaultValue) const {
    auto sit = config_.find(section);
    if(sit == config_.end()) return defaultValue;
    
    auto kit = sit->second.find(key);
    return kit != sit->second.end() ? kit->second : defaultValue;
}

int Config::getInt(const std::string& section,
                  const std::string& key,
                  int defaultValue) const {
    try {
        return std::stoi(getString(section, key, std::to_string(defaultValue)));
    } catch(...) {
        return defaultValue;
    }
}

double Config::getDouble(const std::string& section,
                        const std::string& key,
                        double defaultValue) const {
    try {
        return std::stod(getString(section, key, std::to_string(defaultValue)));
    } catch(...) {
        return defaultValue;
    }
}

}