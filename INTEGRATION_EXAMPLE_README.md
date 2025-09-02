# Timed Automata Examples

This project contains examples demonstrating the use of the UDBM (Uppsala Data Base Manager) and UTAP (Uppsala Timed Automata Parser) libraries for working with timed automata and zones.

## Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler
- Git

## Setup

### Automatic Setup

Run the setup script to automatically clone and build both UDBM and UTAP libraries:

```bash
./setup.sh
```

### Manual Setup

If you prefer to set up the libraries manually:

1. Clone the UDBM repository:
   ```bash
   git clone https://github.com/UPPAALModelChecker/UDBM
   ```

2. Build UDBM:
   ```bash
   cd UDBM
   ./getlibs.sh x86_64-linux
   ./compile.sh x86_64-linux-libs-release
   cd ..
   ```

3. Clone the UTAP repository:
   ```bash
   git clone https://github.com/UPPAALModelChecker/utap
   ```

4. Build UTAP:
   ```bash
   cd utap
   ./getlibs.sh x86_64-linux
   ./compile.sh x86_64-linux
   cd ..
   ```

## Building Examples

1. Create a build directory and configure:
   ```
   mkdir -p build
   cd build
   cmake ..
   ```

2. Build all examples:
   ```
   make
   ```

   Or build individual examples:
   ```
   make simple_example
   make test_example
   make railway_example
   make zonegraph_example
   make utap_example
   ```

3. Build all examples at once using the custom target:
   ```
   make examples
   ```

## Running Examples

After building, you can run the examples from the build directory:

```bash
./simple_example      # Basic DBM operations
./test_example        # Test functionality
./railway_example     # Railway crossing example
./zonegraph_example   # Zone graph operations
./utap_example        # UTAP XML parsing example
```

## Examples Description

- **simple_example**: Demonstrates basic DBM (Difference Bound Matrices) operations
- **test_example**: Contains test cases for various UDBM functionality
- **railway_example**: Models a railway crossing system using timed automata
- **zonegraph_example**: Shows zone graph construction and manipulation
- **utap_example**: Demonstrates XML parsing of timed automata models using UTAP

## Project Structure

```
TimedAutomata/
├── CMakeLists.txt          # Main CMake configuration
├── setup.sh               # Automatic UDBM and UTAP setup script
├── README.md              # This file
├── src/                   # Source headers
│   └── timedautomaton.h
├── example/               # Example source files
│   ├── simple_example.cpp
│   ├── test_example.cpp
│   ├── railway_example.cpp
│   ├── zonegraph_example.cpp
│   └── utap_example.cpp
├── UDBM/                  # UDBM library (auto-downloaded)
│   ├── include/           # UDBM headers
│   ├── build-*/src/       # Built UDBM library
│   └── local/*/lib/       # Dependencies
└── utap/                  # UTAP library (auto-downloaded)
    ├── include/           # UTAP headers
    ├── build-*/src/       # Built UTAP library
    └── local/*/lib/       # Dependencies
```

## Troubleshooting

If you encounter build issues:

1. Make sure both UDBM and UTAP are properly built:
   ```bash
   ls UDBM/build-x86_64-linux-release/src/libUDBM.a
   ls utap/build-x86_64-linux-release/src/libUTAP.a
   ```

2. Check that dependencies are present:
   ```bash
   ls UDBM/local/x86_64-linux/lib/
   ls utap/local/x86_64-linux/lib/
   ```

3. Clean and rebuild:
   ```bash
   rm -rf build
   mkdir build && cd build
   cmake .. && make
   ```

## Dependencies

The project uses the following libraries:
- **UDBM**: Core library for DBM operations and timed automata
- **UUtils**: Utility libraries (base, hash, debug)
- **UTAP**: Uppsala Timed Automata Parser for XML model parsing
- **libxml2**: XML parsing library (dependency of UTAP)
- **Boost**: Some components for advanced functionality

All dependencies are automatically managed through the UDBM build system.
