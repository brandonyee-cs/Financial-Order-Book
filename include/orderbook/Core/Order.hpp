#pragma once
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <string>

namespace orderbook {

struct Order {
    enum class Side { Buy, Sell };
    enum class Type { Limit, Market };
    
    uint64_t id;
    Side side;
    Type type;
    double price;
    uint64_t quantity;
    std::string symbol;
    std::chrono::system_clock::time_point timestamp;
};

class OrderBook {
public:
    void addOrder(const Order& order);
    void cancelOrder(uint64_t orderId);
    
    std::optional<double> getBestBid() const;
    std::optional<double> getBestAsk() const;

private:
    struct PriceLevel {
        std::list<Order> orders;
        uint64_t totalQuantity = 0;
    };

    std::map<double, PriceLevel, std::greater<>> bids_;
    std::map<double, PriceLevel> asks_;
    std::unordered_map<uint64_t, 
        std::tuple<Order::Side, double, std::list<Order>::iterator>> orderMap_;

    void processLimitOrder(Order& order);
    void processMarketOrder(Order& order);
    void addToBook(Order& order);
    
    bool validateOrder(const Order& order) const;
    void executeTrade(Order& taker, Order& maker, 
                     uint64_t quantity, double price);
    void publishMarketDataUpdate() const;
};

}