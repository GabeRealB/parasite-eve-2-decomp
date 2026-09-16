#ifndef ACTOR_104000_H
#define ACTOR_104000_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/session.h"

/// Payload `func_actor_104000_80138CC8` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `Actor444000Msg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor104000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor104000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor104000Msg7DA, 0x4);

/// Event packet handed to the message handlers: the same four bytes read as
/// two `u16` words, a command word (0x1003, 0x1203, 0x302) and a sub-command.
typedef union Actor104000Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor104000Event;

/// Two counters the overlay clears together with `D_actor_104000_8013E538`
/// when it restarts its run; still assembly everywhere they are written.
extern s32 D_actor_104000_8013E530[2];

/// Six-entry counter table the overlay resets before dispatching its 0x7DA
/// message.
extern s32 D_actor_104000_8013E538[8];

/// The actor's per-instance work block (`field_1C` of `Actor104000`),
/// allocated and filled by `func_actor_204000_8014AED8`. It embeds four
/// collision objects linked with `Gp_LinkObj`, each followed by the `GpRec18`
/// table its `field_C` points at; the high bit of their flag words gates one
/// behaviour and bit 0x4000 another.
typedef struct Actor104000Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4;
    /* 0x006 */ u16        field_6;
    /* 0x008 */ s16        field_8;
    /* 0x00A */ s16        field_A;
    /* 0x00C */ GpAnimCtx  anim;
    /* 0x020 */ GpAnimSlot slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[2];
    /* 0x04A */ u16        field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ s16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A;
    /* 0x17C */ s16        field_17C;
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ s32        field_180;
    /* 0x184 */ s32        field_184;
    /* 0x188 */ s32        field_188;
    /* 0x18C */ s32        field_18C;
    /* 0x190 */ s32        field_190;
    /* 0x194 */ u16        field_194;
    /* 0x196 */ byte       pad_196[2];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ s16        field_19C;
    /* 0x19E */ byte       pad_19E[2];
    /* 0x1A0 */ s16        field_1A0;
    /* 0x1A2 */ s16        field_1A2;
    /* 0x1A4 */ byte       pad_1A4[0xC];
    /* 0x1B0 */ GpRec18    rec1B0[8];
    /* 0x270 */ GpObj      obj270;
    /* 0x290 */ GpRec18    hits[8]; // this frame's collision records, ended by a zero id
    /* 0x350 */ GpObj      obj350;
    /* 0x370 */ GpRec18    rec370;
    /* 0x388 */ GpObj      obj388;
    /* 0x3A8 */ GpRec18    rec3A8;
    /* 0x3C0 */ GpObj      obj3C0;
    /* 0x3E0 */ GpEffArg   eff;           // `func_800FDB18` argument record
    /* 0x3E8 */ SVECTOR    effOfs;        // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x3F0 */ SVECTOR    origin;        // model position at spawn
    /* 0x3F8 */ SVECTOR    dir;           // facing direction captured on restart
    /* 0x400 */ SVECTOR    ahead;         // spawn position plus 1000 units along the facing (XZ)
    /* 0x408 */ SVECTOR    behind;        // spawn position minus the same offset
    /* 0x410 */ byte       pad_410[4];
    /* 0x414 */ MATRIX     lightMtx;      // installed at `Actor104000Obj2C.field_1C`
    /* 0x434 */ MATRIX     colorMtx;      // installed at `Actor104000Obj2C.field_20`
    /* 0x454 */ MATRIX     savedColorMtx; // `colorMtx` before the death fade scales it
    /* 0x474 */ u16        field_474;     // animation id that last raised the reaction
    /* 0x476 */ byte       pad_476[3];
    /* 0x479 */ u8         field_479;
    /* 0x47A */ u8         field_47A;
    /* 0x47B */ byte       pad_47B[1];
    /* 0x47C */ byte       field_47C[0x14];
    /* 0x490 */ s32        field_490;
    /* 0x494 */ s16        field_494;
    /* 0x496 */ s16        field_496;
} Actor104000Work;
STATIC_ASSERT_SIZEOF(Actor104000Work, 0x498);

/// Display object hung off `field_2C`; `field_C` is the visibility/alpha value
/// the state handlers clear when the actor restarts.
typedef struct Actor104000Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ MATRIX*        field_1C; // light matrix
    /* 0x20 */ MATRIX*        field_20; // color matrix
} Actor104000Obj2C;

/// Caller-owned context the actor also keeps a pointer to at `field_20`;
/// `field_14` is the flag the state handlers clear; the top nibble of `field_8`
/// selects whether it is raised again.
typedef struct Actor104000Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[0x2B];
    /* 0x40 */ s16  field_40; // 0 or below frees the actor's lead slot (`func_actor_204000_801501A0`)
} Actor104000Ctx;

