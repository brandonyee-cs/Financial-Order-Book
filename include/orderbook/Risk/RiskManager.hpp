#pragma once
#include "../Core/Types.hpp"
#include "../Core/Interfaces.hpp"
#include "../Core/Order.hpp"
#include <unordered_map>
#include <memory>

namespace orderbook {

class Config;

/**
 * @brief Concrete implementation of risk management
 */
class RiskManager : public IRiskManager {
public:
    struct RiskLimits {
        Quantity max_order_size = 10000;
        Price max_price = 1000000.0;
        Price min_price = 0.01;
        int64_t max_position = 100000;
        int64_t min_position = -100000;
    };
    
    explicit RiskManager(const RiskLimits& limits = RiskLimits{}, LoggerPtr logger = nullptr);
    explicit RiskManager(std::shared_ptr<Config> config, LoggerPtr logger = nullptr);
    
    // IRiskManager interface implementation
    RiskCheck validateOrder(const Order& order, const Portfolio& portfolio) override;
    void updatePosition(const Trade& trade) override;
    const Portfolio& getPortfolio(const std::string& account) const override;
    
    // Configuration
    void setLimits(const RiskLimits& limits);
    const RiskLimits& getLimits() const;
    void loadConfiguration(std::shared_ptr<Config> config);
    void reloadConfiguration();
    
    // Account management
    void associateOrderWithAccount(OrderId order_id, const std::string& account);
    std::string getAccountForOrder(OrderId order_id) const;

private:
    RiskLimits limits_;
    mutable std::unordered_map<std::string, Portfolio> portfolios_;
    std::unordered_map<OrderId, std::string, OrderIdHash> order_to_account_;
    std::shared_ptr<Config> config_;
    LoggerPtr logger_;
    
    // Validation helpers
    bool validateOrderSize(Quantity quantity) const;
    bool validatePrice(Price price) const;
    bool validatePosition(const Portfolio& portfolio, const Order& order) const;
};

}