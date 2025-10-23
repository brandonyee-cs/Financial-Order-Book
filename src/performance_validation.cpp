#include "orderbook/Utilities/PerformanceTest.hpp"
#include "orderbook/Utilities/PerformanceMeasurement.hpp"
#include "orderbook/Utilities/MemoryManager.hpp"
#include <iostream>
#include <string>

/**
 * @brief Performance validation executable
 * Tests the orderbook against stated performance requirements
 */
int main(int argc, char* argv[]) {
    std::cout << "OrderBook Performance Validation Suite\n";
    std::cout << "======================================\n\n";
    
    try {
        // Parse command line arguments
        bool run_latency_benchmark = false;
        bool run_throughput_stress = false;
        bool run_full_test = true;
        size_t num_orders = 100000;
        size_t num_threads = 1;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--latency") {
                run_latency_benchmark = true;
                run_full_test = false;
            } else if (arg == "--throughput") {
                run_throughput_stress = true;
                run_full_test = false;
            } else if (arg == "--orders" && i + 1 < argc) {
                num_orders = std::stoull(argv[++i]);
            } else if (arg == "--threads" && i + 1 < argc) {
                num_threads = std::stoull(argv[++i]);
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n";
                std::cout << "Options:\n";
                std::cout << "  --latency          Run latency benchmark only\n";
                std::cout << "  --throughput       Run throughput stress test only\n";
                std::cout << "  --orders N         Number of orders to process (default: 100000)\n";
                std::cout << "  --threads N        Number of threads to use (default: 1)\n";
                std::cout << "  --help             Show this help message\n";
                return 0;
            }
        }
        
        if (run_latency_benchmark) {
            std::cout << "Running Latency Benchmark...\n";
            orderbook::PerformanceTest::runLatencyBenchmark();
        }
        
        if (run_throughput_stress) {
            std::cout << "Running Throughput Stress Test...\n";
            orderbook::PerformanceTest::runThroughputStressTest();
        }
        
        if (run_full_test) {
            std::cout << "Running Full Performance Test...\n";
            
            // Configure test
            orderbook::PerformanceTest::TestConfig config;
            config.num_orders = num_orders;
            config.num_threads = num_threads;
            config.enable_risk_management = true;
            config.enable_market_data = true;
            
            // Run test
            auto results = orderbook::PerformanceTest::runPerformanceTest(config);
            
            // Print results
            orderbook::PerformanceTest::printResults(results);
            
            // Return appropriate exit code
            if (!results.all_validations_passed) {
                std::cout << "\nPERFORMANCE VALIDATION FAILED!\n";
                std::cout << "The system does not meet the stated performance requirements.\n";
                return 1;
            } else {
                std::cout << "\nPERFORMANCE VALIDATION PASSED!\n";
                std::cout << "The system meets all stated performance requirements.\n";
                return 0;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error during performance validation: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}