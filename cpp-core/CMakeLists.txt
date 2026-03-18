cmake_minimum_required(VERSION 3.20)
project(trading_gateway LANGUAGES CXX)

# Настройки стандарта C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск потоков (Threads)
find_package(Threads REQUIRED)

# --- 1. Lock-free queue library ---
add_library(lockfree_queue STATIC
    lockfree_queue/lockfree_queue.h
    lockfree_queue/lockfree_queue.cpp
)
# Настройка инклюдов: PUBLIC позволяет зависимым целям видеть эти хедеры
target_include_directories(lockfree_queue PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lockfree_queue>
)
target_link_libraries(lockfree_queue PRIVATE Threads::Threads)

# --- 2. Network library ---
add_library(network STATIC
    network/tcp_server.h
    network/tcp_server.cpp
    network/message_parser.h
    network/message_parser.cpp
    network/statsd_client.h
    network/statsd_client.cpp
)
target_include_directories(network PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network>
)
target_link_libraries(network PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(network PUBLIC Ws2_32)
endif()

# --- 3. Matching engine library ---
add_library(matching_engine STATIC
    matching_engine/order_book.h
    matching_engine/order_book.cpp
    matching_engine/matching_engine.h
    matching_engine/matching_engine.cpp
)
target_include_directories(matching_engine PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/matching_engine>
)
# matching_engine зависит от lockfree_queue
target_link_libraries(matching_engine 
    PUBLIC lockfree_queue 
    PRIVATE Threads::Threads
)

# --- 4. Order gateway executable ---
add_executable(order_gateway
    order_gateway/main.cpp
    order_gateway/gateway_server.h
    order_gateway/gateway_server.cpp
)
target_include_directories(order_gateway PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/order_gateway
)
# Линкуем только прямые зависимости (транзитивность сработает сама)
target_link_libraries(order_gateway PRIVATE 
    network 
    matching_engine
    lockfree_queue
)

# --- Флаги компиляции (Target-specific) ---
if(MSVC)
    # Флаги только для Debug: включаем проверки RTC1 и отключаем оптимизацию
    target_compile_options(order_gateway PRIVATE 
        $<$<CONFIG:Debug>:/Od /RTC1>
    )

    # Флаги только для Release: включаем оптимизацию O2, что автоматически требует отключения RTC1
    target_compile_options(order_gateway PRIVATE 
        $<$<CONFIG:Release>:/O2 /Oi /Ot>
    )
else()
    # Для GCC/Clang оставляем ваши прежние настройки
    target_compile_options(order_gateway PRIVATE 
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g -O0>
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(order_gateway PRIVATE -pthread)
        target_link_options(order_gateway PRIVATE -pthread)
    endif()
endif()

# Установка
include(GNUInstallDirs)
install(TARGETS order_gateway DESTINATION ${CMAKE_INSTALL_BINDIR})