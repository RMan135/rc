cmake_minimum_required(VERSION 3.9)
project(connect4server)

find_package(Threads)

set(CMAKE_CXX_STANDARD 11)

add_executable(connect4server main.cpp)
target_link_libraries(connect4server/lib ${CMAKE_THREAD_LIBS_INIT})