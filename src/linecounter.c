#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>


#define BUFSIZE 8192  /* Size of the read buffer, in bytes */

/*
 * CountLinesBufferedUnrolled
 * --------------------------
 * Counts the number of newline characters ('\n') in the given text file.
 * This version uses a fixed-size memory buffer (BUFSIZE) and reads the file
 * in chunks for performance. The loop is unrolled by two to reduce overhead.
 *
 * Parameters:
 *   filename - a null-terminated string path to the file to read
 *
 * Returns:
 *   Number of lines (number of '\n' characters) on success,
 *   or -1 on failure (file not found or memory allocation error).
 */
LONG CountLines(STRPTR filename)
{
    BPTR fh;              /* File handle returned by Open() */
    UBYTE *buffer;        /* Pointer to memory buffer for reading file */
    LONG bytesRead;       /* Number of bytes read in each chunk */
    LONG i;               /* Loop counter */
    LONG lines = 0;       /* Line counter */

    /* Open the file for reading */
    fh = Open(filename, MODE_OLDFILE);
    if (fh == (BPTR)0)
    {
        return -1;  /* Failed to open file */
    }

    /* Allocate buffer memory in FAST RAM for performance
     if available, otherwise use chip mem*/
    buffer = (UBYTE *)AllocMem(BUFSIZE, MEMF_ANY);
    if (buffer == NULL)
    {
        Close(fh);
        return -1;  /* Failed to allocate memory */
    }

    /* Read the file in BUFSIZE-byte chunks */
    while ((bytesRead = Read(fh, buffer, BUFSIZE)) > 0)
    {
        /* Loop unrolled by 2: process two bytes per iteration */
        for (i = 0; i < bytesRead - 1; i += 2)
        {
            if (buffer[i] == '\n') lines++;
            if (buffer[i + 1] == '\n') lines++;
        }

        /* If an odd number of bytes was read, check the last byte */
        if (i < bytesRead)
        {
            if (buffer[i] == '\n') lines++;
        }
    }

    /* Clean up: free memory and close file */
    FreeMem(buffer, BUFSIZE);
    Close(fh);

    return lines;
}