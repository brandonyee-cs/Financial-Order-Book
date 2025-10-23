#include "orderbook/Network/FixSession.hpp"
#include "orderbook/Network/FixConstants.hpp"
#include "orderbook/Utilities/Config.hpp"
#include "orderbook/Utilities/PerformanceTimer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>

namespace orderbook {

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace fix;

FixSession::FixSession(tcp::socket socket, boost::asio::io_context& io_context, LoggerPtr logger)
    : ioContext_(io_context), socket_(std::move(socket)), 
      heartbeatTimer_(io_context), testRequestTimer_(io_context),
      sessionStartTime_(std::chrono::system_clock::now()), logger_(logger) {
    if (logger_) {
        logger_->info("FixSession created (server-side)", "FixSession::ctor");
    }
}

FixSession::FixSession(boost::asio::io_context& io_context, LoggerPtr logger)
    : ioContext_(io_context), socket_(io_context), 
      heartbeatTimer_(io_context), testRequestTimer_(io_context),
      sessionStartTime_(std::chrono::system_clock::now()), logger_(logger) {
    if (logger_) {
        logger_->info("FixSession created (client-side)", "FixSession::ctor");
    }
}

FixSession::FixSession(boost::asio::io_context& io_context, std::shared_ptr<Config> config, LoggerPtr logger)
    : ioContext_(io_context), socket_(io_context), 
      heartbeatTimer_(io_context), testRequestTimer_(io_context),
      sessionStartTime_(std::chrono::system_clock::now()), config_(config), logger_(logger) {
    loadConfiguration(config);
    if (logger_) {
        logger_->info("FixSession created with configuration", "FixSession::ctor");
    }
}

FixSession::~FixSession() {
    close();
}

void FixSession::start() {
    if (logger_) {
        logger_->info("Starting FIX session", "FixSession::start");
    }
    updateState(SessionState::LoggedIn, "Session started");
    startRead();
    startHeartbeatTimer();
}

void FixSession::connect(const std::string& host, uint16_t port,
                        const std::string& senderCompId, const std::string& targetCompId) {
    if (logger_) {
        logger_->info("Connecting to " + host + ":" + std::to_string(port) + 
                     " as " + senderCompId + " -> " + targetCompId, "FixSession::connect");
    }
    setSessionIds(senderCompId, targetCompId);
    updateState(SessionState::Connecting, "Connecting to " + host + ":" + std::to_string(port));
    
    tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    
    async_connect(socket_, endpoints,
        [self = shared_from_this()](boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                self->updateState(SessionState::LogonSent, "Connected, sending logon");
                self->sendLogon();
                self->startRead();
                self->startHeartbeatTimer();
            } else {
                self->handleError(ec, "connect");
            }
        });
}

void FixSession::sendLogon(int heartBtInt) {
    heartbeatInterval_ = heartBtInt;
    std::string logonMsg = parser_.generateLogon(senderCompId_, targetCompId_, 
                                               getNextOutgoingSeqNum(), heartBtInt);
    sendMessage(logonMsg);
    updateState(SessionState::LogonSent, "Logon sent");
}

void FixSession::sendLogout(const std::string& text) {
    std::string logoutMsg = parser_.generateLogout(senderCompId_, targetCompId_, 
                                                 getNextOutgoingSeqNum(), text);
    sendMessage(logoutMsg);
    updateState(SessionState::LogoutSent, "Logout sent: " + text);
}

void FixSession::sendExecutionReport(const FixMessageParser::ExecutionReport& execReport) {
    if (!isLoggedIn()) {
        return;
    }
    
    std::string execReportMsg = parser_.generateExecutionReport(execReport, senderCompId_, 
                                                              targetCompId_, getNextOutgoingSeqNum());
    sendMessage(execReportMsg);
}

void FixSession::sendHeartbeat(const std::string& testReqId) {
    std::string heartbeatMsg = parser_.generateHeartbeat(senderCompId_, targetCompId_, 
                                                       getNextOutgoingSeqNum(), testReqId);
    sendMessage(heartbeatMsg);
    
    std::lock_guard<std::mutex> lock(statsMutex_);
    ++heartbeatsSent_;
    lastHeartbeatSent_ = std::chrono::system_clock::now();
}

