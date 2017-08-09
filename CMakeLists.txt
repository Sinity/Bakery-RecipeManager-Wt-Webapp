project(CukierniaRecepty)
cmake_minimum_required(VERSION 2.8)

#build type
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

#flags
add_definitions(-std=c++14 -Wall -Wextra)

#project files and include pathes
file(GLOB_RECURSE SRCS "CukierniaRecepty/*.cpp")

add_executable(CukierniaRecepty ${SRCS})

# wt.lib;wtdbo.lib;wtdbomysql.lib;wthttp.lib; -> change buitltin httpd to fcgi. Libs need to be 32bit
target_link_libraries(CukierniaRecepty wt wtdbo wtdbomysql wtfcgi)
