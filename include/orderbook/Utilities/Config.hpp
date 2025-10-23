#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace orderbook {

class Logger;

class Config {
public:
    explicit Config(const std::string& filename);
    
    // Type-safe parameter access
    std::string getString(const std::string& section, 
                         const std::string& key,
                         const std::string& defaultValue = "") const;
    
    int getInt(const std::string& section,
              const std::string& key,
              int defaultValue = 0) const;
    
    double getDouble(const std::string& section,
                   const std::string& key,
                   double defaultValue = 0.0) const;
    
    bool getBool(const std::string& section,
                const std::string& key,
                bool defaultValue = false) const;
    
    // Configuration validation
    bool isValid() const { return is_valid_; }
    const std::vector<std::string>& getErrors() const { return errors_; }
    
    // Check if section/key exists
    bool hasSection(const std::string& section) const;
    bool hasKey(const std::string& section, const std::string& key) const;
    
    // Get all keys in a section
    std::vector<std::string> getKeys(const std::string& section) const;
    
    // Reload configuration
    bool reload();
    
    // Set logger for error reporting
    void setLogger(std::shared_ptr<Logger> logger);

private:
    std::string filename_;
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::string>> config_;
    bool is_valid_;
    std::vector<std::string> errors_;
    std::shared_ptr<Logger> logger_;
    
    // Internal methods
    bool parseFile();
    void validateConfiguration();
    void logError(const std::string& message);
    void logWarning(const std::string& message);
    std::string trim(const std::string& str) const;
};

}