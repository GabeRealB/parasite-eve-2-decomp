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
    /* 0x160 */ byte       aux0[0x80]; // GpAnimCtx::field_8, one 0x10-byte record per slot
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
    /// `hits[0].recs` as `GpEnemy::field_54`, and the hit handlers
    /// (`func_actor_444000_8013C060` for group 0,
    /// `func_actor_444000_8013C4B0` for groups 1 and 2, ...) each scan five
    /// records of their own group. The count is what the used multiples of
    /// 0x98 bound, not a figure read out of the game.
    /* 0x7F4 */ Actor444000HitGroup hits[9];
    /// The tenth collision object, the one `func_actor_444000_8013AFF8` links by
    /// hand rather than through `func_8010C980`: a kind-3 `GpObj` whose `ctx.d4rec`
    /// is `d4rec`, whose own `field_14` points at the `GpRec18` table
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

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_444000_80161860` points at, which is a different and much smaller
/// block than the enemy's `Actor444000Work` above.
///
/// `func_actor_444000_80132358` allocates it with `memCalloc(0x34, 0)`,
/// `Mem_Set`s 0x34 bytes and parks it in that task's `Task::work` slot, so
/// the size is anchored; the same function stores the `Game_GetPtrSlot(3)`
/// task in `field_20` and publishes its owning task in
/// `D_actor_444000_80161860`. `field_20` is the target of every
/// `Gp_DispatchMsg` the leaf helpers send, and they null-check it first
/// (`func_actor_444000_801321FC`). `field_2C` is the action index
/// `func_actor_444000_80132054` switches on, with `field_2E` the sub-state
/// counter reset alongside it. `field_2A` is a one-shot flag guarding the sound
/// cue `func_actor_444000_80132608` enqueues.
typedef struct Actor444000EventWork {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ Task* field_20; // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x24 */ Task* field_24; // subordinate task, killed and cleared by func_actor_444000_80132694
                               /// Area-record id published to `D_8007216C` on every enter/re-enter. The
                               /// spawn state writes it as a halfword, clearing the byte at 0x29 with it,
                               /// while every reader takes the low byte, so both views are named.
    /* 0x28 */ union {
        u8  b;
        s16 h;
    } field_28;
    /* 0x2A */ u16  field_2A; // one-shot flag: set once func_actor_444000_80132608 has played its cue
    /* 0x2C */ u16  field_2C; // action index, switched on by func_actor_444000_80132054
    /* 0x2E */ s16  field_2E; // cleared whenever field_2C is set
    /* 0x30 */ u16  field_30; // one-shot flag: set once func_actor_444000_80132778 has armed the death sequence
    /* 0x32 */ byte pad_32[0x2];
} Actor444000EventWork;
STATIC_ASSERT_SIZEOF(Actor444000EventWork, 0x34);

/// Payload `func_actor_444000_801326DC` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `AcropolisBridgeMsg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor444000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor444000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor444000Msg7DA, 0x4);

/// The same four bytes seen from the receiving end, in
/// `func_actor_444000_8013ACD0`: the handler copies the three payload bytes
/// into `Actor444000Work` one at a time, but tests the sender id and the
/// action selector as the two halfwords they are, so both views are named.
typedef union Actor444000Msg7DB {
    u8 b[4];
    struct {
        /* 0x0 */ u16 id;
        /* 0x2 */ u16 action;
    } h;
} Actor444000Msg7DB;
STATIC_ASSERT_SIZEOF(Actor444000Msg7DB, 0x4);

/// Reply buffer `func_actor_444000_80138490` passes with message 0x3F8, held
/// at `D_actor_444000_80161898`; only `field_14` is seeded (to 0x28) before
/// the query. The same shape as `Actor400600Msg3F8`.
typedef struct Actor444000Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
    /* 0x18 */ byte pad_18[0x8];
} Actor444000Msg3F8;
STATIC_ASSERT_SIZEOF(Actor444000Msg3F8, 0x20);

