# RTWBS (Relaxed Weak Timed Bisimulation) Implementation

This directory contains an implementation of RTWBS equivalence checking for timed automata, based on the research described in the ICSE_DT paper.

IMPORTANT: Branch points are not implemented
## Overview

RTWBS (Relaxed Weak Timed Bisimulation) is a formal equivalence relation that allows for:

- **Relaxed timing constraints** on received events (events ending with `?`)
- **Strict timing constraints** on sent events (events ending with `!`)

This is particularly useful for modeling distributed systems where:
- Message reception can be delayed (network delays, processing delays)
- Message sending must respect original timing requirements for correctness

## Files

- `src/rtwbs.h` - Header file with RTWBS checker interface
- `src/rtwbs.cpp` - Implementation of RTWBS equivalence checking
- `example/rtwbs_example.cpp` - Example demonstrating RTWBS equivalence checking

## Key Concepts

### RTWBS Rules

1. **Received Events (`event?`)**: The refined automaton can have MORE relaxed timing than the abstract automaton
2. **Sent Events (`event!`)**: The refined automaton must have SAME or STRICTER timing than the abstract automaton

### Example

```cpp
// Abstract automaton (PT - Physical Time)
send_data! within 10 time units

// Valid RTWBS refinement (DT - Distributed Time)  
trigger? within 15 time units (relaxed - OK for received events)
send_data! within 8 time units (stricter - OK for sent events)

// Invalid RTWBS refinement
send_data! within 15 time units (more relaxed - INVALID for sent events)
```

## Usage

```cpp
#include "rtwbs.h"

// Create your timed automata
TimedAutomaton abstract(2);  // Physical Time model
TimedAutomaton refined(2);   // Distributed Time model

// Set up automata with locations and transitions...

// Check RTWBS equivalence
rtwbs::RTWBSChecker checker;
bool is_equivalent = checker.check_rtwbs_equivalence(refined, abstract);

if (is_equivalent) {
    std::cout << "The refined automaton is a valid RTWBS refinement!" << std::endl;
} else {
    std::cout << "The refined automaton violates RTWBS constraints." << std::endl;
}
```

## Building and Running all the benchmarks

```bash

#create python environment with venv, install requirements.txt
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

python3 generate_syn_benchmarks.py


./setup.sh

# Build the RTWBS 
make release
#run all benchmarks
./run_all && ./run_synthetic


#-----ALTERNATIVELY-----
# Run the benchmarks with 3 workers and save them to folder results_3
./run_all --n-workers 3 --folder results_3 && ./run_synthetic --n-workers 3 --folder results_3


```

