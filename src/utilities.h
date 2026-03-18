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
void wait_char(void);
void remove_CR_LF_from_string(char *str);
void trim(char *str);
int get_workbench_version(void);
//char *GetWorkbenchVersionString(void);
char *run_dos_command_and_get_first_line(const char *cmd);
ULONG clamp_timeout_seconds(ULONG requested);
BOOL parse_timeout_seconds(const char *value, ULONG *out_seconds, ULONG *raw_value);
archive_type detect_archive_type_from_filename(const char *filename);
#endif /* UTILITIES_H */