/*------------------------------------------------------------------------*/
/*                                                                        *
 *  $Id: http_download.c,v 1.0 2023/10/16 12:00:00 user Exp $
 *                                                                        */
/*------------------------------------------------------------------------*/

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <devices/timer.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "amiga_download.h"
#include "display_message.h"
#include "timer_shared.h"
#include "../platform/platform.h"

/* Constants */
#define BUFFER_SIZE 1024
#define MAX_HOST_LEN 256
#define MAX_PATH_LEN 512
#define MAX_REQUEST_LEN 768
#define MAX_HEADER_LINE 256


static BOOL g_dns_cache_valid = FALSE;
static char g_dns_cache_host[MAX_HOST_LEN] = {0};
static ULONG g_dns_cache_addr = 0;



/* Global library bases managed in download_lib.c and timer_shared.c */
extern struct Library *SocketBase;
extern struct Device *TimerBase;

/*
 * strnicmp is a SAS/C extension not present in VBCC's libnix.
 * Provide a portable C89 implementation for all non-SAS/C compilers.
 */
#ifndef __SASC
static int ad_strnicmp(const char *s1, const char *s2, size_t n)
{
	unsigned char c1;  /* Left-hand comparison character */
	unsigned char c2;  /* Right-hand comparison character */
	while (n-- > 0)
	{
		c1 = (unsigned char)tolower((unsigned char)*s1++);
		c2 = (unsigned char)tolower((unsigned char)*s2++);
		if (c1 != c2) { return (int)c1 - (int)c2; }
		if (c1 == '\0') { return 0; }
	}
    return 0;
} /* ad_strnicmp */
#endif /* !__SASC */

/* Add this near other external declarations */
extern BOOL ad_get_lib_initialized_status(void);

/* Forward declarations for helper functions (updated) */
static int parse_url(const char *url, char *host, char *path, BOOL silent);
static int connect_to_server(const char *host, LONG *sock, BOOL silent);
static int send_http_request(LONG sock, const char *host, const char *path, BOOL silent);
static int process_response(LONG sock, const char *output_path, BPTR *file, BOOL silent);
static void update_progress(ULONG bytes_downloaded, LONG content_length);
static void process_header_line(char *header_line, int *content_length, BOOL silent);
static int open_output_file(BPTR *file, const char *output_path, BOOL silent);
static BOOL ad_ctrl_c_pressed(void);

static BOOL ad_ctrl_c_pressed(void)
{
    ULONG signal_mask = SetSignal(0L, 0L);
    if (signal_mask & SIGBREAKF_CTRL_C)
    {
        SetSignal(0L, SIGBREAKF_CTRL_C);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Downloads a file via HTTP 1.0 protocol and saves it to disk
 *
 * @param url The HTTP URL to download (must start with "http://")
 * @param output_path Path where the downloaded file should be saved
 * @param silent If TRUE, suppresses all error messages
 * @return AD_SUCCESS on success, or one of the error codes defined above
 */
int ad_download_http_file(const char *url, const char *output_path, BOOL silent)
{

    
    char host[MAX_HOST_LEN];  /* Buffer to store hostname */
    char path[MAX_PATH_LEN];  /* Buffer to store path */
    LONG sock = -1;           /* Socket descriptor */
    BPTR file = 0;            /* Output file handle */
    int result = AD_ERROR;    /* Return value */

    /* Check if library is initialized */
    if (!ad_get_lib_initialized_status())
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Download library not initialized");
        }
        return AD_NOT_INITIALIZED;
    }

    /* Make sure the library is initialized */
    if (!SocketBase || !TimerBase)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Download library not initialized. Call InitDownloadLib() first.");
        }
        return AD_NOT_INITIALIZED;
    }

    /* Parse URL */
    if (parse_url(url, host, path, silent) != 0)
    {
        return AD_INVALID_URL;
    }

    /* Connect to server */
    if (connect_to_server(host, &sock, silent) != 0)
    {
        if (sock >= 0)
        {
            CloseSocket(sock);
        }
        return AD_HOST_NOT_FOUND;
    }

    /* Send HTTP request */
    if (send_http_request(sock, host, path, silent) != 0)
    {
        CloseSocket(sock);
        return AD_REQUEST_FAILED;
    }

    /* Process response and download file */
    result = process_response(sock, output_path, &file, silent);

    /* Clean up resources */
    if (sock >= 0)
    {
        CloseSocket(sock);
    }
    
    if (file)
    {
        Close(file);
    }
    
    return result;
} /* ad_download_http_file */

