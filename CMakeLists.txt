cmake_minimum_required(VERSION 3.29.3)

project(tinyRedis)

set(SERVER "tinyRedisServer")

add_executable(${SERVER})

set_target_properties(${SERVER} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${SERVER}
)

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS src/server/*.cpp)
target_sources(${SERVER} PRIVATE
        ${SERVER_SOURCES}
)

target_compile_options(${SERVER} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-Og -fsanitize=address>
        $<$<CONFIG:Release>:-Ofast>
)

target_link_options(${SERVER} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(${SERVER} PRIVATE
        uring
)
