#ifndef ACTOR_105600_H
#define ACTOR_105600_H

#include "common.h"

#include "gameplay/1BC.h"
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
    /* 0x0A */ byte pad_A[0x42];
    /// Request bits the per-frame handler acts on: bit 1 asks the approach
    /// cycle to switch to its handover animation and is cleared once taken.
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[3];
} Actor105600Ctx;

/// 0x6E4-byte animation/state work block hung off the approach-cycle task's
/// `Task::idMap`. It opens with the animation context and its nineteen
/// 0x28-byte slots, exactly like the `Actor105700Work` block of `actor_105700`
/// and the `Actor02000Work` block of `actor_102000`; the halfwords around
/// 0x694-0x6E0 keep those blocks' offsets and meaning.
typedef struct Actor105600Work {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       field_30C[0x130];
    /// Colour and light matrices the model object's `field_20` / `field_1C`
    /// are pointed at.
    /* 0x43C */ MATRIX field_43C;
    /* 0x45C */ MATRIX field_45C;
    /// The five collision/proximity list nodes the spawn handler links, each
    /// followed by the `GpRec18` table it walks. The first and last reach
    /// their tables through a `GpActorD4Rec`.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /* 0x4CC */ GpObj        field_4CC;
    /* 0x4EC */ GpRec18      field_4EC[5];
    /* 0x564 */ GpObj        field_564;
    /* 0x584 */ GpRec18      field_584[4];
    /* 0x5E4 */ GpObj        field_5E4;
    /* 0x604 */ GpRec18      field_604[1];
    /* 0x61C */ GpObj        field_61C;
    /* 0x63C */ GpActorD4Rec field_63C;
    /* 0x654 */ GpRec18      field_654[1];
    /// The spawn table the companion enemy comes from.
    /* 0x66C */ TaskDesc*      field_66C;
    /* 0x670 */ GsCOORDINATE2* field_670;
    /* 0x674 */ s16            field_674;
    /* 0x676 */ s16            field_676;
    /// World position of the root coordinate as of the previous frame, saved
    /// before the per-frame drift below is applied.
    /* 0x678 */ s32  field_678;
    /* 0x67C */ s32  field_67C;
    /* 0x680 */ s32  field_680;
    /* 0x684 */ byte pad_684[0x10];
    /// Animation index selected by the state machine.
    /* 0x694 */ s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    /* 0x696 */ s16  field_696;
    /* 0x698 */ s16  field_698; ///< current frame of the playing clip
    /* 0x69A */ byte pad_69A[2];
    /* 0x69C */ s16  field_69C; ///< forward speed, applied along the root Z axis
    /* 0x69E */ s16  field_69E; ///< non-zero runs the turn helper
    /* 0x6A0 */ byte pad_6A0[2];
    /* 0x6A2 */ s16  field_6A2; ///< yaw the root coordinate currently faces
    /* 0x6A4 */ s16  field_6A4; ///< yaw the actor wants to face
    /* 0x6A6 */ s16  field_6A6; ///< state-machine step, indexes the handler table
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ byte pad_6AA[2];
    /// Awake variant the actor was placed in (bit 0 of the placement record's
    /// `field_2`); non-zero starts it on the longer approach.
    /* 0x6AC */ s16  field_6AC;
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2; ///< non-zero forces the state-F0 path
    /* 0x6B4 */ s16  field_6B4; ///< non-zero runs the tilt helper
    /* 0x6B6 */ s16  field_6B6;
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ s16  field_6BA;
    /* 0x6BC */ s16  field_6BC;
    /* 0x6BE */ s16  field_6BE;
    /* 0x6C0 */ byte pad_6C0[0xA];
    /* 0x6CA */ s16  field_6CA;
    /* 0x6CC */ s16  field_6CC;
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ s16  field_6D0;
    /* 0x6D2 */ byte pad_6D2[4];
    /// Streaming cue id for this room, looked up in `D_actor_105600_80148298`.
    /* 0x6D6 */ s16  field_6D6;
    /* 0x6D8 */ byte pad_6D8[2];
    /// Approach budget, 1000 per unit of the placement record's byte 1.
    /* 0x6DA */ s16  field_6DA;
    /* 0x6DC */ s16  field_6DC;
    /* 0x6DE */ s16  field_6DE; ///< below 2 the actor also drifts upward
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor105600Work;
STATIC_ASSERT_SIZEOF(Actor105600Work, 0x6E4);

/// 0x10-byte per-room record reached through `GpAreaRec::field_0`, the same
/// table `Actor02000AreaRec` describes. `field_D` / `field_E` are the texture
/// page and CLUT row the spawn handler copies into the companion model object.
typedef struct Actor105600AreaRec {
    /* 0x00 */ byte pad_0[0xD];
    /* 0x0D */ u8   field_D;
    /* 0x0E */ u8   field_E;
    /* 0x0F */ byte pad_F[1];
} Actor105600AreaRec;
STATIC_ASSERT_SIZEOF(Actor105600AreaRec, 0x10);

/// Placement descriptor for this actor.
extern Actor105600PlaceSrc D_actor_105600_80147FDC;

/// Pair source the approach cycle parks at `GpEnemy::field_50`; its `field_4`
/// becomes the enemy's `field_40`.
extern GpPairSrcE D_actor_105600_80147FF0[];

/// Per-stage tables of streaming cue ids, indexed by `GameSession::field_7`
/// and then `GameSession::field_6`.
extern u16* D_actor_105600_80148298[];

/// Spawn table the approach cycle starts its companion enemy from, index 1.
extern TaskDesc D_actor_105600_801482C0[];

/// Animation stream set bound into the work block's animation context.
extern void D_actor_105600_801482E4;

/// Sound id of the burst cue, with the spawn context's room/channel bits packed
/// in.
extern s32 D_actor_105600_80148110;

#endif
