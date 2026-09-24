#ifndef ACTOR_444000_H
#define ACTOR_444000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// One of the nine back-to-back collision groups in `Actor444000Work` at
/// 0x7F4. `obj` is the `GpObj` the gameplay collision list carries and `recs`
/// is the `GpRec18` table it fills in for that part, which is why the stride is
/// 0x98: `func_actor_444000_8013AFF8` hands `func_8010C980` the pair
/// (`&hits[i].obj`, `hits[i].recs`) for each group, and that helper is what
/// spells the boundary out. `obj.field_8` is the part's own coordinate -- what
/// `func_actor_444000_80134688` spawns the hit effect on.
typedef struct Actor444000HitGroup {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 recs[5];
} Actor444000HitGroup;
STATIC_ASSERT_SIZEOF(Actor444000HitGroup, 0x98);

/// `GsCOORDINATE2` with the leading words of its rotation matrix named, so the
/// identity can be written with the aligned word stores GCC 2.8.1 emits -- the
/// same shape as `Actor403100Matrix`, widened to cover the whole coordinate
/// because the descent state builds the identity in a local `GsCOORDINATE2`.
typedef union Actor444000DropCoord {
    GsCOORDINATE2 c;
    struct {
        /* 0x00 */ s32 flg;
        /* 0x04 */ s32 m00_m01;
        /* 0x08 */ s32 m02_m10;
        /* 0x0C */ s32 m11_m12;
        /* 0x10 */ s32 m20_m21;
        /* 0x14 */ s16 m22;
    } ident;
} Actor444000DropCoord;
STATIC_ASSERT_SIZEOF(Actor444000DropCoord, 0x50);

