set(component client)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    scripts/move_camera.hpp
    scripts/move_camera.cpp

    application.cpp
    application.hpp

    sample_scene.hpp
    sample_scene.cpp
    sample_scene_2d.hpp
    sample_scene_2d.cpp

    time_manager.hpp
)

target_link_libraries(${component}
    PUBLIC wsi
    PUBLIC graphics
    PUBLIC renderer
)