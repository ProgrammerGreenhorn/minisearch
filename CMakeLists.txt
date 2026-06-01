cmake_minimum_required(VERSION 3.15)

project(MiniSearch VERSION 0.1.0 LANGUAGES CXX)

add_executable(minisearch
    src/main.cpp
    src/cli/CommandParser.cpp
    src/index/FileScanner.cpp
    src/index/InvertedIndex.cpp
    src/index/IndexStorage.cpp
    src/index/TextParser.cpp
    src/search/SearchEngine.cpp
    src/util/Logger.cpp
    src/util/ThreadPool.cpp
)

target_include_directories(minisearch PRIVATE include)
target_compile_features(minisearch PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minisearch PRIVATE stdc++fs)
endif()

