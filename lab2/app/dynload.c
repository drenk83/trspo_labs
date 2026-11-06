#include "dynload.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FT_Error (*p_FT_Init_FreeType)(FT_Library *);
FT_Error (*p_FT_New_Face)(FT_Library, const char *, FT_Long, FT_Face *);
FT_Error (*p_FT_Set_Pixel_Sizes)(FT_Face, FT_UInt, FT_UInt);
FT_UInt (*p_FT_Get_Char_Index)(FT_Face, FT_ULong);
FT_Error (*p_FT_Load_Glyph)(FT_Face, FT_UInt, FT_Int32);
FT_Error (*p_FT_Done_Face)(FT_Face);
FT_Error (*p_FT_Done_FreeType)(FT_Library);
const char *(*p_zlibVersion)(void);

png_structp (*p_png_create_write_struct)(png_const_charp, png_voidp,
                                         png_error_ptr, png_error_ptr);
png_infop (*p_png_create_info_struct)(png_const_structrp);
void (*p_png_init_io)(png_structrp, png_FILE_p);
void (*p_png_set_IHDR)(png_const_structrp, png_inforp, png_uint_32, png_uint_32,
                       int, int, int, int, int);
jmp_buf *(*p_png_set_longjmp_fn)(png_structrp, png_longjmp_ptr, size_t);
void (*p_png_write_info)(png_structrp, png_const_inforp);
void (*p_png_write_image)(png_structrp, png_bytepp);
void (*p_png_write_end)(png_structrp, png_inforp);
void (*p_png_destroy_write_struct)(png_structpp, png_infopp);

static void *libz;     /* libz.so.1, его грузим первым */
static void *libpng;   /* libpng.so, ему нужен уже загруженный zlib */
static void *freetype; /* libfreetype.so, тоже зависит от zlib */

/* .so ищем рядом с бинарником, а не в текущей папке */
static void *open_beside(const char *filename)
{
    char exe[PATH_MAX];  /* путь к самому бинарнику из /proc/self/exe */
    char path[PATH_MAX]; /* каталог бинарника плюс имя .so */
    ssize_t length;      /* сколько байт вернул readlink */
    char *slash;         /* последний /, отрезает имя файла */
    void *handle;        /* результат dlopen */

    length = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (length < 0) {
        printf("cannot read /proc/self/exe\n");
        return NULL;
    }
    exe[length] = '\0';
    slash = strrchr(exe, '/');
    if (!slash) {
        printf("cannot resolve path to %s\n", filename);
        return NULL;
    }
    *slash = '\0';
    if (snprintf(path, sizeof path, "%s/%s", exe, filename) >= (int)sizeof path) {
        printf("path to %s is too long\n", filename);
        return NULL;
    }

    /* NOW — проверить символы сразу, GLOBAL — zlib виден libpng и freetype */
    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
        printf("dlopen %s: %s\n", path, dlerror());
    return handle;
}

static int bind(void *lib, const char *name, void *slot)
{
    void *symbol; /* адрес функции внутри .so */

    dlerror();
    symbol = dlsym(lib, name);
    if (dlerror() != NULL || symbol == NULL) {
        printf("dlsym %s failed\n", name);
        return 0;
    }
    /* dlsym отдаёт void *, в слот кладём тот же адрес, что у функции */
    memcpy(slot, &symbol, sizeof symbol);
    return 1;
}

void unload_libraries(void)
{
    if (freetype)
        dlclose(freetype);
    if (libpng)
        dlclose(libpng);
    if (libz)
        dlclose(libz);
    freetype = NULL;
    libpng = NULL;
    libz = NULL;
}

int load_libraries(void)
{
    int ok = 1;

    /* zlib раньше остальных: libpng.so и libfreetype.so ищут его символы */
    libz = open_beside("libz.so.1");
    libpng = libz ? open_beside("libpng.so") : NULL;
    freetype = libpng ? open_beside("libfreetype.so") : NULL;
    if (!libz || !libpng || !freetype) {
        unload_libraries();
        return 0;
    }

    ok &= bind(libz, "zlibVersion", &p_zlibVersion);
    ok &= bind(freetype, "FT_Init_FreeType", &p_FT_Init_FreeType);
    ok &= bind(freetype, "FT_New_Face", &p_FT_New_Face);
    ok &= bind(freetype, "FT_Set_Pixel_Sizes", &p_FT_Set_Pixel_Sizes);
    ok &= bind(freetype, "FT_Get_Char_Index", &p_FT_Get_Char_Index);
    ok &= bind(freetype, "FT_Load_Glyph", &p_FT_Load_Glyph);
    ok &= bind(freetype, "FT_Done_Face", &p_FT_Done_Face);
    ok &= bind(freetype, "FT_Done_FreeType", &p_FT_Done_FreeType);
    ok &= bind(libpng, "png_create_write_struct", &p_png_create_write_struct);
    ok &= bind(libpng, "png_create_info_struct", &p_png_create_info_struct);
    ok &= bind(libpng, "png_init_io", &p_png_init_io);
    ok &= bind(libpng, "png_set_IHDR", &p_png_set_IHDR);
    ok &= bind(libpng, "png_set_longjmp_fn", &p_png_set_longjmp_fn);
    ok &= bind(libpng, "png_write_info", &p_png_write_info);
    ok &= bind(libpng, "png_write_image", &p_png_write_image);
    ok &= bind(libpng, "png_write_end", &p_png_write_end);
    ok &= bind(libpng, "png_destroy_write_struct", &p_png_destroy_write_struct);

    if (!ok)
        unload_libraries();
    return ok;
}
