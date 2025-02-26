#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <optional>
#include <functional>
#include <cstdint>

namespace orderbook {

template <typename T>
constexpr T SideMultiplier = T(1);  // Compile-time constant

enum class Side { Buy, Sell };

struct Order {
    uint64_t id;
    Side side;
    uint64_t quantity;
    double price;
    Order* prev = nullptr;
    Order* next = nullptr;
};

struct Limit {
    double price;
    uint64_t total_quantity;
    Order* head = nullptr;
    Order* tail = nullptr;
    Limit* prev_limit = nullptr;
    Limit* next_limit = nullptr;
};

class OrderBook {
private:
    std::vector<Limit> bids_;
    std::vector<Limit> asks_;
    std::unordered_map<double, Limit*> bid_price_map_;
    std::unordered_map<double, Limit*> ask_price_map_;
    std::unordered_map<uint64_t, std::pair<Order*, Limit*>> order_map_;
    
    template <Side side>
    void maintainSorted(std::vector<Limit>& vec);
    
    void addToLimit(Limit* limit, Order* order);
    void removeFromLimit(Limit* limit, Order* order);
    Limit* findOrCreateLimit(double price, Side side);
    void cleanupEmptyLimits();

public:
    OrderBook();
    void addOrder(uint64_t id, Side side, double price, uint64_t quantity);
    void cancelOrder(uint64_t order_id);
    bool modifyOrder(uint64_t order_id, double new_price, uint64_t new_quantity);
    
    std::optional<double> bestBid() const;
    std::optional<double> bestAsk() const;
    
    static constexpr size_t InitialCapacity = 1024;
    static constexpr double MinPriceIncrement = 0.01;
};

}