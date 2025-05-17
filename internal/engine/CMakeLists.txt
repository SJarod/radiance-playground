set(component engine)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    scriptable.hpp
    
    uniform.hpp

    vertex.hpp

    transform.hpp
    transform.cpp

    camera.hpp
    camera.cpp

    probe_grid.hpp
    probe_grid.cpp
)

target_link_libraries(${component}
    PUBLIC ${Vulkan_LIBRARY}
    PUBLIC glm
)

target_include_directories(${component} PUBLIC "${Vulkan_INCLUDE_DIR}")
target_include_directories(${component} PUBLIC "${glm_SOURCE_DIR}")

target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")