set(component client)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    scripts/move_camera.hpp
    scripts/move_camera.cpp
    scripts/probes.hpp
    scripts/probes.cpp
    scripts/radiance_cascades.hpp
    scripts/radiance_cascades.cpp

    application.cpp
    application.hpp

    sample_scene.hpp
    sample_scene.cpp
    sample_scene_2d.hpp
    sample_scene_2d.cpp

    time_manager.hpp
	
	input_manager.hpp
	input_manager.cpp
) 

target_link_libraries(${component}
    PUBLIC wsi
    PUBLIC graphics
    PUBLIC renderer
    PUBLIC imgui
)

target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")