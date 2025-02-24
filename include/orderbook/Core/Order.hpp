#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace orderbook {

struct Order {
    enum class Side { Buy, Sell };
    enum class Type { Limit, Market, Stop };
    enum class TIF { GTC, IOC, FOK };
    
    uint64_t id;
    Side side;
    Type type;
    TIF tif;
    double price;
    uint64_t quantity;
    std::string symbol;
    std::string account;
    std::chrono::system_clock::time_point timestamp;

    Order(uint64_t id, Side side, Type type, TIF tif,
          double price, uint64_t quantity,
          std::string symbol, std::string account);
};

}