cmake_minimum_required(VERSION 3.5)
project(syslog-win32)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
  add_compile_options(-MP)
else()
  add_compile_options(-W -Wall -Wextra)
endif()

add_library(syslog-win32 STATIC
  syslog-win32.cpp
)
