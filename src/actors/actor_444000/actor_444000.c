#include "common.h"
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80135990.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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
    /* 0x7DC */ byte pad_7DC[0x16];
    /* 0x7F2 */ s8   field_7F2;
    /* 0x7F3 */ u8   field_7F3;
    /// One collision group per body part the boss can be struck on: the
    /// coordinate a landed hit spawns its effect at, followed by that part's
    /// own `GpRec18` table. `func_actor_444000_8013AFF8` publishes
    /// `hits[0].recs` as `GpEnemy::recs`, and the hit handlers
    /// (`func_actor_444000_8013C060` for group 0,
    /// `func_actor_444000_8013C4B0` for groups 1 and 2, ...) each scan five
    /// records of their own group. The count is what the used multiples of
    /// 0x98 bound, not a figure read out of the game.
    /* 0x7F4 */ Actor403200HitGroup hits[9];
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
    /* 0xE3C */ Actor403200DropCoord field_E3C;
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

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_444000_80161860` points at, which is a different and much smaller
/// block than the enemy's `Actor444000Work` above.
///
/// `func_actor_444000_80132358` allocates it with `memCalloc(0x34, 0)`,
/// `Mem_Set`s 0x34 bytes and parks it in that task's `Task::work` slot, so
/// the size is anchored; the same function stores the `gameGetPtrSlot(3)`
/// task in `field_20` and publishes its owning task in
/// `D_actor_444000_80161860`. `field_20` is the target of every
/// `Gp_DispatchMsg` the leaf helpers send, and they null-check it first
/// (`func_actor_444000_801321FC`). `field_2C` is the action index
/// `func_actor_444000_80132054` switches on, with `field_2E` the sub-state
/// counter reset alongside it. `field_2A` is a one-shot flag guarding the sound
/// cue `func_actor_444000_80132608` enqueues.
typedef struct Actor444000EventWork {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ Task* field_20; // gameGetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x24 */ Task* field_24; // subordinate task, killed and cleared by func_actor_444000_80132694
                               /// Area-record id published to `Mc_SaveData.at4.loc.view` on every enter/re-enter. The
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

/// Scratchpad frame `func_actor_444000_8013482C` carves off `G_SCRATCH_HEAD`
/// for the run-out / turn / run-back pass. `dir` is first the offset from the
/// model to the player, whose yaw against the model's own facing becomes
/// `Actor444000Work::field_7C4`, and later the normalised, GPF-scaled step the
/// turn adds to the coordinate; `m` is the working copy of the model's root
/// coordinate and `angle` the yaw `Gfx_RotMatrixY` rebuilds it from.
typedef struct Actor444000RunScratch {
    /* 0x00 */ SVECTOR    dir;
    /* 0x08 */ OverlayMat m;
    /* 0x28 */ s16        pad_28;
    /* 0x2A */ s16        angle;
} Actor444000RunScratch;
STATIC_ASSERT_SIZEOF(Actor444000RunScratch, 0x2C);

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

/// The overlay's event/controller task, whose `work` holds an
/// `Actor444000EventWork`.
extern Task* D_actor_444000_80161860;
/// The enemy task itself, published for the overlay's other code.
extern Task* D_actor_444000_80161878;

/// Weapon class (1 is the class whose animations sit at the low base) and the
/// equipped-weapon index within it; together they pick the player animation the
/// action-1 cue installs.
extern s8 D_8007218A;
extern u8 D_80073BA9;

extern s8 D_8007216D;

/// 0xFF-terminated area-record list this overlay applies on entry.
extern GpAreaApplyRec D_8018FB6C[];

/// Main-executable globals with no module header yet: `D_80071075` gates the
/// event on the "everything is dead" state, and `D_8007272D` is the ending
/// selector the death sequence latches.
extern u8 D_80071075;
extern s8 D_8007272D;

/// Gameplay-resident globals the state-3 hand-off touches: `D_80187150` is the
/// task table the successor is spawned from, `D_8018FBC8` the view id copied
/// into `GameSession::sceneClock`, and `D_801855DE` a counter cleared with it.
extern s16      D_801855DE;
extern TaskDesc D_80187150;
extern u16      D_8018FBC8;

/// Spawn tables `func_800E8634` forwards to `Task_Spawn`, taken as raw
/// addresses: the first pair is used by the `spawnArg1` fast path in state 0
/// and the second by state 2.
extern u8 D_actor_444000_80144634;
extern u8 D_actor_444000_8014488C;
extern u8 D_actor_444000_8014431C;
extern u8 D_actor_444000_801444E4;

/// Animation-set table this overlay hands the player task as message 0x3F4's
/// `GpAnimArg::animBlock`, the counterpart of `D_actor_403100_8015570C`. The first
/// entry is a `GpAnimSet` in the overlay's own data; the other three point at
/// its work areas.
extern GpAnimSet* D_actor_444000_8014430C[4];

extern SVECTOR D_actor_444000_80161870;

/// Six-halfword block in the overlay's parameter data -- the same shape repeats
/// at the next label -- whose `[0]` is the damage pool the hit handler for groups
/// 3, 4 and 5 refills `field_F0A` with once it runs out. The aggregate
/// declaration is load-bearing as well as accurate: as a bare scalar the load
/// stops aliasing the neighbouring struct store and GCC fills the load delay
/// with it (see DECOMPILATION_LEARNINGS.md, "Struct-typing a body changes GCC
/// 2.8.1's aliasing").
extern s16 D_actor_444000_80144A3C[];
/// The same six-halfword shape as `D_actor_444000_80144A3C`, one label along:
/// `[0]` refills `field_F0C`, the pool the hit handler for groups 6, 7 and 8
/// draws down. Declared as an aggregate for the same aliasing reason.
extern s16 D_actor_444000_80144A4C[];

/// Pair descriptors the host and its escorts publish as `GpEnemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern GpPairSrcE D_actor_444000_80144A28;
extern GpPairSrcE D_actor_444000_80144A38;
extern GpPairSrcE D_actor_444000_80144A48;
extern GpPairSrcE D_actor_444000_80144A58;

extern s16 D_actor_444000_80144A68;
extern s32 D_actor_444000_80144A6C;
extern s16 D_actor_444000_80144A70;
extern s16 D_actor_444000_80144A72;
extern s32 D_actor_444000_80144A74;
extern s32 D_actor_444000_80144A7C;
extern s32 D_actor_444000_80144A84;
extern s32 D_actor_444000_80144A8C;
/// Script pair the drag tick spawns every `period` frames.
extern s32 D_actor_444000_80144A94;
extern s32 D_actor_444000_80144AA0;

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern GpAnimSet* D_actor_444000_80161448[];
extern GpAnimSet* D_actor_444000_80161500[];
extern GpAnimSet* D_actor_444000_801615B8[];
/// The enemy task's message-handler table, parked in `Task::msgTable`.
extern void* D_actor_444000_80161818;
/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_444000_801616B0;
/// Effect argument block the spawn state points at the host's root coordinate.
extern GpEffArg D_actor_444000_80161880;
/// Shared 0x7DA payload buffer, also used by `func_actor_444000_80141618`.
extern GpCmdArg D_actor_444000_80161888;
/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80172604;

/// Global freeze flag: 1 while the game is halted, which stops the run below
/// from advancing the model.
extern u8         D_80072729;
extern GpAnimSet* D_actor_444000_80161694[];

/// Which of the three drop-point groups the falling enemies use this round,
/// rerolled off `Gp_LcgState` whenever a spawn arrives with `spawnArg1` 0.
extern u8 D_actor_444000_80161690;
/// Per-`spawnArg1` offset from the host model to the point the enemy is stood
/// up at when it is spawned.
extern SVECTOR D_actor_444000_80161704[];
/// The drop points themselves: `vz` is added to the ring x coordinate and `vx`
/// (less 0x189C) becomes the z coordinate.
extern SVECTOR D_actor_444000_80161744[];
/// `[group][spawnArg1]` index into `D_actor_444000_80161744`.
extern u8 D_actor_444000_801617C4[][8];
/// Reply buffer the hold state hands message 0x3F8.
extern GpDelayArg D_actor_444000_80161898;
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

/// World point the spinner chases: written by `func_actor_444000_8013E058`,
/// read by the spinner's tick as the target of its step.
extern SVECTOR D_actor_444000_80161890;

/// Shared coordinate `func_actor_444000_80140BBC` rebuilds when the fight
/// reaches sub-state 0x2D of state 9, parented to the host model's fifth part.
extern Actor403200DropCoord D_actor_444000_801618B8;

/// Which of the three shared debris coordinates below the next launch uses,
/// cycled 0/1/2 by `func_actor_444000_801404C0`.
extern s16 D_actor_444000_80161850;
/// The three coordinates that debris effects are spawned on, each rebuilt in
/// view space from the first escort's second part.
extern GsCOORDINATE2 D_actor_444000_80161948[];
/// Spawn table of the enemy the arena fight drops in every tenth step.
extern TaskDesc D_actor_444000_801617DC;

/// The animation-set table the fight installs on the player through message
/// 0x3FF; entry 4 is rebuilt from the player's own weapon block before the
/// second (`field_4 == 4`) send.
extern GpAnimSet* D_actor_444000_80161670[];
/// The companion table used instead when the player is more than a quarter turn
/// off the host's facing, so the hold plays from the other side.
extern GpAnimSet* D_actor_444000_80161680[];
/// Set while the escort-order tick holds the player at a placement of its own;
/// 1 marks the plain re-placement, 0 the full grab.
extern s8 D_actor_444000_80161868;
/// Shared message 0x3E9 placement payload the escort-order tick sends slot 3.
extern GpXformArg D_actor_444000_80161908;
/// Reply buffer the fight hands message 0x3F8 before asking for the hold.
extern GpDelayArg D_actor_444000_80161928;

/// Global game-mode byte; sits inside a small flag block, so it is declared as
/// an array -- the load has to keep aliasing the scratch stores beside it (see
/// DECOMPILATION_LEARNINGS.md, "Declare a fixed-address global as an array").
extern s8 D_8007218B[];

/// Per-animation reset argument, a `[?][0x2D]` table of `field_7B3` indexed by
/// the id that was playing before the switch.
extern s8 D_actor_444000_80160C5C[][0x2D];

extern void func_80185220(void);
void        func_8004BFF8(s16 angle, MATRIX* matrix);
MATRIX*     ScaleMatrix(MATRIX* m, VECTOR* v);
void        func_8010C980(void* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_444000_80132808(GsCOORDINATE2* coord, s16 yaw);
s32  func_actor_444000_80132B14(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);
void func_actor_444000_80132CB8(Task* task, s16 scale, s16 drop, s16 index);
void func_actor_444000_80133010(Task* task);
void func_actor_444000_80133C58(Task* task, s16 arg1);
void func_actor_444000_80133DE4(Task* task, s16 arg1);
void func_actor_444000_80133F64(Task* task);
void func_actor_444000_801341C4(Task* arg0);
void func_actor_444000_8013441C(Task* arg0);
void func_actor_444000_80134688(GsCOORDINATE2* coord, s32 id);
void func_actor_444000_8013482C(Task* task);
void func_actor_444000_80135448(Task* arg0);
void func_actor_444000_801371E8(Task* task, s32 scale, s16 face);
void func_actor_444000_80139EE4(GpEnemy* enemy, Task* task);
void func_actor_444000_8013A1C4(GpEnemy* enemy, Task* task);
void func_actor_444000_8013A3AC(GpEnemy* enemy, Task* task);
void func_actor_444000_8013AFF8(GpEnemy* enemy, Task* task);
void func_actor_444000_80141DFC(Task* arg0);
void func_actor_444000_801423C4(GpEnemy* enemy, Task* task);
void func_actor_444000_801433B8(Task* arg0);
void func_actor_444000_801434C4(Task* arg0);
void func_actor_444000_801435CC(Task* arg0);
void func_actor_444000_801436CC(GpEnemy* enemy, Task* task);
void func_actor_444000_80143798(GpEnemy* enemy, Task* arg1);
void func_actor_444000_80143BFC(GpEnemy* arg0, Task* arg1);
s32  func_actor_444000_80143D68(Task* arg0);
s32  func_actor_444000_80143F38(Task* arg0);
void func_actor_444000_80143F4C(Task* arg0);

/// Run one step of the event task: act on the pending action index in
/// `field_2C`, then clear it so the action fires once.
void func_actor_444000_80132054(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* other;
    Actor444000EventWork* target;
    GpAnimArg             msg;
    s32                   anim;

    switch (work->field_2C) {
        case 0:
            break;
        case 1:
            /* Install the weapon-specific player animation on the slot-3 task. */
            anim = D_80073BA9;
            if (D_8007218A == 1) {
                anim += 1;
            } else {
                anim += 0x22;
            }
            msg.animBlock.index = anim;
            msg.field_4         = 1;
            msg.field_8         = 1;
            msg.field_C         = 0xA;
            msg.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        case 2:
            if (work->field_20 != NULL) {
                msg.animBlock.ptr = D_actor_444000_8014430C;
                msg.field_4       = 3;
                msg.field_8       = 0;
                msg.field_C       = 0;
                msg.field_10      = 0;
                Gp_DispatchMsg(work->field_20, 0x3F4, (s32)&msg, 0);
            }
            /* Same one-shot cue as func_actor_444000_80132608. */
            other = (Actor444000EventWork*)D_actor_444000_80161860->work;
            if (other->field_2A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                other->field_2A = 1;
            }
            break;
        case 3:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            target               = (Actor444000EventWork*)task->work;
            if (target->field_20 != NULL) {
                msg.animBlock.ptr = D_actor_444000_8014430C;
                msg.field_4       = 0;
                msg.field_8       = 1;
                msg.field_C       = 0xA;
                msg.field_10      = 0;
                Gp_DispatchMsg(target->field_20, 0x3F4, (s32)&msg, 0);
            }
            break;
    }
    work->field_2C = 0;
}

/// Bring the room's presentation up to date for an enter (0), a first entry
/// (1) or a re-entry (2): pick the view set from the current disc/scenario
/// stage in `GameSession::field_132`, republish the area-record id, and on a
/// first entry spawn the accompanying task. Any other `arg0` does nothing.
void func_actor_444000_801321FC(s32 arg0)
{
    Actor444000EventWork* work;

    work = (Actor444000EventWork*)D_actor_444000_80161860->work;
    switch (arg0) {
        case 0:
            gGameSession->viewDirty  = 1;
            Mc_SaveData.at4.loc.view = work->field_28.b;
            break;
        case 1:
        case 2:
            switch (gGameSession->field_132) {
                case 0:
                    gGameSession->at4.loc.room = 4;
                    D_8007216D                 = 4;
                    break;
                case 1:
                    gGameSession->at4.loc.room = 5;
                    D_8007216D                 = 5;
                    break;
                case 2:
                case 3:
                    gGameSession->at4.loc.room = 6;
                    D_8007216D                 = 6;
                    break;
            }
            gGameSession->eventRoomIndex = gGameSession->at4.loc.room - 1;
            gGameSession->field_133      = 1;
            gGameSession->roomObjsDirty  = 1;
            Mc_SaveData.at4.loc.view     = work->field_28.b;
            Gp_ApplyAreaRecs(D_8018FB6C);
            if (arg0 == 1) {
                work->field_24 = Task_Spawn(1, 0x2D, 0x10, 0);
            }
            gGameSession->viewDirty = 1;
            break;
    }
}

/// Task body of the overlay's event/controller task, run once per frame while
/// the session is not paused (`GameSession::field_65`), no cutscene is active
/// (`Gp_StateC08.field_9`) and the battle state is not frozen
/// (`Gp_StateF0.field_4`).
///
/// State 0 allocates the `Actor444000EventWork` block and publishes the task in
/// `D_actor_444000_80161860`; a task spawned with `spawnArg1` set jumps
/// straight to state 3, otherwise it advances one state at a time. State 1
/// counts 0x2BD frames and then arms the death/ending sequence once. State 2
/// counts 0x15 frames and hands off to the follow-up task table. State 3 waits
/// for the room to settle, spawns the successor from `D_80187150` and kills
/// this task.
void func_actor_444000_80132358(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* alloc;
    Actor444000EventWork* other;
    s32                   state;
    s16                   timer;

    if (gGameSession->field_65 != 0) {
        return;
    }
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    if (Gp_StateF0.field_4 != 0) {
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.field_A == 1) {
                return;
            }
            if (D_80071075 != 0) {
                return;
            }
            alloc      = (Actor444000EventWork*)memCalloc(sizeof(Actor444000EventWork), false);
            task->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(task);
            } else {
                Mem_Set(alloc, 0, sizeof(Actor444000EventWork));
                alloc->field_20         = gameGetPtrSlot(3);
                D_actor_444000_80161860 = task;
            }
            if (task->spawnArg1 != 0) {
                work             = (Actor444000EventWork*)task->work;
                work->field_28.h = gGameSession->at4.loc.view;
                Gp_MsgPlayerWeapon(0);
                func_800E8634((s32)&D_actor_444000_80144634, 0, (s32)&D_actor_444000_8014488C);
                task->state = 3;
            } else {
                task->state += 1;
            }
            break;
        case 1:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x2BD) {
                Gp_MsgPlayerWeapon(0);
                other = (Actor444000EventWork*)D_actor_444000_80161860->work;
                if (other->field_30 == 0) {
                    Gp_StateF0.field_6       = 0;
                    Gp_StateF0.field_1       = 0xF;
                    Gp_StateF0.field_0       = 0;
                    Gp_StateF0.field_2       = 0;
                    Gp_StateF0.field_3       = 0;
                    gGameSession->flowFlags |= 0x80;
                    D_8007272D               = 0xD;
                    other->field_30          = state;
                }
                task->killCountdown = 0;
                task->state        += 1;
            }
            break;
        case 2:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x15) {
                work->field_28.h = gGameSession->at4.loc.view;
                func_800E8634((s32)&D_actor_444000_8014431C, 0, (s32)&D_actor_444000_801444E4);
                task->state += 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_801855DE               = 0;
                gGameSession->sceneClock = D_8018FBC8;
                Task_SpawnFromTable(&D_80187150, 0, 1, 0);
                taskKill(task);
                return;
            }
            break;
    }
    func_actor_444000_80132054(task);
}

/// Play the event's sound cue once, latching a flag so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_2A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_2A = 1;
    }
}

/// Forward a message to the slot-3 task the event work block carries.
void func_actor_444000_8013265C(s32 arg0)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    Gp_DispatchMsg(work->field_20, 0x3F3, arg0, 0);
}

/// Kill the subordinate task the event work block carries, if it is still alive.
void func_actor_444000_80132694(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_24 != NULL) {
        taskKill(work->field_24);
        work->field_24 = NULL;
    }
}

void func_actor_444000_801326DC(void)
{
    GpCmdArg msg;

    msg.from.loc.stage = 0;
    msg.from.loc.area  = 0x2C;
    msg.command        = 3;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

/// Send message 0x7DA to the slot-4 task, tagged with the current session's
/// stage and area and the caller's selector. Nothing in the actor calls it.
void func_actor_444000_80132724(s16 arg0)
{
    GpCmdArg msg;

    msg.from.loc.stage = gGameSession->at4.loc.stage;
    msg.from.loc.area  = gGameSession->at4.loc.area;
    msg.command        = arg0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

/// Arm the actor's death sequence once: reset the `Gp_StateF0` claim block,
/// flag the session and pick area script 0xD, then latch `field_30` so a later
/// call does nothing.
void func_actor_444000_80132778(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_30 == 0) {
        Gp_StateF0.field_6       = 0;
        Gp_StateF0.field_1       = 0xF;
        Gp_StateF0.field_0       = 0;
        Gp_StateF0.field_2       = 0;
        Gp_StateF0.field_3       = 0;
        gGameSession->flowFlags |= 0x80;
        D_8007272D               = 0xD;
        work->field_30           = 1;
    }
}

/// Set the actor's action index, resetting the sub-state counter that goes
/// with it.
void func_actor_444000_801327E8(s16 action)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    work->field_2C = action;
    work->field_2E = 0;
}

/// Set `coord`'s rotation to its view-space orientation turned by `yaw`,
/// expressed back in its parent's frame, and refresh the coordinate. The work
/// matrix is borrowed from the scratchpad stack.
void func_actor_444000_80132808(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    actorAccumulateToView(coord, rotation);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

s32 func_actor_444000_80132B14(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]         += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]         += s->delta.vz.h.hi;
        D_actor_444000_80161870.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_444000_80161870.vy = s->delta.vy.w >> 16;
        D_actor_444000_80161870.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_444000_80161870.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_444000_80161870.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_444000_80161870.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_444000_80161870.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Rebuild quad `index` of the collision grid as a wall across the front of
/// the task's model: its edge runs 0x1388 either way along the model's x axis
/// at `scale` out along its z axis, and vertices 0 and 1 sit `drop` below 2
/// and 3. The quad's grid normal becomes the model's z axis at unit length,
/// and the face record takes flag 3 in area 0x27, 2 elsewhere.
void func_actor_444000_80132CB8(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR     dir;
    GpGridFace  face;
    SVECTOR*    normal;
    SVECTOR*    verts;
    GpGridFace* faces;
    SVECTOR*    d;

    normal = &Gp_GridParams->field_4[index];
    verts  = Gp_GridParams->field_8;
    faces  = Gp_GridParams->field_C;

    face.verts[0] = index * 4;
    face.verts[1] = index * 4 + 1;
    face.verts[2] = index * 4 + 2;
    face.verts[3] = index * 4 + 3;
    face.field_8  = index;
    face.field_A  = 3;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, normal);
    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(normal, normal);
    gte_lddp(scale);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);
    gte_lddp(0x1388);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] + dir.vx + normal->vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy = dir.vy + normal->vy;
    verts[index * 4].vz                           = verts[index * 4 + 2].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] + dir.vz + normal->vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] - dir.vx + normal->vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy = -dir.vy + normal->vy;
    verts[index * 4 + 1].vz                           = verts[index * 4 + 3].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] - dir.vz + normal->vz;

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, normal);
    VectorNormalSS(normal, normal);
    gte_lddp(0x1000);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);

    if (gGameSession->at4.loc.area == 0x27) {
        face.field_A = 3;
    } else {
        face.field_A = 2;
    }

    faces[index] = face;
}

