cmake_minimum_required(VERSION 3.15)
project(OrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system)

add_executable(OrderBook
    src/Core/Order.cpp
    src/Core/OrderBook.cpp
    src/Network/FixParser.cpp
    src/Network/FixSession.cpp
    src/MarketData/MarketDataFeed.cpp
    src/Risk/RiskManager.cpp
    src/Utilities/Logger.cpp
    src/main.cpp
)

target_include_directories(OrderBook PRIVATE include)
target_link_libraries(OrderBook PRIVATE Boost::system)