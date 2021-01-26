

VULKANCFLAGS=/
VULKANLIBS=-lvulkan 



PKGS=talloc glfw3

CFLAGS+=`pkg-config --cflags ${PKGS}` -F /Library/Frameworks -iframework /Library/Frameworks -framework Cocoa -framework IOSurface -framework IOKit -framework CoreGraphics -framework QuartzCore -lstdc++ -framework Metal
LDFLAGS+=`pkg-config --libs ${PKGS}` /Library/Frameworks/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a

SHADERCC=glslc

ALL: triangles shaders/frag.spv shaders/vert.spv

shaders/frag.spv: shaders/shader.frag
	${SHADERCC} -o $@ $<
	
shaders/vert.spv: shaders/shader.vert
	${SHADERCC} -o $@ $<

triangles: triangles.c
	${CC} ${CFLAGS} -o $@ $< ${LDFLAGS}
