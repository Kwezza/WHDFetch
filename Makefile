################################################################################
# amiga-base - Amiga Build Makefile (vbcc +aos68k)
#
# Usage:
#   make                    - release build (no console, no memtrack)
#   make CONSOLE=1          - enable console window + printf output
#   make MEMTRACK=1         - enable memory leak detection
#   make CONSOLE=1 MEMTRACK=1
#   make clean              - remove build artefacts
#   make help               - show this help
#
# Customise:
#   APP_NAME   - displayed in window title and OOM dialogs
#   PROJECT    - output executable name
################################################################################

CONSOLE  ?= 0
MEMTRACK ?= 0

# ---------------------------------------------------------------------------
# Project identity - change these for each new project
# ---------------------------------------------------------------------------

APP_NAME = AmigaApp
PROJECT  = AmigaApp

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------

SRC_DIR   = src
BUILD_DIR = build
OUT_DIR   = $(BUILD_DIR)/amiga
BIN_DIR   = Bin/Amiga
BIN       = $(BIN_DIR)/$(PROJECT)

# ---------------------------------------------------------------------------
# Feature flags
# ---------------------------------------------------------------------------

ifeq ($(CONSOLE),1)
    CONSOLE_FLAG = -DENABLE_CONSOLE
else
    CONSOLE_FLAG =
endif

ifeq ($(MEMTRACK),1)
    MEMTRACK_FLAG = -DDEBUG_MEMORY_TRACKING
else
    MEMTRACK_FLAG =
endif

# ---------------------------------------------------------------------------
# Compiler settings (VBCC cross-compiler, AmigaOS 68000 target)
# ---------------------------------------------------------------------------

CC     = vc
# NOTE: VBCC on Windows strips quotes from -D string values, so AMIGA_APP_NAME
# cannot be set via -D. Edit the #define in src/platform/platform.h instead.
CFLAGS = +aos68k -c99 -cpu=68000 -O2 -size \
         -I$(SRC_DIR) \
         -DPLATFORM_AMIGA=1 \
         -D__AMIGA__ \
         -DDEBUG \
         $(CONSOLE_FLAG) \
         $(MEMTRACK_FLAG)

LDFLAGS = +aos68k -cpu=68000 -O2 -size -final -lamiga -lauto

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

SRCS = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/log/log.c \
    $(SRC_DIR)/platform/platform.c

# ---------------------------------------------------------------------------
# Object files
# ---------------------------------------------------------------------------

OBJS = \
    $(OUT_DIR)/main.o \
    $(OUT_DIR)/log/log.o \
    $(OUT_DIR)/platform/platform.o

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all clean help directories

all: directories $(BIN)

directories:
	@if not exist "$(OUT_DIR)"           mkdir "$(OUT_DIR)"
	@if not exist "$(OUT_DIR)\log"       mkdir "$(OUT_DIR)\log"
	@if not exist "$(OUT_DIR)\platform"  mkdir "$(OUT_DIR)\platform"
	@if not exist "$(BIN_DIR)"           mkdir "$(BIN_DIR)"

# Link
$(BIN): $(OBJS)
	@echo Linking: $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo Build complete: $(BIN)

# Compile rules
$(OUT_DIR)/main.o: $(SRC_DIR)/main.c
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/log/log.o: $(SRC_DIR)/log/log.c
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/platform/platform.o: $(SRC_DIR)/platform/platform.c
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@if exist "$(OUT_DIR)" rmdir /S /Q "$(OUT_DIR)"
	@echo Done.

help:
	@echo amiga-base Build System
	@echo =======================
	@echo.
	@echo   make                    Build release (no console, no memtrack)
	@echo   make CONSOLE=1          Enable console window
	@echo   make MEMTRACK=1         Enable memory tracking
	@echo   make clean              Remove build artefacts
	@echo.
	@echo   APP_NAME=$(APP_NAME)  (edit src/platform/platform.h to change AMIGA_APP_NAME)
	@echo   PROJECT=$(PROJECT)
	@echo   BIN=$(BIN)
	@echo   CONSOLE=$(CONSOLE)
	@echo   MEMTRACK=$(MEMTRACK)
