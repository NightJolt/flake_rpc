project(fl_rpc_gen)

file (
    GLOB_RECURSE sources

    source/*.h
    source/*.cpp
)

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)

target_link_libraries(${PROJECT_NAME} flake_core)

function(fl_rpc_gen namespace rpc_file_name rpc_file_path)
    set(RPC_BIN "${CMAKE_BINARY_DIR}/flake_rpc/gen/fl_rpc_gen")

    message("generating rpc files")
    execute_process(COMMAND "${RPC_BIN}" "${CMAKE_CURRENT_SOURCE_DIR}/${rpc_file_path}/${rpc_file_name}.rpc" "${CMAKE_BINARY_DIR}/rpc/${namespace}/${rpc_file_name}.h")
endfunction()