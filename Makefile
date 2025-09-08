# Makefile to simplify the build process.

# The build directories
BUILD_DIR = build
RELEASE_DIR = release

# Get the number of processor cores
NPROC = $(shell nproc)

# Default target - development build
all: $(BUILD_DIR)/Makefile
	@echo "--- Building in $(BUILD_DIR) with $(NPROC) cores ---"
	$(MAKE) -C $(BUILD_DIR) -j$(NPROC)

# Release target - optimized build without debug output
release: $(RELEASE_DIR)/Makefile
	@echo "--- Building RELEASE in $(RELEASE_DIR) with $(NPROC) cores ---"
	@echo "--- Release build: DEV_MODE=OFF, optimizations enabled ---"
	$(MAKE) -C $(RELEASE_DIR) -j$(NPROC)

# Rule to run cmake and generate the Makefile in the build directory (development)
$(BUILD_DIR)/Makefile: CMakeLists.txt
	@echo "--- Configuring DEVELOPMENT build with CMake ---"
	@mkdir -p $(BUILD_DIR)
	@cmake -S . -B $(BUILD_DIR) -DDEV_MODE=ON

# Rule to run cmake and generate the Makefile in the release directory (optimized)
$(RELEASE_DIR)/Makefile: CMakeLists.txt
	@echo "--- Configuring RELEASE build with CMake ---"
	@mkdir -p $(RELEASE_DIR)
	@cmake -S . -B $(RELEASE_DIR) \
		-DDEV_MODE=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto" \
		-DCMAKE_EXE_LINKER_FLAGS_RELEASE="-flto"

# A 'clean' target to remove the build directory
clean: clean-debug clean-release

# A 'clean-release' target to remove the release directory
clean-release:
	@echo "--- Cleaning release directory ---"
	@rm -rf $(RELEASE_DIR)

# Clean everything
clean-debug: 
	@echo "--- Cleaning debug directory ---"
	@rm -rf $(BUILD_DIR)
# Rebuild target to force cmake and then make
rebuild: clean all

# Rebuild release target
rebuild-release: clean-release release



# Phony targets
.PHONY: all clean clean-release clean-debug rebuild rebuild-release release

example/debug_test9.o: example/debug_test9.cpp.o
.PHONY : example/debug_test9.o

# target to build an object file
example/debug_test9.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/debug_test9.dir/build.make CMakeFiles/debug_test9.dir/example/debug_test9.cpp.o
.PHONY : example/debug_test9.cpp.o

example/debug_test9.i: example/debug_test9.cpp.i
.PHONY : example/debug_test9.i

# target to preprocess a source file
example/debug_test9.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/debug_test9.dir/build.make CMakeFiles/debug_test9.dir/example/debug_test9.cpp.i
.PHONY : example/debug_test9.cpp.i

example/debug_test9.s: example/debug_test9.cpp.s
.PHONY : example/debug_test9.s

# target to generate assembly for a file
example/debug_test9.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/debug_test9.dir/build.make CMakeFiles/debug_test9.dir/example/debug_test9.cpp.s
.PHONY : example/debug_test9.cpp.s

example/integration_example.o: example/integration_example.cpp.o
.PHONY : example/integration_example.o

# target to build an object file
example/integration_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example.dir/build.make CMakeFiles/integration_example.dir/example/integration_example.cpp.o
.PHONY : example/integration_example.cpp.o

example/integration_example.i: example/integration_example.cpp.i
.PHONY : example/integration_example.i

# target to preprocess a source file
example/integration_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example.dir/build.make CMakeFiles/integration_example.dir/example/integration_example.cpp.i
.PHONY : example/integration_example.cpp.i

example/integration_example.s: example/integration_example.cpp.s
.PHONY : example/integration_example.s

# target to generate assembly for a file
example/integration_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example.dir/build.make CMakeFiles/integration_example.dir/example/integration_example.cpp.s
.PHONY : example/integration_example.cpp.s

example/integration_example_2.o: example/integration_example_2.cpp.o
.PHONY : example/integration_example_2.o

# target to build an object file
example/integration_example_2.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example_2.dir/build.make CMakeFiles/integration_example_2.dir/example/integration_example_2.cpp.o
.PHONY : example/integration_example_2.cpp.o

example/integration_example_2.i: example/integration_example_2.cpp.i
.PHONY : example/integration_example_2.i

