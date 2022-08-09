cmake_minimum_required(VERSION 3.15)
project(lx-ats CXX)

add_compile_options( -Wall 
    "$<$CONFIG:RELEASE>:-O3;-g>"
    "$<$CONFIG:DEBUG>:-g3;-ggdb3;-O0>"
)

add_subdirectory(src)