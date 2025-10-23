#pragma once
#include "../Core/OrderManager.hpp"
#include "../Risk/RiskManager.hpp"
#include "FixSession.hpp"
#include "FixMessageHandler.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

namespace orderbook {

/**
 * @brief FIX Protocol Server
 * Manages multiple FIX sessions and handles incoming connections
 */
class FixServer {
public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     * @param orderManager Order management system
     * @param riskManager Risk management system
     */
    FixServer(boost::asio::io_context& io_context,
             std::shared_ptr<OrderManager> orderManager,
             std::shared_ptr<RiskManager> riskManager);
    
    /**
     * @brief Start the server on specified port
     * @param port Port to listen on
     * @param senderCompId Server component ID
     */
    void start(uint16_t port, const std::string& senderCompId);
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Get server statistics
     */
    struct ServerStats {
        size_t activeConnections;
        size_t totalConnections;
        size_t messagesProcessed;
        size_t ordersProcessed;
        std::chrono::system_clock::time_point startTime;
    };
    
    ServerStats getStats() const;

private:
    /**
     * @brief Accept new connections
     */
    void startAccept();
    
    /**
     * @brief Handle new connection
     * @param session New FIX session
     * @param error Error code
     */
    void handleAccept(std::shared_ptr<FixSession> session, const boost::system::error_code& error);
    
    /**
     * @brief Handle session events
     * @param session Session that generated the event
     * @param state New session state
     * @param reason Reason for state change
     */
    void handleSessionEvent(std::shared_ptr<FixSession> session, 
                          FixSession::SessionState state, const std::string& reason);
    
    /**
     * @brief Clean up disconnected sessions
     */
    void cleanupSessions();

private:
    // Network components
    boost::asio::io_context& ioContext_;
    boost::asio::ip::tcp::acceptor acceptor_;
    
    // Core components
    std::shared_ptr<OrderManager> orderManager_;
    std::shared_ptr<RiskManager> riskManager_;
    
    // Session management
    std::vector<std::shared_ptr<FixSession>> sessions_;
    std::vector<std::shared_ptr<FixMessageHandler>> messageHandlers_;
    
    // Server configuration
    std::string senderCompId_;
    
    // Statistics
    std::atomic<size_t> totalConnections_{0};
    std::atomic<size_t> messagesProcessed_{0};
    std::atomic<size_t> ordersProcessed_{0};
    std::chrono::system_clock::time_point startTime_;
    
    // Server state
    std::atomic<bool> running_{false};
};

}