#pragma once
#include <string>
#include <unordered_map>

namespace orderbook {

class Config {
public:
    explicit Config(const std::string& filename);
    
    std::string getString(const std::string& section, 
                         const std::string& key,
                         const std::string& defaultValue = "") const;
    
    int getInt(const std::string& section,
              const std::string& key,
              int defaultValue = 0) const;
    
    double getDouble(const std::string& section,
                   const std::string& key,
                   double defaultValue = 0.0) const;

private:
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::string>> config_;
};

}