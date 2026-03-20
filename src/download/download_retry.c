#include "platform/platform.h"

#include <stdio.h>

#include "download/amiga_download.h"
#include "download/download_retry.h"
#include "log/log.h"
#include "utilities.h"

BOOL is_retryable_archive_download_error(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
        case AD_SOCKET_ERROR:
        case AD_CONNECT_ERROR:
        case AD_CONNECTION_ERROR:
            return TRUE;
        default:
            return FALSE;
    }
}

const char *download_retry_reason_text(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
            return "timeout";
        case AD_SOCKET_ERROR:
            return "socket error";
        case AD_CONNECT_ERROR:
            return "connect error";
        case AD_CONNECTION_ERROR:
            return "connection closed";
        case AD_CRC_ERROR:
            return "CRC mismatch";
        default:
            return "download error";
    }
}

const char *download_failure_reason_text(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
            return "timeout";
        case AD_CANCELLED:
            return "cancelled";
        case AD_SOCKET_ERROR:
            return "socket error";
        case AD_CONNECT_ERROR:
            return "connect error";
        case AD_SEND_ERROR:
            return "send error";
        case AD_RECEIVE_ERROR:
            return "receive error";
        case AD_CONNECTION_ERROR:
            return "connection closed";
        case AD_BAD_REQUEST:
            return "HTTP 400 bad request";
        case AD_UNAUTHORIZED:
            return "HTTP 401 unauthorized";
        case AD_FORBIDDEN:
            return "HTTP 403 forbidden";
        case AD_NOT_FOUND:
            return "HTTP 404 not found";
        case AD_SERVER_ERROR:
            return "HTTP 500 server error";
        case AD_SERVICE_UNAVAILABLE:
            return "HTTP 503 service unavailable";
        case AD_REQUEST_FAILED:
            return "HTTP request failed";
        case AD_FILE_ERROR:
            return "file write error";
        case AD_CRC_ERROR:
            return "CRC mismatch";
        case AD_CRC_FORMAT_ERROR:
            return "invalid DAT CRC";
        case AD_MEMORY_ERROR:
            return "memory error";
        case AD_NOT_INITIALIZED:
            return "download library not initialized";
        case AD_INVALID_URL:
            return "invalid URL";
        case AD_HOST_NOT_FOUND:
            return "host not found";
        default:
            return "download error";
    }
}

LONG download_archive_with_retry(const char *download_url,
                                 const char *output_path,
                                 const char *archive_name,
                                 BOOL download_silent,
                                 BOOL crc_check,
                                 const char *archive_crc)
{
    LONG returnCode = AD_ERROR;
    ULONG max_retries = 0;
    ULONG total_attempts = 1;
    ULONG attempt_number;
    ULONG backoff_seconds = 1;

    if (!ad_get_config_value(ADTAG_MaxRetries, &max_retries))
    {
        max_retries = 2;
        log_warning(LOG_DOWNLOAD,
                    "download: failed to read ADTAG_MaxRetries, using fallback retries=%ld for '%s'\n",
                    (long)max_retries,
                    archive_name);
    }

    total_attempts = max_retries + 1;

    for (attempt_number = 1; attempt_number <= total_attempts; attempt_number++)
    {
        BOOL retry_due_to_error = FALSE;
        BOOL retry_due_to_crc = FALSE;

        create_download_marker(output_path);

        log_info(LOG_DOWNLOAD,
                 "download: attempt %ld/%ld for '%s'\n",
                 (long)attempt_number,
                 (long)total_attempts,
                 archive_name);

        returnCode = (LONG)ad_download_http_file(download_url, output_path, download_silent);

        if (returnCode == AD_CANCELLED)
        {
            log_info(LOG_DOWNLOAD, "download: user cancelled '%s' via Ctrl-C\n", archive_name);
            return 20;
        }

        if (returnCode == AD_SUCCESS &&
            crc_check == TRUE &&
            archive_crc != NULL && archive_crc[0] != '\0')
        {
            LONG crc_result = (LONG)ad_verify_file_crc(output_path, archive_crc);
            if (crc_result != AD_SUCCESS)
            {
                returnCode = crc_result;
                if (attempt_number < total_attempts)
                {
                    printf("CRC failed: %s (expected %s) - retrying.\n",
                           archive_name,
                           archive_crc);
                }
                else
                {
                    printf("CRC failed: %s (expected %s) - no retries left.\n",
                           archive_name,
                           archive_crc);
                }
                fflush(stdout);
                log_warning(LOG_DOWNLOAD,
                            "download: CRC verification failed for '%s' on attempt %ld/%ld (expected=%s, code=%ld)\n",
                            archive_name,
                            (long)attempt_number,
                            (long)total_attempts,
                            archive_crc,
                            (long)returnCode);
            }
            else
            {
                printf("CRC OK: %s\n", archive_name);
                fflush(stdout);
                log_info(LOG_DOWNLOAD,
                         "download: CRC verified for '%s' on attempt %ld/%ld\n",
                         archive_name,
                         (long)attempt_number,
                         (long)total_attempts);
            }
        }

        if (returnCode == AD_SUCCESS)
        {
            if (attempt_number > 1)
            {
                printf("Download completed after retry %ld/%ld: %s\n",
                       (long)(attempt_number - 1),
                       (long)max_retries,
                       archive_name);
                fflush(stdout);
            }

            delete_download_marker(output_path);
            break;
        }

        ad_print_download_error((int)returnCode);
        log_error(LOG_DOWNLOAD,
                  "download: attempt %ld/%ld failed for '%s' (code=%ld)\n",
                  (long)attempt_number,
                  (long)total_attempts,
                  archive_name,
                  (long)returnCode);

        retry_due_to_error = is_retryable_archive_download_error(returnCode);
        retry_due_to_crc = (returnCode == AD_CRC_ERROR);

        if ((retry_due_to_error || retry_due_to_crc) && attempt_number < total_attempts)
        {
            DeleteFile((STRPTR)output_path);
            delete_download_marker(output_path);

            printf("Retry %ld/%ld after %s in %ld second%s...\n",
                   (long)attempt_number,
                   (long)max_retries,
                   download_retry_reason_text(returnCode),
                   (long)backoff_seconds,
                   (backoff_seconds == 1) ? "" : "s");

            log_info(LOG_DOWNLOAD,
                     "download: retrying '%s' after %s (retry %ld/%ld, delay=%lds)\n",
                     archive_name,
                     download_retry_reason_text(returnCode),
                     (long)attempt_number,
                     (long)max_retries,
                     (long)backoff_seconds);

            Delay(backoff_seconds * 50);

            if (backoff_seconds < 8)
            {
                backoff_seconds <<= 1;
            }

            continue;
        }

        DeleteFile((STRPTR)output_path);
        delete_download_marker(output_path);
        return returnCode;
    }

    if (returnCode != AD_SUCCESS)
    {
        DeleteFile((STRPTR)output_path);
        delete_download_marker(output_path);
    }

    return returnCode;
}
