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

if (NOT RENDERER_OPTION_USE_NV_PRO_CORE)
add_subdirectory(glm)
target_compile_definitions(glm PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(glm PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_library(imgui STATIC)
target_sources(imgui PUBLIC
    imgui/imgui.h
	imgui/imgui.cpp
	imgui/backends/imgui_impl_glfw.h
	imgui/backends/imgui_impl_glfw.cpp
	imgui/backends/imgui_impl_vulkan.h
	imgui/backends/imgui_impl_vulkan.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/imgui_demo.cpp
)

target_include_directories(imgui PUBLIC 
"${Vulkan_INCLUDE_DIR}"
./imgui/
)

target_link_libraries(imgui PUBLIC 
glfw
${Vulkan_LIBRARY}
)
endif()

set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_NO_EXPORT ON)
add_subdirectory(assimp)

add_library(stb INTERFACE)
target_sources(stb INTERFACE
    stb/stb_image.h
)
target_include_directories(stb INTERFACE ./stb/)

add_library(legitprofiler INTERFACE)
target_sources(legitprofiler INTERFACE
    LegitProfiler/ProfilerTask.h
	LegitProfiler/ImGuiProfilerRenderer.h
)
target_include_directories(legitprofiler INTERFACE ./LegitProfiler/)

add_subdirectory(VulkanMemoryAllocator)

add_subdirectory(tracy)
target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE)