#ifndef DYNLOAD_H
#define DYNLOAD_H

/* только при -DMODE_DYNAMIC и после заголовков библиотек
 * макросы подменяют FT_New_Face и остальные имена на указатели ниже */

#include <ft2build.h>
#include FT_FREETYPE_H
#include <png.h>
#include <zlib.h>

int load_libraries(void);
void unload_libraries(void);

/* адреса из dlsym, макросы ниже подставляют их вместо обычных имён */
extern FT_Error (*p_FT_Init_FreeType)(FT_Library *);
extern FT_Error (*p_FT_New_Face)(FT_Library, const char *, FT_Long, FT_Face *);
extern FT_Error (*p_FT_Set_Pixel_Sizes)(FT_Face, FT_UInt, FT_UInt);
extern FT_UInt (*p_FT_Get_Char_Index)(FT_Face, FT_ULong);
extern FT_Error (*p_FT_Load_Glyph)(FT_Face, FT_UInt, FT_Int32);
extern FT_Error (*p_FT_Done_Face)(FT_Face);
extern FT_Error (*p_FT_Done_FreeType)(FT_Library);
extern const char *(*p_zlibVersion)(void);

extern png_structp (*p_png_create_write_struct)(png_const_charp, png_voidp,
                                                png_error_ptr, png_error_ptr);
extern png_infop (*p_png_create_info_struct)(png_const_structrp);
extern void (*p_png_init_io)(png_structrp, png_FILE_p);
extern void (*p_png_set_IHDR)(png_const_structrp, png_inforp, png_uint_32,
                              png_uint_32, int, int, int, int, int);
extern jmp_buf *(*p_png_set_longjmp_fn)(png_structrp, png_longjmp_ptr, size_t);
extern void (*p_png_write_info)(png_structrp, png_const_inforp);
extern void (*p_png_write_image)(png_structrp, png_bytepp);
extern void (*p_png_write_end)(png_structrp, png_inforp);
extern void (*p_png_destroy_write_struct)(png_structpp, png_infopp);

#define FT_Init_FreeType p_FT_Init_FreeType
#define FT_New_Face p_FT_New_Face
#define FT_Set_Pixel_Sizes p_FT_Set_Pixel_Sizes
#define FT_Get_Char_Index p_FT_Get_Char_Index
#define FT_Load_Glyph p_FT_Load_Glyph
#define FT_Done_Face p_FT_Done_Face
#define FT_Done_FreeType p_FT_Done_FreeType
#define zlibVersion p_zlibVersion
#define png_create_write_struct p_png_create_write_struct
#define png_create_info_struct p_png_create_info_struct
#define png_init_io p_png_init_io
#define png_set_IHDR p_png_set_IHDR
#define png_set_longjmp_fn p_png_set_longjmp_fn
#define png_write_info p_png_write_info
#define png_write_image p_png_write_image
#define png_write_end p_png_write_end
#define png_destroy_write_struct p_png_destroy_write_struct

#endif
