#ifndef UTILITIES_H
#define UTILITIES_H

#include <exec/types.h>

/* Function prototypes */
/* Timeout constants */
#define TIMEOUT_SECONDS_MIN 5
#define TIMEOUT_SECONDS_MAX 60

typedef enum archive_type {
	ARCHIVE_TYPE_UNKNOWN = 0,
	ARCHIVE_TYPE_LHA,
	ARCHIVE_TYPE_LZX
} archive_type;

int stricmp_custom(const char *s1, const char *s2);
int strncasecmp_custom(const char *s1, const char *s2, size_t n);
BOOL does_file_or_folder_exist(const char *filename, int appendWorkingDirectory);
BOOL is_file_locked(const char *filePath);
BOOL append_string_to_file(const char *target_filename, const char *append_text, BOOL create_new_file);
BOOL create_Directory_and_unlock(const char *dirName);
void wait_char(void);
void remove_CR_LF_from_string(char *str);
void trim(char *str);
int get_workbench_version(void);
//char *GetWorkbenchVersionString(void);
char *run_dos_command_and_get_first_line(const char *cmd);
ULONG clamp_timeout_seconds(ULONG requested);
BOOL parse_timeout_seconds(const char *value, ULONG *out_seconds, ULONG *raw_value);
archive_type detect_archive_type_from_filename(const char *filename);
int extract_date_from_filename(const char *filename, char *buffer, int bufSize);
long convert_string_date_to_int(const char *date);
int compare_dates_greater_then_date2(const char *date1, const char *date2);
void create_day_with_suffix(int day, char *buffer);
const char *get_month_name(int month);
void convert_date_to_long_style(const char *date, char *result);
int Get_latest_filename_from_directory(const char *directory, const char *searchText, char *latestFileName);
void create_directory_based_on_filename(const char *parentDir, const char *fileName);
void delete_all_files_in_dir(const char *directory);
void sanitize_amiga_file_path(char *path);
void get_folder_name_from_character(char *c);
void remove_all_occurrences(char *source_string, const char *toRemove);
void Format_text_split_by_Caps(const char *original, char *buffer, size_t buffer_len);
void turn_filename_into_text_with_spaces(const char *filename, char *outputArray);
void ensure_time_zone_set(void);
int get_bsdSocket_version(void);
char *concatStrings(const char *s1, const char *s2);
char *get_executable_version(const char *file_path);

/* Download marker helpers — detect/clean up incomplete downloads */
BOOL create_download_marker(const char *archive_path);
void delete_download_marker(const char *archive_path);
BOOL has_download_marker(const char *archive_path);
#endif /* UTILITIES_H */