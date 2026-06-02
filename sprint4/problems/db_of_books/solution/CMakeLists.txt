cmake_minimum_required(VERSION 3.11)

project(book_manager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

add_executable(book_manager
    src/main.cpp
)

target_link_libraries(book_manager PRIVATE CONAN_PKG::libpqxx CONAN_PKG::boost)
