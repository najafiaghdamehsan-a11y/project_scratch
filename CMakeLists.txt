cmake_minimum_required(VERSION 3.20)
project(project_scratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(project_scratch
        src/main.cpp
        src/core/log.cpp
        src/model/model.cpp
        src/io/serializer.cpp
        src/engine/runtime.cpp
	    src/engine/safety.cpp
		src/engine/scheduler.cpp
		src/engine/value.cpp
		src/engine/vars.cpp
        src/ui/app.cpp
)

target_include_directories(project_scratch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(project_scratch PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

