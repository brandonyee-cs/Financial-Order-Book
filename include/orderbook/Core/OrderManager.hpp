#pragma once
#include "Types.hpp"
#include "Order.hpp"
#include "Interfaces.hpp"
#include <unordered_map>
#include <memory>
#include <atomic>

namespace orderbook {

/**
 * @brief Manages order lifecycle with O(1) lookup and validation
 * Provides centralized order management with state tracking and validation
 */
class OrderManager {
public:
    /**
     * @brief Constructor
     * @param logger Logger instance for order lifecycle events
     */
    explicit OrderManager(LoggerPtr logger = nullptr);
    
    /**
     * @brief Destructor
     */
    ~OrderManager();
    
    /**
     * @brief Add a new order to the system
     * @param order Order to add
     * @return OrderResult with order ID on success or error message
     */
    OrderResult addOrder(std::unique_ptr<Order> order);
    
    /**
     * @brief Cancel an existing order
     * @param order_id ID of order to cancel
     * @return CancelResult indicating success/failure
     */
    CancelResult cancelOrder(OrderId order_id);
    
    /**
     * @brief Modify an existing order
     * @param order_id ID of order to modify
     * @param new_price New price (0 to keep current)
     * @param new_quantity New quantity (0 to keep current)
     * @return ModifyResult indicating success/failure
     */
    ModifyResult modifyOrder(OrderId order_id, Price new_price, Quantity new_quantity);
    
    /**
     * @brief Get order by ID
     * @param order_id Order ID to lookup
     * @return Pointer to order or nullptr if not found
     */
    Order* getOrder(OrderId order_id) const;
    
    /**
     * @brief Get order location for fast book operations
     * @param order_id Order ID to lookup
     * @return OrderLocation or invalid location if not found
     */
    OrderLocation getOrderLocation(OrderId order_id) const;
    
    /**
     * @brief Set order location after adding to price level
     * @param order_id Order ID
     * @param location Order location in book
     */
    void setOrderLocation(OrderId order_id, const OrderLocation& location);
    
    /**
     * @brief Remove order from management (after full fill or cancel)
     * @param order_id Order ID to remove
     * @return Unique pointer to removed order for cleanup
     */
    std::unique_ptr<Order> removeOrder(OrderId order_id);
    
    /**
     * @brief Update order status
     * @param order_id Order ID
     * @param status New status
     */
    void updateOrderStatus(OrderId order_id, OrderStatus status);
    
    /**
     * @brief Fill order with specified quantity
     * @param order_id Order ID
     * @param fill_quantity Quantity filled
     * @return true if order is now fully filled
     */
    bool fillOrder(OrderId order_id, Quantity fill_quantity);
    
    /**
     * @brief Validate order parameters
     * @param order Order to validate
     * @return ValidationResult with success/error
     */
    ValidationResult validateOrder(const Order& order) const;
    
    /**
     * @brief Get total number of active orders
     * @return Number of orders being managed
     */
    size_t getActiveOrderCount() const;
    
    /**
     * @brief Get next available order ID
     * @return Unique order ID
     */
    OrderId getNextOrderId();
    
    /**
     * @brief Clear all orders (for testing/reset)
     */
    void clear();

private:
    // Order storage with O(1) lookup
    std::unordered_map<OrderId, std::unique_ptr<Order>, OrderIdHash> orders_;
    
    // Order location tracking for fast book operations
    std::unordered_map<OrderId, OrderLocation, OrderIdHash> order_locations_;
    
    // Atomic counter for order ID generation
    std::atomic<uint64_t> next_order_id_;
    
    // Logger for order lifecycle events
    LoggerPtr logger_;
    
    // Internal validation methods
    bool isValidPrice(Price price) const;
    bool isValidQuantity(Quantity quantity) const;
    bool isValidSymbol(const std::string& symbol) const;
    
    // Logging helpers
    void logOrderEvent(const std::string& event, const Order& order) const;
    void logOrderError(const std::string& error, OrderId order_id) const;
};

} // namespace orderbook