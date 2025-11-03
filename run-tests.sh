#!/bin/bash

# Script to configure, build and run tests
set -e

TARGET_DIR=build

# Create build directory if it doesn't exist
cmake -S . -B ${TARGET_DIR}

# Build the project
cmake --build ${TARGET_DIR}

# Run tests with detailed output
ctest --test-dir ${TARGET_DIR} --output-on-failure --verbose