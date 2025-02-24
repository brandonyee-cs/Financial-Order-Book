# Order Book System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance trading system with FIX protocol support and configurable runtime parameters.

## Features

- **Configurable Operation** via INI-style config file
- FIX 4.4 Protocol Support
- Real-time Market Data Feed
- Risk Management Controls
- Async Network Layer

## Configuration

Edit `config/orderbook.cfg`:

```ini
[orderbook]
symbol = BTC/USD       # Trading instrument pair
max_orders = 1000000   # Maximum active orders

[network]
port = 5000            # FIX protocol listening port
max_connections = 1000 # Maximum concurrent clients

[risk]
max_order_size = 10000 # Maximum quantity per order
max_position = 100000  # Maximum net position allowed
max_price = 1000000.0  # Maximum acceptable price

[logging]
level = info           # Log verbosity (debug|info|warn|error)
file = orderbook.log   # Log file path
```

**To Modify Configuration**:
1. Edit `config/orderbook.cfg`
2. Restart the application
3. Changes take effect immediately

## File Architecture

```
orderbook/
├── CMakeLists.txt
├── config/
│   └── orderbook.cfg       # Runtime configuration
├── include/
│   └── orderbook/
│       ├── Core/           # Matching engine core
│       ├── Network/        # FIX protocol implementation
│       ├── MarketData/     # Real-time data distribution
│       ├── Risk/           # Risk controls
│       └── Utilities/      # Support components
├── src/
│   ├── Core/               # Business logic
│   ├── Network/            # Protocol handling
│   ├── MarketData/         # Data publishing
│   ├── Risk/               # Risk checks
│   ├── Utilities/          # Infrastructure
│   └── main.cpp            # Entry point
└── third_party/
    └── fix/                # Protocol specifications
```

## Build & Run

```bash
# Clone and build
git clone https://github.com/yourrepo/orderbook
cd orderbook
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run with default config
./OrderBook ../config/orderbook.cfg

# Run with custom config
./OrderBook /path/to/custom.cfg
```

## License

MIT License - See [LICENSE](LICENSE) for details.

---

This implementation provides a complete configuration system with:
1. INI-file parsing
2. Type-safe accessors
3. Default value handling
4. Runtime reconfiguration capability
5. Clear documentation

The system will automatically load configuration at startup and use it for all operational parameters.