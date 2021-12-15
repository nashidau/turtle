# CMake version literally just picked from the tutorial.
# brew shows 3.21.1 for mac
# ubuntu 18.04 (github actions) 3.12.4 apparently
cmake_minimum_required(VERSION 3.10)

project(turtle VERSION 0.1)

configure_file(turtle_config.h.in turtle_config.h)

# specify the C standard (want to be C23 ASAP)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Our main turtle library
add_library(turtleobjs OBJECT
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
# shared libraries need PIC
set_property(TARGET turtleobjs PROPERTY POSITION_INDEPENDENT_CODE 1)

# shared and static libraries built from the same object files
add_library(turtle SHARED $<TARGET_OBJECTS:turtleobjs>)
add_library(turtlestatic STATIC $<TARGET_OBJECTS:turtleobjs>)

target_include_directories(turtleobjs PRIVATE ./third-party)
target_include_directories(turtleobjs PRIVATE ./third-party/cglm/include)

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
target_link_libraries(turtle PUBLIC talloc glfw ${Vulkan_LIBRARIES})
target_link_libraries(turtlestatic PUBLIC talloc glfw ${Vulkan_LIBRARIES})


###
# Test Suite
##
# Test suite target.  Uses static library so we can mock the turtle and vulkan symbols.
###
add_executable(trtl_check
	trtl_check.c
	trtl_shader_check.c
	trtl_solo_check.c
	trtl_timer_check.c
	trtl_uniform_check.c)
target_link_libraries(trtl_check PUBLIC turtlestatic talloc check)
target_include_directories(trtl_check PRIVATE ./third-party)

