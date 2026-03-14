#ifndef LINECOUNTER_H
#define LINECOUNTER_H

#include <exec/types.h>
#include <dos/dos.h>

/**
@Function CountLines

    Counts the number of lines in a text file by scanning for newline
    characters ('\n') using a buffered read with 2x loop unrolling for
    performance. Memory is allocated in FAST RAM to speed up processing
    on systems with fast memory.

@Synopsis
    LONG CountLines(STRPTR filename);

@Input
    filename - A pointer to a null-terminated string containing the
               path of the file to read. The file must be a text file
               accessible via dos.library.

@Result
    Returns the number of newline characters found in the file on success.
    Returns -1 on error (e.g. file could not be opened or buffer allocation
    failed).

@Notes
    The function reads the file in chunks (default 8192 bytes) and does not
    load the entire file into memory. It is optimized for performance using
    loop unrolling and MEMF_FAST allocation.

@Bugs
    Only counts lines based on the newline character '\n'. Files using
    carriage return '\r' or CRLF (\r\n) line endings may not count correctly
    unless pre-processed.

@Example
    STRPTR path = "ram:myfile.txt";
    LONG count = CountLines(path);
    if (count >= 0)
        // success
    else
        // error

@SeeAlso
    Open(), Read(), AllocMem(), FreeMem()

**/

LONG CountLines(STRPTR filename);

#endif /* LINECOUNTER_H */
