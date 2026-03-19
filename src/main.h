#ifndef MAIN_H
#define MAIN_H

#include <ctype.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Constants */
#define BUFFER_SIZE 1024
#define MAX_LINK_LENGTH 256
#define CLI_LINES_TO_PAUSE_AT 17
#define MAX_LINK_CLEANUP_REMOVALS 8

extern const char *DOWNLOAD_WEBSITE;
extern const char *FILE_PART_TO_REMOVE;
extern const char *HTML_LINK_PREFIX_FILTER;
extern const char *HTML_LINK_CONTENT_FILTER;
extern const char *LINK_CLEANUP_REMOVALS[MAX_LINK_CLEANUP_REMOVALS];
extern int LINK_CLEANUP_REMOVAL_COUNT;

/* URLs */
extern const char *WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_DOWNLOAD_DEMOS;
extern const char *WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_DOWNLOAD_GAMES;
extern const char *WHDLOAD_DOWNLOAD_MAGAZINES;

/* Text Names */
extern const char *WHDLOAD_TEXT_NAME_DEMOS_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_TEXT_NAME_DEMOS;
extern const char *WHDLOAD_TEXT_NAME_GAMES_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_TEXT_NAME_GAMES;
extern const char *WHDLOAD_TEXT_NAME_MAGAZINES;

/* Directory Names */
extern const char *WHDLOAD_DIR_DEMOS_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_DIR_DEMOS;
extern const char *WHDLOAD_DIR_GAMES_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_DIR_GAMES;
extern const char *WHDLOAD_DIR_MAGAZINES;

/* File Filters */
extern const char *WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_FILE_FILTER_DAT_DEMOS;
extern const char *WHDLOAD_FILE_FILTER_DAT_GAMES_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_FILE_FILTER_DAT_GAMES;
extern const char *WHDLOAD_FILE_FILTER_DAT_MAGAZINES;

extern const char *WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_FILE_FILTER_ZIP_DEMOS;
extern const char *WHDLOAD_FILE_FILTER_ZIP_GAMES_BETA_AND_UNRELEASED;
extern const char *WHDLOAD_FILE_FILTER_ZIP_GAMES;
extern const char *WHDLOAD_FILE_FILTER_ZIP_MAGAZINES;

extern const char *GITHUB_ADDRESS;

/* Constants for WHDLoad Pack Definitions */
extern const int DEMOS_BETA;
extern const int DEMOS;
extern const int GAMES_BETA;
extern const int GAMES;
extern const int MAGAZINES;

/* Global Variables */
extern int files_downloaded;
extern int files_skipped;
extern int no_skip_messages;
extern int quiet_output;
extern int skip_AGA;
extern int skip_CD;
extern int skip_NTSC;
extern int skip_NonEnglish;

extern long start_time;

extern const char VERSION_STRING[];
extern const char PROGRAM_NAME[];
extern const char version[];

/* Text Formatting */
#define textBlack "\x1B[31m"
#define textBlue "\x1B[33m"
#define textBold "\x1B[1m"
#define textGrey "\x1B[30m"
#define textItalic "\x1B[3m"
#define textReset "\x1B[0m"
#define textReverse "\x1B[7m"
#define textUnderline "\x1B[4m"
#define textWhite "\x1B[32m"

/* Struct Definitions */
typedef struct whdload_pack_def {
    BOOL updated_dat_downloaded;
    const char *download_url;
    const char *extracted_pack_dir;
    const char *fileNameStart;
    const char *filter_dat_files;
    const char *filter_zip_files;
    const char *full_text_name_of_pack;
    int count_existing_files_skipped;
    int count_new_files_downloaded;
    int downloadUpdates;
    int user_requested_download;
    int file_count;
    int filtered_file_count;
} whdload_pack_def;

