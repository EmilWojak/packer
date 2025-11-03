find_package(GTest REQUIRED)

# Add test executable
add_executable(unit_tests)
file(GLOB UNIT_TEST_SOURCES "*.cpp")
target_sources(unit_tests PRIVATE ${UNIT_TEST_SOURCES})

target_include_directories(unit_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Link test executable with app library and Google Test
target_link_libraries(unit_tests
    PRIVATE
    libpacker
    gtest
    gtest_main
)

# register all individual test cases with CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)
