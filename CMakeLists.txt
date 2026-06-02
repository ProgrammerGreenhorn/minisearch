cmake_minimum_required(VERSION 3.15)

project(MiniSearch VERSION 0.1.0 LANGUAGES CXX)

find_package(Protobuf REQUIRED)

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(GENERATED_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(INDEX_PROTO ${PROTO_DIR}/index.proto)
set(INDEX_PROTO_SRC ${GENERATED_PROTO_DIR}/index.pb.cc)
set(INDEX_PROTO_HDR ${GENERATED_PROTO_DIR}/index.pb.h)

add_custom_command(
    OUTPUT ${INDEX_PROTO_SRC} ${INDEX_PROTO_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_PROTO_DIR}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${GENERATED_PROTO_DIR}
            --proto_path=${PROTO_DIR}
            ${INDEX_PROTO}
    DEPENDS ${INDEX_PROTO}
    COMMENT "Generating protobuf sources"
    VERBATIM
)

add_executable(minisearch
    ${INDEX_PROTO_SRC}
    ${INDEX_PROTO_HDR}
    src/main.cpp
    src/cli/CommandParser.cpp
    src/index/FileScanner.cpp
    src/index/InvertedIndex.cpp
    src/index/IndexRepository.cpp
    src/index/IndexStorage.cpp
    src/index/TextParser.cpp
    src/search/SearchEngine.cpp
    src/shell/InteractiveShell.cpp
    src/util/Logger.cpp
    src/util/ThreadPool.cpp
)

target_include_directories(minisearch PRIVATE
    include
    ${GENERATED_PROTO_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
target_compile_features(minisearch PRIVATE cxx_std_17)
target_link_libraries(minisearch PRIVATE ${Protobuf_LIBRARIES})

target_compile_options(minisearch PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-O0 -g3>
)

target_compile_definitions(minisearch PRIVATE
    $<$<CONFIG:Debug>:MINISEARCH_ENABLE_DEBUG_LOG>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minisearch PRIVATE stdc++fs)
endif()

install(TARGETS minisearch
    RUNTIME DESTINATION bin
)