# target to preprocess a source file
example/integration_example_2.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example_2.dir/build.make CMakeFiles/integration_example_2.dir/example/integration_example_2.cpp.i
.PHONY : example/integration_example_2.cpp.i

example/integration_example_2.s: example/integration_example_2.cpp.s
.PHONY : example/integration_example_2.s

# target to generate assembly for a file
example/integration_example_2.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/integration_example_2.dir/build.make CMakeFiles/integration_example_2.dir/example/integration_example_2.cpp.s
.PHONY : example/integration_example_2.cpp.s

example/memory_debug_minimal.o: example/memory_debug_minimal.cpp.o
.PHONY : example/memory_debug_minimal.o

# target to build an object file
example/memory_debug_minimal.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_debug_minimal.dir/build.make CMakeFiles/memory_debug_minimal.dir/example/memory_debug_minimal.cpp.o
.PHONY : example/memory_debug_minimal.cpp.o

example/memory_debug_minimal.i: example/memory_debug_minimal.cpp.i
.PHONY : example/memory_debug_minimal.i

# target to preprocess a source file
example/memory_debug_minimal.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_debug_minimal.dir/build.make CMakeFiles/memory_debug_minimal.dir/example/memory_debug_minimal.cpp.i
.PHONY : example/memory_debug_minimal.cpp.i

example/memory_debug_minimal.s: example/memory_debug_minimal.cpp.s
.PHONY : example/memory_debug_minimal.s

# target to generate assembly for a file
example/memory_debug_minimal.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_debug_minimal.dir/build.make CMakeFiles/memory_debug_minimal.dir/example/memory_debug_minimal.cpp.s
.PHONY : example/memory_debug_minimal.cpp.s

example/memory_leak_test.o: example/memory_leak_test.cpp.o
.PHONY : example/memory_leak_test.o

# target to build an object file
example/memory_leak_test.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_leak_test.dir/build.make CMakeFiles/memory_leak_test.dir/example/memory_leak_test.cpp.o
.PHONY : example/memory_leak_test.cpp.o

example/memory_leak_test.i: example/memory_leak_test.cpp.i
.PHONY : example/memory_leak_test.i

# target to preprocess a source file
example/memory_leak_test.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_leak_test.dir/build.make CMakeFiles/memory_leak_test.dir/example/memory_leak_test.cpp.i
.PHONY : example/memory_leak_test.cpp.i

example/memory_leak_test.s: example/memory_leak_test.cpp.s
.PHONY : example/memory_leak_test.s

# target to generate assembly for a file
example/memory_leak_test.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/memory_leak_test.dir/build.make CMakeFiles/memory_leak_test.dir/example/memory_leak_test.cpp.s
.PHONY : example/memory_leak_test.cpp.s

example/minimal_test.o: example/minimal_test.cpp.o
.PHONY : example/minimal_test.o

# target to build an object file
example/minimal_test.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/minimal_test.dir/build.make CMakeFiles/minimal_test.dir/example/minimal_test.cpp.o
.PHONY : example/minimal_test.cpp.o

example/minimal_test.i: example/minimal_test.cpp.i
.PHONY : example/minimal_test.i

# target to preprocess a source file
example/minimal_test.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/minimal_test.dir/build.make CMakeFiles/minimal_test.dir/example/minimal_test.cpp.i
.PHONY : example/minimal_test.cpp.i

example/minimal_test.s: example/minimal_test.cpp.s
.PHONY : example/minimal_test.s

# target to generate assembly for a file
example/minimal_test.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/minimal_test.dir/build.make CMakeFiles/minimal_test.dir/example/minimal_test.cpp.s
.PHONY : example/minimal_test.cpp.s

example/railway_example.o: example/railway_example.cpp.o
.PHONY : example/railway_example.o

# target to build an object file
example/railway_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/railway_example.dir/build.make CMakeFiles/railway_example.dir/example/railway_example.cpp.o
.PHONY : example/railway_example.cpp.o

example/railway_example.i: example/railway_example.cpp.i
.PHONY : example/railway_example.i

# target to preprocess a source file
example/railway_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/railway_example.dir/build.make CMakeFiles/railway_example.dir/example/railway_example.cpp.i
.PHONY : example/railway_example.cpp.i

example/railway_example.s: example/railway_example.cpp.s
.PHONY : example/railway_example.s

# target to generate assembly for a file
example/railway_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/railway_example.dir/build.make CMakeFiles/railway_example.dir/example/railway_example.cpp.s
.PHONY : example/railway_example.cpp.s

