#include "orderbook/Core/OrderBook.hpp"
#include "orderbook/Utilities/PerformanceTimer.hpp"
#include "orderbook/Utilities/MemoryManager.hpp"
#include "orderbook/Utilities/PerformanceMeasurement.hpp"
#include <algorithm>
#include <chrono>

namespace orderbook {

// Constructor with dependency injection
OrderBook::OrderBook(RiskManagerPtr risk_manager,
                     MarketDataPublisherPtr market_data,
                     LoggerPtr logger)
    : risk_manager_(risk_manager), market_data_(market_data), logger_(logger) {
    
    // Reserve capacity for typical number of price levels
    bids_.reserve(InitialCapacity);
    asks_.reserve(InitialCapacity);
    
    if (logger_) {
        logger_->info("OrderBook initialized", "OrderBook::Constructor");
    }
}

// Core operations
OrderResult OrderBook::addOrder(const Order& order) {
    PERF_TIMER("OrderBook::addOrder", logger_);
    PERF_MEASURE("OrderBook::addOrder");
    
    if (logger_) {
        logger_->debug("Adding order ID: " + std::to_string(order.id.value) + 
                      " Side: " + (order.side == Side::Buy ? "Buy" : "Sell") +
                      " Price: " + std::to_string(order.price) +
                      " Quantity: " + std::to_string(order.quantity), "OrderBook::addOrder");
    }
    
    // Risk validation if risk manager is available
    if (risk_manager_) {
        // Associate order with account
        std::string account = order.account.empty() ? "default" : order.account;
        risk_manager_->associateOrderWithAccount(order.id, account);
        
        // Get the portfolio for the order's account
        const Portfolio& portfolio = risk_manager_->getPortfolio(account);
        auto risk_check = risk_manager_->validateOrder(order, portfolio);
        if (risk_check.isRejected()) {
            if (logger_) {
                logger_->error("Order rejected by risk manager: " + risk_check.reason, 
                              "OrderBook::addOrder - OrderID: " + std::to_string(order.id.value) + 
                              " Account: " + account);
            }
            return OrderResult::error("Risk validation failed: " + risk_check.reason);
        }
        
        if (logger_) {
            logger_->debug("Order passed risk validation: " + risk_check.reason, 
                          "OrderBook::addOrder - OrderID: " + std::to_string(order.id.value) + 
                          " Account: " + account);
        }
    }
    
    // Check if order already exists
    if (order_index_.find(order.id) != order_index_.end()) {
        if (logger_) {
            logger_->error("Duplicate order ID: " + std::to_string(order.id.value), 
                          "OrderBook::addOrder");
        }
        return OrderResult::error("Order ID already exists");
    }
    
    // Create a copy of the order for processing
    auto order_copy = std::make_unique<Order>(order);
    order_copy->timestamp = std::chrono::system_clock::now();
    
    // Find or create price level
    PriceLevel* price_level = findOrCreatePriceLevel(order_copy->price, order_copy->side);
    if (!price_level) {
        if (logger_) {
            logger_->error("Failed to create price level for price: " + std::to_string(order_copy->price) +
                          " side: " + (order_copy->side == Side::Buy ? "Buy" : "Sell"), 
                          "OrderBook::addOrder - OrderID: " + std::to_string(order_copy->id.value));
        }
        return OrderResult::error("Failed to create price level");
    }
    
    // Add order to price level
    price_level->addOrder(order_copy.get());
    
    // Add to order index for fast lookup
    OrderLocation location(order_copy.get(), price_level, order_copy->side);
    order_index_[order_copy->id] = location;
    
    // Release ownership - order is now managed by price level
    Order* order_ptr = order_copy.release();
    
    // Maintain sorted order
    maintainSortedOrder();
    
    // Publish book update for order addition
    publishBookUpdate(BookUpdate::Type::Add, order_ptr->side, order_ptr->price, 
                     order_ptr->remainingQuantity(), price_level->order_count);
    
    // Check for immediate matching opportunities
    processMatching(*order_ptr);
    
    // Publish market data update (best prices and depth)
    publishMarketDataUpdate();
    
    if (logger_) {
        logger_->info("Order added successfully ID: " + std::to_string(order_ptr->id.value), 
                     "OrderBook::addOrder");
    }
    
    return OrderResult::success(order_ptr->id);
}

CancelResult OrderBook::cancelOrder(OrderId id) {
    PERF_TIMER("OrderBook::cancelOrder", logger_);
    PERF_MEASURE("OrderBook::cancelOrder");
    
    if (logger_) {
        logger_->debug("Canceling order ID: " + std::to_string(id.value), "OrderBook::cancelOrder");
    }
    
    // Find order in index
    auto it = order_index_.find(id);
    if (it == order_index_.end()) {
        if (logger_) {
            logger_->warn("Cancel requested for non-existent order ID: " + std::to_string(id.value), 
                         "OrderBook::cancelOrder");
        }
        return CancelResult::error("Order not found");
    }
    
    const OrderLocation& location = it->second;
    if (!location.isValid()) {
        return CancelResult::error("Invalid order location");
    }
    
    // Store order details before removal for book update
    Side order_side = location.order->side;
    Price order_price = location.order->price;
    Quantity remaining_qty = location.order->remainingQuantity();
    
    // Remove from price level
    location.price_level->removeOrder(location.order);
    
    // Publish book update for order removal
    publishBookUpdate(BookUpdate::Type::Remove, order_side, order_price, 
                     remaining_qty, location.price_level->order_count);
    
    // Clean up empty price level
    if (location.price_level->isEmpty()) {
        removePriceLevel(location.price_level, location.side);
    }
    
    // Remove from order index
    order_index_.erase(it);
    
    // Delete the order
    delete location.order;
    
    // Publish market data update (best prices and depth)
    publishMarketDataUpdate();
    
    if (logger_) {
        logger_->info("Order canceled successfully ID: " + std::to_string(id.value), 
                     "OrderBook::cancelOrder");
    }
    
    return CancelResult::success(true);
}

ModifyResult OrderBook::modifyOrder(OrderId id, Price new_price, Quantity new_quantity) {
    PERF_TIMER("OrderBook::modifyOrder", logger_);
    PERF_MEASURE("OrderBook::modifyOrder");
    
    if (logger_) {
        logger_->debug("Modifying order ID: " + std::to_string(id.value) +
                      " New Price: " + std::to_string(new_price) +
                      " New Quantity: " + std::to_string(new_quantity), "OrderBook::modifyOrder");
    }
    
    // Find order in index
    auto it = order_index_.find(id);
    if (it == order_index_.end()) {
        return ModifyResult::error("Order not found");
    }
    
    const OrderLocation& location = it->second;
    if (!location.isValid()) {
        return ModifyResult::error("Invalid order location");
    }
    
    Order* order = location.order;
    
    // If price is changing, we need to move the order to a different price level
    if (new_price > 0 && std::abs(order->price - new_price) > MinPriceIncrement) {
        // Store old details for book update
        Price old_price = order->price;
        Quantity old_remaining = order->remainingQuantity();
        
        // Remove from current price level
        location.price_level->removeOrder(order);
        
        // Publish book update for removal from old price level
        publishBookUpdate(BookUpdate::Type::Remove, order->side, old_price, 
                         old_remaining, location.price_level->order_count);
        
        // Clean up empty price level
        if (location.price_level->isEmpty()) {
            removePriceLevel(location.price_level, location.side);
        }
        
        // Update order price
        order->price = new_price;
        
        // Find or create new price level
        PriceLevel* new_price_level = findOrCreatePriceLevel(new_price, order->side);
        if (!new_price_level) {
            // Restore order to original price level if new level creation fails
            order->price = old_price;
            location.price_level->addOrder(order);
            return ModifyResult::error("Failed to create new price level");
        }
        
        // Add to new price level
        new_price_level->addOrder(order);
        
        // Publish book update for addition to new price level
        publishBookUpdate(BookUpdate::Type::Add, order->side, new_price, 
                         order->remainingQuantity(), new_price_level->order_count);
        
        // Update order index
        order_index_[id] = OrderLocation(order, new_price_level, order->side);
        
        maintainSortedOrder();
    }
    
    // Update quantity if specified
    if (new_quantity > 0) {
        // Store old quantity for book update
        Quantity old_remaining = order->remainingQuantity();
        
        // Update price level quantity
        PriceLevel* current_level = order_index_[id].price_level;
        current_level->total_quantity -= old_remaining;
        
        order->quantity = new_quantity;
        // Reset filled quantity if new quantity is smaller
        if (order->filled_quantity > new_quantity) {
            order->filled_quantity = new_quantity;
        }
        
        current_level->total_quantity += order->remainingQuantity();
        
        // Publish book update for quantity modification
        publishBookUpdate(BookUpdate::Type::Modify, order->side, order->price, 
                         order->remainingQuantity(), current_level->order_count);
    }
    
    // Publish market data update
    publishMarketDataUpdate();
    
    if (logger_) {
        logger_->info("Order modified successfully ID: " + std::to_string(id.value), 
                     "OrderBook::modifyOrder");
    }
    
    return ModifyResult::success(true);
}

// Market data queries with O(1) performance
std::optional<Price> OrderBook::bestBid() const {
    PERF_MEASURE("OrderBook::bestBid");
    if (bids_.empty()) {
        return std::nullopt;
    }
    // Best bid is at the end (highest price)
    return bids_.back().price;
}

std::optional<Price> OrderBook::bestAsk() const {
    PERF_MEASURE("OrderBook::bestAsk");
    if (asks_.empty()) {
        return std::nullopt;
    }
    // Best ask is at the end (lowest price)
    return asks_.back().price;
}

BestPrices OrderBook::getBestPrices() const {
    BestPrices prices;
    prices.timestamp = std::chrono::system_clock::now();
    
    if (!bids_.empty()) {
        const auto& best_bid_level = bids_.back();
        prices.bid = best_bid_level.price;
        prices.bid_size = best_bid_level.total_quantity;
    }
    
    if (!asks_.empty()) {
        const auto& best_ask_level = asks_.back();
        prices.ask = best_ask_level.price;
        prices.ask_size = best_ask_level.total_quantity;
    }
    
    return prices;
}

MarketDepth OrderBook::getDepth(size_t levels) const {
    MarketDepth depth;
    depth.timestamp = std::chrono::system_clock::now();
    
    // Get bid levels (highest to lowest)
    size_t bid_count = std::min(levels, bids_.size());
    depth.bids.reserve(bid_count);
    
    for (size_t i = 0; i < bid_count; ++i) {
        const auto& level = bids_[bids_.size() - 1 - i]; // Start from best (end)
        depth.bids.emplace_back(MarketDepth::Level{
            level.price, 
            level.total_quantity, 
            level.order_count
        });
    }
    
    // Get ask levels (lowest to highest)
    size_t ask_count = std::min(levels, asks_.size());
    depth.asks.reserve(ask_count);
    
    for (size_t i = 0; i < ask_count; ++i) {
        const auto& level = asks_[asks_.size() - 1 - i]; // Start from best (end)
        depth.asks.emplace_back(MarketDepth::Level{
            level.price, 
            level.total_quantity, 
            level.order_count
        });
    }
    
    return depth;
}

// Statistics
size_t OrderBook::getOrderCount() const {
    return order_index_.size();
}

size_t OrderBook::getBidLevelCount() const {
    return bids_.size();
}

size_t OrderBook::getAskLevelCount() const {
    return asks_.size();
}

// Helper methods for price level management
PriceLevel* OrderBook::findOrCreatePriceLevel(Price price, Side side) {
    // Check if price level already exists in index
    auto it = price_index_.find(price);
    if (it != price_index_.end()) {
        // Verify the price level is still valid
        PriceLevel* level = it->second;
        if (level && std::abs(level->price - price) < MinPriceIncrement) {
            return level;
        }
        // Remove invalid entry
        price_index_.erase(it);
    }
    
    // Create new price level
    auto& levels = (side == Side::Buy) ? bids_ : asks_;
    
    // Find insertion point to maintain sorted order
    auto insert_pos = levels.end();
    if (side == Side::Buy) {
        // For bids: insert in ascending order (best bid at end)
        insert_pos = std::lower_bound(levels.begin(), levels.end(), price,
            [](const PriceLevel& level, Price p) {
                return level.price < p;
            });
    } else {
        // For asks: insert in descending order (best ask at end)
        insert_pos = std::lower_bound(levels.begin(), levels.end(), price,
            [](const PriceLevel& level, Price p) {
                return level.price > p;
            });
    }
    
    // Insert new price level at correct position
    auto inserted_it = levels.emplace(insert_pos, price);
    PriceLevel* new_level = &(*inserted_it);
    
    // Add to price index for O(1) lookup
    price_index_[price] = new_level;
    
    if (logger_) {
        logger_->debug("Created new price level at " + std::to_string(price) + 
                      " for " + (side == Side::Buy ? "bids" : "asks"), 
                      "OrderBook::findOrCreatePriceLevel");
    }
    
    return new_level;
}

void OrderBook::removePriceLevel(PriceLevel* level, Side side) {
    if (!level) {
        if (logger_) {
            logger_->warn("Attempted to remove null price level", "OrderBook::removePriceLevel");
        }
        return;
    }
    
    // Verify the level is actually empty before removing
    if (!level->isEmpty()) {
        if (logger_) {
            logger_->warn("Attempted to remove non-empty price level at " + 
                         std::to_string(level->price), "OrderBook::removePriceLevel");
        }
        return;
    }
    
    Price price = level->price;
    
    // Remove from price index first
    auto index_it = price_index_.find(price);
    if (index_it != price_index_.end()) {
        price_index_.erase(index_it);
    }
    
    // Remove from appropriate vector
    auto& levels = (side == Side::Buy) ? bids_ : asks_;
    
    // Find and remove the level efficiently
    auto it = std::find_if(levels.begin(), levels.end(),
        [level](const PriceLevel& pl) { return &pl == level; });
    
    if (it != levels.end()) {
        levels.erase(it);
        
        if (logger_) {
            logger_->debug("Removed empty price level at " + std::to_string(price) + 
                          " from " + (side == Side::Buy ? "bids" : "asks"), 
                          "OrderBook::removePriceLevel");
        }
    } else {
        if (logger_) {
            logger_->warn("Price level not found in vector during removal at " + 
                         std::to_string(price), "OrderBook::removePriceLevel");
        }
    }
}

void OrderBook::maintainSortedOrder() {
    // Check if vectors are already sorted to avoid unnecessary work
    bool bids_sorted = std::is_sorted(bids_.begin(), bids_.end(),
        [](const PriceLevel& a, const PriceLevel& b) {
            return a.price < b.price;
        });
    
    bool asks_sorted = std::is_sorted(asks_.begin(), asks_.end(),
        [](const PriceLevel& a, const PriceLevel& b) {
            return a.price > b.price;
        });
    
    // Only sort if necessary
    if (!bids_sorted) {
        std::sort(bids_.begin(), bids_.end(),
            [](const PriceLevel& a, const PriceLevel& b) {
                return a.price < b.price;
            });
        
        if (logger_) {
            logger_->debug("Sorted bid levels", "OrderBook::maintainSortedOrder");
        }
    }
    
    if (!asks_sorted) {
        std::sort(asks_.begin(), asks_.end(),
            [](const PriceLevel& a, const PriceLevel& b) {
                return a.price > b.price;
            });
        
        if (logger_) {
            logger_->debug("Sorted ask levels", "OrderBook::maintainSortedOrder");
        }
    }
    
    // Rebuild price index to ensure consistency after sorting
    rebuildPriceIndex();
}

void OrderBook::rebuildPriceIndex() {
    price_index_.clear();
    
    // Add all bid levels to index
    for (auto& level : bids_) {
        price_index_[level.price] = &level;
    }
    
    // Add all ask levels to index
    for (auto& level : asks_) {
        price_index_[level.price] = &level;
    }
    
    if (logger_) {
        logger_->debug("Rebuilt price index with " + std::to_string(price_index_.size()) + 
                      " levels", "OrderBook::rebuildPriceIndex");
    }
}

void OrderBook::publishMarketDataUpdate() {
    if (market_data_) {
        // Publish best prices with sub-10μs latency requirement
        BestPrices prices = getBestPrices();
        market_data_->publishBestPrices(prices);
        
        // Publish market depth (top 5 levels) for full book updates
        MarketDepth depth = getDepth(5);
        market_data_->publishDepth(depth);
    }
}

void OrderBook::publishBookUpdate(BookUpdate::Type type, Side side, Price price, 
                                 Quantity quantity, size_t order_count) {
    if (market_data_) {
        // Create sequence number for gap detection
        static std::atomic<SequenceNumber> book_sequence{0};
        SequenceNumber seq = ++book_sequence;
        
        BookUpdate update(type, side, price, quantity, order_count, seq);
        market_data_->publishBookUpdate(update);
    }
}

void OrderBook::processMatching(Order& incoming_order) {
    // Simple matching logic - check if we can match against opposite side
    auto& opposite_levels = (incoming_order.side == Side::Buy) ? asks_ : bids_;
    
    if (opposite_levels.empty()) {
        return; // No opposite side orders to match against
    }
    
    // Find best opposite price
    const auto& best_level = opposite_levels.back(); // Best price is at the end
    
    bool can_match = false;
    if (incoming_order.side == Side::Buy) {
        // Buy order can match if buy price >= ask price
        can_match = incoming_order.price >= best_level.price;
    } else {
        // Sell order can match if sell price <= bid price
        can_match = incoming_order.price <= best_level.price;
    }
    
    if (can_match && !best_level.isEmpty()) {
        // Execute trades
        executeMatching(incoming_order, opposite_levels);
    }
}

void OrderBook::executeMatching(Order& incoming_order, std::vector<PriceLevel>& opposite_levels) {
    // Sort levels for best price first
    if (incoming_order.side == Side::Buy) {
        // For buy orders, sort asks ascending (lowest price first)
        std::sort(opposite_levels.begin(), opposite_levels.end(),
                  [](const PriceLevel& a, const PriceLevel& b) {
                      return a.price < b.price;
                  });
    } else {
        // For sell orders, sort bids descending (highest price first)
        std::sort(opposite_levels.begin(), opposite_levels.end(),
                  [](const PriceLevel& a, const PriceLevel& b) {
                      return a.price > b.price;
                  });
    }
    
    // Match against price levels
    for (auto& level : opposite_levels) {
        if (level.isEmpty() || incoming_order.isFullyFilled()) {
            break;
        }
        
        // Check if we can still match at this price
        bool can_match = false;
        if (incoming_order.side == Side::Buy) {
            can_match = incoming_order.price >= level.price;
        } else {
            can_match = incoming_order.price <= level.price;
        }
        
        if (!can_match) {
            break; // No more matches possible
        }
        
        // Match against orders in this price level
        matchAgainstPriceLevel(incoming_order, level);
    }
}

void OrderBook::matchAgainstPriceLevel(Order& incoming_order, PriceLevel& price_level) {
    Order* current_order = price_level.getFirstOrder();
    
    while (current_order && !incoming_order.isFullyFilled()) {
        // Calculate trade quantity
        Quantity trade_quantity = std::min(incoming_order.remainingQuantity(),
                                          current_order->remainingQuantity());
        
        if (trade_quantity == 0) break;
        
        // Execute the trade
        executeTrade(incoming_order, *current_order, price_level.price, trade_quantity);
        
        // Update price level quantity
        price_level.updateQuantity(current_order, trade_quantity);
        
        // Publish book update for the passive order modification/removal
        if (current_order->isFullyFilled()) {
            // Order fully filled - publish removal
            publishBookUpdate(BookUpdate::Type::Remove, current_order->side, 
                             current_order->price, 0, price_level.order_count - 1);
        } else {
            // Order partially filled - publish modification
            publishBookUpdate(BookUpdate::Type::Modify, current_order->side, 
                             current_order->price, current_order->remainingQuantity(), 
                             price_level.order_count);
        }
        
        // Move to next order if current is fully filled
        if (current_order->isFullyFilled()) {
            Order* next_order = current_order->next;
            
            // Remove filled order from order index
            auto it = order_index_.find(current_order->id);
            if (it != order_index_.end()) {
                order_index_.erase(it);
            }
            
            // Delete the filled order
            delete current_order;
            current_order = next_order;
        }
    }
    
    // Clean up empty price level
    if (price_level.isEmpty()) {
        Side opposite_side = (incoming_order.side == Side::Buy) ? Side::Sell : Side::Buy;
        removePriceLevel(&price_level, opposite_side);
    }
}

void OrderBook::executeTrade(Order& aggressive_order, Order& passive_order, 
                            Price trade_price, Quantity trade_quantity) {
    // Fill both orders
    aggressive_order.fill(trade_quantity);
    passive_order.fill(trade_quantity);
    
    // Create trade record
    static uint64_t trade_counter = 1;
    TradeId trade_id(trade_counter++);
    
    // Determine buy/sell order IDs
    OrderId buy_order_id = aggressive_order.isBuy() ? aggressive_order.id : passive_order.id;
    OrderId sell_order_id = aggressive_order.isSell() ? aggressive_order.id : passive_order.id;
    
    Trade trade(trade_id.value, buy_order_id, sell_order_id, 
               trade_price, trade_quantity, aggressive_order.symbol);
    
    // Update positions through risk manager
    if (risk_manager_) {
        risk_manager_->updatePosition(trade);
        
        if (logger_) {
            logger_->debug("Updated positions for trade ID: " + std::to_string(trade.id.value), 
                          "OrderBook::executeTrade");
        }
    }
    
    // Publish trade to market data
    if (market_data_) {
        market_data_->publishTrade(trade);
    }
    
    // Log trade execution with risk context
    if (logger_) {
        std::string buy_account = risk_manager_ ? risk_manager_->getAccountForOrder(trade.buy_order_id) : "unknown";
        std::string sell_account = risk_manager_ ? risk_manager_->getAccountForOrder(trade.sell_order_id) : "unknown";
        
        logger_->info("Trade executed: ID=" + std::to_string(trade.id.value) + 
                     " Buy=" + std::to_string(trade.buy_order_id.value) + " (Account: " + buy_account + ")" +
                     " Sell=" + std::to_string(trade.sell_order_id.value) + " (Account: " + sell_account + ")" +
                     " Price=" + std::to_string(trade.price) + 
                     " Qty=" + std::to_string(trade.quantity) + 
                     " Symbol=" + trade.symbol,
                     "OrderBook::executeTrade");
        
        // Log position updates
        if (risk_manager_) {
            const Portfolio& buy_portfolio = risk_manager_->getPortfolio(buy_account);
            const Portfolio& sell_portfolio = risk_manager_->getPortfolio(sell_account);
            
            logger_->debug("Position updates - Buy account " + buy_account + 
                          " new position: " + std::to_string(buy_portfolio.getPosition(trade.symbol)) +
                          ", Sell account " + sell_account + 
                          " new position: " + std::to_string(sell_portfolio.getPosition(trade.symbol)),
                          "OrderBook::executeTrade");
        }
    }
}

}