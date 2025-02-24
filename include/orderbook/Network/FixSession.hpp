#pragma once
#include "../Core/Order.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <memory>

namespace orderbook {

class FixSession : public std::enable_shared_from_this<FixSession> {
public:
    using OrderHandler = std::function<void(const Order&)>;
    
    FixSession(boost::asio::ip::tcp::socket socket, OrderHandler handler);
    void start();

private:
    void readHeader();
    void readBody();
    void write(const std::string& message);

    boost::asio::ip::tcp::socket socket_;
    OrderHandler orderHandler_;
    std::array<char, 8192> buffer_;
};

}