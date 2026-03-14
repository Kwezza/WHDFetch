#ifndef HTTP_DOWNLOAD_H
#define HTTP_DOWNLOAD_H

/*
 * HTTP Download function for Amiga systems
 *
 * This function downloads a file via HTTP 1.0 and saves it to disk,
 * providing a progress indicator on the console.
 *
 * Requirements:
 * - bsdsocket.library v4 or later
 * - timer.device
 *
 * Parameters:
 * - url: The URL to download (must begin with http://)
 * - outputPath: Path where the downloaded file will be saved
 * - silent: If TRUE, suppresses error messages (0 = show messages, 1 = silent)
 *
 * Returns:
 * - 0 on success
 * - negative values on failure (see amiga_download.h for error codes)
 */
int DownloadHTTPFile(const char *url, const char *outputPath, BOOL silent);

#endif /* HTTP_DOWNLOAD_H */