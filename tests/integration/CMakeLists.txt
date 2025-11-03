enable_testing()

find_program(PYTEST_EXECUTABLE pytest REQUIRED
    DOC "Path to the pytest executable used for integration tests"
)

# Run pytest and point it at the already-built packer binary
add_test(
    NAME integration_test
    COMMAND ${PYTEST_EXECUTABLE} -vv -s "${CMAKE_CURRENT_SOURCE_DIR}/test_packer.py" --path-to-packer $<TARGET_FILE:packer>
)

set_tests_properties(integration_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    LABELS "integration"
)
