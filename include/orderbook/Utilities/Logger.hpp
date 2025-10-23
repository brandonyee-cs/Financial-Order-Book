#pragma once
#include "../Core/Types.hpp"
#include "../Core/Interfaces.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace orderbook {

class Config;

/**
 * @brief Concrete implementation of structured logging
 */
class Logger : public ILogger {
public:
    struct LogConfig {
        std::string filename = "orderbook.log";
        LogLevel min_level = LogLevel::INFO;
        bool console_output = true;
        bool json_format = true;
        size_t max_file_size = 100 * 1024 * 1024; // 100MB
        size_t max_files = 5;
    };
    
    explicit Logger(const LogConfig& config = LogConfig{});
    explicit Logger(std::shared_ptr<Config> config);
    ~Logger();
    
    // ILogger interface implementation
    void log(LogLevel level, const std::string& message, 
             const std::string& context = "") override;
    void debug(const std::string& message, const std::string& context = "") override;
    void info(const std::string& message, const std::string& context = "") override;
    void warn(const std::string& message, const std::string& context = "") override;
    void error(const std::string& message, const std::string& context = "") override;
    void logPerformance(const std::string& operation, 
                       uint64_t latency_ns,
                       const std::vector<std::pair<std::string, std::string>>& additional_metrics = {}) override;
    
    // Enhanced performance logging methods
    void logThroughput(const std::string& operation, uint64_t count, uint64_t duration_ns);
    void logLatencyStats(const std::string& operation, uint64_t min_ns, uint64_t max_ns, uint64_t avg_ns);
    void logMemoryUsage(const std::string& component, size_t bytes_used, size_t bytes_allocated);
    void setLogLevel(LogLevel level) override;
    
    // Static convenience methods for global logger
    static void setGlobalLogger(std::shared_ptr<Logger> logger);
    static std::shared_ptr<Logger> getGlobalLogger();
    
    // Configuration
    void setConfig(const LogConfig& config);
    const LogConfig& getConfig() const;
    void loadConfiguration(std::shared_ptr<Config> config);
    static LogLevel parseLogLevel(const std::string& level_str);

private:
    LogConfig config_;
    std::unique_ptr<std::ofstream> file_stream_;
    std::mutex log_mutex_;
    size_t current_file_size_;
    
    static std::shared_ptr<Logger> global_logger_;
    static std::mutex global_mutex_;
    
    // Helper methods
    void writeToFile(const std::string& formatted_message);
    void writeToConsole(const std::string& formatted_message);
    std::string formatMessage(LogLevel level, const std::string& message, 
                             const std::string& context);
    std::string formatJsonMessage(LogLevel level, const std::string& message, 
                                 const std::string& context);
    std::string logLevelToString(LogLevel level);
    void rotateLogFile();
    bool shouldLog(LogLevel level) const;
};

}