# Order Book System

A high-performance order book implementation with FIX protocol support, designed for electronic trading systems. Provides core matching engine functionality with real-time market data dissemination.

## Features

- **Order Matching Engine**
  - Limit/Market orders
  - Price-time priority matching
  - Order cancellation/modification
  - Best Bid/Offer (BBO) tracking
- **FIX Protocol 4.4 Support**
  - New Order Single (D) messages
  - Execution Reports (8)
  - TCP-based order entry
- **Market Data Feed**
  - Real-time updates
  - Best bid/ask tracking
  - Depth-of-book information
- **Risk Management**
  - Order validation
  - Position limits
  - Fat-finger prevention
- **Production-Ready Infrastructure**
  - Async networking (Boost.Asio)
  - Comprehensive logging
  - Configurable parameters

## Installation

### Prerequisites
- C++17 compiler
- CMake 3.15+
- Boost 1.70+

### Build Instructions
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

## Usage

### Start Server
```bash
./OrderBook
```

### Send FIX Order (Example)
```bash
echo -e "8=FIX.4.4\x0111=123\x0155=BTC/USD\x0154=1\x0140=2\x0144=50000\x0138=100\x0135=D\x01" | nc localhost 5000
```

### Expected Output
```
[Market Update] BID: 50000.00 (100) | ASK: 50100.00 (50)
[Execution] Order 123: Filled 100 @ 50000.00
```

## File Architecture

```
orderbook/
├── CMakeLists.txt
├── include/
│   └── orderbook/
│       ├── Core/               # Order book logic
│       │   ├── Order.hpp
│       │   ├── OrderBook.hpp
│       │   └── Types.hpp
│       ├── Network/            # FIX protocol implementation
│       │   ├── FixParser.hpp
│       │   ├── FixSession.hpp
│       │   └── FixConstants.hpp
│       ├── MarketData/         # Market data distribution
│       │   └── MarketDataFeed.hpp
│       ├── Risk/               # Risk controls
│       │   └── RiskManager.hpp
│       └── Utilities/          # Support components
│           └── Logger.hpp
├── src/
│   ├── Core/                   # Core implementation
│   │   ├── Order.cpp
│   │   └── OrderBook.cpp
│   ├── Network/                # Network layer
│   │   ├── FixParser.cpp
│   │   └── FixSession.cpp
│   ├── MarketData/             # Market data impl
│   │   └── MarketDataFeed.cpp
│   ├── Risk/                   # Risk management
│   │   └── RiskManager.cpp
│   ├── Utilities/              # Utilities
│   │   └── Logger.cpp
│   └── main.cpp                # Entry point
└── third_party/
    └── fix/                    # FIX specifications
        └── FIX44.xml
```

## Key Components

1. **Order Book Core**
   - Implements price-time priority matching
   - Handles order lifecycle management
   - Maintains bid/ask ladders

2. **FIX Protocol Layer**
   - TCP-based order entry
   - Async message processing
   - Session management

3. **Market Data System**
   - Real-time updates via callbacks
   - BBO tracking
   - Volume-at-price information

4. **Risk Management**
   - Order validation checks
   - Position monitoring
   - Circuit breakers

## Configuration

Edit `config/orderbook.cfg`:
```ini
[network]
port = 5000
max_connections = 100

[risk]
max_order_size = 10000
position_limit = 100000
```

## License

MIT License - See [LICENSE](LICENSE) for details

## Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature`)
3. Commit changes (`git commit -am 'Add feature'`)
4. Push branch (`git push origin feature`)
5. Open Pull Request

## Acknowledgements

- Boost.Asio for networking
- FIX Protocol Limited for specifications
- C++ Standard Template Library

---

This implementation provides a foundation for building exchange-grade trading systems. For production use, consider adding:
- Order persistence
- Performance monitoring
- Binary protocols (SBE/FAST)
- Colocation optimizations