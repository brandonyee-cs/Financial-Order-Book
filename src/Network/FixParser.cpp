#include "orderbook/Network/FixParser.hpp"
#include "orderbook/Network/FixConstants.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace orderbook {

using namespace fix;

FixMessageParser::FixMessage FixMessageParser::parseMessage(const std::string& rawMessage) {
    FixMessage msg(rawMessage);
    
    if (rawMessage.empty()) {
        msg.errorMessage = "Empty message";
        return msg;
    }
    
    // Split message into fields using SOH delimiter
    std::vector<std::string> fields;
    std::string current_field;
    
    for (char c : rawMessage) {
        if (c == FIELD_DELIMITER) {
            if (!current_field.empty()) {
                fields.push_back(current_field);
                current_field.clear();
            }
        } else {
            current_field += c;
        }
    }
    
    // Add last field if it doesn't end with SOH
    if (!current_field.empty()) {
        fields.push_back(current_field);
    }
    
    // Parse each field (tag=value)
    for (const auto& field : fields) {
        size_t eq_pos = field.find('=');
        if (eq_pos == std::string::npos) {
            msg.errorMessage = "Invalid field format: " + field;
            return msg;
        }
        
        try {
            int tag = std::stoi(field.substr(0, eq_pos));
            std::string value = field.substr(eq_pos + 1);
            msg.fields[tag] = value;
        } catch (const std::exception& e) {
            msg.errorMessage = "Failed to parse field: " + field + " - " + e.what();
            return msg;
        }
    }
    
    // Validate required header fields
    if (!msg.hasField(TAG_BEGIN_STRING) || !msg.hasField(TAG_BODY_LENGTH) || 
        !msg.hasField(TAG_MSG_TYPE) || !msg.hasField(TAG_CHECKSUM)) {
        msg.errorMessage = "Missing required header fields";
        return msg;
    }
    
    // Validate FIX version
    if (msg.getField(TAG_BEGIN_STRING) != BEGIN_STRING_44) {
        msg.errorMessage = "Unsupported FIX version: " + msg.getField(TAG_BEGIN_STRING);
        return msg;
    }
    
    // Get message type
    std::string msgTypeStr = msg.getField(TAG_MSG_TYPE);
    if (msgTypeStr.empty()) {
        msg.errorMessage = "Missing message type";
        return msg;
    }
    msg.msgType = msgTypeStr[0];
    
    // Validate checksum (simplified - in production would calculate actual checksum)
    std::string checksumStr = msg.getField(TAG_CHECKSUM);
    if (checksumStr.length() != 3) {
        msg.errorMessage = "Invalid checksum format";
        return msg;
    }
    
    msg.isValid = true;
    return msg;
}

FixMessageParser::NewOrderSingle FixMessageParser::parseNewOrderSingle(const FixMessage& fixMsg) {
    NewOrderSingle nos;
    
    if (!fixMsg.isValid) {
        nos.errorMessage = "Invalid FIX message: " + fixMsg.errorMessage;
        return nos;
    }
    
    if (fixMsg.msgType != MSG_TYPE_NEW_ORDER_SINGLE) {
        nos.errorMessage = "Not a New Order Single message";
        return nos;
    }
    
    try {
        // Required fields
        nos.clOrdId = fixMsg.getField(TAG_CLORD_ID);
        nos.symbol = fixMsg.getField(TAG_SYMBOL);
        
        if (nos.clOrdId.empty() || nos.symbol.empty()) {
            nos.errorMessage = "Missing required fields (ClOrdID or Symbol)";
            return nos;
        }
        
        // Side
        std::string sideStr = fixMsg.getField(TAG_SIDE);
        if (sideStr.empty()) {
            nos.errorMessage = "Missing Side field";
            return nos;
        }
        nos.side = fixCharToSide(sideStr[0]);
        
        // Order Type
        std::string ordTypeStr = fixMsg.getField(TAG_ORD_TYPE);
        if (ordTypeStr.empty()) {
            nos.errorMessage = "Missing OrdType field";
            return nos;
        }
        nos.orderType = fixCharToOrderType(ordTypeStr[0]);
        
        // Quantity
        std::string qtyStr = fixMsg.getField(TAG_ORDER_QTY);
        if (qtyStr.empty()) {
            nos.errorMessage = "Missing OrderQty field";
            return nos;
        }
        nos.quantity = std::stoull(qtyStr);
        
        // Price (required for limit orders)
        std::string priceStr = fixMsg.getField(TAG_PRICE);
        if (nos.orderType == OrderType::Limit) {
            if (priceStr.empty()) {
                nos.errorMessage = "Missing Price field for limit order";
                return nos;
            }
            nos.price = std::stod(priceStr);
        } else {
            nos.price = 0.0; // Market order
        }
        
        // Time In Force (optional, default to GTC)
        std::string tifStr = fixMsg.getField(TAG_TIME_IN_FORCE);
        nos.timeInForce = tifStr.empty() ? TimeInForce::GTC : fixCharToTif(tifStr[0]);
        
        // Account (optional)
        nos.account = fixMsg.getField(TAG_CLORD_ID); // Using ClOrdID as account for simplicity
        
        // Transaction Time
        std::string transactTimeStr = fixMsg.getField(TAG_TRANSACT_TIME);
        nos.transactTime = transactTimeStr.empty() ? 
            std::chrono::system_clock::now() : parseTimestamp(transactTimeStr);
        
        nos.isValid = true;
        
    } catch (const std::exception& e) {
        nos.errorMessage = "Failed to parse New Order Single: " + std::string(e.what());
    }
    
    return nos;
}

