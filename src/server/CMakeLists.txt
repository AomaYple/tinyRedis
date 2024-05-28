project(server)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PATH}/${PROJECT_NAME}
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE
        ${SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-Og -fsanitize=address>
        $<$<CONFIG:Release>:-Ofast>
)

target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        uring
)