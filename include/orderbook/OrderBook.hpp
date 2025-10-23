#pragma once

/**
 * @file OrderBook.hpp
 * @brief Main header file for the OrderBook library
 * 
 * This header provides access to all core types, interfaces, and classes
 * needed to use the OrderBook system.
 */

// Core types and interfaces
#include "Core/Types.hpp"
#include "Core/Interfaces.hpp"
#include "Core/Order.hpp"
#include "Core/OrderBook.hpp"
#include "Core/MarketData.hpp"

// Concrete implementations
#include "Risk/RiskManager.hpp"
#include "MarketData/MarketDataFeed.hpp"
#include "Utilities/Logger.hpp"

namespace orderbook {

/**
 * @brief Factory function to create a fully configured OrderBook
 * @param risk_limits Risk management limits
 * @param log_config Logger configuration
 * @return Configured OrderBook instance
 */
inline std::unique_ptr<OrderBook> createOrderBook(
    const RiskManager::RiskLimits& risk_limits = RiskManager::RiskLimits{},
    const Logger::LogConfig& log_config = Logger::LogConfig{}) {
    
    auto logger = std::make_shared<Logger>(log_config);
    auto risk_manager = std::make_shared<RiskManager>(risk_limits);
    auto market_data = std::make_shared<MarketDataPublisher>();
    
    return std::make_unique<OrderBook>(risk_manager, market_data, logger);
}

}