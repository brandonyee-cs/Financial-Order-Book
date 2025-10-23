#pragma once
#include "../Core/Order.hpp"
#include "../Core/Types.hpp"
#include "../Core/Interfaces.hpp"
#include "FixParser.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <chrono>
#include <queue>
#include <mutex>
#include <atomic>

namespace orderbook {

class Config;

/**
 * @brief FIX 4.4 Session Management
 * Handles async TCP connection, message parsing, sequence numbers, and heartbeats
 */
class FixSession : public std::enable_shared_from_this<FixSession> {
public:
    /**
     * @brief Session state enumeration
     */
    enum class SessionState {
        Disconnected,
        Connecting,
        LogonSent,
        LoggedIn,
        LogoutSent,
        Disconnecting
    };
    
    /**
     * @brief Message handlers
     */
    using NewOrderHandler = std::function<void(const FixMessageParser::NewOrderSingle&)>;
    using CancelReplaceHandler = std::function<void(const FixMessageParser::OrderCancelReplaceRequest&)>;
    using CancelHandler = std::function<void(const FixMessageParser::OrderCancelRequest&)>;
    using SessionEventHandler = std::function<void(SessionState, const std::string&)>;
    
    /**
     * @brief Constructor for server-side session (accepting connection)
     * @param socket Connected TCP socket
     * @param io_context IO context for async operations
     * @param logger Logger instance for session logging
     */
    FixSession(boost::asio::ip::tcp::socket socket, boost::asio::io_context& io_context, LoggerPtr logger = nullptr);
    
    /**
     * @brief Constructor for client-side session (initiating connection)
     * @param io_context IO context for async operations
     * @param logger Logger instance for session logging
     */
    explicit FixSession(boost::asio::io_context& io_context, LoggerPtr logger = nullptr);
    
    /**
     * @brief Constructor with configuration
     * @param io_context IO context for async operations
     * @param config Configuration object
     * @param logger Logger instance for session logging
     */
    FixSession(boost::asio::io_context& io_context, std::shared_ptr<Config> config, LoggerPtr logger = nullptr);
    
    /**
     * @brief Destructor
     */
    ~FixSession();
    
    /**
     * @brief Start the session (for server-side)
     */
    void start();
    
    /**
     * @brief Connect to remote endpoint (for client-side)
     * @param host Remote host
     * @param port Remote port
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     */
    void connect(const std::string& host, uint16_t port,
                const std::string& senderCompId, const std::string& targetCompId);
    
    /**
     * @brief Send logon message
     * @param heartBtInt Heartbeat interval in seconds
     */
    void sendLogon(int heartBtInt = fix::HEARTBEAT_INTERVAL);
    
    /**
     * @brief Send logout message
     * @param text Optional logout text
     */
    void sendLogout(const std::string& text = "");
    
    /**
     * @brief Send execution report
     * @param execReport Execution report data
     */
    void sendExecutionReport(const FixMessageParser::ExecutionReport& execReport);
    
    /**
     * @brief Send heartbeat message
     * @param testReqId Optional test request ID
     */
    void sendHeartbeat(const std::string& testReqId = "");
    
    /**
     * @brief Send reject message
     * @param refSeqNum Referenced sequence number
     * @param text Rejection reason
     */
    void sendReject(SequenceNumber refSeqNum, const std::string& text);
    
    /**
     * @brief Set message handlers
     */
    void setNewOrderHandler(NewOrderHandler handler) { newOrderHandler_ = std::move(handler); }
    void setCancelReplaceHandler(CancelReplaceHandler handler) { cancelReplaceHandler_ = std::move(handler); }
    void setCancelHandler(CancelHandler handler) { cancelHandler_ = std::move(handler); }
    void setSessionEventHandler(SessionEventHandler handler) { sessionEventHandler_ = std::move(handler); }
    
    /**
     * @brief Set session identifiers
     * @param senderCompId Sender component ID
     * @param targetCompId Target component ID
     */
    void setSessionIds(const std::string& senderCompId, const std::string& targetCompId);
    
    /**
     * @brief Get current session state
     * @return Current session state
     */
    SessionState getState() const { return state_; }
    
    /**
     * @brief Check if session is logged in
     * @return true if logged in
     */
    bool isLoggedIn() const { return state_ == SessionState::LoggedIn; }
    
    /**
     * @brief Get session statistics
     */
    struct SessionStats {
        SequenceNumber incomingSeqNum;
        SequenceNumber outgoingSeqNum;
        size_t messagesReceived;
        size_t messagesSent;
        size_t heartbeatsSent;
        size_t heartbeatsReceived;
        std::chrono::system_clock::time_point lastHeartbeat;
        std::chrono::system_clock::time_point sessionStartTime;
    };
    
