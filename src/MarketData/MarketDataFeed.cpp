#include "orderbook/MarketData/MarketDataFeed.hpp"
#include "orderbook/Utilities/PerformanceTimer.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace orderbook {

MarketDataPublisher::MarketDataPublisher(LoggerPtr logger) 
    : sequence_number_(0), logger_(logger) {
    if (logger_) {
        logger_->info("MarketDataPublisher initialized", "MarketDataPublisher::ctor");
    }
}

void MarketDataPublisher::publishTrade(const Trade& trade) {
    PERF_TIMER("MarketDataPublisher::publishTrade", logger_);
    
    if (logger_) {
        logger_->debug("Publishing trade ID: " + std::to_string(trade.id.value) +
                      " Symbol: " + trade.symbol + " Price: " + std::to_string(trade.price) +
                      " Quantity: " + std::to_string(trade.quantity), "MarketDataPublisher::publishTrade");
    }
    
    auto start_time = getCurrentTimeNs();
    
    // Generate sequence number for gap detection
    SequenceNumber seq = ++sequence_number_;
    
    // Notify typed subscribers first (fastest path)
    notifyTypedSubscribers(trade, seq);
    
    // Format and notify string subscribers
    std::string message = formatTradeMessage(trade, seq);
    notifyStringSubscribers(message);
    
    // Record performance metrics
    auto end_time = getCurrentTimeNs();
    recordLatency(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.trades_published++;
    }
    
    if (logger_) {
        logger_->debug("Trade published successfully, sequence: " + std::to_string(seq), 
                      "MarketDataPublisher::publishTrade");
    }
}

void MarketDataPublisher::publishBookUpdate(const BookUpdate& update) {
    auto start_time = getCurrentTimeNs();
    
    // Notify typed subscribers
    notifyTypedSubscribers(update);
    
    // Format and notify string subscribers
    std::string message = formatBookUpdateMessage(update);
    notifyStringSubscribers(message);
    
    // Record performance metrics
    auto end_time = getCurrentTimeNs();
    recordLatency(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.book_updates_published++;
    }
}

void MarketDataPublisher::publishBestPrices(const BestPrices& prices) {
    auto start_time = getCurrentTimeNs();
    
    // Generate sequence number for gap detection
    SequenceNumber seq = ++sequence_number_;
    
    // Notify typed subscribers
    notifyTypedSubscribers(prices, seq);
    
    // Format and notify string subscribers
    std::string message = formatBestPricesMessage(prices, seq);
    notifyStringSubscribers(message);
    
    // Record performance metrics
    auto end_time = getCurrentTimeNs();
    recordLatency(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.best_price_updates_published++;
    }
}

void MarketDataPublisher::publishDepth(const MarketDepth& depth) {
    auto start_time = getCurrentTimeNs();
    
    // Generate sequence number for gap detection
    SequenceNumber seq = ++sequence_number_;
    
    // Notify typed subscribers
    notifyTypedSubscribers(depth, seq);
    
    // Format and notify string subscribers
    std::string message = formatDepthMessage(depth, seq);
    notifyStringSubscribers(message);
    
    // Record performance metrics
    auto end_time = getCurrentTimeNs();
    recordLatency(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.depth_updates_published++;
    }
}

void MarketDataPublisher::subscribe(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    string_subscribers_.push_back(std::move(callback));
}

void MarketDataPublisher::subscribe(std::shared_ptr<IMarketDataSubscriber> subscriber) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    typed_subscribers_.push_back(subscriber);
}

void MarketDataPublisher::unsubscribe(std::shared_ptr<IMarketDataSubscriber> subscriber) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    // Remove matching subscribers
    typed_subscribers_.erase(
        std::remove_if(typed_subscribers_.begin(), typed_subscribers_.end(),
            [&subscriber](const std::weak_ptr<IMarketDataSubscriber>& weak_sub) {
                auto shared_sub = weak_sub.lock();
                return !shared_sub || shared_sub == subscriber;
            }),
        typed_subscribers_.end()
    );
}

SequenceNumber MarketDataPublisher::getSequenceNumber() const {
    return sequence_number_.load();
}

size_t MarketDataPublisher::getSubscriberCount() const {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    // Clean up expired subscribers before counting
    const_cast<MarketDataPublisher*>(this)->cleanupExpiredSubscribers();
    
    return string_subscribers_.size() + typed_subscribers_.size();
}

uint64_t MarketDataPublisher::getTotalPublishedMessages() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.trades_published + stats_.book_updates_published + 
           stats_.best_price_updates_published + stats_.depth_updates_published;
}

MarketDataPublisher::PublishingStats MarketDataPublisher::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void MarketDataPublisher::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = PublishingStats{};
}

std::string MarketDataPublisher::formatTradeMessage(const Trade& trade, SequenceNumber seq) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "TRADE|" << seq << "|" << trade.id.value << "|" << trade.symbol 
        << "|" << trade.price << "|" << trade.quantity 
        << "|" << trade.buy_order_id.value << "|" << trade.sell_order_id.value
        << "|" << std::chrono::duration_cast<std::chrono::microseconds>(
               trade.timestamp.time_since_epoch()).count();
    return oss.str();
}