example/rtwbs_example.o: example/rtwbs_example.cpp.o
.PHONY : example/rtwbs_example.o

# target to build an object file
example/rtwbs_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs_example.dir/build.make CMakeFiles/rtwbs_example.dir/example/rtwbs_example.cpp.o
.PHONY : example/rtwbs_example.cpp.o

example/rtwbs_example.i: example/rtwbs_example.cpp.i
.PHONY : example/rtwbs_example.i

# target to preprocess a source file
example/rtwbs_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs_example.dir/build.make CMakeFiles/rtwbs_example.dir/example/rtwbs_example.cpp.i
.PHONY : example/rtwbs_example.cpp.i

example/rtwbs_example.s: example/rtwbs_example.cpp.s
.PHONY : example/rtwbs_example.s

# target to generate assembly for a file
example/rtwbs_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs_example.dir/build.make CMakeFiles/rtwbs_example.dir/example/rtwbs_example.cpp.s
.PHONY : example/rtwbs_example.cpp.s

example/run_unit_tests.o: example/run_unit_tests.cpp.o
.PHONY : example/run_unit_tests.o

# target to build an object file
example/run_unit_tests.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/run_unit_tests.dir/build.make CMakeFiles/run_unit_tests.dir/example/run_unit_tests.cpp.o
.PHONY : example/run_unit_tests.cpp.o

example/run_unit_tests.i: example/run_unit_tests.cpp.i
.PHONY : example/run_unit_tests.i

# target to preprocess a source file
example/run_unit_tests.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/run_unit_tests.dir/build.make CMakeFiles/run_unit_tests.dir/example/run_unit_tests.cpp.i
.PHONY : example/run_unit_tests.cpp.i

example/run_unit_tests.s: example/run_unit_tests.cpp.s
.PHONY : example/run_unit_tests.s

# target to generate assembly for a file
example/run_unit_tests.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/run_unit_tests.dir/build.make CMakeFiles/run_unit_tests.dir/example/run_unit_tests.cpp.s
.PHONY : example/run_unit_tests.cpp.s

example/simple_example.o: example/simple_example.cpp.o
.PHONY : example/simple_example.o

# target to build an object file
example/simple_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_example.dir/build.make CMakeFiles/simple_example.dir/example/simple_example.cpp.o
.PHONY : example/simple_example.cpp.o

example/simple_example.i: example/simple_example.cpp.i
.PHONY : example/simple_example.i

# target to preprocess a source file
example/simple_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_example.dir/build.make CMakeFiles/simple_example.dir/example/simple_example.cpp.i
.PHONY : example/simple_example.cpp.i

example/simple_example.s: example/simple_example.cpp.s
.PHONY : example/simple_example.s

# target to generate assembly for a file
example/simple_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_example.dir/build.make CMakeFiles/simple_example.dir/example/simple_example.cpp.s
.PHONY : example/simple_example.cpp.s

example/simple_sync_test.o: example/simple_sync_test.cpp.o
.PHONY : example/simple_sync_test.o

# target to build an object file
example/simple_sync_test.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_sync_test.dir/build.make CMakeFiles/simple_sync_test.dir/example/simple_sync_test.cpp.o
.PHONY : example/simple_sync_test.cpp.o

example/simple_sync_test.i: example/simple_sync_test.cpp.i
.PHONY : example/simple_sync_test.i

# target to preprocess a source file
example/simple_sync_test.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_sync_test.dir/build.make CMakeFiles/simple_sync_test.dir/example/simple_sync_test.cpp.i
.PHONY : example/simple_sync_test.cpp.i

example/simple_sync_test.s: example/simple_sync_test.cpp.s
.PHONY : example/simple_sync_test.s

# target to generate assembly for a file
example/simple_sync_test.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/simple_sync_test.dir/build.make CMakeFiles/simple_sync_test.dir/example/simple_sync_test.cpp.s
.PHONY : example/simple_sync_test.cpp.s

example/single_transition_test.o: example/single_transition_test.cpp.o
.PHONY : example/single_transition_test.o

# target to build an object file
example/single_transition_test.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/single_transition_test.dir/build.make CMakeFiles/single_transition_test.dir/example/single_transition_test.cpp.o
.PHONY : example/single_transition_test.cpp.o

example/single_transition_test.i: example/single_transition_test.cpp.i
.PHONY : example/single_transition_test.i

