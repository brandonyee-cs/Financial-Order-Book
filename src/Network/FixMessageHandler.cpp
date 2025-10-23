#include "orderbook/Network/FixMessageHandler.hpp"
#include "orderbook/Network/FixConstants.hpp"
#include <sstream>
#include <iomanip>

namespace orderbook {

using namespace fix;

FixMessageHandler::FixMessageHandler(std::shared_ptr<OrderManager> orderManager,
                                   std::shared_ptr<RiskManager> riskManager)
    : orderManager_(std::move(orderManager)), riskManager_(std::move(riskManager)) {
}

void FixMessageHandler::setFixSession(std::shared_ptr<FixSession> session) {
    fixSession_ = std::move(session);
}

void FixMessageHandler::handleNewOrderSingle(const FixMessageParser::NewOrderSingle& newOrder) {
    ++ordersProcessed_;
    
    if (!newOrder.isValid) {
        sendRejectionReport(newOrder.clOrdId, newOrder.symbol, newOrder.side, 
                          "Invalid order format: " + newOrder.errorMessage);
        ++ordersRejected_;
        return;
    }
    
    // Convert to internal order
    auto order = convertToInternalOrder(newOrder);
    if (!order) {
        sendRejectionReport(newOrder.clOrdId, newOrder.symbol, newOrder.side, 
                          "Failed to create internal order");
        ++ordersRejected_;
        return;
    }
    
    // Risk validation
    if (riskManager_) {
        // Create a simple portfolio for risk checking (in production, would be more sophisticated)
        Portfolio portfolio(newOrder.account.empty() ? "DEFAULT" : newOrder.account);
        auto riskCheck = riskManager_->validateOrder(*order, portfolio);
        
        if (riskCheck.result == RiskResult::Rejected) {
            sendRejectionReport(newOrder.clOrdId, newOrder.symbol, newOrder.side, 
                              "Risk check failed: " + riskCheck.reason);
            ++ordersRejected_;
            return;
        }
    }
    
    // Store client order ID mapping
    clOrdIdToOrderId_[newOrder.clOrdId] = order->id;
    orderIdToClOrdId_[order->id] = newOrder.clOrdId;
    
    // Send acknowledgment (New execution report)
    sendExecutionReport(*order, EXEC_TYPE_NEW);
    
    // Submit order to order manager
    auto result = orderManager_->addOrder(std::move(order));
    
    if (result.isError()) {
        // Order was rejected by order manager
        sendRejectionReport(newOrder.clOrdId, newOrder.symbol, newOrder.side, 
                          "Order rejected: " + result.error());
        
        // Clean up mappings
        clOrdIdToOrderId_.erase(newOrder.clOrdId);
        orderIdToClOrdId_.erase(OrderId(result.value()));
        ++ordersRejected_;
    }
}

void FixMessageHandler::handleOrderCancelReplaceRequest(const FixMessageParser::OrderCancelReplaceRequest& cancelReplace) {
    if (!cancelReplace.isValid) {
        // Send reject for invalid message
        if (fixSession_) {
            fixSession_->sendReject(0, "Invalid cancel replace request: " + cancelReplace.errorMessage);
        }
        return;
    }
    
    // Find original order
    auto it = clOrdIdToOrderId_.find(cancelReplace.origClOrdId);
    if (it == clOrdIdToOrderId_.end()) {
        sendRejectionReport(cancelReplace.clOrdId, cancelReplace.symbol, cancelReplace.side, 
                          "Original order not found: " + cancelReplace.origClOrdId);
        return;
    }
    
    OrderId originalOrderId = it->second;
    
    // Modify the order
    auto result = orderManager_->modifyOrder(originalOrderId, cancelReplace.price, cancelReplace.quantity);
    
    if (result.isSuccess()) {
        // Update client order ID mapping
        clOrdIdToOrderId_.erase(cancelReplace.origClOrdId);
        clOrdIdToOrderId_[cancelReplace.clOrdId] = originalOrderId;
        orderIdToClOrdId_[originalOrderId] = cancelReplace.clOrdId;
        
        // Find the modified order and send execution report
        Order* modifiedOrder = findOrderByClOrdId(cancelReplace.clOrdId);
        if (modifiedOrder) {
            sendExecutionReport(*modifiedOrder, EXEC_TYPE_NEW); // Modified order treated as new
        }
    } else {
        sendRejectionReport(cancelReplace.clOrdId, cancelReplace.symbol, cancelReplace.side, 
                          "Modify failed: " + result.error());
    }
}

void FixMessageHandler::handleOrderCancelRequest(const FixMessageParser::OrderCancelRequest& cancelRequest) {
    if (!cancelRequest.isValid) {
        // Send reject for invalid message
        if (fixSession_) {
            fixSession_->sendReject(0, "Invalid cancel request: " + cancelRequest.errorMessage);
        }
        return;
    }
    
    // Find original order
    auto it = clOrdIdToOrderId_.find(cancelRequest.origClOrdId);
    if (it == clOrdIdToOrderId_.end()) {
        sendRejectionReport(cancelRequest.clOrdId, cancelRequest.symbol, cancelRequest.side, 
                          "Original order not found: " + cancelRequest.origClOrdId);
        return;
    }
    
    OrderId originalOrderId = it->second;
    
    // Cancel the order
    auto result = orderManager_->cancelOrder(originalOrderId);
    
    if (result.isSuccess()) {
        // Update client order ID mapping for the cancel confirmation
        clOrdIdToOrderId_[cancelRequest.clOrdId] = originalOrderId;
        orderIdToClOrdId_[originalOrderId] = cancelRequest.clOrdId;
        
        // Find the cancelled order and send execution report
        Order* cancelledOrder = findOrderByClOrdId(cancelRequest.clOrdId);
        if (cancelledOrder) {
            sendExecutionReport(*cancelledOrder, EXEC_TYPE_CANCELLED);
        }
    } else {
        sendRejectionReport(cancelRequest.clOrdId, cancelRequest.symbol, cancelRequest.side, 
                          "Cancel failed: " + result.error());
    }
}

void FixMessageHandler::handleTradeExecution(const Trade& trade) {
    ++tradesReported_;
    
    // Send execution reports for both buy and sell orders
    Order* buyOrder = nullptr;
    Order* sellOrder = nullptr;
    
    // Find orders by their IDs
    auto buyIt = orderIdToClOrdId_.find(trade.buy_order_id);
    auto sellIt = orderIdToClOrdId_.find(trade.sell_order_id);
    
    if (buyIt != orderIdToClOrdId_.end()) {
        buyOrder = findOrderByClOrdId(buyIt->second);
    }
    
    if (sellIt != orderIdToClOrdId_.end()) {
        sellOrder = findOrderByClOrdId(sellIt->second);
    }
    
    // Send trade execution reports
    if (buyOrder) {
        sendTradeExecutionReport(trade, *buyOrder);
    }
    
    if (sellOrder) {
        sendTradeExecutionReport(trade, *sellOrder);
    }
}

void FixMessageHandler::handleOrderStatusChange(const Order& order) {
    // Send execution report for status changes (e.g., cancelled, rejected)
    char execType = EXEC_TYPE_NEW;
    
    switch (order.status) {
        case OrderStatus::Cancelled:
            execType = EXEC_TYPE_CANCELLED;
            break;
        case OrderStatus::Rejected:
            execType = EXEC_TYPE_REJECTED;
            break;
        case OrderStatus::Filled:
            execType = EXEC_TYPE_FILL;
            break;
        case OrderStatus::PartiallyFilled:
            execType = EXEC_TYPE_PARTIAL_FILL;
            break;
        default:
            execType = EXEC_TYPE_NEW;
            break;
    }
    
    sendExecutionReport(order, execType);
}

std::string FixMessageHandler::generateExecutionId() {
    std::ostringstream oss;
    oss << "EXEC" << std::setfill('0') << std::setw(10) << executionIdCounter_++;
    return oss.str();
}

std::unique_ptr<Order> FixMessageHandler::convertToInternalOrder(const FixMessageParser::NewOrderSingle& newOrder) {
    try {
        // Generate internal order ID (in production, would use a proper ID generator)
        static std::atomic<uint64_t> orderIdCounter{1};
        uint64_t orderId = orderIdCounter++;
        
        auto order = std::make_unique<Order>(
            orderId,
            newOrder.side,
            newOrder.orderType,
            newOrder.timeInForce,
            newOrder.price,
            newOrder.quantity,
            newOrder.symbol,
            newOrder.account
        );
        
        return order;
        
    } catch (const std::exception&) {
        return nullptr;
    }
}

void FixMessageHandler::sendExecutionReport(const Order& order, char execType, 
                                          Quantity lastQty, Price lastPx) {
    if (!fixSession_ || !fixSession_->isLoggedIn()) {
        return;
    }
    
    // Find client order ID
    auto it = orderIdToClOrdId_.find(order.id);
    if (it == orderIdToClOrdId_.end()) {
        return; // No client order ID found
    }
    
    FixMessageParser::ExecutionReport execReport;
    execReport.orderId = std::to_string(order.id.value);
    execReport.clOrdId = it->second;
    execReport.execId = generateExecutionId();
    execReport.execType = execType;
    execReport.ordStatus = orderStatusToFixChar(order.status);
    execReport.symbol = order.symbol;
    execReport.side = order.side;
    execReport.orderQty = order.quantity;
    execReport.price = order.price;
    execReport.lastQty = lastQty;
    execReport.lastPx = lastPx;
    execReport.leavesQty = order.remainingQuantity();
    execReport.cumQty = order.filled_quantity;
    execReport.avgPx = order.price; // Simplified - in production would calculate actual average
    execReport.transactTime = std::chrono::system_clock::now();
    
    fixSession_->sendExecutionReport(execReport);
}

void FixMessageHandler::sendTradeExecutionReport(const Trade& trade, const Order& order) {
    char execType = order.isFullyFilled() ? EXEC_TYPE_FILL : EXEC_TYPE_PARTIAL_FILL;
    sendExecutionReport(order, execType, trade.quantity, trade.price);
}

void FixMessageHandler::sendRejectionReport(const std::string& clOrdId, const std::string& symbol, 
                                          Side side, const std::string& reason) {
    if (!fixSession_ || !fixSession_->isLoggedIn()) {
        return;
    }
    
    FixMessageParser::ExecutionReport execReport;
    execReport.orderId = "0"; // No internal order ID for rejected orders
    execReport.clOrdId = clOrdId;
    execReport.execId = generateExecutionId();
    execReport.execType = EXEC_TYPE_REJECTED;
    execReport.ordStatus = ORD_STATUS_REJECTED;
    execReport.symbol = symbol;
    execReport.side = side;
    execReport.orderQty = 0;
    execReport.price = 0.0;
    execReport.lastQty = 0;
    execReport.lastPx = 0.0;
    execReport.leavesQty = 0;
    execReport.cumQty = 0;
    execReport.avgPx = 0.0;
    execReport.transactTime = std::chrono::system_clock::now();
    
    fixSession_->sendExecutionReport(execReport);
}

Order* FixMessageHandler::findOrderByClOrdId(const std::string& clOrdId) {
    auto it = clOrdIdToOrderId_.find(clOrdId);
    if (it == clOrdIdToOrderId_.end()) {
        return nullptr;
    }
    
    // Get order from order manager (simplified - in production would have better order lookup)
    return orderManager_->getOrder(it->second);
}

// Helper function to convert OrderStatus to FIX character
char FixMessageHandler::orderStatusToFixChar(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return ORD_STATUS_NEW;
        case OrderStatus::PartiallyFilled: return ORD_STATUS_PARTIALLY_FILLED;
        case OrderStatus::Filled: return ORD_STATUS_FILLED;
        case OrderStatus::Cancelled: return ORD_STATUS_CANCELLED;
        case OrderStatus::Rejected: return ORD_STATUS_REJECTED;
        default: return ORD_STATUS_NEW;
    }
}

}