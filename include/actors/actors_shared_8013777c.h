#ifndef ACTORS_SHARED_8013777C_H
#define ACTORS_SHARED_8013777C_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "main/task.h"

/// 0x38-byte scratch the movement step takes off `G_SCRATCH_HEAD`. The
/// `GpDeltaScratch` at 0x20 is the one `func_800E0C10` fills with the 16.16
/// translation toward the collision response; the block is handed out whole so
/// the walk's own `GpSlideScratch`, taken below the head, stays clear of it.
/// Same shape as `GrenadePistolScratch`.
typedef struct ActorsShared8013777cScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
} ActorsShared8013777cScratch;
STATIC_ASSERT_SIZEOF(ActorsShared8013777cScratch, 0x38);

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names the two
/// collision-record tables `func_800E0C10` and `Gp_ClearRec18Occupied` walk,
/// and the step's own position and speed scalars. It overlaps the carriers'
/// own layouts - `Actor107000Work::field_2CC`, an `s16`, sits where the second
/// table starts - so the two are views of one block, not one layout.
typedef struct ActorsShared8013777cWork {
    /* 0x000 */ byte    pad_0[0x24C];
    /* 0x24C */ GpRec18 field_24C[4]; // collision table; `func_800E0C10` steps it with count 4
    /* 0x2AC */ byte    pad_2AC[0x20];
    /* 0x2CC */ GpRec18 field_2CC;    // second record table, wiped once the step is done
    /* 0x2E4 */ s16     field_2E4;    // armed to 0x400 when the step is taken
    /* 0x2E6 */ byte    pad_2E6[0x56];
    /* 0x33C */ VECTOR3 field_33C;    // position the mode-2 arm snaps back to
    /* 0x348 */ byte    pad_348[0x30];
    /* 0x378 */ u16     field_378;    // forward speed; a step loses a quarter of it
    /* 0x37A */ byte    pad_37A[0x12];
    /* 0x38C */ s16     field_38C;    // latched step mode
    /* 0x38E */ byte    pad_38E[0xA];
    /* 0x398 */ s16     field_398;    // vertical speed, -0x50 while a step runs
    /* 0x39A */ u16     field_39A;    // non-zero once the step has been taken
} ActorsShared8013777cWork;

/// Steps the carrier's coordinate along the collision response; carried by
/// `actor_107000` and `actor_207000`.
void ActorsShared8013777c(Task* arg0);

#endif
