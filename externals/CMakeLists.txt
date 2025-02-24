add_subdirectory(glfw)

find_package(Vulkan REQUIRED)
if (${Vulkan_FOUND})
    message("Found Vulkan version ${Vulkan_VERSION}")
endif()

get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

add_compile_definitions(GLM_FORCE_RADIANS)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(GLM_FORCE_LEFT_HANDED)
add_subdirectory(glm)

set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_NO_EXPORT ON)
add_subdirectory(assimp)

add_library(stb INTERFACE)
target_sources(stb INTERFACE
    stb/stb_image.h
)
target_include_directories(stb INTERFACE ./stb/)