FixMessageParser::OrderCancelReplaceRequest FixMessageParser::parseOrderCancelReplaceRequest(const FixMessage& fixMsg) {
    OrderCancelReplaceRequest ocrr;
    
    if (!fixMsg.isValid) {
        ocrr.errorMessage = "Invalid FIX message: " + fixMsg.errorMessage;
        return ocrr;
    }
    
    if (fixMsg.msgType != MSG_TYPE_ORDER_CANCEL_REPLACE_REQUEST) {
        ocrr.errorMessage = "Not an Order Cancel Replace Request message";
        return ocrr;
    }
    
    try {
        // Required fields
        ocrr.clOrdId = fixMsg.getField(TAG_CLORD_ID);
        ocrr.origClOrdId = fixMsg.getField(TAG_ORIG_CLORD_ID);
        ocrr.symbol = fixMsg.getField(TAG_SYMBOL);
        
        if (ocrr.clOrdId.empty() || ocrr.origClOrdId.empty() || ocrr.symbol.empty()) {
            ocrr.errorMessage = "Missing required fields";
            return ocrr;
        }
        
        // Side
        std::string sideStr = fixMsg.getField(TAG_SIDE);
        if (!sideStr.empty()) {
            ocrr.side = fixCharToSide(sideStr[0]);
        }
        
        // Order Type
        std::string ordTypeStr = fixMsg.getField(TAG_ORD_TYPE);
        if (!ordTypeStr.empty()) {
            ocrr.orderType = fixCharToOrderType(ordTypeStr[0]);
        }
        
        // Quantity
        std::string qtyStr = fixMsg.getField(TAG_ORDER_QTY);
        if (!qtyStr.empty()) {
            ocrr.quantity = std::stoull(qtyStr);
        }
        
        // Price
        std::string priceStr = fixMsg.getField(TAG_PRICE);
        if (!priceStr.empty()) {
            ocrr.price = std::stod(priceStr);
        }
        
        // Time In Force
        std::string tifStr = fixMsg.getField(TAG_TIME_IN_FORCE);
        ocrr.timeInForce = tifStr.empty() ? TimeInForce::GTC : fixCharToTif(tifStr[0]);
        
        // Account
        ocrr.account = fixMsg.getField(TAG_CLORD_ID);
        
        // Transaction Time
        std::string transactTimeStr = fixMsg.getField(TAG_TRANSACT_TIME);
        ocrr.transactTime = transactTimeStr.empty() ? 
            std::chrono::system_clock::now() : parseTimestamp(transactTimeStr);
        
        ocrr.isValid = true;
        
    } catch (const std::exception& e) {
        ocrr.errorMessage = "Failed to parse Order Cancel Replace Request: " + std::string(e.what());
    }
    
    return ocrr;
}

