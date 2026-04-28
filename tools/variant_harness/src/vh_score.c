#include "vh_score.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const VhTokenList *vh_parsed_list_for_field(const VhParsedName *parsed, VhProfileField field)
{
    if (parsed == NULL) {
        return NULL;
    }

    switch (field) {
        case VH_PROFILE_FIELD_LANGUAGE: return &parsed->language;
        case VH_PROFILE_FIELD_CHIPSET: return &parsed->chipset;
        case VH_PROFILE_FIELD_VIDEO: return &parsed->video;
        case VH_PROFILE_FIELD_MEMORY: return &parsed->memory;
        case VH_PROFILE_FIELD_MEDIA: return &parsed->media;
        case VH_PROFILE_FIELD_SPECIAL: return &parsed->special;
        default: return NULL;
    }
}

static int vh_idlist_contains(const VhIdList *list, int id)
{
    int i;

    if (list == NULL) {
        return 0;
    }

    for (i = 0; i < list->count; ++i) {
        if (list->ids[i] == id) {
            return 1;
        }
    }

    return 0;
}

static int vh_best_include_rank(const VhTokenList *tokens, const VhIdList *includes)
{
    int best_rank;
    int i;

    if (tokens == NULL || includes == NULL || tokens->count == 0 || includes->count == 0) {
        return -1;
    }

    best_rank = -1;

    for (i = 0; i < tokens->count; ++i) {
        int j;
        for (j = 0; j < includes->count; ++j) {
            if (tokens->items[i].id == includes->ids[j]) {
                if (best_rank < 0 || j < best_rank) {
                    best_rank = j;
                }
                break;
            }
        }
    }

    return best_rank;
}

void vh_score_candidate(const VhParsedName *parsed, const VhProfile *profile, VhScoreResult *out)
{
    int field_index;

    if (out == NULL) {
        return;
    }

    out->score = 0;
    out->rejected = 0;
    out->reject_reason[0] = '\0';

    if (parsed == NULL || profile == NULL) {
        return;
    }

    for (field_index = 0; field_index < VH_PROFILE_FIELD_COUNT; ++field_index) {
        const VhFieldRule *rule = &profile->rules[field_index];
        const VhTokenList *tokens = vh_parsed_list_for_field(parsed, (VhProfileField)field_index);
        int i;

        if (tokens == NULL) {
            continue;
        }

        for (i = 0; i < tokens->count; ++i) {
            if (vh_idlist_contains(&rule->exclude, tokens->items[i].id)) {
                out->rejected = 1;
                snprintf(out->reject_reason, sizeof(out->reject_reason),
                    "%s excluded by profile token '%s'",
                    vh_profile_field_name((VhProfileField)field_index),
                    tokens->items[i].text);
                return;
            }
        }

        if (rule->weight == 0) {
            continue;
        }

        {
            int rank = vh_best_include_rank(tokens, &rule->include);
            if (rank >= 0) {
                int field_score = rule->include.count - rank;
                out->score += (long)field_score * (long)rule->weight;
            }
        }
    }
}
