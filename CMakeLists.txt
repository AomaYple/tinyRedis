cmake_minimum_required(VERSION 3.30.5)

project(tinyRedis)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(PATH ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

add_compile_options(
        -Wall -Wextra -Wpedantic
)

add_subdirectory(src/client)
add_subdirectory(src/server)
