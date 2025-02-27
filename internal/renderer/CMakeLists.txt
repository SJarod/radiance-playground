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
    skybox.cpp)

target_link_libraries(${component}
    PUBLIC graphics
    PUBLIC assimp::assimp
    PUBLIC stb
    PUBLIC imgui
    PUBLIC engine
)


target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")