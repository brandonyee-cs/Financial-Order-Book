#pragma once
#include "Order.hpp"
#include "../MarketData/MarketDataFeed.hpp"
#include "../Risk/RiskManager.hpp"
#include <map>
#include <list>
#include <unordered_map>
#include <optional>

namespace orderbook {

class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    
    void addOrder(const Order& order);
    void cancelOrder(uint64_t orderId);
    
    std::optional<double> getBestBid() const;
    std::optional<double> getBestAsk() const;
    double getSpread() const;
    
    MarketDataFeed& getMarketDataFeed() { return marketData_; }
    RiskManager& getRiskManager() { return riskManager_; }

private:
    struct PriceLevel {
        std::list<Order> orders;
        uint64_t totalQuantity;
    };

    std::string symbol_;
    std::map<double, PriceLevel, std::greater<>> bids_;
    std::map<double, PriceLevel> asks_;
    std::unordered_map<uint64_t, std::list<Order>::iterator> orderMap_;
    
    MarketDataFeed marketData_;
    RiskManager riskManager_;

    void processMarketOrder(Order& order);
    void processLimitOrder(Order& order);
    void matchOrders(Order& order);
    void addToBook(Order& order);
    void publishMarketDataUpdate();
};

}