void FixSession::sendReject(SequenceNumber refSeqNum, const std::string& text) {
    std::string rejectMsg = parser_.generateReject(senderCompId_, targetCompId_, 
                                                 getNextOutgoingSeqNum(), refSeqNum, text);
    sendMessage(rejectMsg);
}

void FixSession::setSessionIds(const std::string& senderCompId, const std::string& targetCompId) {
    senderCompId_ = senderCompId;
    targetCompId_ = targetCompId;
}

FixSession::SessionStats FixSession::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    SessionStats stats;
    stats.incomingSeqNum = incomingSeqNum_.load();
    stats.outgoingSeqNum = outgoingSeqNum_.load();
    stats.messagesReceived = messagesReceived_;
    stats.messagesSent = messagesSent_;
    stats.heartbeatsSent = heartbeatsSent_;
    stats.heartbeatsReceived = heartbeatsReceived_;
    stats.lastHeartbeat = lastHeartbeatReceived_;
    stats.sessionStartTime = sessionStartTime_;
    return stats;
}

void FixSession::close() {
    if (state_ != SessionState::Disconnected) {
        updateState(SessionState::Disconnecting, "Closing session");
        
        boost::system::error_code ec;
        heartbeatTimer_.cancel(ec);
        testRequestTimer_.cancel(ec);
        socket_.close(ec);
        
        updateState(SessionState::Disconnected, "Session closed");
    }
}

void FixSession::startRead() {
    readMessage();
}

void FixSession::readMessage() {
    auto self = shared_from_this();
    
    // Read until we find a complete FIX message (ending with SOH)
    async_read_until(socket_, boost::asio::dynamic_buffer(messageBuffer_), FIELD_DELIMITER,
        [self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                // Extract the complete message
                std::string message = self->messageBuffer_.substr(0, bytes_transferred);
                self->messageBuffer_.erase(0, bytes_transferred);
                
                self->processMessage(message);
                
                // Continue reading
                self->readMessage();
            } else {
                self->handleError(ec, "read");
            }
        });
}

void FixSession::processMessage(const std::string& message) {
    PERF_TIMER("FixSession::processMessage", logger_);
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++messagesReceived_;
    }
    
    if (logger_) {
        logger_->debug("Processing FIX message: " + message.substr(0, std::min(message.length(), size_t(100))) + 
                      (message.length() > 100 ? "..." : ""), "FixSession::processMessage");
    }
    
    // Parse the FIX message
    auto fixMsg = parser_.parseMessage(message);
    
    if (!fixMsg.isValid) {
        sendReject(incomingSeqNum_.load(), "Invalid message format: " + fixMsg.errorMessage);
        return;
    }
    
    // Validate sequence number
    SequenceNumber expectedSeqNum = getNextIncomingSeqNum();
    SequenceNumber receivedSeqNum = 0;
    
    try {
        std::string seqNumStr = fixMsg.getField(TAG_MSG_SEQ_NUM);
        if (!seqNumStr.empty()) {
            receivedSeqNum = std::stoull(seqNumStr);
        }
    } catch (const std::exception&) {
        sendReject(expectedSeqNum, "Invalid sequence number");
        return;
    }
    
    if (!validateSequenceNumber(expectedSeqNum, receivedSeqNum)) {
        sendReject(receivedSeqNum, "Sequence number too low");
        return;
    }
    
    // Handle different message types
    switch (fixMsg.msgType) {
        case MSG_TYPE_LOGON:
            handleLogon(fixMsg);
            break;
        case MSG_TYPE_LOGOUT:
            handleLogout(fixMsg);
            break;
        case MSG_TYPE_HEARTBEAT:
            handleHeartbeat(fixMsg);
            break;
        case MSG_TYPE_TEST_REQUEST:
            handleTestRequest(fixMsg);
            break;
        case MSG_TYPE_NEW_ORDER_SINGLE:
            handleNewOrderSingle(fixMsg);
            break;
        case MSG_TYPE_ORDER_CANCEL_REPLACE_REQUEST:
            handleOrderCancelReplaceRequest(fixMsg);
            break;
        case MSG_TYPE_ORDER_CANCEL_REQUEST:
            handleOrderCancelRequest(fixMsg);
            break;
        case MSG_TYPE_REJECT:
            handleReject(fixMsg);
            break;
        default:
            sendReject(receivedSeqNum, "Unsupported message type: " + std::string(1, fixMsg.msgType));
            break;
    }
}

