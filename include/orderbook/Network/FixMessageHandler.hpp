#pragma once
#include "../Core/Order.hpp"
#include "../Core/Types.hpp"
#include "../Core/OrderManager.hpp"
#include "../Risk/RiskManager.hpp"
#include "FixParser.hpp"
#include "FixSession.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace orderbook {

/**
 * @brief FIX Message Handler
 * Bridges FIX protocol messages with the order management system
 * Handles order lifecycle management and execution reporting
 */
class FixMessageHandler {
public:
    /**
     * @brief Constructor
     * @param orderManager Order management system
     * @param riskManager Risk management system
     */
    FixMessageHandler(std::shared_ptr<OrderManager> orderManager,
                     std::shared_ptr<RiskManager> riskManager);
    
    /**
     * @brief Set the FIX session for sending responses
     * @param session FIX session
     */
    void setFixSession(std::shared_ptr<FixSession> session);
    
    /**
     * @brief Handle New Order Single message
     * @param newOrder Parsed new order data
     */
    void handleNewOrderSingle(const FixMessageParser::NewOrderSingle& newOrder);
    
    /**
     * @brief Handle Order Cancel Replace Request message
     * @param cancelReplace Parsed cancel replace request data
     */
    void handleOrderCancelReplaceRequest(const FixMessageParser::OrderCancelReplaceRequest& cancelReplace);
    
    /**
     * @brief Handle Order Cancel Request message
     * @param cancelRequest Parsed cancel request data
     */
    void handleOrderCancelRequest(const FixMessageParser::OrderCancelRequest& cancelRequest);
    
    /**
     * @brief Handle trade execution (called by OrderManager)
     * @param trade Trade execution data
     */
    void handleTradeExecution(const Trade& trade);
    
    /**
     * @brief Handle order status change (called by OrderManager)
     * @param order Order with updated status
     */
    void handleOrderStatusChange(const Order& order);

private:
    /**
     * @brief Generate unique execution ID
     * @return Unique execution ID
     */
    std::string generateExecutionId();
    
    /**
     * @brief Convert internal Order to FIX NewOrderSingle
     * @param newOrder FIX new order data
     * @return Internal Order object
     */
    std::unique_ptr<Order> convertToInternalOrder(const FixMessageParser::NewOrderSingle& newOrder);
    
    /**
     * @brief Send execution report for order
     * @param order Order to report
     * @param execType Execution type
     * @param lastQty Last quantity filled (0 for non-fill reports)
     * @param lastPx Last price filled (0 for non-fill reports)
     */
    void sendExecutionReport(const Order& order, char execType, 
                           Quantity lastQty = 0, Price lastPx = 0.0);
    
    /**
     * @brief Send execution report for trade
     * @param trade Trade execution
     * @param order Order that was filled
     */
    void sendTradeExecutionReport(const Trade& trade, const Order& order);
    
    /**
     * @brief Send rejection execution report
     * @param clOrdId Client order ID
     * @param symbol Symbol
     * @param side Side
     * @param reason Rejection reason
     */
    void sendRejectionReport(const std::string& clOrdId, const std::string& symbol, 
                           Side side, const std::string& reason);
    
    /**
     * @brief Find order by client order ID
     * @param clOrdId Client order ID
     * @return Order pointer or nullptr if not found
     */
    Order* findOrderByClOrdId(const std::string& clOrdId);
    
    /**
     * @brief Convert OrderStatus enum to FIX order status character
     * @param status OrderStatus enum value
     * @return FIX order status character
     */
    char orderStatusToFixChar(OrderStatus status);

private:
    // Core components
    std::shared_ptr<OrderManager> orderManager_;
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<FixSession> fixSession_;
    
    // Order tracking
    std::unordered_map<std::string, OrderId> clOrdIdToOrderId_;  // Client Order ID -> Internal Order ID
    std::unordered_map<OrderId, std::string, OrderIdHash> orderIdToClOrdId_;  // Internal Order ID -> Client Order ID
    
    // Execution ID generation
    std::atomic<uint64_t> executionIdCounter_{1};
    
    // Statistics
    std::atomic<size_t> ordersProcessed_{0};
    std::atomic<size_t> ordersRejected_{0};
    std::atomic<size_t> tradesReported_{0};
};

}