/// Scratchpad frame `func_actor_444000_8013ACD0` carves off `G_SCRATCH_HEAD` to
/// rebuild the host's root coordinate: `Gfx_RotMatrixY` writes `m`, `scale`
/// is the 1.0 / 0.0 / 1.0 vector `ScaleMatrix` applies to it, and `angle` is
/// the `ratan2` yaw the rotation was built from.
typedef struct Actor444000RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ byte   pad_32[0x2];
} Actor444000RotScratch;
STATIC_ASSERT_SIZEOF(Actor444000RotScratch, 0x34);

/// Word-wise view of the `MATRIX` in `Actor444000RunScratch`, used only to
/// splat an identity rotation: five aligned stores instead of nine halfword
/// ones, each word holding two adjacent `m[][]` entries. Same shape as
/// `ActorsShared8016bd98MatWords`.
typedef struct Actor444000RunMatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor444000RunMatWords;

typedef union Actor444000RunMat {
    MATRIX                 mat;
    Actor444000RunMatWords ident;
} Actor444000RunMat;
STATIC_ASSERT_SIZEOF(Actor444000RunMat, 0x20);

/// Scratchpad frame `func_actor_444000_8013482C` carves off `G_SCRATCH_HEAD`
/// for the run-out / turn / run-back pass. `dir` is first the offset from the
/// model to the player, whose yaw against the model's own facing becomes
/// `Actor444000Work::field_7C4`, and later the normalised, GPF-scaled step the
/// turn adds to the coordinate; `m` is the working copy of the model's root
/// coordinate and `angle` the yaw `Gfx_RotMatrixY` rebuilds it from.
typedef struct Actor444000RunScratch {
    /* 0x00 */ SVECTOR           dir;
    /* 0x08 */ Actor444000RunMat m;
    /* 0x28 */ s16               pad_28;
    /* 0x2A */ s16               angle;
} Actor444000RunScratch;
STATIC_ASSERT_SIZEOF(Actor444000RunScratch, 0x2C);

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

/// Work block of the overlay's *other* enemy, the one dispatched through
/// `D_actor_444000_80131F30` rather than `D_actor_444000_80131E90`.
/// `func_actor_444000_8013A1C4` allocates it with `memCalloc(0xA0, 0)` and
/// parks it in that task's `Task::work` slot, so the size is anchored rather
/// than guessed. The two matrices are this instance's own light and colour
/// matrices: the spawn state points the model object's `field_1C` / `field_20`
/// at them so it lights independently of the shared defaults. `field_9C` is the
/// spin countdown -- the spawn state seeds it from `Task::spawnArg1` and the
/// tick `func_actor_444000_8013A3AC` decrements it, yawing the model by +0x40
/// and -0x3C on alternate steps of its low two bits.
typedef struct Actor444000SpinnerWork {
    /* 0x00 */ byte   pad_0[0x50];
    /* 0x50 */ MATRIX colorMtx;
    /* 0x70 */ MATRIX lightMtx;
    /* 0x90 */ s16    field_90; // set when the dispatcher sees the state change, cleared when it has not
    /* 0x92 */ byte   pad_92[0x2];
    /* 0x94 */ s16    field_94; // the state the dispatcher last ran, so it can spot the change
    /* 0x96 */ s16    field_96;
    /* 0x98 */ s16    field_98;
    /* 0x9A */ byte   pad_9A[0x2];
    /* 0x9C */ u8     spin; // countdown, also the phase the tick yaws on
    /* 0x9D */ byte   pad_9D[0x3];
} Actor444000SpinnerWork;
STATIC_ASSERT_SIZEOF(Actor444000SpinnerWork, 0xA0);

/// That enemy's task, the same `Task` layout as `Actor444000` above but with
/// the smaller work block in the `work` slot.
typedef struct Actor444000Spinner {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ Actor444000SpinnerWork* field_1C;
    /* 0x20 */ GpEnemy*                spawnArg2;
    /* 0x24 */ void*                   field_24;
    /* 0x28 */ byte                    pad_28[0x4];
    /* 0x2C */ TmdObject*              extra;     // Task::extra
    /* 0x30 */ s32                     state;
    /* 0x34 */ s32                     spawnArg1; // spin preset selector
} Actor444000Spinner;

