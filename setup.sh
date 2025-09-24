#!/bin/bash

# Script to setup UDBM and UTAP libraries for TimedAutomata examples
# This script will clone both libraries if not present and build them

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UDBM_DIR="$SCRIPT_DIR/UDBM"
UTAP_DIR="$SCRIPT_DIR/utap"
UPPAAL_DIR="$SCRIPT_DIR/uppaal"
echo "Setting up UDBM and UTAP libraries..."




# get one argument called --no_uppaal to skip the uppaal setup
NO_UPPAAL=0
for arg in "$@"; do
    if [ "$arg" == "--no_uppaal" ]; then
        NO_UPPAAL=1
    fi
done




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



# Setup UTAP
echo "=== Setting up UPPAAL ==="
# Skip UPPAAL setup if --no_uppaal argument is provided
if [ "$NO_UPPAAL" -eq 0 ]; then
    # Check if UTAP directory exists
    if [ ! -d "$UPPAAL_DIR" ]; then
        echo "UPPAAL directory not found. Cloning from GitHub..."
        wget "https://download.uppaal.org/uppaal-5.0/uppaal-5.0.0/uppaal-5.0.0-linux64.zip"
    fi

    cd "$UPPAAL_DIR"

    # unzip the file
    unzip uppaal-5.0.0-linux64.zip 


    cd "uppaal-5.0.0-linux64"

    #ask for input for the uppaal license and save it in a variable
    read -p "Please enter the UPPAAL license key: " UPPAAL_LICENSE_KEY

    bin/verifyta --lease 168 --key "$UPPAAL_LICENSE_KEY"

    echo "Setting VERIFYTA_PATH environment variable as follows:"
    echo "export VERIFYTA_PATH=\"$UPPAAL_DIR/uppaal-5.0.0-linux64/bin/verifyta\""

    CONFIG_FILE="$HOME/.bashrc"

    # Check if it's already there

# Add to bashrc if not already there
    if ! grep -q "export VERIFYTA_PATH=" "$CONFIG_FILE"; then
        echo "export VERIFYTA_PATH=\"$UPPAAL_DIR/uppaal-5.0.0-linux64/bin/verifyta\"" >> "$CONFIG_FILE"
        echo "Added VERIFYTA_PATH to $CONFIG_FILE"
    else
        echo "VERIFYTA_PATH already exists in $CONFIG_FILE"
    fi
    source "$CONFIG_FILE"
    #export VERIFYTA_PATH="$UPPAAL_DIR/uppaal-5.0.0-linux64/bin/verifyta"
else
    echo "Skipping UPPAAL setup as per user request."
fi

echo "=== Setup Complete ==="
echo "Both UDBM and UTAP libraries are ready!"
echo "You can now build the examples with:"
echo "mkdir -p build && cd build && cmake .. && make"
