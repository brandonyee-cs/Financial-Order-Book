#pragma once
#include "PerformanceMeasurement.hpp"
#include "MemoryManager.hpp"
#include <unordered_map>
#include <vector>
#include <chrono>

namespace orderbook {

/**
 * @brief Comprehensive performance testing suite
 */
class PerformanceTest {
public:
    /**
     * @brief Test configuration
     */
    struct TestConfig {
        size_t num_orders = 100000;
        size_t num_threads = 1;
        double buy_sell_ratio = 0.5;
        Price min_price = 100.0;
        Price max_price = 200.0;
        Quantity min_quantity = 100;
        Quantity max_quantity = 1000;
        bool enable_risk_management = true;
        bool enable_market_data = true;
        std::string symbol = "AAPL";
    };
    
    /**
     * @brief Test results
     */
    struct TestResults {
        std::unordered_map<std::string, OperationStats> operation_stats;
        std::vector<PerformanceValidator::ValidationResult> validation_results;
        MemoryManager::MemoryStats memory_stats;
        std::chrono::milliseconds total_test_time{0};
        size_t orders_processed = 0;
        size_t trades_executed = 0;
        bool all_validations_passed = true;
    };
    
    /**
     * @brief Run comprehensive performance test
     * @param config Test configuration
     * @return Test results
     */
    static TestResults runPerformanceTest(const TestConfig& config = TestConfig{});
    
    /**
     * @brief Print detailed test results
     * @param results Test results to print
     */
    static void printResults(const TestResults& results);
    
    /**
     * @brief Run latency benchmark for specific operations
     */
    static void runLatencyBenchmark();
    
    /**
     * @brief Run throughput stress test
     */
    static void runThroughputStressTest(size_t target_ops_per_sec = 500000, 
                                       int duration_seconds = 10);

private:
    class OrderBook; // Forward declaration
    
    static TestResults runSingleThreadedTest(OrderBook& order_book, const TestConfig& config);
    static TestResults runMultiThreadedTest(OrderBook& order_book, const TestConfig& config);
};

} // namespace orderbook