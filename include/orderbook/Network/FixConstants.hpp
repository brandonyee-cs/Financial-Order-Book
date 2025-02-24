#pragma once

namespace orderbook::fix {
    constexpr char MSG_TYPE_NEW_ORDER = 'D';
    constexpr char MSG_TYPE_EXECUTION_REPORT = '8';
    constexpr int TAG_MSG_TYPE = 35;
    constexpr int TAG_ORDER_ID = 11;
    constexpr int TAG_SYMBOL = 55;
    constexpr int TAG_SIDE = 54;
    constexpr int TAG_PRICE = 44;
    constexpr int TAG_QUANTITY = 38;
}