std::string MarketDataPublisher::formatBookUpdateMessage(const BookUpdate& update) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "BOOK|" << update.sequence << "|";
    
    switch (update.type) {
        case BookUpdate::Type::Add: oss << "ADD"; break;
        case BookUpdate::Type::Remove: oss << "REMOVE"; break;
        case BookUpdate::Type::Modify: oss << "MODIFY"; break;
    }
    
    oss << "|" << (update.side == Side::Buy ? "BUY" : "SELL")
        << "|" << update.price << "|" << update.quantity 
        << "|" << update.order_count
        << "|" << std::chrono::duration_cast<std::chrono::microseconds>(
               update.timestamp.time_since_epoch()).count();
    return oss.str();
}

std::string MarketDataPublisher::formatBestPricesMessage(const BestPrices& prices, SequenceNumber seq) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "BEST|" << seq << "|";
    
    if (prices.bid.has_value()) {
        oss << prices.bid.value() << "|" << prices.bid_size.value_or(0);
    } else {
        oss << "0|0";
    }
    
    oss << "|";
    
    if (prices.ask.has_value()) {
        oss << prices.ask.value() << "|" << prices.ask_size.value_or(0);
    } else {
        oss << "0|0";
    }
    
    oss << "|" << std::chrono::duration_cast<std::chrono::microseconds>(
               prices.timestamp.time_since_epoch()).count();
    return oss.str();
}

std::string MarketDataPublisher::formatDepthMessage(const MarketDepth& depth, SequenceNumber seq) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "DEPTH|" << seq << "|";
    
    // Format bids
    oss << depth.bids.size();
    for (const auto& level : depth.bids) {
        oss << "|" << level.price << "|" << level.quantity << "|" << level.order_count;
    }
    
    // Format asks
    oss << "|" << depth.asks.size();
    for (const auto& level : depth.asks) {
        oss << "|" << level.price << "|" << level.quantity << "|" << level.order_count;
    }
    
    oss << "|" << std::chrono::duration_cast<std::chrono::microseconds>(
               depth.timestamp.time_since_epoch()).count();
    return oss.str();
}

void MarketDataPublisher::notifyStringSubscribers(const std::string& message) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    for (const auto& subscriber : string_subscribers_) {
        try {
            subscriber(message);
        } catch (const std::exception& e) {
            // Log error but continue with other subscribers
            // In a real implementation, we'd use the logger here
        }
    }
}

void MarketDataPublisher::notifyTypedSubscribers(const Trade& trade, SequenceNumber seq) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    for (auto it = typed_subscribers_.begin(); it != typed_subscribers_.end();) {
        if (auto subscriber = it->lock()) {
            try {
                subscriber->onTrade(trade, seq);
                ++it;
            } catch (const std::exception& e) {
                // Log error but continue with other subscribers
                ++it;
            }
        } else {
            // Remove expired weak pointer
            it = typed_subscribers_.erase(it);
        }
    }
}

void MarketDataPublisher::notifyTypedSubscribers(const BookUpdate& update) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    for (auto it = typed_subscribers_.begin(); it != typed_subscribers_.end();) {
        if (auto subscriber = it->lock()) {
            try {
                subscriber->onBookUpdate(update);
                ++it;
            } catch (const std::exception& e) {
                // Log error but continue with other subscribers
                ++it;
            }
        } else {
            // Remove expired weak pointer
            it = typed_subscribers_.erase(it);
        }
    }
}

void MarketDataPublisher::notifyTypedSubscribers(const BestPrices& prices, SequenceNumber seq) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    for (auto it = typed_subscribers_.begin(); it != typed_subscribers_.end();) {
        if (auto subscriber = it->lock()) {
            try {
                subscriber->onBestPrices(prices, seq);
                ++it;
            } catch (const std::exception& e) {
                // Log error but continue with other subscribers
                ++it;
            }
        } else {
            // Remove expired weak pointer
            it = typed_subscribers_.erase(it);
        }
    }
}

void MarketDataPublisher::notifyTypedSubscribers(const MarketDepth& depth, SequenceNumber seq) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    for (auto it = typed_subscribers_.begin(); it != typed_subscribers_.end();) {
        if (auto subscriber = it->lock()) {
            try {
                subscriber->onDepth(depth, seq);
                ++it;
            } catch (const std::exception& e) {
                // Log error but continue with other subscribers
                ++it;
            }
        } else {
            // Remove expired weak pointer
            it = typed_subscribers_.erase(it);
        }
    }
}

void MarketDataPublisher::recordLatency(uint64_t latency_ns) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_latency_ns += latency_ns;
    stats_.max_latency_ns = std::max(stats_.max_latency_ns, latency_ns);
    stats_.min_latency_ns = std::min(stats_.min_latency_ns, latency_ns);
}

uint64_t MarketDataPublisher::getCurrentTimeNs() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void MarketDataPublisher::cleanupExpiredSubscribers() {
    // Remove expired weak pointers
    typed_subscribers_.erase(
        std::remove_if(typed_subscribers_.begin(), typed_subscribers_.end(),
            [](const std::weak_ptr<IMarketDataSubscriber>& weak_sub) {
                return weak_sub.expired();
            }),
        typed_subscribers_.end()
    );
}

}