/// Work block of the enemy dispatched through `D_actor_444000_80131F1C` -- the
/// one that rises out of view and then slams back down onto the floor.
/// `func_actor_444000_80139594` allocates it with `memCalloc(0x1C0, 0)` and
/// parks it in that task's `Task::work` slot, so the size is anchored rather
/// than guessed.
///
/// `target` is the landing point the spawn state picks from the player's
/// distance; `coord` is the coordinate the shadow marker is drawn at, kept on
/// the floor (`coord.t[1] == 0`) directly under the model and refreshed with
/// `Gp_UpdateCoord` every step. `obj` is the collision node the spawn state
/// hands to `Gp_LinkObj` with `field_8` pointing at `coord` and `field_C` at
/// the single-entry `rec` table that follows it. `timer` is the step counter:
/// the descent state grows the shadow quad with it, only starts applying the
/// fall after 0x14 steps, and zeroes it again on touchdown.
typedef struct Actor444000DropWork {
    /* 0x000 */ VECTOR3       target;
    /* 0x00C */ byte          pad_C[0x4];
    /* 0x010 */ GsCOORDINATE2 coord;
    /* 0x060 */ byte          pad_60[0x50];
    /* 0x0B0 */ GpObj         obj;
    /* 0x0D0 */ byte          pad_D0[0x20];
    /* 0x0F0 */ GpRec18       rec;
    /* 0x108 */ byte          pad_108[0x88];
    /* 0x190 */ GpEffWork*    eff; // Gp_SpawnEff result, reparented onto the task
    /* 0x194 */ byte          pad_194[0x16];
    /* 0x1AA */ s16           field_1AA;
    /* 0x1AC */ u16           timer;
    /* 0x1AE */ s16           field_1AE; // per-step fall/rise bias, an LCG bit (0 or 8)
    /* 0x1B0 */ byte          pad_1B0[0x10];
} Actor444000DropWork;
STATIC_ASSERT_SIZEOF(Actor444000DropWork, 0x1C0);

