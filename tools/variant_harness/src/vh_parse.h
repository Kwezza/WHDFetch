#ifndef VH_PARSE_H
#define VH_PARSE_H

#include <stddef.h>

#include "vh_csv.h"

#define VH_MAX_TOKENS_PER_FIELD 8
#define VH_MAX_TOKEN_TEXT 64
#define VH_MAX_TITLE_TEXT 128
#define VH_MAX_ARCHIVE_NAME 256
#define VH_MAX_VERSION_TEXT 32

typedef struct VhFieldToken {
    int id;
    char text[VH_MAX_TOKEN_TEXT];
} VhFieldToken;

typedef struct VhTokenList {
    VhFieldToken items[VH_MAX_TOKENS_PER_FIELD];
    int count;
} VhTokenList;

typedef struct VhParsedName {
    char archive_name[VH_MAX_ARCHIVE_NAME];
    char title[VH_MAX_TITLE_TEXT];
    char group_key[VH_MAX_TITLE_TEXT];
    char version[VH_MAX_VERSION_TEXT];

    VhTokenList chipset;
    VhTokenList language;
    VhTokenList memory;
    VhTokenList video;
    VhTokenList media;
    VhTokenList special;

    VhTokenList unknown;
} VhParsedName;

typedef struct VhParseContext {
    VhCsvFile language_csv;
    VhCsvFile chipset_csv;
    VhCsvFile video_csv;
    VhCsvFile media_csv;
    VhCsvFile memory_csv;
    VhCsvFile special_csv;
    int is_loaded;
} VhParseContext;

int vh_parse_context_load(VhParseContext *ctx, const char *defs_dir);
void vh_parse_context_free(VhParseContext *ctx);
int vh_parse_filename(const VhParseContext *ctx, const char *archive_name, VhParsedName *out);

#endif