/*------------------------------------------------------------------------*/

/**
 * @brief Parses an HTTP URL into host and path components
 *
 * @param url Input URL starting with "http://"
 * @param host Buffer to store the extracted hostname
 * @param path Buffer to store the extracted path
 * @param silent If TRUE, suppresses all error messages
 * @return 0 on success, -1 on failure
 */
static int parse_url(const char *url, char *host, char *path, BOOL silent)
{
    const char *prefix = "http://";  /* HTTP URL prefix */
    const char *p;                   /* Pointer for parsing URL */
    int pos = 0;                     /* Position in host buffer */

    /* Validate URL format */
    if (strncmp(url, prefix, strlen(prefix)) != 0)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "URL must begin with http://");
        }
        return AD_INVALID_URL;
    }

    /* Start after the prefix */
    p = url + strlen(prefix);
    
    /* Extract host */
    while (*p && *p != '/' && pos < MAX_HOST_LEN - 1)
    {
        host[pos++] = *p++;
    }
    host[pos] = '\0';
    
    /* Extract path */
    if (*p == '/')
    {
        strncpy(path, p, MAX_PATH_LEN - 1);
    }
    else
    {
        strcpy(path, "/");
    }
    
    path[MAX_PATH_LEN - 1] = '\0'; /* Ensure null termination */
    
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Host: '%s', Path: '%s'", host, path);
    }
    #endif
    
    return 0;
} /* parse_url */

/*------------------------------------------------------------------------*/

/**
 * @brief Connect to the HTTP server with timeout
 *
 * @param host Hostname to connect to
 * @param sock Pointer to store the socket descriptor
 * @param silent If TRUE, suppresses all error messages
 * @return 0 on success, or one of the error codes defined in amiga_download.h
 */