# target to preprocess a source file
example/single_transition_test.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/single_transition_test.dir/build.make CMakeFiles/single_transition_test.dir/example/single_transition_test.cpp.i
.PHONY : example/single_transition_test.cpp.i

example/single_transition_test.s: example/single_transition_test.cpp.s
.PHONY : example/single_transition_test.s

# target to generate assembly for a file
example/single_transition_test.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/single_transition_test.dir/build.make CMakeFiles/single_transition_test.dir/example/single_transition_test.cpp.s
.PHONY : example/single_transition_test.cpp.s

example/synchronization_example.o: example/synchronization_example.cpp.o
.PHONY : example/synchronization_example.o

# target to build an object file
example/synchronization_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/synchronization_example.dir/build.make CMakeFiles/synchronization_example.dir/example/synchronization_example.cpp.o
.PHONY : example/synchronization_example.cpp.o

example/synchronization_example.i: example/synchronization_example.cpp.i
.PHONY : example/synchronization_example.i

# target to preprocess a source file
example/synchronization_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/synchronization_example.dir/build.make CMakeFiles/synchronization_example.dir/example/synchronization_example.cpp.i
.PHONY : example/synchronization_example.cpp.i

example/synchronization_example.s: example/synchronization_example.cpp.s
.PHONY : example/synchronization_example.s

# target to generate assembly for a file
example/synchronization_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/synchronization_example.dir/build.make CMakeFiles/synchronization_example.dir/example/synchronization_example.cpp.s
.PHONY : example/synchronization_example.cpp.s

example/system_rtwbs_example.o: example/system_rtwbs_example.cpp.o
.PHONY : example/system_rtwbs_example.o

# target to build an object file
example/system_rtwbs_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/system_rtwbs_example.dir/build.make CMakeFiles/system_rtwbs_example.dir/example/system_rtwbs_example.cpp.o
.PHONY : example/system_rtwbs_example.cpp.o

example/system_rtwbs_example.i: example/system_rtwbs_example.cpp.i
.PHONY : example/system_rtwbs_example.i

# target to preprocess a source file
example/system_rtwbs_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/system_rtwbs_example.dir/build.make CMakeFiles/system_rtwbs_example.dir/example/system_rtwbs_example.cpp.i
.PHONY : example/system_rtwbs_example.cpp.i

example/system_rtwbs_example.s: example/system_rtwbs_example.cpp.s
.PHONY : example/system_rtwbs_example.s

# target to generate assembly for a file
example/system_rtwbs_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/system_rtwbs_example.dir/build.make CMakeFiles/system_rtwbs_example.dir/example/system_rtwbs_example.cpp.s
.PHONY : example/system_rtwbs_example.cpp.s

example/test_example.o: example/test_example.cpp.o
.PHONY : example/test_example.o

# target to build an object file
example/test_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/test_example.dir/build.make CMakeFiles/test_example.dir/example/test_example.cpp.o
.PHONY : example/test_example.cpp.o

example/test_example.i: example/test_example.cpp.i
.PHONY : example/test_example.i

# target to preprocess a source file
example/test_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/test_example.dir/build.make CMakeFiles/test_example.dir/example/test_example.cpp.i
.PHONY : example/test_example.cpp.i

example/test_example.s: example/test_example.cpp.s
.PHONY : example/test_example.s

# target to generate assembly for a file
example/test_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/test_example.dir/build.make CMakeFiles/test_example.dir/example/test_example.cpp.s
.PHONY : example/test_example.cpp.s

example/ultra_minimal_test.o: example/ultra_minimal_test.cpp.o
.PHONY : example/ultra_minimal_test.o

# target to build an object file
example/ultra_minimal_test.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/ultra_minimal_test.dir/build.make CMakeFiles/ultra_minimal_test.dir/example/ultra_minimal_test.cpp.o
.PHONY : example/ultra_minimal_test.cpp.o

example/ultra_minimal_test.i: example/ultra_minimal_test.cpp.i
.PHONY : example/ultra_minimal_test.i

# target to preprocess a source file
example/ultra_minimal_test.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/ultra_minimal_test.dir/build.make CMakeFiles/ultra_minimal_test.dir/example/ultra_minimal_test.cpp.i
.PHONY : example/ultra_minimal_test.cpp.i

example/ultra_minimal_test.s: example/ultra_minimal_test.cpp.s
.PHONY : example/ultra_minimal_test.s

