cmake_minimum_required(VERSION 3.30)

project(tinyRedis)

set(ROOT_PATH ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})

add_subdirectory(src/client)
add_subdirectory(src/server)
