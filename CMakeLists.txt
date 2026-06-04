cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(MiniSearch VERSION 0.1.0 LANGUAGES CXX)

option(MINISEARCH_BUILD_TESTS "Build MiniSearch tests" ON)
option(MINISEARCH_BUILD_GUI "Build the wxWidgets GUI" ON)

find_package(Protobuf REQUIRED)

if(MINISEARCH_BUILD_GUI)
    find_package(wxWidgets REQUIRED COMPONENTS core base)
endif()

if(MINISEARCH_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

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

add_library(minisearch_lib
    ${INDEX_PROTO_SRC}
    ${INDEX_PROTO_HDR}
    src/app/Application.cpp
    src/cli/CommandParser.cpp
    src/index/FileScanner.cpp
    src/index/IndexBuilder.cpp
    src/index/InvertedIndex.cpp
    src/index/IndexRepository.cpp
    src/index/IndexSchema.cpp
    src/index/IndexStorage.cpp
    src/index/TextParser.cpp
    src/search/SearchEngine.cpp
    src/shell/InteractiveShell.cpp
    src/util/Hash.cpp
    src/util/Logger.cpp
    src/util/ThreadPool.cpp
)

target_include_directories(minisearch_lib
    PUBLIC
    include
    PRIVATE
    ${GENERATED_PROTO_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
target_compile_features(minisearch_lib PUBLIC cxx_std_17)
target_link_libraries(minisearch_lib PRIVATE ${Protobuf_LIBRARIES})

target_compile_options(minisearch_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-O0 -g3>
)

target_compile_definitions(minisearch_lib
    PUBLIC
    $<$<CONFIG:Debug>:MINISEARCH_ENABLE_DEBUG_LOG>
    PRIVATE
    MINISEARCH_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(minisearch
    src/main.cpp
)

target_link_libraries(minisearch PRIVATE minisearch_lib)

target_compile_options(minisearch PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-O0 -g3>
)

if(MINISEARCH_BUILD_GUI)
    include(${wxWidgets_USE_FILE})

    add_executable(minisearch-gui WIN32
        src/gui/main.cpp
        src/gui/MainFrame.cpp
    )

    target_link_libraries(minisearch-gui
        PRIVATE
        minisearch_lib
        ${wxWidgets_LIBRARIES})

    target_compile_options(minisearch-gui PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-O0 -g3>
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minisearch_lib PUBLIC stdc++fs)
endif()

if(MINISEARCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

install(TARGETS minisearch
    RUNTIME DESTINATION bin
)

if(MINISEARCH_BUILD_GUI)
    install(TARGETS minisearch-gui
        RUNTIME DESTINATION bin
    )
endif()
