cmake_minimum_required(VERSION 3.12)

project(discover CXX)
add_compile_options(-Ofast -Wall) # passing the compiler a `-pthread` flag doesn't work here

find_library(RF24 rf24 REQUIRED)
message(STATUS "using RF24 library: ${RF24}")

add_executable(discover discover.cpp common.cpp)
target_link_libraries(discover PUBLIC ${RF24} pthread)

add_executable(pretender pretender.cpp common.cpp)
target_link_libraries(pretender PUBLIC ${RF24} pthread)

