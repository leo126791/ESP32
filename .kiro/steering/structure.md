# Project Structure

## Directory Layout
```
├── .devcontainer/          # Docker development environment
│   ├── devcontainer.json   # VS Code devcontainer configuration
│   └── Dockerfile          # ESP-IDF development image
├── .kiro/                  # Kiro AI assistant configuration
│   └── steering/           # AI guidance rules
├── .vscode/                # VS Code workspace settings
├── main/                   # Main application source code
│   ├── CMakeLists.txt      # Component build configuration
│   └── hello_world_main.c  # Main application entry point
├── CMakeLists.txt          # Root project build configuration
├── pytest_hello_world.py  # Automated test suite
├── README.md               # Project documentation
└── sdkconfig.ci            # CI/CD build configuration
```

## Code Organization

### Main Application (`main/`)
- **Purpose**: Contains the primary application logic
- **Entry Point**: `app_main()` function in `hello_world_main.c`
- **Build**: Registered as ESP-IDF component with CMakeLists.txt
- **Dependencies**: Requires `spi_flash` component

### Root Level Files
- **CMakeLists.txt**: Project-level build configuration with minimal build settings
- **pytest_hello_world.py**: Comprehensive test suite for multiple targets and environments
- **sdkconfig.ci**: Continuous integration build settings

## Naming Conventions

### Files
- C source files: `snake_case.c`
- Header files: `snake_case.h` 
- CMake files: `CMakeLists.txt` (exact case)
- Python test files: `pytest_*.py`

### Functions
- Application entry: `app_main(void)`
- ESP-IDF APIs: `esp_*` prefix
- FreeRTOS APIs: `v*` or `x*` prefix

### Variables
- Local variables: `snake_case`
- Constants: `UPPER_SNAKE_CASE`
- ESP-IDF configs: `CONFIG_*` prefix

## Component Architecture

### ESP-IDF Components
- Each directory with CMakeLists.txt is a component
- Components declare source files and dependencies
- Use `idf_component_register()` for component registration
- Private requirements (`PRIV_REQUIRES`) for internal dependencies

### Build System
- CMake-based with ESP-IDF extensions
- Minimal build configuration to reduce binary size
- Cross-compilation support for multiple ESP32 targets
- Automatic dependency resolution between components