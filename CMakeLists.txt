cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)

project(tracking)

# Find OpenCV
set(OpenCV_DIR "/opt/homebrew/Cellar/opencv/4.11.0_1/")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
link_directories(${OpenCV_LIBRARY_DIRS})
add_definitions(${OpenCV_DEFINITIONS})

# Compiler Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "--no-as-needed" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "--as-needed" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Define executable
add_executable(tracking main.cpp)

# Link against Torch, OpenCV, and SFML
target_link_libraries(tracking ${OpenCV_LIBS})

# Set C++ Standard
set_property(TARGET tracking PROPERTY CXX_STANDARD 17)
