#ifndef PICOPUTT_CONFIG_H
#define PICOPUTT_CONFIG_H

// Version requirements of some critical features:
// Compute shaders require OpenGL 4.3
// GL_TIME_ELAPSED requires OpenGL 3.3

#define TARGET_GL_MAJOR_VERSION 4
#define TARGET_GL_MINOR_VERSION 3
#define GLSL_HEADER "#version 430\n"
#endif //PICOPUTT_CONFIG_H
