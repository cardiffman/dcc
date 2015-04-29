cmake_minimum_required(VERSION 3.1.0)
project(dcc)

set(CMAKE_CXX_FLAGS "-g -fmessage-length=0 -ftabstop=4 -std=c++11")

add_executable(dccsuper dccsuper.cpp)
