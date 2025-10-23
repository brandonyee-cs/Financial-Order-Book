#pragma once
#include "Types.hpp"
#include <string>

namespace orderbook {

/**
 * @brief Represents a trading order with all necessary fields
 * Optimized for high-performance order book operations with cache-friendly layout
 * SIMD-ready with proper alignment for vectorized operations
 */
struct alignas(64) Order {  // 64-byte alignment for cache line optimization and SIMD
    // Core order identification and pricing (hot data)
    OrderId id;
    Price price;
    Quantity quantity;
    Quantity filled_quantity = 0;
    
    // Order characteristics
    Side side;
    OrderType type;
    TimeInForce tif = TimeInForce::GTC;
    OrderStatus status = OrderStatus::New;
    
    // Linked list pointers for price level management (hot data for matching)
    Order* next = nullptr;
    Order* prev = nullptr;
    
    // Timestamps and metadata (less frequently accessed)
    Timestamp timestamp;
    std::string symbol;
    std::string account;
    
    // Constructors
    Order(uint64_t id, Side side, OrderType type, Price price, Quantity quantity, std::string symbol)
        : id(OrderId(id)), price(price), quantity(quantity), side(side), type(type),
          symbol(std::move(symbol)), timestamp(std::chrono::system_clock::now()) {}
    
    Order(uint64_t id, Side side, OrderType type, TimeInForce tif, Price price, Quantity quantity, 
          std::string symbol, std::string account = "")
        : id(OrderId(id)), price(price), quantity(quantity), side(side), type(type), tif(tif),
          symbol(std::move(symbol)), account(std::move(account)), 
          timestamp(std::chrono::system_clock::now()) {}
    
    // Default constructor for object pooling
    Order() = default;
    
    // Move constructor and assignment for efficiency
    Order(Order&& other) noexcept = default;
    Order& operator=(Order&& other) noexcept = default;
    
    // Delete copy constructor to prevent accidental copying
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;
    
    // Utility methods
    bool isFullyFilled() const { return filled_quantity >= quantity; }
    Quantity remainingQuantity() const { return quantity - filled_quantity; }
    bool isBuy() const { return side == Side::Buy; }
    bool isSell() const { return side == Side::Sell; }
    bool isActive() const { return status == OrderStatus::New || status == OrderStatus::PartiallyFilled; }
    
    /**
     * @brief Fill order with specified quantity
     * @param fill_quantity Quantity to fill
     * @return true if order is now fully filled
     */
    bool fill(Quantity fill_quantity) {
        filled_quantity += fill_quantity;
        if (isFullyFilled()) {
            status = OrderStatus::Filled;
            return true;
        } else {
            status = OrderStatus::PartiallyFilled;
            return false;
        }
    }
    
    /**
     * @brief Cancel the order
     */
    void cancel() {
        status = OrderStatus::Cancelled;
    }
    
    /**
     * @brief Reject the order
     */
    void reject() {
        status = OrderStatus::Rejected;
    }
    
    /**
     * @brief Modify order price and/or quantity
     * @param new_price New price (0 to keep current)
     * @param new_quantity New quantity (0 to keep current)
     */
    void modify(Price new_price, Quantity new_quantity) {
        if (new_price > 0) {
            price = new_price;
        }
        if (new_quantity > 0) {
            quantity = new_quantity;
            // If new quantity is less than filled, adjust
            if (filled_quantity > quantity) {
                filled_quantity = quantity;
                status = OrderStatus::Filled;
            }
        }
    }
    
    /**
     * @brief Reset order for object pooling
     */
    void reset() {
        id = OrderId(0);
        price = 0.0;
        quantity = 0;
        filled_quantity = 0;
        side = Side::Buy;
        type = OrderType::Limit;
        tif = TimeInForce::GTC;
        status = OrderStatus::New;
        next = nullptr;
        prev = nullptr;
        symbol.clear();
        account.clear();
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * @brief Represents a trade execution
 * SIMD-aligned for vectorized processing
 */
struct alignas(32) Trade {  // 32-byte alignment for SIMD operations
    TradeId id;
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    std::string symbol;
    
    Trade(uint64_t trade_id, OrderId buy_id, OrderId sell_id, Price p, Quantity q, std::string sym)
        : id(TradeId(trade_id)), buy_order_id(buy_id), sell_order_id(sell_id), 
          price(p), quantity(q), symbol(std::move(sym)), 
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Portfolio tracking for risk management
 */
struct Portfolio {
    std::string account;
    std::unordered_map<std::string, int64_t> positions; // symbol -> position (signed)
    std::unordered_map<std::string, Price> avg_prices;   // symbol -> average price
    
    explicit Portfolio(std::string acc) : account(std::move(acc)) {}
    
    void updatePosition(const std::string& symbol, int64_t quantity_change, Price price) {
        positions[symbol] += quantity_change;
        // Simple average price calculation (could be enhanced)
        if (positions[symbol] != 0) {
            avg_prices[symbol] = price;
        }
    }
    
    int64_t getPosition(const std::string& symbol) const {
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second : 0;
    }
};

/**
 * @brief Price level with order queue management
 * Optimized for high-performance order book operations
 */
struct PriceLevel {
    Price price;
    Quantity total_quantity = 0;
    size_t order_count = 0;
    Order* head = nullptr;
    Order* tail = nullptr;
    
    explicit PriceLevel(Price p) : price(p) {}
    
    /**
     * @brief Add order to the end of the queue (FIFO)
     * @param order Order to add
     */
    void addOrder(Order* order) {
        if (!order) return;
        
        order->next = nullptr;
        order->prev = tail;
        
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
        
        total_quantity += order->remainingQuantity();
        ++order_count;
    }
    
    /**
     * @brief Remove order from the queue
     * @param order Order to remove
     */
    void removeOrder(Order* order) {
        if (!order) return;
        
        // Update quantity and count
        total_quantity -= order->remainingQuantity();
        --order_count;
        
        // Update linked list pointers
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }
        
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }
        
        order->next = nullptr;
        order->prev = nullptr;
    }
    
    /**
     * @brief Update quantity when order is partially filled
     * @param order Order that was filled
     * @param filled_quantity Quantity that was filled
     */
    void updateQuantity(Order* order, Quantity filled_quantity) {
        if (filled_quantity <= total_quantity) {
            total_quantity -= filled_quantity;
        }
        
        // Remove order if fully filled
        if (order->isFullyFilled()) {
            removeOrder(order);
        }
    }
    
    /**
     * @brief Check if price level is empty
     * @return true if no orders at this level
     */
    bool isEmpty() const {
        return order_count == 0 || head == nullptr;
    }
    
    /**
     * @brief Get first order in queue (for matching)
     * @return Pointer to first order or nullptr if empty
     */
    Order* getFirstOrder() const {
        return head;
    }
    
    /**
     * @brief Get total quantity at this price level
     * @return Total quantity
     */
    Quantity getTotalQuantity() const {
        return total_quantity;
    }
    
    /**
     * @brief Get number of orders at this price level
     * @return Order count
     */
    size_t getOrderCount() const {
        return order_count;
    }
};

/**
 * @brief Order location for fast lookup
 * Used for O(1) order cancellation and modification
 */
struct OrderLocation {
    Order* order;
    PriceLevel* price_level;
    Side side;
    
    OrderLocation(Order* o, PriceLevel* pl, Side s) : order(o), price_level(pl), side(s) {}
    
    /**
     * @brief Check if location is valid
     * @return true if order and price_level are not null
     */
    bool isValid() const {
        return order != nullptr && price_level != nullptr;
    }
};

}