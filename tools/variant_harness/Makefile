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
	src/vh_group.c \
	src/vh_string_pool.c
OBJS := $(SRCS:.c=.o)
STRING_POOL_TEST := tests/vh_string_pool_test
PARSE_GROUP_KEY_TEST := tests/vh_parse_group_key_test

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) tests/test_vh_string_pool.c src/vh_string_pool.c -o $(STRING_POOL_TEST)
	$(CC) $(CFLAGS) tests/test_vh_parse_group_key.c src/vh_parse.c src/vh_csv.c src/vh_string_pool.c -o $(PARSE_GROUP_KEY_TEST)
	powershell -NoProfile -Command "$$p='$(STRING_POOL_TEST).exe'; if (!(Test-Path -LiteralPath $$p)) { $$p='$(STRING_POOL_TEST)' }; $$p=(Resolve-Path -LiteralPath $$p).Path; & $$p; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE }"
	powershell -NoProfile -Command "$$p='$(PARSE_GROUP_KEY_TEST).exe'; if (!(Test-Path -LiteralPath $$p)) { $$p='$(PARSE_GROUP_KEY_TEST)' }; $$p=(Resolve-Path -LiteralPath $$p).Path; & $$p; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE }"
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_milestone4_tests.ps1
else
	$(CC) $(CFLAGS) tests/test_vh_string_pool.c src/vh_string_pool.c -o $(STRING_POOL_TEST)
	$(CC) $(CFLAGS) tests/test_vh_parse_group_key.c src/vh_parse.c src/vh_csv.c src/vh_string_pool.c -o $(PARSE_GROUP_KEY_TEST)
	./$(STRING_POOL_TEST)
	./$(PARSE_GROUP_KEY_TEST)
	./$(TARGET) --select tests/filenames_basic.txt --profile default > test_output/basic_select.txt
	./$(TARGET) --report tests/filenames_basic.txt --profile default > test_output/basic_report.txt
	./$(TARGET) --report tests/filenames_memory.txt --profile default > test_output/memory_report.txt
	./$(TARGET) --report tests/filenames_language.txt --profile default > test_output/language_report.txt
	./$(TARGET) --report tests/filenames_special_report_only.txt --profile default > test_output/special_report_only.txt
endif

ifeq ($(OS),Windows_NT)
clean:
	-cmd /C "del /F /Q $(TARGET) $(TARGET).exe $(STRING_POOL_TEST) $(STRING_POOL_TEST).exe $(PARSE_GROUP_KEY_TEST) $(PARSE_GROUP_KEY_TEST).exe 2>nul & del /F /Q src\\*.o 2>nul"
else
clean:
	-rm -f $(TARGET) $(TARGET).exe $(STRING_POOL_TEST) $(STRING_POOL_TEST).exe $(PARSE_GROUP_KEY_TEST) $(PARSE_GROUP_KEY_TEST).exe src/*.o
endif
