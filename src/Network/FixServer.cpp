#include "orderbook/Network/FixServer.hpp"
#include <iostream>
#include <algorithm>

namespace orderbook {

using namespace boost::asio;
using namespace boost::asio::ip;

FixServer::FixServer(boost::asio::io_context& io_context,
                    std::shared_ptr<OrderManager> orderManager,
                    std::shared_ptr<RiskManager> riskManager)
    : ioContext_(io_context), acceptor_(io_context),
      orderManager_(std::move(orderManager)), riskManager_(std::move(riskManager)),
      startTime_(std::chrono::system_clock::now()) {
}

void FixServer::start(uint16_t port, const std::string& senderCompId) {
    senderCompId_ = senderCompId;
    
    try {
        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        running_ = true;
        startTime_ = std::chrono::system_clock::now();
        
        std::cout << "FIX Server started on port " << port << " with SenderCompID: " << senderCompId << std::endl;
        
        startAccept();
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start FIX server: " << e.what() << std::endl;
        throw;
    }
}

void FixServer::stop() {
    if (running_) {
        running_ = false;
        
        boost::system::error_code ec;
        acceptor_.close(ec);
        
        // Close all sessions
        for (auto& session : sessions_) {
            if (session) {
                session->close();
            }
        }
        
        sessions_.clear();
        messageHandlers_.clear();
        
        std::cout << "FIX Server stopped" << std::endl;
    }
}

FixServer::ServerStats FixServer::getStats() const {
    ServerStats stats;
    stats.activeConnections = std::count_if(sessions_.begin(), sessions_.end(),
        [](const std::shared_ptr<FixSession>& session) {
            return session && session->isLoggedIn();
        });
    stats.totalConnections = totalConnections_.load();
    stats.messagesProcessed = messagesProcessed_.load();
    stats.ordersProcessed = ordersProcessed_.load();
    stats.startTime = startTime_;
    return stats;
}

void FixServer::startAccept() {
    if (!running_) {
        return;
    }
    
    auto socket = std::make_unique<tcp::socket>(ioContext_);
    auto* socketPtr = socket.get();
    
    acceptor_.async_accept(*socketPtr,
        [this, socket = std::move(socket)](const boost::system::error_code& error) mutable {
            if (!error) {
                auto newSession = std::make_shared<FixSession>(std::move(*socket), ioContext_);
                handleAccept(newSession, error);
            } else {
                handleAccept(nullptr, error);
            }
        });
}

void FixServer::handleAccept(std::shared_ptr<FixSession> session, const boost::system::error_code& error) {
    if (!error && running_ && session) {
        ++totalConnections_;
        
        // Create message handler for this session
        auto messageHandler = std::make_shared<FixMessageHandler>(orderManager_, riskManager_);
        messageHandler->setFixSession(session);
        
        // Set up session handlers
        session->setNewOrderHandler([messageHandler](const FixMessageParser::NewOrderSingle& newOrder) {
            messageHandler->handleNewOrderSingle(newOrder);
        });
        
        session->setCancelReplaceHandler([messageHandler](const FixMessageParser::OrderCancelReplaceRequest& cancelReplace) {
            messageHandler->handleOrderCancelReplaceRequest(cancelReplace);
        });
        
        session->setCancelHandler([messageHandler](const FixMessageParser::OrderCancelRequest& cancelRequest) {
            messageHandler->handleOrderCancelRequest(cancelRequest);
        });
        
        session->setSessionEventHandler([this, session](FixSession::SessionState state, const std::string& reason) {
            handleSessionEvent(session, state, reason);
        });
        
        // Set session IDs (client will provide TargetCompID in logon)
        session->setSessionIds(senderCompId_, "CLIENT"); // Default target, will be updated on logon
        
        // Store session and handler
        sessions_.push_back(session);
        messageHandlers_.push_back(messageHandler);
        
        // Start the session
        session->start();
        
        std::cout << "New FIX session accepted. Total connections: " << totalConnections_.load() << std::endl;
        
        // Clean up old sessions periodically
        if (sessions_.size() % 10 == 0) {
            cleanupSessions();
        }
        
    } else if (error) {
        std::cerr << "Accept error: " << error.message() << std::endl;
    }
    
    // Continue accepting new connections
    startAccept();
}

void FixServer::handleSessionEvent(std::shared_ptr<FixSession> session, 
                                 FixSession::SessionState state, const std::string& reason) {
    switch (state) {
        case FixSession::SessionState::LoggedIn:
            std::cout << "Session logged in: " << reason << std::endl;
            break;
        case FixSession::SessionState::Disconnected:
            std::cout << "Session disconnected: " << reason << std::endl;
            break;
        case FixSession::SessionState::LogoutSent:
            std::cout << "Session logout: " << reason << std::endl;
            break;
        default:
            break;
    }
}

void FixServer::cleanupSessions() {
    // Remove disconnected sessions
    auto sessionIt = sessions_.begin();
    auto handlerIt = messageHandlers_.begin();
    
    while (sessionIt != sessions_.end() && handlerIt != messageHandlers_.end()) {
        if (!*sessionIt || (*sessionIt)->getState() == FixSession::SessionState::Disconnected) {
            sessionIt = sessions_.erase(sessionIt);
            handlerIt = messageHandlers_.erase(handlerIt);
        } else {
            ++sessionIt;
            ++handlerIt;
        }
    }
}

}