/// Pose the fifth escort -- the seven-part model whose coordinate array hangs
/// off `field_ECC[4]` -- for the arena phase `field_7A4` names. Every part is
/// reset to the same spot (the whole body dropped by a fifth of the fight's
/// progress counter, which is floored at 0x1CC here), then the phase picks a
/// target yaw per part in `field_784` and each `field_794` walks toward its
/// target by at most `field_7A6`, which is what drives the part rotations.
/// Phase 2 targets the angles the parts are already at, so it holds the pose it
/// was handed; anything past the six it knows poses like the 0x54-step phase.
void func_actor_444000_80133010(Task* task)
{
    Actor444000Work* work = task->work;
    s16              i;

    if (((TmdObject*)work->field_ECC[4]->task->extra)->buffer == NULL) {
        return;
    }

    if (D_actor_444000_80144A70 < 0x1CC) {
        D_actor_444000_80144A70 = 0x1CC;
    }

    for (i = 0; i < 7; i++) {
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].coord.t[0]     = ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].coord.t[1] =
            ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].coord.t[2] = 0;
        if ((u16)i >= 2) {
            ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].coord.t[2] = (s16)(D_actor_444000_80144A70 / 5);
        }
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->coords[i]);
    }

    switch (work->field_7A4) {
        case 4:
            work->field_7A6    = 2;
            work->field_784[1] = 0x180;
            work->field_784[2] = 0x20;
            work->field_784[3] = 0;
            work->field_784[4] = 0;
            work->field_784[5] = 0;
            work->field_784[6] = 0;
            break;

        case 3:
            work->field_7A6 = 8;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x80;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 6; i < 7; i++) {
                    work->field_784[i] = -0x60;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 5; i++) {
                    work->field_784[i] = -0x40;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x40;
                work->field_784[2] = 0;
                for (i = 3; i < 4; i++) {
                    work->field_784[i] = -0x60;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x40;
                }
            }
            break;

        case 2:
            work->field_7A6    = 0x10;
            work->field_784[1] = 0xC0;
            work->field_784[2] = 0x60;
            work->field_784[3] = 0x20;
            work->field_784[4] = work->field_794[4];
            work->field_784[5] = work->field_794[5];
            work->field_784[6] = work->field_794[6];
            break;

        case 1:
            work->field_7A6 = 0x59;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = 0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            }
            break;

        case 0:
            work->field_7A6 = 0x10;
            if (D_actor_444000_80144A70 < 0x258) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x100;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            }
            break;

        case 5:
        default:
            work->field_7A6 = 0x54;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            }
            break;
    }

    for (i = 1; i < 7; i++) {
        if (abs(work->field_794[i] - work->field_784[i]) < work->field_7A6) {
            work->field_794[i] = work->field_784[i];
        } else if (work->field_794[i] < work->field_784[i]) {
            work->field_794[i] = work->field_794[i] + work->field_7A6;
        } else {
            work->field_794[i] = work->field_794[i] - work->field_7A6;
        }
        Gfx_RotMatrixX(&((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].coord, work->field_794[i], 1);
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords[i].flg = 0;
    }
}

/// Walk the yaw `field_7C8` toward `arg1` (clamped to +/-0x200) by at most 0x71
/// per call, turn model part 3 by it through `func_actor_444000_80132808`, and
/// refresh part 3, the root of the fifth escort's model and part 4.
void func_actor_444000_80133C58(Task* task, s16 arg1)
{
    Actor444000Work* work = task->work;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    func_actor_444000_80132808(&((TmdObject*)task->extra)->coords[3], work->field_7C8);
    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->coords[0]);
    ((TmdObject*)task->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[4]);
}

/// Walk the pitch `field_F00` toward `arg1` (clamped to 0..0x500) by at most
/// 0x10 per call, then pitch model parts 3 and 4 about x: part 3 to half of
/// it, part 4 against it, each net of the pitch it already has.
void func_actor_444000_80133DE4(Task* task, s16 arg1)
{
    Actor444000Work* work = task->work;
    s16              value;
    s16              pitch4;
    s16              pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->field_F00 < value) {
        if (value - work->field_F00 >= 0x11) {
            work->field_F00 = work->field_F00 + 0x10;
        } else {
            work->field_F00 = value;
        }
    } else if (value < work->field_F00) {
        if (abs(work->field_F00 - value) >= 0x11) {
            work->field_F00 = work->field_F00 - 0x10;
        } else {
            work->field_F00 = value;
        }
    }

    pitch4 = -ratan2(((TmdObject*)task->extra)->coords[4].coord.m[1][2],
                     ((TmdObject*)task->extra)->coords[4].coord.m[2][2]);
    pitch3 = -ratan2(((TmdObject*)task->extra)->coords[3].coord.m[1][2],
                     ((TmdObject*)task->extra)->coords[3].coord.m[2][2]);

    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[3].coord, work->field_F00 / 2 - pitch3, 0);
    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[4].coord, -work->field_F00 - pitch4, 0);
    ((TmdObject*)task->extra)->coords[4].flg = 0;
}

/// Seed the blend: `field_7BE` to 0x30 and `field_7C0` to 0x800. Slots 1 and up
/// of the first animation context of each of the three pairs take that rate,
/// and the same slots of the second context are reset to animation
/// `field_7BC`.
void func_actor_444000_80133F64(Task* task)
{
    Actor444000Work* work;
    s32              i;

    work            = task->work;
    work->field_7BE = 0x30;
    work->field_7C0 = 0x800;
    i               = 1;
    do {
        work->slots0[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim1, i, work->field_7BC);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->slots2[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim3, i, work->field_7BC);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->slots4[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim5, i, work->field_7BC);
        i++;
    } while (i < 4);
}

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `rate` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void func_actor_444000_80134040(Task* arg0)
{
    Actor444000Work* work = arg0->work;
    s32              i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].rate = work->field_7B6;
            func_800B4114(&work->anim0, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].rate = work->field_7B6;
            func_800B4114(&work->anim2, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].rate = work->field_7B6;
            func_800B4114(&work->anim4, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}

/// Advance the three animation pairs with the second context of each blended
/// in at weight `field_7C0`: every slot of the second context ticks at
/// `field_7BE`, every slot of the first at `field_7B6` less 3, and the pose
/// written to the first is the mix of the two.
void func_actor_444000_801341C4(Task* arg0)
{
    GpAnimPose       pose0;
    GpAnimPose       pose1;
    Actor444000Work* work     = arg0->work;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].rate = work->field_7BE;
            work->slots0[i].rate = work->field_7B6 - 3;
            func_800B3448(&work->anim0, i, (s32)&pose0, 0);
            func_800B3448(&work->anim1, i, (s32)&pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].rate = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].rate = work->field_7BE;
        work->slots2[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim2, i, (s32)&pose0, 0);
        func_800B3448(&work->anim3, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].rate = work->field_7BE;
        work->slots4[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim4, i, (s32)&pose0, 0);
        func_800B3448(&work->anim5, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}

/// Per-frame animation step. `field_7B0` 1 re-seeds the block through
/// `func_actor_444000_80134040`, 2 resets every slot of the three even members
/// from `field_7B3` outright; either way the block is armed (`field_7B0` 3, the
/// frame counter and the 0x20-byte scratch at `field_7D0` cleared). Then the
/// slots are advanced: plainly while `field_7B1` is clear, otherwise through the
/// blended path, which clears `field_7B1` again once the first pair's slot 1
/// reports done. The three trailing flags run the shared reaction helpers.
void func_actor_444000_8013441C(Task* arg0)
{
    Actor444000Work* work = arg0->work;
    Actor444000Work* w;
    s32              i;

    if (work->field_7B0 == 1) {
        func_actor_444000_80134040(arg0);
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    } else if (work->field_7B0 == 2) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim0, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim2, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim4, i, w->field_7B3);
        }
        w->field_7B2    = w->field_7B3;
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    }

    if (work->field_7BA == 2) {
        func_actor_444000_80133F64(arg0);
        work->field_7BA = 3;
    }

    work->field_7B4++;

    if (work->field_7B1 == 0) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim0, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim2, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim4, i);
        }
    } else {
        func_actor_444000_801341C4(arg0);
        if (work->slots1[1].flags & 1) {
            work->field_7B1 = 0;
        }
    }

    if (work->field_EF4 != 0) {
        func_actor_444000_80133DE4(arg0, work->field_EFE);
    }
    if (work->field_EF6 != 0) {
        func_actor_444000_80133C58(arg0, work->field_7C4);
    }
    if (work->field_EF8 != 0) {
        func_actor_444000_80133010(arg0);
    }
}

/// Spawn the hit effect for attack `id` on `coord`. The effect kind comes from
/// the attack's param 1; its rotation from param 0: kinds 2, 4, 6 and 7 use one
/// fixed rotation, every other kind draws one of three off `Gp_LcgState`. The
/// rotation and the effect argument live in a block borrowed from the
/// scratchpad stack for the duration of the call.
void func_actor_444000_80134688(GsCOORDINATE2* coord, s32 id)
{
    Actor403200EffScratch* sc = (Actor403200EffScratch*)(SCRATCH_SP -= sizeof(Actor403200EffScratch));

    sc->eff.spawnArgLo = 0x500;
    sc->eff.coord      = coord;
    sc->eff.spawnArgHi = 3;

    switch (Gp_GetIdParam0(id) & 0xFFFF) {
        case 2:
        case 4:
        case 6:
        case 7:
            sc->rot.vx = 0;
            sc->rot.vy = -0x190;
            sc->rot.vz = 0x258;
            func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
            break;
        case 0:
        case 1:
        case 3:
        case 5:
        case 8:
        case 9:
        default:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            switch ((u16)(((u32)Gp_LcgState >> 16) % 3U)) {
                case 0:
                    sc->rot.vy = 0;
                    sc->rot.vx = 0;
                    sc->rot.vz = 0x384;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 1:
                    sc->rot.vx = 0x258;
                    sc->rot.vy = -0xC8;
                    sc->rot.vz = 0x2BC;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 2:
                    sc->rot.vx = -0x12C;
                    sc->rot.vy = -0x320;
                    sc->rot.vz = 0x320;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
            }
            break;
    }

    SCRATCH_SP += sizeof(Actor403200EffScratch);
}

/// Walk `coord` a fixed 0x32/0x1000 of its own forward axis (column 2 of its
/// rotation, normalised and GPF-scaled) and flag it for rebuild. The direction
/// vector lives in an `SVECTOR` carved off `G_SCRATCH_HEAD` and handed straight
/// back; written as an inline so those scratch-head accesses stay absolute, the
/// same reason as `Actor444000_ShrinkRotation` above.
static __inline__ void Actor444000_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = *(u8**)G_SCRATCH_HEAD;
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    *(SVECTOR**)G_SCRATCH_HEAD = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x32);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

/// The run-out / turn / run-back pass, stepped by `Actor444000Work::field_F08`.
///
/// A reset request re-arms the block: the model's flag word and the enemy's
/// link state are cleared, both colour steps are switched on, animation 2 is
/// requested and the scratch matrix is seeded with an identity rotation.
///
/// Every step publishes the yaw from the model's own facing to the player in
/// `field_7C4`, wrapped into +/-0x800, and -- unless the game is frozen --
/// walks the model forward along that facing. State 0 runs out to x 0x1770,
/// state 1 turns the model 0xD a step until it has swung the full half turn
/// (its rotation is rebuilt from the running `angle` rather than spun in
/// place), and states 2 to 5 run it back through -0x1387, -0x251B and -0x32C7.
/// Past -0x4203 the task hands over to state 0x10 and tells the player task
/// (slot 7) message 0x13F4.
void func_actor_444000_8013482C(Task* task)
{
    Actor444000RunScratch* sc;
    OverlayMat*            mat;
    TmdObject*             tmd;
    Actor444000Work*       work;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         model;
    GsCOORDINATE2*         facing;
    u8*                    head;
    s16                    ang;
    s32                    frame;

    head        = (u8*)SCRATCH_SP;
    SCRATCH_SP -= sizeof(Actor444000RunScratch);
    sc          = (Actor444000RunScratch*)SCRATCH_SP;

    work  = task->work;
    enemy = task->spawnArg2;

    if (work->field_4 != 0) {
        tmd                       = (TmdObject*)task->extra;
        enemy->node.state.b.flags = 0;
        tmd->flags                = 0;
        work->field_EF4           = 1;
        work->field_7B3           = 2;
        work->field_EF6           = 1;
        work->field_7B0           = 1;
        work->field_EFA           = 0;
        work->field_EFE           = 0;
        mat                       = &((Actor444000RunScratch*)(head - sizeof(Actor444000RunScratch)))->m;
        mat->ident.m00_m01        = 0x1000;
        mat->ident.m02_m10        = 0;
        mat->ident.m11_m12        = 0x1000;
        mat->ident.m20_m21        = 0;
        mat->ident.m22            = 0x1000;
    }

    func_actor_444000_8013441C(task);

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
    }

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].curRec & 0x3FF;

    model      = ((TmdObject*)task->extra)->coords;
    sc->dir.vx = Player_Status.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = Player_Status.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = Player_Status.coordMtx->t[2] - model->coord.t[2];

    facing = ((TmdObject*)task->extra)->coords;
    ang    = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);

    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }

    work->field_7C4 = ang;

    switch (work->field_F08) {
        case 0: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[0] >= 0x1770) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 1:
            coord = ((TmdObject*)task->extra)->coords;
            if (coord->coord.t[0] < 0x2134) {
                if (D_80072729 != 1) {
                    Actor444000_StepForward(coord);
                }
            } else {
                sc->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0xD;
                sc->m.mat = ((TmdObject*)task->extra)->coords->coord;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(0xBEA);
                gte_ldsv(&sc->dir);
                gte_gpf12();
                gte_stsv(&sc->dir);

                sc->m.mat.t[0] += sc->dir.vx;
                sc->m.mat.t[1] += sc->dir.vy;
                sc->m.mat.t[2] += sc->dir.vz;

                Gfx_RotMatrixY(&sc->m.mat, sc->angle, 1);
                ((TmdObject*)task->extra)->coords->coord = sc->m.mat;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(-0xBB8);
                gte_ldsv(&sc->dir);
                gte_gpf12();
                gte_stsv(&sc->dir);

                ((TmdObject*)task->extra)->coords->coord.t[0] += sc->dir.vx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += sc->dir.vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += sc->dir.vz;
                ((TmdObject*)task->extra)->coords->flg         = 0;

                if (0x800 - ABS(sc->angle) < 0xD) {
                    sc->angle = 0x800;
                    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x800, 1);
                    work->field_F08++;
                }
            }
            break;

        case 2: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x1387) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 3: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x251B) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 4: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x32C7) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 5: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x4203) {
                work->field_0 = 0x10;
                work->field_F08++;
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            }
            break;
    }

    SCRATCH_SP += sizeof(Actor444000RunScratch);
}

/// Rebuild `coord`'s rotation around the yaw it already faces, left at full
/// width but scaled by `y` vertically -- the squash the death sequence retracts
/// each body with. The same shape as `Actor444000_ScaleRotation` below, except
/// the vertical scale arrives as an `s16`, which is what puts its sign
/// extension at the `scale.vy` store rather than at the call site. The working
/// matrix lives in a frame carved off `G_SCRATCH_HEAD`, handed back once the
/// rotation has been copied onto the coordinate.
static __inline__ void Actor444000_SquashRotation(GsCOORDINATE2* coord, s16 y)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = y;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
}

/// State 0x12, the death sequence: the boss collapses, each of its escort
/// bodies is cut loose from the model hierarchy and then squashed flat in its
/// own window of the sub-state counter.
///
/// A reset request re-arms the block on animation 0x12, clears the enemy's link
/// state, the model's flag word and the four counters, marks the session
/// (`gGameSession::at4.loc.place` 3) and plays the death cue at half depth.
///
/// The rest of the tick splits on bit 0x100 of the second animation slot --
/// whether the collapse animation is still running or has finished.
///
/// While it runs, `D_actor_444000_80144A70` is walked down 0xC8 a step until it
/// is under 0x191, four one-shot cues fire on frames 0x33, 0x3D, 0x4E and 0x71
/// of the fourth slot, and sub-states 0x14, 0x82, 0x14A and 0x1DC each hand one
/// body over: 0x14 switches the host and escort 3 to light mode 1, while the
/// other three reparent escort 2, 4 and 3's model to `gGfxViewCoord`. That
/// reparenting is why both halves of the part's placement have to be resolved
/// by hand -- `actorAccumulateToView` for the rotation it had up the
/// chain and `actorLocalToView` for its origin -- the same pair
/// `func_actor_444000_80137594` uses. Past each of those sub-states the body
/// sinks toward the host's own height 0x1E a step, clamped there, and squashes
/// from 0x1000 to nothing over 0x28 steps, throwing effect 0x60196 at one of
/// three offsets every fifth step and raising flag 0x80 on the last one.
///
/// Once the animation has finished, escort 0, escort 1 and the host model are
/// squashed over their own windows (0..0x28, 0x14..0x3C and 0xD..0x85), the
/// host throws one of five effects around itself every third step of its
/// window, escort 3 halves its height over the 0x5A steps from 0xA1 -- the step
/// that ends it also raises the arena floor's last eight grid corners -- and
/// step 0xA0 enqueues the collapse cue.
void func_actor_444000_80135448(Task* task)
{
    Actor444000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    MATRIX           mat;
    SVECTOR          pos;
    SVECTOR*         verts;
    s32              frame;
    s32              step;

    work  = task->work;
    enemy = task->spawnArg2;

    if (work->field_4 != 0) {
        s32 id;
        s32 pan;

        tmd                       = (TmdObject*)task->extra;
        enemy->node.state.b.flags = 0;
        tmd->flags                = 0;
        work->field_7B3           = 0x12;
        work->field_EF4           = 0;
        work->field_EF6           = 0;
        work->field_EFA           = 0;
        work->field_7B0           = 1;
        work->field_EFE           = 0;

        func_actor_444000_8013441C(task);

        gGameSession->at4.loc.place = 3;
        id                          = (((u16)enemy->placeKey >> 12) << 8) | 0x54280007;
        pan                         = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
        return;
    }

    if (work->slots0[1].flags & 1) {
        work->field_6 = 0;
        func_actor_444000_8013441C(task);
    }

    if (!(work->slots0[1].flags & 0x100)) {
        if (D_actor_444000_80144A70 >= 0x191) {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        }

        func_actor_444000_8013441C(task);

        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200013;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200003;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200014;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200015;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }

        work->field_7A8 = work->slots0[3].curRec & 0x3FF;

        switch (work->field_6) {
            case 0x14:
                Gp_SetLightMode(enemy, 1);
                Gp_SetLightMode(work->field_ECC[3], 1);
                break;

            case 0x82:
                actorAccumulateToView(&((TmdObject*)task->extra)->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&((TmdObject*)task->extra)->coords[4], &pos);

                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->sub        = &gGfxViewCoord;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg        = 0;
                break;

            case 0x1DC:
                actorAccumulateToView(&((TmdObject*)task->extra)->coords[3], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&((TmdObject*)task->extra)->coords[3], &pos);

                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->sub        = &gGfxViewCoord;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg        = 0;
                break;

            case 0x14A:
                actorAccumulateToView(&((TmdObject*)task->extra)->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&((TmdObject*)task->extra)->coords[4], &pos);

                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->sub        = &gGfxViewCoord;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->flg        = 0;
                work->field_EF8                                                   = 0;
                break;
        }

        if (work->field_6 >= 0x83) {
            if (((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x82) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x82;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[2]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->flags = 0x80;
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x14B) {
            if (((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x14A) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x14A;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[4]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->flags = 0x80;
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x1DD) {
            if (((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x1DC) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
            }

            step = work->field_6 - 0x1DC;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[3]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->flags = 0x80;
            }
        }
    } else {
        switch (work->field_6) {
            case 0x28:
                break;
            case 0x78:
                Gp_SetLightMode(enemy, 2);
                break;
        }

        if (work->field_6 > 0) {
            if (work->field_6 < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[0]->task->extra)->coords,
                                           (s16)(0x1000 - ((work->field_6 * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[0]->task->extra)->flags = 2;
            } else if (work->field_6 == 0x28) {
                ((TmdObject*)work->field_ECC[0]->task->extra)->flags = 0x80;
            }
        }

        if (work->field_6 >= 0x15) {
            step = work->field_6 - 0x14;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[1]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[1]->task->extra)->flags = 2;
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[1]->task->extra)->flags = 0x80;
            }
        }

        if (work->field_6 >= 0xE) {
            step = work->field_6 - 0xD;
            if (step < 0x78) {
                Actor444000_SquashRotation(((TmdObject*)task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 120)));
                ((TmdObject*)task->extra)->flags = 2;
            } else if (step == 0x78) {
                ((TmdObject*)task->extra)->flags = 0x80;
            }
        }

        if ((s16)((s16)(u16)work->field_6 % 3) == 0 && (s16)(u16)work->field_6 - 0xD < 0x78) {
            switch ((s16)((s16)((s16)(u16)work->field_6 / 3) % 5)) {
                case 0:
                    pos.vz = 0;
                    pos.vy = 0;
                    pos.vx = 0;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14101900, &pos);
                    break;
                case 1:
                    pos.vx = 0x960;
                    pos.vy = 0;
                    pos.vz = -0x960;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x13201800, &pos);
                    break;
                case 2:
                    pos.vx = -0x9C4;
                    pos.vy = 0;
                    pos.vz = 0x9C4;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x131C1800, &pos);
                    break;
                case 3:
                    pos.vx = -0x6A4;
                    pos.vy = 0;
                    pos.vz = 0x640;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14101800, &pos);
                    break;
                case 4:
                    pos.vx = 0x6A4;
                    pos.vy = 0;
                    pos.vz = -0x640;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14301800, &pos);
                    break;
            }
        }

        if (work->field_6 >= 0xA1) {
            step = work->field_6 - 0xA0;
            if (step < 0x5A) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                           (s16)(0x800 - ((step * 0x800) / 90)));
            } else if (step == 0x5A) {
                ((TmdObject*)task->extra)->flags = 0x80;

                verts        = Gp_GridParams->field_8;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;
            }
        }

        if (work->field_6 == 0xA0) {
            SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x54280007, 1);
        }
    }
}

