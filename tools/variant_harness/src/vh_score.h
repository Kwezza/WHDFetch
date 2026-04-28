#ifndef VH_SCORE_H
#define VH_SCORE_H

#include "vh_parse.h"
#include "vh_profile.h"

typedef struct VhScoreResult {
    long score;
    int rejected;
    char reject_reason[128];
} VhScoreResult;

void vh_score_candidate(const VhParsedName *parsed, const VhProfile *profile, VhScoreResult *out);

#endif