static int connect_to_server(const char *host, LONG *sock, BOOL silent)
{
    struct hostent *he;                /* Host entry from DNS lookup */
    struct sockaddr_in server;         /* Server address structure */
    long non_blocking = 1;             /* For non-blocking socket mode */
    fd_set write_fds;                  /* For select() */
    struct timeval tv;                 /* Timeout structure */
    int res;                           /* Result of operations */
    int sel_res;                       /* Result from WaitSelect */
    ULONG timeout_secs = 30;           /* Default timeout value */

    /* Make sure we have valid input */
    if (!host || !sock)
    {
        return AD_ERROR;
    }

    /* Get timeout configuration value - AFTER checking we have valid pointers */
    ad_get_config_value(ADTAG_Timeout, &timeout_secs);
    
    /* Make sure we have a socket base */
    if (!SocketBase)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Socket library not initialized");
        }
        return AD_NOT_INITIALIZED;
    }
    
    if (g_dns_cache_valid && strcmp(host, g_dns_cache_host) == 0)
    {
        #ifdef DEBUG
        if (!silent)
        {
            display_message(MSG_DEBUG,__FILE__,__LINE__, "Using cached host lookup: %s (timeout: %lu)", host, timeout_secs);
        }
        #endif
    }
    else
    {
        #ifdef DEBUG
        if (!silent)
        {
            display_message(MSG_DEBUG,__FILE__,__LINE__, "Looking up host: %s (timeout: %lu)", host, timeout_secs);
        }
        #endif

        /* Lookup hostname */
        he = gethostbyname((STRPTR)host);
        if (!he)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Host lookup failed for '%s' (h_errno: %d)", host, Errno());
            }
            return AD_HOST_NOT_FOUND;
        }

        strncpy(g_dns_cache_host, host, MAX_HOST_LEN - 1);
        g_dns_cache_host[MAX_HOST_LEN - 1] = '\0';
        g_dns_cache_addr = *((ULONG *)he->h_addr);
        g_dns_cache_valid = TRUE;
    }
    
    /* Create socket */
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Socket creation failed (errno: %d)", errno);
        }
        return AD_SOCKET_ERROR;
    }
    
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Socket created, fd: %ld", *sock);
    }
    #endif
    
    /* Set up server address */
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    server.sin_addr.s_addr = g_dns_cache_addr;
    
    /* Set socket to non-blocking mode for timeout control */
    if (IoctlSocket(*sock, FIONBIO, &non_blocking) < 0)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Failed to set non-blocking mode");
        }
        CloseSocket(*sock);
        return AD_SOCKET_ERROR;
    }
    
    /* Connect to server (will return immediately in non-blocking mode) */
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Attempting to connect (non-blocking)...");
    }
    #endif
    
    res = connect(*sock, (struct sockaddr *)&server, sizeof(server));
    //if (res < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK)
    //{
    // /   /* Immediate error other than "in progress" */
    //    if (!silent)
    //    {
    //        display_message(MSG_ERROR,__FILE__,__LINE__, "Connection failed (errno: %d)", errno);
    //    }
    //    CloseSocket(*sock);
    //    return AD_CONNECT_FAILED;
    //}
    
    /* Wait for connection with timeout */
    FD_ZERO(&write_fds);
    FD_SET(*sock, &write_fds);
    
    tv.tv_secs = timeout_secs;
    tv.tv_micro = 0;
    
    /* Wait for socket to become writable (connected) or timeout */
    sel_res = WaitSelect(*sock + 1, NULL, &write_fds, NULL, &tv, NULL);
    
    if (sel_res <= 0)
    {
        /* Timeout or error */
        if (sel_res == 0)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Connection timed out after %lu seconds", timeout_secs);
            }
            CloseSocket(*sock);
            return AD_TIMEOUT;
        }
        else
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Select failed (errno: %d)", errno);
            }
            CloseSocket(*sock);
            return AD_CONNECT_ERROR;
        }
    }
    
    /* Check if connection succeeded */
    {
        int error = 0;                 /* Error code from socket */
        int len = sizeof(error);       /* Size of error variable */
        
        /* Cast len to the appropriate type expected by getsockopt */
        if (getsockopt(*sock, SOL_SOCKET, SO_ERROR, &error, (void*)&len) < 0 || error)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Connection failed (error: %d)", error ? error : errno);
            }
            CloseSocket(*sock);
            return AD_CONNECT_ERROR;
        }
    }
    
    /* Set socket back to blocking mode */
    non_blocking = 0;
    IoctlSocket(*sock, FIONBIO, &non_blocking);
    
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Connected successfully");
    }
    #endif
    
    return 0;
} /* connect_to_server */

/*------------------------------------------------------------------------*/

/**
 * @brief Send HTTP request to server
 *
 * @param sock Socket descriptor
 * @param host Hostname for Host: header
 * @param path Path to request
 * @param silent If TRUE, suppresses all error messages
 * @return 0 on success, -1 on failure
 */
