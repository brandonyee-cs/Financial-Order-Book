#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Network/FixSession.hpp"
#include <boost/asio.hpp>

using namespace orderbook;
using namespace boost::asio;

int main() {
    io_context io;
    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v4(), 5000));
    
    OrderBook book("BTC/USD");
    Logger::init("orderbook.log");
    
    auto handleFixOrder = [&book](const Order& order) {
        book.addOrder(order);
    };
    
    auto acceptConnection = [&]() {
        auto socket = std::make_shared<ip::tcp::socket>(io);
        acceptor.async_accept(*socket, [=](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<FixSession>(std::move(*socket), handleFixOrder)->start();
            }
            acceptConnection();
        });
    };
    
    acceptConnection();
    io.run();
    return 0;
}