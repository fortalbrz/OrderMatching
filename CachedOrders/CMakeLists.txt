# CMakeList.txt : CMake project for OrderCache, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project (CachedOrders LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(runUnitTests OrderCacheTests.cpp)
target_link_libraries(runUnitTests GTest::GTest GTest::Main)

gtest_discover_tests(runUnitTests)