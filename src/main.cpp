#include "orderbook/Core/OrderBook.hpp"
#include <iostream>

using namespace orderbook;

void printDemoHeader(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main() {
    OrderBook book;

    printDemoHeader("1. Adding Initial Orders");
    book.addOrder({1, Order::Side::Buy, Order::Type::Limit, 100.0, 500, "AAPL"});
    
    book.addOrder({2, Order::Side::Sell, Order::Type::Limit, 101.0, 300, "AAPL"});

    printDemoHeader("2. Market Order Matching");
    book.addOrder({3, Order::Side::Buy, Order::Type::Market, 0.0, 200, "AAPL"});

    printDemoHeader("3. Order Cancellation");
    book.addOrder({4, Order::Side::Buy, Order::Type::Limit, 99.5, 100, "AAPL"});
    book.cancelOrder(4);

    printDemoHeader("4. Error Handling Demo");
    book.cancelOrder(99);
    
    book.addOrder({5, Order::Side::Sell, Order::Type::Market, 105.0, 50, "AAPL"});

    printDemoHeader("Final Book State");
    std::cout << "Best Bid: " << book.getBestBid().value_or(0) << "\n";
    std::cout << "Best Ask: " << book.getBestAsk().value_or(0) << "\n";

    return 0;
}