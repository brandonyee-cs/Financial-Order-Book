#include "orderbook/Core/Order.hpp"

namespace orderbook {

Order::Order(uint64_t id, Side side, Type type, TIF tif,
             double price, uint64_t quantity,
             std::string symbol, std::string account)
    : id(id), side(side), type(type), tif(tif),
      price(price), quantity(quantity),
      symbol(std::move(symbol)), account(std::move(account)),
      timestamp(std::chrono::system_clock::now()) {}

}