#include "orderbook/Utilities/Config.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace orderbook {

Config::Config(const std::string& filename) 
    : filename_(filename), is_valid_(false) {
    parseFile();
    validateConfiguration();
}

bool Config::parseFile() {
    std::ifstream file(filename_);
    if (!file.is_open()) {
        errors_.push_back("Failed to open configuration file: " + filename_);
        logError("Failed to open configuration file: " + filename_);
        return false;
    }
    
    std::string line, currentSection;
    int lineNumber = 0;
    
    while (std::getline(file, line)) {
        ++lineNumber;
        
        // Trim whitespace
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        
        // Handle sections
        if (line[0] == '[') {
            size_t closeBracket = line.find(']');
            if (closeBracket == std::string::npos) {
                std::string error = "Malformed section header at line " + std::to_string(lineNumber) + ": " + line;
                errors_.push_back(error);
                logError(error);
                continue;
            }
            currentSection = trim(line.substr(1, closeBracket - 1));
            if (currentSection.empty()) {
                std::string error = "Empty section name at line " + std::to_string(lineNumber);
                errors_.push_back(error);
                logError(error);
            }
        } else {
            // Handle key-value pairs
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                std::string error = "Invalid key-value pair at line " + std::to_string(lineNumber) + ": " + line;
                errors_.push_back(error);
                logError(error);
                continue;
            }
            
            std::string key = trim(line.substr(0, eqPos));
            std::string value = trim(line.substr(eqPos + 1));
            
            if (key.empty()) {
                std::string error = "Empty key at line " + std::to_string(lineNumber);
                errors_.push_back(error);
                logError(error);
                continue;
            }
            
            if (currentSection.empty()) {
                std::string error = "Key-value pair outside of section at line " + std::to_string(lineNumber) + ": " + line;
                errors_.push_back(error);
                logError(error);
                continue;
            }
            
            config_[currentSection][key] = value;
        }
    }
    
    return true;
}

void Config::validateConfiguration() {
    // Validate required sections and keys
    std::vector<std::pair<std::string, std::string>> requiredKeys = {
        {"orderbook", "symbol"},
        {"orderbook", "max_orders"},
        {"network", "port"},
        {"network", "max_connections"},
        {"risk", "max_order_size"},
        {"risk", "max_position"},
        {"risk", "max_price"},
        {"logging", "level"},
        {"logging", "file"}
    };
    
    for (const auto& [section, key] : requiredKeys) {
        if (!hasKey(section, key)) {
            std::string warning = "Missing configuration key: [" + section + "] " + key + " - using default value";
            logWarning(warning);
        }
    }
    
    // Validate numeric values
    if (hasKey("network", "port")) {
        int port = getInt("network", "port", 5000);
        if (port <= 0 || port > 65535) {
            std::string error = "Invalid port number: " + std::to_string(port) + " (must be 1-65535)";
            errors_.push_back(error);
            logError(error);
        }
    }
    
    if (hasKey("orderbook", "max_orders")) {
        int maxOrders = getInt("orderbook", "max_orders", 1000000);
        if (maxOrders <= 0) {
            std::string error = "Invalid max_orders: " + std::to_string(maxOrders) + " (must be positive)";
            errors_.push_back(error);
            logError(error);
        }
    }
    
    if (hasKey("risk", "max_order_size")) {
        double maxOrderSize = getDouble("risk", "max_order_size", 10000.0);
        if (maxOrderSize <= 0) {
            std::string error = "Invalid max_order_size: " + std::to_string(maxOrderSize) + " (must be positive)";
            errors_.push_back(error);
            logError(error);
        }
    }
    
    // Validate logging level
    if (hasKey("logging", "level")) {
        std::string level = getString("logging", "level", "info");
        std::transform(level.begin(), level.end(), level.begin(), ::tolower);
        if (level != "debug" && level != "info" && level != "warn" && level != "error") {
            std::string error = "Invalid logging level: " + level + " (must be debug, info, warn, or error)";
            errors_.push_back(error);
            logError(error);
        }
    }
    
    is_valid_ = errors_.empty();
}

std::string Config::getString(const std::string& section, 
                             const std::string& key,
                             const std::string& defaultValue) const {
    auto sit = config_.find(section);
    if (sit == config_.end()) {
        return defaultValue;
    }
    
    auto kit = sit->second.find(key);
    return kit != sit->second.end() ? kit->second : defaultValue;
}

int Config::getInt(const std::string& section,
                  const std::string& key,
                  int defaultValue) const {
    try {
        std::string value = getString(section, key);
        if (value.empty()) {
            return defaultValue;
        }
        return std::stoi(value);
    } catch (const std::exception& e) {
        logError("Failed to parse integer value for [" + section + "] " + key + ": " + e.what());
        return defaultValue;
    }
}

double Config::getDouble(const std::string& section,
                        const std::string& key,
                        double defaultValue) const {
    try {
        std::string value = getString(section, key);
        if (value.empty()) {
            return defaultValue;
        }
        return std::stod(value);
    } catch (const std::exception& e) {
        logError("Failed to parse double value for [" + section + "] " + key + ": " + e.what());
        return defaultValue;
    }
}

bool Config::getBool(const std::string& section,
                    const std::string& key,
                    bool defaultValue) const {
    std::string value = getString(section, key);
    if (value.empty()) {
        return defaultValue;
    }
    
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

bool Config::hasSection(const std::string& section) const {
    return config_.find(section) != config_.end();
}

bool Config::hasKey(const std::string& section, const std::string& key) const {
    auto sit = config_.find(section);
    if (sit == config_.end()) {
        return false;
    }
    return sit->second.find(key) != sit->second.end();
}

std::vector<std::string> Config::getKeys(const std::string& section) const {
    std::vector<std::string> keys;
    auto sit = config_.find(section);
    if (sit != config_.end()) {
        for (const auto& [key, value] : sit->second) {
            keys.push_back(key);
        }
    }
    return keys;
}

bool Config::reload() {
    config_.clear();
    errors_.clear();
    is_valid_ = false;
    
    bool success = parseFile();
    validateConfiguration();
    
    if (success && is_valid_) {
        logWarning("Configuration reloaded successfully from " + filename_);
    } else {
        logError("Failed to reload configuration from " + filename_);
    }
    
    return success && is_valid_;
}

void Config::setLogger(std::shared_ptr<Logger> logger) {
    logger_ = logger;
}

void Config::logError(const std::string& message) const {
    if (logger_) {
        logger_->error("Config", message);
    } else {
        std::cerr << "[ERROR] Config: " << message << std::endl;
    }
}

void Config::logWarning(const std::string& message) const {
    if (logger_) {
        logger_->warn("Config", message);
    } else {
        std::cerr << "[WARN] Config: " << message << std::endl;
    }
}

std::string Config::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

}