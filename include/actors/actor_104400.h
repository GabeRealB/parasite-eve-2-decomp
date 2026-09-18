#ifndef ACTOR_104400_H
#define ACTOR_104400_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Status flags at `Actor104400Work` + 0xEC, read through two widths.
///
/// Guards test bit 0 as a halfword and then bits 0x102 as a word
/// (`Actor04400_Fn06618` is the out-of-line copy of the test), the same
/// shape as `Actor341700Flags`.
typedef union Actor104400Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor104400Flags;
STATIC_ASSERT_SIZEOF(Actor104400Flags, 0x4);

/// Per-actor state block for the `actor_104400` overlay's enemy.
///
/// `Actor04400_Fn00B24` and `Actor04400_Fn00D3C` both allocate it with
/// `Mem_Calloc(0x454, 0)` and store it in the `Task::work` slot (0x1C), so
/// the size below is the allocation, not a guess: this actor reuses that
/// pointer field for its own work block and it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor104400Work*)task->work`.
///
/// The layout mirrors the sibling `actor_341700` / `actor_342400` overlays,
/// whose work blocks are the same size and drive the same state machine:
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks, `field_412` is the per-state frame counter, and
/// `field_414` .. `field_426` are the animation request the actor hands to
/// its player. The three `GpObj` nodes are the collision objects
/// `Actor04400_Fn08A40` hands back to `Gp_UnlinkObj`. `obj_2AC` and
/// `obj_2CC` share `rec_2EC`; `obj_3AC` has its own table at `rec_3CC`.
typedef struct Actor104400Work {
    /* 0x000 */ MATRIX    matrix_0; // model root coord, copied out on the kill path
    /* 0x020 */ MATRIX    colorMtx; // the model's `TmdObject::colorMtx`
    /* 0x040 */ MATRIX    lightMtx; // the model's `TmdObject::lightMtx`
    /* 0x060 */ VECTOR    field_60; // position Actor04400_Fn022A8 snaps the root back to when blocked
    /* 0x070 */ SVECTOR   field_70; // origin of slot 4 entry 0's coords[3], carried into view space by Actor04400_Fn05B08
    /* 0x078 */ s16       field_78; // pitch, fed to RotMatrixX
    /* 0x07A */ s16       field_7A; // heading
    /* 0x07C */ s16       field_7C; // roll, fed to RotMatrixZ
    /* 0x07E */ byte      pad_7E[0x2];
    /* 0x080 */ u16       field_80; // spawn position: root coord.t[0]
    /* 0x082 */ u16       field_82; // root coord.t[1], after lifting it by 0x3C
    /* 0x084 */ u16       field_84; // root coord.t[2]
    /* 0x086 */ byte      pad_86[0x2];
    /* 0x088 */ s16       field_88; // x of the vector turned towards
    /* 0x08A */ s16       field_8A;
    /* 0x08C */ s16       field_8C; // z of the vector turned towards
    /* 0x08E */ byte      pad_8E[0x2];
    /* 0x090 */ u16       field_90; // root coord.t[0], snapshotted with field_92 / field_94
    /* 0x092 */ u16       field_92; // root coord.t[1]
    /* 0x094 */ u16       field_94; // root coord.t[2]
    /* 0x096 */ byte      pad_96[0x2];
    /* 0x098 */ SVECTOR   field_98; // translation of coords[6] relative to the view
    /* 0x0A0 */ GpAnimCtx anim;
    /// First of the nine `GpAnimSlot`s (0xB4..0x21C); the second overlaps
    /// `flags_EC`, so only the first is spelled out.
    /* 0x0B4 */ GpAnimSlot       slot_B4;
    /* 0x0DC */ byte             pad_DC[0x10];
    /* 0x0EC */ Actor104400Flags flags_EC;
    /* 0x0F0 */ byte             pad_F0[0x12C];
    /* 0x21C */ byte             field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    /* 0x2AC */ GpObj            obj_2AC;
    /* 0x2CC */ GpObj            obj_2CC;
    /* 0x2EC */ GpRec18          rec_2EC[8];
    /* 0x3AC */ GpObj            obj_3AC;
    /* 0x3CC */ GpRec18          rec_3CC[2];
    /* 0x3FC */ GpEffArg         eff_3FC;   // field_0 is the model's second coord part
    /* 0x404 */ byte             pad_404[0x8];
    /* 0x40C */ s16              field_40C; // heading Actor04400_Fn017B0 moves the root along
    /* 0x40E */ s16              field_40E; // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    /* 0x410 */ s16              field_410; // random 0..0x7FF drawn from `Gp_LcgState`
    /* 0x412 */ u16              field_412; // per-state frame counter
    /* 0x414 */ s16              field_414; // animation request kind
    /* 0x416 */ s16              field_416; // animation id last applied to the slots
    /* 0x418 */ s16              field_418; // animation id
    /* 0x41A */ u16              field_41A; // frames since the animation was applied
    /* 0x41C */ s16              field_41C; // animation speed / step scale
    /* 0x41E */ s16              field_41E;
    /* 0x420 */ u16              field_420; // state index
    /* 0x422 */ u16              field_422; // sub-state index
    /* 0x424 */ s16              field_424; // yaw added to model parts 3..5, a third each
    /* 0x426 */ s16              field_426;
    /* 0x428 */ s16              field_428;
    /* 0x42A */ s16              field_42A;
    /* 0x42C */ s16              field_42C; // frames spent turning toward field_444; 16 enters state 3
    /* 0x42E */ byte             pad_42E[0x2];
    /* 0x430 */ s16              field_430;
    /* 0x432 */ s16              field_432; // 1 runs Actor04400_Fn06520 on the spawn position
    /* 0x434 */ s16              field_434; // pitch, eased back to zero while falling
    /* 0x436 */ s16              field_436; // step picked from `field_43A`'s distance band
    /* 0x438 */ s16              field_438; // 1 on the death path
    /* 0x43A */ s16              field_43A; // distance to the nearer player actor
    /* 0x43C */ byte             pad_43C[0x2];
    /* 0x43E */ s16              field_43E; // counted down each frame by Actor04400_Fn022A8
    /* 0x440 */ s16              field_440; // picks animation 5 (zero) or 6 after animation 8
    /* 0x442 */ u16              field_442;
    /* 0x444 */ u16              field_444; // heading to the nearer player actor, relative to field_7A
    /* 0x446 */ s16              field_446; // randomised hold compared against field_412
    /* 0x448 */ s16              field_448;
    /* 0x44A */ s16              field_44A;
    /* 0x44C */ u16              field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    /* 0x44E */ u8               field_44E; // set while the enemy carries status flag 4/8
    /* 0x44F */ u8               field_44F;
    /* 0x450 */ byte             pad_450[0x1];
    /* 0x451 */ u8               field_451; // 1 skips Actor04400_Fn00220 part-pair colour
    /* 0x452 */ byte             pad_452[0x2];
} Actor104400Work;
STATIC_ASSERT_SIZEOF(Actor104400Work, 0x454);

