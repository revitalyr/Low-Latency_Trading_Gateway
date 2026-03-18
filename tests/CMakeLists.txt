cmake_minimum_required(VERSION 3.20)
project(trading_gateway_tests LANGUAGES CXX)

# Внешние зависимости
find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    ut
    GIT_REPOSITORY https://github.com/boost-ext/ut
    GIT_TAG        v1.1.9
)
FetchContent_MakeAvailable(ut)

# Список исходников тестов
set(TEST_SOURCES
    "test_main.cpp"
    "test_lockfree_queue.cpp"
    "test_message_parser.cpp"
    "test_matching_engine.cpp"
    "test_performance.cpp"
    "test_tcp_server.cpp"
    "test_gateway_server.cpp"
)

add_executable(trading_gateway_tests ${TEST_SOURCES})

# Использование транзитивных зависимостей:
# Линкуя matching_engine, мы автоматически получаем инклюды lockfree_queue
target_link_libraries(trading_gateway_tests
    PRIVATE
        ut
        network
        matching_engine
        Threads::Threads
)

# Дополнительные пути инклюдов (если тестам нужны внутренние файлы)
target_include_directories(trading_gateway_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/cpp-core
)

# Регистрация тестов в CTest
include(CTest)
add_test(NAME AllTests COMMAND trading_gateway_tests)
set_tests_properties(AllTests PROPERTIES TIMEOUT 120)