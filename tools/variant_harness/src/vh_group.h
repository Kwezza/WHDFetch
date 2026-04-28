#ifndef VH_GROUP_H
#define VH_GROUP_H

#include "vh_parse.h"
#include "vh_profile.h"

#define VH_MAX_CANDIDATE_NAME 256
#define VH_MAX_GROUP_KEY 128
#define VH_MAX_REJECT_REASON 128

typedef struct VhCandidate {
    char archive_name[VH_MAX_CANDIDATE_NAME];
    char group_key[VH_MAX_GROUP_KEY];
    int original_index;
    int selected;
    int rejected;
    long score;
    char reject_reason[VH_MAX_REJECT_REASON];
    VhParsedName parsed;
} VhCandidate;

typedef struct VhCandidateList {
    VhCandidate *items;
    int count;
    int capacity;
} VhCandidateList;

int vh_group_load_candidates(VhCandidateList *list, const char *listfile, const VhParseContext *ctx);
void vh_group_free_candidates(VhCandidateList *list);
int vh_group_select_best(VhCandidateList *list, const VhProfile *profile);
void vh_group_print_selected(const VhCandidateList *list);
void vh_group_print_report(const VhCandidateList *list);

#endif
