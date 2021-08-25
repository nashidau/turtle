
VULKANCFLAGS=/
VULKANLIBS=-lvulkan 

PKGS=talloc glfw3 check
CTAGS=/opt/homebrew/bin/ctags

CFLAGS+=-g -O2 -Wall -Wextra `pkg-config --cflags ${PKGS}` -F /Library/Frameworks \
	-iframework /Library/Frameworks  -Icglm/include
LDFLAGS+=`pkg-config --libs ${PKGS}` -lvulkan -framework Cocoa -framework IOSurface \
	 -framework IOKit -framework CoreGraphics -framework QuartzCore -lstdc++ -framework Metal

SHADERCC=glslc

TESTS=	\
	trtl_uniform.o \
	helpers.o \
	turtle.o \
	trtl_uniform_check.o

SOURCES= \
	triangles.c	\
	vertex.c	\
	images.c	\
	stringlist.c	\
	helpers.c	\
	objloader.c	\
	blobby.c	\
	trtl_uniform.c	\
	trtl_object_mesh.c	\
	trtl_object_canvas.c	\
	trtl_pipeline.c	\
	trtl_seer.c	\
	turtle.c

OBJECTS := $(SOURCES:%.c=%.o)


ALL: triangles trtl_check shaders/frag.spv shaders/vert.spv shaders/canvas/test-color-fill.spv \
	shaders/canvas/stars-1.spv


# Dependancies (from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#tldr)
DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o : %.c
%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SOURCES:%.c=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))

%.spv : %.frag
	${SHADERCC} -o $@ $<


trtl_check: trtl_check.o ${TESTS}

shaders/frag.spv: shaders/shader.frag
	${SHADERCC} -o $@ $<
	
shaders/vert.spv: shaders/shader.vert
	${SHADERCC} -o $@ $<

triangles.o: triangles.c vertex.h

vertex.o: vertex.c vertex.h

triangles: ${OBJECTS}

.PHONY: clean
clean:
	rm -f ${OBJECTS} triangles ${SHADERS}

.PHONY: fixme
fixme:
	@grep -in 'fixme\|todo\|xxx' *.[ch] | sort -R

.PHONY: tags
tags:
	${CTAGS} *.c *.h cglm/include/cglm/*.h