FixMessageParser::OrderCancelRequest FixMessageParser::parseOrderCancelRequest(const FixMessage& fixMsg) {
    OrderCancelRequest ocr;
    
    if (!fixMsg.isValid) {
        ocr.errorMessage = "Invalid FIX message: " + fixMsg.errorMessage;
        return ocr;
    }
    
    if (fixMsg.msgType != MSG_TYPE_ORDER_CANCEL_REQUEST) {
        ocr.errorMessage = "Not an Order Cancel Request message";
        return ocr;
    }
    
    try {
        // Required fields
        ocr.clOrdId = fixMsg.getField(TAG_CLORD_ID);
        ocr.origClOrdId = fixMsg.getField(TAG_ORIG_CLORD_ID);
        ocr.symbol = fixMsg.getField(TAG_SYMBOL);
        
        if (ocr.clOrdId.empty() || ocr.origClOrdId.empty() || ocr.symbol.empty()) {
            ocr.errorMessage = "Missing required fields";
            return ocr;
        }
        
        // Side
        std::string sideStr = fixMsg.getField(TAG_SIDE);
        if (!sideStr.empty()) {
            ocr.side = fixCharToSide(sideStr[0]);
        }
        
        // Transaction Time
        std::string transactTimeStr = fixMsg.getField(TAG_TRANSACT_TIME);
        ocr.transactTime = transactTimeStr.empty() ? 
            std::chrono::system_clock::now() : parseTimestamp(transactTimeStr);
        
        ocr.isValid = true;
        
    } catch (const std::exception& e) {
        ocr.errorMessage = "Failed to parse Order Cancel Request: " + std::string(e.what());
    }
    
    return ocr;
}

std::string FixMessageParser::generateExecutionReport(const ExecutionReport& execReport,
                                                    const std::string& senderCompId,
                                                    const std::string& targetCompId,
                                                    SequenceNumber msgSeqNum) {
    std::ostringstream body;
    
    // Required fields for Execution Report
    body << TAG_ORDER_ID << "=" << execReport.orderId << FIELD_DELIMITER;
    body << TAG_CLORD_ID << "=" << execReport.clOrdId << FIELD_DELIMITER;
    body << TAG_EXEC_ID << "=" << execReport.execId << FIELD_DELIMITER;
    body << TAG_EXEC_TYPE << "=" << execReport.execType << FIELD_DELIMITER;
    body << TAG_ORD_STATUS << "=" << execReport.ordStatus << FIELD_DELIMITER;
    body << TAG_SYMBOL << "=" << execReport.symbol << FIELD_DELIMITER;
    body << TAG_SIDE << "=" << sideToFixChar(execReport.side) << FIELD_DELIMITER;
    body << TAG_ORDER_QTY << "=" << execReport.orderQty << FIELD_DELIMITER;
    body << TAG_PRICE << "=" << std::fixed << std::setprecision(2) << execReport.price << FIELD_DELIMITER;
    body << TAG_LEAVES_QTY << "=" << execReport.leavesQty << FIELD_DELIMITER;
    body << TAG_CUM_QTY << "=" << execReport.cumQty << FIELD_DELIMITER;
    body << TAG_AVG_PX << "=" << std::fixed << std::setprecision(2) << execReport.avgPx << FIELD_DELIMITER;
    body << TAG_TRANSACT_TIME << "=" << formatTimestamp(execReport.transactTime) << FIELD_DELIMITER;
    
    // Optional fields for fills
    if (execReport.lastQty > 0) {
        body << TAG_LAST_QTY << "=" << execReport.lastQty << FIELD_DELIMITER;
        body << TAG_LAST_PX << "=" << std::fixed << std::setprecision(2) << execReport.lastPx << FIELD_DELIMITER;
    }
    
    return buildFixMessage(MSG_TYPE_EXECUTION_REPORT, body.str(), senderCompId, targetCompId, msgSeqNum);
}

std::string FixMessageParser::generateHeartbeat(const std::string& senderCompId,
                                               const std::string& targetCompId,
                                               SequenceNumber msgSeqNum,
                                               const std::string& testReqId) {
    std::ostringstream body;
    
    if (!testReqId.empty()) {
        body << TAG_TEST_REQ_ID << "=" << testReqId << FIELD_DELIMITER;
    }
    
    return buildFixMessage(MSG_TYPE_HEARTBEAT, body.str(), senderCompId, targetCompId, msgSeqNum);
}

std::string FixMessageParser::generateLogon(const std::string& senderCompId,
                                           const std::string& targetCompId,
                                           SequenceNumber msgSeqNum,
                                           int heartBtInt) {
    std::ostringstream body;
    
    body << TAG_HEARTBT_INT << "=" << heartBtInt << FIELD_DELIMITER;
    
    return buildFixMessage(MSG_TYPE_LOGON, body.str(), senderCompId, targetCompId, msgSeqNum);
}

std::string FixMessageParser::generateLogout(const std::string& senderCompId,
                                            const std::string& targetCompId,
                                            SequenceNumber msgSeqNum,
                                            const std::string& text) {
    std::ostringstream body;
    
    if (!text.empty()) {
        body << "58=" << text << FIELD_DELIMITER; // Text field
    }
    
    return buildFixMessage(MSG_TYPE_LOGOUT, body.str(), senderCompId, targetCompId, msgSeqNum);
}

