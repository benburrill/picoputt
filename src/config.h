#ifndef PICOPUTT_CONFIG_H
#define PICOPUTT_CONFIG_H

// GL_TIME_ELAPSED requires OpenGL 3.3
// I don't think I'm using anything that requires a newer version.
// Unfortunately, since there are no errors from GLEW if you try to use
// features from a newer version, I'll need to read the docs carefully.
// I may also want to consider using GLAD instead.  The 3.2 GLAD loader
// simply doesn't have a GL_TIME_ELAPSED define, so many such GL version
// problems would be obvious at compile time.
#define TARGET_GL_MAJOR_VERSION 3
#define TARGET_GL_MINOR_VERSION 3
#define GLSL_HEADER "#version 330\n"
#endif //PICOPUTT_CONFIG_H
