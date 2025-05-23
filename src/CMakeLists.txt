set(component playground)

add_executable(${component})

target_sources(${component}
    PRIVATE
    main.cpp
)

target_link_libraries(${component}
    PRIVATE client
	PRIVATE Tracy::TracyClient
)

if (OPTION_USE_NV_PRO_CORE)
_add_project_definitions(${component})
endif()

add_subdirectory(client)

set(SHADER_SOURCES
	shaders/deferred/radiance_apply.frag
	shaders/deferred/unlit.frag

	shaders/forward/phong.frag
	shaders/forward/unlit.frag
	shaders/forward/unlit.vert
	shaders/forward/white.frag
	shaders/forward/red.frag
	shaders/forward/green.frag
	shaders/forward/blue.frag

	shaders/g2ip/environment_map.frag
	shaders/g2ip/environment_map.vert
	shaders/g2ip/irradiance_convolution.frag
	shaders/g2ip/phong.frag
	shaders/g2ip/phongrt.frag

	shaders/pp/final_image.frag
	shaders/pp/radiance_apply.frag
	shaders/pp/screen.vert

	shaders/rc/radiance_gather_2d.comp
	shaders/rc/radiance_gather_3drt.comp

	shaders/rt/phong.frag

	shaders/probe_grid_debug.frag
	shaders/probe_grid_debug.vert
	shaders/simple.vert
	shaders/skybox.frag
	shaders/skybox.vert
)

set(RUNTIME_OUTPUT_DIR $<TARGET_FILE_DIR:${component}>)

list(LENGTH SHADER_SOURCES SHADER_COUNT)
set(SHADER_OUTPUT_DIR "${RUNTIME_OUTPUT_DIR}/shaders")
add_custom_command(TARGET ${component}
	POST_BUILD
	COMMAND echo Compiling shader sources in ${SHADER_OUTPUT_DIR}...
)
set(SHADER_I 1)
foreach(SOURCE ${SHADER_SOURCES})
	set(SHADER_OUTPUT "${RUNTIME_OUTPUT_DIR}/${SOURCE}.spv")
	cmake_path(GET SOURCE PARENT_PATH SOURCE_REL_PATH)
	add_custom_command(TARGET ${component}
		POST_BUILD
		COMMAND echo [${SHADER_I}/${SHADER_COUNT}] ${SOURCE}...
		COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_OUTPUT_DIR}/${SOURCE_REL_PATH}
		COMMAND glslc ${CMAKE_SOURCE_DIR}/${SOURCE} -o ${SHADER_OUTPUT} --target-spv=spv1.4
		COMMAND echo ${SOURCE} : ${SHADER_OUTPUT}
	)
	MATH(EXPR SHADER_I "${SHADER_I} + 1")
endforeach()


add_custom_command(TARGET ${component}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/" "${RUNTIME_OUTPUT_DIR}/assets/"
)