void func_actor_444000_801371E8(Task* task, s32 scale, s16 face)
{
    SVECTOR     dir;
    SVECTOR*    norms   = Gp_GridParams->field_4;
    SVECTOR*    corners = Gp_GridParams->field_8;
    GpGridFace* faces   = Gp_GridParams->field_C;
    GpGridFace  quad0   = { { face * 4, face * 4 + 1, face * 4 + 2, face * 4 + 3 }, face, 2 };
    GpGridFace  quad1   = {
        { (face + 1) * 4, (face + 1) * 4 + 1, (face + 1) * 4 + 2, (face + 1) * 4 + 3 }, face + 1, 2
    };
    SVECTOR* d;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    gte_lddp(scale);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    corners[face * 4].vx = corners[face * 4 + 2].vx = 0x2CEC;
    corners[face * 4].vy = corners[face * 4 + 2].vy = (u16)((TmdObject*)task->extra)->coords->coord.t[1];
    corners[face * 4].vz = corners[face * 4 + 2].vz = -0x1B58;
    corners[face * 4 + 1].vx                        = corners[face * 4 + 3].vx =
        (u16)((TmdObject*)task->extra)->coords->coord.t[0] + (u16)dir.vx;
    corners[face * 4 + 1].vy = corners[face * 4 + 3].vy =
        (u16)((TmdObject*)task->extra)->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 1].vz = corners[face * 4 + 3].vz =
        (u16)((TmdObject*)task->extra)->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4].vy     = (u16)corners[face * 4].vy - 0x190;
    corners[face * 4 + 1].vy = (u16)corners[face * 4 + 1].vy - 0x190;
    faces[face]              = quad0;

    norms[face].vz = (u16)corners[face * 4].vx - (u16)corners[face * 4 + 1].vx;
    norms[face].vy = (u16)corners[face * 4 + 1].vy - (u16)corners[face * 4].vy;
    norms[face].vx = (u16)corners[face * 4 + 1].vz - (u16)corners[face * 4].vz;
    VectorNormalSS(&norms[face], &norms[face]);

    corners[face * 4 + 4].vx = corners[face * 4 + 6].vx =
        (u16)((TmdObject*)task->extra)->coords->coord.t[0] + (u16)dir.vx;
    corners[face * 4 + 4].vy = corners[face * 4 + 6].vy =
        (u16)((TmdObject*)task->extra)->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 4].vz = corners[face * 4 + 6].vz =
        (u16)((TmdObject*)task->extra)->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4 + 5].vx = corners[face * 4 + 7].vx =
        (u16)((TmdObject*)task->extra)->coords->coord.t[0] + (u16)dir.vx + 0x1B58;
    corners[face * 4 + 5].vy = corners[face * 4 + 7].vy =
        (u16)((TmdObject*)task->extra)->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 5].vz = corners[face * 4 + 7].vz =
        (u16)((TmdObject*)task->extra)->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4 + 4].vy = (u16)corners[face * 4 + 4].vy - 0x190;
    corners[face * 4 + 5].vy = (u16)corners[face * 4 + 5].vy - 0x190;
    faces[face + 1]          = quad1;

    norms[face + 1].vx = 0;
    norms[face + 1].vy = 0;
    norms[face + 1].vz = -0x1000;
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131EA8`:
/// allocate its work block and stand the model up where the host's first
/// escort is, in view space.
///
/// The model is reparented to `gGfxViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `actorAccumulateToView` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `actorLocalToView` carries its origin along the same chain for the
/// translation. The model is then turned a quarter turn, its single display
/// node is linked with a 0x394 extent, and that node is paired with the owning
/// enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void func_actor_444000_80137594(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor444000Work*     host;
    SVECTOR              pos;
    SVECTOR              vec;

    owner = task->parent->spawnArg2;
    host  = owner->task->work;

    if (D_actor_444000_80144A68 == 1 || (s16)host->field_0 == 0x10 || (s16)host->field_0 == 5 ||
        (s16)host->field_0 == 0xC || (s16)host->field_0 == 0x12 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->field_1AC                        = 0;
    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;

    actorAccumulateToView(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1],
                          &((TmdObject*)task->extra)->coords->coord);

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x80, 0);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);

    pos.vx = pos.vy = pos.vz = 0;
    actorLinkWorkObj(((TmdObject*)task->extra)->coords, &work->obj0, &work->rec0, &pos, 0x394, 3, 1);

    work->obj0.flags &= 0x7FFF;
    work->obj0.key    = Gp_PackObjPair(owner, 2);
    work->field_1A8   = 1;
    task->state++;
}

/// Flight step of the seized player's model: carry it along the model's own
/// forward axis until it lands. `field_1A8` (the dispatcher's state-changed
/// flag) re-arms the step counter, the ground marker and the first display
/// node on the frame the state starts.
///
/// While the game is running (`Gp_StateF0.field_4` clear) the model falls 0xA a step,
/// column 2 of its coordinate is normalised into a scratchpad `SVECTOR` and
/// scaled by 0x89/0x1000 through the GTE's GPF, and that is the per-step
/// translation added to the coordinate; past step 0x29 the height is pinned to
/// -0x3E8 instead. The marker grows 0x60 a step and is drawn under the work
/// block's own coordinate, which is parented to `gGfxViewCoord` and tracks the
/// model. After 0x35 steps the display node is handed back and the task steps
/// on. Paused (`Gp_StateF0.field_4` set) only the coordinate is refreshed, and the
/// marker is skipped while the host actor sits in state 6.
///
/// Bails out -- unlinking the display node and stepping the task on -- when the
/// overlay is shutting down or the host actor has left the grab states.
void func_actor_444000_8013799C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Actor444000Work*     host;
    GpEnemy*             owner;
    u8*                  head;
    SVECTOR*             dir;
    /// Second live alias of `dir`: the GTE operand is kept in its own register
    /// for the whole function, which is what gives this function its seventh
    /// callee-saved slot.
    SVECTOR* gteDir;

    work  = task->work;
    owner = task->parent->spawnArg2;
    host  = owner->task->work;

    if (D_actor_444000_80144A68 == 1 || (s16)host->field_0 == 0x10 || (s16)host->field_0 == 5 ||
        (s16)host->field_0 == 0xC || (s16)host->field_0 == 0x12) {
        task->state++;
        Gp_UnlinkObj(&work->obj0);
        return;
    }

    head                       = *(u8**)G_SCRATCH_HEAD;
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    *(SVECTOR**)G_SCRATCH_HEAD = dir;
    gteDir                     = dir;

    if (work->field_1A8 != 0) {
        work->field_1AC   = 0;
        work->field_1B0   = 0x400;
        work->field_1A8   = 0;
        work->rec0.key    = 0;
        work->obj0.flags |= 0x8000;
    }

    if (Gp_StateF0.field_4 == 0) {
        work->field_1AC++;
        ((TmdObject*)task->extra)->coords->coord.t[1] += 0xA;

        Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12();
        gte_stsv(gteDir);

        ((TmdObject*)task->extra)->coords->coord.t[0] += dir->vx;
        ((TmdObject*)task->extra)->coords->coord.t[1] += dir->vy;
        if (work->field_1AC >= 0x29) {
            ((TmdObject*)task->extra)->coords->coord.t[1] = -0x3E8;
        }
        ((TmdObject*)task->extra)->coords->coord.t[2] += dir->vz;
        ((TmdObject*)task->extra)->coords->flg         = 0;

        work->field_1B0 += 0x60;
        Gp_ClearRec18Occupied(&work->rec0);

        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                             Gp_State1C->groundShade);

        if (work->field_1AC >= 0x35) {
            Gp_UnlinkObj(&work->obj0);
            task->state++;
            work->field_1A8 = 1;
        }
    } else {
        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        if (host->field_F08 != 6) {
            Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                                 Gp_State1C->groundShade);
        }
    }

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// shrinks it uniformly to half size: `ratan2` of the rotation's Z basis gives
/// the yaw, `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix`
/// applies 0.5 on all three axes. The working matrix lives in a frame carved
/// off `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_RebuildRotation` in `actor_444000_4.c`.
static __inline__ void Actor444000_ShrinkRotation(GsCOORDINATE2* coord)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x800;
    sc->scale.vy = 0x800;
    sc->scale.vz = 0x800;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
}

/// Entry state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block and drop the model onto the floor of the view
/// coordinate, under escort 1 of the host actor.
///
/// The model is reparented to `gGfxViewCoord`, its texture page cleared and its
/// CLUT row set to 2, and -- once the stream buffers exist -- processed twice
/// before the spawn cue is enqueued at the model's own pan and half its depth
/// with the owner's id in its high half. The task's light and colour matrices
/// are pointed into the work block, the translation is replaced by the world
/// position of part 1 of escort 1's model, and `field_1AA` is a fifteenth of
/// that height. `vel` is the horizontal gap to the player, which the later
/// states spend a fifteenth at a time. The rotation is finally rebuilt at half
/// scale around the yaw the model already faces.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_444000_80137D4C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor444000Work*     host;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    host   = owner->task->work;
    player = gameGetPtrSlot(3);

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(sizeof(Actor403200GrabWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;
    ((TmdObject*)task->extra)->tpage       = 0;
    ((TmdObject*)task->extra)->clut        = 2;

    if (((TmdObject*)task->extra)->buffer != NULL) {
        tmdProcessStream(task->extra);
        tmdProcessStream(task->extra);
        sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020001C;
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
    }

    ((TmdObject*)task->extra)->lightMtx = &work->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &work->colorMtx;

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&((TmdObject*)host->field_ECC[1]->task->extra)->coords[1], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    work->field_1AA = ((TmdObject*)task->extra)->coords->coord.t[1] / 15;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
    work->field_1AC = 0;
    work->field_1B2 = 0;

    Actor444000_ShrinkRotation(((TmdObject*)task->extra)->coords);
    task->state++;
}

/// Death throes of the grabbing enemy: bounce the model on the floor until it
/// settles. While the model is still below the floor plane (`coord.t[1] > 0`)
/// it is snapped back to -0x32, the step counter is cleared, the impact cue is
/// enqueued with the object's own pan and half its depth, and the task steps
/// on. Otherwise the body keeps falling by `field_1AA`'s magnitude, drifts a
/// fifteenth of `vel` in x and z, has its colour refreshed from the model's
/// world position, damps the two shake terms and has its rotation rebuilt at
/// half scale.
void func_actor_444000_801381B0(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = task->work;
    GsCOORDINATE2*       coord;
    VECTOR               pos;
    s32                  sfx;
    s32                  pan;
    s32                  drop;
    s32                  bounce;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    coord = ((TmdObject*)task->extra)->coords;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->field_1AC   = 0;
        sfx               = ((enemy->placeKey >> 0xC) << 8) | 0x4020000C;
        pan               = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->field_1AA);
    coord->coord.t[1] = drop + bounce;

    ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx / 15;
    ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz / 15;
    ((TmdObject*)task->extra)->coords->flg         = 0;

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    Actor444000_ShrinkRotation(((TmdObject*)task->extra)->coords);
}

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `xz` on both horizontal axes and `y` on the vertical one. The working
/// matrix lives in a frame carved off `G_SCRATCH_HEAD`, which is handed back
/// once the rotation has been copied onto the coordinate. Written as an inline
/// so the four scratch-head accesses stay absolute, like
/// `Actor444000_ShrinkRotation` above.
static __inline__ void Actor444000_ScaleRotation(GsCOORDINATE2* coord, s16 xz, s32 y)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = xz;
    sc->scale.vy = y;
    sc->scale.vz = xz;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
}

/// Rise state of the enemy dispatched through `D_actor_444000_80131EA8`: for
/// the first nine steps the model is stretched taller and thinner each step --
/// horizontally `step * 400 + 0x800` and vertically `0x800 / step` -- around
/// the yaw it already faces. On step 7 it is squashed to 0x17A0 wide at normal
/// height, and if the player is within 1000 units horizontally, is not in mode
/// 2, still has HP and answers the 0x3F8 query, the overlay's own animation-set
/// table is sent as message 0x3FF and the take-over is latched in `field_1B2`.
/// The task steps on once the count passes ten with no animation installed,
/// once the latched animation has been released, or after 200 steps. Every
/// step refreshes the model's colour from its world position and damps the two
/// shake terms. Bails to `Gp_DestroyEnemy` when the overlay is shutting down,
/// cancelling a still-installed animation on the way out.
void func_actor_444000_80138490(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    GameActor*           actor;
    PlayerStatus*        cfg;
    SVECTOR              gap;
    VECTOR               pos;
    s16                  step;
    s16                  scale;
    s32                  shrink;

    work   = task->work;
    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    cfg    = &Player_Status;

    if (D_actor_444000_80144A68 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    step = ++work->field_1AC;
    if (step < 10) {
        scale  = step * 0x190 + 0x800;
        shrink = 0x800 / step;
        Actor444000_ScaleRotation(((TmdObject*)task->extra)->coords, scale, shrink);
    }

    if (work->field_1AC == 7) {
        Actor444000_ScaleRotation(((TmdObject*)task->extra)->coords, 0x17A0, 0x800);

        gap.vx = ((TmdObject*)task->extra)->coords->coord.t[0] -
                 ((TmdObject*)player->extra)->coords->coord.t[0];
        gap.vy = 0;
        gap.vz = ((TmdObject*)task->extra)->coords->coord.t[2] -
                 ((TmdObject*)player->extra)->coords->coord.t[2];

        if (actorOutOfReach(&gap) == 0 && actor->field_954 != 2 &&
            cfg->hp > 0) {
            D_actor_444000_80161898.field_14 = 0x28;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161898, 0) == 0) {
                D_actor_444000_80144A6C  = 1;
                work->anim.animBlock.ptr = D_actor_444000_80161694;
                work->anim.field_4       = 1;
                work->anim.field_8       = 0;
                work->anim.field_C       = 3;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_1B2 = 1;
            }
        }
    }

    if (work->field_1AC >= 11 && work->field_1B2 == 0) {
        task->state++;
    } else if (work->field_1B2 == 1 && D_actor_444000_80144A6C == 0) {
        task->state++;
    } else if (work->field_1AC >= 0xC9) {
        D_actor_444000_80144A6C = 0;
        task->state++;
    }

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}

