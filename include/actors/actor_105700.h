#ifndef ACTOR_105700_H
#define ACTOR_105700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/task.h"

#include <psyq/libgs.h>

/// 0x6E4-byte work block hung off `Actor105700.field_1C`, allocated by
/// `Actor05700_Fn03CC4` in both actor_105700 and actor_205700. It opens with the animation context and its
/// nineteen 0x28-byte slots, exactly like the `Actor02000Work` block of
/// `actor_102000`; the animation/state halfwords around 0x694-0x6E0 keep that
/// block's offsets and meaning.
typedef struct Actor105700Work {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /// First body object, handed to `Gp_UnlinkObj` by the teardown of
    /// `Actor05700_Fn01A58`; its `pos.vz` is the pose the state-0
    /// branch of `Actor05700_Fn01318` parks (-0xA7 or 0x109) and its
    /// `radius` the frame count parked alongside it.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /// Second body object; `pos.vz` is the pose the state-0 branch parks
    /// (0x15E) and `flags` the bits whose 0x4000 it raises.
    /* 0x4CC */ GpObj   field_4CC;
    /* 0x4EC */ GpRec18 field_4EC[5];
    /// Third body object; `flags` is the field whose bit 0x4000 the state-0
    /// branch clears.
    /* 0x564 */ GpObj   field_564;
    /* 0x584 */ GpRec18 field_584[4];
    /// Fourth body object: `key` is the object `Gp_PackPair` hands it when
    /// `field_698` first reaches the animation's 0x1C mark and `flags` the
    /// bits whose 0x8000 is raised with it and dropped at the 0x28 mark
    /// (`Actor05700_Fn023AC`).
    /* 0x5E4 */ GpObj   field_5E4;
    /* 0x604 */ GpRec18 field_604[1];
    /// Fifth body object, unlinked with the others by `Actor05700_Fn01A58`.
    /* 0x61C */ GpObj        field_61C;
    /* 0x63C */ GpActorD4Rec field_63C;
    /* 0x654 */ GpRec18      field_654[1];
    /* 0x66C */ TaskDesc*    field_66C;
    /* 0x670 */ GpEffArg     field_670;
    /* 0x678 */ s32          field_678;
    /* 0x67C */ s32          field_67C;
    /* 0x680 */ s32          field_680;
    /* 0x684 */ byte         pad_684[4];
    /// Tilt angles decayed toward zero by `Actor05700_Fn016D0`.
    /* 0x688 */ SVECTOR           field_688;
    /* 0x690 */ struct GpEffWork* field_690;
    /// Animation index selected by the state machine; 4 is the "handover"
    /// clip of `Actor05700_Fn04CC0`'s state 0.
    /* 0x694 */ s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    /* 0x696 */ s16 field_696;
    /* 0x698 */ s16 field_698; ///< current frame of the playing clip
    /* 0x69A */ s16 field_69A;
    /* 0x69C */ s16 field_69C; ///< dwell counter, cleared on state 0 entry
    /* 0x69E */ s16 field_69E; ///< dwell counter, cleared on state 0 entry
    /* 0x6A0 */ u16 field_6A0; ///< sound flags; bit 5/4 gate the two cues
                               /// Current yaw, walked toward `field_6A4` by
                               /// `Actor05700_Fn01544`, using `field_69E` as the per-frame step.
    /* 0x6A2 */ s16 field_6A2;
    /* 0x6A4 */ s16 field_6A4; ///< yaw the actor wants to face
    /* 0x6A6 */ s16 field_6A6; ///< parked animation for the state-F0 path
    /* 0x6A8 */ s16 field_6A8; ///< state-machine step
    /* 0x6AA */ s16 field_6AA; ///< animation the state-0 branch picks
    /* 0x6AC */ s16 field_6AC;
    /* 0x6AE */ s16 field_6AE; ///< state-0 frame budget
    /* 0x6B0 */ s16 field_6B0;
    /* 0x6B2 */ s16 field_6B2; ///< non-zero forces the state-F0 path
    /* 0x6B4 */ s16 field_6B4; ///< cleared once the tilt has settled
    /* 0x6B6 */ s16 field_6B6;
    /// State-0 branch selector: 1 picks the short dwell and animation 1,
    /// 2 the long dwell and animation 2.
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ s16  field_6BA;
    /* 0x6BC */ s16  field_6BC;
    /* 0x6BE */ s16  field_6BE;
    /* 0x6C0 */ s16  field_6C0;
    /* 0x6C2 */ s16  field_6C2;
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[4];
    /// Body variant select: `Actor05700_Fn01A58` drops the fifth body
    /// object for the two values 0x38 / 0x39 and hands the halfword to
    /// `Gp_ReleaseStateF0Add`.
    /* 0x6CA */ s16 field_6CA;
    /* 0x6CC */ s16 field_6CC;
    /* 0x6CE */ s16 field_6CE;
    /* 0x6D0 */ s16 field_6D0;
    /// Spawn state driven by `Actor05700_Fn05310`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    /* 0x6D2 */ s16 field_6D2;
    /// Latched on state-0 entry, cleared when the frame budget runs out.
    /* 0x6D4 */ s16  field_6D4;
    /* 0x6D6 */ s16  field_6D6; ///< animation index, used as a table row
    /* 0x6D8 */ byte pad_6D8[2];
    /* 0x6DA */ s16  field_6DA; ///< state-0 frame budget, drained by `field_69C`
    /* 0x6DC */ s16  field_6DC;
    /// State-1 step gate: 1 while the state-0 exit is still to be seen, 2
    /// once it has been.
    /* 0x6DE */ s16 field_6DE;
    /// State-1 branch selector: zero picks the short dwell and animation 2,
    /// non-zero the long dwell and animation 0x14.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor105700Work;
STATIC_ASSERT_SIZEOF(Actor105700Work, 0x6E4);

/// Spawn/context block behind `Actor105700.field_20`; `field_8` is the
/// halfword the sound id takes its room/channel bits from.
typedef struct Actor105700Ctx {
    /* 0x00 */ byte         pad_0[8];
    /* 0x08 */ u16          field_8;
    /* 0x0A */ byte         pad_A[0x32];
    /* 0x3C */ GpAreaPlace* field_3C;
    /// Positive while the actor keeps dwelling after an approach cycle.
    /* 0x40 */ s16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /// Cleared by `Actor05700_Fn01318` on state-0 entry.
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[3];
} Actor105700Ctx;

/// Model object behind `Actor105700.field_2C`; `field_8` is the root
/// `GsCOORDINATE2` whose world matrix the pan/depth helpers read. `field_C` is
/// the halfword
/// `Actor05700_Fn05310` mirrors from the owner's object.
typedef struct Actor105700Obj {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ u16            field_C;
} Actor105700Obj;

/// The actor's own task, viewed through the fields its handlers read: the
/// work block, the spawn context, the model object and the handler state.
typedef struct Actor105700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor105700Work* field_1C;
    /* 0x20 */ Actor105700Ctx*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor105700Obj*  field_2C;
    /// State the actor's handler chain advances to when the approach cycle
    /// ends: 2 hands over to the next handler.
    /* 0x30 */ s32 field_30;
} Actor105700;

#endif