/// A `MATRIX` plus the word-wise view `func_actor_444000_80140BBC` uses to
/// splat an identity rotation into the shared coordinate
/// `D_actor_444000_801618B8`: five aligned stores instead of nine halfword
/// ones, each word holding two adjacent `m[][]` entries. The same shape as
/// `Actor403100Matrix`; the rotation half of `Actor444000DropCoord` seen
/// through a pointer, which is what keeps those stores in source order.
typedef union Actor444000Matrix {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor444000Matrix;
STATIC_ASSERT_SIZEOF(Actor444000Matrix, 0x20);

/// That enemy's task: the same `Task` layout, named for the slots the
/// `D_actor_444000_80131F1C` states reach through it.
typedef struct Actor444000Drop {
    /* 0x00 */ byte                 pad_0[0x8];
    /* 0x08 */ Task*                parent;
    /* 0x0C */ byte                 pad_C[0x10];
    /* 0x1C */ Actor444000DropWork* field_1C;
    /* 0x20 */ byte                 pad_20[0xC];
    /* 0x2C */ TmdObject*           extra;
    /* 0x30 */ s32                  state;
    /* 0x34 */ s32                  spawnArg1;
} Actor444000Drop;

/// Work block of the enemy dispatched through `D_actor_444000_80131EA8`, the
/// one that seizes the player: its states install a scripted animation on the
/// slot-3 task with message 0x3FF and cancel it again with message 0x3F1.
/// `func_actor_444000_80137D4C` allocates it with `memCalloc(0x1C0, 0)` and
/// parks it in that task's `Task::work` slot, so the size is anchored rather
/// than guessed.
///
/// `anim` is the `GpAnimArg` handed to the player as message 0x3FF's payload --
/// sent by address out of the work block rather than from a local, which is why
/// it lives here. `field_1B2` is the one-shot flag saying that animation is
/// currently installed, so only the state that set it sends the cancel;
/// `field_1AC` is the step counter the hold states compare against their own
/// limit, and `field_1A8` gates the take-over in `func_actor_444000_801389EC`.
/// Fill in the padding as the remaining states are matched.
typedef struct Actor444000GrabWork {
    /// Per-step world delta the hold states add to the model's coordinate,
    /// a fifteenth at a time; only x and z are read.
    /* 0x000 */ VECTOR3 vel;
    /* 0x00C */ byte    pad_C[0x54];
    /// The work block's own coordinate, parented to `gGfxViewCoord` and kept
    /// tracking the model's world position so the ground marker below it can
    /// be drawn from `coord.workm.t`.
    /* 0x060 */ GsCOORDINATE2 coord;
    /// The two `GpObj` display nodes the teardown path hands back to
    /// `Gp_UnlinkObj`; the hold state ORs `0x8000` into the first's `flags`
    /// and `0x4000` into the second's once the model has passed its apex.
    /* 0x0B0 */ GpObj obj0;
    /* 0x0D0 */ GpObj obj1;
    /// The two collision-record tables `Gp_ClearRec18Occupied` wipes each step;
    /// `rec1` is also the table `func_actor_444000_80132B14` collides against.
    /* 0x0F0 */ GpRec18 rec0;
    /* 0x108 */ GpRec18 rec1;
    /* 0x120 */ byte    pad_120[0x30];
    /// The colour and light matrices the task hands to its `TmdObject`
    /// (`field_20` and `field_1C`); `colorMtx.t[1]` / `t[2]` are the two shake
    /// terms the bounce state damps each step.
    /* 0x150 */ MATRIX    colorMtx;
    /* 0x170 */ MATRIX    lightMtx;
    /* 0x190 */ byte      pad_190[0x4];
    /* 0x194 */ GpAnimArg anim;      // message 0x3FF payload, sent by address
    /* 0x1A8 */ s16       field_1A8; // set when the dispatcher sees the state change; gates the take-over
    /* 0x1AA */ s16       field_1AA; // bounce height added back to the model's y each step, taken as a magnitude
    /* 0x1AC */ s16       field_1AC; // step counter within the state
    /* 0x1AE */ byte      pad_1AE[0x2];
    /// Radius of the ground marker, in eighths once shifted down; re-armed to
    /// 0x400 when the state starts and grown by 0x60 a step.
    /* 0x1B0 */ u16  field_1B0;
    /* 0x1B2 */ s16  field_1B2; // one-shot flag: the player animation is installed
    /* 0x1B4 */ s16  field_1B4; // the state the dispatcher last ran, so it can spot the change
    /* 0x1B6 */ byte pad_1B6[0xA];
} Actor444000GrabWork;
STATIC_ASSERT_SIZEOF(Actor444000GrabWork, 0x1C0);

/// That enemy's task: the same `Task` layout, named for the slots the
/// `D_actor_444000_80131EA8` states reach through it.
typedef struct Actor444000Grab {
    /* 0x00 */ byte                 pad_0[0x8];
    /* 0x08 */ Task*                parent;
    /* 0x0C */ byte                 pad_C[0x10];
    /* 0x1C */ Actor444000GrabWork* field_1C;
    /* 0x20 */ GpEnemy*             spawnArg2;
    /* 0x24 */ byte                 pad_24[0x8];
    /* 0x2C */ TmdObject*           extra;
    /* 0x30 */ s32                  state;
    /* 0x34 */ s32                  spawnArg1;
} Actor444000Grab;

/// Work block of the enemy dispatched through `D_actor_444000_80131F0C` --
/// named for that table because the creature itself is not identified yet.
/// `func_actor_444000_80138B94` allocates it with `memCalloc(0x1C0, 0)` and
/// parks it in that task's `Task::work` slot, so the size is anchored rather
/// than guessed.
///
/// The two named fields are the pair the dispatcher
/// `func_actor_444000_80143A6C` keeps: `field_1B4` is the state it last ran and
/// `field_1A8` the flag it sets when that state has changed since.
typedef struct Actor444000F0CWork {
    /* 0x000 */ byte pad_0[0x1A8];
    /* 0x1A8 */ s16  field_1A8; // set when the dispatcher sees the state change, cleared when it has not
    /* 0x1AA */ byte pad_1AA[0xA];
    /* 0x1B4 */ s16  field_1B4; // the state the dispatcher last ran, so it can spot the change
    /* 0x1B6 */ byte pad_1B6[0xA];
} Actor444000F0CWork;
STATIC_ASSERT_SIZEOF(Actor444000F0CWork, 0x1C0);

/// That enemy's task: the same `Task` layout, named for the slots the
/// `D_actor_444000_80131F0C` states reach through it.
typedef struct Actor444000F0C {
    /* 0x00 */ byte                pad_0[0x1C];
    /* 0x1C */ Actor444000F0CWork* field_1C;
    /* 0x20 */ GpEnemy*            spawnArg2;
    /* 0x24 */ byte                pad_24[0x8];
    /* 0x2C */ TmdObject*          extra;
    /* 0x30 */ s32                 state;
} Actor444000F0C;

/// The head of a `Gp_PlayerAnimBlkTbl` entry as this overlay reads it: an array
/// of animation-set pointers, of which the grab state copies entry 9 onto its
/// own `D_actor_444000_80161694` table. The same shape as
/// `Actor403100AnimTable`, extended to reach that entry.
typedef struct Actor444000AnimTable {
    /* 0x00 */ GpAnimSet* sets[10];
} Actor444000AnimTable;
STATIC_ASSERT_SIZEOF(Actor444000AnimTable, 0x28);

/// Scratchpad frame (`0x10` bytes carved off the scratchpad stack) used by
/// `func_actor_444000_80134688` to hand `func_800FDB18` a hit-effect rotation
/// together with the `GpEffArg` naming the coordinate it hangs off.
typedef struct Actor444000EffScratch {
    /* 0x0 */ SVECTOR  rot; // effect rotation, chosen from the attack's param 0
    /* 0x8 */ GpEffArg eff; // `func_800FDB18`'s arg3: coordinate, 0x500, 3
} Actor444000EffScratch;
STATIC_ASSERT_SIZEOF(Actor444000EffScratch, 0x10);

/// 0x30-byte scratchpad frame the per-group hit handlers
/// (`func_actor_444000_8013C060` and its siblings) carve off `G_SCRATCH_HEAD`
/// for the one hit they take this frame. `pos` is the contact point copied out
/// of the `GpRec18`; `delta` is the player-relative offset whose length is
/// `dist`, the range `Gp_ComputeDamage` scales `damage` by. `rot` doubles as
/// `Gp_SpawnEff`'s rotation argument and, afterwards, as the workspace for the
/// contact point relative to the part's world translation, which `angle`
/// is the yaw of.
typedef struct Actor444000HitScratch {
    /* 0x00 */ VECTOR3 delta;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR rot;
    /* 0x18 */ SVECTOR pos;
    /* 0x20 */ s32     id;     // attack id of the hit that landed, 0 for none
    /* 0x24 */ u32     damage; // HP taken off the enemy
    /* 0x28 */ s32     dist;   // distance from the player, in world units
    /* 0x2C */ s16     angle;  // yaw of the contact point, wrapped to +/-0x800
    /* 0x2E */ byte    pad_2E[0x2];
} Actor444000HitScratch;
STATIC_ASSERT_SIZEOF(Actor444000HitScratch, 0x30);

/// 0xC-byte scratchpad frame `func_actor_444000_80141618` carves off
/// `G_SCRATCH_HEAD` for the escort-spawn tick: `delta` is the player-relative
/// offset the tick yaws the host by, and `i` is the escort slot the loop and
/// the 0x7DB message both index `Actor444000Work::field_EE8` with.
typedef struct Actor444000SpawnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ byte    pad_8[0x2];
    /* 0xA */ s16     i; // escort slot, 0 or 1
} Actor444000SpawnScratch;
STATIC_ASSERT_SIZEOF(Actor444000SpawnScratch, 0xC);