/// Hold state of the enemy dispatched through `D_actor_444000_80131EA8`: once
/// `field_1A8` says the take-over is armed and `field_1B2` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void func_actor_444000_801389EC(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    s32                  armed;

    work   = task->work;
    player = gameGetPtrSlot(3);
    if (D_actor_444000_80144A68 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        armed           = work->field_1B2;
        work->field_1AC = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        D_actor_444000_80161694[2] =
            ((Actor403200AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->sets[9];
        work->anim.animBlock.ptr = D_actor_444000_80161694;
        work->anim.field_4       = 2;
        work->anim.field_8       = armed;
        work->anim.field_C       = 9;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        ((TmdObject*)task->extra)->flags = 0x80;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block, drop the model onto the floor of the view
/// coordinate and hang the two display nodes off it.
///
/// The model is reparented to `gGfxViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height -- the bounce the
/// descent state adds back -- and `vel` the horizontal gap to the player, which
/// the later states spend a fifteenth at a time. The landing cue is enqueued at
/// the model's own pan and depth with the owner's id in its high half, the
/// model is spun to a random yaw, and the two nodes are linked with their
/// collision-record tables before the task's colour and light matrices are
/// pointed into the work block.
void func_actor_444000_80138B94(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    player = gameGetPtrSlot(3);

    if (D_actor_444000_80144A68 == 1 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&((TmdObject*)owner->task->extra)->coords[3], &vec);

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->coords->flg        = 0;

    work->field_1AA = ((TmdObject*)task->extra)->coords->coord.t[1] / 9;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    actorLinkWorkObj(((TmdObject*)task->extra)->coords, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.coord    = ((TmdObject*)task->extra)->coords;
    work->obj1.ctx.recs = &work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0x3000A;
    work->obj1.radius   = 0x100;
    work->obj1.flags    = 1;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= 0x8000;
    Gp_InitRec18Table(work->obj1.ctx.recs, 3, 0);
    work->obj1.flags |= 0x4000;
    work->obj0.key    = Gp_PackObjPair(owner, 5);

    ((TmdObject*)task->extra)->lightMtx = &work->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &work->colorMtx;
}

/// Descent state that follows the hold: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `field_1AA` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `rec1` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void func_actor_444000_80138FC4(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = task->work;
    VECTOR               pos;
    s32                  pan;

    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        Gp_SetLightMode(enemy, 0);
        ((TmdObject*)task->extra)->flags = 2;
    }

    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0) {
        work->obj0.flags                              |= 0x8000;
        work->obj1.flags                              |= 0x4000;
        ((TmdObject*)task->extra)->coords->coord.t[1] += ABS(work->field_1AA);
    }

    if (((TmdObject*)task->extra)->coords->coord.t[1] >= -0x31) {
        ((TmdObject*)task->extra)->coords->coord.t[1] = -0x32;
        work->field_1AC                               = 0;
        pan                                           = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        task->state++;
    }

    if (func_actor_444000_80132B14(((TmdObject*)task->extra)->coords, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        ((TmdObject*)task->extra)->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx / 9;
    ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz / 9;
    ((TmdObject*)task->extra)->coords->flg         = 0;

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// State handlers of the textured prop sub-task: setup, per-frame refresh and
/// teardown.
const GpEnemyTaskFuncTable3 D_actor_444000_80131E90 = {
    {
        func_actor_444000_801436CC,
        func_actor_444000_80143798,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the enemy `func_actor_444000_801438E4` dispatches.
const GpEnemyTaskFuncTable3 D_actor_444000_80131E9C = {
    {
        func_actor_444000_80137594,
        func_actor_444000_8013799C,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the grab enemy, by state: setup, bounce, rise, hold and
/// teardown.
const GpEnemyTaskFuncTable5 D_actor_444000_80131EA8 = {
    {
        func_actor_444000_80137D4C,
        func_actor_444000_801381B0,
        func_actor_444000_80138490,
        func_actor_444000_801389EC,
        Gp_DestroyEnemy,
    },
};

/// Settling state that follows the bounce: the step counter drives the whole
/// thing. When the dispatcher flags a state change the horizontal velocity is
/// cut to a ninth, both light modes are reset and the two display nodes drop
/// the draw flags the descent raised. Past x 0x4B65 in room 0x0427 the x
/// velocity is killed outright; for the first eight steps what is left of it
/// moves the model and is halved again each step. Steps 1, 2, 4, 8 and 20 puff
/// a `0x600A5` effect out of the model's coordinate, and 4 and 8 also switch
/// the light mode. After 0x51 steps both nodes are unlinked and the task steps
/// on; until then the two collision-record tables are wiped each step. The
/// model's own `workm` translation is handed to `Gp_UpdateActorColor`.
void func_actor_444000_8013928C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = task->work;
    VECTOR               pos;
    s16                  step;

    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        work->field_1AC = 0;
        work->vel.vx   /= 9;
        work->vel.vz   /= 9;
        Gp_SetLightMode(enemy, 0);
        Gp_SetLightMode(enemy, 1);
        work->obj1.flags                &= ~0x4000;
        work->obj0.flags                &= ~0x8000;
        ((TmdObject*)task->extra)->flags = 2;
    }

    work->field_1AC++;

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        ((TmdObject*)task->extra)->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    if (work->field_1AC < 8) {
        ((TmdObject*)task->extra)->coords->coord.t[0] += work->vel.vx;
        ((TmdObject*)task->extra)->coords->coord.t[2] += work->vel.vz;
        work->vel.vx                                 >>= 1;
        work->vel.vz                                 >>= 1;
        ((TmdObject*)task->extra)->coords->flg         = 0;
    }

    step = work->field_1AC - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(0x600A5, ((TmdObject*)task->extra)->coords, 1, NULL);
            Gp_SetLightMode(enemy, 2);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(0x600A5, ((TmdObject*)task->extra)->coords, 1, NULL);
            break;
    }

    if (work->field_1AC >= 0x51) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        task->state++;
    }

    if (work->field_1AC < 0x51) {
        Gp_ClearRec18Occupied(&work->rec1);
        Gp_ClearRec18Occupied(&work->rec0);
    }

    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// Horizontal gap from `coord` to the player's coordinate matrix `Player_Status.coordMtx`, as an
/// `SVECTOR` the caller supplies.
static __inline__ void Actor444000_GapToCamera(GsCOORDINATE2* coord, SVECTOR* out)
{
    out->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    out->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    out->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `D_actor_444000_80161690`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->target` is then the host model's position pushed out
/// by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on, measured
/// against the player -- plus the `[group][spawnArg1]` entry of
/// `D_actor_444000_80161744`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `D_actor_444000_80161704` offset, its work coordinate is parented to
/// `gGfxViewCoord` with an identity rotation and carries the single display
/// node, and the spawn cue is enqueued at the model's own pan and depth with
/// the owner's id in its high half. The trailing `Gp_SpawnEff` effect becomes
/// this task's parent so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_444000_80139594(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    GpEnemy*             owner;
    Task*                parent;
    Task*                player;
    OverlayMat*          mtx;
    SVECTOR              vec;
    s32                  dist;
    s32                  rnd;
    s32                  snd;
    s32                  pan;

    player = gameGetPtrSlot(3);
    owner  = task->parent->spawnArg2;
    parent = task->parent;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(Actor403200DropWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    work->field_1AA                        = 0;

    Actor444000_GapToCamera(((TmdObject*)task->extra)->coords, &vec);

    if ((u16)task->spawnArg1 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = ((u32)Gp_LcgState >> 16) & 3;
        switch (rnd) {
            case 0:
            case 1:
                D_actor_444000_80161690 = 1;
                break;
            case 2:
                D_actor_444000_80161690 = rnd;
                break;
            case 3:
                D_actor_444000_80161690 = 0;
                break;
            default:
                D_actor_444000_80161690 = 0;
                break;
        }
    }

    Actor444000_GapToCamera(((TmdObject*)parent->extra)->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x2710;
    } else {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x32C8;
    }

    work->target.vx +=
        D_actor_444000_80161744[D_actor_444000_801617C4[D_actor_444000_80161690]
                                                       [(u16)task->spawnArg1]]
            .vz;
    work->target.vy = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->target.vz = D_actor_444000_80161744[D_actor_444000_801617C4[D_actor_444000_80161690]
                                                                     [(u16)task->spawnArg1]]
                          .vx -
                      0x189C;
    work->target.vz = (((u32)Gp_LcgState >> 16) & 0x7F) + work->target.vz;

    if ((u16)task->spawnArg1 == 4) {
        work->target.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        work->target.vy = 0;
        work->target.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
    }

    work->timer     = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_1AE = ((u32)Gp_LcgState >> 16) & 8;

    vec.vx = D_actor_444000_80161704[(u16)task->spawnArg1].vx;
    vec.vy = D_actor_444000_80161704[(u16)task->spawnArg1].vy;
    vec.vz = D_actor_444000_80161704[(u16)task->spawnArg1].vz;

    ((TmdObject*)task->extra)->coords->coord.t[0] = vec.vx + ((TmdObject*)parent->extra)->coords->coord.t[0];
    ((TmdObject*)task->extra)->coords->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = vec.vz + ((TmdObject*)parent->extra)->coords->coord.t[2];

    work->obj.key = Gp_PackObjPair(owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    work->coord.sub    = &gGfxViewCoord;
    mtx                = (OverlayMat*)&work->coord.coord;
    mtx->ident.m00_m01 = 0x1000;
    mtx->ident.m02_m10 = 0;
    mtx->ident.m11_m12 = 0x1000;
    mtx->ident.m20_m21 = 0;
    mtx->ident.m22     = 0x1000;
    Gfx_RotMatrixY(&mtx->mat, 0, 1);

    actorLinkWorkObj(&work->coord, &work->obj, &work->rec, &vec, 0x100, 3, 1);
    work->obj.flags &= 0x7FFF;

    snd = ((owner->placeKey >> 12) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));

    work->eff = Gp_SpawnEff(0x6019B, ((TmdObject*)task->extra)->coords, 0, NULL);
    if (work->eff != NULL) {
        Task_Reparent(task, work->eff->task);
    }
    task->state++;
}

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `field_1AE` a step until it passes -0x4E20, then clamp it there, snap its
/// horizontal position back onto `work->target`, restart the step counter, pick
/// a fresh 0..0x1F bias for the next leg, flag the list object and step the task
/// on. Either way the work block's own coordinate is left tracking the model.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_444000_80139AF8(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    s32                  y;

    work = task->work;
    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    y                                             = ((TmdObject*)task->extra)->coords->coord.t[1] - 0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[1] = y - work->field_1AE;
    if (((TmdObject*)task->extra)->coords->coord.t[1] < -0x4E20) {
        task->state++;
        ((TmdObject*)task->extra)->coords->coord.t[0] = work->target.vx;
        ((TmdObject*)task->extra)->coords->coord.t[2] = work->target.vz;
        Gp_LcgState                                   = Gp_LcgState * 5 + 0x71357911;
        ((TmdObject*)task->extra)->coords->coord.t[1] = -0x4E20;
        work->timer                                   = 0;
        work->field_1AE                               = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                              |= 0x8000;
    }

    ((TmdObject*)task->extra)->coords->flg = 0;
    work->coord.coord.t[0]                 = ((TmdObject*)task->extra)->coords->coord.t[0];
    work->coord.coord.t[1]                 = ((TmdObject*)task->extra)->coords->coord.t[1];
    work->coord.coord.t[2]                 = ((TmdObject*)task->extra)->coords->coord.t[2];
    work->coord.flg                        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Descent state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + field_1AE` a step. When
/// it reaches floor level, zero the height, restart the step counter, tell the
/// trailing `Gp_SpawnEff` effect to wind down, play the landing cue and step
/// the task on. Either way the work block's own coordinate is left tracking
/// the model. Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_444000_80139C80(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    Actor403200DropCoord coord;
    MATRIX*              mtx;
    GpEnemy*             owner;
    s32                  snd;
    s32                  pan;

    work = task->work;
    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->timer++;
    coord.c.sub          = &gGfxViewCoord;
    mtx                  = &coord.c.coord;
    coord.ident.m00_m01  = 0x1000;
    coord.ident.m02_m10  = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    coord.ident.m20_m21  = 0;
    mtx->m[2][2]         = 0x1000;
    Gfx_RotMatrixY(mtx, 0, 1);

    coord.c.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
    coord.c.coord.t[1] = 0;
    coord.c.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
    coord.c.flg        = 0;
    Gp_UpdateCoord(&coord.c);

    Gp_DrawEffGroundQuad((VECTOR3*)coord.c.workm.t, (s16)((s16)work->timer * 8 + 0x80),
                         Gp_State1C->groundShade);

    if ((s16)work->timer >= 0x14) {
        ((TmdObject*)task->extra)->coords->coord.t[1] =
            ((TmdObject*)task->extra)->coords->coord.t[1] + (work->field_1AE + 0x258);
        if (((TmdObject*)task->extra)->coords->coord.t[1] > 0) {
            owner                                         = task->parent->spawnArg2;
            ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
            work->timer                                   = 0;
            if (work->eff != NULL) {
                work->eff->task->spawnArg1 = 2;
            }
            task->state++;
            snd = ((owner->placeKey >> 12) << 8) | 0x4020000C;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        }
    }

    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
    work->coord.coord.t[1] = ((TmdObject*)task->extra)->coords->coord.t[1];
    work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// State handlers of the enemy `func_actor_444000_80143A6C` dispatches: spawn,
/// descent, settle and teardown.
const GpEnemyTaskFuncTable4 D_actor_444000_80131F0C = {
    {
        func_actor_444000_80138B94,
        func_actor_444000_80138FC4,
        func_actor_444000_8013928C,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the dropped enemy: spawn, ascent, descent, landing and
/// teardown.
const GpEnemyTaskFuncTable5 D_actor_444000_80131F1C = {
    {
        func_actor_444000_80139594,
        func_actor_444000_80139AF8,
        func_actor_444000_80139C80,
        func_actor_444000_80139EE4,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the spinner enemy: spawn, hidden wait, chase and teardown.
const GpEnemyTaskFuncTable4 D_actor_444000_80131F30 = {
    {
        func_actor_444000_8013A1C4,
        func_actor_444000_80143BFC,
        func_actor_444000_8013A3AC,
        Gp_DestroyEnemy,
    },
};

/// Landing state of the dropped enemy, the one after the descent in
/// `D_actor_444000_80131F1C`: each step rebuild the model's rotation about y
/// from its own yaw and flatten it through a 0x34-byte block borrowed from the
/// scratchpad -- scaled (`0x4000, 0x66, 0x4000`) for the first 0xA steps, then
/// (`0x4C00, 0x199, 0x4C00`). The collision object's radius grows with the step
/// counter over the first steps and is then held at 0x380. After 0xC steps the
/// object is unlinked and the task steps on; either way the work block's
/// coordinate keeps tracking the model.
///
/// The absolute `G_SCRATCH_HEAD` accesses are written out: at `-O2` the
/// expander forces a constant address into a register (`explow.c`
/// `memory_address`), so the `lui $at` assembler-macro form the original
/// carries cannot come from plain C here.
void func_actor_444000_80139EE4(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork*  work;
    TmdObject*            extra;
    GsCOORDINATE2*        coord;
    ActorScaleRotScratch* blk;
    u8*                   head;
    s16                   ang;

    work = task->work;
    work->timer++;
    if ((s16)work->timer < 0xA) {
        u8* tail;
        s16 spin;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4000;
        blk->scale.vy = 0x66;
        blk->scale.vz = 0x4000;
        ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        m22                  = *(u16*)&blk->m.m[2][2];
        coord->flg           = 0;
        coord->coord.m[2][2] = m22;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail = *(u8**)(tail + 0x3FC);
        tail = tail + 0x34;
        spin = (s16)work->timer * 0x40 + 0x100;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        TOUCH_REG(tail);
        work->obj.radius = spin;
    } else {
        u8* tail;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4C00;
        blk->scale.vy = 0x199;
        blk->scale.vz = 0x4C00;
        ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail       = *(u8**)(tail + 0x3FC);
        m22        = *(u16*)&blk->m.m[2][2];
        coord->flg = 0;
        tail       = tail + 0x34;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        coord->coord.m[2][2] = m22;
        work->obj.radius     = 0x380;
    }

    Gp_ClearRec18Occupied(&work->rec);
    if ((s16)work->timer >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        task->state++;
    }

    work->coord.coord.t[0] = ((TmdObject*)task->extra)->coords->coord.t[0];
    work->coord.coord.t[1] = ((TmdObject*)task->extra)->coords->coord.t[1];
    work->coord.coord.t[2] = ((TmdObject*)task->extra)->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F30`:
/// allocate its `Actor403200SpinnerWork`, parent the model object to the world
/// coordinate, give it a random orientation off `Gp_LcgState`, point it at its
/// own light and colour matrices and step the task on. Bails to
/// `Gp_DestroyEnemy` when the overlay is shutting down or the allocation fails.
void func_actor_444000_8013A1C4(GpEnemy* enemy, Task* task)
{
    Actor403200SpinnerWork* work;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(0xA0, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    ((TmdObject*)task->extra)->flags       = 0;

    switch ((u16)task->spawnArg1) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->msgTable = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    ((TmdObject*)task->extra)->lightMtx    = &work->lightMtx;
    ((TmdObject*)task->extra)->colorMtx    = &work->colorMtx;
    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    func_800D7A9C(task->extra, (VECTOR*)((TmdObject*)task->extra)->coords->workm.t, 0, 3);
    task->state++;
}

/// Per-tick state of the spinner enemy. While `spin` is counting down the model
/// only yaws in place -- 0x40 on phase 1 and -0x3C on phase 3 of every four
/// frames -- and nothing else happens. Once it reaches zero the enemy homes on
/// `D_actor_444000_80161890`: the offset from the model root to that point is
/// squared against `field_98` in a `VECTOR3` borrowed off `G_SCRATCH_HEAD`, and
/// the task steps on when the enemy is inside that radius. `field_96` then ties
/// the spin rate to the step count (`field_98 += field_96 / 8`), the offset is
/// normalised and scaled by `field_98` through the GTE's `gpf` interpolator, and
/// the result is added to the root translation before the three rotations are
/// rebuilt from `field_98` and `field_96`. Bails to `Gp_DestroyEnemy` while the
/// overlay is shutting down.
void func_actor_444000_8013A3AC(GpEnemy* enemy, Task* task)
{
    Actor403200SpinnerWork* work;
    SVECTOR                 step;
    SVECTOR*                stepp;
    VECTOR3*                sq;
    u8*                     head;
    s16                     angle;
    s32                     spin;
    s32                     phase;
    s32                     inside;

    work                                   = task->work;
    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    func_800D7A9C(task->extra, (VECTOR*)((TmdObject*)task->extra)->coords->workm.t, 0, 3);

    if (D_actor_444000_80144A68 == 1 || D_actor_444000_80144A72 == 0) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->flags = 0;

    spin = work->spin;
    if (spin != 0) {
        spin--;
        work->spin = spin;
        phase      = work->spin;
        if ((phase & 3) == 1) {
            Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x40, 0);
        }
        if ((work->spin & 3) == 3) {
            Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, -0x3C, 0);
        }
        ((TmdObject*)task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
        return;
    }

    stepp    = &step;
    *stepp   = D_actor_444000_80161890;
    step.vx -= ((TmdObject*)task->extra)->coords->coord.t[0];
    step.vy -= ((TmdObject*)task->extra)->coords->coord.t[1];
    step.vz -= ((TmdObject*)task->extra)->coords->coord.t[2];

    head = actorGetScratchHead();
    sq   = (VECTOR3*)(head - sizeof(VECTOR3));
    actorSetScratchHead(sq);
    angle  = work->field_98;
    sq->vx = step.vx;
    sq->vy = stepp->vz;
    sq->vz = angle;
    sq->vx = sq->vx * sq->vx;
    sq->vy = sq->vy * sq->vy;
    sq->vz = sq->vz * sq->vz;
    actorSetScratchHead(head);
    inside = sq->vx + sq->vy >= sq->vz;
    if (!inside) {
        task->state++;
    }

    work->field_96++;
    work->field_98 += work->field_96 / 8;
    VectorNormalSS(stepp, stepp);

    gte_lddp((u16)work->field_98);
    gte_ldsv(stepp);
    gte_gpf12();
    gte_stsv(stepp);

    ((TmdObject*)task->extra)->coords->coord.t[0] += step.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] += step.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] += step.vz;
    ((TmdObject*)task->extra)->coords->flg         = 0;

    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, work->field_98 / 2, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, work->field_98 * 2, 0);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, work->field_96, 0);
}

/// Screen-shake driver for the enemy task: `func_actor_444000_80143490` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `Display_ClampField126` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out.
void func_actor_444000_8013A77C(Task* task)
{
    Actor444000Work* work = task->work;
    s32              phase;

    if (work->field_EAC != work->field_EAD) {
        switch (work->field_EAC) {
            case 1:
                work->field_EAE = 5;
                break;
            case 2:
                work->field_EAE = 0xA;
                break;
            case 3:
                work->field_EAE = 0x16;
                break;
            case 0:
            default:
                return;
        }
        work->field_EAD = work->field_EAC;
    }

    if (work->field_EAE == 0) {
        Display_ClampField126(0);
        work->field_EAC = 0;
        work->field_EAD = 0;
        return;
    }
    work->field_EAE--;

    switch (work->field_EAC) {
        case 1:
            phase = work->field_EAE;
            if ((phase & 1) == 0) {
                work->field_EAF = 0;
            } else {
                work->field_EAF = 2;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 2:
            phase = work->field_EAE;
            switch (phase & 3) {
                case 0:
                    work->field_EAF = 0;
                    break;
                case 1:
                    work->field_EAF = 2;
                    break;
                case 2:
                    work->field_EAF = 3;
                    break;
                case 3:
                    work->field_EAF = 2;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 3:
            phase = work->field_EAE;
            switch (phase & 7) {
                case 3:
                case 4:
                    work->field_EAF = 4;
                    break;
                case 2:
                case 5:
                    work->field_EAF = 3;
                    break;
                case 1:
                case 6:
                    work->field_EAF = 1;
                    break;
                case 0:
                case 7:
                    work->field_EAF = 0;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 0:
        default:
            Display_ClampField126(0);
            break;
    }
}

/// Message 0x7D5 handler, the visibility control the event task drives the
/// boss with: each sub-command sets the host model's flag word and pushes it
/// onto all seven escorts' models, differing in what the flag word becomes and
/// whether the model buffers are (re)allocated first.
///
/// 0 brings the group back with buffers and the 0x80 flag, 1 clears the flag
/// before making sure the buffers exist, 2 raises bit 2 and reports it back
/// through the flag word, and 3 clears the word, pushes the clear, then raises
/// bit 2 on the host alone. The two that end with a cleared flag word also
/// reset the work block's state index.
s32 func_actor_444000_8013A958(Task* task, s32 msgId, s32 arg2)
{
    TmdObject*       tmd;
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    Actor444000Work* rebuilt;
    TmdObject*       hostTmd;
    TmdObject*       escortTmd;
    s32              flags;
    s16              i;
    s16              j;

    tmd  = (TmdObject*)task->extra;
    work = task->work;
    switch (arg2) {
        case 0:
            buffers = task->work;
            if (tmd->buffer == NULL) {
                Tmd_AllocBuffers(tmd);
            }
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            escorts                          = task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                          = task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            hostTmd = (TmdObject*)task->extra;
            rebuilt = task->work;
            if (hostTmd->buffer == NULL) {
                Tmd_AllocBuffers(hostTmd);
            }
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)rebuilt->field_ECC[j]->task->extra;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            break;
        case 2:
            tmd->flags |= 4;
            flags       = tmd->flags;
            escorts     = task->work;
            if (flags & 4) {
                escorts->field_7F3               = 3;
                ((TmdObject*)task->extra)->flags = 0x80;
            } else {
                escorts->field_7F3               = 0;
                ((TmdObject*)task->extra)->flags = flags;
            }
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            tmd->flags                       = 0;
            escorts                          = task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            tmd->flags |= 4;
            break;
    }
    return 0;
}

/// Rebuilds the host's root coordinate around the yaw it is already facing:
/// `ratan2` of the rotation's Z basis gives the yaw, `Gfx_RotMatrixY` rebuilds
/// the rotation from it, and `ScaleMatrix` widens it to 1.0 / 0.0 / 1.0 so the
/// model flattens vertically. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back before the coordinate is refreshed.
static __inline__ void Actor444000_RebuildRotation(Task* task)
{
    GsCOORDINATE2*        coord = ((TmdObject*)task->extra)->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = 0;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    ((TmdObject*)task->extra)->coords->flg = 0;
    *(u8**)G_SCRATCH_HEAD                  = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
}

/// The enemy task's 0x7DB message handler, listed in `D_actor_444000_80161818`.
/// The three payload bytes are always recorded in the work block; only messages
/// from sender 0x2804 act, and then on three of the selector's values. 0 and 1
/// both announce the state change with the same pair of cues, 1 additionally
/// re-arms the animation blocks and drops the model onto its start position,
/// and 19 switches the host and its fourth escort to light mode 2 before
/// raising eight floor vertices and flattening the model's rotation.
s32 func_actor_444000_8013ACD0(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor444000Work* work  = task->work;
    GpEnemy*         enemy = task->spawnArg2;
    SVECTOR*         verts;
    s32              action;

    work->field_EC4 = msg->from.loc.stage;
    work->field_EC5 = msg->from.loc.area;
    work->field_EC6 = (u8)msg->command;

    if (msg->from.key == 0x2804) {
        action = msg->command;
        switch (action) {
            case 0:
                work->field_0 = 0;
                SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000D, 1);
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                func_actor_444000_8013441C(task);
                func_actor_444000_8013441C(task);
                func_actor_444000_8013441C(task);
                work->field_7B6 = 1;
                func_actor_444000_8013441C(task);
                work->field_7B6                               = 0x10;
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0xBB8;
                ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x992;
                ((TmdObject*)task->extra)->coords->flg        = 0;
                work->field_0                                 = 0x11;
                SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000D, 1);
                break;

            case 19:
                Gp_SetLightMode(enemy, 2);
                Gp_SetLightMode(work->field_ECC[3], 2);
                work->field_0   = action;
                work->field_2   = -1;
                work->field_F04 = 1;
                SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x54280007, 1);

                verts        = Gp_GridParams->field_8;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;

                Actor444000_RebuildRotation(task);
                break;
        }
    }
    return 1;
}

/// Rebuilds the host's root coordinate from its own facing yaw with a uniform
/// 1.0 scale, marks the model for a rebuild and arms the first state. The
/// matrix lives in a frame taken off `G_SCRATCH_HEAD`, which is handed back
/// once the rotation has been copied out; inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void Actor444000_SeedRootCoord(Task* task, Actor444000Work* work)
{
    GsCOORDINATE2*        coord = ((TmdObject*)task->extra)->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    work->field_0                    = 1;
    ((TmdObject*)task->extra)->flags = 0;
    *(u8**)G_SCRATCH_HEAD            = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
}

/// Spawn state of the arena boss: allocate its `Actor444000Work`, wire the host
/// enemy up to the model's root coordinate and its nine collision objects, then
/// spawn the seven escorts that make up the rest of the creature.
///
/// The host takes collision groups 0..2 (group 0's record table doubles as its
/// own `GpEnemy::recs`), escort 0 takes 3..5 and escort 1 takes 6..8; each
/// group is armed through `func_8010C980` on one of the model's part
/// coordinates. Escorts 2..5 are parented to a part coordinate at a fixed
/// offset and nothing else, and escort 6 is only spawned when the high half of
/// `Task::spawnArg1` is clear. Escort 3 is the one the colour updates treat as
/// the host's twin, so it shares the host's group-1 record table.
///
/// The root coordinate is flattened to its own yaw with a uniform 1.0 scale
/// (`Actor444000_SeedRootCoord`), the tenth collision object at `obj` is linked
/// by hand around the free coordinate `field_E3C`, and both the host and every
/// escort model are pointed at the work block's light and colour matrices
/// before the fight announces itself with message 0x7DA.
void func_actor_444000_8013AFF8(GpEnemy* enemy, Task* task)
{
    Actor444000Work* work;
    Actor444000Work* buffers;
    Actor444000Work* escorts;
    OverlayMat*      mtx;
    TmdObject*       tmd;
    TmdObject*       model;
    TmdObject*       escortTmd;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   freeCoord;
    GpEnemy*         esc;
    Task*            escTask;
    SVECTOR          dir;
    SVECTOR*         gteDir;
    VECTOR           pos;
    s16              i;
    s16              j;
    s16              k;

    tmd   = (TmdObject*)task->extra;
    coord = tmd->coords;

    work       = memCalloc(0xF24, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    task->exitCallback = func_actor_444000_801433B8;

    enemy->field_4    = &((TmdObject*)task->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = -0xC8;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->reactionFlags = 0;
    enemy->hp            = D_actor_444000_80144A28.hpMax;
    enemy->param         = &D_actor_444000_80144A28;
    enemy->recs          = work->hits[0].recs;

    func_800B3F84(&work->anim0, D_actor_444000_80161448, tmd, work->aux0, work->slots0);
    func_800B3F84(&work->anim1, D_actor_444000_80161448, tmd, work->aux1, work->slots1);

    work->field_7B0 = 2;
    work->field_7B3 = 2;
    work->field_EF8 = 1;
    work->field_7B1 = 0;
    work->field_7C4 = work->field_7C8 = 0;
    work->field_7B6 = work->field_7B8 = 0x10;

    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[0].obj, work->hits[0].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[1].obj, work->hits[1].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[1], &work->hits[2].obj, work->hits[2].recs, 5, 0x20, 0xBB8);

    work->hits[1].obj.pos.vx = 0;
    work->hits[1].obj.pos.vy = 0;
    work->hits[1].obj.pos.vz = -0x100;
    work->hits[2].obj.pos.vx = 0;
    work->hits[2].obj.pos.vy = 0x400;
    work->hits[2].obj.pos.vz = -0x400;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12();
    gte_stsv(gteDir);

    work->anim.animBlock.ptr = NULL;
    work->anim.field_4       = 1;
    work->anim.field_8       = 0;
    work->anim.field_C       = 3;
    work->anim.field_10      = 1;
    work->field_F12          = 0;
    task->msgTable           = &D_actor_444000_80161818;
    coord->sub               = &gGfxViewCoord;
    coord->flg               = 0;
    Gp_UpdateCoord(coord);

    D_actor_444000_80161880.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_444000_80161880.spawnArgLo = 0x100;
    D_actor_444000_80161880.spawnArgHi = 2;
    work->field_2                      = -1;

    model   = (TmdObject*)task->extra;
    buffers = task->work;
    if (model->buffer == NULL) {
        Tmd_AllocBuffers(model);
    }
    for (i = 0; i < 7; i++) {
        if (buffers->field_ECC[i] != NULL) {
            escortTmd = (TmdObject*)buffers->field_ECC[i]->task->extra;
            if (escortTmd->buffer == NULL) {
                Tmd_AllocBuffers(escortTmd);
            }
        }
    }

    Actor444000_SeedRootCoord(task, work);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 0, 0, task->spawnArg2);
    work->field_ECC[0]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim2, D_actor_444000_80161500, work->field_ECC[0]->task->extra, work->aux2,
                  work->slots2);
    func_800B3F84(&work->anim3, D_actor_444000_80161500, work->field_ECC[0]->task->extra, work->aux3,
                  work->slots3);
    work->field_ECC[0]->field_4    = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[0]->field_48   = 0;
    work->field_ECC[0]->bodyPos.vx = 0xC8;
    work->field_ECC[0]->bodyPos.vy = 0;
    work->field_ECC[0]->bodyPos.vz = 0x3E8;
    work->field_ECC[0]->coord      = &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[0]->node);
    work->field_ECC[0]->reactionFlags = 0;
    work->field_ECC[0]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0A                   = D_actor_444000_80144A38.hpMax;
    work->field_ECC[0]->param         = &D_actor_444000_80144A38;
    work->field_ECC[0]->recs          = work->hits[3].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], &work->hits[3].obj, work->hits[3].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[2], &work->hits[4].obj, work->hits[4].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[3], &work->hits[5].obj, work->hits[5].recs, 5,
                  0x20, 0x300);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 1, 0, task->spawnArg2);
    work->field_ECC[1]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim4, D_actor_444000_801615B8, work->field_ECC[1]->task->extra, work->aux4,
                  work->slots4);
    func_800B3F84(&work->anim5, D_actor_444000_801615B8, work->field_ECC[1]->task->extra, work->aux5,
                  work->slots5);
    work->field_ECC[1]->field_4    = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[1]->field_48   = 0;
    work->field_ECC[1]->bodyPos.vx = -0xC8;
    work->field_ECC[1]->bodyPos.vy = 0;
    work->field_ECC[1]->bodyPos.vz = 0x3E8;
    work->field_ECC[1]->coord      = &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[1]->node);
    work->field_ECC[1]->reactionFlags = 0;
    work->field_ECC[1]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0C                   = D_actor_444000_80144A48.hpMax;
    work->field_ECC[1]->param         = &D_actor_444000_80144A48;
    work->field_ECC[1]->recs          = work->hits[6].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], &work->hits[6].obj, work->hits[6].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[2], &work->hits[7].obj, work->hits[7].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[3], &work->hits[8].obj, work->hits[8].recs, 5,
                  0x20, 0x300);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 2, 0, task->spawnArg2);
    work->field_ECC[2]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] = 0x59;
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[2] = -0x64;
    ((TmdObject*)work->field_ECC[2]->task->extra)->flags              = 0;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 3, 0, task->spawnArg2);
    work->field_ECC[3]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[3];
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->flags              = 0;
    work->field_ECC[3]->field_4                                       = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[3]->field_48                                      = 0;
    work->field_ECC[3]->bodyPos.vx                                    = 0;
    work->field_ECC[3]->bodyPos.vy                                    = 0;
    work->field_ECC[3]->bodyPos.vz                                    = 0x514;
    work->field_ECC[3]->coord                                         = ((TmdObject*)work->field_ECC[3]->task->extra)->coords;
    Gp_LinkNode(&work->field_ECC[3]->node);
    work->field_ECC[3]->reactionFlags = 0;
    work->field_ECC[3]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0E                   = D_actor_444000_80144A58.hpMax;
    work->field_ECC[3]->param         = &D_actor_444000_80144A58;
    work->field_ECC[3]->recs          = work->hits[1].recs;

    freeCoord                     = &work->field_E3C.c;
    work->field_E3C.c.sub         = ((TmdObject*)task->extra)->coords;
    work->field_E3C.ident.m00_m01 = 0x1000;
    mtx                           = (OverlayMat*)&work->field_E3C.c.coord;
    mtx->ident.m02_m10            = 0;
    mtx->ident.m11_m12            = 0x1000;
    mtx->ident.m20_m21            = 0;
    mtx->ident.m22                = 0x1000;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.flg                                                                      = 0;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.end1.vz    = 0x1B58;
    work->d4rec.end0Radius = 0x258;
    work->d4rec.end1Radius = 0x258;
    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = 0;
    work->d4rec.recs       = work->recs2;
    work->obj.coord        = freeCoord;
    work->obj.ctx.d4rec    = &work->d4rec;
    work->obj.pos.vx       = 0;
    work->obj.pos.vy       = -0xFA;
    work->obj.pos.vz       = 0x25F;
    work->obj.key          = 0x30000 | 0x20;
    work->obj.radius       = 0;
    work->obj.flags        = 3;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->recs2, 5, 0);
    work->obj.flags &= 0x7FFF;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 4, 0, task->spawnArg2);
    work->field_ECC[4]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[2] = 0x14;
    ((TmdObject*)work->field_ECC[4]->task->extra)->flags              = 0;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 5, 0, task->spawnArg2);
    work->field_ECC[5]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[2];
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[1] = 0x67C;
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[2] = 0xC8;
    ((TmdObject*)work->field_ECC[5]->task->extra)->flags              = 0;

    if ((task->spawnArg1 >> 16) == 0) {
        esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 6, 0, task->spawnArg2);
        work->field_ECC[6]                                                = esc;
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[1];
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[1] = 0x62C;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[2] = 0x5DC;
        ((TmdObject*)work->field_ECC[6]->task->extra)->flags              = 0;
    } else {
        work->field_ECC[6] = NULL;
    }

    work->field_F04 = 0;
    work->field_F06 = 0;
    work->field_F08 = 0;
    work->field_F0C = D_actor_444000_80144A4C[0];
    work->field_F0A = D_actor_444000_80144A3C[0];

    escorts = task->work;

    D_actor_444000_80144A68 = 0;

    ((TmdObject*)task->extra)->lightMtx = &escorts->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &escorts->colorMtx;
    for (j = 0; j < 7; j++) {
        esc = escorts->field_ECC[j];
        if (esc != NULL) {
            escTask                                = esc->task;
            ((TmdObject*)escTask->extra)->lightMtx = &escorts->lightMtx;
            ((TmdObject*)escTask->extra)->colorMtx = &escorts->colorMtx;
        }
    }

    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    if (work->field_EFA != 0) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
    } else {
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
    }
    func_actor_444000_8013441C(task);

    D_actor_444000_80161888.from.loc.stage = 0;
    D_actor_444000_80161888.from.loc.area  = 0x2C;
    D_actor_444000_80161888.command        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);

    work->field_E94 = work->field_E96 = 0xFA0;
    for (k = 0; k < 2; k++) {
        work->field_EE8[k] = NULL;
    }

    Gp_StateF0.field_6 = 0xA;
    Gp_ReleaseStateF0Add(task, 0x20);
    D_actor_444000_80161878 = task;
    work->field_F1B = work->field_F1C = 0;
    task->state                      += 1;
}

/// Applies the single hit collision group 0 took this frame: the first of the
/// group's five `GpRec18` records holding a type-2 attack id wins, and its
/// contact point drives the hit effect `func_actor_444000_80134688` spawns on
/// the group's coordinate. Damage is `Gp_ComputeDamage` of the attack id scaled
/// by the player's distance, quadrupled on a `Gp_RollEnemyChance` critical and
/// doubled again before it is reported, and the leftover `field_4C` bits 0xC
/// tick a second helping off through `Gp_TickObjFlag4`. The scratchpad frame
/// also keeps the yaw from the part's facing to the contact point, wrapped into
/// +/-0x800, which the drive step reads back at 0x2C.
///
/// The record scan is written with labels rather than a `for` loop on purpose:
/// as a real loop, GCC's `find_and_verify_loops` moves the match arm out of
/// line (see DECOMPILATION_LEARNINGS.md, "loop.c relocates a loop block that
/// ends in a jump out"). `mask` / `kind` and the two hoisted pointers then have
/// to be spelled out, since nothing lifts them out of a goto loop.
void func_actor_444000_8013C060(Task* task)
{
    Actor403200HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               enemy;
    GpRec18*               recs;
    PlayerStatus*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;

    cfg   = &Player_Status;
    enemy = task->spawnArg2;
    work  = task->work;
    sc    = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        func_actor_444000_80134688(work->hits[0].obj.coord, id);
        work->field_E8C = Gp_GetIdParam2(sc->id);
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->rot.vy = 0x190;
            sc->rot.vx = 0;
            sc->rot.vz = 0x1F4;
            Gp_SpawnEff(0x6009C, &((TmdObject*)enemy->task->extra)->coords[3], 3, &sc->rot);
        }
        ((TmdObject*)task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
        sc->rot.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
        sc->rot.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        sc->rot.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        sc->rot.vx = sc->pos.vx - ((TmdObject*)task->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)task->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)task->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                       ((TmdObject*)task->extra)->coords->workm.m[2][2]);
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        sc->angle = angle;

        if (work->field_7B3 != 4) {
            work->field_7C8 = 0;
            work->field_7C4 = 0;
        }
        sc->damage *= 2;
        func_800E2C78(enemy, sc->id, sc->damage, 0);
        enemy->hp -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
    }

    if (enemy->reactionFlags & 0xC) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
        enemy->hp -= sc->damage;
    }

    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `func_actor_444000_8013C060` runs for group 0, done twice: group 1 first,
/// and group 2 only if nothing landed on group 1. Both scans are written as
/// real `for` loops so `find_and_verify_loops` parks the match arm out of line
/// (see DECOMPILATION_LEARNINGS.md, "loop.c relocates a loop block that ends in
/// a jump out"), which is the opposite of what group 0's handler needed, and
/// the second scan needs its own locals: sharing `recs` / `pos` / `i` with the
/// first gives both loops one pseudo each and the wrong registers.
///
/// A hit takes the boss out of its 3 / 9 states into 8 on attack kind 2, and on
/// kinds 4 and 6 only one time in six. The damage is computed from how far the
/// player is from the host model, quadrupled when `Gp_RollEnemyChance` fires
/// and the boss is not already in one of the five states that ignore it, and
/// zero otherwise; it comes off the host, the fourth escort and the work
/// block's own pool. `sc->angle` is the yaw of the contact point relative to
/// that escort's facing, wrapped to +/-0x800.
void func_actor_444000_8013C4B0(Task* task)
{
    Actor403200HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    u16                    roll;

    cfg  = &Player_Status;
    host = task->spawnArg2;
    work = task->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    coord = work->hits[2].obj.coord;
hit:
    func_actor_444000_80134688(coord, id);
    if (sc->id != 0) {
        work->field_E92 = Gp_GetIdParam2(sc->id);
        switch (Gp_GetIdParam0(sc->id) & 0xFFFF) {
            case 0:
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 9:
                break;

            case 4:
            case 6:
                state = work->field_0;
                if (state != 3 && state != 9) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    roll        = ((u32)Gp_LcgState >> 16) % 6;
                    if (roll == 0) {
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
                state = work->field_0;
                if (state != 3 && state != 9) {
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_ECC[3]->task->extra)->coords, 0, &sc->rot);
            if ((s16)work->field_0 != 9) {
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
        } else {
            sc->damage = 0;
        }

        func_800E2C78(host, sc->id, sc->damage, 0);
        host->hp -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E                                           -= sc->damage;
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)work->field_ECC[3]->task->extra)->coords);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                       ((TmdObject*)task->extra)->coords->workm.m[2][2]);
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        sc->angle = angle;

        if (work->field_7B3 != 4) {
            work->field_7C8 = 0;
            work->field_7C4 = 0;
        }
    }
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 3, 4 and 5 -- `func_actor_444000_8013C4B0`
/// done three times, the next group only scanned when the previous one landed
/// nothing and the part it hit reported no attack id back. Unlike groups 1 and 2
/// this one runs no `Gp_GetIdParam0` switch: the call is made and its kind
/// thrown away, so every hit is treated alike.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the first escort and `field_F0A`. Emptying that pool
/// spawns the same effect again and refills it from
/// `D_actor_444000_80144A3C`. Both effect spawns and the state change to 0xE
/// are skipped while the boss is in one of the seven states that ignore hits,
/// while `field_F08` is clear, or while the player hold is armed.
void func_actor_444000_8013CA60(Task* task)
{
    Actor403200HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;

    cfg  = &Player_Status;
    host = task->spawnArg2;
    work = task->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[3].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[4].obj.coord;
    hit:
        func_actor_444000_80134688(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    func_actor_444000_80134688(work->hits[5].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    work->field_E8E = Gp_GetIdParam2(sc->id);
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x1F4;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    work->field_F0A -= sc->damage;
    if (work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = D_actor_444000_80144A3C[0];
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[0]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                   ((TmdObject*)task->extra)->coords->workm.m[2][2]);
    sc->angle = angle;
    if (angle < 0) {
    wrapUp3:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp3;
        }
    } else {
    wrapDown3:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown3;
        }
    }
    sc->angle = angle;

    if (work->field_7B3 != 4) {
        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 6, 7 and 8 -- the same three-scan shape
/// as `func_actor_444000_8013CA60` runs for groups 3, 4 and 5, with the next
/// group only scanned when the previous one landed nothing and the part it hit
/// reported no attack id back. `Gp_GetIdParam0` is called and its kind thrown
/// away here too.
///
/// Damage is the distance-scaled hit -- measured from an offset point rather
/// than the model origin -- quadrupled when `Gp_RollEnemyChance` fires, then
/// divided by six (never down to zero unless it already was), and comes off the
/// host, the second escort and `field_F0C`. Emptying that pool spawns the same
/// effect again and refills it from `D_actor_444000_80144A4C`. Both effect
/// spawns and the state change to 0xE are skipped while the boss is in one of
/// the seven states that ignore hits, while `field_F08` is clear, or while the
/// player hold is armed.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the *first* escort's facing.
void func_actor_444000_8013D128(Task* task)
{
    Actor403200HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;

    cfg  = &Player_Status;
    host = task->spawnArg2;
    work = task->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[6].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[7].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[7].obj.coord;
    hit:
        func_actor_444000_80134688(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    func_actor_444000_80134688(work->hits[8].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    work->field_E90 = Gp_GetIdParam2(sc->id);
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0]) - 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[1], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        sc->damage     *= 4;
        work->field_0   = 0xE;
        work->field_F0C = D_actor_444000_80144A4C[0];
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->id, sc->damage, 0);
    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    host->hp        -= sc->damage;
    work->field_F0C -= sc->damage;
    if (work->field_F0C <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0C = D_actor_444000_80144A4C[0];
    }

    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[1]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                   ((TmdObject*)task->extra)->coords->workm.m[2][2]);
    sc->angle = angle;
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    sc->angle = angle;

    if (work->field_7B3 != 4) {
        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// Reset/teardown handler: when the work block is asking for a reset, arm the
/// re-spawn sequence and push the host's model flag word onto each of the seven
/// escorts' models. Otherwise run the ordinary re-arm while the sub-state
/// counter is still below 0xA, and once it reaches 2 release the host's and
/// every escort's model buffers.
void func_actor_444000_8013D810(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* dying;
    s16              i;
    s16              j;

    work = arg0->work;
    if (work->field_4 != 0) {
        escorts                          = arg0->work;
        work->field_7F3                  = 3;
        ((TmdObject*)arg0->extra)->flags = 0x80;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_7B3 = 0xA;
        work->field_7B0 = 2;
        work->field_6   = 0;
        work->field_7B6 = 0x10;
        func_actor_444000_8013441C(arg0);
    } else {
        if (work->field_6 < 0xA) {
            func_actor_444000_8013441C(arg0);
        }
        if (work->field_6 == 2) {
            dying = arg0->work;
            Tmd_FreeBuffers((TmdObject*)arg0->extra);
            for (j = 0; j < 7; j++) {
                if (dying->field_ECC[j] != NULL) {
                    Tmd_FreeBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
                }
            }
        }
    }
}

/// Hands the scratchpad frame `Actor444000_FlattenRotation` borrowed back to
/// `G_SCRATCH_HEAD`. Written as an inline like the rotation itself: only
/// inline-expanded code keeps the absolute `lui $at` form of the scratch-head
/// accesses, so a release written straight into the caller does not match.
static __inline__ void Actor444000_ReleaseRotScratch(void)
{
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(ActorScaleRotScratch);
}

/// Rebuilds one model's root coordinate around the yaw it already faces and
/// flattens it vertically: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// 1.0 / `vy` / 1.0. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`; the caller releases it with
/// `Actor444000_ReleaseRotScratch` once it has cleared the coordinate again.
static __inline__ void Actor444000_FlattenRotation(GsCOORDINATE2* coord, s32 vy)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                      = (ActorScaleRotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(ActorScaleRotScratch));
    *(ActorScaleRotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = vy;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;
}

/// Re-arm handler run once the block asks for a reset: clear the host's model
/// flag word onto itself and every escort, drop the two counters at 0xEF4, then
/// step the animation on and flatten six of the models -- escorts 2, 4, 3, 0 and
/// 1 plus the host itself -- onto the ground plane. Escort 3 keeps a little
/// height (`vy` 0x400) where the rest are flattened outright. The last release
/// clears escort 2's coordinate flag again rather than escort 1's, which looks
/// like a copy-paste slip in the original but is what the ROM does.
void func_actor_444000_8013D96C(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    s16              i;

    work = arg0->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    }

    func_actor_444000_8013441C(arg0);

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)arg0->extra)->coords, 0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords, 0x400);
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[0]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[1]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();
}

/// Drag tick of the arena fight: the state the boss runs while it is hauling the
/// player in along the line between them.
///
/// A reset request (`field_4`) re-arms the block on animation 3, clears the host
/// model's flag word and pushes it onto each of the seven escorts' models, makes
/// sure the host and every escort has its model buffers allocated, re-seeds the
/// spinner target `D_actor_444000_80161890` from the fourth part of slot 4's
/// model and announces sub-state 2 through message 0x7DA.
///
/// Every tick then pins the player down to the arena floor, runs the ordinary
/// re-arm and stows the yaw from the host to the player -- relative to the
/// host's own facing, wrapped to +/-0x800 -- in `field_7C4`. The host's fifth
/// part is carried into view space, the player-relative offset from there gives
/// the direction and distance the pull works along, and the animation frame
/// picks how hard: `pull` is the phase's base strength and the frame divides
/// `-(pull + 0x19)` by 1, 2, 3, 4, 6 or 2/3 before `gte_gpf12` scales the
/// normalised direction by it. Frames outside 9..20 drop the pull and clear
/// `field_EFA`. `func_80105B74` hands the result to the player actor unless the
/// game is in mode 2 or 0xA or the player is already in mode 2.
///
/// Alongside that: a script fires every `period` frames while the frame sits in
/// 0xA..0x12, two cues play on frames 0x3C and 0xE8, the fight asks slot 3 for
/// the hold (message 0x3F8) once the player is inside 0x4B0 on frames 0xB..0xF
/// and phase 6 onward clamps the player back behind -0x52D0. Once `slots0[1]`
/// raises its flag the fight announces sub-state 3, moves to state 0xA and drops
/// its two spawned escorts.
void func_actor_444000_8013E058(Task* task)
{
    Actor444000Work*        work  = task->work;
    GpEnemy*                enemy = task->spawnArg2;
    Task*                   slot3;
    GameActor*              actor;
    Actor444000DragScratch* sc;
    Actor444000Work*        escorts;
    Actor444000Work*        buffers;
    PlayerStatus*           cfg;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          yawCoord;
    GsCOORDINATE2*          clamp;
    SVECTOR*                posp;
    SVECTOR*                dirp;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    s16                     angle;
    s16                     i;
    s16                     j;
    s16                     dz;

    slot3 = gameGetPtrSlot(3);
    sc    = (Actor444000DragScratch*)(SCRATCH_SP -= sizeof(Actor444000DragScratch));
    actor = slot3->work;

    if (work->field_4 != 0) {
        work->field_7B3                  = 3;
        work->field_7B0                  = 2;
        escorts                          = task->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)task->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
            }
        }
        tmd     = (TmdObject*)task->extra;
        buffers = task->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EFE = 0;
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        posp            = &D_actor_444000_80161890;
        posp->vz        = 0;
        posp->vy        = 0;
        posp->vx        = 0;
        actorLocalToView(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords[3], posp);
        D_actor_444000_80161888.from.loc.stage = 0;
        D_actor_444000_80161888.from.loc.area  = 0x2C;
        D_actor_444000_80161888.command        = 2;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
    }

    coord = ((TmdObject*)slot3->extra)->coords;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1]                       = 0;
        ((TmdObject*)slot3->extra)->coords->flg = 0;
    }
    func_actor_444000_8013441C(task);

    cfg        = &Player_Status;
    facing     = ((TmdObject*)task->extra)->coords;
    dirp       = &sc->dir;
    sc->dir.vx = *(u16*)&cfg->coordMtx->t[0] - *(u16*)&facing->coord.t[0];
    dirp->vy   = *(u16*)&cfg->coordMtx->t[1] - *(u16*)&facing->coord.t[1];
    dz         = *(u16*)&cfg->coordMtx->t[2] - *(u16*)&facing->coord.t[2];
    dirp->vz   = dz;
    yawCoord   = ((TmdObject*)task->extra)->coords;
    angle      = ratan2(sc->dir.vx, dz) - ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;

    sc->dir.vz = 0;
    sc->dir.vy = 0;
    sc->dir.vx = 0;
    actorLocalToView(&((TmdObject*)task->extra)->coords[4], &sc->dir);

    sc->dir.vx = *(u16*)&((TmdObject*)slot3->extra)->coords->coord.t[0] - (u16)sc->dir.vx;
    sc->dir.vy = *(u16*)&((TmdObject*)slot3->extra)->coords->coord.t[1] - (u16)sc->dir.vy;
    sc->dir.vz = *(u16*)&((TmdObject*)slot3->extra)->coords->coord.t[2] - (u16)sc->dir.vz;
    sc->dist   = sc->dir.vx * sc->dir.vx;
    sc->dist  += sc->dir.vz * sc->dir.vz;
    sc->dist   = SquareRoot0(sc->dist);
    VectorNormalSS(&sc->dir, &sc->dir);

    switch (work->field_F08) {
        case 0:
            sc->period = 0x19;
            break;
        case 1:
            sc->period = 0x11;
            break;
        case 2:
        default:
            sc->period = 0xE;
            break;
    }
    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xA) < 9U) && ((work->field_6 % sc->period) == 0)) {
        Gp_SpawnScript18((s32)&D_actor_444000_80144A94, (s32)&D_actor_444000_80144AA0);
    }

    switch (work->field_F08) {
        case 0:
        case 6:
            sc->pull = 0;
            break;
        case 1:
            sc->pull = 5;
            break;
        case 2:
            sc->pull = 0xA;
            break;
        case 3:
        case 4:
        case 5:
        default:
            sc->pull = 0xF;
            break;
    }

    if (work->field_6 == 0x3C) {
        s32 id;
        s32 pan;

        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020000A;
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
    if (work->field_6 == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
    }

    work->field_EFA = 1;
    switch (work->slots0[1].curRec & 0x3FF) {
        case 9:
            gte_lddp(-(sc->pull + 0x19) / 4);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x180;
            break;
        case 10:
            gte_lddp(-(sc->pull + 0x19) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(sc->pull + 0x19));
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((sc->pull + 0x19) * 3) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x500;
            break;
        case 16:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x400;
            break;
        case 19:
        case 20:
            sc->dir.vz = 0;
            sc->dir.vx = 0;
            gte_lddp(-(sc->pull + 0x19) / 6);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0;
            break;
        default:
            work->field_EFA = 0;
            sc->dir.vz      = 0;
            sc->dir.vx      = 0;
            break;
    }

    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xB) < 5U) && (sc->dist < 0x4B0) && (work->field_F08 < 6)) {
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161928, 0) == 0) {
            work->field_0   = 0xD;
            work->field_EC8 = 1;
        }
    }
    if (work->field_F08 >= 6) {
        clamp = ((TmdObject*)slot3->extra)->coords;
        if (clamp->coord.t[2] > -0x52D0) {
            clamp->coord.t[2] = -0x52D0;
        }
    }

    if (sc->dir.vx != 0 || sc->dir.vz != 0) {
        sc->push.vx = sc->dir.vx;
        sc->push.vy = 0;
        sc->push.vz = sc->dir.vz;
        if (D_8007218B[0] != 2 && D_8007218B[0] != 0xA && actor->field_954 != 2) {
            func_80105B74(&sc->push);
        }
    }

    if (work->slots0[1].flags & 1) {
        D_actor_444000_80161888.from.loc.stage = 0;
        D_actor_444000_80161888.from.loc.area  = 0x2C;
        D_actor_444000_80161888.command        = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
        work->field_0 = 0xA;
        for (sc->i = 0; sc->i < 2; sc->i++) {
            work->field_EE8[sc->i] = NULL;
        }
    }
    work->field_F1C = 0;
    SCRATCH_SP     += sizeof(Actor444000DragScratch);
}