void FixSession::handleLogon(const FixMessageParser::FixMessage& msg) {
    if (state_ == SessionState::LogonSent || state_ == SessionState::Disconnected) {
        updateState(SessionState::LoggedIn, "Logon received");
        
        // Extract heartbeat interval
        std::string heartBtIntStr = msg.getField(TAG_HEARTBT_INT);
        if (!heartBtIntStr.empty()) {
            try {
                heartbeatInterval_ = std::stoi(heartBtIntStr);
            } catch (const std::exception&) {
                heartbeatInterval_ = HEARTBEAT_INTERVAL;
            }
        }
        
        // Send logon response if we're the server
        if (state_ == SessionState::Disconnected) {
            sendLogon(heartbeatInterval_);
        }
    }
}

void FixSession::handleLogout(const FixMessageParser::FixMessage& msg) {
    std::string text = msg.getField(58); // Text field
    updateState(SessionState::Disconnecting, "Logout received: " + text);
    
    // Send logout response
    sendLogout("Logout acknowledged");
    close();
}

void FixSession::handleHeartbeat(const FixMessageParser::FixMessage& msg) {
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++heartbeatsReceived_;
        lastHeartbeatReceived_ = std::chrono::system_clock::now();
    }
    
    // Reset test request flag if this was a response
    std::string testReqId = msg.getField(TAG_TEST_REQ_ID);
    if (!testReqId.empty()) {
        testRequestSent_ = false;
        testRequestTimer_.cancel();
    }
}

void FixSession::handleTestRequest(const FixMessageParser::FixMessage& msg) {
    std::string testReqId = msg.getField(TAG_TEST_REQ_ID);
    sendHeartbeat(testReqId);
}

void FixSession::handleNewOrderSingle(const FixMessageParser::FixMessage& msg) {
    if (!isLoggedIn()) {
        return;
    }
    
    auto newOrder = parser_.parseNewOrderSingle(msg);
    if (newOrder.isValid && newOrderHandler_) {
        newOrderHandler_(newOrder);
    } else {
        sendReject(incomingSeqNum_.load(), "Invalid New Order Single: " + newOrder.errorMessage);
    }
}

void FixSession::handleOrderCancelReplaceRequest(const FixMessageParser::FixMessage& msg) {
    if (!isLoggedIn()) {
        return;
    }
    
    auto cancelReplace = parser_.parseOrderCancelReplaceRequest(msg);
    if (cancelReplace.isValid && cancelReplaceHandler_) {
        cancelReplaceHandler_(cancelReplace);
    } else {
        sendReject(incomingSeqNum_.load(), "Invalid Cancel Replace Request: " + cancelReplace.errorMessage);
    }
}

void FixSession::handleOrderCancelRequest(const FixMessageParser::FixMessage& msg) {
    if (!isLoggedIn()) {
        return;
    }
    
    auto cancelRequest = parser_.parseOrderCancelRequest(msg);
    if (cancelRequest.isValid && cancelHandler_) {
        cancelHandler_(cancelRequest);
    } else {
        sendReject(incomingSeqNum_.load(), "Invalid Cancel Request: " + cancelRequest.errorMessage);
    }
}

void FixSession::handleReject(const FixMessageParser::FixMessage& msg) {
    std::string text = msg.getField(58); // Text field
    std::string refSeqNum = msg.getField(45); // RefSeqNum field
    
    if (sessionEventHandler_) {
        sessionEventHandler_(state_, "Message rejected - SeqNum: " + refSeqNum + ", Reason: " + text);
    }
}

