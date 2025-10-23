#pragma once
#include "../Core/Order.hpp"
#include "../Core/Types.hpp"
#include "FixConstants.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>

namespace orderbook {

/**
 * @brief FIX 4.4 Message Parser and Generator
 * Handles parsing of incoming FIX messages and generation of outgoing messages
 */
class FixMessageParser {
public:
    /**
     * @brief Represents a parsed FIX message
     */
    struct FixMessage {
        char msgType;
        std::unordered_map<int, std::string> fields;
        std::string rawMessage;
        bool isValid = false;
        std::string errorMessage;
        
        FixMessage() = default;
        explicit FixMessage(const std::string& raw) : rawMessage(raw) {}
        
        std::string getField(int tag) const {
            auto it = fields.find(tag);
            return it != fields.end() ? it->second : "";
        }
        
        bool hasField(int tag) const {
            return fields.find(tag) != fields.end();
        }
    };
    
    /**
     * @brief New Order Single message data
     */
    struct NewOrderSingle {
        std::string clOrdId;
        std::string symbol;
        Side side;
        OrderType orderType;
        TimeInForce timeInForce;
        Price price;
        Quantity quantity;
        std::string account;
        std::chrono::system_clock::time_point transactTime;
        
        bool isValid = false;
        std::string errorMessage;
    };
    
    /**
     * @brief Order Cancel Replace Request message data
     */
    struct OrderCancelReplaceRequest {
        std::string clOrdId;
        std::string origClOrdId;
        std::string symbol;
        Side side;
        OrderType orderType;
        TimeInForce timeInForce;
        Price price;
        Quantity quantity;
        std::string account;
        std::chrono::system_clock::time_point transactTime;
        
        bool isValid = false;
        std::string errorMessage;
    };
    
    /**
     * @brief Order Cancel Request message data
     */
    struct OrderCancelRequest {
        std::string clOrdId;
        std::string origClOrdId;
        std::string symbol;
        Side side;
        std::chrono::system_clock::time_point transactTime;
        
        bool isValid = false;
        std::string errorMessage;
    };
    
    /**
     * @brief Execution Report message data
     */
    struct ExecutionReport {
        std::string orderId;
        std::string clOrdId;
        std::string execId;
        char execType;
        char ordStatus;
        std::string symbol;
        Side side;
        Quantity orderQty;
        Price price;
        Quantity lastQty = 0;
        Price lastPx = 0.0;
        Quantity leavesQty;
        Quantity cumQty;
        Price avgPx;
        std::chrono::system_clock::time_point transactTime;
        
        ExecutionReport() = default;
    };

public:
    FixMessageParser() = default;
    
    /**
     * @brief Parse a raw FIX message
     * @param rawMessage Raw FIX message string
     * @return Parsed FixMessage
     */
    FixMessage parseMessage(const std::string& rawMessage);
    
    /**
     * @brief Parse New Order Single message
     * @param fixMsg Parsed FIX message
     * @return NewOrderSingle data structure
     */
    NewOrderSingle parseNewOrderSingle(const FixMessage& fixMsg);
    
    /**
     * @brief Parse Order Cancel Replace Request message
     * @param fixMsg Parsed FIX message
     * @return OrderCancelReplaceRequest data structure
     */
    OrderCancelReplaceRequest parseOrderCancelReplaceRequest(const FixMessage& fixMsg);
    
    /**
     * @brief Parse Order Cancel Request message
     * @param fixMsg Parsed FIX message
     * @return OrderCancelRequest data structure
     */
    OrderCancelRequest parseOrderCancelRequest(const FixMessage& fixMsg);
    
    /**
     * @brief Generate Execution Report message
     * @param execReport Execution report data
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @return FIX message string
     */
    std::string generateExecutionReport(const ExecutionReport& execReport,
                                      const std::string& senderCompId,
                                      const std::string& targetCompId,
                                      SequenceNumber msgSeqNum);
    
    /**
     * @brief Generate Heartbeat message
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @param testReqId Optional test request ID
     * @return FIX message string
     */
    std::string generateHeartbeat(const std::string& senderCompId,
                                const std::string& targetCompId,
                                SequenceNumber msgSeqNum,
                                const std::string& testReqId = "");
    
    /**
     * @brief Generate Logon message
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @param heartBtInt Heartbeat interval
     * @return FIX message string
     */
    std::string generateLogon(const std::string& senderCompId,
                            const std::string& targetCompId,
                            SequenceNumber msgSeqNum,
                            int heartBtInt = fix::HEARTBEAT_INTERVAL);
    
    /**
     * @brief Generate Logout message
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @param text Optional logout text
     * @return FIX message string
     */
    std::string generateLogout(const std::string& senderCompId,
                             const std::string& targetCompId,
                             SequenceNumber msgSeqNum,
                             const std::string& text = "");
    
    /**
     * @brief Generate Reject message
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @param refSeqNum Referenced sequence number
     * @param text Rejection reason
     * @return FIX message string
     */
    std::string generateReject(const std::string& senderCompId,
                             const std::string& targetCompId,
                             SequenceNumber msgSeqNum,
                             SequenceNumber refSeqNum,
                             const std::string& text);

private:
    /**
     * @brief Calculate FIX message checksum
     * @param message Message without checksum field
     * @return Checksum value
     */
    uint8_t calculateChecksum(const std::string& message);
    
    /**
     * @brief Format timestamp for FIX message
     * @param timestamp Timestamp to format
     * @return Formatted timestamp string
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp);
    
    /**
     * @brief Parse timestamp from FIX message
     * @param timestampStr Timestamp string from FIX message
     * @return Parsed timestamp
     */
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr);
    
    /**
     * @brief Convert Side enum to FIX side character
     * @param side Side enum value
     * @return FIX side character
     */
    char sideToFixChar(Side side);
    
    /**
     * @brief Convert FIX side character to Side enum
     * @param fixSide FIX side character
     * @return Side enum value
     */
    Side fixCharToSide(char fixSide);
    
    /**
     * @brief Convert OrderType enum to FIX order type character
     * @param orderType OrderType enum value
     * @return FIX order type character
     */
    char orderTypeToFixChar(OrderType orderType);
    
    /**
     * @brief Convert FIX order type character to OrderType enum
     * @param fixOrderType FIX order type character
     * @return OrderType enum value
     */
    OrderType fixCharToOrderType(char fixOrderType);
    
    /**
     * @brief Convert TimeInForce enum to FIX TIF character
     * @param tif TimeInForce enum value
     * @return FIX TIF character
     */
    char tifToFixChar(TimeInForce tif);
    
    /**
     * @brief Convert FIX TIF character to TimeInForce enum
     * @param fixTif FIX TIF character
     * @return TimeInForce enum value
     */
    TimeInForce fixCharToTif(char fixTif);
    
    /**
     * @brief Convert OrderStatus enum to FIX order status character
     * @param status OrderStatus enum value
     * @return FIX order status character
     */
    char orderStatusToFixChar(OrderStatus status);
    
    /**
     * @brief Build FIX message with header and trailer
     * @param msgType Message type
     * @param body Message body (without header/trailer)
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     * @param msgSeqNum Message sequence number
     * @return Complete FIX message
     */
    std::string buildFixMessage(char msgType,
                              const std::string& body,
                              const std::string& senderCompId,
                              const std::string& targetCompId,
                              SequenceNumber msgSeqNum);
};

}