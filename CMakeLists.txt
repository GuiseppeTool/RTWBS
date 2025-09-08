cmake_minimum_required(VERSION 3.16)
project(TimedAutomataExamples)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default installation prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()



# Create subdirectories for examples and benchmarks
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)

# Development mode option (ON by default)
option(DEV_MODE "Enable development mode with debug prints" ON)

if(DEV_MODE)
    message(STATUS "Development mode: ON (debug prints enabled)")
    add_definitions(-DDEV_MODE=1)
else()
    message(STATUS "Development mode: OFF (debug prints disabled)")
    add_definitions(-DDEV_MODE=0)
endif()

# Check if UDBM directory exists, if not provide instructions
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UDBM")
    message(FATAL_ERROR "UDBM directory not found! Please run:\n"
                        "git clone https://github.com/UPPAALModelChecker/UDBM\n"
                        "cd UDBM\n"
                        "./getlibs.sh x86_64-linux\n"
                        "./compile.sh x86_64-linux-libs-release")
endif()

# Check if UDBM is built, if not provide instructions
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UDBM/build-x86_64-linux-release/src/libUDBM.a")
    message(FATAL_ERROR "UDBM library not built! Please run:\n"
                        "cd UDBM\n"
                        "./getlibs.sh x86_64-linux\n"
                        "./compile.sh x86_64-linux-libs-release")
endif()

# Check if UTAP directory exists, if not provide instructions
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utap")
    message(FATAL_ERROR "UTAP directory not found! Please run:\n"
                        "git clone https://github.com/UPPAALModelChecker/utap\n"
                        "cd utap\n"
                        "./getlibs.sh x86_64-linux\n"
                        "./compile.sh x86_64-linux")
endif()

# Check if UTAP is built, if not provide instructions
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utap/build-x86_64-linux-release/src/libUTAP.a")
    message(FATAL_ERROR "UTAP library not built! Please run:\n"
                        "cd utap\n"
                        "./getlibs.sh x86_64-linux\n"
                        "./compile.sh x86_64-linux")
endif()

# Find the UDBM library
find_library(UDBM_LIBRARY 
    NAMES UDBM libUDBM
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/build-x86_64-linux-release/src
    NO_DEFAULT_PATH
)

if(NOT UDBM_LIBRARY)
    message(FATAL_ERROR "UDBM library not found!")
endif()

# Find UUtils libraries (dependencies)
find_library(UUTILS_BASE_LIBRARY 
    NAMES base libbase
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/local/x86_64-linux/lib
    NO_DEFAULT_PATH
)

find_library(UUTILS_HASH_LIBRARY 
    NAMES hash libhash
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/local/x86_64-linux/lib
    NO_DEFAULT_PATH
)

find_library(UUTILS_DEBUG_LIBRARY 
    NAMES udebug libudebug
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/local/x86_64-linux/lib
    NO_DEFAULT_PATH
)

# Find the UTAP library
find_library(UTAP_LIBRARY 
    NAMES UTAP libUTAP
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/utap/build-x86_64-linux-release/src
    NO_DEFAULT_PATH
)

if(NOT UTAP_LIBRARY)
    message(FATAL_ERROR "UTAP library not found!")
endif()

# Find UTAP dependencies
find_library(XML2_LIBRARY 
    NAMES xml2 libxml2
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/utap/local/x86_64-linux/lib
    NO_DEFAULT_PATH
)

find_library(Y_LIBRARY 
    NAMES y liby
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/utap/local/x86_64-linux/lib
    NO_DEFAULT_PATH
)

# Common include directories
set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/include
    ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/build-x86_64-linux-release/include
    ${CMAKE_CURRENT_SOURCE_DIR}/UDBM/local/x86_64-linux/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utap/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utap/build-x86_64-linux-release/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utap/local/x86_64-linux/include
)

# Common libraries for UDBM examples
set(UDBM_LIBRARIES
    ${UDBM_LIBRARY}
    ${UUTILS_BASE_LIBRARY}
    ${UUTILS_HASH_LIBRARY} 
    ${UUTILS_DEBUG_LIBRARY}
)

# Common libraries for UTAP examples
set(UTAP_LIBRARIES
    ${UTAP_LIBRARY}
    ${XML2_LIBRARY}
    ${Y_LIBRARY}
)

# Collect all source files in src directory and header files in include directory
file(GLOB_RECURSE RTWBS_SOURCES "src/*.cpp")
file(GLOB_RECURSE RTWBS_HEADERS "include/*.h" "include/*.hpp")

# Create the rtwbs library
add_library(rtwbs STATIC ${RTWBS_SOURCES} ${RTWBS_HEADERS})

