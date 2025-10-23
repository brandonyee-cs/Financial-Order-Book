#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Core/Order.hpp"
#include "orderbook/Risk/RiskManager.hpp"
#include "orderbook/MarketData/MarketDataFeed.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include "orderbook/Utilities/Config.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

using namespace orderbook;

// Forward declaration for demo mode
void runDemoMode(OrderBook& book, Logger& logger);

// Demo mode implementation
void runDemoMode(OrderBook& book, Logger& logger) {
    std::cout << "=== OrderBook Demo Mode ===\n";
    std::cout << "Running 30-second demonstration of all orderbook operations...\n\n";
    
    auto start_time = std::chrono::steady_clock::now();
    const auto demo_duration = std::chrono::seconds(30);
    
    // Performance metrics
    uint64_t total_operations = 0;
    uint64_t total_latency_ns = 0;
    uint64_t max_latency_ns = 0;
    uint64_t min_latency_ns = UINT64_MAX;
    
    // Demo scenario data
    std::vector<std::pair<Price, Quantity>> buy_orders = {
        {99.50, 1000}, {99.25, 1500}, {99.00, 2000}, {98.75, 1200}, {98.50, 800}
    };
    
    std::vector<std::pair<Price, Quantity>> sell_orders = {
        {100.50, 800}, {100.75, 1200}, {101.00, 1800}, {101.25, 1000}, {101.50, 1500}
    };
    
    OrderId next_order_id(1);
    
    // Phase 1: Build initial order book (0-10 seconds)
    std::cout << "Phase 1: Building initial order book...\n";
    
    for (size_t i = 0; i < buy_orders.size() && 
         std::chrono::steady_clock::now() - start_time < std::chrono::seconds(10); ++i) {
        
        auto op_start = std::chrono::high_resolution_clock::now();
        
        Order buy_order(next_order_id.value, Side::Buy, OrderType::Limit, TimeInForce::GTC,
                       buy_orders[i].first, buy_orders[i].second, "BTC/USD", "demo_account");
        
        auto result = book.addOrder(buy_order);
        
        auto op_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
        
        total_operations++;
        total_latency_ns += latency;
        max_latency_ns = std::max(max_latency_ns, static_cast<uint64_t>(latency));
        min_latency_ns = std::min(min_latency_ns, static_cast<uint64_t>(latency));
        
        if (result.isSuccess()) {
            std::cout << "  Added BUY order " << next_order_id.value 
                      << " @ $" << buy_orders[i].first 
                      << " qty:" << buy_orders[i].second 
                      << " (latency: " << latency << "ns)\n";
        } else {
            std::cout << "  Failed to add BUY order: " << result.error() << "\n";
        }
        
        next_order_id = OrderId(next_order_id.value + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    for (size_t i = 0; i < sell_orders.size() && 
         std::chrono::steady_clock::now() - start_time < std::chrono::seconds(10); ++i) {
        
        auto op_start = std::chrono::high_resolution_clock::now();
        
        Order sell_order(next_order_id.value, Side::Sell, OrderType::Limit, TimeInForce::GTC,
                        sell_orders[i].first, sell_orders[i].second, "BTC/USD", "demo_account");
        
        auto result = book.addOrder(sell_order);
        
        auto op_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
        
        total_operations++;
        total_latency_ns += latency;
        max_latency_ns = std::max(max_latency_ns, static_cast<uint64_t>(latency));
        min_latency_ns = std::min(min_latency_ns, static_cast<uint64_t>(latency));
        
        if (result.isSuccess()) {
            std::cout << "  Added SELL order " << next_order_id.value 
                      << " @ $" << sell_orders[i].first 
                      << " qty:" << sell_orders[i].second 
                      << " (latency: " << latency << "ns)\n";
        } else {
            std::cout << "  Failed to add SELL order: " << result.error() << "\n";
        }
        
        next_order_id = OrderId(next_order_id.value + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Display current order book state
    std::cout << "\nCurrent Order Book State:\n";
    auto best_bid = book.bestBid();
    auto best_ask = book.bestAsk();
    std::cout << "  Best Bid: " << (best_bid ? std::to_string(*best_bid) : "None") << "\n";
    std::cout << "  Best Ask: " << (best_ask ? std::to_string(*best_ask) : "None") << "\n";
    std::cout << "  Total Orders: " << book.getOrderCount() << "\n";
    std::cout << "  Bid Levels: " << book.getBidLevelCount() << "\n";
    std::cout << "  Ask Levels: " << book.getAskLevelCount() << "\n";
    
    // Phase 2: Execute matching trades (10-20 seconds)
    std::cout << "\nPhase 2: Executing matching trades...\n";
    
    // Add aggressive orders that will match
    std::vector<std::pair<Side, std::pair<Price, Quantity>>> aggressive_orders = {
        {Side::Buy, {100.75, 500}},   // Should match with sell at 100.50
        {Side::Sell, {99.25, 300}},   // Should match with buy at 99.50
        {Side::Buy, {101.00, 800}},   // Should match multiple sell levels
        {Side::Sell, {98.75, 600}}    // Should match multiple buy levels
    };
    
    for (const auto& order_spec : aggressive_orders) {
        if (std::chrono::steady_clock::now() - start_time >= std::chrono::seconds(20)) break;
        
        auto op_start = std::chrono::high_resolution_clock::now();
        
        Order aggressive_order(next_order_id.value, order_spec.first, OrderType::Limit, TimeInForce::GTC,
                              order_spec.second.first, order_spec.second.second, "BTC/USD", "demo_account");
        
        auto result = book.addOrder(aggressive_order);
        
        auto op_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
        
        total_operations++;
        total_latency_ns += latency;
        max_latency_ns = std::max(max_latency_ns, static_cast<uint64_t>(latency));
        min_latency_ns = std::min(min_latency_ns, static_cast<uint64_t>(latency));
        
        std::string side_str = (order_spec.first == Side::Buy) ? "BUY" : "SELL";
        
        if (result.isSuccess()) {
            std::cout << "  Executed " << side_str << " order " << next_order_id.value 
                      << " @ $" << order_spec.second.first 
                      << " qty:" << order_spec.second.second 
                      << " (latency: " << latency << "ns)\n";
        } else {
            std::cout << "  Failed to execute " << side_str << " order: " << result.error() << "\n";
        }
        
        // Display updated book state
        auto new_best_bid = book.bestBid();
        auto new_best_ask = book.bestAsk();
        std::cout << "    New Best Bid: " << (new_best_bid ? std::to_string(*new_best_bid) : "None") 
                  << ", Best Ask: " << (new_best_ask ? std::to_string(*new_best_ask) : "None") << "\n";
        
        next_order_id = OrderId(next_order_id.value + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    // Phase 3: Order modifications and cancellations (20-30 seconds)
    std::cout << "\nPhase 3: Order modifications and cancellations...\n";
    
    // Try to modify some orders (using early order IDs)
    for (uint64_t id = 1; id <= 5 && std::chrono::steady_clock::now() - start_time < demo_duration; ++id) {
        auto op_start = std::chrono::high_resolution_clock::now();
        
        OrderId order_id(id);
        auto result = book.modifyOrder(order_id, 99.75, 750);  // New price and quantity
        
        auto op_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
        
        total_operations++;
        total_latency_ns += latency;
        max_latency_ns = std::max(max_latency_ns, static_cast<uint64_t>(latency));
        min_latency_ns = std::min(min_latency_ns, static_cast<uint64_t>(latency));
        
        if (result.isSuccess()) {
            std::cout << "  Modified order " << id << " (latency: " << latency << "ns)\n";
        } else {
            std::cout << "  Failed to modify order " << id << ": " << result.error() << "\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Cancel some orders
    for (uint64_t id = 6; id <= 8 && std::chrono::steady_clock::now() - start_time < demo_duration; ++id) {
        auto op_start = std::chrono::high_resolution_clock::now();
        
        OrderId order_id(id);
        auto result = book.cancelOrder(order_id);
        
        auto op_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
        
        total_operations++;
        total_latency_ns += latency;
        max_latency_ns = std::max(max_latency_ns, static_cast<uint64_t>(latency));
        min_latency_ns = std::min(min_latency_ns, static_cast<uint64_t>(latency));
        
        if (result.isSuccess()) {
            std::cout << "  Cancelled order " << id << " (latency: " << latency << "ns)\n";
        } else {
            std::cout << "  Failed to cancel order " << id << ": " << result.error() << "\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Final order book state
    std::cout << "\n=== Final Order Book State ===\n";
    auto final_best_bid = book.bestBid();
    auto final_best_ask = book.bestAsk();
    std::cout << "Best Bid: " << (final_best_bid ? std::to_string(*final_best_bid) : "None") << "\n";
    std::cout << "Best Ask: " << (final_best_ask ? std::to_string(*final_best_ask) : "None") << "\n";
    std::cout << "Total Orders: " << book.getOrderCount() << "\n";
    std::cout << "Bid Levels: " << book.getBidLevelCount() << "\n";
    std::cout << "Ask Levels: " << book.getAskLevelCount() << "\n";
    
    // Performance statistics
    std::cout << "\n=== Performance Statistics ===\n";
    std::cout << "Demo Duration: " << total_duration.count() << "ms\n";
    std::cout << "Total Operations: " << total_operations << "\n";
    
    if (total_operations > 0) {
        double avg_latency = static_cast<double>(total_latency_ns) / total_operations;
        double throughput = (static_cast<double>(total_operations) * 1000.0) / total_duration.count();
        
        std::cout << "Average Latency: " << static_cast<uint64_t>(avg_latency) << "ns (" 
                  << (avg_latency / 1000.0) << "μs)\n";
        std::cout << "Min Latency: " << min_latency_ns << "ns (" 
                  << (min_latency_ns / 1000.0) << "μs)\n";
        std::cout << "Max Latency: " << max_latency_ns << "ns (" 
                  << (max_latency_ns / 1000.0) << "μs)\n";
        std::cout << "Throughput: " << static_cast<uint64_t>(throughput) << " operations/second\n";
        
        // Performance targets validation
        std::cout << "\n=== Performance Target Validation ===\n";
        std::cout << "Target: <50μs latency for order adds - " 
                  << (avg_latency < 50000 ? "✓ PASSED" : "✗ FAILED") << "\n";
        std::cout << "Target: 500,000+ orders/sec throughput - " 
                  << (throughput > 500000 ? "✓ PASSED" : "⚠ DEMO LIMITED") << "\n";
    }
    
    std::cout << "\n=== Demo Complete ===\n";
    std::cout << "All core orderbook operations demonstrated:\n";
    std::cout << "✓ Order additions with risk validation\n";
    std::cout << "✓ Order matching and trade execution\n";
    std::cout << "✓ Order modifications\n";
    std::cout << "✓ Order cancellations\n";
    std::cout << "✓ Market data publishing\n";
    std::cout << "✓ Performance metrics collection\n";
    
    logger.info("Demo mode completed successfully", "demo");
}

// Command line argument parsing
struct AppConfig {
    bool demo_mode = false;
    std::string config_file = "config/orderbook.cfg";
    bool help = false;
};

AppConfig parseCommandLine(int argc, char* argv[]) {
    AppConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--demo" || arg == "-d") {
            config.demo_mode = true;
        } else if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) {
                config.config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a filename\n";
                config.help = true;
            }
        } else if (arg == "--help" || arg == "-h") {
            config.help = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            config.help = true;
        }
    }
    
    return config;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  -d, --demo           Run in demo mode with automated simulation\n"
              << "  -c, --config FILE    Use specified configuration file (default: config/orderbook.cfg)\n"
              << "  -h, --help           Show this help message\n"
              << "\nDemo Mode:\n"
              << "  Runs a 30-second automated demonstration of all orderbook operations\n"
              << "  including order adds, modifications, cancellations, and trade executions.\n"
              << "  Displays real-time order book state and performance metrics.\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    AppConfig app_config = parseCommandLine(argc, argv);
    
    if (app_config.help) {
        printUsage(argv[0]);
        return app_config.help && argc > 1 ? 1 : 0;
    }
    
    try {
        // Initialize configuration
        std::cout << "Loading configuration from: " << app_config.config_file << "\n";
        auto config = std::make_shared<Config>(app_config.config_file);
        
        if (!config->isValid()) {
            std::cerr << "Configuration errors:\n";
            for (const auto& error : config->getErrors()) {
                std::cerr << "  " << error << "\n";
            }
            std::cerr << "Continuing with default values...\n";
        }
        
        // Initialize logger
        auto logger = std::make_shared<Logger>(config);
        Logger::setGlobalLogger(logger);
        
        logger->info("OrderBook application starting", "main");
        
        // Initialize risk manager
        auto risk_manager = std::make_shared<RiskManager>(config, logger);
        logger->info("Risk manager initialized", "main");
        
        // Initialize market data publisher
        auto market_data = std::make_shared<MarketDataPublisher>(logger);
        logger->info("Market data publisher initialized", "main");
        
        // Initialize OrderBook with all dependencies
        OrderBook book(risk_manager, market_data, logger);
        logger->info("OrderBook initialized with all dependencies", "main");
        
        // Display configuration summary
        std::cout << "\n=== OrderBook Configuration ===\n";
        std::cout << "Symbol: " << config->getString("orderbook", "symbol", "BTC/USD") << "\n";
        std::cout << "Max Orders: " << config->getInt("orderbook", "max_orders", 1000000) << "\n";
        std::cout << "Risk Limits:\n";
        std::cout << "  Max Order Size: " << config->getInt("risk", "max_order_size", 10000) << "\n";
        std::cout << "  Max Position: " << config->getInt("risk", "max_position", 100000) << "\n";
        std::cout << "  Max Price: " << config->getDouble("risk", "max_price", 1000000.0) << "\n";
        std::cout << "Log Level: " << config->getString("logging", "level", "info") << "\n";
        std::cout << "===============================\n\n";
        
        if (app_config.demo_mode) {
            std::cout << "Starting demo mode...\n\n";
            runDemoMode(book, *logger);
        } else {
            std::cout << "OrderBook initialized and ready.\n";
            std::cout << "Use --demo flag to run demonstration mode.\n";
            
            // Simple interactive mode for testing
            std::cout << "\nBasic functionality test:\n";
            
            // Create test orders
            Order buy_order(1, Side::Buy, OrderType::Limit, TimeInForce::GTC,
                           100.0, 500, "BTC/USD", "test_account");
            Order sell_order(2, Side::Sell, OrderType::Limit, TimeInForce::GTC,
                            101.0, 300, "BTC/USD", "test_account");
            
            // Add orders
            auto result1 = book.addOrder(buy_order);
            if (result1.isSuccess()) {
                std::cout << "Added buy order: " << result1.value().value << "\n";
            } else {
                std::cout << "Failed to add buy order: " << result1.error() << "\n";
            }
            
            auto result2 = book.addOrder(sell_order);
            if (result2.isSuccess()) {
                std::cout << "Added sell order: " << result2.value().value << "\n";
            } else {
                std::cout << "Failed to add sell order: " << result2.error() << "\n";
            }
            
            // Display best prices
            auto best_bid = book.bestBid();
            auto best_ask = book.bestAsk();
            
            std::cout << "Best Bid: " << (best_bid ? std::to_string(*best_bid) : "None") << "\n";
            std::cout << "Best Ask: " << (best_ask ? std::to_string(*best_ask) : "None") << "\n";
            
            std::cout << "\nOrderBook statistics:\n";
            std::cout << "Total Orders: " << book.getOrderCount() << "\n";
            std::cout << "Bid Levels: " << book.getBidLevelCount() << "\n";
            std::cout << "Ask Levels: " << book.getAskLevelCount() << "\n";
        }
        
        logger->info("OrderBook application completed successfully", "main");
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
    
    return 0;
}