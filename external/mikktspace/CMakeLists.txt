cmake_minimum_required(VERSION 3.18)
project(MikkTSpace)

add_library(${PROJECT_NAME} STATIC mikktspace.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

