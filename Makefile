
VULKANCFLAGS=/
VULKANLIBS=-lvulkan 

PKGS=talloc glfw3 check
CTAGS=/opt/homebrew/bin/ctags

# From: https://airbus-seclab.github.io/c-compiler-security/#clang-tldr
# Removed: -Wfloat-equal <- too many floats
# Removed: -Wcast-qual <- stb image is super unhappy about this
# Not yet: -Wconversion; many of them
# Not yet: -Wimplicit-fallthrough
# Not yet; built in is set to '0'; -D_FORTIFY_SOURCE=2
WARNINGS= \
	-Werror \
	-Wall -Wextra \
	-Walloca -Wformat=2 -Wformat-security -Wnull-dereference \
	-Wstack-protector -Wstrict-overflow=3 -Wvla -Warray-bounds \
	-Warray-bounds-pointer-arithmetic -Wassign-enum -Wbad-function-cast \
	-Wconditional-uninitialized  -Wformat-type-confusion \
	-Widiomatic-parentheses -Wloop-analysis -Wpointer-arith \
	-Wshift-sign-overflow -Wshorten-64-to-32 -Wswitch-enum \
	-Wtautological-constant-in-range-compare -Wunreachable-code-aggressive \
	-fstack-protector-strong

OPTIMIZATION=-O2

CFLAGS+=-g ${OPTIMIZATION} ${WARNINGS} `pkg-config --cflags ${PKGS}` -F /Library/Frameworks \
	-iframework /Library/Frameworks  -Ithird-party/cglm/include -fsanitize=address
# -all_load is only needed for a static library; move to dynamicl and it goes away
LDFLAGS+=`pkg-config --libs ${PKGS}` -lvulkan -framework Cocoa -framework IOSurface \
	 -framework IOKit -framework CoreGraphics -framework QuartzCore -lstdc++ -framework Metal \
	 -fsanitize=address -rdynamic -all_load

SHADERCC=glslc

TESTS=	\
	trtl_uniform.o \
	helpers.o \
	turtle.o \
	trtl_uniform_check.o

SOURCES= \
	vertex.c	\
	images.c	\
	stringlist.c	\
	helpers.c	\
	objloader.c	\
	blobby.c	\
	trtl_barriers.c \
	trtl_uniform.c	\
	trtl_object_mesh.c	\
	trtl_object_canvas.c	\
	trtl_object_grid.c	\
	trtl_pipeline.c	\
	trtl_scribe.c   \
	trtl_shader.c	\
	trtl_shell.c    \
	trtl_seer.c	\
	trtl_solo.c	\
	trtl_texture.c	\
	trtl_vulkan.c	\
	turtle.c

APPS=triangles


SHADERS= \
	shaders/frag.spv \
	shaders/vert.spv \
	shaders/canvas/test-color-fill.spv \
	shaders/canvas/stars-1.spv \
	shaders/canvas/canvas-vertex.spv \
	shaders/grid/lines.spv \
	shaders/grid/grid-vertex.spv \
	shaders/grid/browns.spv \
	shaders/grid/red.spv

OBJECTS := $(SOURCES:%.c=%.o)
SHADER_OBJECTS := $(SHADERS:%.spv=%.o)


ALL: triangles libturtle.a trtl_check ${SHADERS} 

# Dependancies (from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#tldr)
DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	@echo Compile $<
	@$(COMPILE.c) $(OUTPUT_OPTION) $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SOURCES:%.c=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))

%.spv : %.frag
	@echo Shader Compile $<
	@${SHADERCC} -o $@ $<

%.spv : %.vert
	@echo Shader Compile $<
	@${SHADERCC} -o $@ $<

%.s: SYMBOLNAME=$(subst -,_,$(subst .,_,$(subst /,_,$<)))
%.s: %.spv
	@echo Generate SPV binary $@
	@echo "\t.global _data_start_${SYMBOLNAME}" > $@
	@echo "\t.global _data_end_${SYMBOLNAME}" >> $@
	@echo "\t.align 2" >> $@
	@echo "_data_start_${SYMBOLNAME}:" >> $@
	@echo "\t.incbin \"$<\"" >> $@
	@echo "_data_end_${SYMBOLNAME}:" >> $@

# FIXME: Fix this dependancy
shaders/frag.spv: shaders/shader.frag
	@echo Shader Compile $<
	@${SHADERCC} -o $@ $<
	
shaders/vert.spv: shaders/shader.vert
	${SHADERCC} -o $@ $<

trtl_check: trtl_check.o ${TESTS} ${OBJECTS}

libturtle.a: ${OBJECTS} ${SHADER_OBJECTS}
	@echo Link #<
	@ar ru $@ $^
	@ranlib $@

triangles: triangles.o libturtle.a

.PHONY: clean
clean:
	rm -f ${OBJECTS} triangles ${SHADERS} ${SHADER_OBJECTS}

.PHONY: fixme
fixme:
	@grep -in 'fixme\|todo\|xxx' *.[ch] | sort -R

.PHONY: tags
tags:
	${CTAGS} *.c *.h cglm/include/cglm/*.h