typedef struct download_option {
    int download_all;
    int download_demos;
    int download_demos_beta;
    int download_games;
    int download_games_beta;
    int download_magazines;
    int no_skip_messages;
    int quiet_output;
    BOOL extract_archives;
    BOOL skip_existing_extractions;
    BOOL force_extract;
    BOOL skip_download_if_extracted;
    BOOL force_download;
    const char *extract_path;
    BOOL delete_archives_after_extract;
    BOOL purge_archives;
    int extract_existing_only;
    BOOL use_custom_icons;
    BOOL unsnapshot_icons;
    BOOL disable_counters;
    BOOL crc_check;
    ULONG timeout_seconds;
} download_option;

typedef struct download_progress_state {
    LONG total_queued_entries;
    LONG current_download_index;
    ULONG total_queued_kb;
    ULONG remaining_queued_kb;
} download_progress_state;


/* Function Prototypes */
BOOL append_string_to_file(const char *target_filename, const char *append_text, BOOL create_new_file);
BOOL create_Directory_and_unlock(const char *dirName);

BOOL extract_Zip_file_and_rename(const char *zipPath, whdload_pack_def[], int size_WHDLoadPackDef, int quietMode);

BOOL is_file_locked(const char *filePath);

BOOL process_and_archive_WHDLoadDat_Files(whdload_pack_def WHDLoadPackDefs[], int size_WHDLoadPackDef);
BOOL startup_text_and_needed_progs_are_installed(int number_of_args);
char *concatStrings(const char *s1, const char *s2);
char *get_first_matching_fileName(const char *directory);
const char *get_month_name(int month);
int compare_and_decide_DatFileDownload(char *filename,
                                       const char *searchText,
                                       int *should_download_zip,
                                       BOOL force_download);
int compare_dates_greater_then_date2(const char *date1, const char *date2);
int download_WHDLoadPacks_From_Links(const char *bufferm,
                                     whdload_pack_def WHDLoadPackDefs[],
                                     int size_WHDLoadPackDef,
                                     const download_option *download_options);
int extract_date_from_filename(const char *filename, char *buffer, int bufSize);
int Get_latest_filename_from_directory(const char *directory, const char *text, char *latestFileName);
int get_bsdSocket_version(void);
long convert_string_date_to_int(const char *date);
LONG download_roms_from_file(const char *filename,
                             whdload_pack_def *WHDLoadPackDefs,
                             const download_option *download_options,
                             int replaceFiles,
                             download_progress_state *progress_state);
LONG download_roms_if_file_exists(whdload_pack_def *WHDLoadPackDefs,
                                  const download_option *download_options,
                                  int replaceFiles,
                                  download_progress_state *progress_state);
LONG execute_archive_download_command(const char *downloadWHDFile,
                                      ULONG archive_size_bytes,
                                      const char *archive_crc,
                                      whdload_pack_def *WHDLoadPackDefs,
                                      const download_option *download_options,
                                      int replaceFiles,
                                      download_progress_state *progress_state);
LONG extract_existing_archives_from_file(const char *filename,
                                         whdload_pack_def *WHDLoadPackDefs,
                                         const download_option *download_options);
LONG extract_existing_archives_if_file_exists(whdload_pack_def *WHDLoadPackDefs,
                                              const download_option *download_options);
void convert_date_to_long_style(const char *date, char *result);
void create_day_with_suffix(int day, char *buffer);
void create_directory_based_on_filename(const char *parentDir, const char *fileName);
void delete_all_files_in_dir(const char *directory);
void ensure_time_zone_set(void);
int extract_and_save_rom_names_from_XML(char *inputFilePath, char *outputFilePath, const char *packName);
void extract_and_validate_HTML_links(whdload_pack_def WHDLoadPackDefs[],
                                     int size_WHDLoadPackDef,
                                     const download_option *download_options);
void Format_text_split_by_Caps(const char *original, char *buffer, size_t buffer_len);
void get_folder_name_from_character(char *c);
void remove_all_occurrences(char *src, const char *toRemove);
void sanitize_amiga_file_path(char *path);
void setup_app_defaults(whdload_pack_def WHDLoadPackDefs[], download_option *downloadOptions);

void turn_filename_into_text_with_spaces(const char *filename, char *storage);
char *get_executable_version(const char *filePath);



#endif /* MAIN_H */