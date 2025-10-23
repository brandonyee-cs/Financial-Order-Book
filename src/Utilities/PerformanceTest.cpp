#include "orderbook/Utilities/PerformanceMeasurement.hpp"
#include "orderbook/Utilities/MemoryManager.hpp"
#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Core/MatchingEngine.hpp"
#include "orderbook/Risk/RiskManager.hpp"
#include "orderbook/MarketData/MarketDataFeed.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <iomanip>

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
    static TestResults runPerformanceTest(const TestConfig& config = TestConfig{}) {
        std::cout << "Starting performance test with " << config.num_orders 
                  << " orders on " << config.num_threads << " threads...\n";
        
        // Initialize components
        auto logger = std::make_shared<Logger>(LogLevel::INFO);
        auto risk_manager = config.enable_risk_management ? 
            std::make_shared<RiskManager>(logger) : nullptr;
        auto market_data = config.enable_market_data ? 
            std::make_shared<MarketDataFeed>(logger) : nullptr;
        
        // Create OrderBook
        OrderBook order_book(risk_manager, market_data, logger);
        
        // Pre-warm memory pools
        MemoryManager::getInstance().prewarmPools();
        MemoryManager::getInstance().resetStats();
        
        // Reset performance measurements
        auto& perf = PerformanceMeasurement::getInstance();
        perf.reset();
        perf.startMonitoring(100); // Monitor every 100ms
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Run test based on thread count
        TestResults results;
        if (config.num_threads == 1) {
            results = runSingleThreadedTest(order_book, config);
        } else {
            results = runMultiThreadedTest(order_book, config);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        results.total_test_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        perf.stopMonitoring();
        
        // Collect final statistics
        results.operation_stats = perf.getAllStats();
        results.validation_results = PerformanceValidator::validateAllOperations();
        results.memory_stats = MemoryManager::getInstance().getStats();
        
        // Check if all validations passed
        results.all_validations_passed = std::all_of(
            results.validation_results.begin(), 
            results.validation_results.end(),
            [](const auto& result) { return result.passed; });
        
        return results;
    }
    
    /**
     * @brief Print detailed test results
     * @param results Test results to print
     */
    static void printResults(const TestResults& results) {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "PERFORMANCE TEST RESULTS\n";
        std::cout << std::string(80, '=') << "\n";
        
        // Overall summary
        std::cout << "Test Duration: " << results.total_test_time.count() << " ms\n";
        std::cout << "Orders Processed: " << results.orders_processed << "\n";
        std::cout << "Trades Executed: " << results.trades_executed << "\n";
        std::cout << "Overall Throughput: " << 
            (results.orders_processed * 1000.0 / results.total_test_time.count()) << " orders/sec\n";
        std::cout << "Validation Status: " << 
            (results.all_validations_passed ? "PASSED" : "FAILED") << "\n\n";
        
        // Operation statistics
        std::cout << "OPERATION LATENCY STATISTICS\n";
        std::cout << std::string(80, '-') << "\n";
        std::cout << std::left << std::setw(25) << "Operation"
                  << std::setw(10) << "Count"
                  << std::setw(12) << "Avg (μs)"
                  << std::setw(12) << "P95 (μs)"
                  << std::setw(12) << "P99 (μs)"
                  << std::setw(15) << "Throughput/s" << "\n";
        std::cout << std::string(80, '-') << "\n";
        
        for (const auto& [operation_name, stats] : results.operation_stats) {
            std::cout << std::left << std::setw(25) << operation_name
                      << std::setw(10) << stats.sample_count
                      << std::setw(12) << std::fixed << std::setprecision(2) 
                      << (stats.avg_latency.count() / 1000.0)
                      << std::setw(12) << (stats.p95_latency.count() / 1000.0)
                      << std::setw(12) << (stats.p99_latency.count() / 1000.0)
                      << std::setw(15) << std::fixed << std::setprecision(0) 
                      << stats.throughput_ops_per_sec << "\n";
        }
        
        // Memory statistics
        std::cout << "\nMEMORY STATISTICS\n";
        std::cout << std::string(80, '-') << "\n";
        std::cout << "Order Pool - Available: " << results.memory_stats.order_pool.available
                  << ", Total Created: " << results.memory_stats.order_pool.total_created << "\n";
        std::cout << "Trade Pool - Available: " << results.memory_stats.trade_pool.available
                  << ", Total Created: " << results.memory_stats.trade_pool.total_created << "\n";
        std::cout << "Peak Memory Usage: " << (results.memory_stats.peak_memory_used / 1024) << " KB\n";
        std::cout << "Avg Allocation Time: " << results.memory_stats.avg_allocation_time.count() << " ns\n";
        
        // Validation results
        std::cout << "\nPERFORMANCE VALIDATION\n";
        std::cout << std::string(80, '-') << "\n";
        for (const auto& validation : results.validation_results) {
            std::cout << validation.operation_name << ": " 
                      << (validation.passed ? "PASSED" : "FAILED");
            if (!validation.passed) {
                std::cout << " - " << validation.failure_reason;
            }
            std::cout << "\n";
        }
        
        std::cout << std::string(80, '=') << "\n";
    }
    
    /**
     * @brief Run latency benchmark for specific operations
     */
    static void runLatencyBenchmark() {
        std::cout << "Running latency benchmark...\n";
        
        auto logger = std::make_shared<Logger>(LogLevel::ERROR); // Minimal logging
        OrderBook order_book(nullptr, nullptr, logger);
        
        // Warm up
        for (int i = 0; i < 1000; ++i) {
            Order order(i, Side::Buy, OrderType::Limit, 100.0 + i * 0.01, 100, "AAPL");
            order_book.addOrder(order);
        }
        
        // Benchmark order operations
        const int iterations = 10000;
        
        // Add order benchmark
        {
            PERF_MEASURE("Benchmark::AddOrder");
            for (int i = 0; i < iterations; ++i) {
                Order order(1000000 + i, Side::Buy, OrderType::Limit, 
                           100.0 + (i % 100) * 0.01, 100, "AAPL");
                order_book.addOrder(order);
            }
        }
        
        // Best price query benchmark
        {
            PERF_MEASURE("Benchmark::BestPrice");
            for (int i = 0; i < iterations; ++i) {
                volatile auto bid = order_book.bestBid();
                volatile auto ask = order_book.bestAsk();
                (void)bid; (void)ask; // Prevent optimization
            }
        }
        
        // Cancel order benchmark
        {
            PERF_MEASURE("Benchmark::CancelOrder");
            for (int i = 0; i < iterations / 2; ++i) {
                order_book.cancelOrder(OrderId(1000000 + i));
            }
        }
        
        auto stats = PerformanceMeasurement::getInstance().getAllStats();
        std::cout << "Latency Benchmark Results:\n";
        for (const auto& [name, stat] : stats) {
            if (name.find("Benchmark::") == 0) {
                std::cout << name << " - Avg: " << (stat.avg_latency.count() / 1000.0) 
                          << "μs, P95: " << (stat.p95_latency.count() / 1000.0) << "μs\n";
            }
        }
    }
    
    /**
     * @brief Run throughput stress test
     */
    static void runThroughputStressTest(size_t target_ops_per_sec = 500000, 
                                       int duration_seconds = 10) {
        std::cout << "Running throughput stress test for " << duration_seconds 
                  << " seconds targeting " << target_ops_per_sec << " ops/sec...\n";
        
        auto logger = std::make_shared<Logger>(LogLevel::ERROR);
        OrderBook order_book(nullptr, nullptr, logger);
        
        std::atomic<size_t> operations_completed{0};
        std::atomic<bool> stop_test{false};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Worker thread
        std::thread worker([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> price_dist(100.0, 200.0);
            std::uniform_int_distribution<> qty_dist(100, 1000);
            std::uniform_int_distribution<> side_dist(0, 1);
            
            uint64_t order_id = 1;
            while (!stop_test.load()) {
                Order order(order_id++, 
                           side_dist(gen) ? Side::Buy : Side::Sell,
                           OrderType::Limit,
                           price_dist(gen),
                           qty_dist(gen),
                           "AAPL");
                
                {
                    PERF_MEASURE("StressTest::AddOrder");
                    order_book.addOrder(order);
                }
                
                operations_completed.fetch_add(1);
                
                // Throttle if needed
                auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                if (elapsed_ms > 0) {
                    auto current_rate = operations_completed.load() * 1000 / elapsed_ms;
                    if (current_rate > target_ops_per_sec) {
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                }
            }
        });
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
        stop_test.store(true);
        worker.join();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        double actual_throughput = operations_completed.load() * 1000.0 / actual_duration;
        
        std::cout << "Stress Test Results:\n";
        std::cout << "Operations Completed: " << operations_completed.load() << "\n";
        std::cout << "Actual Duration: " << actual_duration << " ms\n";
        std::cout << "Achieved Throughput: " << actual_throughput << " ops/sec\n";
        std::cout << "Target Achievement: " << (actual_throughput / target_ops_per_sec * 100.0) << "%\n";
        
        auto stats = PerformanceMeasurement::getInstance().getOperationStats("StressTest::AddOrder");
        if (stats.sample_count > 0) {
            std::cout << "Average Latency: " << (stats.avg_latency.count() / 1000.0) << "μs\n";
            std::cout << "P95 Latency: " << (stats.p95_latency.count() / 1000.0) << "μs\n";
        }
    }

private:
    static TestResults runSingleThreadedTest(OrderBook& order_book, const TestConfig& config) {
        TestResults results;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(config.min_price, config.max_price);
        std::uniform_int_distribution<> qty_dist(config.min_quantity, config.max_quantity);
        std::uniform_real_distribution<> side_dist(0.0, 1.0);
        
        for (size_t i = 0; i < config.num_orders; ++i) {
            Side side = side_dist(gen) < config.buy_sell_ratio ? Side::Buy : Side::Sell;
            Order order(i + 1, side, OrderType::Limit, price_dist(gen), qty_dist(gen), config.symbol);
            
            {
                PERF_MEASURE("OrderBook::addOrder");
                auto result = order_book.addOrder(order);
                if (result.isSuccess()) {
                    results.orders_processed++;
                }
            }
            
            // Occasionally query best prices
            if (i % 100 == 0) {
                PERF_MEASURE("OrderBook::bestBid");
                order_book.bestBid();
                
                PERF_MEASURE("OrderBook::bestAsk");
                order_book.bestAsk();
            }
            
            // Occasionally cancel orders
            if (i % 500 == 0 && i > 100) {
                PERF_MEASURE("OrderBook::cancelOrder");
                order_book.cancelOrder(OrderId(i - 100));
            }
        }
        
        return results;
    }
    
    static TestResults runMultiThreadedTest(OrderBook& order_book, const TestConfig& config) {
        TestResults results;
        std::atomic<size_t> orders_processed{0};
        
        std::vector<std::thread> threads;
        size_t orders_per_thread = config.num_orders / config.num_threads;
        
        for (size_t t = 0; t < config.num_threads; ++t) {
            threads.emplace_back([&, t, orders_per_thread, config]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t); // Different seed per thread
                std::uniform_real_distribution<> price_dist(config.min_price, config.max_price);
                std::uniform_int_distribution<> qty_dist(config.min_quantity, config.max_quantity);
                std::uniform_real_distribution<> side_dist(0.0, 1.0);
                
                size_t start_id = t * orders_per_thread + 1;
                for (size_t i = 0; i < orders_per_thread; ++i) {
                    Side side = side_dist(gen) < config.buy_sell_ratio ? Side::Buy : Side::Sell;
                    Order order(start_id + i, side, OrderType::Limit, 
                               price_dist(gen), qty_dist(gen), config.symbol);
                    
                    {
                        PERF_MEASURE("OrderBook::addOrder");
                        auto result = order_book.addOrder(order);
                        if (result.isSuccess()) {
                            orders_processed.fetch_add(1);
                        }
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        results.orders_processed = orders_processed.load();
        return results;
    }
};

} // namespace orderbook