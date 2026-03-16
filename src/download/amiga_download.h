#ifndef AMIGA_DOWNLOAD_H
#define AMIGA_DOWNLOAD_H
/*------------------------------------------------------------------------*/
/*                                                                        *
 *  $Id: amiga_download.h,v 1.0 2023/10/29 10:00:00 user Exp $
 *                                                                        */
/*------------------------------------------------------------------------*/

/**
 * @file amiga_download.h
 *
 * @brief HTTP Download and File Verification Library for Amiga computers
 *
 * This library provides a simple API for downloading files over HTTP and
 * validating their integrity using CRC32 checksums on Amiga systems.
 * It is designed to work on AmigaOS 3.x with minimal setup by the
 * programmer, encapsulating all necessary network and timer management 
 * internally.
 *
 * Features:
 * - HTTP/1.0 downloads with automatic socket management
 * - Progress display with download speed calculation
 * - CRC32 file verification
 * - Self-contained operation (no external TCP/IP stack knowledge needed)
 * - Handles large files with incremental progress updates
 *
 * System requirements:
 * - AmigaOS 3.x (tested on 3.0, 3.1, 3.2)
 * - TCP/IP stack with bsdsocket.library v4+
 * - 1MB free memory recommended
 *
 * Usage example:
 * @code
 * #include <stdio.h>
 * #include "amiga_download.h"
 *
 * int main(void)
 * {
 *     if (!ad_init_download_lib()) {
 *         printf("Failed to initialize download library\n");
 *         return 20;
 *     }
 *
 *     // Download a file
 *     if (ad_download_http_file("http://example.com/file.lha", "file.lha", FALSE) == AD_SUCCESS) {
 *         // Verify downloaded file against known CRC
 *         if (ad_verify_file_crc("file.lha", "a1b2c3d4") == AD_SUCCESS) {
 *             printf("File downloaded and verified successfully.\n");
 *         }
 *     }
 *
 *     // Always clean up when done
 *     ad_cleanup_download_lib();
 *     return 0;
 * }
 * @endcode
 *
 * @note This library handles all necessary TCP/IP stack interactions and
 *       does not require the end programmer to manage socket library bases.
 *       All memory allocation is performed using Amiga system functions.
 */

#include <exec/types.h>


/*------------------------------------------------------------------------*/
/*                          Error Codes                                   */
/*------------------------------------------------------------------------*/

/* Success codes */
#define AD_SUCCESS              0  /* Operation completed successfully */

/* General error codes */
#define AD_ERROR               -1  /* Generic error */
#define AD_NOT_INITIALIZED     -2  /* Library not initialized */
#define AD_MEMORY_ERROR        -3  /* Memory allocation failed */
#define AD_TIMEOUT             -4  /* Operation timed out */
#define AD_CANCELLED           -5  /* Operation cancelled by user (Ctrl-C) */

/* URL and connection error codes */
#define AD_INVALID_URL         -10 /* Invalid URL format */
#define AD_HOST_NOT_FOUND      -11 /* Host lookup failed */
#define AD_SOCKET_ERROR        -12 /* Socket creation or operation failed */
#define AD_CONNECT_ERROR       -13 /* Could not connect to server */
#define AD_CONNECT_FAILED      -13 /* Could not connect to server (alias) */
#define AD_SEND_ERROR          -14 /* Failed to send HTTP request */
#define AD_RECEIVE_ERROR       -15 /* Failed to receive data from server */
#define AD_CONNECTION_ERROR    -16 /* Connection closed by server */
#define AD_REQUEST_TOO_LONG    -17 /* HTTP request exceeds buffer size */

/* HTTP protocol error codes */
#define AD_HEADER_ERROR        -20 /* Error parsing HTTP headers */
#define AD_BAD_REQUEST         -21 /* HTTP 400 Bad Request */
#define AD_UNAUTHORIZED        -22 /* HTTP 401 Unauthorized */
#define AD_FORBIDDEN           -23 /* HTTP 403 Forbidden */
#define AD_NOT_FOUND           -24 /* HTTP 404 Not Found */
#define AD_SERVER_ERROR        -25 /* HTTP 500 Internal Server Error */
#define AD_SERVICE_UNAVAILABLE -26 /* HTTP 503 Service Unavailable */
#define AD_REQUEST_FAILED      -29 /* HTTP request generation/transmission failed */

/* File error codes */
#define AD_FILE_ERROR          -30 /* File I/O error */
#define AD_CRC_ERROR           -31 /* CRC verification failed */
#define AD_CRC_FORMAT_ERROR    -32 /* Invalid CRC format */

