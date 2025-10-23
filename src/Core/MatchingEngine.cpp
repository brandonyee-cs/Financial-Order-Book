#include "orderbook/Core/MatchingEngine.hpp"
#include "orderbook/Core/Order.hpp"
#include "orderbook/Core/MarketData.hpp"
#include "orderbook/Utilities/PerformanceTimer.hpp"
#include <chrono>
#include <algorithm>

namespace orderbook {

MatchingEngine::MatchingEngine(LoggerPtr logger)
    : logger_(std::move(logger)) {
    if (logger_) {
        logger_->info("MatchingEngine initialized", "MatchingEngine::ctor");
    }
}

MatchResult MatchingEngine::matchOrder(Order& incoming_order, 
                                      std::vector<PriceLevel>& opposite_side_levels) {
    PERF_TIMER("MatchingEngine::matchOrder", logger_);
    
    MatchResult result;
    
    if (incoming_order.isBuy()) {
        result = matchBuyOrder(incoming_order, opposite_side_levels);
    } else {
        result = matchSellOrder(incoming_order, opposite_side_levels);
    }
    
    // Log performance metrics
    if (logger_ && result.hasTrades()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();
        
        logger_->logPerformance("order_matching", latency_ns, {
            {"order_id", std::to_string(incoming_order.id.value)},
            {"trades_generated", std::to_string(result.getTradeCount())},
            {"quantity_filled", std::to_string(result.total_filled_quantity)},
            {"fully_filled", result.fully_filled ? "true" : "false"}
        });
    }
    
    return result;
}

MatchResult MatchingEngine::matchBuyOrder(Order& buy_order, 
                                         std::vector<PriceLevel>& ask_levels) {
    MatchResult result;
    
    if (logger_) {
        logger_->debug("Matching buy order " + std::to_string(buy_order.id.value) + 
                      " price=" + std::to_string(buy_order.price) + 
                      " qty=" + std::to_string(buy_order.remainingQuantity()),
                      "MatchingEngine::matchBuyOrder");
    }
    
    // Sort ask levels by price (ascending) for best price first
    std::sort(ask_levels.begin(), ask_levels.end(), 
              [](const PriceLevel& a, const PriceLevel& b) {
                  return a.price < b.price;
              });
    
    // Match against ask levels while buy price >= ask price
    for (auto& ask_level : ask_levels) {
        if (ask_level.isEmpty()) continue;
        
        // Check if we can match (buy price >= ask price)
        if (!canMatch(buy_order.price, ask_level.price)) {
            break; // No more matches possible at higher prices
        }
        
        // Match against this price level
        Quantity filled_from_level = matchAgainstPriceLevel(buy_order, ask_level, result.trades);
        result.total_filled_quantity += filled_from_level;
        
        // Check if buy order is fully filled
        if (buy_order.isFullyFilled()) {
            result.fully_filled = true;
            break;
        }
    }
    
    // Generate execution reports based on fill status
    if (result.total_filled_quantity > 0) {
        // Generate execution report for the incoming order
        if (buy_order.isFullyFilled()) {
            result.execution_reports.push_back(generateExecutionReport(buy_order, 
                result.hasTrades() ? std::optional<TradeId>(result.trades.back().id) : std::nullopt));
        } else {
            result.execution_reports.push_back(generateExecutionReport(buy_order, 
                result.hasTrades() ? std::optional<TradeId>(result.trades.back().id) : std::nullopt));
        }
    }
    
    // If order has remaining quantity, include it in result
    if (!buy_order.isFullyFilled()) {
        result.remaining_order = std::move(buy_order);
        result.fully_filled = false;
    } else {
        result.fully_filled = true;
    }
    
    if (logger_ && result.hasTrades()) {
        logger_->info("Buy order matching completed: " + 
                     std::to_string(result.getTradeCount()) + " trades, " +
                     std::to_string(result.total_filled_quantity) + " quantity filled, " +
                     std::to_string(result.getExecutionReportCount()) + " execution reports",
                     "MatchingEngine::matchBuyOrder");
    }
    
    return result;
}

MatchResult MatchingEngine::matchSellOrder(Order& sell_order, 
                                          std::vector<PriceLevel>& bid_levels) {
    MatchResult result;
    
    if (logger_) {
        logger_->debug("Matching sell order " + std::to_string(sell_order.id.value) + 
                      " price=" + std::to_string(sell_order.price) + 
                      " qty=" + std::to_string(sell_order.remainingQuantity()),
                      "MatchingEngine::matchSellOrder");
    }
    
    // Sort bid levels by price (descending) for best price first
    std::sort(bid_levels.begin(), bid_levels.end(), 
              [](const PriceLevel& a, const PriceLevel& b) {
                  return a.price > b.price;
              });
    
    // Match against bid levels while bid price >= sell price
    for (auto& bid_level : bid_levels) {
        if (bid_level.isEmpty()) continue;
        
        // Check if we can match (bid price >= sell price)
        if (!canMatch(bid_level.price, sell_order.price)) {
            break; // No more matches possible at lower prices
        }
        
        // Match against this price level
        Quantity filled_from_level = matchAgainstPriceLevel(sell_order, bid_level, result.trades);
        result.total_filled_quantity += filled_from_level;
        
        // Check if sell order is fully filled
        if (sell_order.isFullyFilled()) {
            result.fully_filled = true;
            break;
        }
    }
    
    // Generate execution reports based on fill status
    if (result.total_filled_quantity > 0) {
        // Generate execution report for the incoming order
        if (sell_order.isFullyFilled()) {
            result.execution_reports.push_back(generateExecutionReport(sell_order, 
                result.hasTrades() ? std::optional<TradeId>(result.trades.back().id) : std::nullopt));
        } else {
            result.execution_reports.push_back(generateExecutionReport(sell_order, 
                result.hasTrades() ? std::optional<TradeId>(result.trades.back().id) : std::nullopt));
        }
    }
    
    // If order has remaining quantity, include it in result
    if (!sell_order.isFullyFilled()) {
        result.remaining_order = std::move(sell_order);
        result.fully_filled = false;
    } else {
        result.fully_filled = true;
    }
    
    if (logger_ && result.hasTrades()) {
        logger_->info("Sell order matching completed: " + 
                     std::to_string(result.getTradeCount()) + " trades, " +
                     std::to_string(result.total_filled_quantity) + " quantity filled, " +
                     std::to_string(result.getExecutionReportCount()) + " execution reports",
                     "MatchingEngine::matchSellOrder");
    }
    
    return result;
}

Quantity MatchingEngine::matchAgainstPriceLevel(Order& incoming_order,
                                               PriceLevel& price_level,
                                               std::vector<Trade>& trades) {
    Quantity total_filled = 0;
    Order* current_order = price_level.getFirstOrder();
    
    while (current_order && !incoming_order.isFullyFilled()) {
        // Determine trade quantity (minimum of remaining quantities)
        Quantity trade_quantity = std::min(incoming_order.remainingQuantity(),
                                          current_order->remainingQuantity());
        
        if (trade_quantity == 0) break;
        
        // Validate trade before execution
        if (!validateTrade(incoming_order, *current_order, trade_quantity)) {
            if (logger_) {
                logger_->error("Trade validation failed for orders " + 
                              std::to_string(incoming_order.id.value) + " and " +
                              std::to_string(current_order->id.value),
                              "MatchingEngine::matchAgainstPriceLevel");
            }
            break;
        }
        
        // Execute the trade
        Trade trade = executeTrade(incoming_order, *current_order, 
                                  price_level.price, trade_quantity);
        trades.push_back(trade);
        total_filled += trade_quantity;
        
        // Update price level quantity
        price_level.updateQuantity(current_order, trade_quantity);
        
        // Move to next order if current order is fully filled
        if (current_order->isFullyFilled()) {
            current_order = current_order->next;
        }
    }
    
    return total_filled;
}

Trade MatchingEngine::executeTrade(Order& aggressive_order, 
                                  Order& passive_order,
                                  Price trade_price, 
                                  Quantity trade_quantity) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Fill both orders
    aggressive_order.fill(trade_quantity);
    passive_order.fill(trade_quantity);
    