/// Escort-order tick of the arena fight: the state the boss runs while it has
/// the player pinned in front of it.
///
/// A reset request raises the eight arena floor vertices `Gp_GridParams` keeps
/// at 24..31, re-arms the block on animation 0xF, clears the host model's flag
/// word and pushes it onto each of the seven escorts' models, makes sure the
/// host and every escort has its model buffers allocated, and -- if the player
/// has drifted inside 0xB54 -- drags them back out to that range along the
/// line between the two. It then places the player with
/// `Actor444000_PlacePlayerAhead`, hands the fight the battle flag, drops the
/// host and three of its escorts out of the actor slots and orders escort 3
/// through a 0x7DA message.
///
/// Every other tick runs the ordinary re-arm first. Animation 0xF hands over to
/// 0xE once the second slot raises its flag, and each animation fires one-shot
/// cues on the frames it reaches -- 0x19 under 0xF, 0x1D / 0x23 / 0x27 under
/// 0xE, the last two also kicking the pad -- with `field_7A8` remembering the
/// frame so none repeats while it is held. While message 0x3ED reports the
/// player free they are put back on the host's own position, and sub-state 0x17
/// re-places them and re-sends the 0x3FF animation.
/// Places the player in front of the host and points the pair at each other:
/// the host's fifth part is carried into view space, the yaw from there to the
/// player picks which of the two message-0x3FF animation tables the tick will
/// send (`..._80161680` past a quarter turn, `..._80161670` within it), and the
/// opposite yaw is stowed in `field_7C4` for the drive step. The normalised
/// direction scaled to 0x384 is where the player is asked to stand.
static __inline__ void Actor444000_PlacePlayerAhead(Task* task, Actor444000Work* work,
                                                    Task* player, Actor444000WarpScratch* sc,
                                                    PlayerStatus* cfg)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* facing;
    s16            angle;
    s32            yaw;

    sc->pos.vz = 0;
    sc->pos.vy = 0;
    sc->pos.vx = 0;
    actorLocalToView(&((TmdObject*)task->extra)->coords[4], &sc->pos);

    sc->dir.vx = sc->pos.vx - ((TmdObject*)player->extra)->coords->coord.t[0];
    sc->dir.vy = 0;
    sc->dir.vz = sc->pos.vz - ((TmdObject*)player->extra)->coords->coord.t[2];
    facing     = ((TmdObject*)player->extra)->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    yaw       = angle;
    sc->angle = yaw;
    if (abs(sc->angle) > 0x400) {
        if (sc->angle > 0) {
            sc->angle = yaw - 0x800;
        } else {
            sc->angle = yaw + 0x800;
        }
        work->anim.animBlock.ptr = D_actor_444000_80161680;
    } else {
        work->anim.animBlock.ptr = D_actor_444000_80161670;
    }
    coord      = ((TmdObject*)player->extra)->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);

    sc->dir.vx = ((TmdObject*)player->extra)->coords->coord.t[0] - sc->pos.vx;
    sc->dir.vy = 0;
    sc->dir.vz = ((TmdObject*)player->extra)->coords->coord.t[2] - sc->pos.vz;
    facing     = ((TmdObject*)task->extra)->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp2:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp2;
        }
    } else {
    wrapDown2:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown2;
        }
    }
    work->field_7C4 = angle;

    VectorNormalSS(&sc->dir, &sc->dir);
    gte_lddp(0x384);
    gte_ldsv(&sc->dir);
    gte_gpf12();
    gte_stsv(&sc->dir);

    D_actor_444000_80161908.pos.vx = sc->pos.vx + sc->dir.vx;
    D_actor_444000_80161908.pos.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
    D_actor_444000_80161908.pos.vz = sc->pos.vz + sc->dir.vz;
    D_actor_444000_80161908.rot.vx = 0;
    D_actor_444000_80161908.rot.vy = sc->angle;
    D_actor_444000_80161908.rot.vz = 0;
    if (cfg->hp > 0) {
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_444000_80161908, 0);
    }
}

