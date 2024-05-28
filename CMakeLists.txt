cmake_minimum_required(VERSION 3.29.3)

project(tinyRedis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

add_compile_options(
        -Wall -Wextra -Wpedantic
)

add_subdirectory(src/server)
add_subdirectory(src/client)