# target to generate assembly for a file
example/ultra_minimal_test.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/ultra_minimal_test.dir/build.make CMakeFiles/ultra_minimal_test.dir/example/ultra_minimal_test.cpp.s
.PHONY : example/ultra_minimal_test.cpp.s

example/unit_test_example.o: example/unit_test_example.cpp.o
.PHONY : example/unit_test_example.o

# target to build an object file
example/unit_test_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/unit_test_example.dir/build.make CMakeFiles/unit_test_example.dir/example/unit_test_example.cpp.o
.PHONY : example/unit_test_example.cpp.o

example/unit_test_example.i: example/unit_test_example.cpp.i
.PHONY : example/unit_test_example.i

# target to preprocess a source file
example/unit_test_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/unit_test_example.dir/build.make CMakeFiles/unit_test_example.dir/example/unit_test_example.cpp.i
.PHONY : example/unit_test_example.cpp.i

example/unit_test_example.s: example/unit_test_example.cpp.s
.PHONY : example/unit_test_example.s

# target to generate assembly for a file
example/unit_test_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/unit_test_example.dir/build.make CMakeFiles/unit_test_example.dir/example/unit_test_example.cpp.s
.PHONY : example/unit_test_example.cpp.s

example/utap_example.o: example/utap_example.cpp.o
.PHONY : example/utap_example.o

# target to build an object file
example/utap_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/utap_example.dir/build.make CMakeFiles/utap_example.dir/example/utap_example.cpp.o
.PHONY : example/utap_example.cpp.o

example/utap_example.i: example/utap_example.cpp.i
.PHONY : example/utap_example.i

# target to preprocess a source file
example/utap_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/utap_example.dir/build.make CMakeFiles/utap_example.dir/example/utap_example.cpp.i
.PHONY : example/utap_example.cpp.i

example/utap_example.s: example/utap_example.cpp.s
.PHONY : example/utap_example.s

# target to generate assembly for a file
example/utap_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/utap_example.dir/build.make CMakeFiles/utap_example.dir/example/utap_example.cpp.s
.PHONY : example/utap_example.cpp.s

example/zonegraph_example.o: example/zonegraph_example.cpp.o
.PHONY : example/zonegraph_example.o

# target to build an object file
example/zonegraph_example.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/zonegraph_example.dir/build.make CMakeFiles/zonegraph_example.dir/example/zonegraph_example.cpp.o
.PHONY : example/zonegraph_example.cpp.o

example/zonegraph_example.i: example/zonegraph_example.cpp.i
.PHONY : example/zonegraph_example.i

# target to preprocess a source file
example/zonegraph_example.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/zonegraph_example.dir/build.make CMakeFiles/zonegraph_example.dir/example/zonegraph_example.cpp.i
.PHONY : example/zonegraph_example.cpp.i

example/zonegraph_example.s: example/zonegraph_example.cpp.s
.PHONY : example/zonegraph_example.s

# target to generate assembly for a file
example/zonegraph_example.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/zonegraph_example.dir/build.make CMakeFiles/zonegraph_example.dir/example/zonegraph_example.cpp.s
.PHONY : example/zonegraph_example.cpp.s

src/rtwbs.o: src/rtwbs.cpp.o
.PHONY : src/rtwbs.o

# target to build an object file
src/rtwbs.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/rtwbs.cpp.o
.PHONY : src/rtwbs.cpp.o

src/rtwbs.i: src/rtwbs.cpp.i
.PHONY : src/rtwbs.i

# target to preprocess a source file
src/rtwbs.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/rtwbs.cpp.i
.PHONY : src/rtwbs.cpp.i

src/rtwbs.s: src/rtwbs.cpp.s
.PHONY : src/rtwbs.s

# target to generate assembly for a file
src/rtwbs.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/rtwbs.cpp.s
.PHONY : src/rtwbs.cpp.s

src/system.o: src/system.cpp.o
.PHONY : src/system.o

# target to build an object file
src/system.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/system.cpp.o
.PHONY : src/system.cpp.o

src/system.i: src/system.cpp.i
.PHONY : src/system.i

# target to preprocess a source file
src/system.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/system.cpp.i
.PHONY : src/system.cpp.i

src/system.s: src/system.cpp.s
.PHONY : src/system.s

# target to generate assembly for a file
src/system.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/system.cpp.s
.PHONY : src/system.cpp.s

src/timedautomaton.o: src/timedautomaton.cpp.o
.PHONY : src/timedautomaton.o

