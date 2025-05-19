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

    render_graphs/global_illumination_with_irradiance_probes/graph_g2ip.cpp
    render_graphs/global_illumination_with_irradiance_probes/graph_g2ip.hpp
    render_graphs/global_illumination_with_irradiance_probes/graph_g2iprt.cpp
    render_graphs/global_illumination_with_irradiance_probes/graph_g2iprt.hpp
    render_graphs/radiance_cascades/graph_rc2d.cpp
    render_graphs/radiance_cascades/graph_rc2d.hpp
    render_graphs/radiance_cascades/graph_rc3d.cpp
    render_graphs/radiance_cascades/graph_rc3d.hpp
    render_graphs/radiance_cascades/graph_rc3drt.cpp
    render_graphs/radiance_cascades/graph_rc3drt.hpp

    scenes/global_illumination_with_irradiance_probes/scene_g2ip.cpp
    scenes/global_illumination_with_irradiance_probes/scene_g2ip.hpp
    scenes/global_illumination_with_irradiance_probes/scene_g2iprt.cpp
    scenes/global_illumination_with_irradiance_probes/scene_g2iprt.hpp
    scenes/radiance_cascades/scene_rc2d.cpp
    scenes/radiance_cascades/scene_rc2d.hpp
    scenes/radiance_cascades/scene_rc3d.cpp
    scenes/radiance_cascades/scene_rc3d.hpp
    scenes/radiance_cascades/scene_rc3drt.cpp
    scenes/radiance_cascades/scene_rc3drt.hpp

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

if (OPTION_USE_NV_PRO_CORE)
_add_project_definitions(${component})
endif()

target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")