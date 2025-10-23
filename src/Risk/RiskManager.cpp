#include "orderbook/Risk/RiskManager.hpp"
#include "orderbook/Utilities/Config.hpp"
#include "orderbook/Utilities/PerformanceTimer.hpp"
#include <sstream>

namespace orderbook {

RiskManager::RiskManager(const RiskLimits& limits, LoggerPtr logger) 
    : limits_(limits), logger_(logger) {
    if (logger_) {
        logger_->info("RiskManager initialized with custom limits", "RiskManager::ctor");
    }
}

RiskManager::RiskManager(std::shared_ptr<Config> config, LoggerPtr logger) 
    : config_(config), logger_(logger) {
    loadConfiguration(config);
    if (logger_) {
        logger_->info("RiskManager initialized with configuration", "RiskManager::ctor");
    }
}

RiskCheck RiskManager::validateOrder(const Order& order, const Portfolio& portfolio) {
    PERF_TIMER("RiskManager::validateOrder", logger_);
    
    if (logger_) {
        logger_->debug("Validating order ID: " + std::to_string(order.id.value) +
                      " Symbol: " + order.symbol + " Quantity: " + std::to_string(order.quantity) +
                      " Price: " + std::to_string(order.price), "RiskManager::validateOrder");
    }
    
    // Validate order size
    if (!validateOrderSize(order.quantity)) {
        std::ostringstream oss;
        oss << "Order size " << order.quantity << " exceeds maximum allowed " << limits_.max_order_size;
        if (logger_) {
            logger_->warn("Order size validation failed: " + oss.str(), 
                         "RiskManager::validateOrder - OrderID: " + std::to_string(order.id.value));
        }
        return RiskCheck(RiskResult::Rejected, oss.str());
    }
    
    // Validate price
    if (!validatePrice(order.price)) {
        std::ostringstream oss;
        oss << "Order price " << order.price << " outside allowed range [" 
            << limits_.min_price << ", " << limits_.max_price << "]";
        if (logger_) {
            logger_->warn("Order price validation failed: " + oss.str(), 
                         "RiskManager::validateOrder - OrderID: " + std::to_string(order.id.value));
        }
        return RiskCheck(RiskResult::Rejected, oss.str());
    }
    
    // Validate position limits
    if (!validatePosition(portfolio, order)) {
        std::ostringstream oss;
        oss << "Order would exceed position limits for symbol " << order.symbol;
        if (logger_) {
            logger_->warn("Position limit validation failed: " + oss.str(), 
                         "RiskManager::validateOrder - OrderID: " + std::to_string(order.id.value));
        }
        return RiskCheck(RiskResult::Rejected, oss.str());
    }
    
    if (logger_) {
        logger_->debug("Order passed all risk validations", 
                      "RiskManager::validateOrder - OrderID: " + std::to_string(order.id.value));
    }
    
    return RiskCheck(RiskResult::Approved, "Order passed all risk checks");
}

void RiskManager::updatePosition(const Trade& trade) {
    // Get accounts for the orders involved in the trade
    std::string buy_account = getAccountForOrder(trade.buy_order_id);
    std::string sell_account = getAccountForOrder(trade.sell_order_id);
    
    // Update buyer's position (positive quantity)
    if (portfolios_.find(buy_account) == portfolios_.end()) {
        portfolios_[buy_account] = Portfolio(buy_account);
    }
    portfolios_[buy_account].updatePosition(trade.symbol, static_cast<int64_t>(trade.quantity), trade.price);
    
    // Update seller's position (negative quantity)
    if (portfolios_.find(sell_account) == portfolios_.end()) {
        portfolios_[sell_account] = Portfolio(sell_account);
    }
    portfolios_[sell_account].updatePosition(trade.symbol, -static_cast<int64_t>(trade.quantity), trade.price);
}

void RiskManager::associateOrderWithAccount(OrderId order_id, const std::string& account) {
    order_to_account_[order_id] = account;
}

std::string RiskManager::getAccountForOrder(OrderId order_id) const {
    auto it = order_to_account_.find(order_id);
    if (it != order_to_account_.end()) {
        return it->second;
    }
    // Return default account if not found
    return "account_" + std::to_string(order_id.value);
}

const Portfolio& RiskManager::getPortfolio(const std::string& account) const {
    auto it = portfolios_.find(account);
    if (it == portfolios_.end()) {
        // Create new portfolio if it doesn't exist
        portfolios_[account] = Portfolio(account);
        return portfolios_[account];
    }
    return it->second;
}

void RiskManager::setLimits(const RiskLimits& limits) {
    limits_ = limits;
}

const RiskManager::RiskLimits& RiskManager::getLimits() const {
    return limits_;
}

void RiskManager::loadConfiguration(std::shared_ptr<Config> config) {
    config_ = config;
    if (!config_) {
        return;
    }
    
    // Load risk limits from configuration
    limits_.max_order_size = config_->getInt("risk", "max_order_size", limits_.max_order_size);
    limits_.max_price = config_->getDouble("risk", "max_price", limits_.max_price);
    limits_.min_price = config_->getDouble("risk", "min_price", limits_.min_price);
    limits_.max_position = config_->getInt("risk", "max_position", limits_.max_position);
    limits_.min_position = config_->getInt("risk", "min_position", limits_.min_position);
}

void RiskManager::reloadConfiguration() {
    if (config_) {
        loadConfiguration(config_);
    }
}

bool RiskManager::validateOrderSize(Quantity quantity) const {
    return quantity > 0 && quantity <= limits_.max_order_size;
}

bool RiskManager::validatePrice(Price price) const {
    return price >= limits_.min_price && price <= limits_.max_price;
}

bool RiskManager::validatePosition(const Portfolio& portfolio, const Order& order) const {
    int64_t current_position = portfolio.getPosition(order.symbol);
    int64_t position_change = static_cast<int64_t>(order.quantity);
    
    if (order.side == Side::Sell) {
        position_change = -position_change;
    }
    
    int64_t new_position = current_position + position_change;
    
    return new_position >= limits_.min_position && new_position <= limits_.max_position;
}

}