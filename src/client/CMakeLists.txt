project(tinyRedisClient)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        CXX_STANDARD ${CMAKE_CXX_STANDARD_LATEST}
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        RUNTIME_OUTPUT_DIRECTORY ${ROOT_PATH}/${PROJECT_NAME}
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        src/*.cpp
        ../common/*.cpp
)
target_sources(${PROJECT_NAME}
        PRIVATE
        ${SOURCES}
)

target_compile_options(${PROJECT_NAME}
        PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-Og -fsanitize=address -fsanitize=leak -fsanitize=undefined>
        $<$<CONFIG:Release>:-Ofast>
)

target_link_options(${PROJECT_NAME}
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak -fsanitize=undefined>
)
