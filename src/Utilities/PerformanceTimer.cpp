#include "orderbook/Utilities/PerformanceTimer.hpp"
#include "orderbook/Utilities/Logger.hpp"

namespace orderbook {

PerformanceTimer::PerformanceTimer(const std::string& operation_name, 
                                 std::shared_ptr<ILogger> logger)
    : operation_name_(operation_name)
    , logger_(logger)
    , start_time_(std::chrono::high_resolution_clock::now())
    , logged_(false) {
}

PerformanceTimer::~PerformanceTimer() {
    if (!logged_ && logger_) {
        stopAndLog();
    }
}

uint64_t PerformanceTimer::getElapsedNs() const {
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time_).count();
}

void PerformanceTimer::stopAndLog() {
    if (logged_ || !logger_) {
        return;
    }
    
    uint64_t elapsed_ns = getElapsedNs();
    logger_->logPerformance(operation_name_, elapsed_ns, additional_metrics_);
    logged_ = true;
}

void PerformanceTimer::addMetric(const std::string& key, const std::string& value) {
    additional_metrics_.emplace_back(key, value);
}

// PerformanceStats implementation

void PerformanceStats::recordLatency(uint64_t latency_ns) {
    min_latency_ = std::min(min_latency_, latency_ns);
    max_latency_ = std::max(max_latency_, latency_ns);
    total_latency_ += latency_ns;
    sample_count_++;
}

void PerformanceStats::recordThroughput(uint64_t count, uint64_t duration_ns) {
    // For throughput, we can record the effective latency per operation
    if (count > 0) {
        uint64_t avg_latency_per_op = duration_ns / count;
        recordLatency(avg_latency_per_op);
    }
}

uint64_t PerformanceStats::getAvgLatency() const {
    return sample_count_ > 0 ? total_latency_ / sample_count_ : 0;
}

void PerformanceStats::reset() {
    min_latency_ = UINT64_MAX;
    max_latency_ = 0;
    total_latency_ = 0;
    sample_count_ = 0;
}

}