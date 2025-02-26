#include "orderbook/Core/OrderBook.hpp"
#include <iostream>

using namespace orderbook;

int main() {
    OrderBook book;
    
    // Add orders
    book.addOrder(1, Side::Buy, 100.0, 500);
    book.addOrder(2, Side::Sell, 101.0, 300);
    
    std::cout << "Initial Best Bid: " << book.bestBid().value_or(0) << "\n";
    std::cout << "Initial Best Ask: " << book.bestAsk().value_or(0) << "\n\n";
    
    // Modify order
    book.modifyOrder(1, 100.5, 600);
    std::cout << "After Modification - Best Bid: " << book.bestBid().value_or(0) << "\n";
    
    // Cancel order
    book.cancelOrder(2);
    std::cout << "After Cancellation - Best Ask: " << book.bestAsk().value_or(0) << "\n";
    
    return 0;
}