/*
 * Amiga CLI Utilities - Implementation
 *
 * This file contains functions to retrieve cursor position and console size
 * for AmigaOS CLI environments. It makes use of ANSI escape sequences
 * and works on Amiga Workbench 2.04 and later.
 *
 * Compiles with SAS/C.
 */

 #include "cli_utilities.h"
 #include <exec/types.h>
 #include <dos/dos.h>
 #include <proto/dos.h>
 #include <stdio.h>


 /* Function to retrieve the cursor position in the Amiga CLI */
 CursorPos getCursorPos(void) {
     BPTR fh;
     CursorPos pos = { -1, -1 };
     char buffer[32];
     int i = 0;
     char ch;
     LONG bytesRead;
 
     /* Open the console for direct communication */
     fh = Open("CONSOLE:", MODE_OLDFILE);
     if (fh == 0) {
         return pos;
     }
 
     SetMode(fh, 1);
 
     /* Send the cursor position request sequence */
     if (Write(fh, "\x9b" "6n", 3) != 3) {
         SetMode(fh, 0);
         Close(fh);
         return pos;
     }
 
     /* Small delay to ensure the response is available */
     Delay(10);
 
     /* Read the response character by character */
     while (i < (int)(sizeof(buffer) - 1)) {
         bytesRead = Read(fh, &ch, 1);
         if (bytesRead <= 0) {
             break;
         }
         buffer[i++] = ch;
         if (ch == 'R') {
             break;
         }
     }
     buffer[i] = '\0';
 
     SetMode(fh, 0);
     Close(fh);
 
     /* Parse the response string for cursor position */
     if (sscanf(buffer, "\x9b%d;%dR", &pos.yPos, &pos.xPos) != 2) {
         pos.xPos = -1;
         pos.yPos = -1;
     }
 
     return pos;
 }
 
 /* Function to retrieve the console window size */
 ConsoleSize getConsoleSize(void) {


     BPTR fh;
     ConsoleSize size = { 24, 80 };
     char buffer[64];
     int i = 0;
     char ch;
     LONG bytesRead;
 
     /* Open the console for reading and writing */
     fh = Open("CONSOLE:", MODE_OLDFILE);
     if (fh == 0) {
         return size;
     }
 
     SetMode(fh, 1);
 
     /* Send the Window Status Request command */
     if (Write(fh, "\x9b" "0 q", 4) != 4) {
         SetMode(fh, 0);
         Close(fh);
         return size;
     }
 
     Delay(5);
 
     /* Read the response from the console */
     while (i < (int)(sizeof(buffer) - 1)) {
         bytesRead = Read(fh, &ch, 1);
         if (bytesRead <= 0 || ch == 'r') {
             break;
         }
         buffer[i++] = ch;
     }
     buffer[i] = '\0';
 
     SetMode(fh, 0);
     Close(fh);
 
     /* Parse the response string for console size */
     if (sscanf(buffer, "\x9b" "1;1;%d;%d r", &size.rows, &size.columns) != 2) {
         size.rows = 24;
         size.columns = 80;
     }
 
     return size;
 }
 
void ClearLine_SetCursorToStartOfLine(void) {
    printf("\x1B[2K");
    printf("\x1b[2K\r");
}


