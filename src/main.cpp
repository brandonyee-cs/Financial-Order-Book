#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Network/FixSession.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include "orderbook/Utilities/Config.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

using namespace orderbook;
using namespace boost::asio;
using namespace boost::asio::ip;

int main(int argc, char* argv[]) {
    try {
        const std::string configFile = (argc > 1) ? argv[1] : "../config/orderbook.cfg";
        Config config(configFile);
        
        Logger::init(config.getString("logging", "file", "orderbook.log"));
        
        OrderBook book(config.getString("orderbook", "symbol", "BTC/USD"));
        
        const int port = config.getInt("network", "port", 5000);
        const int maxConnections = config.getInt("network", "max_connections", 1000);
        
        io_context io;
        tcp::endpoint endpoint(tcp::v4(), port);
        tcp::acceptor acceptor(io, endpoint);
        
        Logger::log("Starting order book on port: " + std::to_string(port));
        Logger::log("Symbol: " + book.getSymbol());
        
        auto orderHandler = [&book](const Order& order) {
            try {
                book.addOrder(order);
            } catch(const std::exception& e) {
                Logger::error("Order processing failed: " + std::string(e.what()));
            }
        };
        
        std::function<void()> acceptConnection;
        acceptConnection = [&]() {
            auto socket = std::make_shared<tcp::socket>(io);
            acceptor.async_accept(*socket, [=](boost::system::error_code ec) {
                if(!ec) {
                    Logger::log("New connection from: " + 
                               socket->remote_endpoint().address().to_string());
                    
                    std::make_shared<FixSession>(std::move(*socket), orderHandler)->start();
                } else {
                    Logger::error("Accept error: " + ec.message());
                }
                acceptConnection();
            });
        };
        
        acceptConnection();
        
        io.run();
    } catch(const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}