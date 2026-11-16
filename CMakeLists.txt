cmake_minimum_required(VERSION 3.16)
project(liona_soulbound VERSION 0.1.0 LANGUAGES CXX)

add_subdirectory(core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests/core)
endif()
