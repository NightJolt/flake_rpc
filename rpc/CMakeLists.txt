project(flake_rpc)

file(
    GLOB_RECURSE sources

    flake/*.h
    flake/*.cpp
)

add_library(${PROJECT_NAME} STATIC ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC flake_core)
