
VULKANCFLAGS=/
VULKANLIBS=-lvulkan 



PKGS=talloc glfw3

CFLAGS+=-g -Wall -O2 `pkg-config --cflags ${PKGS}` -F /Library/Frameworks -iframework /Library/Frameworks 
LDFLAGS+=`pkg-config --libs ${PKGS}` -lvulkan -framework Cocoa -framework IOSurface -framework IOKit -framework CoreGraphics -framework QuartzCore -lstdc++ -framework Metal

SHADERCC=glslc

ALL: triangles shaders/frag.spv shaders/vert.spv

shaders/frag.spv: shaders/shader.frag
	${SHADERCC} -o $@ $<
	
shaders/vert.spv: shaders/shader.vert
	${SHADERCC} -o $@ $<

triangles.o: triangles.c vertex.h

vertex.o: vertex.c vertex.h

triangles: triangles.o vertex.o
