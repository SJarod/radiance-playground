# CMake version
cmake_minimum_required(VERSION 3.21)

# C++ standard version
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(PROJECT_NAME radiance_playground)

# initiating project given its name
project(${PROJECT_NAME})

option(OPTION_USE_NV_PRO_CORE "Use nvpro_core library instead of a custom Acceleration Structure implementation" ON)

add_subdirectory(externals)

if (OPTION_USE_NV_PRO_CORE)
#--------------------------------------------------------------------------------------------------
# look for nvpro_core 1) as a sub-folder 2) at some other locations
# this cannot be put anywhere else since we still didn't find setup.cmake yet
if(NOT BASE_DIRECTORY)

  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/..
    REQUIRED
    DOC "Directory containing nvpro_core"
    )
endif()

## Various functions and macros REQUIRED
if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else()
  message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing nvpro_core")
endif()

set(TUTO_KHR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_definitions(/wd26812)  # 'enum class' over 'enum'
    add_definitions(/wd26451)  # Arithmetic overflow, casting 4 byte value to 8 byte value
endif()

#--------------------------------------------------------------------------------------------------
# Package shared by all projects
_add_package_VulkanSDK()
_add_package_ImGUI()
_add_nvpro_core_lib()

target_compile_definitions(glm INTERFACE GLM_FORCE_RADIANS)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

endif()

add_subdirectory(internal)
add_subdirectory(src)