void func_actor_444000_8013EC84(Task* arg0)
{
    Actor444000WarpScratch* sc;
    Actor444000Work*        work;
    Actor444000Work*        escorts;
    Actor444000Work*        buffers;
    GpEnemy*                enemy;
    Task*                   player;
    PlayerStatus*           cfg;
    Task*                   target;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    SVECTOR*                verts;
    s16                     i;
    s16                     j;
    s32                     frame;
    s32                     cueId;
    s32                     cuePan;
    s32                     hitId;
    s32                     hitPan;
    s32                     endId;
    s32                     endPan;

    work   = arg0->work;
    enemy  = arg0->spawnArg2;
    player = gameGetPtrSlot(3);
    cfg    = &Player_Status;

    if (work->field_4 != 0) {
        sc = (Actor444000WarpScratch*)(SCRATCH_SP -= sizeof(Actor444000WarpScratch));

        verts        = Gp_GridParams->field_8;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

        work->field_7B3    = 0xF;
        work->field_7B0    = 2;
        escorts            = arg0->work;
        escorts->field_7F3 = 0;

        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }

        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }

        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        sc->delta.vx = ((TmdObject*)player->extra)->coords->coord.t[0] -
                       ((TmdObject*)arg0->extra)->coords->coord.t[0];
        sc->delta.vz = ((TmdObject*)player->extra)->coords->coord.t[2] -
                       ((TmdObject*)arg0->extra)->coords->coord.t[2];
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vz * sc->delta.vz);
        if (sc->dist < 0xB54) {
            sc->dir.vy = 0;
            sc->dir.vx = sc->delta.vx;
            sc->dir.vz = sc->delta.vz;
            VectorNormalSS(&sc->dir, &sc->dir);
            gte_lddp(0xCE4);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            ((TmdObject*)player->extra)->coords->coord.t[0] =
                ((TmdObject*)arg0->extra)->coords->coord.t[0] + sc->dir.vx;
            ((TmdObject*)player->extra)->coords->coord.t[2] =
                ((TmdObject*)arg0->extra)->coords->coord.t[2] + sc->dir.vz;
            ((TmdObject*)player->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        }

        Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);

        D_actor_444000_80161868 = 0;
        Gp_StateC08.field_6    |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);

        D_actor_444000_80161888.from.loc.stage = 0;
        D_actor_444000_80161888.from.loc.area  = 0x2C;
        D_actor_444000_80161888.command        = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
    } else {
        sc = (Actor444000WarpScratch*)(SCRATCH_SP -= sizeof(Actor444000WarpScratch));
        func_actor_444000_8013441C(arg0);

        if ((work->slots0[1].flags & 1) && work->field_7B3 == 0xF) {
            work->field_7B0 = 2;
            work->field_7B3 = 0xE;
        }

        if (work->field_7B3 == 0xF) {
            if (cfg->hp > 0) {
                target = gameGetPtrSlot(3);
                Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 3), 0);
                if (cfg->hp <= 0) {
                    ((GameActor*)player->work)->field_956 = 0xA;
                    gGameSession->areaBgmCountdown        = 0x1E;
                    gGameSession->field_12E               = 0x36;
                    gGameSession->deathRestartDelay       = 0x5A;
                }
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x19 && work->field_7A8 != frame) {
                cueId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200011;
                cuePan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(cueId, cuePan,
                                    (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            }
            work->field_7A8 = work->slots0[3].curRec & 0x3FF;
        }

        if (work->field_7B3 == 0xE) {
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x1D && work->field_7A8 != frame) {
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x23 && work->field_7A8 != frame) {
                hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
                hitPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitId, hitPan,
                                    (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x27 && work->field_7A8 != frame) {
                endId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
                endPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(endId, endPan,
                                    (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            work->field_7A8 = work->slots0[3].curRec & 0x3FF;
        }

        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            D_actor_444000_80161908.pos.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0];
            D_actor_444000_80161908.pos.vy = ((TmdObject*)arg0->extra)->coords->coord.t[1];
            D_actor_444000_80161908.pos.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2];
            D_actor_444000_80161908.rot.vx = 0;
            D_actor_444000_80161908.rot.vy = 0;
            D_actor_444000_80161908.rot.vz = 0;
            Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_444000_80161908, 0);
            D_actor_444000_80161868 = 1;
        }

        if (work->field_6 == 0x17) {
            SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
            Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);
            work->anim.field_4 = 1;
            work->anim.field_8 = 0;
            work->anim.field_C = 0;
            work->field_F02    = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        }
    }

    SCRATCH_SP += sizeof(Actor444000WarpScratch);
}

/// Tick of the arena fight that runs the boss' two swipes and keeps the player
/// pinned in the scripted animation.
///
/// A reset request re-arms the block on animation 4, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, then rebuilds the
/// free coordinate at `field_E3C` from `field_7C8` and plays the entry cue.
/// That coordinate is pushed through `Gp_UpdateCoord` again on every step.
///
/// The two swipes are one-shot: animation 4 reaching frame 0xC raises bit
/// 0x8000 of the collision object's flags, kicks the pad and fires two cues,
/// and animation 5 reaching frame 0x1C fires a third. `field_7AC` remembers the
/// frame each step so neither repeats while the frame is held, and the bit is
/// cleared on every step the first swipe is not live.
///
/// `field_6` then picks the blend weight in `field_7A4` (and hands over to
/// state 0xA at 0xDC), and while it sits in 0x29..0x2E the shared timer
/// `D_actor_444000_80144A70` climbs by 0x258 a step up to 0x1770 -- past 0x39 it
/// is wound back down again instead.
///
/// The rest is the player hold: once one of the collision object's five records
/// reports a hit of class 1, message 0x3F8 is asked whether the player can be
/// taken over and message 0x3F9 asks for the hold itself, with `field_ECA`
/// keeping that reply and `field_EC8` marking the hold as ours. While it is,
/// the 0x3FF animation is re-sent every step the reply and `field_EC8` agree
/// (or, if they do not, for the first 0x28 steps), and after 0x17 steps without
/// the hold the payload is swapped for the player's own weapon animation
/// (`field_4` 4). A reply of something other than 1 on that second stage
/// cancels the animation with message 0x3F1 and drops the hold.
void func_actor_444000_8013FB74(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         enemy;
    Task*            player;
    Task*            target;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    GsCOORDINATE2*   coord;
    GpRec18*         recs;
    s16              i;
    s16              j;
    s16              k;
    s16              mode;
    s32              found;
    s32              frame;
    s32              frame2;
    s32              resetId;
    s32              resetPan;
    s32              swipeId;
    s32              swipePan;
    s32              swipe2Id;
    s32              swipe2Pan;
    s32              hitId;
    s32              hitPan;
    s32              cueId;
    s32              cuePan;
    u16              count;

    work        = arg0->work;
    enemy       = arg0->spawnArg2;
    player      = gameGetPtrSlot(3);
    SCRATCH_SP -= 0x30;

    if (work->field_4 != 0) {
        work->field_F1D                  = 0xB;
        work->field_7B3                  = 4;
        work->field_7B0                  = 2;
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        Gfx_RotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.flg = 0;
        Gp_UpdateCoord(&work->field_E3C.c);
        work->field_E96 = 0xC80;

        resetId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }

    coord                 = &work->field_E3C.c;
    work->field_E3C.c.flg = 0;
    Gp_UpdateCoord(coord);

    if (work->field_7B3 == 4 && (frame = work->slots0[1].curRec & 0x3FF) == 0xC &&
        work->field_7AC != frame) {
        Gfx_RotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.flg = 0;
        Gp_UpdateCoord(coord);
        work->field_EAC  = 3;
        work->obj.flags |= 0x8000;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);

        swipeId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200019;
        swipePan = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));

        swipe2Id  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001A;
        swipe2Pan = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));
    } else {
        work->obj.flags &= 0x7FFF;
    }

    if (work->field_7B3 == 5 && (frame2 = work->slots0[2].curRec & 0x3FF) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x8F, 8);

        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001B;
        hitPan = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->slots0[1].curRec & 0x3FF;
    } else {
        work->field_7AC = work->slots0[2].curRec & 0x3FF;
    }

    switch (work->field_6) {
        case 0x14:
            D_actor_444000_80144A70 = 0x640;
            work->field_7A4         = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->placeKey >> 12) << 8) | 0x40200018;
            cuePan          = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->field_7A4 = 2;
            break;
        case 0x38:
            work->field_7A4 = 4;
            break;
        case 0x44:
            work->field_7A4 = 3;
            break;
        case 0xDC:
            work->field_0 = 0xA;
            break;
    }

    if ((u32)((u16)work->field_6 - 0x29) < 6 && D_actor_444000_80144A70 < 0x1770) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 + 0x258;
    }

    func_actor_444000_8013441C(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].key == 0) {
            goto missed;
        }
        if ((recs[k].key & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161928, 0) == 0) {
        target          = gameGetPtrSlot(3);
        work->field_ECA = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 4), 0);
        if (work->field_ECA == 1) {
            ((GameActor*)player->work)->field_956 = 0xA;
        }
        work->anim.animBlock.ptr = D_actor_444000_80161670;
        work->field_EC8          = 1;
        work->anim.field_4       = 2;
        work->anim.field_8       = 0;
        work->anim.field_C       = 0;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        work->field_7CA = 0;
    }

    mode = work->field_EC8;
    if (mode == 1 && (s16)work->field_0 != 0xD) {
        count           = work->field_7CA + 1;
        work->field_7CA = count;
        if (work->field_ECA == mode) {
            if (work->anim.field_4 == 2) {
                work->anim.animBlock.ptr = D_actor_444000_80161670;
                work->anim.field_8       = 0;
                work->anim.field_C       = 0;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_7CA = 0;
            }
        } else if (work->anim.field_4 == 2 && (s16)count < 0x28) {
            work->anim.animBlock.ptr = D_actor_444000_80161670;
            work->anim.field_8       = 0;
            work->anim.field_C       = 0;
            Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        }

        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            switch (work->anim.field_4) {
                case 2:
                    if (work->field_ECA != 1 && (s16)work->field_7CA >= 0x17) {
                        work->anim.animBlock.ptr   = D_actor_444000_80161670;
                        D_actor_444000_80161670[4] = ((Actor403200AnimTable*)Gp_PlayerAnimBlkTbl
                                                          [Gp_WeaponIdBase[Mc_SaveData.characterId - 1] + Player_Status.weapon])
                                                         ->sets[7];
                        work->anim.field_4 = 4;
                        work->anim.field_8 = 1;
                        work->anim.field_C = 3;
                        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                        work->field_7CA = 0;
                    }
                    break;
                case 4:
                    if (work->field_ECA != 1) {
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
                        work->field_EC8 = 0;
                    }
                    break;
            }
        }
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (D_actor_444000_80144A70 >= 0xBB9) {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        } else {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0x1E;
        }
    }

    SCRATCH_SP += 0x30;
}

