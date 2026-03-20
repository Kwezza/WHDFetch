#ifndef STARTUP_H
#define STARTUP_H

#include "main.h"

BOOL startup_text_and_needed_progs_are_installed(int number_of_args);
BOOL validate_extraction_startup_configuration(const download_option *download_options);
void log_effective_configuration(const whdload_pack_def *pack_defs,
                                 const download_option *download_options,
                                 const char *stage);

#endif /* STARTUP_H */
