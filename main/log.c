/**
 * @file log.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"

#define MAX_LINES  100
static char *lines[MAX_LINES] = { NULL };
static uint32_t w  =  0;
static uint32_t r  =  0;
static uint32_t r0 =  0;

static uint32_t rr(uint32_t idx)
{
    idx++;
    if (idx == MAX_LINES)
        idx = 0;

    return idx;
}


static size_t va_list_size(const char *fmt, va_list ap)
{
    size_t size = 0;
    const char *s;

    while (*fmt) {
        switch (*fmt++) {
            case 's':              /* string */
                s = va_arg(ap, char *);
                size += strlen(s);
                break;
            case 'd':              /* int */
            case 'i':
            case 'u':
                size += 10;
                break;
            case 'f':              /* float */
                size += 10;
                break;
            case 'c':              /* char */
                size++;
                break;
            case 'x':
            case 'X':
                size += 4;
                break;
        }
    }

    return size;
}


void logw(const char *fmt, ...)
{
    va_list ap;
    size_t l;

    if (!fmt)
        return;

    va_start(ap, fmt);
    l = va_list_size(fmt, ap);
    va_end(ap);
    l += strlen(fmt) + 1;

    free(lines[w]);
    lines[w] = malloc(l);
    va_start(ap, fmt);
    vsnprintf(lines[w], l, fmt, ap);
    va_end(ap);

    w = rr(w);

    if (r == w)
        r = rr(r);

    if (r0 == w)
        r0 = rr(r0);
}


const char *logr(void)
{
    const char *ret = NULL;

    if (r < w) {
        ret = lines[r];
        r = rr(r);
    }

    return ret;
}


void log_rewind(void)
{
    r = r0;
}


void log_clear(void)
{
    size_t i;

    for (i = 0; i < MAX_LINES; i++) {
        free(lines[i]);
        lines[i] = NULL;
    }

    r = r0 = w = 0;
}