static int send_http_request(LONG sock, const char *host, const char *path, BOOL silent)
{
    STRPTR request;                /* HTTP request buffer */
    STRPTR user_agent;             /* User-Agent string */
    
    /* Get user agent from library configuration */
    if (!ad_get_config_value(ADTAG_UserAgent, &user_agent))
    {
        /* Fall back to default if retrieval fails */
        user_agent = "AmigaHTTP/1.0";
    }
    
    /* Allocate request buffer */
    request = (STRPTR)amiga_malloc(MAX_REQUEST_LEN);
    if (!request)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Memory allocation failed");
        }
        return AD_MEMORY_ERROR;
    }
    
    /* Ensure request will fit in buffer */
    if (strlen(path) + strlen(host) + strlen(user_agent) + 64 >= MAX_REQUEST_LEN)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Request too long");
        }
        amiga_free(request);
        return AD_REQUEST_TOO_LONG;
    }
    
    /* Format HTTP request */
    sprintf((char *)request,
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "User-Agent: %s\r\n"
        "Connection: close\r\n\r\n",
        path, host, user_agent);
    
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Sending request:\n%s", request);
    }
    #endif
    
    /* Send the request */
    if (send(sock, request, strlen((char *)request), 0) < 0)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Send failed (errno: %d)", errno);
        }
        amiga_free(request);
        return AD_SEND_ERROR;
    }
    
    #ifdef DEBUG
    if (!silent)
    {
        display_message(MSG_DEBUG,__FILE__,__LINE__, "Request sent");
    }
    #endif
    
    /* Free request buffer */
    amiga_free(request);
    
    /* Display waiting message */
    if (!silent)
    {
        display_message(MSG_INFO,__FILE__,__LINE__, "Waiting for server response...");
        display_message(MSG_CLEAR,__FILE__,__LINE__, CLI_MOVE_UP_ONE_LINE);
    }
    
    return 0;
} /* send_http_request */

/*------------------------------------------------------------------------*/

/**
 * @brief Process a single header line
 * 
 * @param header_line Line to process
 * @param content_length Pointer to content_length variable to update
 * @param silent If TRUE, suppresses all debug messages
 */
static void process_header_line(char *header_line, int *content_length, BOOL silent)
{
    /* Check for Content-Length */
    if (ad_strnicmp(header_line, "Content-Length:", 15) == 0)
    {
        *content_length = atoi(header_line + 15);
        #ifdef DEBUG
        if (!silent)
        {
            display_message(MSG_DEBUG,__FILE__,__LINE__, "Found Content-Length: %d", *content_length);
        }
        #endif
    }
} /* process_header_line */

/**
 * @brief Attempt to open the output file
 * 
 * @param file Pointer to store file handle
 * @param output_path Path to save downloaded file
 * @param silent If TRUE, suppresses all error messages
 * @return AD_SUCCESS or AD_FILE_ERROR
 */
static int open_output_file(BPTR *file, const char *output_path, BOOL silent)
{
    *file = Open((STRPTR)output_path, MODE_NEWFILE);
    if (!*file)
    {
        if (!silent)
        {
            display_message(MSG_ERROR,__FILE__,__LINE__, "Failed to create output file '%s'", output_path);
        }
        return AD_FILE_ERROR;
    }
    return AD_SUCCESS;
} /* open_output_file */

/**
 * @brief Process HTTP response, handle headers and save body to file
 *
 * @param sock Socket descriptor
 * @param output_path Path to save downloaded file
 * @param file Pointer to store file handle
 * @param silent If TRUE, suppresses all error messages
 * @return AD_SUCCESS on success, or one of the error codes defined in amiga_download.h
 */
