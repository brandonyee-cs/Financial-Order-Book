#include "orderbook/Core/OrderBook.hpp"
#include <algorithm>
#include <iostream>

namespace orderbook {

void OrderBook::addOrder(const Order& order) {
    if (!validateOrder(order)) {
        std::cerr << "Order validation failed for ID: " << order.id << "\n";
        return;
    }

    Order workingOrder = order;
    workingOrder.timestamp = std::chrono::system_clock::now();

    if (workingOrder.type == Order::Type::Market) {
        processMarketOrder(workingOrder);
    } else {
        processLimitOrder(workingOrder);
    }

    publishMarketDataUpdate();
}

void OrderBook::cancelOrder(uint64_t orderId) {
    auto it = orderMap_.find(orderId);
    if (it == orderMap_.end()) {
        std::cerr << "Cancel failed: Order ID " << orderId << " not found\n";
        return;
    }

    const auto& [side, price, orderIt] = it->second;
    auto& book = (side == Order::Side::Buy) ? bids_ : asks_;

    auto levelIt = book.find(price);
    if (levelIt != book.end()) {
        auto& priceLevel = levelIt->second;
        priceLevel.totalQuantity -= orderIt->quantity;
        priceLevel.orders.erase(orderIt);

        if (priceLevel.orders.empty()) {
            book.erase(levelIt);
        }
    }

    orderMap_.erase(it);
    publishMarketDataUpdate();
}

void OrderBook::processLimitOrder(Order& order) {
    auto& oppositeBook = (order.side == Order::Side::Buy) ? asks_ : bids_;
    auto comp = order.side == Order::Side::Buy 
        ? [](double a, double b) { return a <= b; }
        : [](double a, double b) { return a >= b; };

    for (auto it = oppositeBook.begin(); it != oppositeBook.end() && order.quantity > 0;) {
        if (!comp(it->first, order.price)) break;

        auto& priceLevel = it->second;
        while (!priceLevel.orders.empty() && order.quantity > 0) {
            auto& oppositeOrder = priceLevel.orders.front();
            uint64_t fillQty = std::min(order.quantity, oppositeOrder.quantity);

            // Execute trade
            executeTrade(order, oppositeOrder, fillQty, it->first);

            // Update quantities
            order.quantity -= fillQty;
            oppositeOrder.quantity -= fillQty;
            priceLevel.totalQuantity -= fillQty;

            // Remove filled orders
            if (oppositeOrder.quantity == 0) {
                orderMap_.erase(oppositeOrder.id);
                priceLevel.orders.pop_front();
            }
        }

        if (priceLevel.orders.empty()) {
            it = oppositeBook.erase(it);
        } else {
            ++it;
        }
    }

    if (order.quantity > 0) {
        addToBook(order);
    }
}

void OrderBook::processMarketOrder(Order& order) {
    auto& oppositeBook = (order.side == Order::Side::Buy) ? asks_ : bids_;
    
    for (auto it = oppositeBook.begin(); it != oppositeBook.end() && order.quantity > 0;) {
        auto& priceLevel = it->second;
        while (!priceLevel.orders.empty() && order.quantity > 0) {
            auto& oppositeOrder = priceLevel.orders.front();
            uint64_t fillQty = std::min(order.quantity, oppositeOrder.quantity);

            executeTrade(order, oppositeOrder, fillQty, it->first);

            order.quantity -= fillQty;
            oppositeOrder.quantity -= fillQty;
            priceLevel.totalQuantity -= fillQty;

            if (oppositeOrder.quantity == 0) {
                orderMap_.erase(oppositeOrder.id);
                priceLevel.orders.pop_front();
            }
        }

        if (priceLevel.orders.empty()) {
            it = oppositeBook.erase(it);
        } else {
            ++it;
        }
    }
}

void OrderBook::addToBook(Order& order) {
    auto& book = (order.side == Order::Side::Buy) ? bids_ : asks_;
    auto& priceLevel = book[order.price];
    
    priceLevel.orders.push_back(order);
    priceLevel.totalQuantity += order.quantity;
    
    orderMap_[order.id] = {
        order.side,
        order.price,
        std::prev(priceLevel.orders.end())
    };
}

std::optional<double> OrderBook::getBestBid() const {
    return bids_.empty() ? std::nullopt : std::make_optional(bids_.begin()->first);
}

std::optional<double> OrderBook::getBestAsk() const {
    return asks_.empty() ? std::nullopt : std::make_optional(asks_.begin()->first);
}

bool OrderBook::validateOrder(const Order& order) const {
    return order.quantity > 0 && order.price >= 0 &&
          (order.type == Order::Type::Market || order.price > 0);
}

void OrderBook::executeTrade(Order& taker, Order& maker, 
                            uint64_t quantity, double price) {
    std::cout << "TRADE EXECUTED: "
              << quantity << " @ " << price
              << " BETWEEN " << taker.id << " AND " << maker.id << "\n";
}

void OrderBook::publishMarketDataUpdate() const {
    std::cout << "BOOK UPDATE - Best Bid: " 
              << getBestBid().value_or(0)
              << " | Best Ask: "
              << getBestAsk().value_or(0) << "\n";
}

}