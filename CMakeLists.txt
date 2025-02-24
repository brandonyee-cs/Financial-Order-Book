cmake_minimum_required(VERSION 3.15)
project(OrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.70 REQUIRED COMPONENTS system)

set(CORE_SOURCES
    src/Core/Order.cpp
    src/Core/OrderBook.cpp
)

set(NETWORK_SOURCES
    src/Network/FixParser.cpp
    src/Network/FixSession.cpp
)

set(UTILITY_SOURCES
    src/Utilities/Logger.cpp
    src/Utilities/Config.cpp
)

add_executable(OrderBook
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${UTILITY_SOURCES}
    src/main.cpp
)

target_include_directories(OrderBook PRIVATE include)
target_link_libraries(OrderBook PRIVATE 
    Boost::system
)

install(TARGETS OrderBook DESTINATION bin)
install(DIRECTORY config/ DESTINATION config)
install(DIRECTORY third_party/ DESTINATION third_party)