/// Per-tick state of the arena fight once it is under way. A reset request
/// re-arms the block on animation 0xB, clears the host model's flag word and
/// pushes it onto each of the seven escorts' models, then plays the entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned at the first
/// escort's second coordinate. From 0x3D on the fight also drops debris: every
/// fifth step one of the three shared coordinates in
/// `D_actor_444000_80161948` is rebuilt at that escort's second part -- its
/// rotation accumulated up the parent chain, its origin carried into view
/// space, then turned a quarter turn each way so `Gfx_MatrixCol2` yields the
/// launch direction, which is normalised and scaled to 0x320 before being
/// added to the origin -- and an effect is spawned on it. Every tenth step a
/// fresh enemy is spawned from `D_actor_444000_801617DC` and remembered in
/// `field_EF0`.
///
/// The tick then runs the ordinary re-arm and hands over to state 0xA once the
/// second animation slot raises its flag.
void func_actor_444000_801404C0(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    GsCOORDINATE2*   coord;
    SVECTOR          pos;
    SVECTOR*         posp;
    s16              i;
    s32              resetId;
    s32              resetPan;
    s32              cueId;
    s32              cuePan;
    s32              hitId;
    s32              hitPan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;

    if (work->field_4 != 0) {
        work->field_F1D    = 7;
        work->field_7B3    = 0xB;
        work->field_7B0    = 2;
        escorts            = arg0->work;
        escorts->field_7F3 = 0;

        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        resetId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(cueId, cuePan,
                            (s8)gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(hitId, hitPan,
                            (s8)gpGetObjDepth(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
            if (D_actor_444000_80161850 >= 2) {
                D_actor_444000_80161850 = 0;
            } else {
                D_actor_444000_80161850 = (u16)D_actor_444000_80161850 + 1;
            }

            actorAccumulateToView(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1],
                                  &D_actor_444000_80161948[D_actor_444000_80161850].coord);
            D_actor_444000_80161948[D_actor_444000_80161850].sub = &gGfxViewCoord;

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], &pos);

            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[0] = pos.vx;
            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[1] = pos.vy;
            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[2] = pos.vz;
            Gfx_RotMatrixY(&D_actor_444000_80161948[D_actor_444000_80161850].coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_444000_80161948[D_actor_444000_80161850].coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_444000_80161948[D_actor_444000_80161850].coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            coord              = &D_actor_444000_80161948[D_actor_444000_80161850];
            coord->coord.t[0] += pos.vx;
            coord->coord.t[1] += pos.vy;
            coord->coord.t[2] += pos.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x60196, &D_actor_444000_80161948[D_actor_444000_80161850], 0x27A0D600, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_444000_801617DC, 2, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
        }
    }

    func_actor_444000_8013441C(arg0);

    if (work->slots0[1].flags & 1) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000D, 1);
    }
}

/// Tick of the arena fight state that runs alongside `func_actor_444000_80140E28`:
/// on a reset request it clears the host model's flag word, pushes it onto each
/// of the seven escorts' models, makes sure the host and every escort has its
/// model buffers allocated and restores the normal blend weight.
///
/// The three state checks that follow are independent. In state 0xD the 0x18
/// script is spawned once, on the step the third animation slot first reaches
/// frame 0x15, which `field_7D8` remembers so the spawn does not repeat while
/// the frame is held. In state 9 at sub-state 0x2D the shared coordinate
/// `D_actor_444000_801618B8` is rebuilt as an identity sitting 100 units below
/// and 100 in front of the host model's fifth part, which it is parented to.
/// State 0x14 hands over to state 0xD once the second slot raises its flag.
/// The tick then runs the ordinary re-arm and re-flags the root coordinate for
/// rebuild.
void func_actor_444000_80140BBC(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    OverlayMat*      mtx;
    GsCOORDINATE2*   coords;
    s16              i;
    s16              j;
    s32              frame;

    work = arg0->work;
    if (work->field_4 != 0) {
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x10;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->slots0[2].curRec & 0x3FF;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_444000_80144A84, (s32)&D_actor_444000_80144A8C);
        }
        work->field_7D8 = work->slots0[2].curRec & 0x3FF;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                = ((TmdObject*)arg0->extra)->coords;
        D_actor_444000_801618B8.ident.m00_m01 = 0x1000;
        mtx                                   = (OverlayMat*)&D_actor_444000_801618B8.c.coord;
        mtx->ident.m02_m10                    = 0;
        mtx->ident.m11_m12                    = 0x1000;
        mtx->ident.m20_m21                    = 0;
        mtx->ident.m22                        = 0x1000;
        D_actor_444000_801618B8.c.coord.t[1]  = -0x64;
        D_actor_444000_801618B8.c.coord.t[0]  = 0;
        D_actor_444000_801618B8.c.coord.t[2]  = 0x64;
        D_actor_444000_801618B8.c.flg         = 0;
        D_actor_444000_801618B8.c.sub         = &coords[4];
        Gp_UpdateCoord(&D_actor_444000_801618B8.c);
    }
    if (work->field_7B3 == 0x14 && (work->slots0[1].flags & 1)) {
        work->field_7B3 = 0xD;
        work->field_7B0 = 1;
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Reset handler for the arena fight: on a reset request, clear the host
/// model's flag word, push it onto each of the seven escorts' models, make sure
/// the host and every escort has its model buffers allocated, then fast-forward
/// the animation by running the re-arm step an eighth of `field_F14` times
/// before restoring the normal blend weight and playing the entry cue.
///
/// Either way the tick then runs the ordinary re-arm, re-flags the root
/// coordinate for rebuild, and spawns the 0x18 script once -- on the step the
/// second animation slot first reaches frame 0x1C, which `field_7D8` remembers
/// so the spawn does not repeat while the frame is held.
void func_actor_444000_80140E28(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         obj;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    s16              i;
    s16              j;
    s16              k;
    s32              frame;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                              = arg0->spawnArg2;
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x7F;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        for (k = 0; k < work->field_F14 / 8; k++) {
            func_actor_444000_8013441C(arg0);
        }
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000A, 1);
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    frame                                  = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x1C && work->field_7D8 != frame) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
    }
    work->field_7D8 = work->slots0[2].curRec & 0x3FF;
}

void func_actor_444000_8014105C(Task* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              state;
    s32              id;
    s32              pan;

    work = arg0->work;
    if (work->field_4 != 0) {
        tmd                     = (TmdObject*)arg0->extra;
        obj                     = arg0->spawnArg2;
        obj->node.state.b.flags = 8;
        tmd->flags              = 0;
        state                   = work->field_7B3;
        work->field_EF4         = 0;
        work->field_EF6         = 0;
        work->field_EFA         = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->placeKey >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    if (D_actor_444000_80144A70 >= 0x191) {
        work->field_7A4         = 0;
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].flags & 1) {
        work->field_0 = 0xA;
    }
    SCRATCH_SP += 0xC;
}

/// Idle/approach tick of the arena fight: re-arms the block on request, keeps
/// the boss yawed at `Player_Status.coordMtx` (the player's coordinate matrix)
/// and then picks the state to run next.
///
/// `field_7C4` is that yaw, relative to the host part's own facing and wrapped
/// into +/-0x800. `field_F10` is a stagger countdown -- while it is positive the
/// tick only spins it down, and the reset arms it to 0x28 if it is not already
/// running.
///
/// The choice is a ladder: `field_F1C` picks state 3 outright, then each attack
/// pattern in `field_F08` has a depth the player has to be past before the
/// fight advances to state 9, the last two only while the boss still has HP in
/// hand. Failing all of those, `field_F1A` picks 0xF and pattern 6 picks 7, and
/// otherwise the distance from the player to a point just in front of the host
/// picks between 3, 7 and 0xB on a coin flip off `Gp_LcgState`.
///
/// `coord` and `facing` are the same coordinate read twice on purpose: the
/// stores into `vec` cut the first read's value, and the second read has to
/// outlive the first `ratan2` call.
void func_actor_444000_801411C8(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               enemy;
    Task*                  player;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    SVECTOR                vec;
    SVECTOR*               d;
    s16                    angle;

    work   = arg0->work;
    player = gameGetPtrSlot(3);
    enemy  = arg0->spawnArg2;

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
    }

    d      = &vec;
    coord  = ((TmdObject*)arg0->extra)->coords;
    d->vx  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    d->vy  = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    d->vz  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    facing = ((TmdObject*)arg0->extra)->coords;
    angle  = ratan2(d->vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;

    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        work->field_7A4         = 0;
    }
    func_actor_444000_8013441C(arg0);

    if (work->field_F10 > 0) {
        work->field_F10 = work->field_F10 - 1;
        return;
    }
    if (work->field_F1C > 0) {
        work->field_0 = 3;
        return;
    }

    if (work->field_F08 == 0) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 1 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x1D4C) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 2) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 3 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x30D4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 4 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x3DB8 &&
        enemy->hp < 0x9C4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 5 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x4268 &&
        enemy->hp < 0x7D0) {
        work->field_0 = 9;
        return;
    }

    if ((s8)work->field_F1A > 0) {
        work->field_0 = 0xF;
        return;
    }
    if (work->field_F08 == 6) {
        work->field_0 = 7;
        return;
    }

    sc           = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    sc->delta.vx = ((TmdObject*)player->extra)->coords->coord.t[0] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[0] - 0x51F;
    sc->delta.vy = ((TmdObject*)player->extra)->coords->coord.t[1] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0xFA;
    sc->delta.vz = ((TmdObject*)player->extra)->coords->coord.t[2] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[2] + 0x25F;
    sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                           sc->delta.vz * sc->delta.vz);
    if (sc->dist < 0x2329) {
        if (sc->dist >= 0xED9) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                work->field_0 = 7;
            } else {
                work->field_0 = 3;
            }
        } else {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                work->field_0 = 3;
            } else {
                work->field_0 = 0xB;
            }
        }
    } else {
        work->field_0 = 3;
    }
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// Escort-spawn tick of the arena fight: re-arms the block on request and, on
/// that first pass, tops the two escort slots (`field_EE8`) back up to two live
/// enemies, seeding each one's model texture page from the current area record
/// and stamping its slot index into `GpEnemy::placeKey`. Every tick it then
/// yaws the host at the player, and at sub-state 0x46 / 0x78 it sends escort 0
/// or 1 a 0x7DB order whose action is picked from `field_F08` and a coin flip.
void func_actor_444000_80141618(Task* task)
{
    Actor403200SpawnScratch* sc;
    Actor444000Work*         work;
    GpEnemy*                 host;
    GpEnemy*                 escort;
    PlayerStatus*            cfg;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    TmdObject*               model;
    GpAreaPlace*             entry;
    GpAreaKey                key;
    GpAreaKey*               sessionKey;
    s32                      cueId;
    s32                      cuePan;
    s32                      blastId;
    s32                      blastPan;
    s32                      rnd;
    s32                      state;
    s16                      angle;
    u32                      frame;

    sc   = (Actor403200SpawnScratch*)(SCRATCH_SP -= sizeof(Actor403200SpawnScratch));
    work = task->work;
    host = task->spawnArg2;
    if (work->field_4 != 0) {
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        state           = work->field_7B3;
        work->field_7B6 = 0x10;
        work->field_EFA = 0;
        if (state != 0x13) {
            work->field_7B3 = 0x13;
            work->field_7B0 = 1;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        for (sc->i = 0; sc->i < 2; sc->i++) {
            if (work->field_EE8[sc->i] == NULL && (u8)work->field_F1B < 8 && work->field_F08 < 6) {
                work->field_EE8[sc->i] = Gp_SpawnEnemyFromTable(&D_80172604, 3, 2, NULL);
                if (work->field_EE8[sc->i] != NULL) {
                    work->field_F1B++;
                    model      = (TmdObject*)work->field_EE8[sc->i]->task->extra;
                    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                    key.stage  = sessionKey->stage;
                    key.area   = sessionKey->area;
                    key.room   = sessionKey->room;
                    key.view   = sessionKey->view;
                    Gp_SyncAreaKeyIndex(&key);
                    entry        = (GpAreaPlace*)((s32)Gp_GetNestedAreaRec(&key)->field_0 + 0x20);
                    model->tpage = entry->tpage;
                    model->clut  = entry->clut;
                    if (model->buffer != NULL) {
                        tmdProcessStream(model);
                        tmdProcessStream(model);
                    }
                    work->field_EE8[sc->i]->workType = 0x900;
                    escort                           = work->field_EE8[sc->i];
                    escort->placeKey                |= sc->i << 12;
                    work->field_F1C++;
                }
            }
        }
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)host->placeKey >> 12) << 8) | 0x4020000D, 1);
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(task);
    if (work->field_7B3 == 0x13 && (frame = work->slots0[1].curRec & 0x3FF) >= 4 && frame < 0xD) {
        work->field_EFA = 1;
    } else {
        work->field_EFA = 0;
    }
    cfg          = &Player_Status;
    coord        = ((TmdObject*)task->extra)->coords;
    sc->delta.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
    sc->delta.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
    facing       = ((TmdObject*)task->extra)->coords;
    angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    if ((work->slots0[1].flags & 1) && work->field_7B3 == 0x13) {
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_7B6 = 0x10;
        func_actor_444000_8013441C(task);
    }
    if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && work->field_F1C == 0)) {
        work->field_0 = 3;
    }
    if (work->field_6 == 6) {
        cueId  = (((u16)host->placeKey >> 12) << 8) | 0x40200004;
        cuePan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(cueId, cuePan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
    if (work->field_6 == 0x3B) {
        blastId  = (((u16)host->placeKey >> 12) << 8) | 0x40200010;
        blastPan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(blastId, blastPan, (s8)(gpGetObjDepth(((TmdObject*)task->extra)->coords) / 2));
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
    }
    if (work->field_6 != 0x46 && work->field_6 != 0x78) {
        goto out;
    }
    if (work->field_6 == 0x46) {
        sc->i = 0;
    } else {
        sc->i = 1;
    }
    if (work->field_EE8[sc->i] != NULL && work->field_F08 < 6) {
        D_actor_444000_80161888.from.loc.stage = 0;
        D_actor_444000_80161888.from.loc.area  = 0x2C;
        switch (work->field_F08) {
            case 0:
            case 1:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 3;
                    } else {
                        D_actor_444000_80161888.command = 4;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 5;
                    } else {
                        D_actor_444000_80161888.command = 6;
                    }
                }
                break;
            case 2:
            case 3:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 0xd;
                    } else {
                        D_actor_444000_80161888.command = 8;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 7;
                    } else {
                        D_actor_444000_80161888.command = 0xe;
                    }
                }
                break;
            case 4:
            case 5:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 9;
                    } else {
                        D_actor_444000_80161888.command = 0xf;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_80161888.command = 9;
                    } else {
                        D_actor_444000_80161888.command = 0xf;
                    }
                }
                break;
        }
        D_actor_444000_80161888.command <<= 8;
        rnd                               = (Gp_LcgState * 5) + 0x71357911;
        D_actor_444000_80161888.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
        Gp_LcgState                       = rnd;
        Gp_DispatchMsg(work->field_EE8[sc->i]->task, 0x7DB, (s32)&D_actor_444000_80161888, 0);
    }
out:
    SCRATCH_SP += sizeof(Actor403200SpawnScratch);
}

/// Runs the arena attack sequence: restores the host and escort models, handles
/// animation cues and spawns the additional escort, then keeps the host facing
/// the player's coordinate matrix through the shared drive step.
void func_actor_444000_80141DFC(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         obj;
    GpEnemy*         child;
    GpAreaKey*       sessionKey;
    GpAreaRec*       rec;
    GpAreaPlace*     entry;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   headCoord;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    TmdObject*       model;
    SVECTOR          vec;
    SVECTOR*         v;
    GpAreaKey        key;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    s16              i;
    s16              j;
    s16              angle;
    s32              id;
    s32              pan;

    work = arg0->work;
    obj  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_7B3                  = 0xE;
        work->field_7B0                  = 1;
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        work->field_EFE = 0;
    }
    switch (work->field_6) {
        case 0x64:
        case 0x104:
            if ((s8)work->field_F1A > 0) {
                work->field_7B3 = 0x10;
                work->field_7B0 = 1;
                work->field_EF4 = 1;
                work->field_F1A = work->field_F1A - 1;
            } else {
                work->field_0   = 0xA;
                work->field_EFE = 0;
            }
            break;
        case 0x74:
            id  = (((u16)obj->placeKey >> 12) << 8) | 0x40200017;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            break;
        case 0x1A4:
            work->field_0   = 0xA;
            work->field_EFE = 0;
            work->field_F1A = 0;
            break;
        case 0x9B:
        case 0x113:
            work->field_EFE = 0x80;
            break;
        case 0xAF:
        case 0x145:
            child           = Gp_SpawnEnemyFromTable(&D_actor_444000_801617DC, 3, 0, arg0->spawnArg2);
            child->workType = 0x900;
            work->field_EF0 = child;
            if (child != NULL) {
                model      = (TmdObject*)child->task->extra;
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = sessionKey->view;
                SOFT_BARRIER();
                keyPtr = &key;
                TOUCH_REG(keyPtr);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyPtr);
                rec          = Gp_GetNestedAreaRec(&key);
                entry        = (GpAreaPlace*)((s32)rec->field_0 + 0x20);
                model->tpage = entry->tpage;
                model->clut  = entry->clut;
                if (model->buffer != NULL) {
                    tmdProcessStream(model);
                    tmdProcessStream(model);
                }
                work->field_EFE = 0;
            }
            break;
    }
    coord     = ((TmdObject*)arg0->extra)->coords;
    v         = &vec;
    v->vx     = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    v->vy     = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    v->vz     = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    headCoord = ((TmdObject*)arg0->extra)->coords;
    angle     = ratan2(v->vx, v->vz) - ratan2(-headCoord->coord.m[2][0], headCoord->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    func_actor_444000_8013441C(arg0);
    if (work->field_7B3 == 0x10 && (work->slots0[1].flags & 1)) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
    }
}

/// Keeps the player inside the arena: clamps the player model's root
/// translation every tick. `t[1]` (height) is never allowed above 0, and `t[2]`
/// (depth) is capped at 0 in front and -26000 at the back. The `t[2]` ladder
/// then picks the `t[0]` (lateral) corridor for that depth band, so the walls
/// narrow and widen as the player moves through the room.
void func_actor_444000_80142254(void)
{
    Task* player;
    s32   z;

    player = gameGetPtrSlot(3);

    if (((TmdObject*)player->extra)->coords->coord.t[1] > 0) {
        ((TmdObject*)player->extra)->coords->coord.t[1] = 0;
    }

    z = ((TmdObject*)player->extra)->coords->coord.t[2];
    if (z > 0) {
        ((TmdObject*)player->extra)->coords->coord.t[2] = 0;
    } else if (z > -1000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -5000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -7000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 9500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 9500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -13200) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -14750) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 17500;
        }
    } else if (z > -21250) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -22800) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 17500;
        }
    } else if (z > -26000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else {
        ((TmdObject*)player->extra)->coords->coord.t[2] = -26000;
    }
}

