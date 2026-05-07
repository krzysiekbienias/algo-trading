file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp"
)

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

include(GoogleTest)
gtest_discover_tests(test_environment)