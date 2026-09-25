#ifndef ROOMS_ACROPOLIS_PATIO_H
#define ROOMS_ACROPOLIS_PATIO_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/coord.h"

/// 0x78 stack scratch the patio's look-at task
/// (`func_acropolis_patio_8017DE2C`) builds each frame and hands to
/// `func_800B0CF4`.
///
/// Only `coord.coord.t` is written - the world point Aya is asked to face,
/// `(-0x1F40, 0, 0x384)` with the patio's own approach offset subtracted from
/// Z - and it is the only part `func_800B0CF4` reads, so the rest of the
/// block is left uninitialised. It is a `GpCoord` rather than a bare
/// `VECTOR` because that is what puts the translation at +0x18, the same
/// offset the callee reads the skeleton's own `GpCoord.coord.t` from.
typedef struct ApLookAtWork {
    /* 0x00 */ GpCoord coord;
    /* 0x50 */ byte    pad_50[0x28];
} ApLookAtWork;
STATIC_ASSERT_SIZEOF(ApLookAtWork, 0x78);

/// The three grey levels a flickering sprite
/// (`func_acropolis_patio_8017E324`) picks its colour from, one per animation
/// column. The task copies the whole set onto its stack before indexing it.
typedef struct ApGreyLevels {
    u8 level[3];
} ApGreyLevels;

extern const ApGreyLevels D_acropolis_patio_8017D5E8;

#endif
