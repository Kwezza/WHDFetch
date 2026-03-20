#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "main.h"

BOOL cli_has_pack_selection(int argc, char *argv[]);
BOOL cli_is_argument_present(int argc, char *argv[], const char *argument);
void cli_apply_arguments(int argc,
                         char *argv[],
                         struct whdload_pack_def whdload_pack_defs[],
                         struct download_option *download_options);

#endif /* CLI_PARSER_H */
