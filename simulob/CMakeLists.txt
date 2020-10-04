cmake_minimum_required(VERSION 2.6)
project(lob)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIRS})

include_directories(include)
file(GLOB SOURCES "src/*.cpp")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++17 -pthread")

add_executable(lob ${SOURCES})

set_property(TARGET lob PROPERTY CXX_STANDARD 17)
target_link_libraries(lob ${CURSES_LIBRARIES} stdc++fs)
# TODO: Run -Werror in CI.
target_compile_options(lob PRIVATE -Wall -Wextra)
