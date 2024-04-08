cmake_minimum_required(VERSION 3.29.1)

project(tinyKv)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
)

target_sources(${PROJECT_NAME} PRIVATE
        src/main.cpp
)

target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-Og -fsanitize=address>
        $<$<CONFIG:Release>:-Ofast>
)

target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)
