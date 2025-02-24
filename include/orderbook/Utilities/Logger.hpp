#pragma once
#include <string>

namespace orderbook {

class Logger {
public:
    static void init(const std::string& filename);
    static void log(const std::string& message);
    static void error(const std::string& message);

private:
    static std::string logFile_;
};

}