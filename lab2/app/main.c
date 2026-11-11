#include "funcs.h"

#include <stdio.h>
#include <time.h>

#ifdef MODE_DYNAMIC
#include "dynload.h"
#endif
#ifdef MODE_BLOB
#include "host_api.h"
#endif

/* Вычисляет разницу между двумя моментами времени в секундах */
static double seconds_between(const struct timespec *a, const struct timespec *b)
{
    return (double)(b->tv_sec - a->tv_sec) +
           (double)(b->tv_nsec - a->tv_nsec) / 1000000000.0;
}

/* Печатает разницу между двумя моментами времени в секундах */
static void print_span(const char *label, const struct timespec *from,
                       const struct timespec *to)
{
    printf("%s: %.6f s\n", label, seconds_between(from, to));
}

#ifdef MODE_BLOB

int blob_entry(struct blob_params *params)
{
    struct timespec start;
    struct timespec loaded;
    struct timespec now;

    if (!params || !params->font_path || !params->png_path || !params->text ||
        !params->api.vprintf)
        return 0;

    /* Устанавливаем API хоста для blob */
    host_install(&params->api);
    clock_gettime(CLOCK_MONOTONIC, &start);
    loaded = start;
    print_span("load", &start, &loaded);
    render_png(params->font_path, params->png_path, params->text);
    clock_gettime(CLOCK_MONOTONIC, &now);
    print_span("total", &start, &now);
    return 0;
}

#else

int main(int argc, char **argv)
{
    struct timespec start;
    struct timespec loaded;
    struct timespec now;
    const char *program = (argc > 0 && argv[0]) ? argv[0] : "app"; /* имя в usage */

    /* Засекаем время начала работы программы */
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (argc != 4)
    {
        printf("usage: %s font png text\n", program);
        print_span("load", &start, &start);
        print_span("total", &start, &start);
        return 0;
    }

#ifdef MODE_DYNAMIC
    if (!load_libraries())
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
        print_span("load", &start, &now);
        print_span("total", &start, &now);
        return 0;
    }
#endif

    clock_gettime(CLOCK_MONOTONIC, &loaded);
    print_span("load", &start, &loaded);
    render_png(argv[1], argv[2], argv[3]);
    clock_gettime(CLOCK_MONOTONIC, &now);
    print_span("total", &start, &now);

#ifdef MODE_DYNAMIC
    unload_libraries();
#endif
    return 0;
}

#endif
