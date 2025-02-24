#include "orderbook/Network/FixSession.hpp"
#include "orderbook/Network/FixParser.hpp"
#include <boost/asio.hpp>

namespace orderbook {

using namespace boost::asio;
using namespace boost::asio::ip;

FixSession::FixSession(tcp::socket socket, OrderHandler handler)
    : socket_(std::move(socket)), orderHandler_(handler) {}

void FixSession::start() {
    readHeader();
}