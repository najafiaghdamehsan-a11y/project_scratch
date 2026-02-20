cmake_minimum_required(VERSION 3.20)
project(project_scratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(project_scratch main.cpp)
target_link_libraries(project_scratch PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)
