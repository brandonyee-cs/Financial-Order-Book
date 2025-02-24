#include "orderbook/MarketData/MarketDataFeed.hpp"

namespace orderbook {

void MarketDataFeed::subscribe(std::function<void(const MarketData&)> callback) {
    subscribers_.push_back(callback);
}

void MarketDataFeed::publishUpdate(const OrderBook& book) {
    MarketData data{
        book.getBestBid().value_or(0),
        book.getBestAsk().value_or(0),
    };
    
    for (auto& sub : subscribers_) {
        sub(data);
    }
}

}