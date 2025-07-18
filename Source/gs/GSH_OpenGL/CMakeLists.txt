include(Header)

project(GSH_OpenGL)

if(NOT TARGET Framework_OpenGl)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/Framework/build_cmake/FrameworkOpenGl
		${CMAKE_CURRENT_BINARY_DIR}/FrameworkOpenGl
	)
endif()
list(APPEND GSH_OPENGL_PROJECT_LIBS Framework_OpenGl)

if(NOT TARGET app_shared)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../../app_shared
		${CMAKE_CURRENT_BINARY_DIR}/app_shared
	)
endif()
list(APPEND GSH_OPENGL_PROJECT_LIBS app_shared)

if(TARGET_PLATFORM_UNIX_ARM)
	list(APPEND GSH_OPENGL_COMPILE_OPTIONS "-mfpu=neon")
endif()

if(TARGET_PLATFORM_UNIX AND NOT TARGET_PLATFORM_UNIX_ARM AND NOT TARGET_PLATFORM_UNIX_AARCH64)
	list(APPEND GSH_OPENGL_COMPILE_OPTIONS -msse -msse2 -mssse3)
endif()

add_library(gsh_opengl STATIC 
	GSH_OpenGL.cpp
	GSH_OpenGL.h
	GSH_OpenGL_Shader.cpp
	GSH_OpenGL_Texture.cpp
)
target_link_libraries(gsh_opengl Framework_OpenGl ${GSH_OPENGL_PROJECT_LIBS})
target_include_directories(gsh_opengl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/gs/GSH_OpenGL/)
target_compile_options(gsh_opengl PRIVATE ${GSH_OPENGL_COMPILE_OPTIONS})
