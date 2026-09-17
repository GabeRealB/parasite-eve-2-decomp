#ifndef ACTOR_105600_H
#define ACTOR_105600_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgs.h>

/// 0xF0-byte body block `func_actor_105600_80134FD0` parks at `Task::idMap`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `GpObj` bodies collide against `rec60`
/// (shared by the first two) and, through the `GpActorD4Rec` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct Actor105600FxWork {
    /* 0x00 */ MATRIX       colorMtx;
    /* 0x20 */ MATRIX       lightMtx;
    /* 0x40 */ GpObj        obj40;
    /* 0x60 */ GpRec18      rec60[1];
    /* 0x78 */ GpObj        obj78;
    /* 0x98 */ GpObj        obj98;
    /* 0xB8 */ GpActorD4Rec d4rec;
    /* 0xD0 */ GpRec18      recD0[1];
    /* 0xE8 */ s16          field_E8;
    /* 0xEA */ s16          field_EA;
    /* 0xEC */ byte         pad_EC[2];
    /* 0xEE */ s16          field_EE;
} Actor105600FxWork;
STATIC_ASSERT_SIZEOF(Actor105600FxWork, 0xF0);

/// 0x14-byte placement descriptor in the overlay's `.data`, handed to
/// `Gp_PackPair` as the source of the body objects' `GpObj.field_18`.
/// `field_E` is the variant flag `func_actor_105600_80134FD0` latches into its
/// work block: it is 1 when the actor is placed normally, and anything else
/// puts the body in the other pose.
typedef struct Actor105600PlaceSrc {
    /* 0x00 */ GpU16Pair pair;
    /* 0x04 */ u16       field_4;
    /* 0x06 */ u16       field_6;
    /* 0x08 */ u16       field_8;
    /* 0x0A */ u16       field_A;
    /* 0x0C */ u16       field_C;
    /* 0x0E */ u16       field_E;
    /* 0x10 */ u16       field_10;
    /* 0x12 */ u16       field_12;
} Actor105600PlaceSrc;
STATIC_ASSERT_SIZEOF(Actor105600PlaceSrc, 0x14);

/// 0x38-byte scratch carved off `G_SCRATCH_HEAD` by
/// `func_actor_105600_80134FD0`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct Actor105600PlaceScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ VECTOR  pos;
    /* 0x18 */ MATRIX  mtx;
} Actor105600PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor105600PlaceScratch, 0x38);

/// Spawn/context block behind `Task::spawnArg2`; `field_8` is the halfword the
/// sound id takes its room/channel bits from.
typedef struct Actor105600Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor105600Ctx;

/// Placement descriptor for this actor.
extern Actor105600PlaceSrc D_actor_105600_80147FDC;

/// Sound id of the burst cue, with the spawn context's room/channel bits packed
/// in.
extern s32 D_actor_105600_80148110;

#endif