/// Per-actor work block for the enemy task `D_actor_444000_80161878` points
/// at, reached through the `Task::work` slot (0x1C) rather than being a
/// `TaskIdMap` here.
///
/// `func_actor_444000_8013AFF8` allocates it with `memCalloc(0xF24, 0)` and
/// stores the result in that slot, so the size below is the allocation rather
/// than a guess. `field_0` is the leading state word, which
/// `func_actor_444000_80143D7C` reads with `lhu` and range-checks and the
/// 0x7D9 message handler `func_actor_444000_80143F38` clears;
/// `func_actor_444000_80143490` writes the byte at 0xEAC. The remaining named
/// fields are the block `func_actor_444000_801435CC` re-arms when `field_4` is
/// set. Fill in the padding as the remaining functions are matched.
///
/// 0xC..0x784 is six back-to-back animation blocks, each a `GpAnimCtx` followed
/// by its own `GpAnimSlot[N]` and an N-entry 0x10-byte table -- the three
/// argument groups `func_actor_444000_8013AFF8` hands to `func_800B3F84`, which
/// is what anchors every offset here. They pair up (0/1, 2/3, 4/5) with eight
/// slots in the first pair and four in the others;
/// `func_actor_444000_80133F64` seeds the even member's slots while resetting
/// the odd member's.
typedef struct Actor444000Work {
    /* 0x000 */ u16        field_0; // state index
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4; // reset request: non-zero makes func_actor_444000_801435CC re-arm the block
    /* 0x006 */ s16        field_6; // sub-state counter, cleared by that reset and compared against 0xA
    /* 0x008 */ byte       pad_8[0x4];
    /* 0x00C */ GpAnimCtx  anim0;
    /* 0x020 */ GpAnimSlot slots0[8];
    /* 0x160 */ byte       aux0[0x80]; // GpAnimCtx.poses, one 0x10-byte record per slot
    /* 0x1E0 */ GpAnimCtx  anim1;
    /* 0x1F4 */ GpAnimSlot slots1[8];
    /* 0x334 */ byte       aux1[0x80];
    /* 0x3B4 */ GpAnimCtx  anim2;
    /* 0x3C8 */ GpAnimSlot slots2[4];
    /* 0x468 */ byte       aux2[0x40];
    /* 0x4A8 */ GpAnimCtx  anim3;
    /* 0x4BC */ GpAnimSlot slots3[4];
    /* 0x55C */ byte       aux3[0x40];
    /* 0x59C */ GpAnimCtx  anim4;
    /* 0x5B0 */ GpAnimSlot slots4[4];
    /* 0x650 */ byte       aux4[0x40];
    /* 0x690 */ GpAnimCtx  anim5;
    /* 0x6A4 */ GpAnimSlot slots5[4];
    /* 0x744 */ byte       aux5[0x40];
    /// Per-part yaw the escort model is being driven to, one entry per part of
    /// the fifth escort's model, and the angle each part is currently at.
    /// `func_actor_444000_80133010` picks the targets from `field_7A4` and the
    /// fight's progress counter, then walks every `field_794` toward its
    /// `field_784` by at most `field_7A6` a call.
    /* 0x784 */ s16  field_784[7];
    /* 0x792 */ byte pad_792[0x2];
    /* 0x794 */ s16  field_794[7];
    /* 0x7A2 */ byte pad_7A2[0x2];
    /* 0x7A4 */ s16  field_7A4; // escort pose index, switched on by func_actor_444000_80133010
    /* 0x7A6 */ s16  field_7A6; // most a field_794 entry may move in one call
                                /// The masked `slots0[3].field_2` frame the escort-order tick
                                /// (`func_actor_444000_8013EC84`) last saw, so each of its one-shot cues
                                /// only fires on the step the animation first reaches that frame.
    /* 0x7A8 */ s32 field_7A8;
    /// The masked `slots0[1]` / `slots0[2]` frame the arena tick
    /// (`func_actor_444000_8013FB74`) last saw, so its two one-shot cues only fire
    /// on the step the animation reaches their frame. Same role as `field_7D8`,
    /// which the other ticks use.
    /* 0x7AC */ s32 field_7AC;
    /* 0x7B0 */ s8  field_7B0;
    /* 0x7B1 */ s8  field_7B1; // set while the blended tick path runs
                               /// Animation id currently playing; `func_actor_444000_80134040` latches
                               /// `field_7B3` here once it has reseeded every slot.
    /* 0x7B2 */ s8   field_7B2;
    /* 0x7B3 */ s8   field_7B3;
    /* 0x7B4 */ u16  field_7B4; // frames since the block was re-armed
    /* 0x7B6 */ s16  field_7B6;
    /* 0x7B8 */ s16  field_7B8;
    /* 0x7BA */ s16  field_7BA;
    /* 0x7BC */ s16  field_7BC; // animation id the slot resets seed from
    /* 0x7BE */ s16  field_7BE; // GpAnimSlot.rate the resets seed with
    /* 0x7C0 */ s16  field_7C0;
    /* 0x7C2 */ byte pad_7C2[0x2];
    /* 0x7C4 */ s16  field_7C4;
    /* 0x7C6 */ byte pad_7C6[0x2];
    /* 0x7C8 */ s16  field_7C8; // yaw the drive step walks toward its target, clamped to +/-0x200 per call
                                /// Frames since the arena tick last (re)sent the player its message 0x3FF
                                /// animation; the retries in `func_actor_444000_8013FB74` are bounded by it.
    /* 0x7CA */ u16  field_7CA;
    /* 0x7CC */ byte pad_7CC[0x4];
    /* 0x7D0 */ byte field_7D0[0x8]; // start of a 0x20-byte run zeroed whenever the block is re-armed
                                     /// The masked `slots0[2].field_2` value the 0x1C-arrival check in
                                     /// `func_actor_444000_80140E28` last saw, so the script spawn only fires on
                                     /// the step the animation reaches that frame rather than every step after.
    /* 0x7D8 */ s32  field_7D8;
    /* 0x7DC */ byte pad_7DC[0x14];
    /* 0x7F0 */ byte pad_7F0[0x3];
    /* 0x7F3 */ u8   field_7F3;
    /// One collision group per body part the boss can be struck on: the
    /// coordinate a landed hit spawns its effect at, followed by that part's
    /// own `GpRec18` table. `func_actor_444000_8013AFF8` publishes
    /// `hits[0].recs` as `GpEnemy::recs`, and the hit handlers
    /// (`func_actor_444000_8013C060` for group 0,
    /// `func_actor_444000_8013C4B0` for groups 1 and 2, ...) each scan five
    /// records of their own group. The count is what the used multiples of
    /// 0x98 bound, not a figure read out of the game.
    /* 0x7F4 */ Actor444000HitGroup hits[9];
    /// The tenth collision object, the one `func_actor_444000_8013AFF8` links by
    /// hand rather than through `func_8010C980`: a kind-3 `GpObj` whose `ctx.d4rec`
    /// is `d4rec`, whose own `recs` points at the `GpRec18` table
    /// `recs2`. The arena tick raises `obj.flags` bit 0x8000 while the swipe is
    /// live and clears it otherwise.
    /* 0xD4C */ GpObj        obj;
    /* 0xD6C */ GpActorD4Rec d4rec;
    /* 0xD84 */ GpRec18      recs2[5];
    /// The light and colour matrices the spawn state points the host model and
    /// every escort model at (`TmdObject::lightMtx` / `field_20`).
    /* 0xDFC */ MATRIX lightMtx;
    /* 0xE1C */ MATRIX colorMtx;
    /// Free coordinate the arena tick rebuilds from `field_7C8` every step and
    /// pushes through `Gp_UpdateCoord`.
    /* 0xE3C */ Actor444000DropCoord field_E3C;
    /* 0xE8C */ s16                  field_E8C; // Gp_GetIdParam2 of the hit group 0 took
    /* 0xE8E */ s16                  field_E8E; // Gp_GetIdParam2 of the hit groups 3, 4 and 5 took
    /* 0xE90 */ s16                  field_E90; // Gp_GetIdParam2 of the hit groups 6, 7 and 8 took
    /* 0xE92 */ s16                  field_E92; // Gp_GetIdParam2 of the hit groups 1 and 2 took
                                                /// Yaw the arena tick walks toward `field_E96` in steps of 0x32, snapping
                                                /// once the two are within 0x33 of each other. `field_E96` is the target
                                                /// the state ladder picks each tick and `field_E98` the companion drop the
                                                /// shared floor-marker helper takes.
    /* 0xE94 */ s16  field_E94;
    /* 0xE96 */ s16  field_E96;
    /* 0xE98 */ s16  field_E98;
    /* 0xE9A */ byte pad_E9A[0x12];
    /// Screen-shake request written from outside the task by
    /// `func_actor_444000_80143490`: 1, 2 and 3 pick a shake length, anything
    /// else leaves the driver alone. `field_EAD` is the value the driver has
    /// already armed, so a change is what starts a new shake; `field_EAE` is
    /// the frames still to run and `field_EAF` the amplitude handed to
    /// `Display_ClampField126`.
    /* 0xEAC */ u8 field_EAC;
    /* 0xEAD */ u8 field_EAD;
    /* 0xEAE */ u8 field_EAE;
    /* 0xEAF */ s8 field_EAF;
    /// Message 0x3FF payload the arena tick sends the player, pointed at
    /// `D_actor_444000_80161670`.
    /* 0xEB0 */ GpAnimArg anim;
    /// The three payload bytes of the last 0x7DB message
    /// `func_actor_444000_8013ACD0` accepted, kept whether or not the id half
    /// selected one of its cases.
    /* 0xEC4 */ u8   field_EC4;
    /* 0xEC5 */ u8   field_EC5;
    /* 0xEC6 */ u8   field_EC6;
    /* 0xEC7 */ byte pad_EC7[0x1];
    /// Set while the arena tick has an animation installed on the player;
    /// `field_ECA` is the reply message 0x3F9 gave when it asked for the hold, so
    /// the two differing is what makes the tick re-send the animation.
    /* 0xEC8 */ s16 field_EC8;
    /* 0xECA */ s16 field_ECA;
    /// The seven escorts `func_actor_444000_8013AFF8` spawns with
    /// `Gp_SpawnEnemyFromTable`; the resets walk them to push the host's
    /// `TmdObject::flags` onto each escort's own model object.
    /* 0xECC */ GpEnemy* field_ECC[7];
    /// Two nearby-enemy slots, each dropped once its HP runs out; the spawn
    /// state clears them in a loop, which is what makes them an array.
    /* 0xEE8 */ GpEnemy* field_EE8[2];
    /// The enemy `func_actor_444000_801404C0` drops into the arena every tenth
    /// sub-state step once the fight passes 0x3D.
    /* 0xEF0 */ GpEnemy* field_EF0;
    /* 0xEF4 */ s16      field_EF4;
    /* 0xEF6 */ s16      field_EF6;
    /* 0xEF8 */ s16      field_EF8;
    /* 0xEFA */ s16      field_EFA;
    /* 0xEFC */ s16      field_EFC; // the field_EFA the colour update last ran for
    /* 0xEFE */ s16      field_EFE;
    /* 0xF00 */ s16      field_F00; // pitch the head tracker walks toward its target, clamped to 0..0x500
    /* 0xF02 */ s16      field_F02; // raised with the message 0x3FF the 0x17 sub-state sends the player
    /* 0xF04 */ s16      field_F04;
    /* 0xF06 */ s16      field_F06;
    /* 0xF08 */ s16      field_F08;
    /* 0xF0A */ s16      field_F0A; // damage pool the hit handler for groups 3, 4 and 5 draws down
    /* 0xF0C */ s16      field_F0C; // damage pool the hit handler for groups 6, 7 and 8 draws down
    /* 0xF0E */ s16      field_F0E; // damage pool the hit handler for groups 1 and 2 draws down
    /* 0xF10 */ s16      field_F10; // stagger countdown: the tick spins here until it runs out
    /* 0xF12 */ s16      field_F12;
    /* 0xF14 */ s16      field_F14; // eighths of it is how many extra re-arm steps the reset runs
    /* 0xF16 */ s16      field_F16;
    /* 0xF18 */ byte     pad_F18[0x2];
    /* 0xF1A */ u8       field_F1A; // free-running counter bumped on every heal tick
    /* 0xF1B */ s8       field_F1B;
    /* 0xF1C */ s8       field_F1C; // countdown, decremented while positive
    /* 0xF1D */ s8       field_F1D;
    /* 0xF1E */ byte     pad_F1E[0x6];
} Actor444000Work;
STATIC_ASSERT_SIZEOF(Actor444000Work, 0xF24);