    // Create trade record
    TradeId trade_id = getNextTradeId();
    
    // Determine buy/sell order IDs based on sides
    OrderId buy_order_id = aggressive_order.isBuy() ? aggressive_order.id : passive_order.id;
    OrderId sell_order_id = aggressive_order.isSell() ? aggressive_order.id : passive_order.id;
    
    Trade trade(trade_id.value, buy_order_id, sell_order_id, 
               trade_price, trade_quantity, aggressive_order.symbol);
    
    // Log trade execution
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time).count();
    
    logTradeExecution(trade, latency_ns);
    
    return trade;
}

TradeId MatchingEngine::getNextTradeId() {
    return TradeId(trade_counter_.fetch_add(1, std::memory_order_relaxed));
}

void MatchingEngine::logTradeExecution(const Trade& trade, uint64_t latency_ns) {
    if (!logger_) return;
    
    logger_->info("Trade executed: ID=" + std::to_string(trade.id.value) + 
                 " Buy=" + std::to_string(trade.buy_order_id.value) + 
                 " Sell=" + std::to_string(trade.sell_order_id.value) + 
                 " Price=" + std::to_string(trade.price) + 
                 " Qty=" + std::to_string(trade.quantity) + 
                 " Symbol=" + trade.symbol,
                 "MatchingEngine::executeTrade");
    
    logger_->logPerformance("trade_execution", latency_ns, {
        {"trade_id", std::to_string(trade.id.value)},
        {"price", std::to_string(trade.price)},
        {"quantity", std::to_string(trade.quantity)},
        {"symbol", trade.symbol}
    });
}

