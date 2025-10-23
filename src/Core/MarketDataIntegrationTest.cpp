#include "orderbook/OrderBook.hpp"
#include <iostream>
#include <memory>
#include <vector>

namespace orderbook {

/**
 * @brief Simple market data subscriber for testing
 */
class TestMarketDataSubscriber : public IMarketDataSubscriber {
public:
    void onTrade(const Trade& trade, SequenceNumber sequence) override {
        std::cout << "TRADE [" << sequence << "]: " << trade.symbol 
                  << " " << trade.quantity << "@" << trade.price 
                  << " (Buy: " << trade.buy_order_id.value 
                  << ", Sell: " << trade.sell_order_id.value << ")" << std::endl;
        trades_received_++;
    }
    
    void onBookUpdate(const BookUpdate& update) override {
        std::cout << "BOOK [" << update.sequence << "]: ";
        switch (update.type) {
            case BookUpdate::Type::Add: std::cout << "ADD"; break;
            case BookUpdate::Type::Remove: std::cout << "REMOVE"; break;
            case BookUpdate::Type::Modify: std::cout << "MODIFY"; break;
        }
        std::cout << " " << (update.side == Side::Buy ? "BUY" : "SELL")
                  << " " << update.quantity << "@" << update.price 
                  << " (Orders: " << update.order_count << ")" << std::endl;
        book_updates_received_++;
    }
    
    void onBestPrices(const BestPrices& prices, SequenceNumber sequence) override {
        std::cout << "BEST [" << sequence << "]: ";
        if (prices.bid.has_value()) {
            std::cout << "Bid=" << prices.bid.value() << "(" << prices.bid_size.value_or(0) << ") ";
        }
        if (prices.ask.has_value()) {
            std::cout << "Ask=" << prices.ask.value() << "(" << prices.ask_size.value_or(0) << ")";
        }
        std::cout << std::endl;
        best_price_updates_received_++;
    }
    
    void onDepth(const MarketDepth& depth, SequenceNumber sequence) override {
        std::cout << "DEPTH [" << sequence << "]: " << depth.bids.size() 
                  << " bids, " << depth.asks.size() << " asks" << std::endl;
        depth_updates_received_++;
    }
    
    // Statistics
    size_t trades_received_ = 0;
    size_t book_updates_received_ = 0;
    size_t best_price_updates_received_ = 0;
    size_t depth_updates_received_ = 0;
};

/**
 * @brief Test market data integration with OrderBook
 */
void testMarketDataIntegration() {
    std::cout << "=== Market Data Integration Test ===" << std::endl;
    
    // Create OrderBook with market data publisher
    auto orderbook = createOrderBook();
    
    // Create test subscriber
    auto subscriber = std::make_shared<TestMarketDataSubscriber>();
    
    // Get the market data publisher and subscribe
    // Note: In a real implementation, we'd need a way to access the publisher
    // For now, we'll demonstrate the functionality through order operations
    
    std::cout << "\n1. Adding buy orders..." << std::endl;
    
    // Add some buy orders
    Order buy1(OrderId(1), Side::Buy, OrderType::Limit, TimeInForce::GTC, 
               100.0, 1000, "AAPL", "account1");
    auto result1 = orderbook->addOrder(buy1);
    
    Order buy2(OrderId(2), Side::Buy, OrderType::Limit, TimeInForce::GTC, 
               99.5, 500, "AAPL", "account1");
    auto result2 = orderbook->addOrder(buy2);
    
    std::cout << "\n2. Adding sell orders..." << std::endl;
    
    // Add some sell orders
    Order sell1(OrderId(3), Side::Sell, OrderType::Limit, TimeInForce::GTC, 
                101.0, 800, "AAPL", "account2");
    auto result3 = orderbook->addOrder(sell1);
    
    Order sell2(OrderId(4), Side::Sell, OrderType::Limit, TimeInForce::GTC, 
                101.5, 300, "AAPL", "account2");
    auto result4 = orderbook->addOrder(sell2);
    
    std::cout << "\n3. Adding matching order (should trigger trade)..." << std::endl;
    
    // Add a buy order that should match with sell1
    Order buy3(OrderId(5), Side::Buy, OrderType::Limit, TimeInForce::GTC, 
               101.0, 600, "AAPL", "account3");
    auto result5 = orderbook->addOrder(buy3);
    
    std::cout << "\n4. Modifying an order..." << std::endl;
    
    // Modify an existing order
    auto modify_result = orderbook->modifyOrder(OrderId(2), 99.8, 750);
    
    std::cout << "\n5. Canceling an order..." << std::endl;
    
    // Cancel an order
    auto cancel_result = orderbook->cancelOrder(OrderId(4));
    
    std::cout << "\n6. Current market state:" << std::endl;
    std::cout << "Best Bid: " << (orderbook->bestBid().has_value() ? 
                                  std::to_string(orderbook->bestBid().value()) : "None") << std::endl;
    std::cout << "Best Ask: " << (orderbook->bestAsk().has_value() ? 
                                  std::to_string(orderbook->bestAsk().value()) : "None") << std::endl;
    std::cout << "Total Orders: " << orderbook->getOrderCount() << std::endl;
    std::cout << "Bid Levels: " << orderbook->getBidLevelCount() << std::endl;
    std::cout << "Ask Levels: " << orderbook->getAskLevelCount() << std::endl;
    
    // Display market depth
    auto depth = orderbook->getDepth(3);
    std::cout << "\nMarket Depth (Top 3 levels):" << std::endl;
    std::cout << "Bids:" << std::endl;
    for (const auto& level : depth.bids) {
        std::cout << "  " << level.quantity << "@" << level.price 
                  << " (" << level.order_count << " orders)" << std::endl;
    }
    std::cout << "Asks:" << std::endl;
    for (const auto& level : depth.asks) {
        std::cout << "  " << level.quantity << "@" << level.price 
                  << " (" << level.order_count << " orders)" << std::endl;
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
}

} // namespace orderbook

int main() {
    try {
        orderbook::testMarketDataIntegration();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}