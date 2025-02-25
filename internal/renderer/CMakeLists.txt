set(component renderer)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    render_state.hpp
    render_state.cpp
    
    renderer.hpp
    renderer.cpp

    mesh.hpp
    mesh.cpp

    texture.hpp
    texture.cpp

    scene.hpp

    light.hpp
    light.cpp
)

target_link_libraries(${component}
    PUBLIC graphics
    PUBLIC assimp::assimp
    PUBLIC stb
)


target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")