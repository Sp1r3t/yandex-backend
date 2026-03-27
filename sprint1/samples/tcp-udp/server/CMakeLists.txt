cmake_minimum_required(VERSION 3.16)

project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost (header-only часть нам достаточно)
find_package(Boost REQUIRED)

# Потоки (обязательно для asio!)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(server server.cpp)

target_link_libraries(server PRIVATE Boost::boost Threads::Threads)
