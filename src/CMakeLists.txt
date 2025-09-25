cmake_minimum_required(VERSION 3.13)

project(edat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    parsers.cpp
    )

if(ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

add_library(edat ${SOURCES})

