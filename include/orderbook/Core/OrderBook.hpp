#pragma once
#include "Order.hpp"
#include "../MarketData/MarketDataFeed.hpp"
#include "../Risk/RiskManager.hpp"
#include <vector>
#include <list>
#include <unordered_map>
#include <optional>
#include <algorithm>

namespace orderbook {

template <typename PriceType = double, typename QuantityType = uint64_t>
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    
    // Order management
    void addOrder(const Order& order);
    void cancelOrder(uint64_t orderId);
    bool modifyOrder(uint64_t orderId, PriceType newPrice, QuantityType newQuantity);
    
    // Book status queries
    std::optional<PriceType> getBestBid() const;
    std::optional<PriceType> getBestAsk() const;
    PriceType getSpread() const;
    
    // Access to related components
    MarketDataFeed& getMarketDataFeed() { return marketData_; }
    RiskManager& getRiskManager() { return riskManager_; }

private:
    struct PriceLevel {
        std::list<Order> orders;
        QuantityType totalQuantity = 0;
    };

    std::string symbol_;
    
    // Vector of price levels, best price at the end for O(1) access
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    
    // Hash maps for O(1) lookup of price level index in vectors
    std::unordered_map<PriceType, std::size_t> bidPriceMap_;
    std::unordered_map<PriceType, std::size_t> askPriceMap_;
    
    // Map order ID to its location for O(1) lookup
    struct OrderLocation {
        bool isBid;
        PriceType price;
        typename std::list<Order>::iterator orderIt;
    };
    std::unordered_map<uint64_t, OrderLocation> orderMap_;
    
    MarketDataFeed marketData_;
    RiskManager riskManager_;

    void processMarketOrder(Order& order);
    void processLimitOrder(Order& order);
    void matchOrders(Order& order);
    
    void addToBook(Order& order);
    
    std::size_t findOrCreatePriceLevel(std::vector<PriceLevel>& levels, 
                                      std::unordered_map<PriceType, std::size_t>& priceMap, 
                                      PriceType price);
                                      
    void cleanupPriceLevels(std::vector<PriceLevel>& levels, 
                           std::unordered_map<PriceType, std::size_t>& priceMap,
                           PriceType price);
    
    void publishMarketDataUpdate();
    
    static constexpr bool compareBidPrices(const PriceLevel& a, const PriceLevel& b) {
        return a.price > b.price;
    }
    
    static constexpr bool compareAskPrices(const PriceLevel& a, const PriceLevel& b) {
        return a.price < b.price;
    }
};

}