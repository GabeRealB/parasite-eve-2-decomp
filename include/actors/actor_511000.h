#ifndef ACTOR_511000_H
#define ACTOR_511000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. `func_actor_511000_80133034`
/// allocates it with `Mem_Calloc(0x70, 0)`. `light` / `color` are the matrices
/// the TMD object's `field_1C` / `field_20` are republished from.
/// `field_8` is the Tmd_FreeBuffers countdown (`-1` disables it);
/// `field_C` is the dest buffer published through `D_actor_511000_80147EB0`.
typedef struct Actor511000Work {
    /* 0x00 */ byte   pad_0[8];
    /* 0x08 */ s32    field_8;
    /* 0x0C */ u16    field_C;
    /* 0x0E */ byte   pad_E[0x21];
    /* 0x2F */ s8     field_2F;
    /* 0x30 */ MATRIX light;
    /* 0x50 */ MATRIX color;
} Actor511000Work;
STATIC_ASSERT_SIZEOF(Actor511000Work, 0x70);

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs
/// `param`, and `super` at 0x48) holds the Euler angles
/// `func_actor_511000_801336E0` writes and then hands straight to `RotMatrix`,
/// the same reuse `ActorsShared8013231cCoord` documents for its own overlay.
typedef struct Actor511000Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor511000Coord;
STATIC_ASSERT_SIZEOF(Actor511000Coord, 0x4C);

/// Work block of the spawner this overlay's model-attach handlers parent to,
/// reached through the parent task's `Task::idMap`. The spawn handler
/// `func_actor_511000_80133958` allocates it (`Mem_Calloc(0x488, 0)`), hands
/// `anim` / `slots` / `field_30C` to `func_800B3F84`, and points its own model
/// at the two matrices; the three children it spawns do the same. The same
/// pair sits at the same offsets in `ActorsShared80135b64Work` /
/// `Actor02000Work`.
typedef struct Actor511000ParentWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[1];
    /* 0x03C */ byte       pad_3C[0x2D0];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX     field_45C; ///< light matrix, handed to TmdObject::field_1C
    /* 0x47C */ s32        field_47C; ///< cleared by the spawn handler
    /* 0x480 */ byte       pad_480[8];
} Actor511000ParentWork;
STATIC_ASSERT_SIZEOF(Actor511000ParentWork, 0x488);

/// Work block `func_actor_511000_80132480` allocates (`Mem_Calloc(0x4D4, 0)`)
/// and parks in that task's `Task::idMap` slot. Distinct from
/// `Actor511000Work`: this variant's light/color pair sits at 0x484 / 0x4A4
/// and is republished onto model part 1, not the root coordinate.
/// `func_actor_511000_80133DEC` treats the block as a `GpAnimCtx` at offset 0
/// and reseeds slots from `field_47C`.
/// The spawn handler seeds the three -1 words at 0x478 / 0x47C / 0x480 and
/// parks the two spawned tasks at 0x4C4 / 0x4C8; `field_4D2` is cleared.
typedef struct Actor511000Work2 {
    /* 0x000 */ byte pad_0[0x474];
    /* 0x474 */ s32  field_474; ///< nonzero while the tick state steps animation slots 1..19
    /* 0x478 */ s32  field_478; ///< -1 out of the spawn handler
    /* 0x47C */ s32  field_47C; ///< animation id handed to `Gp_AnimResetSlot`
    /* 0x480 */ union {
        s32 word;               ///< seeded to -1 whole by the spawn handler
        s16 half;               ///< the halfword `func_actor_511000_80133DEC` clears after the slot reseed
    } field_480;
    /* 0x484 */ MATRIX light;
    /* 0x4A4 */ MATRIX color;
    /* 0x4C4 */ Task*  field_4C4; ///< task spawned from the table's index 1
    /* 0x4C8 */ Task*  field_4C8; ///< task spawned from the table's index 2
    /* 0x4CC */ s16    field_4CC; ///< set to 1 alongside `field_4D0` by the message-0x7E0 handler's mode 3
    /* 0x4CE */ u16    field_4CE; ///< upload countdown the state handlers at 0x80131E78 / 0x80132048 run down, reloaded from `field_4CC` on underflow
    /* 0x4D0 */ s16    field_4D0; ///< set to 1 alongside `field_4CC` by the message-0x7E0 handler's mode 3
    /* 0x4D2 */ s16    field_4D2; ///< cleared by the spawn handler
} Actor511000Work2;
STATIC_ASSERT_SIZEOF(Actor511000Work2, 0x4D4);

/// Animation preset `func_actor_511000_80133DEC` takes as `arg2`. `field_4`
/// is the animation id copied into `Actor511000Work2::field_47C`.
typedef struct Actor511000AnimPreset {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor511000AnimPreset;

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor143900Msg` and `Actor335800Msg`. This
/// overlay's 0x7DB handler, `func_actor_511000_8013287C`, reads the halfword
/// at 0x2.
typedef struct Actor511000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor511000Msg;
STATIC_ASSERT_SIZEOF(Actor511000Msg, 0x4);

s32  func_actor_511000_8013287C(GpActorWork* arg0, s32 arg1, Actor511000Msg* msg);
void func_actor_511000_80132E6C(Actor511000Work* work);
void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index);
void func_actor_511000_80133760(Task* task);
void func_actor_511000_801337F0(Task* task);
s32  func_actor_511000_80133DEC(Task* task, s32 arg1, Actor511000AnimPreset* preset);

#endif // ACTOR_511000_H
