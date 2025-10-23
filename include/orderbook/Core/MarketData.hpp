#pragma once
#include "Types.hpp"
#include <vector>
#include <optional>

namespace orderbook {

/**
 * @brief Book update event for market data publishing
 */
struct BookUpdate {
    enum class Type { Add, Remove, Modify };
    
    Type type;
    Side side;
    Price price;
    Quantity quantity;
    size_t order_count;
    SequenceNumber sequence;
    Timestamp timestamp;
    
    BookUpdate(Type t, Side s, Price p, Quantity q, size_t count, SequenceNumber seq)
        : type(t), side(s), price(p), quantity(q), order_count(count), sequence(seq),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Execution report for FIX protocol
 */
struct ExecutionReport {
    enum class ExecType { New, PartialFill, Fill, Cancelled, Rejected };
    
    OrderId order_id;
    ExecType exec_type;
    OrderStatus order_status;
    Side side;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Quantity leaves_quantity;
    std::optional<TradeId> trade_id;
    std::string symbol;
    std::string account;
    std::string text; // For rejection reasons
    Timestamp timestamp;
    
    ExecutionReport(OrderId id, ExecType exec, OrderStatus status, Side s, 
                   Price p, Quantity q, Quantity filled, std::string sym, std::string acc)
        : order_id(id), exec_type(exec), order_status(status), side(s), price(p), 
          quantity(q), filled_quantity(filled), leaves_quantity(q - filled),
          symbol(std::move(sym)), account(std::move(acc)),
          timestamp(std::chrono::system_clock::now()) {}
};

}