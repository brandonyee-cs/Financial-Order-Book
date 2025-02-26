#include "OrderBook.hpp"
#include <algorithm>
#include <iostream>

namespace orderbook {

OrderBook::OrderBook() {
    bids_.reserve(InitialCapacity);
    asks_.reserve(InitialCapacity);
}

template <Side side>
void OrderBook::maintainSorted(std::vector<Limit>& vec) {
    constexpr auto comp = [] (const Limit& a, const Limit& b) {
        if constexpr (side == Side::Buy) {
            return a.price > b.price;  // Descending for bids
        } else {
            return a.price < b.price;  // Ascending for asks
        }
    };
    
    if(vec.size() > 1) {
        std::sort(vec.begin(), vec.end(), comp);
    }
}

Limit* OrderBook::findOrCreateLimit(double price, Side side) {
    auto& price_map = (side == Side::Buy) ? bid_price_map_ : ask_price_map_;
    auto& vec = (side == Side::Buy) ? bids_ : asks_;
    
    if(auto it = price_map.find(price); it != price_map.end()) {
        return it->second;
    }
    
    vec.emplace_back(Limit{price, 0, nullptr, nullptr});
    Limit* new_limit = &vec.back();
    price_map[price] = new_limit;
    
    // Maintain sorted order
    if(side == Side::Buy) {
        maintainSorted<Side::Buy>(vec);
    } else {
        maintainSorted<Side::Sell>(vec);
    }
    
    return new_limit;
}

void OrderBook::addToLimit(Limit* limit, Order* order) {
    order->prev = limit->tail;
    if(limit->tail) {
        limit->tail->next = order;
    } else {
        limit->head = order;
    }
    limit->tail = order;
    limit->total_quantity += order->quantity;
}

void OrderBook::addOrder(uint64_t id, Side side, double price, uint64_t quantity) {
    if(order_map_.count(id)) return;
    
    Limit* limit = findOrCreateLimit(price, side);
    Order* new_order = new Order{id, side, quantity, price};
    addToLimit(limit, new_order);
    order_map_[id] = {new_order, limit};
}

void OrderBook::removeFromLimit(Limit* limit, Order* order) {
    if(order->prev) order->prev->next = order->next;
    if(order->next) order->next->prev = order->prev;
    
    if(order == limit->head) limit->head = order->next;
    if(order == limit->tail) limit->tail = order->prev;
    
    limit->total_quantity -= order->quantity;
    delete order;
}

void OrderBook::cancelOrder(uint64_t order_id) {
    auto it = order_map_.find(order_id);
    if(it == order_map_.end()) return;
    
    auto [order, limit] = it->second;
    removeFromLimit(limit, order);
    order_map_.erase(it);
    cleanupEmptyLimits();
}

bool OrderBook::modifyOrder(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto it = order_map_.find(order_id);
    if(it == order_map_.end()) return false;
    
    auto [old_order, old_limit] = it->second;
    const Side side = old_order->side;
    
    // Cancel old order
    removeFromLimit(old_limit, old_order);
    
    // Create new order
    Limit* new_limit = findOrCreateLimit(new_price, side);
    Order* new_order = new Order{order_id, side, new_quantity, new_price};
    addToLimit(new_limit, new_order);
    order_map_[order_id] = {new_order, new_limit};
    
    cleanupEmptyLimits();
    return true;
}

void OrderBook::cleanupEmptyLimits() {
    auto cleanup = [](auto& vec, auto& price_map) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&](const Limit& l) {
                if(l.total_quantity == 0) {
                    price_map.erase(l.price);
                    return true;
                }
                return false;
            }), vec.end());
    };
    
    cleanup(bids_, bid_price_map_);
    cleanup(asks_, ask_price_map_);
}

std::optional<double> OrderBook::bestBid() const {
    return bids_.empty() ? std::nullopt : std::make_optional(bids_.back().price);
}

std::optional<double> OrderBook::bestAsk() const {
    return asks_.empty() ? std::nullopt : std::make_optional(asks_.back().price);
}

} // namespace orderbook