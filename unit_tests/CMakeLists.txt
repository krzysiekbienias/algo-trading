file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp"
)

if(NOT ENABLE_IBKR)
    list(FILTER TEST_SOURCES EXCLUDE REGEX "test_ibkr_forex_pair\\.cpp$")
endif()

if(TEST_SOURCES STREQUAL "")
    message(STATUS "No unit tests found (add unit_tests/test_*.cpp).")
    return()
endif()

find_package(GTest REQUIRED)

add_executable(test_environment ${TEST_SOURCES})
set_target_properties(test_environment PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)


target_link_libraries(test_environment
        PRIVATE
        core_lib
        GTest::gtest
        GTest::gtest_main
)

if(ENABLE_IBKR)
    target_link_libraries(test_environment PRIVATE ibkr_session)
endif()

# Expose the fixtures directory as a compile-time constant so tests can locate
# fixture files regardless of the working directory they are launched from.
target_compile_definitions(test_environment PRIVATE
        TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

include(GoogleTest)
gtest_discover_tests(test_environment)