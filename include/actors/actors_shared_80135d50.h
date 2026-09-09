#ifndef ACTORS_SHARED_80135D50_H
#define ACTORS_SHARED_80135D50_H

#include "common.h"

#include "main/task.h"

/// The part of the carrier's work block the light ramp touches. Each carrier's
/// block is its own, larger type; the shared unit only names these two fields.
typedef struct ActorsShared80135d50Work {
    /* 0x000 */ byte pad_0[0x2A4];
    /* 0x2A4 */ s16  field_2A4; ///< blend counter, ramped between 0 and 0x12
    /* 0x2A6 */ s16  field_2A6; ///< non-zero while the blend should rise
} ActorsShared80135d50Work;

/// The display context handed to the actor's state functions as `spawnArg2`.
/// Only the flag at 0x14 that the light ramp toggles is modelled so far.
typedef struct ActorsShared80135d50Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[3];
} ActorsShared80135d50Ctx;

void ActorsShared80135d50(Task* task);

#endif