void FixSession::sendMessage(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++messagesSent_;
    }
    
    writeMessage(message);
}

void FixSession::writeMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    
    bool write_in_progress = !writeQueue_.empty();
    writeQueue_.push(message);
    
    if (!write_in_progress) {
        doWrite();
    }
}

void FixSession::doWrite() {
    auto self = shared_from_this();
    
    async_write(socket_, buffer(writeQueue_.front()),
        [self](boost::system::error_code ec, std::size_t) {
            std::lock_guard<std::mutex> lock(self->writeMutex_);
            
            if (!ec) {
                self->writeQueue_.pop();
                if (!self->writeQueue_.empty()) {
                    self->doWrite();
                }
            } else {
                self->handleError(ec, "write");
            }
        });
}

void FixSession::startHeartbeatTimer() {
    heartbeatTimer_.expires_after(std::chrono::seconds(heartbeatInterval_));
    
    auto self = shared_from_this();
    heartbeatTimer_.async_wait([self](const boost::system::error_code& error) {
        self->onHeartbeatTimeout(error);
    });
}

void FixSession::onHeartbeatTimeout(const boost::system::error_code& error) {
    if (error || state_ != SessionState::LoggedIn) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastHeartbeatReceived_).count();
    
    if (timeSinceLastHeartbeat > heartbeatInterval_ * 2) {
        if (!testRequestSent_) {
            // Send test request
            std::string testReqId = "TR" + std::to_string(outgoingSeqNum_.load());
            sendHeartbeat(testReqId);
            testRequestSent_ = true;
            startTestRequestTimer();
        } else {
            // No response to test request - disconnect
            updateState(SessionState::Disconnecting, "Heartbeat timeout");
            close();
            return;
        }
    } else if (timeSinceLastHeartbeat > heartbeatInterval_) {
        // Send regular heartbeat
        sendHeartbeat();
    }
    
    // Restart timer
    startHeartbeatTimer();
}

void FixSession::startTestRequestTimer() {
    testRequestTimer_.expires_after(std::chrono::seconds(heartbeatInterval_));
    
    auto self = shared_from_this();
    testRequestTimer_.async_wait([self](const boost::system::error_code& error) {
        self->onTestRequestTimeout(error);
    });
}

void FixSession::onTestRequestTimeout(const boost::system::error_code& error) {
    if (error || state_ != SessionState::LoggedIn) {
        return;
    }
    
    if (testRequestSent_) {
        // No response to test request - disconnect
        updateState(SessionState::Disconnecting, "Test request timeout");
        close();
    }
}

bool FixSession::validateSequenceNumber(SequenceNumber expectedSeqNum, SequenceNumber receivedSeqNum) {
    return receivedSeqNum >= expectedSeqNum;
}

void FixSession::updateState(SessionState newState, const std::string& reason) {
    SessionState oldState = state_.exchange(newState);
    
    if (oldState != newState && sessionEventHandler_) {
        sessionEventHandler_(newState, reason);
    }
}

void FixSession::handleError(const boost::system::error_code& error, const std::string& context) {
    if (error != boost::asio::error::operation_aborted) {
        if (logger_) {
            logger_->error("FIX session error in " + context + ": " + error.message() + 
                          " (code: " + std::to_string(error.value()) + ")", 
                          "FixSession::handleError - " + senderCompId_ + "->" + targetCompId_);
        }
        updateState(SessionState::Disconnected, context + " error: " + error.message());
        close();
    }
}

void FixSession::loadConfiguration(std::shared_ptr<Config> config) {
    config_ = config;
    if (!config_) {
        return;
    }
    
    // Load network settings
    heartbeatInterval_ = config_->getInt("network", "heartbeat_interval", fix::HEARTBEAT_INTERVAL);
    
    // Load session identifiers if available
    std::string senderCompId = config_->getString("network", "sender_comp_id", "");
    std::string targetCompId = config_->getString("network", "target_comp_id", "");
    
    if (!senderCompId.empty() && !targetCompId.empty()) {
        setSessionIds(senderCompId, targetCompId);
    }
}

}