bool MatchingEngine::validateTrade(const Order& aggressive_order,
                                  const Order& passive_order,
                                  Quantity quantity) {
    // Basic validation checks
    if (quantity == 0) return false;
    if (quantity > aggressive_order.remainingQuantity()) return false;
    if (quantity > passive_order.remainingQuantity()) return false;
    if (aggressive_order.symbol != passive_order.symbol) return false;
    if (aggressive_order.side == passive_order.side) return false;
    
    // Price validation
    if (aggressive_order.isBuy() && passive_order.isSell()) {
        return aggressive_order.price >= passive_order.price;
    } else if (aggressive_order.isSell() && passive_order.isBuy()) {
        return passive_order.price >= aggressive_order.price;
    }
    
    return false;
}

}Ex
ecutionReport MatchingEngine::generateExecutionReport(const Order& order, 
                                                       std::optional<TradeId> trade_id) {
    ExecutionReport::ExecType exec_type;
    
    // Determine execution type based on order status
    switch (order.status) {
        case OrderStatus::New:
            exec_type = ExecutionReport::ExecType::New;
            break;
        case OrderStatus::PartiallyFilled:
            exec_type = ExecutionReport::ExecType::PartialFill;
            break;
        case OrderStatus::Filled:
            exec_type = ExecutionReport::ExecType::Fill;
            break;
        case OrderStatus::Cancelled:
            exec_type = ExecutionReport::ExecType::Cancelled;
            break;
        case OrderStatus::Rejected:
            exec_type = ExecutionReport::ExecType::Rejected;
            break;
        default:
            exec_type = ExecutionReport::ExecType::New;
            break;
    }
    
    ExecutionReport report(order.id, exec_type, order.status, order.side,
                          order.price, order.quantity, order.filled_quantity,
                          order.symbol, order.account);
    
    // Set trade ID if provided
    if (trade_id.has_value()) {
        report.trade_id = trade_id.value();
    }
    
    return report;
}

ExecutionReport MatchingEngine::generateNewOrderReport(const Order& order) {
    ExecutionReport report(order.id, ExecutionReport::ExecType::New, OrderStatus::New,
                          order.side, order.price, order.quantity, 0,
                          order.symbol, order.account);
    return report;
}

ExecutionReport MatchingEngine::generateRejectionReport(const Order& order, const std::string& reason) {
    ExecutionReport report(order.id, ExecutionReport::ExecType::Rejected, OrderStatus::Rejected,
                          order.side, order.price, order.quantity, 0,
                          order.symbol, order.account);
    report.text = reason;
    return report;
}Ex
ecutionReport MatchingEngine::handlePartialFill(Order& order, Quantity filled_quantity) {
    // Update order with partial fill
    order.fill(filled_quantity);
    
    // Generate execution report for partial fill
    ExecutionReport report(order.id, ExecutionReport::ExecType::PartialFill, 
                          OrderStatus::PartiallyFilled, order.side,
                          order.price, order.quantity, order.filled_quantity,
                          order.symbol, order.account);
    
    return report;
}