/// 0x4C-byte scratchpad frame `func_actor_444000_8013EC84` carves off
/// `G_SCRATCH_HEAD` for the escort-order tick. `delta` is the player-relative
/// offset in the arena plane whose length is `dist` -- under 0xB54 the player is
/// dragged back along `dir` to a fixed range -- and `pos` is the host's fifth
/// part carried into view space, which the yaw `angle` and the final message
/// 0x3E9 placement are both built from. `dir` doubles as `VectorNormalSS`'s
/// workspace throughout.
typedef struct Actor444000WarpScratch {
    /* 0x00 */ SVECTOR dir;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ MATRIX  m;
    /* 0x40 */ s32     dist;  // length of `delta`, in world units
    /* 0x44 */ byte    pad_44[0x6];
    /* 0x4A */ s16     angle; // yaw handed to the placement, wrapped to +/-0x800
} Actor444000WarpScratch;
STATIC_ASSERT_SIZEOF(Actor444000WarpScratch, 0x4C);

/// 0x4C-byte scratchpad frame `func_actor_444000_8013E058` carves off
/// `SCRATCH_SP` for the drag tick. `dir` starts as the player-relative offset in
/// the arena plane, is carried into view space, renormalised and then scaled by
/// the per-frame pull the animation frame selects; `push` is the same vector as
/// the 32-bit triple `func_80105B74` copies onto the player actor, `dist` is the
/// offset's length and `pull` / `period` are the frame-derived strength and the
/// script-spawn interval the current phase uses.
typedef struct Actor444000DragScratch {
    /* 0x00 */ VECTOR3 push;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ byte    pad_18[0x20];
    /* 0x38 */ s32     dist;   // length of `dir` before it is normalised
    /* 0x3C */ byte    pad_3C[0x8];
    /* 0x44 */ s16     pull;   // phase offset folded into the gpf scale
    /* 0x46 */ s16     i;      // escort slot being cleared, 0 or 1
    /* 0x48 */ s16     period; // frames between script spawns
    /* 0x4A */ byte    pad_4A[0x2];
} Actor444000DragScratch;
STATIC_ASSERT_SIZEOF(Actor444000DragScratch, 0x4C);

