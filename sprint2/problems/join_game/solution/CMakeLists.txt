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
    boost_json.cpp
    http_server.cpp
    json_loader.cpp
    logger.cpp
    main.cpp
    model.cpp
    request_handler.cpp
    static_file_handler.cpp
)

target_include_directories(game_server PRIVATE .)

target_link_libraries(game_server PRIVATE
    Boost::log
    Boost::log_setup
    Boost::system
)

if (UNIX AND NOT APPLE)
    target_link_libraries(game_server PRIVATE pthread)
endif()