/// Payload the sender of message 0x2C00 passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor342400Msg` and `Actor335800Msg`. The
/// overlay's 0x2C00 handler, `Actor04400_Fn0648C`, tests the id at 0x0 and
/// stores the halfword at 0x2 in `Actor104400Work::field_44C`.
typedef struct Actor104400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor104400Msg;
STATIC_ASSERT_SIZEOF(Actor104400Msg, 0x4);

/// A `MATRIX`'s word-wise view, for the identity splat `Actor04400_Fn08C64`
/// writes over the root coordinate before `ScaleMatrix` scales it: five aligned
/// stores instead of nine halfword ones, each word holding two adjacent `m[][]`
/// entries. The same idiom as `ActorsShared8016bd98Mat`, whose body this
/// function repeats on the `Actor104400Work` layout.
typedef struct Actor104400MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor104400MatWords;

typedef union Actor104400Mat {
    MATRIX              mat;
    Actor104400MatWords ident;
} Actor104400Mat;
STATIC_ASSERT_SIZEOF(Actor104400Mat, 0x20);

extern u8             Actor04400_D10814[]; // per animation id (1-based): the value to put in `field_44F`
extern u8             Actor04400_D10828[]; // per animation id (1-based): the animation to follow it
extern u8             D_801153F4;          // absolute; nonzero skips the controller's state handler
extern GpPairSrcE     Actor04400_D0D318;   // the main enemy's `GpEnemy::field_50` record
extern u8             Actor04400_D10778[]; // animation bank handed to `func_800B3F84`
extern u8             Actor04400_D107CC[]; // stored into `Task::msgTable` by Actor04400_Fn00B24
extern TaskFuncTable3 Actor04400_D00070;   // dispatcher table Actor04400_Fn06ACC copies onto its stack
extern TaskFuncTable3 Actor04400_D0007C;   // dispatcher table Actor04400_Fn06870 copies onto its stack
extern TaskFuncTable5 Actor04400_D00088;   // dispatcher table Actor04400_Fn068F8 copies onto its stack
extern TaskFuncTable5 Actor04400_D0009C;   // dispatcher table Actor04400_Fn06964 copies onto its stack
extern TaskFuncTable3 Actor04400_D00150;   // dispatcher table Actor04400_Fn07CF0 copies onto its stack
extern TaskFuncTable3 Actor04400_D0015C;   // dispatcher table Actor04400_Fn07D78 copies onto its stack
extern TaskFuncTable4 Actor04400_D00174;   // dispatcher table Actor04400_Fn07F04 copies onto its stack
extern TaskFuncTable6 Actor04400_D001AC;   // dispatcher table Actor04400_Fn06B50 copies onto its stack

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits. Same
/// declaration as `ActorsShared8016a538`, whose body `Actor04400_Fn07404`
/// repeats on this overlay's work block.
void func_8004BFF8(s32 angle, MATRIX* matrix);

#endif
