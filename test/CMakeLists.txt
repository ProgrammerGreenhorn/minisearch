include(GoogleTest)

file(GLOB_RECURSE MINISEARCH_TEST_SOURCES
     "${PROJECT_SOURCE_DIR}/test/*_test.cpp"
     "${PROJECT_SOURCE_DIR}/test/*/*_test.cpp")

add_custom_target(build-tests COMMAND ${CMAKE_CTEST_COMMAND} --show-only)
add_custom_target(check-tests COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

foreach(minisearch_test_source ${MINISEARCH_TEST_SOURCES})
    get_filename_component(minisearch_test_filename ${minisearch_test_source}
                           NAME)
    string(REPLACE ".cpp" "" minisearch_test_name
           ${minisearch_test_filename})

    add_executable(${minisearch_test_name} EXCLUDE_FROM_ALL
                   ${minisearch_test_source})

    add_dependencies(build-tests ${minisearch_test_name})
    add_dependencies(check-tests ${minisearch_test_name})

    target_link_libraries(${minisearch_test_name}
        PRIVATE
        minisearch_lib
        GTest::gtest
        GTest::gmock_main)

    target_include_directories(${minisearch_test_name}
        PRIVATE
        ${GENERATED_PROTO_DIR}
        ${Protobuf_INCLUDE_DIRS})

    target_compile_options(${minisearch_test_name} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-O0 -g3>)

    set_target_properties(${minisearch_test_name}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
        COMMAND ${minisearch_test_name})

    gtest_discover_tests(${minisearch_test_name}
        EXTRA_ARGS
        --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${minisearch_test_name}.xml
        --gtest_catch_exceptions=0
        DISCOVERY_TIMEOUT 120
        PROPERTIES TIMEOUT 120)
endforeach()