static int process_response(LONG sock, const char *output_path, BPTR *file, BOOL silent)
{
    char buffer[BUFFER_SIZE];                 /* Receive buffer */
    char header_line[MAX_HEADER_LINE];        /* Current header line */
    char search_buffer[4] = {0};              /* Buffer for end of headers search */
    int header_line_pos = 0;                  /* Position in header line */
    int search_pos = 0;                       /* Position in search buffer */
    int header_done = 0;                      /* Flag: headers completed */
    int status_code = 0;                      /* HTTP status code */
    int content_length = -1;                  /* Content length if known */
    LONG bytes;                               /* Bytes received */
    int i;                                    /* Loop counter */
    ULONG total_bytes_downloaded = 0;         /* Total bytes downloaded */
    ULONG last_update_time = 0;               /* Last progress update time */
    struct timeval current_time;              /* Current time */
    fd_set readfds;                           /* Read file descriptor set */
    struct timeval tv;                        /* Select timeout */
    ULONG old_seconds = 0;                    /* Last seconds value for waiting dots */
    long non_blocking = 1;                    /* Socket non-blocking flag */
    int result;                               /* Result from helper functions */
    ULONG timeout_secs = 30;                  /* Inactivity timeout in seconds */
    ULONG wait_start_secs = 0;                /* Start time for initial response wait */
    ULONG idle_start_secs = 0;                /* Start time for body/header inactivity */
    ULONG last_wait_report_secs = 0;          /* Last second shown in wait feedback */
    int wait_res;                             /* Result from WaitSelect */
    
    /* Get configured timeout (minimum 1 second) */
    ad_get_config_value(ADTAG_Timeout, &timeout_secs);
    if (timeout_secs == 0)
    {
        timeout_secs = 1;
    }

    /* Get starting time */
    GetSysTime(&current_time);
    old_seconds = current_time.tv_secs;
    wait_start_secs = current_time.tv_secs;
    idle_start_secs = current_time.tv_secs;
    last_wait_report_secs = current_time.tv_secs;
    
    /* Set non-blocking mode for the socket */
    IoctlSocket(sock, FIONBIO, &non_blocking);
    
    /* Wait for first response bytes with timeout and visual indicator */
    while (1)
    {
        /* Check for data */
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        /* 1-second polling interval to update UI and enforce timeout */
        tv.tv_secs = 1;
        tv.tv_micro = 0;
        
        wait_res = WaitSelect(sock + 1, &readfds, NULL, NULL, &tv, NULL);
        GetSysTime(&current_time);

        if (ad_ctrl_c_pressed())
        {
            display_message(MSG_INFO,__FILE__,__LINE__, "Cancelled by user (Ctrl-C)");
            return AD_CANCELLED;
        }

        if (wait_res > 0)
        {
            /* Data available */
            bytes = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes > 0)
            {
                /* Data received, break out to process it */
                if (!silent)
                {
                    display_message(MSG_CLEAR,__FILE__,__LINE__, "");
                }
                break;
            }
            else if (bytes == 0)
            {
                /* Connection closed */
                if (!silent)
                {
                    display_message(MSG_ERROR,__FILE__,__LINE__, "Connection closed by server");
                }
                return AD_CONNECTION_ERROR;
            }
            else if (bytes < 0 && errno != EWOULDBLOCK)
            {
                /* Error */
                if (!silent)
                {
                    display_message(MSG_ERROR,__FILE__,__LINE__, "Receive failed (errno: %d)", errno);
                }
                return AD_SOCKET_ERROR;
            }
        }
        else if (wait_res == 0)
        {
            ULONG elapsed_secs = current_time.tv_secs - wait_start_secs;

            if (current_time.tv_secs > last_wait_report_secs)
            {
                last_wait_report_secs = current_time.tv_secs;
                display_message(MSG_PROGRESS,__FILE__,__LINE__,
                               "Waiting for server response %lu/%lu sec   ",
                               elapsed_secs, timeout_secs);
            }

            if (elapsed_secs >= timeout_secs)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__,
                               "Timed out waiting for server response after %lu seconds",
                               timeout_secs);
                return AD_TIMEOUT;
            }
        }
        else
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "WaitSelect failed (errno: %d)", errno);
            }
            return AD_SOCKET_ERROR;
        }
        
        old_seconds = current_time.tv_secs;
    }
    
    /* Process the first received chunk */
    
    /* Try to parse HTTP status code from first line */
    if (strncmp(buffer, "HTTP/", 5) == 0)
    {
        char *space_ptr = strchr(buffer + 5, ' ');
        if (space_ptr)
        {
            status_code = atoi(space_ptr + 1);
            #ifdef DEBUG
            if (!silent)
            {
                display_message(MSG_DEBUG,__FILE__,__LINE__, "HTTP Status Code: %d", status_code);
            }
            #endif
            
            /* Client or server error status (4xx or 5xx) */
            if (status_code >= 400)
            {
                /* Display error message if not in silent mode */
                if (!silent)
                {
                    switch (status_code)
                    {
                        case 400:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "Bad request (400)");
                            break;
                        case 401:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "Authentication required (401)");
                            break; 
                        case 403:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "Access forbidden (403)");
                            break;
                        case 404:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "File not found (404)");
                            break;
                        case 500:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "Server internal error (500)");
                            break;
                        case 503:
                            display_message(MSG_ERROR,__FILE__,__LINE__, "Service unavailable (503)");
                            break;
                        default:
                            /* No message for other status codes */
                            break;
                    }
                }
                
                /* Always return proper error code regardless of silent mode */
                switch (status_code)
                {
                    case 400:
                        return AD_BAD_REQUEST;
                    case 401:
                        return AD_UNAUTHORIZED;
                    case 403:
                        return AD_FORBIDDEN;
                    case 404:
                        return AD_NOT_FOUND;
                    case 500:
                        return AD_SERVER_ERROR;
                    case 503:
                        return AD_SERVICE_UNAVAILABLE;
                    default:
                        /* For other status codes, return the actual HTTP status code */
                        return status_code;
                }
            }
        }
    }
    
    /* Process header data - combined processing loop for first chunk and any subsequent ones */
    while (!header_done)
    {
        /* Process current buffer first */
        for (i = 0; i < bytes; i++)
        {
            char c = buffer[i];
            
            /* Build header line for logging/parsing */
            if (c == '\r' || c == '\n')
            {
                if (header_line_pos > 0)
                {
                    /* Terminate and process the header line */
                    header_line[header_line_pos] = '\0';
                    process_header_line(header_line, &content_length, silent);
                    
                    /* Reset for next line */
                    header_line_pos = 0;
                }
            }
            else if (header_line_pos < MAX_HEADER_LINE - 1)
            {
                header_line[header_line_pos++] = c;
            }
            
            /* Search for end of headers marker (\r\n\r\n) */
            search_buffer[0] = search_buffer[1];
            search_buffer[1] = search_buffer[2];
            search_buffer[2] = search_buffer[3];
            search_buffer[3] = c;
            search_pos = (search_pos < 3) ? search_pos + 1 : 3;
            
            /* Check if we found \r\n\r\n */
            if (search_pos >= 3 && 
                search_buffer[0] == '\r' && search_buffer[1] == '\n' &&
                search_buffer[2] == '\r' && search_buffer[3] == '\n')
            {
                /* Found end of headers! */
                header_done = 1;
                
                #ifdef DEBUG
                if (!silent)
                {
                    display_message(MSG_DEBUG,__FILE__,__LINE__, "Headers complete. Content-Length: %d", content_length);
                }
                #endif
                
                if (!silent)
                {
                    display_message(MSG_INFO,__FILE__,__LINE__, "Saving \"%s\" (%ld bytes)", output_path,
                                   (LONG)((content_length >= 0) ? content_length : -1));
                }
                
                /* Open output file */
                result = open_output_file(file, output_path, silent);
                if (result != AD_SUCCESS)
                {
                    return result;
                }
                
                /* Get initial time for progress updates */
                GetSysTime(&current_time);
                last_update_time = current_time.tv_secs;
                
                /* Any remaining data in this chunk is body data */
                if (i < bytes - 1)
                {
                    int remaining_bytes = bytes - (i + 1);
                    Write(*file, buffer + i + 1, remaining_bytes);
                    total_bytes_downloaded += remaining_bytes;
                    
                    /* Show initial progress */
                    update_progress(total_bytes_downloaded, content_length);
                }
                
                /* Exit this loop to start normal body processing */
                break;
            }
        }
        
        /* If we found end of headers, exit the header parsing loop */
        if (header_done)
        {
            break;
        }
        
        /* Need to get more data to find complete headers */
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_secs = 1;
        tv.tv_micro = 0;

        wait_res = WaitSelect(sock + 1, &readfds, NULL, NULL, &tv, NULL);
        GetSysTime(&current_time);

        if (ad_ctrl_c_pressed())
        {
            display_message(MSG_INFO,__FILE__,__LINE__, "Cancelled by user (Ctrl-C)");
            return AD_CANCELLED;
        }

        if (wait_res == 0)
        {
            ULONG idle_secs = current_time.tv_secs - idle_start_secs;
            if (current_time.tv_secs > last_wait_report_secs)
            {
                last_wait_report_secs = current_time.tv_secs;
                display_message(MSG_PROGRESS,__FILE__,__LINE__,
                               "Waiting for HTTP headers... %lu/%lu sec   ",
                               idle_secs, timeout_secs);
            }
            if (idle_secs >= timeout_secs)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__,
                               "Timed out waiting for HTTP headers after %lu seconds",
                               timeout_secs);
                return AD_TIMEOUT;
            }
            continue;
        }
        else if (wait_res < 0)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "WaitSelect failed (errno: %d)", errno);
            }
            return AD_SOCKET_ERROR;
        }

        bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        
        /* Handle error or EOF during header processing */
        if (bytes <= 0)
        {
            if (bytes < 0 && errno == EWOULDBLOCK)
            {
                continue;
            }
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Failed to receive complete headers");
            }
            return AD_HEADER_ERROR;
        }

        idle_start_secs = current_time.tv_secs;
        last_wait_report_secs = current_time.tv_secs;
    }
    
    /* Download file body with inactivity timeout */
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_secs = 1;
        tv.tv_micro = 0;

        wait_res = WaitSelect(sock + 1, &readfds, NULL, NULL, &tv, NULL);
        GetSysTime(&current_time);

        if (ad_ctrl_c_pressed())
        {
            display_message(MSG_INFO,__FILE__,__LINE__, "Cancelled by user (Ctrl-C)");
            return AD_CANCELLED;
        }

        if (wait_res == 0)
        {
            ULONG idle_secs = current_time.tv_secs - idle_start_secs;
            if (current_time.tv_secs > last_wait_report_secs)
            {
                last_wait_report_secs = current_time.tv_secs;
                display_message(MSG_PROGRESS,__FILE__,__LINE__,
                               "Waiting for download data... %lu/%lu sec   ",
                               idle_secs, timeout_secs);
            }
            if (idle_secs >= timeout_secs)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__,
                               "Timed out waiting for download data after %lu seconds",
                               timeout_secs);
                return AD_TIMEOUT;
            }
            continue;
        }
        else if (wait_res < 0)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "WaitSelect failed (errno: %d)", errno);
            }
            return AD_SOCKET_ERROR;
        }

        bytes = recv(sock, buffer, BUFFER_SIZE, 0);

        if (bytes == 0)
        {
            break;
        }

        if (bytes < 0)
        {
            if (errno == EWOULDBLOCK)
            {
                continue;
            }
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Receive failed (errno: %d)", errno);
            }
            return AD_SOCKET_ERROR;
        }

        /* Write received data to file */
        if (Write(*file, buffer, bytes) != bytes)
        {
            if (!silent)
            {
                display_message(MSG_ERROR,__FILE__,__LINE__, "Failed to write to output file");
            }
            return AD_FILE_ERROR;
        }
        total_bytes_downloaded += bytes;
        idle_start_secs = current_time.tv_secs;
        
        /* Update progress display once per second */
        if (current_time.tv_secs > last_update_time)
        {
            last_update_time = current_time.tv_secs;
            update_progress(total_bytes_downloaded, content_length);
        }
    }

    /* Restore blocking mode before returning */
    non_blocking = 0;
    IoctlSocket(sock, FIONBIO, &non_blocking);
    
    /* Download successful */
    if (!silent)
    {
        display_message(MSG_CLEAR,__FILE__,__LINE__, "");
        display_message(MSG_INFO,__FILE__,__LINE__, "Download complete. Total: %lu KB", total_bytes_downloaded / 1024);
    }
    
    return AD_SUCCESS;
} /* process_response */

