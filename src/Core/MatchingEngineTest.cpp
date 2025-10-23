#include "orderbook/Core/MatchingEngine.hpp"
#include "orderbook/Core/Order.hpp"
#include "orderbook/Utilities/Logger.hpp"
#include <iostream>
#include <cassert>

using namespace orderbook;

void testBasicMatching() {
    std::cout << "Testing basic order matching..." << std::endl;
    
    // Create logger
    auto logger = std::make_shared<Logger>();
    
    // Create matching engine
    MatchingEngine engine(logger);
    
    // Create orders
    Order buy_order(1, Side::Buy, OrderType::Limit, 100.0, 50, "AAPL", "account1");
    Order sell_order(2, Side::Sell, OrderType::Limit, 99.0, 30, "AAPL", "account2");
    
    // Create price levels
    std::vector<PriceLevel> ask_levels;
    ask_levels.emplace_back(99.0);
    ask_levels.back().addOrder(&sell_order);
    
    // Match buy order against ask levels
    MatchResult result = engine.matchBuyOrder(buy_order, ask_levels);
    
    // Verify results
    assert(result.hasTrades());
    assert(result.getTradeCount() == 1);
    assert(result.total_filled_quantity == 30);
    assert(!result.fully_filled);
    assert(result.hasRemainingOrder());
    assert(result.hasExecutionReports());
    
    // Verify trade details
    const Trade& trade = result.trades[0];
    assert(trade.buy_order_id == OrderId(1));
    assert(trade.sell_order_id == OrderId(2));
    assert(trade.price == 99.0);
    assert(trade.quantity == 30);
    
    // Verify execution report
    assert(result.execution_reports.size() == 1);
    const ExecutionReport& report = result.execution_reports[0];
    assert(report.order_id == OrderId(1));
    assert(report.exec_type == ExecutionReport::ExecType::PartialFill);
    assert(report.filled_quantity == 30);
    assert(report.leaves_quantity == 20);
    
    std::cout << "Basic matching test passed!" << std::endl;
}

void testFullFill() {
    std::cout << "Testing full order fill..." << std::endl;
    
    auto logger = std::make_shared<Logger>();
    MatchingEngine engine(logger);
    
    // Create orders with exact matching quantities
    Order buy_order(3, Side::Buy, OrderType::Limit, 100.0, 50, "AAPL", "account1");
    Order sell_order(4, Side::Sell, OrderType::Limit, 99.0, 50, "AAPL", "account2");
    
    // Create price levels
    std::vector<PriceLevel> ask_levels;
    ask_levels.emplace_back(99.0);
    ask_levels.back().addOrder(&sell_order);
    
    // Match buy order
    MatchResult result = engine.matchBuyOrder(buy_order, ask_levels);
    
    // Verify full fill
    assert(result.hasTrades());
    assert(result.getTradeCount() == 1);
    assert(result.total_filled_quantity == 50);
    assert(result.fully_filled);
    assert(!result.hasRemainingOrder());
    
    // Verify execution report shows full fill
    assert(result.execution_reports.size() == 1);
    const ExecutionReport& report = result.execution_reports[0];
    assert(report.exec_type == ExecutionReport::ExecType::Fill);
    assert(report.filled_quantity == 50);
    assert(report.leaves_quantity == 0);
    
    std::cout << "Full fill test passed!" << std::endl;
}

void testMultiplePriceLevels() {
    std::cout << "Testing matching across multiple price levels..." << std::endl;
    
    auto logger = std::make_shared<Logger>();
    MatchingEngine engine(logger);
    
    // Create large buy order
    Order buy_order(5, Side::Buy, OrderType::Limit, 102.0, 100, "AAPL", "account1");
    
    // Create multiple sell orders at different prices
    Order sell_order1(6, Side::Sell, OrderType::Limit, 99.0, 30, "AAPL", "account2");
    Order sell_order2(7, Side::Sell, OrderType::Limit, 100.0, 40, "AAPL", "account3");
    Order sell_order3(8, Side::Sell, OrderType::Limit, 101.0, 50, "AAPL", "account4");
    
    // Create price levels
    std::vector<PriceLevel> ask_levels;
    ask_levels.emplace_back(99.0);
    ask_levels.back().addOrder(&sell_order1);
    ask_levels.emplace_back(100.0);
    ask_levels.back().addOrder(&sell_order2);
    ask_levels.emplace_back(101.0);
    ask_levels.back().addOrder(&sell_order3);
    
    // Match buy order
    MatchResult result = engine.matchBuyOrder(buy_order, ask_levels);
    
    // Verify multiple trades
    assert(result.hasTrades());
    assert(result.getTradeCount() == 3);
    assert(result.total_filled_quantity == 100);
    assert(result.fully_filled);
    
    // Verify trades are at correct prices (price-time priority)
    assert(result.trades[0].price == 99.0);
    assert(result.trades[0].quantity == 30);
    assert(result.trades[1].price == 100.0);
    assert(result.trades[1].quantity == 40);
    assert(result.trades[2].price == 101.0);
    assert(result.trades[2].quantity == 30);
    
    std::cout << "Multiple price levels test passed!" << std::endl;
}

void testExecutionReportGeneration() {
    std::cout << "Testing execution report generation..." << std::endl;
    
    MatchingEngine engine;
    
    // Test new order report
    Order order(9, Side::Buy, OrderType::Limit, 100.0, 50, "AAPL", "account1");
    ExecutionReport new_report = MatchingEngine::generateNewOrderReport(order);
    
    assert(new_report.order_id == OrderId(9));
    assert(new_report.exec_type == ExecutionReport::ExecType::New);
    assert(new_report.order_status == OrderStatus::New);
    assert(new_report.filled_quantity == 0);
    assert(new_report.leaves_quantity == 50);
    
    // Test rejection report
    ExecutionReport reject_report = MatchingEngine::generateRejectionReport(order, "Risk limit exceeded");
    assert(reject_report.exec_type == ExecutionReport::ExecType::Rejected);
    assert(reject_report.order_status == OrderStatus::Rejected);
    assert(reject_report.text == "Risk limit exceeded");
    
    // Test partial fill report
    ExecutionReport partial_report = MatchingEngine::handlePartialFill(order, 20);
    assert(partial_report.exec_type == ExecutionReport::ExecType::PartialFill);
    assert(partial_report.filled_quantity == 20);
    assert(partial_report.leaves_quantity == 30);
    
    std::cout << "Execution report generation test passed!" << std::endl;
}

int main() {
    try {
        testBasicMatching();
        testFullFill();
        testMultiplePriceLevels();
        testExecutionReportGeneration();
        
        std::cout << "\nAll MatchingEngine tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}