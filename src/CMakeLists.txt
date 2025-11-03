# Add library target
add_library(libpacker STATIC)
file(GLOB APP_SOURCES "*.cpp")
list(REMOVE_ITEM APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_sources(libpacker PRIVATE ${APP_SOURCES})

target_link_libraries(libpacker PUBLIC xxhash)

# Add executable target
add_executable(packer
    main.cpp
)

# Link executable with library
target_link_libraries(packer
    PRIVATE
    libpacker
)