typedef struct Actor104000 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor104000Work*  field_1C;
    /* 0x20 */ Actor104000Ctx*   field_20;
    /* 0x24 */ void*             field_24;
    /* 0x28 */ byte              pad_28[4];
    /* 0x2C */ Actor104000Obj2C* field_2C;
    /* 0x30 */ s32               state;
    /* 0x34 */ s16               field_34;
    /* 0x36 */ s16               field_36; // 1 starts in state 2, otherwise 7
} Actor104000;
STATIC_ASSERT_SIZEOF(Actor104000, 0x38);

/// 0xC-byte scratch taken from `0x1F8003FC` for the player-in-radius test:
/// the X/Z offset to the camera target and the radius, squared in place.
typedef struct Actor104000RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor104000RangeScratch;
STATIC_ASSERT_SIZEOF(Actor104000RangeScratch, 0xC);

/// 0x18-byte scratch taken from `0x1F8003FC` while applying a hit: the first
/// type-2 record's position, its offset from the model origin, the attack id,
/// the computed damage and the hit's yaw relative to the model's facing.
typedef struct Actor104000HitScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     id;
    /* 0x14 */ u16     dmg;
    /* 0x16 */ s16     angle;
} Actor104000HitScratch;
STATIC_ASSERT_SIZEOF(Actor104000HitScratch, 0x18);

/// 0xC-byte scratch taken from `0x1F8003FC` by the walking state: the offset
/// to the spawn point (later the camera target) and the clamped new yaw.
typedef struct Actor104000TurnScratch {
    /* 0x0 */ SVECTOR d;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad;
} Actor104000TurnScratch;
STATIC_ASSERT_SIZEOF(Actor104000TurnScratch, 0xC);

/// 0x14-byte scratch taken from `0x1F8003FC` by the lunge state: the offset to
/// the player (later the snap direction), the final yaw and the relative yaw.
typedef struct Actor104000AimScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ byte    pad_8[8];
    /* 0x10 */ s16     yaw;
    /* 0x12 */ s16     angle;
} Actor104000AimScratch;
STATIC_ASSERT_SIZEOF(Actor104000AimScratch, 0x14);

/// Argument block for the lunge state's message 0x3FF: the side-dependent
/// animation data and a count.
typedef struct Actor104000MsgArg {
    /* 0x0 */ void* field_0;
    /* 0x4 */ s32   field_4;
} Actor104000MsgArg;

/// 0x34-byte scratch from `G_SCRATCH_HEAD` for the death state's facing
/// rebuild: the rotation, the uniform scale applied to it and the yaw.
typedef struct Actor104000FaceScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
} Actor104000FaceScratch;
STATIC_ASSERT_SIZEOF(Actor104000FaceScratch, 0x34);

/// A `MATRIX` rotation block written a word at a time: the identity is stored
/// as 0x1000 / 0 pairs over the halfword elements, `m22` last.
typedef struct Actor104000MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor104000MatWords;

/// A per-state handler, indexed by `Actor104000Work.field_0`.
typedef void (*Actor104000StateFn)(Actor104000Ctx* arg0, Actor104000* arg1);

/// The nineteen handlers the tick copies onto its stack before dispatching.
typedef struct Actor104000StateTable {
    /* 0x00 */ Actor104000StateFn fn[19];
} Actor104000StateTable;
STATIC_ASSERT_SIZEOF(Actor104000StateTable, 0x4C);

/// 0x54-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_204000_8014A06C` to push a coordinate away from the obstacles in
/// a `GpRec18` table. `angle`/`ok` hold up to eight bearings collected from the
/// records, `i`/`j` are the loop cursors, and `blocked` is set when any record's
/// kind is 0x10000.
typedef struct Actor104000AvoidScratch {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s16      angle[8];
    /* 0x44 */ s8       ok[8];
    /* 0x4C */ s16      face;
    /* 0x4E */ s16      diff;
    /* 0x50 */ u8       i;
    /* 0x51 */ u8       j;
    /* 0x52 */ u8       count;
    /* 0x53 */ u8       blocked;
} Actor104000AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor104000AvoidScratch, 0x54);

/// 0x10-byte scratch the bearing helpers of the avoid walk nest inside
/// `Actor104000AvoidScratch`: an obstacle's offset, widened to words.
typedef struct Actor104000AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor104000AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor104000AvoidDelta, 0x10);

void func_actor_104000_80132C8C(Actor104000* arg0);
void func_actor_104000_80138698(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_104000_80138AA0(Actor104000Ctx* arg0, Actor104000* arg1);

#endif
