#ifndef GAMEFILE_PARSER_H
#define GAMEFILE_PARSER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "utilities.h"

#define MAX_FILENAME_LEN 256
#define MAX_TITLE_LEN 100
#define MAX_VERSION_LEN 10
#define MAX_MACHINE_TYPE_LEN 10
#define MAX_VIDEO_FORMAT_LEN 10
#define MAX_LANGUAGE_LEN 200  // Increased to handle multiple languages
#define MAX_MEDIA_FORMAT_LEN 10
#define MAX_MEMORY_LEN 20
#define MAX_DISKS_LEN 10
#define MAX_SPS_LEN 10
#define MAX_SPECIAL_LEN 200  // Increased to handle multiple special tokens

typedef struct {
    char title[MAX_TITLE_LEN];
    char version[MAX_VERSION_LEN];
    char machineType[MAX_MACHINE_TYPE_LEN];
    char videoFormat[MAX_VIDEO_FORMAT_LEN];
    char language[MAX_LANGUAGE_LEN];
    char mediaFormat[MAX_MEDIA_FORMAT_LEN];
    char memory[MAX_MEMORY_LEN];
    int numberOfDisks;
    char sps[MAX_SPS_LEN];
    char special[MAX_SPECIAL_LEN];
} game_metadata;


void extract_game_info_from_filename(const char* filename, game_metadata* game);
void get_full_language_name(const char* abbreviation, char* fullName);
void append_to_special(char* special, const char* newSpecial);
void add_language_to_string(char* language, const char* newLanguage);
void extract_languages_from_token(const char* token, char* language);
int check_if_four_digits(const char* token);
int filterGame(game_metadata* game, const char* machineTypeFilter, const char* videoFormatFilter, const char* versionFilter, const char* mediaFormatFilter);


#endif // GAMEFILE_PARSER_H
