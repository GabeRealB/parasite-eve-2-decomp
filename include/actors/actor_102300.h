#ifndef ACTOR_102300_H
#define ACTOR_102300_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Per-instance work block this overlay's task holds in the 0x1C slot of
/// `Actor102300`, the same shape the other enemy overlays give theirs:
/// `field_694` is the current animation id, `field_698` the frame counter the
/// state handlers compare against the per-animation start frame table
/// `D_actor_102300_80135D64`, and `field_6A8` the state the frame dispatcher
/// switches on.
typedef struct Actor102300Work {
    /// Animation context `func_800B3F84` fills in, followed by the nineteen
    /// slots and the pose buffer it is handed.
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       poses[0x130];
    /// Light and colour matrices the model object is pointed at, in place of
    /// the defaults `Tmd_SetupDraw` would otherwise load.
    /* 0x43C */ MATRIX field_43C;
    /* 0x45C */ MATRIX field_45C;
    /// The four list nodes the spawn handler links, each with its own
    /// `GpRec18` table. `field_47C` goes on list 3 through the bounding-box
    /// record at `field_49C`; the other three point straight at their tables.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /* 0x4CC */ GpObj        field_4CC;
    /* 0x4EC */ GpRec18      field_4EC[5];
    /* 0x564 */ GpObj        field_564;
    /* 0x584 */ GpRec18      field_584[4];
    /// Collision/proximity list node, the slot the lunge raises bit 0x8000 of
    /// and parks its `Gp_PackPair` entry in.
    /* 0x5E4 */ GpObj   field_5E4;
    /* 0x604 */ GpRec18 field_604[1];
    /* 0x61C */ byte    pad_61C[0x50];
    /// The spawn table this overlay's enemies come from, kept for the state
    /// handlers to respawn through.
    /* 0x66C */ TaskDesc* field_66C;
    /* 0x670 */ GpEffArg  field_670;
    /// Root coordinate the collision tick restores when a push-out resolves
    /// back to the recorded spawn position.
    /* 0x678 */ s32  field_678;
    /* 0x67C */ s32  field_67C;
    /* 0x680 */ s32  field_680;
    /* 0x684 */ byte pad_684[4];
    /// Hit tilt, seeded from the LCG when a hit lands with no reaction state.
    /* 0x688 */ SVECTOR           field_688;
    /* 0x690 */ struct GpEffWork* field_690;
    /* 0x694 */ s16               field_694;
    /* 0x696 */ byte              pad_696[2];
    /* 0x698 */ s16               field_698;
    /// Hit cooldown in frames, seeded from `Gp_GetIdParam2`; while non-zero the
    /// weapon-hit records are ignored.
    /* 0x69A */ s16  field_69A;
    /* 0x69C */ s16  field_69C;
    /* 0x69E */ s16  field_69E;
    /* 0x6A0 */ byte pad_6A0[2];
    /// Facing angle the lunge steers `field_6A4` towards; the tick compares the
    /// two and only commits once they are within 0x100.
    /* 0x6A2 */ u16 field_6A2;
    /* 0x6A4 */ s16 field_6A4;
    /* 0x6A6 */ s16 field_6A6;
    /* 0x6A8 */ s16 field_6A8;
    /// 1 when the attacker is in front of this enemy, 0 behind; taken from the
    /// sign of the hit vector against the root's forward axis.
    /* 0x6AA */ s16 field_6AA;
    /// Awake variant this enemy starts in, taken from bit 0 of the placement
    /// record's `mode`.
    /* 0x6AC */ s16  field_6AC;
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /// Raised by the collision node once the lunge connects; the state
    /// handlers check it to break out of the approach cycle.
    /* 0x6B2 */ s16 field_6B2;
    /// Raised with `field_688` when a hit lands and no reaction animation is
    /// selected; the tilt decay clears it.
    /* 0x6B4 */ s16 field_6B4;
    /// Frames spent in the current lunge cycle; at 0x4C the tick gives up and
    /// falls back to animation 8.
    /* 0x6B6 */ s16 field_6B6;
    /// Non-zero selects the second reaction family (animations 4/6/0xC/0xE).
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ byte pad_6BA[6];
    /// Shift count for the idle-to-lunge draw: each cycle widens the LCG mask
    /// by one bit, so the enemy grows less likely to lunge again.
    /* 0x6C0 */ s16 field_6C0;
    /* 0x6C2 */ s16 field_6C2;
    /// 1 or 2, picked from bit 16 of the next LCG draw.
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[4];
    /* 0x6CA */ s16  field_6CA;
    /* 0x6CC */ s16  field_6CC;
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ s16  field_6D0;
    /* 0x6D2 */ s16  field_6D2;
    /// Non-zero suppresses the two reaction animations that would otherwise
    /// interrupt the one already playing.
    /* 0x6D4 */ s16 field_6D4;
    /// Sector id of the enemy's voice stream, looked up per room from
    /// `D_actor_102300_80147AA0` and queued with `CdCmd_Enqueue(0x21, ...)`.
    /* 0x6D6 */ s16  field_6D6;
    /* 0x6D8 */ byte pad_6D8[2];
    /// Dwell budget in thousandths, scaled by the placement record's `variant`.
    /* 0x6DA */ s16  field_6DA;
    /* 0x6DC */ s16  field_6DC;
    /* 0x6DE */ byte pad_6DE[2];
    /// Non-zero suppresses the crit and knock-back reactions.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor102300Work;
STATIC_ASSERT_SIZEOF(Actor102300Work, 0x6E4);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object whose `field_8`
/// is the per-part coordinate array. Same shape as the other actor overlays'
/// contexts.
typedef struct Actor102300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor102300Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              field_30;
} Actor102300;

/// 0x40-byte scratch the hit tick claims from `G_SCRATCH_HEAD`: the collision
/// delta `func_800E0C10` fills in, the normalised push-out derived from it, and
/// the two points `Actor00300_Fn04B14` tests the player's sight line over.
typedef struct Actor102300HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         normal;
    /* 0x20 */ VECTOR         push;
    /* 0x30 */ SVECTOR        effOfs;
    /* 0x38 */ SVECTOR        target;
} Actor102300HitScratch;
STATIC_ASSERT_SIZEOF(Actor102300HitScratch, 0x40);

#endif // ACTOR_102300_H
