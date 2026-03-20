#ifndef DAT_PHASE_H
#define DAT_PHASE_H

#include "main.h"

BOOL extract_Zip_file_and_rename(const char *zipPath,
                                 struct whdload_pack_def WHDLoadPackDefs[],
                                 int size_WHDLoadPackDef,
                                 int verboseMode);
BOOL process_and_archive_WHDLoadDat_Files(struct whdload_pack_def whdload_pack_defs[],
                                          int num_packs);
int extract_and_save_rom_names_from_XML(char *input_file_path,
                                        char *output_file_path,
                                        const char *pack_name);

#endif /* DAT_PHASE_H */
