#ifndef AMIGA_HEADERS_H
#define AMIGA_HEADERS_H

#include "platform.h"

/*---------------------------------------------------------------------------*/
/* Amiga System Headers (Only included for Amiga builds)                     */
/*---------------------------------------------------------------------------*/

#if PLATFORM_AMIGA
    #include <exec/types.h>
    #include <exec/memory.h>
    #include <libraries/dos.h>
    #include <dos/dosasl.h>          /* For pattern matching (MatchFirst/MatchNext) */
    #include <dos/dostags.h>         /* For AllocDosObject tags */
    #include <utility/tagitem.h>     /* For TAG_DONE */
    #include <workbench/workbench.h>
    #include <workbench/icon.h>
    #include <proto/exec.h>
    #include <proto/dos.h>
    #include <proto/utility.h>
    #include <proto/icon.h>
    #include <proto/intuition.h>
    #include <proto/graphics.h>

    /* DOS return codes */
    #define RETURN_OK   0
    #define RETURN_FAIL 20

    #ifndef DOS_ANCHORPATH
        #define DOS_ANCHORPATH 2
    #endif
#else
    /* Host build stubs for Amiga types */

    typedef char *STRPTR;
    typedef const char *CONST_STRPTR;

    struct Gadget {
        uint16_t Width;
        uint16_t Height;
    };

    struct DiskObject {
        uint32_t do_Magic;
        uint16_t do_Version;
        struct Gadget do_Gadget;
        uint8_t  do_Type;
        char    *do_DefaultTool;
        char   **do_ToolTypes;
        int32_t  do_CurrentX;
        int32_t  do_CurrentY;
    };

    struct FileInfoBlock {
        int32_t  fib_DirEntryType;
        char     fib_FileName[108];
        uint32_t fib_Size;
    };

    struct DiskObject* GetDiskObject(const char *name);
    void  FreeDiskObject(struct DiskObject *diskobj);
    void* AllocVec(uint32_t byteSize, uint32_t requirements);
    void  FreeVec(void *memoryBlock);
    void* Open(const char *name, int32_t accessMode);
    void  Close(void *file);
    int32_t Read(void *file, void *buffer, int32_t length);
    int32_t Write(void *file, const void *buffer, int32_t length);
    int32_t Seek(void *file, int32_t position, int32_t mode);
    int32_t IoErr(void);

    #define DOS_FIB        1
    #define DOS_ANCHORPATH 2
    void* AllocDosObject(uint32_t type, void *tags);
    void  FreeDosObject(uint32_t type, void *ptr);
    int32_t Examine(void *lock, struct FileInfoBlock *fib);
    int32_t ExNext(void *lock, struct FileInfoBlock *fib);

    struct TextExtent { int16_t te_Width; int16_t te_Height; int16_t te_Extent; };

    #define RETURN_OK   0
    #define RETURN_FAIL 20
    #define MEMF_CLEAR  0x00010000

    #define WBDISK    1
    #define WBDRAWER  2
    #define WBTOOL    3
    #define WBPROJECT 4
    #define WBGARBAGE 5
    #define WBDEVICE  6
    #define WBKICK    7
    #define WBAPPICON 8

    #define MODE_OLDFILE 1005
    #define MODE_NEWFILE 1006

    #ifndef SEEK_SET
        #define SEEK_SET 0
    #endif
    #ifndef SEEK_CUR
        #define SEEK_CUR 1
    #endif
    #ifndef SEEK_END
        #define SEEK_END 2
    #endif

    #define OFFSET_BEGINNING -1
    #define OFFSET_CURRENT    0
    #define OFFSET_END        1

#endif

#endif /* AMIGA_HEADERS_H */
