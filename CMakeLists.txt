# CMake version
cmake_minimum_required(VERSION 3.21)

# C++ standard version
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(PROJECT_NAME radiance_playground)

# initiating project given its name
project(${PROJECT_NAME})

set(RENDERER_OPTION_USE_NV_PRO_CORE ON)

add_subdirectory(externals)
add_subdirectory(internal)
add_subdirectory(src)