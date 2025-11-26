# Technology Stack

## Framework & SDK
- **ESP-IDF**: Espressif IoT Development Framework
- **FreeRTOS**: Real-time operating system for microcontrollers
- **CMake**: Build system (minimum version 3.16)

## Programming Languages
- **C**: Primary language for firmware development
- **Python**: Used for testing and automation (pytest)

## Development Environment
- **Docker**: Containerized development with `espressif/idf` image
- **VS Code**: Recommended IDE with ESP-IDF extensions
- **DevContainer**: Pre-configured development environment

## Key Libraries & Components
- `esp_chip_info`: Hardware information retrieval
- `esp_flash`: Flash memory operations
- `esp_system`: System-level functions
- `freertos/FreeRTOS.h` & `freertos/task.h`: RTOS functionality
- `spi_flash`: SPI flash driver (private requirement)

## Common Commands

### Build & Flash
```bash
# Set up ESP-IDF environment (in container)
source /opt/esp/idf/export.sh

# Configure project
idf.py menuconfig

# Build project
idf.py build

# Flash to device
idf.py -p PORT flash

# Monitor serial output
idf.py -p PORT monitor

# Build, flash and monitor in one command
idf.py -p PORT flash monitor
```

### Testing
```bash
# Run automated tests
pytest pytest_hello_world.py

# Run specific test
pytest pytest_hello_world.py::test_hello_world

# Run host tests (Linux)
pytest pytest_hello_world.py::test_hello_world_linux
```

### Project Management
```bash
# Clean build
idf.py clean

# Full clean (including config)
idf.py fullclean

# Show project size
idf.py size
```

## Build Configuration
- Uses minimal build configuration (`MINIMAL_BUILD ON`)
- CMake-based build system with component registration
- Supports cross-compilation for multiple ESP32 targets