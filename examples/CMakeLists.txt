cmake_minimum_required(VERSION 3.16)
project(videosdk_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(videosdk REQUIRED)

add_executable(raspberry_pi raspberry_pi.cpp)
target_link_libraries(raspberry_pi PRIVATE videosdk::cpp)

add_executable(arduino arduino.cpp)
target_link_libraries(arduino PRIVATE videosdk::cpp)
