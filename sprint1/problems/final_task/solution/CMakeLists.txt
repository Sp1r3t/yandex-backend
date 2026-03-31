cmake_minimum_required(VERSION 3.11)

project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/boost_json.cpp
    src/model.cpp
    src/json_loader.cpp
    src/request_handler.cpp
)

target_include_directories(game_server PRIVATE src)

target_link_libraries(game_server PRIVATE
    ${CONAN_LIBS}
    Threads::Threads
)