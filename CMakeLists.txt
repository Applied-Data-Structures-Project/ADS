cmake_minimum_required(VERSION 3.10.0)
project(ADSProject VERSION 0.1.0 LANGUAGES C CXX)

file(GLOB_RECURSE HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "include/*.hpp")
#file(GLOB_RECURSE SOURCE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")

add_executable(ADSProject main.cpp ${HEADERS} ${SOURCE})
target_include_directories(ADSProject PUBLIC include)


include(CTest)
enable_testing()

