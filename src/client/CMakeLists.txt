set(component client)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    application.cpp
    application.hpp

    time_manager.hpp
)

target_link_libraries(${component}
    PUBLIC wsi
    PUBLIC graphics
    PUBLIC renderer
    PUBLIC imgui
)