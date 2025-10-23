#pragma once
#include "../Core/Types.hpp"
#include "../Core/Interfaces.hpp"
#include "../Core/Order.hpp"
#include "../Core/MarketData.hpp"
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <chrono>

namespace orderbook {

/**
 * @brief Market data subscriber interface for receiving updates
 */
class IMarketDataSubscriber {
public:
    virtual ~IMarketDataSubscriber() = default;
    
    /**
     * @brief Called when a trade is published
     * @param trade The executed trade
     * @param sequence Sequence number for gap detection
     */
    virtual void onTrade(const Trade& trade, SequenceNumber sequence) = 0;
    
    /**
     * @brief Called when book update is published
     * @param update Book level changes
     */
    virtual void onBookUpdate(const BookUpdate& update) = 0;
    
    /**
     * @brief Called when best prices are updated
     * @param prices Current best prices
     * @param sequence Sequence number for gap detection
     */
    virtual void onBestPrices(const BestPrices& prices, SequenceNumber sequence) = 0;
    
    /**
     * @brief Called when market depth is published
     * @param depth Full market depth snapshot
     * @param sequence Sequence number for gap detection
     */
    virtual void onDepth(const MarketDepth& depth, SequenceNumber sequence) = 0;
};

/**
 * @brief Concrete implementation of market data publishing with subscriber management
 */
class MarketDataPublisher : public IMarketDataPublisher {
public:
    explicit MarketDataPublisher(LoggerPtr logger = nullptr);
    ~MarketDataPublisher() = default;
    
    // IMarketDataPublisher interface implementation
    void publishTrade(const Trade& trade) override;
    void publishBookUpdate(const BookUpdate& update) override;
    void publishBestPrices(const BestPrices& prices) override;
    void publishDepth(const MarketDepth& depth) override;
    void subscribe(std::function<void(const std::string&)> callback) override;
    
    // Enhanced subscriber management
    void subscribe(std::shared_ptr<IMarketDataSubscriber> subscriber);
    void unsubscribe(std::shared_ptr<IMarketDataSubscriber> subscriber);
    
    // Statistics and monitoring
    SequenceNumber getSequenceNumber() const;
    size_t getSubscriberCount() const;
    uint64_t getTotalPublishedMessages() const;
    
    // Performance metrics
    struct PublishingStats {
        uint64_t trades_published = 0;
        uint64_t book_updates_published = 0;
        uint64_t best_price_updates_published = 0;
        uint64_t depth_updates_published = 0;
        uint64_t total_latency_ns = 0;
        uint64_t max_latency_ns = 0;
        uint64_t min_latency_ns = UINT64_MAX;
    };
    
    PublishingStats getStats() const;
    void resetStats();

private:
    // Subscriber management
    std::vector<std::function<void(const std::string&)>> string_subscribers_;
    std::vector<std::weak_ptr<IMarketDataSubscriber>> typed_subscribers_;
    mutable std::mutex subscribers_mutex_;
    
    // Sequence numbering for gap detection
    std::atomic<SequenceNumber> sequence_number_;
    
    // Performance tracking
    mutable std::mutex stats_mutex_;
    PublishingStats stats_;
    
    // Logging
    LoggerPtr logger_;
    
    // Helper methods for message formatting
    std::string formatTradeMessage(const Trade& trade, SequenceNumber seq) const;
    std::string formatBookUpdateMessage(const BookUpdate& update) const;
    std::string formatBestPricesMessage(const BestPrices& prices, SequenceNumber seq) const;
    std::string formatDepthMessage(const MarketDepth& depth, SequenceNumber seq) const;
    
    // Notification methods
    void notifyStringSubscribers(const std::string& message);
    void notifyTypedSubscribers(const Trade& trade, SequenceNumber seq);
    void notifyTypedSubscribers(const BookUpdate& update);
    void notifyTypedSubscribers(const BestPrices& prices, SequenceNumber seq);
    void notifyTypedSubscribers(const MarketDepth& depth, SequenceNumber seq);
    
    // Performance tracking
    void recordLatency(uint64_t latency_ns);
    uint64_t getCurrentTimeNs() const;
    
    // Cleanup expired weak pointers
    void cleanupExpiredSubscribers();
};

}