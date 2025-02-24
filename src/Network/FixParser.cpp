#include "orderbook/Network/FixParser.hpp"
#include "orderbook/Network/FixConstants.hpp"
#include <sstream>

namespace orderbook {

using namespace fix;

Order FixParser::parseNewOrderSingle(const std::string& fixMsg) {
    auto msg = parseMessage(fixMsg);
    return Order(
        std::stoull(getFieldValue(msg, TAG_ORDER_ID)),
        getFieldValue(msg, TAG_SIDE) == "1" ? Order::Side::Buy : Order::Side::Sell,
        Order::Type::Limit,
        Order::TIF::GTC,
        std::stod(getFieldValue(msg, TAG_PRICE)),
        std::stoull(getFieldValue(msg, TAG_QUANTITY)),
        getFieldValue(msg, TAG_SYMBOL),
        ""
    );
}