#ifndef ACTOR_105700_H
#define ACTOR_105700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/task.h"

#include <psyq/libgs.h>

extern u8 D_801153F2;

/// 0x6E4-byte work block hung off `Actor105700.field_1C`, allocated by
/// `func_actor_105700_80135AE4`. It opens with the animation context and its
/// nineteen 0x28-byte slots, exactly like the `Actor02000Work` block of
/// `actor_102000`; the animation/state halfwords around 0x694-0x6E0 keep that
/// block's offsets and meaning.
typedef struct Actor105700Work {
    /* 0x000 */ GpAnimCtx ctx;
    /* 0x014 */ byte      slots[19][0x28];
    /* 0x30C */ byte      pad_30C[0x2F0];
    /// Object handed to the body by `Gp_PackPair` when `field_698` first
    /// reaches the animation's 0x1C mark (`func_actor_105700_801341CC`).
    /* 0x5FC */ s32  field_5FC;
    /* 0x600 */ byte pad_600[2];
    /// Bit 0x8000 is raised with `field_5FC` at the 0x1C mark and dropped
    /// again at the 0x28 mark.
    /* 0x602 */ u16  field_602;
    /* 0x604 */ byte pad_604[0x90];
    /// Animation index selected by the state machine; 4 is the "handover"
    /// clip of `func_actor_105700_80136AE0`'s state 0.
    /* 0x694 */ s16  field_694;
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698; ///< current frame of the playing clip
    /* 0x69A */ byte pad_69A[2];
    /* 0x69C */ s16  field_69C; ///< dwell counter, cleared on state 0 entry
    /* 0x69E */ s16  field_69E; ///< dwell counter, cleared on state 0 entry
    /* 0x6A0 */ u16  field_6A0; ///< sound flags; bit 5/4 gate the two cues
                                /// Current yaw, walked toward `field_6A4` by
                                /// `func_actor_105700_80133364`, using `field_69E` as the per-frame step.
    /* 0x6A2 */ s16  field_6A2;
    /* 0x6A4 */ s16  field_6A4; ///< yaw the actor wants to face
    /* 0x6A6 */ s16  field_6A6; ///< parked animation for the state-F0 path
    /* 0x6A8 */ s16  field_6A8; ///< state-machine step
    /* 0x6AA */ s16  field_6AA; ///< animation the state-0 branch picks
    /* 0x6AC */ byte pad_6AC[2];
    /* 0x6AE */ s16  field_6AE; ///< state-0 frame budget
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2; ///< non-zero forces the state-F0 path
    /* 0x6B4 */ byte pad_6B4[0x1E];
    /// Spawn state driven by `func_actor_105700_80137130`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    /* 0x6D2 */ s16  field_6D2;
    /* 0x6D4 */ byte pad_6D4[2];
    /* 0x6D6 */ s16  field_6D6; ///< animation index, used as a table row
    /* 0x6D8 */ byte pad_6D8[8];
    /// State-1 branch selector: zero picks the short dwell and animation 2,
    /// non-zero the long dwell and animation 0x14.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor105700Work;
STATIC_ASSERT_SIZEOF(Actor105700Work, 0x6E4);

/// Spawn/context block behind `Actor105700.field_20`; `field_8` is the
/// halfword the sound id takes its room/channel bits from.
typedef struct Actor105700Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor105700Ctx;

/// Model object behind `Actor105700.field_2C`; `field_8` is the root
/// `GsCOORDINATE2` the pan/depth helpers read (as `GpObj38`, whose `field_24`
/// is that coordinate's `workm`). `field_C` is the halfword
/// `func_actor_105700_80137130` mirrors from the owner's object.
typedef struct Actor105700Obj {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ u16            field_C;
} Actor105700Obj;

typedef struct Actor105700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor105700Work* field_1C;
    /* 0x20 */ Actor105700Ctx*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor105700Obj*  field_2C;
} Actor105700;

/// 0x14-byte placement descriptor in the overlay's `.data`, handed to
/// `Gp_PackPair` as the source of the body objects' `GpObj.field_18`.
/// `field_E` is the variant flag `func_actor_105700_80134FDC` latches into its
/// work block: it is 1 (the table's own value is 2) when the actor is placed
/// normally, and anything else puts the body in the other pose.
typedef struct Actor105700PlaceSrc {
    /* 0x00 */ GpU16Pair pair;
    /* 0x04 */ u16       field_4;
    /* 0x06 */ u16       field_6;
    /* 0x08 */ u16       field_8;
    /* 0x0A */ u16       field_A;
    /* 0x0C */ u16       field_C;
    /* 0x0E */ u16       field_E;
    /* 0x10 */ u16       field_10;
    /* 0x12 */ u16       field_12;
} Actor105700PlaceSrc;
STATIC_ASSERT_SIZEOF(Actor105700PlaceSrc, 0x14);

/// 0x38-byte scratch carved off `G_SCRATCH_HEAD` by
/// `func_actor_105700_80134FDC`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct Actor105700PlaceScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ VECTOR  pos;
    /* 0x18 */ MATRIX  mtx;
} Actor105700PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor105700PlaceScratch, 0x38);

/// 0xF0-byte body block `func_actor_105700_80134FDC` parks at `Task::idMap`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `GpObj` bodies collide against `rec60`
/// (shared by the first two) and, through the `GpActorD4Rec` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct Actor105700FxWork {
    /* 0x00 */ MATRIX       colorMtx;
    /* 0x20 */ MATRIX       lightMtx;
    /* 0x40 */ GpObj        obj40;
    /* 0x60 */ GpRec18      rec60[1];
    /* 0x78 */ GpObj        obj78;
    /* 0x98 */ GpObj        obj98;
    /* 0xB8 */ GpActorD4Rec d4rec;
    /* 0xD0 */ GpRec18      recD0[1];
    /* 0xE8 */ byte         pad_E8[6];
    /* 0xEE */ s16          field_EE;
} Actor105700FxWork;
STATIC_ASSERT_SIZEOF(Actor105700FxWork, 0xF0);

void func_actor_105700_801336FC(Actor105700* arg0);

#endif
