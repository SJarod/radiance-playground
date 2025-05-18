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
_add_project_definitions(${component})
target_sources(${component} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${component} PUBLIC ${PACKAGE_SOURCE_FILES})
target_link_libraries(${component} PUBLIC ${PLATFORM_LIBRARIES} PUBLIC nvpro_core)
target_compile_definitions(${component} PUBLIC USE_NV_PRO_CORE)
endif()


target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")