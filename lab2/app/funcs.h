#ifndef FUNCS_H
#define FUNCS_H

/* 0 — PNG записан. -1 — ошибка уже напечатана, программа не падает. */
int render_png(const char *font_path, const char *png_path, const char *text);

#endif
