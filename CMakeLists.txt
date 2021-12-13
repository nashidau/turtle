# CMake version literally just picked from the tutorial.
# brew shows 3.21.1 for mac
# ubuntu 18.04 (github actions) 3.12.4 apparently
cmake_minimum_required(VERSION 3.10)

project(turtle VERSION 0.1)

configure_file(turtle_config.h.in turtle_config.h)

# Project binary dir is the output dir
#target_include_directories(turtle PUBLIC
#                           "${PROJECT_BINARY_DIR}"
#                           )

# specify the C standard (want to be C23 ASAP)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Our main turtle library
add_library(turtle 
	blobby.c
	helpers.c
	images.c
	objloader.c
	polyobj.c
	stringlist.c
	trtl_barriers.c
	trtl_object_canvas.c
	trtl_object_grid.c
	trtl_object_mesh_old.c
	trtl_object_sprite.c
	trtl_pipeline.c
	trtl_scribe.c
	trtl_seer.c
	trtl_shader.c
	trtl_shell.c
	trtl_solo.c
	trtl_texture.c
	trtl_timer.c
	trtl_uniform.c
	trtl_vulkan.c
	turtle.c
	vertex.c)

target_include_directories(turtle PRIVATE ./third-party)
target_include_directories(turtle PRIVATE ./third-party/cglm/include)


find_package(Vulkan REQUIRED FATAL_ERROR) # error
find_package(glfw3 3.3 REQUIRED)

###
# Triangles
##
# Simple test application; triangle
###
add_executable(triangles triangles.c)
target_include_directories(triangles PRIVATE ./third-party)
target_include_directories(triangles PRIVATE ./third-party/cglm/include)
target_link_libraries(triangles PUBLIC talloc turtle glfw ${Vulkan_LIBRARIES})
#target_link_libraries(triangles PUBLIC turtle)
#target_link_libraries(triangles glfw)
#target_link_libraries(triangles ${VULKAN_LIB_LIST})




# FIXME: Add static and dynamic libraries:
# https://stackoverflow.com/questions/2152077/is-it-possible-to-get-cmake-to-build-both-a-static-and-shared-library-at-the-sam

