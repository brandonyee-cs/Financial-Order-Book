#pragma once
#include "../Core/Order.hpp"
#include <string>
#include <vector>

namespace orderbook {

class FixParser {
public:
    struct FixMessage {
        char msgType;
        std::vector<std::pair<int, std::string>> fields;
    };

    Order parseNewOrderSingle(const std::string& fixMsg);
    std::string generateExecutionReport(const Order& order);

private:
    FixMessage parseMessage(const std::string& fixMsg);
    std::string getFieldValue(const FixMessage& msg, int tag);
};

}