# target to build an object file
src/timedautomaton.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/timedautomaton.cpp.o
.PHONY : src/timedautomaton.cpp.o

src/timedautomaton.i: src/timedautomaton.cpp.i
.PHONY : src/timedautomaton.i

# target to preprocess a source file
src/timedautomaton.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/timedautomaton.cpp.i
.PHONY : src/timedautomaton.cpp.i

src/timedautomaton.s: src/timedautomaton.cpp.s
.PHONY : src/timedautomaton.s

# target to generate assembly for a file
src/timedautomaton.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/rtwbs.dir/build.make CMakeFiles/rtwbs.dir/src/timedautomaton.cpp.s
.PHONY : src/timedautomaton.cpp.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... copy_unit_tests"
	@echo "... examples"
	@echo "... debug_test9"
	@echo "... integration_example"
	@echo "... integration_example_2"
	@echo "... memory_debug_minimal"
	@echo "... memory_leak_test"
	@echo "... minimal_test"
	@echo "... railway_example"
	@echo "... rtwbs"
	@echo "... rtwbs_example"
	@echo "... run_unit_tests"
	@echo "... simple_example"
	@echo "... simple_sync_test"
	@echo "... single_transition_test"
	@echo "... synchronization_example"
	@echo "... system_rtwbs_example"
	@echo "... test_example"
	@echo "... ultra_minimal_test"
	@echo "... unit_test_example"
	@echo "... utap_example"
	@echo "... zonegraph_example"
	@echo "... example/debug_test9.o"
	@echo "... example/debug_test9.i"
	@echo "... example/debug_test9.s"
	@echo "... example/integration_example.o"
	@echo "... example/integration_example.i"
	@echo "... example/integration_example.s"
	@echo "... example/integration_example_2.o"
	@echo "... example/integration_example_2.i"
	@echo "... example/integration_example_2.s"
	@echo "... example/memory_debug_minimal.o"
	@echo "... example/memory_debug_minimal.i"
	@echo "... example/memory_debug_minimal.s"
	@echo "... example/memory_leak_test.o"
	@echo "... example/memory_leak_test.i"
	@echo "... example/memory_leak_test.s"
	@echo "... example/minimal_test.o"
	@echo "... example/minimal_test.i"
	@echo "... example/minimal_test.s"
	@echo "... example/railway_example.o"
	@echo "... example/railway_example.i"
	@echo "... example/railway_example.s"
	@echo "... example/rtwbs_example.o"
	@echo "... example/rtwbs_example.i"
	@echo "... example/rtwbs_example.s"
	@echo "... example/run_unit_tests.o"
	@echo "... example/run_unit_tests.i"
	@echo "... example/run_unit_tests.s"
	@echo "... example/simple_example.o"
	@echo "... example/simple_example.i"
	@echo "... example/simple_example.s"
	@echo "... example/simple_sync_test.o"
	@echo "... example/simple_sync_test.i"
	@echo "... example/simple_sync_test.s"
	@echo "... example/single_transition_test.o"
	@echo "... example/single_transition_test.i"
	@echo "... example/single_transition_test.s"
	@echo "... example/synchronization_example.o"
	@echo "... example/synchronization_example.i"
	@echo "... example/synchronization_example.s"
	@echo "... example/system_rtwbs_example.o"
	@echo "... example/system_rtwbs_example.i"
	@echo "... example/system_rtwbs_example.s"
	@echo "... example/test_example.o"
	@echo "... example/test_example.i"
	@echo "... example/test_example.s"
	@echo "... example/ultra_minimal_test.o"
	@echo "... example/ultra_minimal_test.i"
	@echo "... example/ultra_minimal_test.s"
	@echo "... example/unit_test_example.o"
	@echo "... example/unit_test_example.i"
	@echo "... example/unit_test_example.s"
	@echo "... example/utap_example.o"
	@echo "... example/utap_example.i"
	@echo "... example/utap_example.s"
	@echo "... example/zonegraph_example.o"
	@echo "... example/zonegraph_example.i"
	@echo "... example/zonegraph_example.s"
	@echo "... src/rtwbs.o"
	@echo "... src/rtwbs.i"
	@echo "... src/rtwbs.s"
	@echo "... src/system.o"
	@echo "... src/system.i"
	@echo "... src/system.s"
	@echo "... src/timedautomaton.o"
	@echo "... src/timedautomaton.i"
	@echo "... src/timedautomaton.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

