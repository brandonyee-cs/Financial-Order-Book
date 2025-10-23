#pragma once
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <numeric>
#include <thread>

namespace orderbook {

/**
 * @brief High-precision performance measurement utilities
 * Provides latency and throughput measurement with minimal overhead
 */
class PerformanceMeasurement {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;
    
    /**
     * @brief Performance statistics for an operation
     */
    struct OperationStats {
        std::string operation_name;
        size_t sample_count = 0;
        Duration min_latency{Duration::max()};
        Duration max_latency{Duration::zero()};
        Duration avg_latency{Duration::zero()};
        Duration p50_latency{Duration::zero()};
        Duration p95_latency{Duration::zero()};
        Duration p99_latency{Duration::zero()};
        Duration total_time{Duration::zero()};
        double throughput_ops_per_sec = 0.0;
        
        // Memory statistics
        size_t peak_memory_usage = 0;
        size_t avg_memory_usage = 0;
    };
    
    /**
     * @brief RAII timer for measuring operation latency
     */
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& operation_name, PerformanceMeasurement& perf)
            : operation_name_(operation_name), perf_(perf), start_time_(Clock::now()) {}
        
        ~ScopedTimer() {
            auto end_time = Clock::now();
            auto duration = std::chrono::duration_cast<Duration>(end_time - start_time_);
            perf_.recordLatency(operation_name_, duration);
        }
        
        // Non-copyable, non-movable
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;
        
    private:
        std::string operation_name_;
        PerformanceMeasurement& perf_;
        TimePoint start_time_;
    };
    
    /**
     * @brief Throughput measurement helper
     */
    class ThroughputMeasurement {
    public:
        ThroughputMeasurement() : start_time_(Clock::now()) {}
        
        void recordOperation() {
            operation_count_.fetch_add(1, std::memory_order_relaxed);
        }
        
        double getCurrentThroughput() const {
            auto current_time = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time_).count();
            
            if (elapsed == 0) return 0.0;
            
            return (operation_count_.load(std::memory_order_relaxed) * 1000.0) / elapsed;
        }
        
        void reset() {
            start_time_ = Clock::now();
            operation_count_.store(0, std::memory_order_relaxed);
        }
        
    private:
        TimePoint start_time_;
        std::atomic<size_t> operation_count_{0};
    };
    
    /**
     * @brief Get singleton instance
     */
    static PerformanceMeasurement& getInstance() {
        static PerformanceMeasurement instance;
        return instance;
    }
    
    /**
     * @brief Create a scoped timer for an operation
     * @param operation_name Name of the operation being measured
     * @return RAII timer object
     */
    ScopedTimer createTimer(const std::string& operation_name) {
        return ScopedTimer(operation_name, *this);
    }
    
    /**
     * @brief Record latency for an operation
     * @param operation_name Name of the operation
     * @param latency Measured latency
     */
    void recordLatency(const std::string& operation_name, Duration latency) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& samples = latency_samples_[operation_name];
        samples.push_back(latency);
        
        // Limit sample size to prevent unbounded growth
        constexpr size_t MAX_SAMPLES = 10000;
        if (samples.size() > MAX_SAMPLES) {
            samples.erase(samples.begin(), samples.begin() + (samples.size() - MAX_SAMPLES));
        }
        
        // Update throughput
        throughput_measurements_[operation_name].recordOperation();
    }
    
    /**
     * @brief Get statistics for a specific operation
     * @param operation_name Name of the operation
     * @return Operation statistics
     */
    OperationStats getOperationStats(const std::string& operation_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = latency_samples_.find(operation_name);
        if (it == latency_samples_.end() || it->second.empty()) {
            return OperationStats{operation_name, 0};
        }
        
        const auto& samples = it->second;
        OperationStats stats;
        stats.operation_name = operation_name;
        stats.sample_count = samples.size();
        
        // Calculate basic statistics
        stats.min_latency = *std::min_element(samples.begin(), samples.end());
        stats.max_latency = *std::max_element(samples.begin(), samples.end());
        stats.total_time = std::accumulate(samples.begin(), samples.end(), Duration::zero());
        stats.avg_latency = Duration(stats.total_time.count() / samples.size());
        
        // Calculate percentiles
        auto sorted_samples = samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        stats.p50_latency = sorted_samples[samples.size() * 50 / 100];
        stats.p95_latency = sorted_samples[samples.size() * 95 / 100];
        stats.p99_latency = sorted_samples[samples.size() * 99 / 100];
        
        // Get throughput
        auto throughput_it = throughput_measurements_.find(operation_name);
        if (throughput_it != throughput_measurements_.end()) {
            stats.throughput_ops_per_sec = throughput_it->second.getCurrentThroughput();
        }
        
        return stats;
    }
    
    /**
     * @brief Get statistics for all measured operations
     * @return Map of operation name to statistics
     */
    std::unordered_map<std::string, OperationStats> getAllStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unordered_map<std::string, OperationStats> all_stats;
        for (const auto& [operation_name, samples] : latency_samples_) {
            all_stats[operation_name] = getOperationStats(operation_name);
        }
        
        return all_stats;
    }
    
    /**
     * @brief Reset all measurements
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        latency_samples_.clear();
        for (auto& [name, throughput] : throughput_measurements_) {
            throughput.reset();
        }
    }
    
    /**
     * @brief Start continuous performance monitoring
     * @param interval_ms Monitoring interval in milliseconds
     */
    void startMonitoring(int interval_ms = 1000) {
        if (monitoring_active_.exchange(true)) {
            return; // Already monitoring
        }
        
        monitoring_thread_ = std::thread([this, interval_ms]() {
            while (monitoring_active_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                
                // Record current statistics
                auto stats = getAllStats();
                std::lock_guard<std::mutex> lock(history_mutex_);
                performance_history_.push_back({Clock::now(), stats});
                
                // Limit history size
                constexpr size_t MAX_HISTORY = 1000;
                if (performance_history_.size() > MAX_HISTORY) {
                    performance_history_.erase(performance_history_.begin());
                }
            }
        });
    }
    
    /**
     * @brief Stop continuous performance monitoring
     */
    void stopMonitoring() {
        monitoring_active_.store(false);
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
    
    /**
     * @brief Get performance history
     */
    struct HistoryEntry {
        TimePoint timestamp;
        std::unordered_map<std::string, OperationStats> stats;
    };
    
    std::vector<HistoryEntry> getPerformanceHistory() const {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return performance_history_;
    }
    
private:
    PerformanceMeasurement() = default;
    ~PerformanceMeasurement() {
        stopMonitoring();
    }
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Duration>> latency_samples_;
    std::unordered_map<std::string, ThroughputMeasurement> throughput_measurements_;
    
    // Continuous monitoring
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    mutable std::mutex history_mutex_;
    std::vector<HistoryEntry> performance_history_;
};

