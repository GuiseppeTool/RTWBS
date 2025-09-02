#!/bin/bash

# Script to setup UDBM and UTAP libraries for TimedAutomata examples
# This script will clone both libraries if not present and build them

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UDBM_DIR="$SCRIPT_DIR/UDBM"
UTAP_DIR="$SCRIPT_DIR/utap"

echo "Setting up UDBM and UTAP libraries..."

# Setup UDBM
echo "=== Setting up UDBM ==="
# Check if UDBM directory exists
if [ ! -d "$UDBM_DIR" ]; then
    echo "UDBM directory not found. Cloning from GitHub..."
    git clone https://github.com/UPPAALModelChecker/UDBM "$UDBM_DIR"
fi

cd "$UDBM_DIR"

# Check if library is already built
if [ ! -f "build-x86_64-linux-release/src/libUDBM.a" ]; then
    echo "UDBM library not built. Building now..."
    
    # Get dependencies
    echo "Getting UDBM dependencies..."
    ./getlibs.sh x86_64-linux
    
    # Compile the library
    echo "Compiling UDBM..."
    ./compile.sh x86_64-linux-libs-release
    
    echo "UDBM library built successfully!"
else
    echo "UDBM library already built."
fi

cd "$SCRIPT_DIR"

# Setup UTAP
echo "=== Setting up UTAP ==="
# Check if UTAP directory exists
if [ ! -d "$UTAP_DIR" ]; then
    echo "UTAP directory not found. Cloning from GitHub..."
    git clone https://github.com/UPPAALModelChecker/utap "$UTAP_DIR"
fi

cd "$UTAP_DIR"

# Check if library is already built
if [ ! -f "build-x86_64-linux-release/src/libUTAP.a" ]; then
    echo "UTAP library not built. Building now..."
    
    # Get dependencies
    echo "Getting UTAP dependencies..."
    ./getlibs.sh x86_64-linux
    
    # Compile the library
    echo "Compiling UTAP..."
    ./compile.sh x86_64-linux
    
    echo "UTAP library built successfully!"
else
    echo "UTAP library already built."
fi

echo "=== Setup Complete ==="
echo "Both UDBM and UTAP libraries are ready!"
echo "You can now build the examples with:"
echo "mkdir -p build && cd build && cmake .. && make"
