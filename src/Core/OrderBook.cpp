#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Utilities/Logger.hpp"

namespace orderbook {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol), marketData_(), riskManager_() {}

void OrderBook::addOrder(const Order& order) {
    if (!riskManager_.validateOrder(order)) {
        Logger::error("Order failed risk checks: " + std::to_string(order.id));
        return;
    }

    Order workingOrder = order;
    
    try {
        if (workingOrder.type == Order::Type::Market) {
            processMarketOrder(workingOrder);
        } else {
            processLimitOrder(workingOrder);
        }
        publishMarketDataUpdate();
    }
    catch (const std::exception& e) {
        Logger::error("Order processing error: " + std::string(e.what()));
    }
}