/**
 * @brief Performance validation against target requirements
 */
class PerformanceValidator {
public:
    /**
     * @brief Performance targets from requirements
     */
    struct PerformanceTargets {
        Duration max_order_add_latency{std::chrono::microseconds(50)};      // <50μs
        Duration max_order_cancel_latency{std::chrono::microseconds(10)};   // <10μs
        Duration max_best_price_latency{std::chrono::microseconds(1)};      // <1μs
        double min_throughput_ops_per_sec = 500000.0;                       // 500,000+ ops/sec
        size_t max_memory_per_order = 128;                                  // 128 bytes per order
    };
    
    /**
     * @brief Validation result
     */
    struct ValidationResult {
        bool passed = false;
        std::string operation_name;
        std::string failure_reason;
        OperationStats actual_stats;
        PerformanceTargets targets;
    };
    
    /**
     * @brief Validate performance against targets
     * @param stats Operation statistics to validate
     * @param targets Performance targets
     * @return Validation result
     */
    static ValidationResult validatePerformance(const OperationStats& stats, 
                                               const PerformanceTargets& targets) {
        ValidationResult result;
        result.operation_name = stats.operation_name;
        result.actual_stats = stats;
        result.targets = targets;
        result.passed = true;
        
        // Validate latency targets
        if (stats.operation_name == "OrderBook::addOrder") {
            if (stats.p95_latency > targets.max_order_add_latency) {
                result.passed = false;
                result.failure_reason = "Order add latency exceeds target: " +
                    std::to_string(stats.p95_latency.count()) + "ns > " +
                    std::to_string(targets.max_order_add_latency.count()) + "ns";
            }
        } else if (stats.operation_name == "OrderBook::cancelOrder") {
            if (stats.p95_latency > targets.max_order_cancel_latency) {
                result.passed = false;
                result.failure_reason = "Order cancel latency exceeds target: " +
                    std::to_string(stats.p95_latency.count()) + "ns > " +
                    std::to_string(targets.max_order_cancel_latency.count()) + "ns";
            }
        } else if (stats.operation_name == "OrderBook::bestBid" || 
                   stats.operation_name == "OrderBook::bestAsk") {
            if (stats.p95_latency > targets.max_best_price_latency) {
                result.passed = false;
                result.failure_reason = "Best price query latency exceeds target: " +
                    std::to_string(stats.p95_latency.count()) + "ns > " +
                    std::to_string(targets.max_best_price_latency.count()) + "ns";
            }
        }
        
        // Validate throughput
        if (stats.throughput_ops_per_sec < targets.min_throughput_ops_per_sec) {
            result.passed = false;
            result.failure_reason += (result.failure_reason.empty() ? "" : "; ") +
                "Throughput below target: " + std::to_string(stats.throughput_ops_per_sec) +
                " ops/sec < " + std::to_string(targets.min_throughput_ops_per_sec) + " ops/sec";
        }
        
        return result;
    }
    
    /**
     * @brief Run comprehensive performance validation
     * @return Vector of validation results for all operations
     */
    static std::vector<ValidationResult> validateAllOperations() {
        auto& perf = PerformanceMeasurement::getInstance();
        auto all_stats = perf.getAllStats();
        
        PerformanceTargets targets;
        std::vector<ValidationResult> results;
        
        for (const auto& [operation_name, stats] : all_stats) {
            if (stats.sample_count > 0) {
                results.push_back(validatePerformance(stats, targets));
            }
        }
        
        return results;
    }
};

// Convenience macros for performance measurement
#define PERF_MEASURE(operation_name) \
    auto _perf_timer = orderbook::PerformanceMeasurement::getInstance().createTimer(operation_name)

#define PERF_MEASURE_SCOPE(operation_name) \
    orderbook::PerformanceMeasurement::ScopedTimer _perf_timer(operation_name, \
        orderbook::PerformanceMeasurement::getInstance())

} // namespace orderbook