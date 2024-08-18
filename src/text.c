#include "text.h"
#include "utils.h"


// TODO: I should consider using the JSON metadata files that can be
//  produced by msdf-atlas-gen rather than CSV.  The JSON files have
//  pxrange and other useful global metadata (eg lineHeight, which I'm
//  currently hard-coding) stored in them so I don't have to awkwardly
//  pass them to loadFont.
int loadFont(Font *result, const char *basePath, const char *fontPath, char32_t missing, float pxrange) {
    char *filePath;
    if (SET_ERR_IF_TRUE(SDL_asprintf(&filePath, "%s%s.bmp", basePath, fontPath) == -1)) {
        return -1;
    }

    SDL_RWops *file = SDL_RWFromFile(filePath, "r");
    SDL_free(filePath);
    if (file == NULL) return 1;
    SDL_Surface *surf = SDL_LoadBMP_RW(file, 1);
    if (surf == NULL) return 1;

    GLenum format = GL_BGR;
    if (surf->format->BytesPerPixel == 4) format = GL_BGRA;
    else if (SET_ERR_IF_TRUE(surf->format->BytesPerPixel != 3)) {
        SDL_FreeSurface(surf);
        return 2;
    }

    int atlWidth = surf->w, atlHeight = surf->h;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlWidth, atlHeight, 0, format, GL_UNSIGNED_BYTE, surf->pixels);
    SDL_FreeSurface(surf);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (SET_ERR_IF_TRUE(SDL_asprintf(&filePath, "%s%s.csv", basePath, fontPath) == -1)) {
        glDeleteTextures(1, &texture);
        return -1;
    }

    char *csv;
    file = SDL_RWFromFile(filePath, "r");
    SDL_free(filePath);
    if (file == NULL || (csv = SDL_LoadFile_RW(file, NULL, 1)) == NULL) {
        glDeleteTextures(1, &texture);
        return 1;
    }

    size_t numLines = 1;
    for (char *cur = csv; (cur = strchr(cur, '\n'));) {
        cur++;
        if (*cur) numLines++;
    }

    // SDL_Log("%s.csv: %d lines", fontPath, numLines);
    Glyph *glyphs = (Glyph *) malloc(numLines * sizeof(Glyph));

    size_t missingIndex = -1;
    size_t numGlyphs = 0;
    unsigned long prevId = -1;
    for (char *cur = csv; *cur && numGlyphs < numLines; numGlyphs++) {
        int numBytes;
        unsigned long code;
        float advance;
        float posLeft, posBot, posRight, posTop;
        float atlLeft, atlBot, atlRight, atlTop;
        int numVals = sscanf(
            cur, "%lu ,%f ,%f ,%f ,%f ,%f ,%f ,%f ,%f ,%f%n",
            &code, &advance,
            &posLeft, &posBot, &posRight, &posTop,
            &atlLeft, &atlBot, &atlRight, &atlTop,
            &numBytes
        );

        if (numVals != 10 || prevId + 1 > code) break;
        if (code == missing) missingIndex = numGlyphs;
        glyphs[numGlyphs] = (Glyph) {
            .code = code, .advance = advance,

            .bbox.x = posLeft, .bbox.y = posBot,
            .bbox.w = posRight - posLeft, .bbox.h = posTop - posBot,

            .uv.x = atlLeft / (float)atlWidth, .uv.y = 1.f - atlBot / (float)atlHeight,
            .uv.w = (atlRight - atlLeft) / (float)atlWidth,
            .uv.h = -(atlTop - atlBot) / (float)atlHeight
        };

        cur += numBytes;
        prevId = code;
    }

    SDL_free(csv);
    if (SET_ERR_IF_TRUE(numGlyphs != numLines) || SET_ERR_IF_TRUE(missingIndex == -1)) {
        free(glyphs);
        glDeleteTextures(1, &texture);
        return 2;
    }

    result->glyphs = glyphs;
    result->numGlyphs = numGlyphs;
    result->atlas = texture;
    result->missing = glyphs[missingIndex];
    result->pxrange = pxrange;

    return 0;
}


void destroyFont(Font *font) {
    font->numGlyphs = 0;
    if (font->glyphs) free(font->glyphs);
    glDeleteTextures(1, &font->atlas);
    font->atlas = 0;
}


Glyph *findGlyph(char32_t code, size_t numGlyphs, Glyph *glyphs) {
    // Since I expect most characters to be in the large contiguous
    // ASCII section at the beginning, for my purposes it seems most
    // sensible to first try a direct lookup from the beginning of the
    // array and then linear search starting from the end if that fails.

    if (!numGlyphs) return NULL;
    uint32_t guess = code - glyphs->code;
    if (guess < numGlyphs && glyphs[guess].code == code) {
        return &glyphs[guess];
    }

    for (size_t i = numGlyphs; i--;) {
        if (glyphs[i].code == code) return &glyphs[i];
    }

    return NULL;
}


