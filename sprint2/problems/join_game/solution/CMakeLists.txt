cmake_minimum_required(VERSION 3.16)

project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Boost REQUIRED COMPONENTS log log_setup system)

add_executable(game_server
    src/boost_json.cpp
    src/http_server.cpp
    src/json_loader.cpp
    src/logger.cpp
    src/main.cpp
    src/model.cpp
    src/request_handler.cpp
    src/static_file_handler.cpp
)

target_include_directories(game_server PRIVATE src)

target_link_libraries(game_server PRIVATE
    Boost::log
    Boost::log_setup
    Boost::system
)

if (UNIX AND NOT APPLE)
    target_link_libraries(game_server PRIVATE pthread)
endif()
