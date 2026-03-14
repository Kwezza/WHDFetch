#ifndef TAG_TEXT_H
#define TAG_TEXT_H


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdarg.h>
#include "cli_utilities.h"

void print_tagged_text(const char **tagged_text_array);

#define INITIAL_LINES 16

typedef struct {
    char **lines;
    int count;
    int capacity;
} TextBuilder;

char *pad_number_with_dots(int number, int total_length);
BOOL init_text_builder(TextBuilder *tb);
void add_line(TextBuilder *tb, const char *line);
void finalize_text_builder(TextBuilder *tb);
void free_text_builder(TextBuilder *tb);
BOOL addf_line(TextBuilder *tb, const char *fmt, ...);
#endif /* TAG_TEXT_H */