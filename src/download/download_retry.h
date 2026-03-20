#ifndef DOWNLOAD_RETRY_H
#define DOWNLOAD_RETRY_H

#include "platform/platform.h"

BOOL is_retryable_archive_download_error(LONG error_code);
const char *download_retry_reason_text(LONG error_code);
const char *download_failure_reason_text(LONG error_code);
LONG download_archive_with_retry(const char *download_url,
                                 const char *output_path,
                                 const char *archive_name,
                                 BOOL download_silent,
                                 BOOL crc_check,
                                 const char *archive_crc);

#endif /* DOWNLOAD_RETRY_H */
