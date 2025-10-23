#pragma once

namespace orderbook::fix {
    // FIX 4.4 Message Types
    constexpr char MSG_TYPE_HEARTBEAT = '0';
    constexpr char MSG_TYPE_TEST_REQUEST = '1';
    constexpr char MSG_TYPE_RESEND_REQUEST = '2';
    constexpr char MSG_TYPE_REJECT = '3';
    constexpr char MSG_TYPE_SEQUENCE_RESET = '4';
    constexpr char MSG_TYPE_LOGOUT = '5';
    constexpr char MSG_TYPE_LOGON = 'A';
    constexpr char MSG_TYPE_NEW_ORDER_SINGLE = 'D';
    constexpr char MSG_TYPE_EXECUTION_REPORT = '8';
    constexpr char MSG_TYPE_ORDER_CANCEL_REPLACE_REQUEST = 'G';
    constexpr char MSG_TYPE_ORDER_CANCEL_REQUEST = 'F';
    
    // Standard FIX Tags
    constexpr int TAG_BEGIN_STRING = 8;
    constexpr int TAG_BODY_LENGTH = 9;
    constexpr int TAG_CHECKSUM = 10;
    constexpr int TAG_CLORD_ID = 11;
    constexpr int TAG_MSG_SEQ_NUM = 34;
    constexpr int TAG_MSG_TYPE = 35;
    constexpr int TAG_ORDER_ID = 37;
    constexpr int TAG_ORDER_QTY = 38;
    constexpr int TAG_ORD_STATUS = 39;
    constexpr int TAG_ORD_TYPE = 40;
    constexpr int TAG_ORIG_CLORD_ID = 41;
    constexpr int TAG_PRICE = 44;
    constexpr int TAG_SENDER_COMP_ID = 49;
    constexpr int TAG_SENDING_TIME = 52;
    constexpr int TAG_SIDE = 54;
    constexpr int TAG_SYMBOL = 55;
    constexpr int TAG_TARGET_COMP_ID = 56;
    constexpr int TAG_TIME_IN_FORCE = 59;
    constexpr int TAG_TRANSACT_TIME = 60;
    constexpr int TAG_EXEC_ID = 17;
    constexpr int TAG_EXEC_TYPE = 150;
    constexpr int TAG_LEAVES_QTY = 151;
    constexpr int TAG_CUM_QTY = 14;
    constexpr int TAG_AVG_PX = 6;
    constexpr int TAG_LAST_QTY = 32;
    constexpr int TAG_LAST_PX = 31;
    constexpr int TAG_HEARTBT_INT = 108;
    constexpr int TAG_TEST_REQ_ID = 112;
    
    // Side values
    constexpr char SIDE_BUY = '1';
    constexpr char SIDE_SELL = '2';
    
    // Order Type values
    constexpr char ORD_TYPE_MARKET = '1';
    constexpr char ORD_TYPE_LIMIT = '2';
    
    // Time In Force values
    constexpr char TIF_DAY = '0';
    constexpr char TIF_GTC = '1';
    constexpr char TIF_IOC = '3';
    constexpr char TIF_FOK = '4';
    
    // Order Status values
    constexpr char ORD_STATUS_NEW = '0';
    constexpr char ORD_STATUS_PARTIALLY_FILLED = '1';
    constexpr char ORD_STATUS_FILLED = '2';
    constexpr char ORD_STATUS_CANCELLED = '4';
    constexpr char ORD_STATUS_REJECTED = '8';
    
    // Execution Type values
    constexpr char EXEC_TYPE_NEW = '0';
    constexpr char EXEC_TYPE_PARTIAL_FILL = '1';
    constexpr char EXEC_TYPE_FILL = '2';
    constexpr char EXEC_TYPE_CANCELLED = '4';
    constexpr char EXEC_TYPE_REJECTED = '8';
    
    // FIX Protocol constants
    constexpr char FIELD_DELIMITER = '\x01';  // SOH character
    constexpr const char* BEGIN_STRING_44 = "FIX.4.4";
    constexpr int HEARTBEAT_INTERVAL = 30;  // seconds
}