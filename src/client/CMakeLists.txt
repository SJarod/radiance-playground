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
    scripts/radiance_cascades3d.hpp
    scripts/radiance_cascades3d.cpp

    application.cpp
    application.hpp

    scenes/sample_scene.hpp
    scenes/sample_scene.cpp
    scenes/sample_scene_2d.hpp
    scenes/sample_scene_2d.cpp
    scenes/sample_scene_rc3d.hpp
    scenes/sample_scene_rc3d.cpp

    render_graphs/compute_pp_graph.hpp
    render_graphs/compute_pp_graph.cpp
    render_graphs/irradiance_baked_graph.hpp
    render_graphs/irradiance_baked_graph.cpp
    render_graphs/rc3d_graph.hpp
    render_graphs/rc3d_graph.cpp

    time_manager.hpp
	
	input_manager.hpp
	input_manager.cpp
) 

target_link_libraries(${component}
    PUBLIC wsi
    PUBLIC graphics
    PUBLIC renderer
    PUBLIC imgui
	PRIVATE Tracy::TracyClient
	PUBLIC legitprofiler
)

target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")