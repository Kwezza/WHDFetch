#ifndef HTML_PHASE_H
#define HTML_PHASE_H

#include "main.h"

void html_phase_extract_and_validate_links(struct whdload_pack_def whdload_pack_defs[],
                                           int size_whdload_pack_def,
                                           const char *dir_temp,
                                           const char *dir_zip_files);

#endif /* HTML_PHASE_H */
