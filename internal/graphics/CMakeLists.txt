set(component graphics)

add_library(${component} STATIC "")

target_sources(${component}
    PRIVATE
    context.hpp
    context.cpp

    instance.hpp
    instance.cpp

    device.hpp
    device.cpp

    surface.hpp
    surface.cpp

    swapchain.hpp
    swapchain.cpp

    render_pass.hpp
    render_pass.cpp

    pipeline.hpp
    pipeline.cpp

    buffer.hpp
    buffer.cpp

    image.hpp
    image.cpp
)

target_link_libraries(${component}
    PUBLIC ${Vulkan_LIBRARY}
    PUBLIC engine
    # TODO : find a way to make PRIVATE
    PUBLIC VulkanMemoryAllocator
)

target_include_directories(${component} PUBLIC "${Vulkan_INCLUDE_DIR}")

target_include_directories(${component} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(${component} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/..")