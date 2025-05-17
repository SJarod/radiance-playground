set(component renderer)

add_library(${component} STATIC "")


target_sources(${component}
    PRIVATE
    render_state.hpp
    render_state.cpp
    
    renderer.hpp
    renderer.cpp

    render_phase.hpp
    render_phase.cpp

    render_graph.hpp
    render_graph.cpp

    mesh.hpp
    mesh.cpp

    texture.hpp
    texture.cpp

    scene.hpp
    scene.cpp

    light.hpp
    light.cpp
    
    skybox.hpp
    skybox.cpp
    
    model.hpp
    model.cpp
)

target_link_libraries(${component}
    PUBLIC graphics
    PUBLIC assimp::assimp
    PUBLIC stb
    PUBLIC imgui
    PUBLIC engine
    PRIVATE Tracy::TracyClient
)

if (RENDERER_OPTION_USE_NV_PRO_CORE)
#--------------------------------------------------------------------------------------------------
# look for nvpro_core 1) as a sub-folder 2) at some other locations
# this cannot be put anywhere else since we still didn't find setup.cmake yet
if(NOT BASE_DIRECTORY)

  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR}/../../..
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
_add_nvpro_core_lib()

target_link_libraries(${component}
    PRIVATE nvpro_core
)

target_compile_definitions(glm INTERFACE GLM_FORCE_RADIANS)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_compile_definitions(${component} PUBLIC USE_NV_PRO_CORE)

endif()


target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")