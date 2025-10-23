#include "orderbook/Utilities/Logger.hpp"
#include "orderbook/Utilities/Config.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace orderbook {

std::shared_ptr<Logger> Logger::global_logger_;
std::mutex Logger::global_mutex_;

Logger::Logger(const LogConfig& config) : config_(config), current_file_size_(0) {
    if (!config_.filename.empty()) {
        file_stream_ = std::make_unique<std::ofstream>(config_.filename, std::ios::app);
        if (file_stream_->is_open()) {
            // Get current file size
            file_stream_->seekp(0, std::ios::end);
            current_file_size_ = file_stream_->tellp();
        }
    }
}

Logger::Logger(std::shared_ptr<Config> config) : current_file_size_(0) {
    loadConfiguration(config);
}

Logger::~Logger() {
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->close();
    }
}

void Logger::loadConfiguration(std::shared_ptr<Config> config) {
    if (!config) {
        return;
    }
    
    // Load logging configuration
    config_.filename = config->getString("logging", "file", "orderbook.log");
    config_.console_output = config->getBool("logging", "console_output", true);
    config_.json_format = config->getBool("logging", "json_format", true);
    config_.max_file_size = config->getInt("logging", "max_file_size", 100 * 1024 * 1024);
    config_.max_files = config->getInt("logging", "max_files", 5);
    
    // Parse log level
    std::string level_str = config->getString("logging", "level", "info");
    config_.min_level = parseLogLevel(level_str);
    
    // Initialize file stream
    if (!config_.filename.empty()) {
        file_stream_ = std::make_unique<std::ofstream>(config_.filename, std::ios::app);
        if (file_stream_->is_open()) {
            // Get current file size
            file_stream_->seekp(0, std::ios::end);
            current_file_size_ = file_stream_->tellp();
        }
    }
}

LogLevel Logger::parseLogLevel(const std::string& level_str) {
    std::string lower_level = level_str;
    std::transform(lower_level.begin(), lower_level.end(), lower_level.begin(), ::tolower);
    
    if (lower_level == "debug") return LogLevel::DEBUG;
    if (lower_level == "info") return LogLevel::INFO;
    if (lower_level == "warn" || lower_level == "warning") return LogLevel::WARN;
    if (lower_level == "error") return LogLevel::ERROR;
    
    return LogLevel::INFO; // Default
}

void Logger::log(LogLevel level, const std::string& message, const std::string& context) {
    if (!shouldLog(level)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string formatted_message;
    if (config_.json_format) {
        formatted_message = formatJsonMessage(level, message, context);
    } else {
        formatted_message = formatMessage(level, message, context);
    }
    
    if (config_.console_output) {
        writeToConsole(formatted_message);
    }
    
    if (file_stream_ && file_stream_->is_open()) {
        writeToFile(formatted_message);
    }
}

void Logger::debug(const std::string& message, const std::string& context) {
    log(LogLevel::DEBUG, message, context);
}

void Logger::info(const std::string& message, const std::string& context) {
    log(LogLevel::INFO, message, context);
}

void Logger::warn(const std::string& message, const std::string& context) {
    log(LogLevel::WARN, message, context);
}

void Logger::error(const std::string& message, const std::string& context) {
    log(LogLevel::ERROR, message, context);
}

void Logger::logPerformance(const std::string& operation, 
                           uint64_t latency_ns,
                           const std::vector<std::pair<std::string, std::string>>& additional_metrics) {
    if (!shouldLog(LogLevel::INFO)) {
        return;
    }
    
    if (config_.json_format) {
        std::ostringstream oss;
        oss << "{\"type\":\"performance\",\"operation\":\"" << operation << "\""
            << ",\"latency_ns\":" << latency_ns;
        
        for (const auto& [key, value] : additional_metrics) {
            oss << ",\"" << key << "\":\"" << value << "\"";
        }
        oss << "}";
        
        log(LogLevel::INFO, oss.str(), "PERF");
    } else {
        std::ostringstream oss;
        oss << "Performance: " << operation << " took " << latency_ns << "ns";
        
        for (const auto& [key, value] : additional_metrics) {
            oss << ", " << key << "=" << value;
        }
        
        log(LogLevel::INFO, oss.str(), "PERF");
    }
}

void Logger::logThroughput(const std::string& operation, uint64_t count, uint64_t duration_ns) {
    if (!shouldLog(LogLevel::INFO)) {
        return;
    }
    
    double ops_per_sec = (count * 1e9) / duration_ns;
    
    if (config_.json_format) {
        std::ostringstream oss;
        oss << "{\"type\":\"throughput\",\"operation\":\"" << operation << "\""
            << ",\"count\":" << count << ",\"duration_ns\":" << duration_ns
            << ",\"ops_per_sec\":" << std::fixed << std::setprecision(2) << ops_per_sec << "}";
        log(LogLevel::INFO, oss.str(), "THROUGHPUT");
    } else {
        std::ostringstream oss;
        oss << "Throughput: " << operation << " processed " << count << " operations in "
            << duration_ns << "ns (" << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec)";
        log(LogLevel::INFO, oss.str(), "THROUGHPUT");
    }
}

