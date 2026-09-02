#ifndef ROOMS_MIST_R18_H
#define ROOMS_MIST_R18_H

#include "common.h"

/// Sprite description the overlay's two primitive emitters read from.
///
/// `func_mist_r18_8017E448` turns it into a flat-shaded `TILE` and
/// `func_mist_r18_8017E534` into a textured `SPRT`; both take the screen
/// rectangle from `x`/`y`/`w`/`h`, the colour from `r`/`g`/`b`, and pick the
/// `SetShadeTex` argument from `semiTrans`. Only the textured emitter reads
/// `u`/`v`.
typedef struct MistR18Sprite {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ u8  u;
    /* 0x05 */ u8  pad_5;
    /* 0x06 */ u8  v;
    /* 0x07 */ u8  pad_7;
    /* 0x08 */ s16 w;
    /* 0x0A */ s16 h;
    /* 0x0C */ u8  r;
    /* 0x0D */ u8  g;
    /* 0x0E */ u8  b;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 semiTrans;
} MistR18Sprite;

STATIC_ASSERT_SIZEOF(MistR18Sprite, 0x12);

/// Spawn descriptor for the sprite task `func_mist_r18_8017E3A4` drives: the
/// screen rectangle it redraws every frame, hung off `Task::spawnArg2`.
typedef struct MistR18SpriteSpawn {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
} MistR18SpriteSpawn;

STATIC_ASSERT_SIZEOF(MistR18SpriteSpawn, 0x8);

#endif
