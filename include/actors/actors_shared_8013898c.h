#ifndef ACTORS_SHARED_8013898C_H
#define ACTORS_SHARED_8013898C_H

#include "common.h"

#include <psyq/libgte.h>

/// Movement freeze flag: the body skips its whole step while this is set.
extern u8 D_80072729;

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8` - the `flg` at 0 /
/// local matrix at 4 shape `ActorsShared8013231cCoord` and
/// `ActorsShared801334c4Coord` document for their own bodies. Only `flg` and
/// the rotation are touched here: the body reads column 2 of that rotation
/// (the object's forward axis) and adds the GPF-scaled result back into the X
/// and Z translation, then clears `flg` so the next update rebuilds the world
/// matrix from it.
typedef struct ActorsShared8013898cCoord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared8013898cCoord;
STATIC_ASSERT_SIZEOF(ActorsShared8013898cCoord, 0x4C);

/// The caller's vector slot: the column copy fills the `SVECTOR` at 0x10, GPF
/// scales it in place and the translation update reads `vx` / `vz` back out of
/// it. Nothing ahead of 0x10 is read, and no carrier references this body at
/// all, so what the caller keeps there is not visible from here.
typedef struct ActorsShared8013898cVec {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} ActorsShared8013898cVec;

void ActorsShared8013898c(ActorsShared8013898cCoord* arg0, ActorsShared8013898cVec* arg1, s32 arg2);

#endif
