#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vh_csv.h"
#include "vh_fields.h"
#include "vh_group.h"
#include "vh_parse.h"
#include "vh_profile.h"

#define VH_PATH_MAX 512

static void print_help(const char *argv0)
{
    printf("Variant Selection Harness (Milestone 3)\n");
    printf("Usage: %s [command]\n\n", argv0);
    printf("Commands:\n");
    printf("  --help                               Show this help\n");
    printf("  --parse <filename>                   Parse filename fields\n");
    printf("  --resolve <field> <token>            Resolve token via CSV lookup\n");
    printf("  --select <listfile> --profile <name> Select one candidate per group\n");
    printf("  --report <listfile> --profile <name> Report grouped scoring details\n");
}

static void print_supported_fields(void)
{
    printf("Supported fields: Memory, Language, Chipset, Video, Media\n");
}

static int handle_resolve(const char *field_name, const char *token)
{
    VhField field;
    const char *csv_file;
    char csv_path[VH_PATH_MAX];
    VhCsvFile csv;
    VhCsvResult result;
    int matched;

    if (!vh_field_from_name(field_name, &field)) {
        fprintf(stderr, "Unknown field: %s\n", field_name);
        print_supported_fields();
        return 1;
    }

    csv_file = vh_field_csv_filename(field);
    if (csv_file == NULL) {
        fprintf(stderr, "No CSV mapping configured for field: %s\n", field_name);
        return 1;
    }

    if (snprintf(csv_path, sizeof(csv_path), "data/Defs/%s", csv_file) >= (int)sizeof(csv_path)) {
        fprintf(stderr, "CSV path too long for field: %s\n", field_name);
        return 1;
    }

    if (!vh_csv_load(&csv, csv_path)) {
        fprintf(stderr, "Failed to load CSV: %s\n", csv_path);
        return 1;
    }

    matched = vh_csv_lookup_token(&csv, token, &result);

    printf("field: %s\n", vh_field_display_name(field));
    printf("token: %s\n", token);
    printf("matched: %s\n", matched ? "yes" : "no");
    if (matched) {
        printf("id: %d\n", result.id);
        printf("canonical: %s\n", (result.canonical != NULL) ? result.canonical : "");
        printf("description: %s\n", (result.description != NULL) ? result.description : "");
    }

    vh_csv_free(&csv);
    return matched ? 0 : 1;
}

static void print_token_list(const char *label, const VhTokenList *list)
{
    int i;

    printf("%s: ", label);
    if (list == NULL || list->count == 0) {
        printf("\n");
        return;
    }

    for (i = 0; i < list->count; ++i) {
        if (i > 0) {
            printf(", ");
        }
        printf("%s", list->items[i].text);
    }
    printf("\n");
}

static int handle_parse(const char *filename)
{
    VhParseContext ctx;
    VhParsedName parsed;

    if (!vh_parse_context_load(&ctx, "data/Defs")) {
        fprintf(stderr, "Failed to load parser CSV definitions from data/Defs\n");
        return 1;
    }

    if (!vh_parse_filename(&ctx, filename, &parsed)) {
        vh_parse_context_free(&ctx);
        fprintf(stderr, "Failed to parse filename: %s\n", filename);
        return 1;
    }

    printf("archive: %s\n", parsed.archive_name);
    printf("title: %s\n", parsed.title);
    printf("group_key: %s\n", parsed.group_key);
    printf("version: %s\n", parsed.version);
    print_token_list("chipset", &parsed.chipset);
    print_token_list("memory", &parsed.memory);
    print_token_list("language", &parsed.language);
    print_token_list("video", &parsed.video);
    print_token_list("media", &parsed.media);
    print_token_list("special", &parsed.special);
    print_token_list("unknown", &parsed.unknown);

    vh_parse_context_free(&ctx);
    return 0;
}

static int build_profile_path(const char *profile_arg, char *out_path, size_t out_size)
{
    if (strchr(profile_arg, '/') != NULL || strchr(profile_arg, '\\') != NULL || strchr(profile_arg, '.') != NULL) {
        return snprintf(out_path, out_size, "%s", profile_arg) < (int)out_size;
    }
    return snprintf(out_path, out_size, "data/Profiles/%s.profile", profile_arg) < (int)out_size;
}

static int run_selection(const char *listfile, const char *profile_arg, int report_mode)
{
    VhParseContext parse_ctx;
    VhProfile profile;
    VhCandidateList candidates;
    char profile_path[VH_PATH_MAX];

    if (!build_profile_path(profile_arg, profile_path, sizeof(profile_path))) {
        fprintf(stderr, "Profile path is too long: %s\n", profile_arg);
        return 1;
    }

    if (!vh_parse_context_load(&parse_ctx, "data/Defs")) {
        fprintf(stderr, "Failed to load parser CSV definitions from data/Defs\n");
        return 1;
    }

    if (!vh_profile_load(&profile, profile_path, &parse_ctx)) {
        vh_parse_context_free(&parse_ctx);
        fprintf(stderr, "Failed to load profile: %s\n", profile_path);
        return 1;
    }

    if (!vh_group_load_candidates(&candidates, listfile, &parse_ctx)) {
        vh_profile_free(&profile);
        vh_parse_context_free(&parse_ctx);
        fprintf(stderr, "Failed to load candidate list: %s\n", listfile);
        return 1;
    }

    if (!vh_group_select_best(&candidates, &profile)) {
        vh_group_free_candidates(&candidates);
        vh_profile_free(&profile);
        vh_parse_context_free(&parse_ctx);
        fprintf(stderr, "Failed to select candidates\n");
        return 1;
    }

    if (report_mode) {
        vh_group_print_report(&candidates);
    } else {
        vh_group_print_selected(&candidates);
    }

    vh_group_free_candidates(&candidates);
    vh_profile_free(&profile);
    vh_parse_context_free(&parse_ctx);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--parse") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s --parse <filename>\n", argv[0]);
            return 1;
        }
        return handle_parse(argv[2]);
    }

    if (strcmp(argv[1], "--resolve") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s --resolve <field> <token>\n", argv[0]);
            print_supported_fields();
            return 1;
        }
        return handle_resolve(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "--select") == 0) {
        if (argc != 5 || strcmp(argv[3], "--profile") != 0) {
            fprintf(stderr, "Usage: %s --select <listfile> --profile <name|path>\n", argv[0]);
            return 1;
        }
        return run_selection(argv[2], argv[4], 0);
    }

    if (strcmp(argv[1], "--report") == 0) {
        if (argc != 5 || strcmp(argv[3], "--profile") != 0) {
            fprintf(stderr, "Usage: %s --report <listfile> --profile <name|path>\n", argv[0]);
            return 1;
        }
        return run_selection(argv[2], argv[4], 1);
    }

    fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
    print_help(argv[0]);
    return 1;
}
