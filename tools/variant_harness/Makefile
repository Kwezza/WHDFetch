CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic
LDFLAGS ?=

TARGET := variant_harness
SRCS := src/main.c \
	src/vh_csv.c \
	src/vh_fields.c \
	src/vh_parse.c \
	src/vh_profile.c \
	src/vh_score.c \
	src/vh_group.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_milestone4_tests.ps1
else
	./$(TARGET) --select tests/filenames_basic.txt --profile default > test_output/basic_select.txt
	./$(TARGET) --report tests/filenames_basic.txt --profile default > test_output/basic_report.txt
	./$(TARGET) --report tests/filenames_memory.txt --profile default > test_output/memory_report.txt
	./$(TARGET) --report tests/filenames_language.txt --profile default > test_output/language_report.txt
	./$(TARGET) --report tests/filenames_special_report_only.txt --profile default > test_output/special_report_only.txt
endif

ifeq ($(OS),Windows_NT)
clean:
	-powershell -NoProfile -Command "if (Test-Path -LiteralPath '$(TARGET)') { Remove-Item -LiteralPath '$(TARGET)' -Force -ErrorAction SilentlyContinue }"
	-powershell -NoProfile -Command "if (Test-Path -LiteralPath '$(TARGET).exe') { Remove-Item -LiteralPath '$(TARGET).exe' -Force -ErrorAction SilentlyContinue }"
	-powershell -NoProfile -Command "Get-ChildItem -Path 'src/*.o' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue"
else
clean:
	-rm -f $(TARGET) $(TARGET).exe src/*.o
endif
