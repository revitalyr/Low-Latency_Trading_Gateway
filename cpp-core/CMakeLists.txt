cmake_minimum_required(VERSION 3.20)
project(trading_gateway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lockfree_queue
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/matching_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/order_gateway
)

# Lock-free queue library
add_library(lockfree_queue STATIC
    lockfree_queue/lockfree_queue.h
    lockfree_queue/lockfree_queue.cpp
)

# Network library
add_library(network STATIC
    network/tcp_server.h
    network/tcp_server.cpp
    network/message_parser.h
    network/message_parser.cpp
)

# Matching engine library
add_library(matching_engine STATIC
    matching_engine/order_book.h
    matching_engine/order_book.cpp
    matching_engine/matching_engine.h
    matching_engine/matching_engine.cpp
)

# Order gateway executable
add_executable(order_gateway
    order_gateway/main.cpp
    order_gateway/gateway_server.h
    order_gateway/gateway_server.cpp
)

# Link libraries
target_link_libraries(lockfree_queue Threads::Threads)
target_link_libraries(network Threads::Threads)
target_link_libraries(matching_engine lockfree_queue Threads::Threads)
target_link_libraries(order_gateway network matching_engine lockfree_queue Threads::Threads)

# Platform-specific optimizations
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(order_gateway PRIVATE -pthread)
    target_link_options(order_gateway PRIVATE -pthread)
endif()

# Install targets
install(TARGETS order_gateway DESTINATION bin)
