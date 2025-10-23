#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <variant>
#include <functional>

namespace orderbook {
    // Strong type for Order IDs to prevent parameter confusion
    struct OrderId {
        uint64_t value;
        
        constexpr explicit OrderId(uint64_t id) : value(id) {}
        constexpr operator uint64_t() const { return value; }
        constexpr bool operator==(const OrderId& other) const { return value == other.value; }
        constexpr bool operator!=(const OrderId& other) const { return value != other.value; }
        constexpr bool operator<(const OrderId& other) const { return value < other.value; }
    };

    // Custom hash function for OrderId to use in unordered_map
    struct OrderIdHash {
        std::size_t operator()(const OrderId& id) const {
            return std::hash<uint64_t>{}(id.value);
        }
    };

    // Strong type for Trade IDs
    struct TradeId {
        uint64_t value;
        
        constexpr explicit TradeId(uint64_t id) : value(id) {}
        constexpr operator uint64_t() const { return value; }
        constexpr bool operator==(const TradeId& other) const { return value == other.value; }
        constexpr bool operator!=(const TradeId& other) const { return value != other.value; }
    };

    // Template-friendly type aliases
    using Price = double;
    using Quantity = uint64_t;
    using Timestamp = std::chrono::system_clock::time_point;
    using SequenceNumber = uint64_t;

    // Core enums
    enum class Side : uint8_t { Buy, Sell };
    enum class OrderType : uint8_t { Limit, Market };
    enum class TimeInForce : uint8_t { GTC, IOC, FOK };

    // Order status for tracking lifecycle
    enum class OrderStatus : uint8_t { 
        New, 
        PartiallyFilled, 
        Filled, 
        Cancelled, 
        Rejected 
    };

    // Risk validation results
    enum class RiskResult : uint8_t { Approved, Rejected };

    // Log levels
    enum class LogLevel : uint8_t { 
        DEBUG, 
        INFO, 
        WARN, 
        ERROR 
    };

    // Result template for error handling
    template<typename T>
    class Result {
    public:
        static Result success(T value) {
            return Result(std::move(value));
        }
        
        static Result error(const std::string& message) {
            return Result(message);
        }
        
        bool isSuccess() const {
            return std::holds_alternative<T>(data_);
        }
        
        bool isError() const {
            return std::holds_alternative<std::string>(data_);
        }
        
        const T& value() const {
            return std::get<T>(data_);
        }
        
        T& value() {
            return std::get<T>(data_);
        }
        
        const std::string& error() const {
            return std::get<std::string>(data_);
        }
        
        // Move semantics
        T&& moveValue() {
            return std::move(std::get<T>(data_));
        }
        
    private:
        explicit Result(T value) : data_(std::move(value)) {}
        explicit Result(const std::string& error) : data_(error) {}
        
        std::variant<T, std::string> data_;
    };

    // Specific result types for common operations
    using OrderResult = Result<OrderId>;
    using CancelResult = Result<bool>;
    using ModifyResult = Result<bool>;
    using ValidationResult = Result<bool>;

    // Forward declarations for interfaces
    class Order;
    class Trade;
    class Portfolio;
    class MarketData;
    class ExecutionReport;
    class BookUpdate;
}