/// Position and Euler rotation payload the escort-order tick sends slot 3 as
/// message 0x3E9; `D_actor_444000_80161908` is the overlay's only instance.
typedef struct Actor444000MsgPos {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor444000MsgPos;
STATIC_ASSERT_SIZEOF(Actor444000MsgPos, 0x18);

/// 0x14-byte scratchpad frame `func_actor_444000_80132B14` carves off
/// `G_SCRATCH_HEAD`: the `GpDeltaScratch` it hands `func_800E0C10` plus the
/// "did the frame actually move" flag it returns.
typedef struct Actor444000DeltaScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor444000DeltaScratch;
STATIC_ASSERT_SIZEOF(Actor444000DeltaScratch, 0x14);

s32  func_actor_444000_80132B14(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);
void func_actor_444000_80134688(GsCOORDINATE2* coord, s32 id);
void func_actor_444000_8013441C(Actor444000* arg0);
void func_actor_444000_8013482C(Actor444000* task);
void func_actor_444000_80135448(Actor444000* arg0);
void func_actor_444000_801371E8(Task* task, s16 arg1, s16 index);
void func_actor_444000_8013AFF8(GpEnemy* enemy, Actor444000* task);
void func_actor_444000_801423C4(GpEnemy* enemy, Actor444000* task);
void func_actor_444000_801434C4(Actor444000* arg0);
void func_actor_444000_801435CC(Actor444000* arg0);
s32  func_actor_444000_80143D68(Actor444000* arg0);
s32  func_actor_444000_80143F38(Actor444000* arg0);
void func_actor_444000_80143F4C(Actor444000* arg0);

#endif
