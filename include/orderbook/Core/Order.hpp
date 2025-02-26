#pragma once
#include <chrono>
#include <string>
#include <cstdint>

namespace orderbook {

// Use strong types to avoid parameter confusion
struct OrderId {
    uint64_t value;
    
    constexpr explicit OrderId(uint64_t id) : value(id) {}
    constexpr operator uint64_t() const { return value; }
    constexpr bool operator==(const OrderId& other) const { return value == other.value; }
};

struct Order {
    enum class Side { Buy, Sell };
    enum class Type { Limit, Market };
    enum class TIF { GTC, IOC, FOK };
    
    OrderId id;
    Side side;
    Type type;
    TIF tif = TIF::GTC;
    double price;
    uint64_t quantity;
    std::string symbol;
    std::string account;
    std::chrono::system_clock::time_point timestamp;
    
    Order(uint64_t id, Side side, Type type, double price, uint64_t quantity, std::string symbol)
        : id(OrderId(id)), side(side), type(type), price(price), quantity(quantity), 
          symbol(std::move(symbol)), timestamp(std::chrono::system_clock::now()) {}
    
    Order(uint64_t id, Side side, Type type, TIF tif, double price, uint64_t quantity, 
          std::string symbol, std::string account = "")
        : id(OrderId(id)), side(side), type(type), tif(tif), price(price), quantity(quantity),
          symbol(std::move(symbol)), account(std::move(account)), 
          timestamp(std::chrono::system_clock::now()) {}
};

// Custom hash function for OrderId to use in unordered_map
struct OrderIdHash {
    std::size_t operator()(const OrderId& id) const {
        return std::hash<uint64_t>{}(id.value);
    }
};

}