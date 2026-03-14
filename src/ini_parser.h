#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <exec/types.h>

#include "main.h"

BOOL ini_parser_load_overrides(whdload_pack_def pack_defs[], download_option *download_opts);
void ini_parser_cleanup_overrides(void);

#endif /* INI_PARSER_H */