static ProgDrawGlyph *activeRenderer = NULL;
static Font *activeFont = NULL;
void useFont(Font *font, ProgDrawGlyph *renderer, GLint atlasTexUnit) {
    glUseProgram(renderer->prog.id);
    glActiveTexture(GL_TEXTURE0 + atlasTexUnit);
    glBindTexture(GL_TEXTURE_2D, font->atlas);
    glUniform1i(renderer->u_atlas, atlasTexUnit);
    glUniform1f(renderer->u_pxrange, font->pxrange);

    activeRenderer = renderer;
    activeFont = font;
}


void drawGlyph(Cursor *cursor, Glyph *glyph) {
    if (activeRenderer == NULL || activeFont == NULL) return;
    if (glyph == NULL) glyph = &activeFont->missing;

    double x = (cursor->x + cursor->size * glyph->bbox.x)/cursor->viewWidth;
    double y = (cursor->y + cursor->size * glyph->bbox.y)/cursor->viewHeight;
    double w = cursor->size * glyph->bbox.w / cursor->viewWidth;
    double h = cursor->size * glyph->bbox.h / cursor->viewHeight;

    glUniform2f(activeRenderer->u_ndcPos, (GLfloat)(2. * x - 1.), (GLfloat)(2. * y - 1.));
    glUniform2f(activeRenderer->u_ndcSize, (GLfloat)(2. * w), (GLfloat)(2. * h));

    glUniform2f(activeRenderer->u_atlasPos, (GLfloat)glyph->uv.x, glyph->uv.y);
    glUniform2f(activeRenderer->u_atlasSize, (GLfloat)glyph->uv.w, glyph->uv.h);

    drawQuad();
    cursor->x += glyph->advance * cursor->size;
}


void drawChar(Cursor *cursor, char32_t code) {
    if (activeFont == NULL) return;
    Glyph *glyph;
    float space = 1.f;
    switch (code) {
        case '\n':
            cursor->y -= 1.22f * cursor->size;  // TODO: depend on font
        case '\r':
            cursor->x = cursor->left;
            break;

        case '\t':
            space = 4.f;  // tab = 4 spaces, would be nicer to do tabstops though
        case ' ':
            glyph = findGlyph(' ', activeFont->numGlyphs, activeFont->glyphs);
            if (glyph == NULL) space *= 0.5f * cursor->size;
            else space *= glyph->advance * cursor->size;
            cursor->x += space;
            break;

        default:
            glyph = findGlyph(code, activeFont->numGlyphs, activeFont->glyphs);
            drawGlyph(cursor, glyph);  // drawGlyph handles NULL
    }
}


void drawString(Cursor *cursor, const char *str) {
    for (int i = 0; str[i]; i++) {
        // TODO: Ideally we should treat str as utf-8 encoded
        drawChar(cursor, str[i]);
    }
}


// TODO: This is a bit silly, maybe would be better to have a function
//  to create a fixed-num variant of the font itself?
void drawStringFixedNum(Cursor *cursor, const char *str) {
    Glyph *zero = findGlyph('0', activeFont->numGlyphs, activeFont->glyphs);
    if (!zero) {
        drawString(cursor, str);
        return;
    }

    for (int i = 0; str[i]; i++) {
        char code = str[i];
        if (isdigit(code)) {
            Glyph *digit = findGlyph(code, activeFont->numGlyphs, activeFont->glyphs);
            if (digit) {
                float pad = (zero->bbox.w - digit->bbox.w) / 2.f;
                Glyph fixed = {
                    .code = code, .advance = zero->advance, .uv = digit->uv,
                    .bbox = {
                        .x = digit->bbox.x + pad, .y = digit->bbox.y,
                        .w = digit->bbox.w, .h = digit->bbox.h
                    }
                };
                drawGlyph(cursor, &fixed);
                continue;
            }
        }

        drawChar(cursor, str[i]);
    }
}


float emWidth(Font *font, const char *str) {
    float maxWidth = 0.f;
    float width = 0.f;
    for (int i = 0; str[i]; i++) {
        char32_t code = (char32_t)str[i];
        if (code == '\n' || code == '\r') {
            if (width > maxWidth) maxWidth = width;
            width = 0.f;
            continue;
        }

        float mul = 1.f;
        if (code == '\t') {
            code = ' ';
            mul = 4.f;
        }

        Glyph *glyph = findGlyph(code, font->numGlyphs, font->glyphs);
        if (!glyph) glyph = &font->missing;
        width += mul * glyph->advance;
    }

    if (width > maxWidth) maxWidth = width;
    return maxWidth;
}
