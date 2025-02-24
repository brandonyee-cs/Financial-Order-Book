#include "orderbook/Utilities/Logger.hpp"
#include <fstream>

namespace orderbook {

std::string Logger::logFile_;

void Logger::init(const std::string& filename) {
    logFile_ = filename;
}

void Logger::log(const std::string& message) {
    std::ofstream file(logFile_, std::ios::app);
    file << message << std::endl;
}

void Logger::error(const std::string& message) {
    log("[ERROR] " + message);
}

}