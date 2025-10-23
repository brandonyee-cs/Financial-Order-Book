#pragma once
#include <chrono>
#include <string>
#include <memory>

namespace orderbook {

class ILogger;

/**
 * @brief RAII-style performance timer for measuring operation latency
 */
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& operation_name, 
                             std::shared_ptr<ILogger> logger = nullptr);
    ~PerformanceTimer();
    
    // Get elapsed time without logging
    uint64_t getElapsedNs() const;
    
    // Manually stop and log (destructor won't log if called)
    void stopAndLog();
    
    // Add additional metrics to be logged
    void addMetric(const std::string& key, const std::string& value);
    
private:
    std::string operation_name_;
    std::shared_ptr<ILogger> logger_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::vector<std::pair<std::string, std::string>> additional_metrics_;
    bool logged_;
};

/**
 * @brief Statistics collector for performance metrics
 */
class PerformanceStats {
public:
    void recordLatency(uint64_t latency_ns);
    void recordThroughput(uint64_t count, uint64_t duration_ns);
    
    uint64_t getMinLatency() const { return min_latency_; }
    uint64_t getMaxLatency() const { return max_latency_; }
    uint64_t getAvgLatency() const;
    uint64_t getSampleCount() const { return sample_count_; }
    
    void reset();
    
private:
    uint64_t min_latency_ = UINT64_MAX;
    uint64_t max_latency_ = 0;
    uint64_t total_latency_ = 0;
    uint64_t sample_count_ = 0;
};

// Convenience macro for timing operations
#define PERF_TIMER(operation_name, logger) \
    PerformanceTimer _perf_timer(operation_name, logger)

#define PERF_TIMER_WITH_METRICS(operation_name, logger, ...) \
    PerformanceTimer _perf_timer(operation_name, logger); \
    do { __VA_ARGS__ } while(0)

}