/*------------------------------------------------------------------------*/

/**
 * @brief Update progress display with download status and speed
 *
 * @param bytes_downloaded Current downloaded bytes
 * @param content_length Total expected bytes (can be -1 if unknown)
 */
static void update_progress(ULONG bytes_downloaded, LONG content_length)
{
    char progress_message[80];    /* Progress message buffer */
    char speed_str[32];           /* Speed display string */
    int percent = 0;              /* Percentage of download complete */
    static ULONG last_bytes = 0;  /* Previous bytes downloaded count */
    static ULONG last_time = 0;   /* Previous update time (in seconds) */
    struct timeval current_time;  /* Current system time */
    ULONG current_secs;           /* Current time in seconds */
    ULONG bytes_per_sec = 0;      /* Download speed in bytes/sec */
    
    /* Get current time */
    GetSysTime(&current_time);
    current_secs = current_time.tv_secs;
    
    /* Calculate speed if we have previous measurements */
    if (last_time > 0 && current_secs > last_time)
    {
        ULONG time_diff;      /* Time difference in seconds */
        ULONG bytes_diff;     /* Bytes downloaded since last update */
        
        /* Calculate time and bytes differences */
        time_diff = current_secs - last_time;
        bytes_diff = bytes_downloaded - last_bytes;
        
        /* Calculate speed (bytes per second) */
        if (time_diff > 0)
        {
            bytes_per_sec = bytes_diff / time_diff;
            
            /* Format speed string based on value */
            if (bytes_per_sec >= 1024 * 1024)
            {
                /* Show in MB/s (with integer division and tenths calculation) */
                ULONG mb_whole = bytes_per_sec / (1024 * 1024);
                ULONG mb_tenth = (bytes_per_sec % (1024 * 1024)) / (102 * 1024); /* 1/10 of MB */
                sprintf(speed_str, "%lu.%lu MB/s", mb_whole, mb_tenth);
            }
            else if (bytes_per_sec >= 1024)
            {
                /* Show in KB/s */
                sprintf(speed_str, "%lu KB/s", bytes_per_sec / 1024);
            }
            else
            {
                /* Show in bytes/s */
                sprintf(speed_str, "%lu bytes/s", bytes_per_sec);
            }
        }
        else
        {
            /* Default for very fast updates (less than 1 second between) */
            strcpy(speed_str, "Calculating...");
        }
    }
    else
    {
        /* First update, can't calculate speed yet */
        strcpy(speed_str, "Calculating...");
    }
    
    /* Store current values for next calculation */
    last_bytes = bytes_downloaded;
    last_time = current_secs;
    
    /* Calculate percentage if content length is known */
    if (content_length > 0)
    {
        percent = (int)((bytes_downloaded * 100) / content_length);
        
        /* Use display_progress_bar from display_message.c */
        display_progress_bar(percent, speed_str, "Downloaded: ");
    }
    else
    {
        /* Format the progress message for unknown size downloads */
        sprintf(progress_message, "Downloaded: %lu KB (%s)", 
                bytes_downloaded / 1024, speed_str);
        
        /* Display the progress message */
        display_message(MSG_PROGRESS,__FILE__,__LINE__, progress_message);
    }
} /* update_progress */

/*------------------------------------------------------------------------*/
/* End of Text */