cmake_minimum_required(VERSION 3.15)
project(record_mic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(record_mic record_mic.cpp)
target_link_libraries(record_mic PRIVATE asound)
