#ifndef ACTORS_SHARED_801385E0_H
#define ACTORS_SHARED_801385E0_H

#include "common.h"

#include <psyq/libgte.h>

/// One 12-bit fixed-point factor per column of the matrix `ActorsShared801385e0`
/// scales (`0x1000` is 1.0). The body reads the first three words; its only call
/// site copies the four words at `D_actor_104900_80131E30` - `0x1400`, `0x1400`,
/// `0x1400`, 0 - onto its own frame and passes that address, so the trailing word
/// is part of what the caller copies but nothing here reads it.
typedef struct ActorsShared801385e0Scale {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad_C;
} ActorsShared801385e0Scale;
STATIC_ASSERT_SIZEOF(ActorsShared801385e0Scale, 0x10);

/// Scales a 12-bit fixed-point rotation matrix in place, one GPF fraction per
/// column: the column is copied into an 8-byte vector carved off
/// `G_SCRATCH_HEAD`, the matching factor from `arg1` goes to the GTE's IR0,
/// `gpf 12` multiplies it through IR1-IR3 and the saturated result is stored
/// back over the same column. The scratch vector is handed back before
/// returning.
///
/// Byte-identical in the five actor slots 101100 / 104900 / 201100 / 204900 /
/// 301100, at 0x801385E0 in the first two and 0x801505E0 / 0x801505E0 /
/// 0x801685E0 in the rest. `func_actor_104900_8013279C` is the only carrier
/// that calls it: it writes an identity matrix into the coordinate at `+4` of
/// its work block and scales it here by `0x1400` (1.25) on every axis.
void ActorsShared801385e0(MATRIX* arg0, ActorsShared801385e0Scale* arg1);

#endif // ACTORS_SHARED_801385E0_H
