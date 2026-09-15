#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"
#include "gameplay/1BC.h"

typedef struct Actor521100Coord {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4C];
    /* 0x50 */ s32  field_50;
} Actor521100Coord;

typedef struct Actor521100Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor521100Coord* field_8;
    /* 0x0C */ s16               field_C;
} Actor521100Obj2C;

typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ byte                    pad_20[0xC];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

/// Per-actor work block. `Mem_Calloc(0x4B4, 0)` in `func_actor_521100_80135DDC`
/// stores the pointer both in `D_actor_521100_8016A3D8` and in the task's
/// 0x1C slot. `anim` sits at 0x40 and the slot array at 0x54, the same layout
/// as `Actor202900Work`; the nineteen slots are the ones
/// `func_actor_521100_80136724` ticks. The step / `animId` / clear-flag triple
/// at 0x47C / 0x480 / 0x482 is the same layout `Actor202900Work` uses.
///
/// `yaw` and `travel` are the cache the "walk to" placement opcode writes:
/// the heading it applied to the root coordinate and the remaining distance,
/// scaled by 20. Same offsets as `ActorsShared80133678Work`.
typedef struct Actor521100Work {
    /* 0x000 */ byte         pad_0[0x40];
    /* 0x040 */ GpAnimCtx    anim;
    /* 0x054 */ GpAnimSlot   slots[0x13];
    /* 0x34C */ byte         pad_34C[0x130];
    /* 0x47C */ s16          field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ byte         pad_47E[2];
    /* 0x480 */ u16          animId;    // animation id the slots are seeded with
    /* 0x482 */ s16          field_482; // cleared when a step body is started
    /* 0x484 */ byte         pad_484[0x2A];
    /* 0x4AE */ u16          yaw;
    /* 0x4B0 */ byte         pad_4B0[0x2];
    /* 0x4B2 */ s16          travel;
    /* 0x4B4 */ byte         pad_4B4[0x1A0];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x28];
    /* 0x680 */ s16          field_680;
    /* 0x682 */ byte         pad_682[0xA];
    /* 0x68C */ s16          field_68C;
    /* 0x68E */ s16          field_68E;
    /* 0x690 */ s16          field_690;
    /* 0x692 */ s16          field_692;
    /* 0x694 */ s16          field_694;
    /* 0x696 */ byte         pad_696[8];
    /* 0x69E */ s16          field_69E;
    /* 0x6A0 */ byte         pad_6A0[8];
    /* 0x6A8 */ s16          field_6A8;
    /* 0x6AA */ byte         pad_6AA[6];
    /* 0x6B0 */ s16          field_6B0;
    /* 0x6B2 */ s16          field_6B2;
} Actor521100Work;

/// 4-byte message record; the handler switches on the halfword at 0x2,
/// the same shape as `Actor361100Msg`.
typedef struct Actor521100Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100Msg;
STATIC_ASSERT_SIZEOF(Actor521100Msg, 0x4);

typedef struct Actor521100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor521100Ctx;

/// Argument block of the "walk to" script opcode: the world position to walk
/// to. Only the horizontal components are read.
typedef struct Actor521100Target {
    /* 0x00 */ VECTOR pos;
} Actor521100Target;
STATIC_ASSERT_SIZEOF(Actor521100Target, 0x10);

/// Argument block of the message handler `func_actor_521100_801369B8`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor202900AnimArgs`, and the same `(u16)` narrowing on the store into the
/// work block's `animId`. The stored id is `args->animId + 1`.
typedef struct Actor521100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor521100AnimArgs;

extern Actor521100Work* D_actor_521100_8016A3D8;

void func_actor_521100_80135414(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_801355C8(Actor521100* arg0);
void func_actor_521100_80135F2C(Task* task);
void func_actor_521100_80136724(void);
s32  func_actor_521100_801369B8(Task* task, s32 arg1, Actor521100AnimArgs* args);
s32  func_actor_521100_80136BE8(Task* task, s32 arg1, Actor521100Target* target);

#endif
