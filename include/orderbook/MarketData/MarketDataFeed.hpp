#pragma once
#include "../Core/OrderBook.hpp"
#include <functional>
#include <vector>

namespace orderbook {

class MarketDataFeed {
public:
    struct MarketData {
        double bestBid;
        double bestAsk;
        uint64_t bidSize;
        uint64_t askSize;
    };

    void subscribe(std::function<void(const MarketData&)> callback);
    void publishUpdate(const OrderBook& book);

private:
    std::vector<std::function<void(const MarketData&)>> subscribers_;
};

}