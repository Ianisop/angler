#!/bin/bash

# Build directory
BUILD_DIR="build"

echo "Configuring project with CMake..."
cmake -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# If configuration fails, exit
if [ $? -ne 0 ]; then
    echo "CMake configuration failed."
    exit 1
fi

echo "Building project..."
cmake --build "$BUILD_DIR"

# If build fails, exit
if [ $? -ne 0 ]; then
    echo "Build failed."
    exit 1
fi

# Copy compile_commands.json if it exists
if [ -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Copying compile_commands.json to project root..."
    cp "$BUILD_DIR/compile_commands.json" .
else
    echo "Warning: compile_commands.json not found in build directory."
fi

echo "Build completed successfully."
