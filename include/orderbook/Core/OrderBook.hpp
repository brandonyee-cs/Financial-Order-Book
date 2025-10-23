#pragma once
#include "Types.hpp"
#include "Order.hpp"
#include "Interfaces.hpp"
#include "MarketData.hpp"
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace orderbook {

// PriceLevel is defined in Order.hpp

/**
 * @brief High-performance order book implementation
 */
class OrderBook {
public:
    // Constructor with dependency injection
    OrderBook(RiskManagerPtr risk_manager = nullptr,
              MarketDataPublisherPtr market_data = nullptr,
              LoggerPtr logger = nullptr);
    
    // Core operations
    OrderResult addOrder(const Order& order);
    CancelResult cancelOrder(OrderId id);
    ModifyResult modifyOrder(OrderId id, Price new_price, Quantity new_quantity);
    
    // Market data queries
    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;
    BestPrices getBestPrices() const;
    MarketDepth getDepth(size_t levels) const;
    
    // Statistics
    size_t getOrderCount() const;
    size_t getBidLevelCount() const;
    size_t getAskLevelCount() const;

private:
    // Optimized storage
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    std::unordered_map<Price, PriceLevel*> price_index_;
    std::unordered_map<OrderId, OrderLocation, OrderIdHash> order_index_;
    
    // Dependencies
    RiskManagerPtr risk_manager_;
    MarketDataPublisherPtr market_data_;
    LoggerPtr logger_;
    
    // Helper methods
    PriceLevel* findOrCreatePriceLevel(Price price, Side side);
    void removePriceLevel(PriceLevel* level, Side side);
    void maintainSortedOrder();
    void rebuildPriceIndex();
    void publishMarketDataUpdate();
    void publishBookUpdate(BookUpdate::Type type, Side side, Price price, 
                          Quantity quantity, size_t order_count);
    
    // Matching and trade execution
    void processMatching(Order& incoming_order);
    void executeMatching(Order& incoming_order, std::vector<PriceLevel>& opposite_levels);
    void matchAgainstPriceLevel(Order& incoming_order, PriceLevel& price_level);
    void executeTrade(Order& aggressive_order, Order& passive_order, 
                     Price trade_price, Quantity trade_quantity);
    
    // Constants
    static constexpr size_t InitialCapacity = 1024;
    static constexpr Price MinPriceIncrement = 0.01;
};

}