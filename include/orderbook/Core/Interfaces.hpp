#pragma once
#include "Types.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace orderbook {

// Forward declarations
class Order;
class Trade;
class Portfolio;
class ExecutionReport;
class BookUpdate;

// Risk validation result
struct RiskCheck {
    RiskResult result;
    std::string reason;
    
    RiskCheck(RiskResult r, const std::string& msg = "") 
        : result(r), reason(msg) {}
    
    bool isApproved() const { return result == RiskResult::Approved; }
    bool isRejected() const { return result == RiskResult::Rejected; }
};

// Market data structures
struct MarketDepth {
    struct Level {
        Price price;
        Quantity quantity;
        size_t order_count;
    };
    
    std::vector<Level> bids;
    std::vector<Level> asks;
    Timestamp timestamp;
};

struct BestPrices {
    std::optional<Price> bid;
    std::optional<Price> ask;
    std::optional<Quantity> bid_size;
    std::optional<Quantity> ask_size;
    Timestamp timestamp;
};

// Abstract interfaces

/**
 * @brief Interface for risk management validation
 */
class IRiskManager {
public:
    virtual ~IRiskManager() = default;
    
    /**
     * @brief Validate an order against risk limits
     * @param order The order to validate
     * @param portfolio Current portfolio state
     * @return RiskCheck result with approval/rejection and reason
     */
    virtual RiskCheck validateOrder(const Order& order, const Portfolio& portfolio) = 0;
    
    /**
     * @brief Update position after trade execution
     * @param trade The executed trade
     */
    virtual void updatePosition(const Trade& trade) = 0;
    
    /**
     * @brief Get current portfolio for an account
     * @param account Account identifier
     * @return Portfolio reference
     */
    virtual const Portfolio& getPortfolio(const std::string& account) const = 0;
};

/**
 * @brief Interface for market data publishing
 */
class IMarketDataPublisher {
public:
    virtual ~IMarketDataPublisher() = default;
    
    /**
     * @brief Publish a trade execution
     * @param trade The executed trade
     */
    virtual void publishTrade(const Trade& trade) = 0;
    
    /**
     * @brief Publish order book update
     * @param update Book level changes
     */
    virtual void publishBookUpdate(const BookUpdate& update) = 0;
    
    /**
     * @brief Publish best bid/ask prices
     * @param prices Current best prices
     */
    virtual void publishBestPrices(const BestPrices& prices) = 0;
    
    /**
     * @brief Publish market depth
     * @param depth Full market depth snapshot
     */
    virtual void publishDepth(const MarketDepth& depth) = 0;
    
    /**
     * @brief Subscribe to market data updates
     * @param callback Function to call on updates
     */
    virtual void subscribe(std::function<void(const std::string&)> callback) = 0;
};

/**
 * @brief Interface for structured logging
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    
    /**
     * @brief Log a message at specified level
     * @param level Log level
     * @param message Message to log
     * @param context Additional context (optional)
     */
    virtual void log(LogLevel level, const std::string& message, 
                    const std::string& context = "") = 0;
    
    /**
     * @brief Log debug message
     * @param message Debug message
     * @param context Additional context
     */
    virtual void debug(const std::string& message, const std::string& context = "") = 0;
    
    /**
     * @brief Log info message
     * @param message Info message
     * @param context Additional context
     */
    virtual void info(const std::string& message, const std::string& context = "") = 0;
    
    /**
     * @brief Log warning message
     * @param message Warning message
     * @param context Additional context
     */
    virtual void warn(const std::string& message, const std::string& context = "") = 0;
    
    /**
     * @brief Log error message
     * @param message Error message
     * @param context Additional context
     */
    virtual void error(const std::string& message, const std::string& context = "") = 0;
    
    /**
     * @brief Log performance metrics
     * @param operation Operation name
     * @param latency_ns Latency in nanoseconds
     * @param additional_metrics Additional key-value metrics
     */
    virtual void logPerformance(const std::string& operation, 
                               uint64_t latency_ns,
                               const std::vector<std::pair<std::string, std::string>>& additional_metrics = {}) = 0;
    
    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    virtual void setLogLevel(LogLevel level) = 0;
};

// Convenience type aliases for shared pointers
using RiskManagerPtr = std::shared_ptr<IRiskManager>;
using MarketDataPublisherPtr = std::shared_ptr<IMarketDataPublisher>;
using LoggerPtr = std::shared_ptr<ILogger>;

}