/// The overlay's enemy task: the same layout as `Task`, named for the two
/// slots this overlay reaches through it. `field_20` is the `GpEnemy` the
/// dispatchers already hand their handlers as `Task::spawnArg2`. Not the event task
/// `D_actor_444000_80161860`, whose `work` holds an `Actor444000EventWork`.
typedef struct Actor444000 {
    /* 0x00 */ byte             pad_0[0x18];
    /* 0x18 */ TaskFunc         exitCallback;
    /* 0x1C */ Actor444000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ void*            field_24; // Task::msgTable, the message handler table
    /* 0x28 */ byte             pad_28[0x4];
    /* 0x2C */ void*            extra;    // Task::extra, a TmdObject
    /* 0x30 */ s32              state;    // Task::state, the dispatcher index
    /* 0x34 */ s16              spawnArg1Lo;
    /* 0x36 */ s16              field_36; // the high half of Task::spawnArg1
} Actor444000;

/// Scratchpad frame (`0x10` bytes carved off the scratchpad stack) used by
/// `func_actor_444000_80134688` to hand `func_800FDB18` a hit-effect rotation
/// together with the `GpEffArg` naming the coordinate it hangs off.
typedef struct Actor444000EffScratch {
    /* 0x0 */ SVECTOR  rot; // effect rotation, chosen from the attack's param 0
    /* 0x8 */ GpEffArg eff; // `func_800FDB18`'s arg3: coordinate, 0x500, 3
} Actor444000EffScratch;
STATIC_ASSERT_SIZEOF(Actor444000EffScratch, 0x10);

#endif
