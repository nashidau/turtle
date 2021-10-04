

#define STB_IMAGE_IMPLEMENTATION 1

// We disbable this on github
#ifndef GITHUB
#define STBI_NEON 1
#endif
#include "stb/stb_image.h"