/// Per-frame body of the boss task: refreshes the host model's coordinate,
/// times out the "model buffers freed" countdown, re-lights the host and its
/// escorts, then runs the state handler `Actor444000Work::field_0` selects and
/// republishes every collision group.
///
/// `Gp_StateF0.field_4` gates how much of that runs. While the controller task is
/// suspended (1 or 2) the tick only pushes the host's `TmdObject::flags`
/// onto the escorts and clears the collision tables, and returns; only the
/// running case (0) and anything else falls through to the state machine.
/// Within the suspended cases the view index decides whether that flag word is
/// 0x80 (hidden) or 0.
///
/// `field_7F3` is a countdown armed when the fight hides the models: while it
/// runs the host is flagged hidden, and the step that takes it to zero also
/// raises bit 2 and hands every model's buffers back with `Tmd_FreeBuffers`.
///
/// `field_EFA` selects which of the two bodies is the "live" one -- the host
/// (`enemy`) or escort 3 (`field_ECC[3]`) -- and that choice drives the colour
/// update, the link-node slots and which collision groups publish their
/// `0x8000` bit this frame. States 0, 1, 5, 0xC, 0x12 and 0x13 are the inert
/// ones: they park both bodies on slot 1 and clear every group.
///
/// `field_F12` is the death timer, only started once the host's HP is gone:
/// step 0 tells the scene (message 0x7DA, action 0x2C) and latches
/// `D_actor_444000_80144A68`, step 3 tells the player's task (0x13F4) and moves
/// the fight to state 0x12 with the two death cues.
///
/// The dispatch table is a local, as in `func_actor_444000_80142F28`.
void func_actor_444000_801423C4(GpEnemy* enemy, Task* task)
{
    PlayerStatus*    cfg  = &Player_Status;
    Actor444000Work* work = task->work;
    VECTOR           pos;
    TaskFunc         handlers[0x15] = {
        func_actor_444000_8013D810,
        func_actor_444000_80143F4C,
        NULL,
        func_actor_444000_8013E058,
        NULL,
        func_actor_444000_80140BBC,
        NULL,
        func_actor_444000_801404C0,
        func_actor_444000_8014105C,
        func_actor_444000_8013482C,
        func_actor_444000_801411C8,
        func_actor_444000_8013FB74,
        func_actor_444000_80140E28,
        func_actor_444000_8013EC84,
        func_actor_444000_80141618,
        func_actor_444000_80141DFC,
        func_actor_444000_801434C4,
        func_actor_444000_801435CC,
        func_actor_444000_80135448,
        func_actor_444000_8013D96C,
        NULL,
    };
    Actor444000Work* escorts;
    Actor444000Work* flagged;
    s32              view;
    s16              i;
    s16              j;

    view                                     = Gp_GetViewIndex() & 0xFF;
    ((TmdObject*)task->extra)->coords[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[0]);

    escorts = task->work;
    if (escorts->field_7F3 != 0) {
        ((TmdObject*)task->extra)->flags = 0x80;
        if (--escorts->field_7F3 == 0) {
            ((TmdObject*)task->extra)->flags |= 4;
            Tmd_FreeBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (escorts->field_ECC[j] != NULL) {
                    ((TmdObject*)escorts->field_ECC[j]->task->extra)->flags |= 4;
                    Tmd_FreeBuffers((TmdObject*)escorts->field_ECC[j]->task->extra);
                }
            }
        }
    }

    func_actor_444000_80142254();

    pos.vx = ((TmdObject*)task->extra)->coords[3].workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords[3].workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords[3].workm.t[2];

    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    Gp_UpdateActorColor(work->field_EFA != 0 ? enemy : work->field_ECC[3], &pos, 0, 0);

    if ((s16)work->field_0 == 0xB) {
        ((TmdObject*)work->field_ECC[4]->task->extra)->otOffset = -1;
    } else {
        ((TmdObject*)work->field_ECC[4]->task->extra)->otOffset = 0;
    }

    switch (Gp_StateF0.field_4) {
        case 0:
            if ((s16)work->field_0 != 0) {
                if (view == 9) {
                    flagged                          = task->work;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0x80;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                } else {
                    flagged                          = task->work;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                }
            }
            break;

        case 1:
            if ((s16)work->field_0 != 0) {
                if (view == 9) {
                    flagged                          = task->work;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0x80;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                } else {
                    flagged                          = task->work;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;

        case 2:
            flagged                          = task->work;
            flagged->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (flagged->field_ECC[i] != NULL) {
                    ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                        ((TmdObject*)task->extra)->flags;
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;
    }

    SCRATCH_SP -= 0x1C;

    if (enemy->hp > 0) {
        if (work->field_EC8 != 1 && cfg->hp > 0 && (s16)work->field_0 != 0xD) {
            if (work->field_E92 > 0) {
                work->field_E92--;
            } else {
                func_actor_444000_8013C4B0(task);
            }
            if (work->field_E8C > 0) {
                work->field_E8C--;
            } else {
                func_actor_444000_8013C060(task);
            }
            if (work->field_E8E > 0) {
                work->field_E8E--;
            } else {
                func_actor_444000_8013CA60(task);
            }
            if (work->field_E90 > 0) {
                work->field_E90--;
            } else {
                func_actor_444000_8013D128(task);
            }
        }
    }
    if (enemy->hp <= 0) {
        if (cfg->hp <= 0) {
            enemy->hp               = 1;
            D_actor_444000_80144A68 = 0;
        }
        if (enemy->hp <= 0 && (s16)work->field_0 != 0) {
            switch (work->field_F12) {
                case 0:
                    D_actor_444000_80144A68                = 1;
                    D_actor_444000_80161888.from.loc.stage = 0;
                    D_actor_444000_80161888.from.loc.area  = 0x2C;
                    D_actor_444000_80161888.command        = 3;
                    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
                    break;

                case 3:
                    if (cfg->hp > 0) {
                        if (work->field_F08 == 6) {
                            Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 2, 0);
                        } else {
                            Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 1, 0);
                        }
                        work->field_0 = 0x12;
                        SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000A, 1);
                        SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x4020000D, 1);
                    }
                    break;
            }
            if (work->field_F12 < 0x100) {
                work->field_F12++;
            }
        }
    }

    if (work->field_2 != (s16)work->field_0) {
        work->field_4 = 1;
        work->field_6 = 0;
    } else {
        if (work->field_6 < 0x7FFF) {
            work->field_6++;
        }
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    handlers[(s16)work->field_0](task);

    if (work->field_0 < 2 || (s16)work->field_0 == 5 || (s16)work->field_0 == 0x12 ||
        (s16)work->field_0 == 0x13 || (s16)work->field_0 == 0xC) {
        enemy->node.state.b.flags              = 1;
        work->field_ECC[3]->node.state.b.flags = 1;
        work->field_ECC[0]->node.state.b.flags = 1;
        work->field_ECC[1]->node.state.b.flags = 1;
    } else if (work->field_EFA != 0) {
        if (Gp_NodeSlotMask(&work->field_ECC[3]->node) != 0) {
            Gp_AssignNodeSlot0(&enemy->node);
        }
        enemy->node.state.b.flags              = 8;
        work->field_ECC[3]->node.state.b.flags = 5;
        work->field_ECC[0]->node.state.b.flags = 5;
        work->field_ECC[1]->node.state.b.flags = 5;
    } else {
        if (Gp_NodeSlotMask(&enemy->node) != 0) {
            Gp_AssignNodeSlot0(&work->field_ECC[3]->node);
        }
        enemy->node.state.b.flags              = 1;
        work->field_ECC[3]->node.state.b.flags = 8;
        work->field_ECC[0]->node.state.b.flags = 8;
        work->field_ECC[1]->node.state.b.flags = 8;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 0x12 && (s16)work->field_0 != 0x13 &&
        (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC && work->field_EFA == 1) {
        work->hits[0].obj.flags |= 0x8000;
    } else {
        work->hits[0].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 0x12 && (s16)work->field_0 != 0x13 &&
        (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC && work->field_EFA != 1) {
        work->hits[1].obj.flags |= 0x8000;
        work->hits[2].obj.flags |= 0x8000;
    } else {
        work->hits[1].obj.flags &= 0x7FFF;
        work->hits[2].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC &&
        (s16)work->field_0 != 0x13 && (s16)work->field_0 != 0x12) {
        work->hits[3].obj.flags |= 0x8000;
        work->hits[4].obj.flags |= 0x8000;
        work->hits[5].obj.flags |= 0x8000;
    } else {
        work->hits[3].obj.flags &= 0x7FFF;
        work->hits[4].obj.flags &= 0x7FFF;
        work->hits[5].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC &&
        (s16)work->field_0 != 0x13 && (s16)work->field_0 != 0x12) {
        work->hits[6].obj.flags |= 0x8000;
        work->hits[7].obj.flags |= 0x8000;
        work->hits[8].obj.flags |= 0x8000;
    } else {
        work->hits[6].obj.flags &= 0x7FFF;
        work->hits[7].obj.flags &= 0x7FFF;
        work->hits[8].obj.flags &= 0x7FFF;
    }

    Gp_ClearRec18Occupied(work->hits[0].recs);
    Gp_ClearRec18Occupied(work->hits[1].recs);
    Gp_ClearRec18Occupied(work->hits[2].recs);
    Gp_ClearRec18Occupied(work->hits[3].recs);
    Gp_ClearRec18Occupied(work->hits[4].recs);
    Gp_ClearRec18Occupied(work->hits[5].recs);
    Gp_ClearRec18Occupied(work->hits[6].recs);
    Gp_ClearRec18Occupied(work->hits[7].recs);
    Gp_ClearRec18Occupied(work->hits[8].recs);
    Gp_ClearRec18Occupied(work->recs2);

    if ((s16)work->field_0 != 5) {
        func_actor_444000_8013A77C(task);
    }

    SCRATCH_SP += 0x1C;
}

/// Per-frame tail of the arena fight: keeps the camera pulled back far enough
/// to hold both the boss and the player, then runs the state the task is in.
///
/// `field_E94` is the camera distance actually in use and `field_E96` the one
/// the current state asks for -- 0xBB8 while the boss is grappling (state 3),
/// 0xD48 for the close patterns and 0x1388 otherwise -- walked 0x32 per frame
/// until the two are within 0x33 of each other. `field_E98` is the companion
/// height the floor-marker helpers take.
///
/// Most states hand that pair to `func_actor_444000_80132CB8`, which rebuilds
/// grid quad 6 as a wall in front of the boss. The exception is pattern 1 in state 9: it uses
/// `func_actor_444000_801371E8` instead, floors the player's own x at 0x2CEC,
/// and pushes the player back by the boss part's view-space depth less 0x7D0 --
/// part 4 of the boss model carried up the coordinate chain by
/// `actorLocalToView`. Whether the camera distance is then added to x or
/// subtracted from z is the same split: patterns other than 1-in-state-9 widen
/// x, the rest pull z in, and pattern 2 additionally floors z at 0x251C.
///
/// States 0, 5, 0xC, 0x12 and 0x13 skip all of that. State 0 -- and any state
/// the calls above dropped back to 0 -- also resets the two floor quads
/// `Gp_GridParams` keeps at vertices 24..31 to their default heights, and
/// state 5 still wants the marker.
///
/// The dispatch table is a local: `Task::state` picks the spawn state, this
/// tick, or `Gp_DestroyEnemy`.
void func_actor_444000_80142F28(Task* arg0)
{
    void (*handlers[3])(GpEnemy*, Task*) = {
        (void (*)(GpEnemy*, Task*))func_actor_444000_8013AFF8,
        (void (*)(GpEnemy*, Task*))func_actor_444000_801423C4,
        Gp_DestroyEnemy,
    };
    SVECTOR          result;
    Actor444000Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         verts;
    s32              diff;
    s16              state;

    enemy  = arg0->spawnArg2;
    player = gameGetPtrSlot(3);
    work   = arg0->work;
    if (work != NULL) {
        if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
            work->field_EE8[0] = NULL;
        }
        if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
            work->field_EE8[1] = NULL;
        }

        state = work->field_0;
        if (state == 3) {
            work->field_E96 = 0xBB8;
            work->field_E98 = 0x190;
        } else if (state == 9) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (state == 0x11) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (work->field_F08 != 0) {
            work->field_E96 = 0xD48;
            work->field_E98 = 0x190;
        } else {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        }

        diff = work->field_E96 - work->field_E94;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->field_E94 < work->field_E96) {
                work->field_E94 = (u16)work->field_E94 + 0x32;
            } else {
                work->field_E94 = (u16)work->field_E94 - 0x32;
            }
        } else {
            work->field_E94 = (u16)work->field_E96;
        }

        state = work->field_0;
        if (state != 0) {
            /* Split so that `0x12` and `0x13` are not the innermost `&&` pair:
               `fold_range_test` would turn two adjacent constants into one
               `sltiu` range check. */
            if (state != 0x12) {
                if (state != 0x13 && state != 5 && state != 0xC) {
                    if (work->field_F08 == 1 && state == 9) {
                        func_actor_444000_801371E8(arg0, work->field_E94, 6);
                        {
                            GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;

                            if (playerCoord->coord.t[0] < 0x2CEC) {
                                playerCoord->coord.t[0] = 0x2CEC;
                            }
                        }
                        result.vx = result.vy = result.vz = 0;
                        actorLocalToView(((TmdObject*)arg0->extra)->coords + 4, &result);
                        {
                            GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                            s32            z           = result.vz - 0x7D0;

                            if (z < playerCoord->coord.t[2]) {
                                playerCoord->coord.t[2] = z;
                            }
                        }
                    } else {
                        func_actor_444000_80132CB8(arg0, work->field_E94, work->field_E98, 6);
                    }

                    if (work->field_F08 == 0 || (work->field_F08 == 1 && (s16)work->field_0 != 9)) {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                        GsCOORDINATE2* selfCoord   = ((TmdObject*)arg0->extra)->coords;
                        s32            x           = work->field_E94 + selfCoord->coord.t[0];

                        if (playerCoord->coord.t[0] < x) {
                            playerCoord->coord.t[0] = x;
                        }
                    } else {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                        GsCOORDINATE2* selfCoord   = ((TmdObject*)arg0->extra)->coords;
                        s32            z           = selfCoord->coord.t[2] - work->field_E94;

                        if (z < playerCoord->coord.t[2]) {
                            playerCoord->coord.t[2] = z;
                        }
                    }

                    if (work->field_F08 == 2) {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;

                        if (playerCoord->coord.t[0] < 0x251C) {
                            playerCoord->coord.t[2] = 0x251C;
                        }
                    }
                }
            }
            /* Re-read: the calls above can drop the fight back to state 0. The
               `goto` is what lets the `state == 0` edge reach the reset
               directly, as the ROM does. */
            if ((s16)work->field_0 != 0) {
                goto skipGrid;
            }
        }

        verts        = Gp_GridParams->field_8;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

    skipGrid:
        state = work->field_0;
        if (state == 5) {
            func_actor_444000_80132CB8(arg0, work->field_E94, work->field_E98, 6);
        }
    }

    handlers[arg0->state](enemy, arg0);
}

/// Set the heights of collision grid quads `arg1` and `arg1 + 1`: 500 for each
/// quad's first two vertices, 800 for the other two. Nothing in the actor calls
/// it.
void func_actor_444000_80143374(s32 arg0, s16 arg1)
{
    SVECTOR* verts;

    verts                  = Gp_GridParams->field_8;
    verts[arg1 * 4].vy     = 500;
    verts[arg1 * 4 + 1].vy = 500;
    verts[arg1 * 4 + 2].vy = 800;
    verts[arg1 * 4 + 3].vy = 800;
    verts[arg1 * 4 + 4].vy = 500;
    verts[arg1 * 4 + 5].vy = 500;
    verts[arg1 * 4 + 6].vy = 800;
    verts[arg1 * 4 + 7].vy = 800;
}

/// Exit callback of the boss task: when its work block exists, send each of the
/// seven escorts to state 2, unlink every collision group but the third, and
/// detach the enemy's contact records, then tear the enemy down.
void func_actor_444000_801433B8(Task* arg0)
{
    Actor444000Work* work;
    GpEnemy*         enemy;
    s16              i;

    work  = (Actor444000Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work != NULL) {
        for (i = 0; i < 7; i++) {
            if (work->field_ECC[i] != NULL) {
                work->field_ECC[i]->task->state = 2;
            }
        }
        Gp_UnlinkObj(&work->hits[0].obj);
        Gp_UnlinkObj(&work->hits[1].obj);
        Gp_UnlinkObj(&work->hits[3].obj);
        Gp_UnlinkObj(&work->hits[4].obj);
        Gp_UnlinkObj(&work->hits[5].obj);
        Gp_UnlinkObj(&work->hits[6].obj);
        Gp_UnlinkObj(&work->hits[7].obj);
        Gp_UnlinkObj(&work->hits[8].obj);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_444000_80143490(s8 arg0)
{
    ((Actor444000Work*)D_actor_444000_80161878->work)->field_EAC = arg0;
}

void func_actor_444000_801434A8(s16 arg0)
{
    D_actor_444000_80144A72 = arg0;
}

s16 func_actor_444000_801434B4(void)
{
    return D_actor_444000_80144A72;
}

/// The re-arm's counterpart: on a reset request it sets the two 0xEF4 counters
/// and the 0x7B0 pair rather than clearing them, and pushes `field_E = 2` onto
/// the first two escorts' model objects. Every tick it also parks one of two
/// yaw presets in `field_7C4`, alternating every 60 counts.
void func_actor_444000_801434C4(Task* arg0)
{
    Actor444000Work* work;
    s16              tick;

    work = arg0->work;
    if (work->field_4 != 0) {
        work->field_EF4                                         = 1;
        work->field_EF6                                         = 1;
        work->field_EFA                                         = 0;
        work->field_7B3                                         = 1;
        work->field_7B0                                         = 1;
        work->field_EFE                                         = 0;
        work->field_F1A                                         = 0;
        ((TmdObject*)work->field_ECC[0]->task->extra)->otOffset = 2;
        ((TmdObject*)work->field_ECC[1]->task->extra)->otOffset = 2;
        func_80185220();
    }
    func_actor_444000_8013441C(arg0);
    tick = work->field_6;
    if (tick % 60 == 0) {
        if (tick % 120 == 0) {
            work->field_7C4 = 0x2B2;
        } else {
            work->field_7C4 = -0x1A2;
        }
    }
}

void func_actor_444000_801435CC(Task* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              id;
    s32              pan;

    work = arg0->work;
    obj  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        tmd                     = (TmdObject*)arg0->extra;
        obj->node.state.b.flags = 0;
        tmd->flags              = 0;
        work->field_7B3         = 0xC;
        work->field_7B0         = 2;
        work->field_EF4         = 0;
        work->field_EF6         = 0;
        work->field_EFA         = 0;
        work->field_7B6         = 0x10;
        work->field_EFE         = 0;
        work->field_6           = 0;
    }
    if (work->field_6 == 0xA) {
        id  = ((obj->placeKey >> 12) << 8) | 0x40200017;
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].flags & 1) {
        work->field_0 = 9;
    }
}

/// Setup state of the handler table `D_actor_444000_80131E90`: look up the
/// area placement the parent's spawn record names (its top nibble) under the
/// current session location, give the model that placement's texture page and
/// CLUT, run its stream twice when it has one, and step the task on.
void func_actor_444000_801436CC(GpEnemy* enemy, Task* task)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    raw        = ((GpWorkObj*)task->parent->spawnArg2)->field_8.as_u16;
    model      = (TmdObject*)task->extra;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    task->state++;
}

/// Per-frame state of the same table: refresh the model's root coordinate. The
/// world position it then copies into a local is never used.
void func_actor_444000_80143798(GpEnemy* enemy, Task* arg1)
{
    VECTOR sp10;

    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    sp10.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    sp10.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    sp10.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
}

void func_actor_444000_8014382C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_444000_80143888(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Same dispatch as `func_actor_444000_80143888` through the second handler
/// table, skipped while the global game state is 2.
void func_actor_444000_801438E4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E9C;
    switch (Gp_StateF0.field_4) {
        default:
        case 0:
        case 1:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 2:
            break;
    }
}

/// Dispatcher of the grab enemy (`D_actor_444000_80131EA8`): park the model
/// object while the global game state is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it. The same shape as `func_actor_444000_80143A6C`, except that it guards
/// the bookkeeping on the state being non-zero rather than on the work block
/// existing, and clears the model object's flag word rather than leaving it 2.
void func_actor_444000_80143960(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;
    Actor403200GrabWork*  work;

    sp   = D_actor_444000_80131EA8;
    work = arg0->work;

    switch (Gp_StateF0.field_4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 0;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 0;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->state != 0) {
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the `D_actor_444000_80131F0C` enemy: park the model object
/// while the global game state is 1 or 2, otherwise note in the work block
/// whether the state changed since the last step and run the handler for it.
/// The same shape as `func_actor_444000_80143C64`, over the other work block
/// and with the model object's flag word left at 2 rather than cleared.
void func_actor_444000_80143A6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;
    Actor444000F0CWork*   work;

    sp = D_actor_444000_80131F0C;

    switch (Gp_StateF0.field_4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 2;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 2;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->work != NULL) {
        work = arg0->work;
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Same dispatch again through a five-entry handler table, skipped while the
/// global game state is 1 or 2.
void func_actor_444000_80143B74(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_actor_444000_80131F1C;
    switch (Gp_StateF0.field_4) {
        case 0:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 1:
        case 2:
            break;
    }
}

/// Waiting state of the spinner enemy (`D_actor_444000_80131F30`): hand the
/// enemy back to `Gp_DestroyEnemy` once `D_actor_444000_80144A68` is set;
/// otherwise keep the model's flag word cleared, so it is not drawn, and step
/// the task on once `D_actor_444000_80144A72` is 1.
void func_actor_444000_80143BFC(GpEnemy* arg0, Task* arg1)
{
    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (D_actor_444000_80144A72 == 1) {
        arg1->state++;
    }

    ((TmdObject*)arg1->extra)->flags = 0;
}

/// Dispatcher of the spinner enemy `D_actor_444000_80131F30` drives: park the
/// model object while the global game state is 1 or 2, otherwise note in the
/// work block whether the state changed since the last step and run the
/// handler for it.
void func_actor_444000_80143C64(Task* arg0)
{
    GpEnemyTaskFuncTable4   sp;
    Actor403200SpinnerWork* work;

    sp = D_actor_444000_80131F30;

    switch (Gp_StateF0.field_4) {
        case 0:
            ((TmdObject*)arg0->extra)->flags = 0;
            break;
        case 1:
            ((TmdObject*)arg0->extra)->flags = 0;
            return;
        case 2:
            ((TmdObject*)arg0->extra)->flags = 0x80;
            return;
    }

    if (arg0->work != NULL) {
        work = arg0->work;
        if (work->field_94 != arg0->state) {
            work->field_90 = 1;
        } else {
            work->field_90 = 0;
        }
        work->field_94 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

s32 func_actor_444000_80143D68(Task* arg0)
{
    return ((GpEnemy*)arg0->spawnArg2)->hp > 0;
}

/// Seeds the enemy's `TmdObject` coordinate frame from `placement`: the three
/// longs become the translation, the Euler angles are applied X/Y/Z unless the
/// work block's state index is 0x12 or 0x13, and the coordinate is marked
/// dirty. Same body as `ActorsShared80135990` with that state gate added.
s32 func_actor_444000_80143D7C(Task* arg0, s32 arg1, GpXformArg* placement)
{
    Actor444000Work* work = arg0->work;

    ((TmdObject*)arg0->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)arg0->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] = placement->pos.vz;
    if ((u32)(work->field_0 - 0x12) >= 2U) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vx, 1);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vy, 0);
        Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vz, 0);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    return 1;
}

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down, re-arms `field_F16` and drops either tracked
/// enemy whose HP has run out.
s32 func_actor_444000_80143E68(Task* arg0, s32 arg1, s32 arg2)
{
    Actor444000Work* work = arg0->work;
    GpEnemy*         obj  = arg0->spawnArg2;

    switch (arg2) {
        case 0:
            work->field_F1A++;
            func_800DA6E8(&obj->node, -0x64, 0);
            if (obj->hp > 0) {
                obj->hp += 0x64;
            }
            break;
        case 1:
            if (work->field_F1C > 0) {
                work->field_F1C--;
            }
            work->field_F16 = 2;
            if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
                work->field_EE8[0] = NULL;
            }
            if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
                work->field_EE8[1] = NULL;
            }
            break;
    }
    return 1;
}

s32 func_actor_444000_80143F38(Task* arg0)
{
    ((Actor444000Work*)arg0->work)->field_0 = 0;
    return 1;
}

/// Reset handler: when the work block is asking for a reset, stop the enemy's
/// own model drawing and push that same flag word onto each of the seven
/// escorts' models, then clear the two counters at 0xEF4. Otherwise just run
/// the ordinary re-arm in `func_actor_444000_8013441C`.
void func_actor_444000_80143F4C(Task* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    s16              i;

    work = arg0->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        escorts                          = arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    } else {
        func_actor_444000_8013441C(arg0);
    }
}