# Set include directories differently for build and install
target_include_directories(rtwbs PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/UDBM/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/UDBM/build-x86_64-linux-release/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/UDBM/local/x86_64-linux/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utap/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utap/build-x86_64-linux-release/src/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utap/local/x86_64-linux/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(rtwbs PUBLIC ${UDBM_LIBRARIES} ${UTAP_LIBRARIES})

# Function to create a UDBM example executable
function(add_udbm_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

# Function to create a UTAP example executable
function(add_utap_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

# Function to create an integrated example executable (needs both UTAP and UDBM)
function(add_integration_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

# Function to add benchmark 
function(add_benchmark BENCHMARK_NAME)
    add_executable(${BENCHMARK_NAME} benchmark/${BENCHMARK_NAME}.cpp)
    target_include_directories(${BENCHMARK_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${BENCHMARK_NAME} rtwbs)
    set_target_properties(${BENCHMARK_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
    )
endfunction()


# Create UDBM example executables
add_udbm_example(simple_example)

# Create integration examples (need both UTAP and UDBM + timedautomaton.cpp)
add_integration_example(railway_example)
add_integration_example(zonegraph_example)

# Create UTAP example executables
add_utap_example(utap_example)

# Create integration example executable
add_integration_example(integration_example)

add_integration_example(integration_example_2)

add_integration_example(synchronization_example)

add_integration_example(simple_sync_test)

add_integration_example(minimal_test)

add_integration_example(single_transition_test)

add_integration_example(rtwbs_example)
add_integration_example(system_rtwbs_example)
add_integration_example(unit_test_example)
add_integration_example(run_unit_tests)
add_integration_example(run_automata)
add_integration_example(run_uppaal_benchmarks)
# Copy assets directory to build directory
# Must be able to specify which subfolder according to the target

add_custom_target(copy_assets_examples ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_BINARY_DIR}/examples/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMENT "Copying assets directory to examples directory and build root"
)

add_custom_target(copy_assets_benchmarks ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_BINARY_DIR}/benchmarks/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMENT "Copying assets directory to benchmarks directory and build root"
)






add_benchmark(run_all)
add_benchmark(run_ASTRail)
add_benchmark(run_FMICS2021)


# Make run_unit_tests depend on copying unit tests
add_dependencies(run_unit_tests copy_assets_examples)
add_dependencies(run_uppaal_benchmarks copy_assets_examples)

# Make benchmarks depend on copying assets
add_dependencies(run_all copy_assets_benchmarks)
add_dependencies(run_ASTRail copy_assets_benchmarks)
add_dependencies(run_FMICS2021 copy_assets_benchmarks)



message(STATUS "UDBM Library: ${UDBM_LIBRARY}")
message(STATUS "UUtils Base: ${UUTILS_BASE_LIBRARY}")
message(STATUS "UUtils Hash: ${UUTILS_HASH_LIBRARY}")
message(STATUS "UUtils Debug: ${UUTILS_DEBUG_LIBRARY}")
message(STATUS "UTAP Library: ${UTAP_LIBRARY}")
message(STATUS "XML2 Library: ${XML2_LIBRARY}")
message(STATUS "Y Library: ${Y_LIBRARY}")

# Add a custom target to build all examples
add_custom_target(examples 
    DEPENDS simple_example railway_example zonegraph_example utap_example integration_example integration_example_2 rtwbs_example system_rtwbs_example unit_test_example run_unit_tests
)

# Add a custom target to build all benchmarks
add_custom_target(benchmarks 
    DEPENDS run_all run_ASTRail run_FMICS2021
)

add_dependencies(run_all copy_assets_benchmarks)
add_dependencies(run_ASTRail copy_assets_benchmarks)
add_dependencies(run_FMICS2021 copy_assets_benchmarks)


# Install configuration
# Install the static library
install(TARGETS rtwbs
    EXPORT RTWBSTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Export targets
install(EXPORT RTWBSTargets
    FILE RTWBSTargets.cmake
    NAMESPACE RTWBS::
    DESTINATION lib/cmake/RTWBS
)

# Install all header files preserving directory structure
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install examples (optional - uncomment if you want to install examples)
# install(DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/examples/
#     DESTINATION bin/examples
#     FILES_MATCHING PATTERN "*"
#     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
# )

# Install benchmarks (optional - uncomment if you want to install benchmarks)  
# install(DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/benchmarks/
#     DESTINATION bin/benchmarks
#     FILES_MATCHING PATTERN "*"
#     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
# )

# Create a find package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "RTWBSConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RTWBSConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RTWBSConfig.cmake"
    INSTALL_DESTINATION lib/cmake/RTWBS
)

# Install the config files (create the cmake directory first)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/RTWBSConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/RTWBSConfigVersion.cmake"
    DESTINATION lib/cmake/RTWBS
)

message(STATUS "Installation directory: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "To install, run: make install")
message(STATUS "To change install directory, use: cmake -DCMAKE_INSTALL_PREFIX=/your/path ..")
