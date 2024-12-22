cmake_minimum_required(VERSION 3.22)
project(http_proxy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Werror)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
        src/main.cpp
        src/proxy_app.cpp
        src/listener.cpp
        src/thread_pool.cpp
        src/http_parser.cpp
        src/connection_handler.cpp
        src/redirect_handler.cpp
        src/signal_handler.cpp
        src/logger.cpp
        src/utils.cpp
)

add_executable(http_proxy ${SOURCES})
