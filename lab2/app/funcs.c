#include "funcs.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef MODE_DYNAMIC
#include "dynload.h"
#endif

#define FONT_PX 48
#define PAD 8
#define TEXT_LIMIT 200

static int write_png(const char *path, unsigned char *image, int width, int height)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info;
    /* longjmp откатывает регистры. Без volatile указатель rows после ошибки был бы потерян. */
    png_bytep * volatile rows = NULL;

    fp = fopen(path, "wb");
    if (!fp) {
        printf("cannot create png '%s'\n", path);
        return -1;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        printf("png_create_write_struct failed\n");
        fclose(fp);
        return -1;
    }
    info = png_create_info_struct(png_ptr);
    if (!info) {
        printf("png_create_info_struct failed\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf)))) {
        printf("png write failed for '%s'\n", path);
        free(rows);
        png_destroy_write_struct(&png_ptr, &info);
        fclose(fp);
        return -1;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info, (png_uint_32)width, (png_uint_32)height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    rows = malloc((size_t)height * sizeof(png_bytep));
    if (!rows) {
        printf("out of memory\n");
        png_destroy_write_struct(&png_ptr, &info);
        fclose(fp);
        return -1;
    }
    for (int y = 0; y < height; y++)
        rows[y] = image + (size_t)y * (size_t)width * 3;

    png_write_info(png_ptr, info);
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, info);

    free(rows);
    rows = NULL;
    png_destroy_write_struct(&png_ptr, &info);
    if (fclose(fp) != 0) {
        printf("cannot close png '%s'\n", path);
        return -1;
    }
    return 0;
}

/* Нарисовать один серый глиф в RGB-буфер. Белый фон, символ темнеет по coverage. */
static void paint_glyph(unsigned char *image, int width, int height,
                        int pen_x, int baseline, FT_GlyphSlot glyph)
{
    FT_Bitmap *bitmap = &glyph->bitmap;
    int pitch;

    if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY || !bitmap->buffer)
        return;

    pitch = bitmap->pitch < 0 ? -bitmap->pitch : bitmap->pitch;
    for (unsigned int row = 0; row < bitmap->rows; row++) {
        int y = baseline - glyph->bitmap_top + (int)row;
        if (y < 0 || y >= height)
            continue;
        for (unsigned int col = 0; col < bitmap->width; col++) {
            int x = pen_x + glyph->bitmap_left + (int)col;
            unsigned char *pixel;
            unsigned char coverage;
            if (x < 0 || x >= width)
                continue;
            coverage = bitmap->buffer[row * (unsigned int)pitch + col];
            pixel = image + ((size_t)y * (size_t)width + (size_t)x) * 3;
            pixel[0] = pixel[1] = pixel[2] = (unsigned char)(255 - coverage);
        }
    }
}

int render_png(const char *font_path, const char *png_path, const char *text)
{
    FT_Library library = NULL;
    FT_Face face = NULL;
    unsigned char *image = NULL;
    int failed = 0;
    int count = 0;
    int width = PAD;
    int ascent;
    int descent;
    int height;
    int baseline;
    int pen_x = PAD;

    if (!font_path || !png_path || !text) {
        printf("render_png: missing argument\n");
        return -1;
    }

    printf("zlib %s\n", zlibVersion());

    if (FT_Init_FreeType(&library) != 0) {
        printf("FT_Init_FreeType failed\n");
        return -1;
    }
    if (FT_New_Face(library, font_path, 0, &face) != 0) {
        printf("cannot open font '%s'\n", font_path);
        failed = 1;
        goto done;
    }
    if (FT_Set_Pixel_Sizes(face, 0, FONT_PX) != 0) {
        printf("FT_Set_Pixel_Sizes failed\n");
        failed = 1;
        goto done;
    }

    while (text[count] != '\0' && count < TEXT_LIMIT)
        count++;

    /* 26.6: единица FreeType — 1/64 пикселя, поэтому сдвиг на 6. */
    ascent = (int)(face->size->metrics.ascender >> 6);
    descent = (int)(-(face->size->metrics.descender >> 6));
    if (ascent < 1)
        ascent = FONT_PX;
    if (descent < 0)
        descent = 0;
    height = ascent + descent + PAD * 2;
    baseline = PAD + ascent;

    for (int i = 0; i < count; i++) {
        FT_UInt index = FT_Get_Char_Index(face, (unsigned char)text[i]);
        if (index == 0 || FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) != 0) {
            width += FONT_PX / 2;
            continue;
        }
        width += (int)(face->glyph->advance.x >> 6);
    }
    width += PAD;

    image = malloc((size_t)height * (size_t)width * 3);
    if (!image) {
        printf("out of memory\n");
        failed = 1;
        goto done;
    }
    memset(image, 0xFF, (size_t)height * (size_t)width * 3);

    for (int i = 0; i < count; i++) {
        FT_UInt index = FT_Get_Char_Index(face, (unsigned char)text[i]);
        int advance = FONT_PX / 2;
        if (index != 0 && FT_Load_Glyph(face, index, FT_LOAD_RENDER) == 0) {
            paint_glyph(image, width, height, pen_x, baseline, face->glyph);
            advance = (int)(face->glyph->advance.x >> 6);
            if (advance <= 0)
                advance = FONT_PX / 2;
        }
        pen_x += advance;
    }

    if (write_png(png_path, image, width, height) != 0)
        failed = 1;

done:
    free(image);
    if (face)
        FT_Done_Face(face);
    if (library)
        FT_Done_FreeType(library);
    return failed ? -1 : 0;
}
