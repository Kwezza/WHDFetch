#include "platform/platform.h"

#include "main.h"

int files_downloaded = 0;
int files_skipped = 0;
int no_skip_messages = 0;
int verbose_output = 0;

int skip_AGA = 0;
int skip_CD = 0;
int skip_NTSC = 0;
int skip_NonEnglish = 0;

long start_time;