std::string FixMessageParser::generateReject(const std::string& senderCompId,
                                            const std::string& targetCompId,
                                            SequenceNumber msgSeqNum,
                                            SequenceNumber refSeqNum,
                                            const std::string& text) {
    std::ostringstream body;
    
    body << "45=" << refSeqNum << FIELD_DELIMITER; // RefSeqNum
    body << "58=" << text << FIELD_DELIMITER;      // Text
    
    return buildFixMessage(MSG_TYPE_REJECT, body.str(), senderCompId, targetCompId, msgSeqNum);
}

// Private helper methods

uint8_t FixMessageParser::calculateChecksum(const std::string& message) {
    uint32_t sum = 0;
    for (char c : message) {
        sum += static_cast<uint8_t>(c);
    }
    return static_cast<uint8_t>(sum % 256);
}

std::string FixMessageParser::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::chrono::system_clock::time_point FixMessageParser::parseTimestamp(const std::string& timestampStr) {
    // Simple timestamp parsing - in production would be more robust
    return std::chrono::system_clock::now();
}

char FixMessageParser::sideToFixChar(Side side) {
    return side == Side::Buy ? SIDE_BUY : SIDE_SELL;
}

Side FixMessageParser::fixCharToSide(char fixSide) {
    return fixSide == SIDE_BUY ? Side::Buy : Side::Sell;
}

char FixMessageParser::orderTypeToFixChar(OrderType orderType) {
    return orderType == OrderType::Market ? ORD_TYPE_MARKET : ORD_TYPE_LIMIT;
}

OrderType FixMessageParser::fixCharToOrderType(char fixOrderType) {
    return fixOrderType == ORD_TYPE_MARKET ? OrderType::Market : OrderType::Limit;
}

char FixMessageParser::tifToFixChar(TimeInForce tif) {
    switch (tif) {
        case TimeInForce::IOC: return TIF_IOC;
        case TimeInForce::FOK: return TIF_FOK;
        default: return TIF_GTC;
    }
}

TimeInForce FixMessageParser::fixCharToTif(char fixTif) {
    switch (fixTif) {
        case TIF_IOC: return TimeInForce::IOC;
        case TIF_FOK: return TimeInForce::FOK;
        default: return TimeInForce::GTC;
    }
}

char FixMessageParser::orderStatusToFixChar(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return ORD_STATUS_NEW;
        case OrderStatus::PartiallyFilled: return ORD_STATUS_PARTIALLY_FILLED;
        case OrderStatus::Filled: return ORD_STATUS_FILLED;
        case OrderStatus::Cancelled: return ORD_STATUS_CANCELLED;
        case OrderStatus::Rejected: return ORD_STATUS_REJECTED;
        default: return ORD_STATUS_NEW;
    }
}

std::string FixMessageParser::buildFixMessage(char msgType,
                                             const std::string& body,
                                             const std::string& senderCompId,
                                             const std::string& targetCompId,
                                             SequenceNumber msgSeqNum) {
    std::ostringstream header;
    std::ostringstream trailer;
    
    // Build header (without BeginString and BodyLength yet)
    header << TAG_MSG_TYPE << "=" << msgType << FIELD_DELIMITER;
    header << TAG_SENDER_COMP_ID << "=" << senderCompId << FIELD_DELIMITER;
    header << TAG_TARGET_COMP_ID << "=" << targetCompId << FIELD_DELIMITER;
    header << TAG_MSG_SEQ_NUM << "=" << msgSeqNum << FIELD_DELIMITER;
    header << TAG_SENDING_TIME << "=" << formatTimestamp(std::chrono::system_clock::now()) << FIELD_DELIMITER;
    
    // Calculate body length (header + body)
    std::string headerStr = header.str();
    int bodyLength = static_cast<int>(headerStr.length() + body.length());
    
    // Build complete message without checksum
    std::ostringstream message;
    message << TAG_BEGIN_STRING << "=" << BEGIN_STRING_44 << FIELD_DELIMITER;
    message << TAG_BODY_LENGTH << "=" << bodyLength << FIELD_DELIMITER;
    message << headerStr;
    message << body;
    
    // Calculate and add checksum
    uint8_t checksum = calculateChecksum(message.str());
    message << TAG_CHECKSUM << "=" << std::setfill('0') << std::setw(3) << static_cast<int>(checksum) << FIELD_DELIMITER;
    
    return message.str();
}

}