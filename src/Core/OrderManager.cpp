#include "orderbook/Core/OrderManager.hpp"
#include <sstream>
#include <limits>

namespace orderbook {

OrderManager::OrderManager(LoggerPtr logger)
    : next_order_id_(1), logger_(logger) {
    if (logger_) {
        logger_->info("OrderManager initialized", "OrderManager");
    }
}

OrderManager::~OrderManager() {
    if (logger_) {
        logger_->info("OrderManager destroyed with " + std::to_string(orders_.size()) + " orders", "OrderManager");
    }
}

OrderResult OrderManager::addOrder(std::unique_ptr<Order> order) {
    if (!order) {
        const std::string error = "Cannot add null order";
        if (logger_) logger_->error(error, "OrderManager::addOrder");
        return OrderResult::error(error);
    }
    
    // Validate order parameters
    auto validation = validateOrder(*order);
    if (!validation.isSuccess()) {
        if (logger_) logger_->error("Order validation failed: " + validation.error(), "OrderManager::addOrder");
        return OrderResult::error("Order validation failed: " + validation.error());
    }
    
    OrderId order_id = order->id;
    
    // Check for duplicate order ID
    if (orders_.find(order_id) != orders_.end()) {
        const std::string error = "Order ID " + std::to_string(order_id.value) + " already exists";
        if (logger_) logger_->error(error, "OrderManager::addOrder");
        return OrderResult::error(error);
    }
    
    // Log order addition
    logOrderEvent("ADD", *order);
    
    // Store the order
    orders_[order_id] = std::move(order);
    
    return OrderResult::success(order_id);
}

CancelResult OrderManager::cancelOrder(OrderId order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        const std::string error = "Order " + std::to_string(order_id.value) + " not found";
        logOrderError(error, order_id);
        return CancelResult::error(error);
    }
    
    Order* order = it->second.get();
    if (!order->isActive()) {
        const std::string error = "Order " + std::to_string(order_id.value) + " is not active";
        logOrderError(error, order_id);
        return CancelResult::error(error);
    }
    
    // Cancel the order
    order->cancel();
    logOrderEvent("CANCEL", *order);
    
    // Remove from location tracking
    order_locations_.erase(order_id);
    
    return CancelResult::success(true);
}

ModifyResult OrderManager::modifyOrder(OrderId order_id, Price new_price, Quantity new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        const std::string error = "Order " + std::to_string(order_id.value) + " not found";
        logOrderError(error, order_id);
        return ModifyResult::error(error);
    }
    
    Order* order = it->second.get();
    if (!order->isActive()) {
        const std::string error = "Order " + std::to_string(order_id.value) + " is not active";
        logOrderError(error, order_id);
        return ModifyResult::error(error);
    }
    
    // Validate new parameters
    if (new_price > 0 && !isValidPrice(new_price)) {
        const std::string error = "Invalid new price: " + std::to_string(new_price);
        logOrderError(error, order_id);
        return ModifyResult::error(error);
    }
    
    if (new_quantity > 0 && !isValidQuantity(new_quantity)) {
        const std::string error = "Invalid new quantity: " + std::to_string(new_quantity);
        logOrderError(error, order_id);
        return ModifyResult::error(error);
    }
    
    // Check if new quantity is less than already filled
    if (new_quantity > 0 && new_quantity < order->filled_quantity) {
        const std::string error = "New quantity " + std::to_string(new_quantity) + 
                                " is less than filled quantity " + std::to_string(order->filled_quantity);
        logOrderError(error, order_id);
        return ModifyResult::error(error);
    }
    
    // Store old values for logging
    Price old_price = order->price;
    Quantity old_quantity = order->quantity;
    
    // Modify the order
    order->modify(new_price, new_quantity);
    
    // Log modification
    std::stringstream ss;
    ss << "MODIFY price: " << old_price << "->" << order->price 
       << " quantity: " << old_quantity << "->" << order->quantity;
    if (logger_) {
        logger_->info(ss.str(), "OrderManager::modifyOrder OrderId=" + std::to_string(order_id.value));
    }
    
    return ModifyResult::success(true);
}

Order* OrderManager::getOrder(OrderId order_id) const {
    auto it = orders_.find(order_id);
    return (it != orders_.end()) ? it->second.get() : nullptr;
}

OrderLocation OrderManager::getOrderLocation(OrderId order_id) const {
    auto it = order_locations_.find(order_id);
    if (it != order_locations_.end()) {
        return it->second;
    }
    // Return invalid location
    return OrderLocation(nullptr, nullptr, Side::Buy);
}