/*------------------------------------------------------------------------*/
/*                          Tag Definitions                               */
/*------------------------------------------------------------------------*/

/* Library initialization tags */
#define ADTAG_BASE              (TAG_USER + 10000)  /* Base value for our tags */
#define ADTAG_Verbose           (ADTAG_BASE + 1)    /* BOOL: Enable verbose mode */
#define ADTAG_Timeout           (ADTAG_BASE + 2)    /* ULONG: Connection timeout in seconds */
#define ADTAG_BufferSize        (ADTAG_BASE + 3)    /* ULONG: Size of download buffer in bytes */
#define ADTAG_UserAgent         (ADTAG_BASE + 4)    /* STRPTR: Custom User-Agent string */
#define ADTAG_MaxRetries        (ADTAG_BASE + 6)    /* ULONG: Maximum connection retry attempts */

/*------------------------------------------------------------------------*/
/*                  Library Initialization Functions                       */
/*------------------------------------------------------------------------*/

/**
 * @brief Initializes the download library
 *
 * This function must be called before using any other functions in the library.
 * It initializes all required resources including the socket library and timer device.
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib(void);

/**
 * @brief Initializes the download library with tags
 *
 * This function allows initialization with specific options provided as tags.
 *
 * @param tags Pointer to a TagItem array specifying initialization options
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib_tags(struct TagItem *tags);

/**
 * @brief Initializes the download library with tags (varargs version)
 *
 * This function allows initialization with specific options provided as tags.
 *
 * @param tag1 First tag in the varargs list
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib_taglist(Tag tag1, ...);

/**
 * @brief Cleans up the download library
 *
 * This function should be called when you're done using the library
 * to properly clean up all resources.
 */
void ad_cleanup_download_lib(void);

/**
 * @brief Checks if the download library is initialized
 * 
 * @return TRUE if the library is initialized, FALSE otherwise
 */
BOOL ad_is_library_initialized(void);

/**
 * @brief Downloads a file via HTTP 1.0 protocol and saves it to disk
 *
 * @param url The HTTP URL to download (must start with "http://")
 * @param output_path Path where the downloaded file should be saved
 * @param silent If TRUE, suppresses all error messages
 * @return AD_SUCCESS on success, or one of the error codes defined above
 */
int ad_download_http_file(const char *url, const char *output_path, BOOL silent);

/**
 * @brief Downloads a file via HTTP with automatic retry for transient network errors
 *
 * Retries are controlled by ADTAG_MaxRetries from library initialization.
 * A value of 2 means: 1 initial attempt + 2 retries (3 total attempts).
 *
 * @param url The HTTP URL to download (must start with "http://")
 * @param output_path Path where the downloaded file should be saved
 * @param silent If TRUE, suppresses all non-critical progress/error messages
 * @return AD_SUCCESS on success, or one of the error codes defined above
 */
int ad_download_http_file_with_retries(const char *url, const char *output_path, BOOL silent);

/**
 * @brief Verifies if a file matches the expected CRC-32 checksum
 *
 * @param filename Path to the file to verify
 * @param expected_crc_str The expected CRC-32 value as a hex string (e.g. "A1B2C3D4")
 * @return AD_SUCCESS if CRC matches, AD_CRC_ERROR if mismatch, other error codes for failures
 */
int ad_verify_file_crc(const char *filename, const char *expected_crc_str);

/**
 * @brief Displays a descriptive error message based on the download error code
 *
 * This function prints a user-friendly message corresponding to the error code
 * returned by ad_download_http_file or other library functions.
 *
 * @param error_code The error code returned by ad_download_http_file or other functions
 */
void ad_print_download_error(int error_code);

/**
 * @brief Returns the library version as a string
 * 
 * @return Pointer to version string in standard Amiga format
 */
const char *ad_get_version_string(void);

/**
 * @brief Returns the library version number
 * 
 * @param revision Pointer to store revision number (can be NULL)
 * @return Version number
 */
UWORD ad_get_version(UWORD *revision);

/**
 * @brief Gets a configuration value from the library settings
 *
 * This function lets users retrieve configuration values after initialization.
 *
 * @param tag The configuration value to get (ADTAG_xxx)
 * @param storage Pointer to store the result value
 * @return TRUE if the tag was recognized and storage was updated, FALSE otherwise
 */
BOOL ad_get_config_value(ULONG tag, APTR storage);

#endif /* AMIGA_DOWNLOAD_H */
/*------------------------------------------------------------------------*/
/* End of Text */