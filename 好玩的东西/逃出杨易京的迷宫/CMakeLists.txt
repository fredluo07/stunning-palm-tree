cmake_minimum_required(VERSION 3.15)
project(DarkMaze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DarkMaze WIN32
    src/main.cpp
    src/glcore.h
    src/math3d.h
    src/shaders.h
    src/resources.h
    src/text.h
    src/audio.h
    src/image.h
    src/game.h
)

target_include_directories(DarkMaze PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(DarkMaze PRIVATE /utf-8 /EHsc /O2 /W3 /permissive-)
    target_compile_definitions(DarkMaze PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(DarkMaze PRIVATE -O2 -Wall)
    target_link_options(DarkMaze PRIVATE -mwindows)
endif()

target_link_libraries(DarkMaze PRIVATE opengl32 gdi32 user32 winmm gdiplus shlwapi)
set_target_properties(DarkMaze PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
)
