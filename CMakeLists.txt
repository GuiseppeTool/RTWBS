cmake_minimum_required(VERSION 3.16)
project(TimedAutomataExamples)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

# Collect all source files in src directory
file(GLOB_RECURSE RTWBS_SOURCES "src/*.cpp")
file(GLOB_RECURSE RTWBS_HEADERS "src/*.h" "src/*.hpp")

# Create the rtwbs library
add_library(rtwbs STATIC ${RTWBS_SOURCES} ${RTWBS_HEADERS})
target_include_directories(rtwbs PUBLIC 
    ${COMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(rtwbs PUBLIC ${UDBM_LIBRARIES} ${UTAP_LIBRARIES})

# Function to create a UDBM example executable
function(add_udbm_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
endfunction()

# Function to create a UTAP example executable
function(add_utap_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
endfunction()

# Function to create an integrated example executable (needs both UTAP and UDBM)
function(add_integration_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example/${EXAMPLE_NAME}.cpp)
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${EXAMPLE_NAME} rtwbs)
endfunction()

# Create UDBM example executables
add_udbm_example(simple_example)
add_udbm_example(test_example)

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

message(STATUS "UDBM Library: ${UDBM_LIBRARY}")
message(STATUS "UUtils Base: ${UUTILS_BASE_LIBRARY}")
message(STATUS "UUtils Hash: ${UUTILS_HASH_LIBRARY}")
message(STATUS "UUtils Debug: ${UUTILS_DEBUG_LIBRARY}")
message(STATUS "UTAP Library: ${UTAP_LIBRARY}")
message(STATUS "XML2 Library: ${XML2_LIBRARY}")
message(STATUS "Y Library: ${Y_LIBRARY}")

# Add a custom target to build all examples
add_custom_target(examples 
    DEPENDS simple_example test_example railway_example zonegraph_example utap_example integration_example integration_example2 rtwbs_example
)
