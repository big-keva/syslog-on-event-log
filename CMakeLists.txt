cmake_minimum_required(VERSION 3.5)
project(syslog-on-event-log)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
  add_compile_options(-MP)
else()
  add_compile_options(-W -Wall -Wextra)
endif()

add_library(syslog-on-event-log STATIC
  syslog-on-event-log.cpp
)
