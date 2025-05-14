set(component rhi_sandbox)

add_executable(${component})

target_sources(${component}
    PRIVATE
    main.cpp
)

target_link_libraries(${component}
    PRIVATE client
	PRIVATE Tracy::TracyClient
)

add_subdirectory(client)

set(SHADER_SOURCES
	shaders/unlit.vert
	shaders/unlit.frag
	shaders/white.frag

	shaders/phong.vert
	shaders/phong.frag

	shaders/environment_map.vert
	shaders/environment_map.frag

	shaders/skybox.vert
	shaders/skybox.frag

	shaders/irradiance_convolution.frag

	shaders/screen.vert
	shaders/final_image_direct.frag
	shaders/final_image_direct_indirect.frag
	shaders/radiance_apply_deferred.frag

	shaders/radiance_gather.comp
	shaders/radiance_gather_voxel.comp
	
	shaders/probe_grid_debug.frag
	shaders/probe_grid_debug.vert
)

set(RUNTIME_OUTPUT_DIR $<TARGET_FILE_DIR:${component}>)

list(LENGTH SHADER_SOURCES SHADER_COUNT)
set(SHADER_OUTPUT_DIR "${RUNTIME_OUTPUT_DIR}/shaders")
add_custom_command(TARGET ${component}
	POST_BUILD
	COMMAND echo Compiling shader sources in ${SHADER_OUTPUT_DIR}...
	COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
)
set(SHADER_I 1)
foreach(SOURCE ${SHADER_SOURCES})
	set(SHADER_OUTPUT "${RUNTIME_OUTPUT_DIR}/${SOURCE}.spv")
	add_custom_command(TARGET ${component}
		POST_BUILD
		COMMAND echo [${SHADER_I}/${SHADER_COUNT}] ${SOURCE}...
		COMMAND glslc ${CMAKE_SOURCE_DIR}/${SOURCE} -o ${SHADER_OUTPUT}
		COMMAND echo ${SOURCE} : ${SHADER_OUTPUT}
	)
	MATH(EXPR SHADER_I "${SHADER_I} + 1")
endforeach()


add_custom_command(TARGET ${component}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/" "${RUNTIME_OUTPUT_DIR}/assets/"
)