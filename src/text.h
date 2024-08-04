#ifndef PICOPUTT_TEXT_H
#define PICOPUTT_TEXT_H
#include <stddef.h>
#include <uchar.h>
#include <GL/glew.h>
#include <SDL.h>
#include "shaders.h"

typedef struct {
    char32_t code;
    float advance;
    SDL_FRect bbox;  // Glyph bounding box, in em units
    SDL_FRect uv;    // UV coordinates of the glyph in the atlas
} Glyph;


typedef struct {
    size_t numGlyphs;
    Glyph *glyphs;
    Glyph missing;
    GLuint atlas;
    // TODO: pxrange wouldn't be meaningful for non-SDF fonts.
    //  If I want to support non-SDF fonts I may want to rethink this
    //  structure.  Likewise for ProgDrawGlyph I justified NOT including
    //  u_color to myself based on the idea that some "fonts" could be
    //  spritesheets with predetermined color, but including u_pxrange
    //  ruins that justification.  The real reason is that u_pxrange is
    //  set by useFont, but u_color (and other style uniforms) are
    //  intended to be manually managed by the caller.
    float pxrange;
} Font;


typedef struct {
    Program prog;
    GLint u_atlas;
    GLint u_ndcPos;
    GLint u_ndcSize;
    GLint u_atlasPos;
    GLint u_atlasSize;
    GLint u_pxrange;
} ProgDrawGlyph;


typedef struct {
    float left;
    float x;
    float y;
    float size;
    // viewWidth and viewHeight should have the same aspect ratio as glViewport,
    // but should have the same units as size (typically screen units)
    float viewWidth;
    float viewHeight;
} Cursor;


int loadFont(Font *result, const char *basePath, const char *fontPath, char32_t missing, float pxrange);
void destroyFont(Font *font);
void drawGlyph(Cursor *cursor, Glyph *glyph);
void drawChar(Cursor *cursor, char32_t code);
void drawString(Cursor *cursor, const char *str);
void useFont(Font *font, ProgDrawGlyph *renderer, GLint atlasTexUnit);
Glyph *findGlyph(char32_t code, size_t numGlyphs, Glyph *glyphs);
float emWidth(Font *font, const char *str);
void drawStringFixedNum(Cursor *cursor, const char *str);

#endif //PICOPUTT_TEXT_H
