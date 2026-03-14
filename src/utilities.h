#ifndef UTILITIES_H
#define UTILITIES_H

#include <exec/types.h>

/* Function prototypes */
int stricmp_custom(const char *s1, const char *s2);
int strncasecmp_custom(const char *s1, const char *s2, size_t n);
BOOL does_file_or_folder_exist(const char *filename, int appendWorkingDirectory);
void wait_char(void);
void remove_CR_LF_from_string(char *str);
void trim(char *str);
int get_workbench_version(void);
//char *GetWorkbenchVersionString(void);
char *run_dos_command_and_get_first_line(const char *cmd);
#endif /* UTILITIES_H */