void Logger::logLatencyStats(const std::string& operation, uint64_t min_ns, uint64_t max_ns, uint64_t avg_ns) {
    if (!shouldLog(LogLevel::INFO)) {
        return;
    }
    
    if (config_.json_format) {
        std::ostringstream oss;
        oss << "{\"type\":\"latency_stats\",\"operation\":\"" << operation << "\""
            << ",\"min_ns\":" << min_ns << ",\"max_ns\":" << max_ns << ",\"avg_ns\":" << avg_ns << "}";
        log(LogLevel::INFO, oss.str(), "LATENCY");
    } else {
        std::ostringstream oss;
        oss << "Latency Stats: " << operation << " - min: " << min_ns << "ns, max: " 
            << max_ns << "ns, avg: " << avg_ns << "ns";
        log(LogLevel::INFO, oss.str(), "LATENCY");
    }
}

void Logger::logMemoryUsage(const std::string& component, size_t bytes_used, size_t bytes_allocated) {
    if (!shouldLog(LogLevel::INFO)) {
        return;
    }
    
    double utilization = bytes_allocated > 0 ? (double)bytes_used / bytes_allocated * 100.0 : 0.0;
    
    if (config_.json_format) {
        std::ostringstream oss;
        oss << "{\"type\":\"memory_usage\",\"component\":\"" << component << "\""
            << ",\"bytes_used\":" << bytes_used << ",\"bytes_allocated\":" << bytes_allocated
            << ",\"utilization_percent\":" << std::fixed << std::setprecision(2) << utilization << "}";
        log(LogLevel::INFO, oss.str(), "MEMORY");
    } else {
        std::ostringstream oss;
        oss << "Memory Usage: " << component << " - used: " << bytes_used << " bytes, allocated: "
            << bytes_allocated << " bytes (" << std::fixed << std::setprecision(2) << utilization << "% utilization)";
        log(LogLevel::INFO, oss.str(), "MEMORY");
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_.min_level = level;
}

void Logger::setGlobalLogger(std::shared_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    global_logger_ = logger;
}

std::shared_ptr<Logger> Logger::getGlobalLogger() {
    std::lock_guard<std::mutex> lock(global_mutex_);
    if (!global_logger_) {
        global_logger_ = std::make_shared<Logger>();
    }
    return global_logger_;
}

void Logger::setConfig(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_ = config;
}

const Logger::LogConfig& Logger::getConfig() const {
    return config_;
}

void Logger::writeToFile(const std::string& formatted_message) {
    if (!file_stream_ || !file_stream_->is_open()) {
        return;
    }
    
    *file_stream_ << formatted_message << std::endl;
    file_stream_->flush();
    
    current_file_size_ += formatted_message.length() + 1; // +1 for newline
    
    if (current_file_size_ > config_.max_file_size) {
        rotateLogFile();
    }
}

void Logger::writeToConsole(const std::string& formatted_message) {
    std::cout << formatted_message << std::endl;
}

std::string Logger::formatMessage(LogLevel level, const std::string& message, const std::string& context) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    oss << " [" << logLevelToString(level) << "]";
    
    if (!context.empty()) {
        oss << " [" << context << "]";
    }
    
    oss << " " << message;
    return oss.str();
}

std::string Logger::formatJsonMessage(LogLevel level, const std::string& message, const std::string& context) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << "{";
    oss << "\"timestamp\":\"";
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z\"";
    oss << ",\"level\":\"" << logLevelToString(level) << "\"";
    
    if (!context.empty()) {
        oss << ",\"context\":\"" << context << "\"";
    }
    
    oss << ",\"message\":\"" << message << "\"";
    oss << "}";
    
    return oss.str();
}

std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::rotateLogFile() {
    if (!file_stream_ || !file_stream_->is_open()) {
        return;
    }
    
    file_stream_->close();
    
    // Rotate existing files
    for (size_t i = config_.max_files - 1; i > 0; --i) {
        std::string old_name = config_.filename + "." + std::to_string(i);
        std::string new_name = config_.filename + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(old_name)) {
            if (i == config_.max_files - 1) {
                std::filesystem::remove(new_name); // Remove oldest
            }
            std::filesystem::rename(old_name, new_name);
        }
    }
    
    // Move current log to .1
    std::string backup_name = config_.filename + ".1";
    std::filesystem::rename(config_.filename, backup_name);
    
    // Create new log file
    file_stream_ = std::make_unique<std::ofstream>(config_.filename, std::ios::app);
    current_file_size_ = 0;
}

bool Logger::shouldLog(LogLevel level) const {
    return level >= config_.min_level;
}

}