void OrderManager::setOrderLocation(OrderId order_id, const OrderLocation& location) {
    if (location.isValid()) {
        order_locations_[order_id] = location;
    }
}

std::unique_ptr<Order> OrderManager::removeOrder(OrderId order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return nullptr;
    }
    
    std::unique_ptr<Order> order = std::move(it->second);
    orders_.erase(it);
    order_locations_.erase(order_id);
    
    logOrderEvent("REMOVE", *order);
    
    return order;
}

void OrderManager::updateOrderStatus(OrderId order_id, OrderStatus status) {
    auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        it->second->status = status;
        
        if (logger_) {
            std::string status_str;
            switch (status) {
                case OrderStatus::New: status_str = "NEW"; break;
                case OrderStatus::PartiallyFilled: status_str = "PARTIALLY_FILLED"; break;
                case OrderStatus::Filled: status_str = "FILLED"; break;
                case OrderStatus::Cancelled: status_str = "CANCELLED"; break;
                case OrderStatus::Rejected: status_str = "REJECTED"; break;
            }
            logger_->info("Order status updated to " + status_str, 
                         "OrderManager::updateOrderStatus OrderId=" + std::to_string(order_id.value));
        }
    }
}

bool OrderManager::fillOrder(OrderId order_id, Quantity fill_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second.get();
    bool fully_filled = order->fill(fill_quantity);
    
    if (logger_) {
        std::stringstream ss;
        ss << "FILL quantity: " << fill_quantity 
           << " remaining: " << order->remainingQuantity()
           << " status: " << (fully_filled ? "FILLED" : "PARTIALLY_FILLED");
        logger_->info(ss.str(), "OrderManager::fillOrder OrderId=" + std::to_string(order_id.value));
    }
    
    return fully_filled;
}

ValidationResult OrderManager::validateOrder(const Order& order) const {
    // Validate order ID
    if (order.id.value == 0) {
        return ValidationResult::error("Order ID cannot be zero");
    }
    
    // Validate price
    if (!isValidPrice(order.price)) {
        return ValidationResult::error("Invalid price: " + std::to_string(order.price));
    }
    
    // Validate quantity
    if (!isValidQuantity(order.quantity)) {
        return ValidationResult::error("Invalid quantity: " + std::to_string(order.quantity));
    }
    
    // Validate symbol
    if (!isValidSymbol(order.symbol)) {
        return ValidationResult::error("Invalid symbol: " + order.symbol);
    }
    
    // Validate filled quantity
    if (order.filled_quantity > order.quantity) {
        return ValidationResult::error("Filled quantity cannot exceed total quantity");
    }
    
    return ValidationResult::success(true);
}

size_t OrderManager::getActiveOrderCount() const {
    size_t count = 0;
    for (const auto& pair : orders_) {
        if (pair.second->isActive()) {
            ++count;
        }
    }
    return count;
}

OrderId OrderManager::getNextOrderId() {
    return OrderId(next_order_id_.fetch_add(1));
}

void OrderManager::clear() {
    orders_.clear();
    order_locations_.clear();
    next_order_id_.store(1);
    
    if (logger_) {
        logger_->info("OrderManager cleared", "OrderManager::clear");
    }
}

// Private helper methods

bool OrderManager::isValidPrice(Price price) const {
    return price > 0.0 && price < std::numeric_limits<Price>::max() && std::isfinite(price);
}

bool OrderManager::isValidQuantity(Quantity quantity) const {
    return quantity > 0 && quantity < std::numeric_limits<Quantity>::max();
}

bool OrderManager::isValidSymbol(const std::string& symbol) const {
    return !symbol.empty() && symbol.length() <= 16; // Reasonable symbol length limit
}

void OrderManager::logOrderEvent(const std::string& event, const Order& order) const {
    if (!logger_) return;
    
    std::stringstream ss;
    ss << event << " OrderId=" << order.id.value 
       << " Symbol=" << order.symbol
       << " Side=" << (order.isBuy() ? "BUY" : "SELL")
       << " Price=" << order.price
       << " Quantity=" << order.quantity
       << " Account=" << order.account;
    
    logger_->info(ss.str(), "OrderManager");
}

void OrderManager::logOrderError(const std::string& error, OrderId order_id) const {
    if (logger_) {
        logger_->error(error, "OrderManager OrderId=" + std::to_string(order_id.value));
    }
}

} // namespace orderbook