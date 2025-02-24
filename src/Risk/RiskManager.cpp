#include "orderbook/Risk/RiskManager.hpp"

namespace orderbook {

bool RiskManager::validateOrder(const Order& order) {
    return order.quantity <= MAX_ORDER_SIZE &&
           order.price <= MAX_PRICE &&
           order.price > 0;
}

}