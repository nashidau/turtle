# CMake version literally just picked from the tutorial.
# brew shows 3.21.1 for mac
# ubuntu 18.04 (github actions) 3.12.4 apparently
cmake_minimum_required(VERSION 3.10)

set_property(GLOBAL PROPERTY GLOBAL_DEPENDS_DEBUG_MODE 1)

set(CMAKE_BUILD_TYPE Debug)

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
	trtl_crier.c
	trtl_events.c
	trtl_object_canvas.c
	trtl_object_grid.c
	trtl_object_mesh.c
	trtl_object_sprite.c
	trtl_pipeline.c
	trtl_scribe.c
	trtl_seer.c
	trtl_shader.c
	trtl_shell.c
	trtl_solo.c
	trtl_strata_base.c
	trtl_strata_grid.c
	trtl_texture.c
	trtl_timer.c
	trtl_uniform.c
	trtl_vulkan.c
	turtle.c
	vertex.c)
# shared libraries need PIC
set_property(TARGET turtleobjs PROPERTY POSITION_INDEPENDENT_CODE 1)

add_dependencies(turtleobjs shaders)

set(SHADERS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(SHADERS_BINARY_DIR ${PROJECT_BINARY_DIR}/shaderspvs)

target_compile_definitions(turtleobjs PUBLIC COMPILED_SHADER_DIR="${SHADERS_BINARY_DIR}")

# shared and static libraries built from the same object files
add_library(turtle SHARED $<TARGET_OBJECTS:turtleobjs>)
add_library(turtlestatic STATIC $<TARGET_OBJECTS:turtleobjs>)

target_link_libraries(turtle PUBLIC dl)
target_link_libraries(turtlestatic PUBLIC dl)

target_include_directories(turtleobjs PRIVATE ./third-party)
target_include_directories(turtleobjs PRIVATE ./third-party/cglm/include)


target_compile_options(turtle PRIVATE -Werror -Wall -Wextra)
target_compile_options(turtlestatic PRIVATE -Werror -Wall -Wextra)

INCLUDE(FindPkgConfig)
pkg_check_modules(CHECK check)
find_package(Vulkan REQUIRED FATAL_ERROR)
find_package(glfw3 3.3 REQUIRED)
if ($ENV{GITHUB}) 
	set(glslc_executable ../tools/fake-shadercc.sh)
else()
	find_program(glslc_executable NAMES glslc  HINTS Vulkan::glslc)
endif()


list(APPEND SHADERS 
	${SHADERS_SOURCE_DIR}/canvas/clouds.frag
	${SHADERS_SOURCE_DIR}/canvas/test-color-fill.frag 
	${SHADERS_SOURCE_DIR}/canvas/stars-1.frag
	${SHADERS_SOURCE_DIR}/canvas/sepia.frag
	${SHADERS_SOURCE_DIR}/canvas/canvas-vertex.vert
	${SHADERS_SOURCE_DIR}/grid/lines.frag
	${SHADERS_SOURCE_DIR}/grid/grid-vertex.vert
	${SHADERS_SOURCE_DIR}/grid/browns.frag
	${SHADERS_SOURCE_DIR}/grid/red.frag
	${SHADERS_SOURCE_DIR}/mesh/mesh-vert.vert
	${SHADERS_SOURCE_DIR}/mesh/mesh.frag
	${SHADERS_SOURCE_DIR}/mesh/mesh-notexture.frag
	${SHADERS_SOURCE_DIR}/sprite/sprite-vertex.vert
	${SHADERS_SOURCE_DIR}/sprite/sprite-fragment.frag)

add_custom_command(
  COMMAND
    ${CMAKE_COMMAND} -E make_directory ${SHADERS_BINARY_DIR}
  OUTPUT ${SHADERS_BINARY_DIR}
  COMMENT "Creating ${SHADERS_BINARY_DIR}"
)

foreach(source IN LISTS SHADERS)
  get_filename_component(FILENAME ${source} NAME_WLE)
  add_custom_command(
    COMMAND
      ${glslc_executable}
      -DTRTL_SHADER=1
      -o ${SHADERS_BINARY_DIR}/${FILENAME}.spv
      ${source}
    OUTPUT ${SHADERS_BINARY_DIR}/${FILENAME}.spv
    DEPENDS ${SHADERS_BINARY_DIR}
    DEPENDS ${source}
    COMMENT "Compiling Shader ${FILENAME}"
  )
  list(APPEND SPV_SHADERS ${SHADERS_BINARY_DIR}/${FILENAME}.spv)
endforeach()

add_custom_target(shaders DEPENDS ${SPV_SHADERS})

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
	trtl_crier_check.c
	trtl_events_check.c
	trtl_shader_check.c
	trtl_solo_check.c
	trtl_timer_check.c
	trtl_uniform_check.c)
target_link_libraries(trtl_check PRIVATE turtlestatic talloc check)
target_include_directories(trtl_check PRIVATE ./third-party)
target_link_libraries(trtl_check PRIVATE ${CHECK_LIBRARIES} m dl pthread)
target_compile_options(trtl_check PRIVATE -Werror -Wall -Wextra)

add_custom_target(graphviz ALL
    COMMAND ${CMAKE_COMMAND} "--graphviz=turtle.dot" .
    COMMAND dot -Tpng turtle.dot -o turtle-depends.png
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# vim: set nospell :
