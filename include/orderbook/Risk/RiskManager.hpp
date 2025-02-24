#pragma once
#include "../Core/Order.hpp"

namespace orderbook {

class RiskManager {
public:
    bool validateOrder(const Order& order);
    
private:
    const uint64_t MAX_ORDER_SIZE = 10000;
    const double MAX_PRICE = 1000000.0;
};

}