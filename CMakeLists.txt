cmake_minimum_required(VERSION 3.29.3)

project(tinyKV)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE
        ${SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-Og -fsanitize=address>
        $<$<CONFIG:Release>:-Ofast>
)

target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)
