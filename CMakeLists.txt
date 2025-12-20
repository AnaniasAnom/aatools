cmake_minimum_required(VERSION 3.31.6)

project(Tools)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-Wall -Werror")

add_executable(chatlog)
target_sources(chatlog
  PRIVATE
    chatlog.cpp
)
