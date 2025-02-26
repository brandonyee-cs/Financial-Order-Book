#include "orderbook/Core/OrderBook.hpp"
#include <algorithm>
#include <iostream>

namespace orderbook {

template <typename PriceType, typename QuantityType>
OrderBook<PriceType, QuantityType>::OrderBook(const std::string& symbol)
    : symbol_(symbol) {
    constexpr size_t TYPICAL_PRICE_LEVELS = 100;
    bids_.reserve(TYPICAL_PRICE_LEVELS);
    asks_.reserve(TYPICAL_PRICE_LEVELS);
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::addOrder(const Order& order) {
    if (!riskManager_.validateOrder(order)) {
        std::cerr << "Order validation failed for ID: " << order.id.value << "\n";
        return;
    }

    Order workingOrder = order;
    workingOrder.timestamp = std::chrono::system_clock::now();

    if (workingOrder.type == Order::Type::Market) {
        processMarketOrder(workingOrder);
    } else {
        processLimitOrder(workingOrder);
    }

    publishMarketDataUpdate();
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::cancelOrder(uint64_t orderId) {
    auto it = orderMap_.find(OrderId(orderId));
    if (it == orderMap_.end()) {
        std::cerr << "Cancel failed: Order ID " << orderId << " not found\n";
        return;
    }

    const auto& location = it->second;
    auto& priceVector = location.isBid ? bids_ : asks_;
    auto& priceMap = location.isBid ? bidPriceMap_ : askPriceMap_;
    
    auto priceMapIt = priceMap.find(location.price);
    if (priceMapIt != priceMap.end()) {
        std::size_t levelIndex = priceMapIt->second;
        if (levelIndex < priceVector.size()) {
            auto& priceLevel = priceVector[levelIndex];
            
            priceLevel.totalQuantity -= location.orderIt->quantity;
            
            priceLevel.orders.erase(location.orderIt);
            
            if (priceLevel.orders.empty()) {
                cleanupPriceLevels(priceVector, priceMap, location.price);
            }
        }
    }

    orderMap_.erase(it);
    publishMarketDataUpdate();
}

template <typename PriceType, typename QuantityType>
bool OrderBook<PriceType, QuantityType>::modifyOrder(uint64_t orderId, PriceType newPrice, QuantityType newQuantity) {
    auto it = orderMap_.find(OrderId(orderId));
    if (it == orderMap_.end()) {
        std::cerr << "Modify failed: Order ID " << orderId << " not found\n";
        return false;
    }

    const auto& location = it->second;
    auto& priceVector = location.isBid ? bids_ : asks_;
    auto& priceMap = location.isBid ? bidPriceMap_ : askPriceMap_;
    
    if (std::abs(location.price - newPrice) < 0.000001) {
        auto priceMapIt = priceMap.find(location.price);
        if (priceMapIt != priceMap.end()) {
            std::size_t levelIndex = priceMapIt->second;
            if (levelIndex < priceVector.size()) {
                auto& priceLevel = priceVector[levelIndex];
                
                QuantityType quantityDelta = newQuantity - location.orderIt->quantity;
                priceLevel.totalQuantity += quantityDelta;
                
                location.orderIt->quantity = newQuantity;
                
                publishMarketDataUpdate();
                return true;
            }
        }
        return false;
    }
    
    Order originalOrder = *(location.orderIt);
    
    cancelOrder(orderId);
    
    Order newOrder = originalOrder;
    newOrder.price = newPrice;
    newOrder.quantity = newQuantity;
    newOrder.timestamp = std::chrono::system_clock::now();
    
    addOrder(newOrder);
    
    return true;
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::processLimitOrder(Order& order) {

    matchOrders(order);
    
    if (order.quantity > 0 && order.tif != Order::TIF::IOC) {
        addToBook(order);
    }
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::matchOrders(Order& order) {
    auto& oppositeVector = (order.side == Order::Side::Buy) ? asks_ : bids_;
    
    while (!oppositeVector.empty() && order.quantity > 0) {
        auto& bestLevel = oppositeVector.back();

        bool priceMatches = (order.side == Order::Side::Buy) 
            ? (order.type == Order::Type::Market || bestLevel.price <= order.price)
            : (order.type == Order::Type::Market || bestLevel.price >= order.price);
            
        if (!priceMatches) break;
        
        auto& orders = bestLevel.orders;
        
        while (!orders.empty() && order.quantity > 0) {
            auto& matchOrder = orders.front();  

            QuantityType tradeQty = std::min(order.quantity, matchOrder.quantity);
            
            std::cout << "TRADE EXECUTED: " << tradeQty << " @ " << bestLevel.price
                      << " BETWEEN " << order.id.value << " AND " << matchOrder.id.value << "\n";
            
            order.quantity -= tradeQty;
            matchOrder.quantity -= tradeQty;
            bestLevel.totalQuantity -= tradeQty;
            
            if (matchOrder.quantity == 0) {
                orderMap_.erase(matchOrder.id);
                orders.pop_front();
            }
        }
        
        if (orders.empty()) {
            auto& oppositeMap = (order.side == Order::Side::Buy) ? askPriceMap_ : bidPriceMap_;
            cleanupPriceLevels(oppositeVector, oppositeMap, bestLevel.price);
        }
    }
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::processMarketOrder(Order& order) {

    matchOrders(order);
    
    if (order.quantity > 0) {
        std::cerr << "Market order partially filled, " << order.quantity << " remaining unfilled\n";
    }
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::addToBook(Order& order) {
    auto& priceVector = (order.side == Order::Side::Buy) ? bids_ : asks_;
    auto& priceMap = (order.side == Order::Side::Buy) ? bidPriceMap_ : askPriceMap_;
    
    std::size_t levelIndex = findOrCreatePriceLevel(priceVector, priceMap, order.price);
    
    auto& priceLevel = priceVector[levelIndex];
    
    priceLevel.orders.push_back(order);
    priceLevel.totalQuantity += order.quantity;
    
    OrderLocation location{
        order.side == Order::Side::Buy,
        order.price,
        std::prev(priceLevel.orders.end())
    };
    
    orderMap_[order.id] = location;
}

template <typename PriceType, typename QuantityType>
std::size_t OrderBook<PriceType, QuantityType>::findOrCreatePriceLevel(
    std::vector<PriceLevel>& levels, 
    std::unordered_map<PriceType, std::size_t>& priceMap,
    PriceType price) {
    
    auto it = priceMap.find(price);
    if (it != priceMap.end()) {
        return it->second;
    }
    
    PriceLevel newLevel;
    newLevel.price = price;
    newLevel.totalQuantity = 0;
    
    auto insertIt = std::lower_bound(levels.begin(), levels.end(), newLevel,
        [&](const PriceLevel& a, const PriceLevel& b) {
            return &levels == &bids_ ? a.price > b.price : a.price < b.price;
        });
    
    auto insertedIt = levels.insert(insertIt, newLevel);
    std::size_t index = std::distance(levels.begin(), insertedIt);
    
    priceMap[price] = index;
    
    for (auto mapIt = priceMap.begin(); mapIt != priceMap.end(); ++mapIt) {
        if (mapIt->second >= index && mapIt->first != price) {
            mapIt->second++;
        }
    }
    
    return index;
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::cleanupPriceLevels(
    std::vector<PriceLevel>& levels, 
    std::unordered_map<PriceType, std::size_t>& priceMap,
    PriceType price) {
    
    auto it = priceMap.find(price);
    if (it == priceMap.end()) return;
    
    std::size_t index = it->second;
    if (index >= levels.size()) return;
    
    levels.erase(levels.begin() + index);
    
    priceMap.erase(it);
    
    for (auto& mapEntry : priceMap) {
        if (mapEntry.second > index) {
            mapEntry.second--;
        }
    }
}

template <typename PriceType, typename QuantityType>
std::optional<PriceType> OrderBook<PriceType, QuantityType>::getBestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.back().price;  
}

template <typename PriceType, typename QuantityType>
std::optional<PriceType> OrderBook<PriceType, QuantityType>::getBestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.back().price;  
}

template <typename PriceType, typename QuantityType>
PriceType OrderBook<PriceType, QuantityType>::getSpread() const {
    auto bid = getBestBid();
    auto ask = getBestAsk();
    
    if (!bid || !ask) return 0;
    return *ask - *bid;
}

template <typename PriceType, typename QuantityType>
void OrderBook<PriceType, QuantityType>::publishMarketDataUpdate() {
    std::cout << "BOOK UPDATE - Best Bid: " 
              << getBestBid().value_or(0)
              << " | Best Ask: "
              << getBestAsk().value_or(0) << "\n";
    
    marketData_.publishUpdate(*this);
}

template class OrderBook<double, uint64_t>;

}