    SessionStats getStats() const;
    
    /**
     * @brief Close the session
     */
    void close();
    
    /**
     * @brief Load configuration settings
     * @param config Configuration object
     */
    void loadConfiguration(std::shared_ptr<Config> config);

private:
    /**
     * @brief Start reading messages
     */
    void startRead();
    
    /**
     * @brief Read message header to determine length
     */
    void readMessage();
    
    /**
     * @brief Process received message
     * @param message Raw message string
     */
    void processMessage(const std::string& message);
    
    /**
     * @brief Handle different message types
     */
    void handleLogon(const FixMessageParser::FixMessage& msg);
    void handleLogout(const FixMessageParser::FixMessage& msg);
    void handleHeartbeat(const FixMessageParser::FixMessage& msg);
    void handleTestRequest(const FixMessageParser::FixMessage& msg);
    void handleNewOrderSingle(const FixMessageParser::FixMessage& msg);
    void handleOrderCancelReplaceRequest(const FixMessageParser::FixMessage& msg);
    void handleOrderCancelRequest(const FixMessageParser::FixMessage& msg);
    void handleReject(const FixMessageParser::FixMessage& msg);
    
    /**
     * @brief Send message with proper sequencing
     * @param message FIX message to send
     */
    void sendMessage(const std::string& message);
    
    /**
     * @brief Write message to socket
     * @param message Message to write
     */
    void writeMessage(const std::string& message);
    
    /**
     * @brief Perform actual write operation
     */
    void doWrite();
    
    /**
     * @brief Start heartbeat timer
     */
    void startHeartbeatTimer();
    
    /**
     * @brief Handle heartbeat timeout
     */
    void onHeartbeatTimeout(const boost::system::error_code& error);
    
    /**
     * @brief Start test request timer
     */
    void startTestRequestTimer();
    
    /**
     * @brief Handle test request timeout
     */
    void onTestRequestTimeout(const boost::system::error_code& error);
    
    /**
     * @brief Validate sequence number
     * @param expectedSeqNum Expected sequence number
     * @param receivedSeqNum Received sequence number
     * @return true if valid
     */
    bool validateSequenceNumber(SequenceNumber expectedSeqNum, SequenceNumber receivedSeqNum);
    
    /**
     * @brief Update session state
     * @param newState New session state
     * @param reason Reason for state change
     */
    void updateState(SessionState newState, const std::string& reason = "");
    
    /**
     * @brief Handle connection error
     * @param error Error code
     * @param context Error context
     */
    void handleError(const boost::system::error_code& error, const std::string& context);
    
    /**
     * @brief Generate next outgoing sequence number
     * @return Next sequence number
     */
    SequenceNumber getNextOutgoingSeqNum() { return ++outgoingSeqNum_; }
    
    /**
     * @brief Generate next incoming sequence number
     * @return Next sequence number
     */
    SequenceNumber getNextIncomingSeqNum() { return ++incomingSeqNum_; }

private:
    // Network components
    boost::asio::io_context& ioContext_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer heartbeatTimer_;
    boost::asio::steady_timer testRequestTimer_;
    
    // FIX protocol components
    FixMessageParser parser_;
    std::string senderCompId_;
    std::string targetCompId_;
    
    // Session state
    std::atomic<SessionState> state_{SessionState::Disconnected};
    std::atomic<SequenceNumber> incomingSeqNum_{0};
    std::atomic<SequenceNumber> outgoingSeqNum_{0};
    
    // Heartbeat management
    int heartbeatInterval_{fix::HEARTBEAT_INTERVAL};
    std::chrono::system_clock::time_point lastHeartbeatReceived_;
    std::chrono::system_clock::time_point lastHeartbeatSent_;
    bool testRequestSent_{false};
    
    // Message handlers
    NewOrderHandler newOrderHandler_;
    CancelReplaceHandler cancelReplaceHandler_;
    CancelHandler cancelHandler_;
    SessionEventHandler sessionEventHandler_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    size_t messagesReceived_{0};
    size_t messagesSent_{0};
    size_t heartbeatsSent_{0};
    size_t heartbeatsReceived_{0};
    std::chrono::system_clock::time_point sessionStartTime_;
    
    // Message buffer
    std::array<char, 8192> readBuffer_;
    std::string messageBuffer_;
    
    // Write queue for thread safety
    std::queue<std::string> writeQueue_;
    std::mutex writeMutex_;
    bool writing_{false};
    
    // Configuration
    std::shared_ptr<Config> config_;
    
    // Logging
    LoggerPtr logger_;
};

}