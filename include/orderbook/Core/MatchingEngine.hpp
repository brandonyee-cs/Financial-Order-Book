#pragma once
#include "Types.hpp"
#include "Order.hpp"
#include "Interfaces.hpp"
#include "MarketData.hpp"
#include <vector>
#include <memory>
#include <atomic>

namespace orderbook {

// Forward declarations
class OrderBook;
class PriceLevel;

/**
 * @brief Result of order matching operation
 */
struct MatchResult {
    std::vector<Trade> trades;                    // All trades generated from matching
    std::vector<ExecutionReport> execution_reports; // FIX execution reports for all affected orders
    std::optional<Order> remaining_order;         // Remaining unfilled portion (if any)
    bool fully_filled;                           // True if incoming order was completely filled
    Quantity total_filled_quantity;              // Total quantity filled
    
    MatchResult() : fully_filled(false), total_filled_quantity(0) {}
    
    bool hasRemainingOrder() const { return remaining_order.has_value(); }
    bool hasTrades() const { return !trades.empty(); }
    bool hasExecutionReports() const { return !execution_reports.empty(); }
    size_t getTradeCount() const { return trades.size(); }
    size_t getExecutionReportCount() const { return execution_reports.size(); }
};

/**
 * @brief High-performance order matching engine
 * 
 * Implements price-time priority matching algorithm with sub-microsecond latency.
 * Handles aggressive orders that cross the spread and generates trade executions.
 */
class MatchingEngine {
public:
    /**
     * @brief Constructor with dependency injection
     * @param logger Logger for trade execution logging
     */
    explicit MatchingEngine(LoggerPtr logger = nullptr);
    
    /**
     * @brief Match an incoming order against the order book
     * @param incoming_order The order to match
     * @param opposite_side_levels Price levels on the opposite side for matching
     * @return MatchResult containing trades and remaining order
     */
    MatchResult matchOrder(Order& incoming_order, 
                          std::vector<PriceLevel>& opposite_side_levels);
    
    /**
     * @brief Match a buy order against ask levels
     * @param buy_order The incoming buy order
     * @param ask_levels Ask price levels sorted by price (ascending)
     * @return MatchResult with trades and remaining order
     */
    MatchResult matchBuyOrder(Order& buy_order, 
                             std::vector<PriceLevel>& ask_levels);
    
    /**
     * @brief Match a sell order against bid levels
     * @param sell_order The incoming sell order
     * @param bid_levels Bid price levels sorted by price (descending)
     * @return MatchResult with trades and remaining order
     */
    MatchResult matchSellOrder(Order& sell_order, 
                              std::vector<PriceLevel>& bid_levels);
    
    /**
     * @brief Get next trade ID (thread-safe)
     * @return Unique trade ID
     */
    TradeId getNextTradeId();
    
    /**
     * @brief Get total number of trades executed
     * @return Trade count
     */
    uint64_t getTotalTradeCount() const { return trade_counter_.load(); }
    
    /**
     * @brief Reset trade counter (for testing)
     */
    void resetTradeCounter() { trade_counter_.store(1); }
    
    /**
     * @brief Generate execution report for order fill
     * @param order The order that was filled
     * @param trade_id Optional trade ID if this is from a trade
     * @return ExecutionReport for FIX protocol
     */
    static ExecutionReport generateExecutionReport(const Order& order, 
                                                  std::optional<TradeId> trade_id = std::nullopt);
    
    /**
     * @brief Generate execution report for new order
     * @param order The new order
     * @return ExecutionReport for order acknowledgment
     */
    static ExecutionReport generateNewOrderReport(const Order& order);
    
    /**
     * @brief Generate execution report for order rejection
     * @param order The rejected order
     * @param reason Rejection reason
     * @return ExecutionReport for order rejection
     */
    static ExecutionReport generateRejectionReport(const Order& order, const std::string& reason);
    
    /**
     * @brief Handle partial fill processing
     * @param order Order that was partially filled
     * @param filled_quantity Quantity that was filled
     * @return ExecutionReport for the partial fill
     */
    static ExecutionReport handlePartialFill(Order& order, Quantity filled_quantity);
    
    /**
     * @brief Calculate remaining quantity after fills
     * @param original_quantity Original order quantity
     * @param filled_quantity Total filled quantity
     * @return Remaining quantity to be filled
     */
    static Quantity calculateRemainingQuantity(Quantity original_quantity, Quantity filled_quantity) {
        return (filled_quantity >= original_quantity) ? 0 : (original_quantity - filled_quantity);
    }

private:
    // Dependencies
    LoggerPtr logger_;
    
    // Trade ID generation (thread-safe)
    std::atomic<uint64_t> trade_counter_{1};
    
    /**
     * @brief Execute a trade between two orders
     * @param aggressive_order The incoming order (taker)
     * @param passive_order The resting order (maker)
     * @param trade_price Price at which trade executes
     * @param trade_quantity Quantity to trade
     * @return Generated Trade object
     */
    Trade executeTrade(Order& aggressive_order, 
                      Order& passive_order,
                      Price trade_price, 
                      Quantity trade_quantity);
    
    /**
     * @brief Match against a single price level
     * @param incoming_order The order to match
     * @param price_level The price level to match against
     * @param trades Vector to append trades to
     * @return Quantity filled from this price level
     */
    Quantity matchAgainstPriceLevel(Order& incoming_order,
                                   PriceLevel& price_level,
                                   std::vector<Trade>& trades);
    
    /**
     * @brief Check if orders can match based on price
     * @param buy_price Buy order price
     * @param sell_price Sell order price
     * @return true if prices cross (can match)
     */
    static bool canMatch(Price buy_price, Price sell_price) {
        return buy_price >= sell_price;
    }
    
    /**
     * @brief Determine trade price based on price-time priority
     * @param aggressive_order Incoming order
     * @param passive_order Resting order
     * @return Price at which trade should execute
     */
    static Price determineTradePrice(const Order& aggressive_order, 
                                   const Order& passive_order) {
        // Price-time priority: passive order (maker) gets their price
        return passive_order.price;
    }
    
    /**
     * @brief Log trade execution for monitoring
     * @param trade The executed trade
     * @param latency_ns Execution latency in nanoseconds
     */
    void logTradeExecution(const Trade& trade, uint64_t latency_ns);
    
    /**
     * @brief Validate trade parameters
     * @param aggressive_order Incoming order
     * @param passive_order Resting order
     * @param quantity Trade quantity
     * @return true if trade is valid
     */
    static bool validateTrade(const Order& aggressive_order,
                            const Order& passive_order,
                            Quantity quantity);
};

}