#include "orderbook/Network/FixServer.hpp"
#include "orderbook/Core/OrderManager.hpp"
#include "orderbook/Risk/RiskManager.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>

namespace orderbook {

/**
 * @brief Example demonstrating FIX protocol integration
 * Shows how to set up a FIX server with order management and risk controls
 */
class FixIntegrationExample {
public:
    void run() {
        std::cout << "Starting FIX Protocol Integration Example..." << std::endl;
        
        // Create IO context for async operations
        boost::asio::io_context ioContext;
        
        // Create logger
        auto logger = std::make_shared<Logger>(LogLevel::INFO);
        
        // Create order manager
        auto orderManager = std::make_shared<OrderManager>(logger);
        
        // Create risk manager with default limits
        auto riskManager = std::make_shared<RiskManager>(logger);
        
        // Create FIX server
        FixServer fixServer(ioContext, orderManager, riskManager);
        
        try {
            // Start FIX server on port 9878
            fixServer.start(9878, "ORDERBOOK_SERVER");
            
            // Run IO context in a separate thread
            std::thread ioThread([&ioContext]() {
                ioContext.run();
            });
            
            // Display server statistics periodically
            displayStats(fixServer);
            
            // Stop server
            std::cout << "Stopping FIX server..." << std::endl;
            fixServer.stop();
            ioContext.stop();
            
            if (ioThread.joinable()) {
                ioThread.join();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        
        std::cout << "FIX Protocol Integration Example completed." << std::endl;
    }

private:
    void displayStats(const FixServer& server) {
        std::cout << "\nFIX Server is running. Press Ctrl+C to stop.\n" << std::endl;
        std::cout << "Server Statistics:" << std::endl;
        std::cout << "==================" << std::endl;
        
        for (int i = 0; i < 10; ++i) {  // Run for 10 seconds
            auto stats = server.getStats();
            
            std::cout << "\rActive Connections: " << stats.activeConnections
                     << " | Total Connections: " << stats.totalConnections
                     << " | Messages: " << stats.messagesProcessed
                     << " | Orders: " << stats.ordersProcessed
                     << std::flush;
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << std::endl;
    }
};

} // namespace orderbook

// Example usage (commented out to avoid main conflicts)
/*
int main() {
    orderbook::FixIntegrationExample example;
    example.run();
    return 0;
}
*/