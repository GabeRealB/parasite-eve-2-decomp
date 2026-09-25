#ifndef ACTOR_210700_H
#define ACTOR_210700_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/task.h"
#include "main/tmd.h"

/// The actor's work block. The spawn handler `func_actor_210700_80149F90`
/// allocates it zeroed with `memCalloc(0x540, 0)` and keeps it in
/// `Task::work`. The front is the animation state the 0x7D3 message handler
/// drives - the context, its slots and the pose buffer handed to
/// `func_800B3F84` - followed by the light / colour matrices
/// `func_actor_210700_8014A208` points the model at, and the texture-upload
/// state the upload handler runs.
typedef struct Actor210700Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /// Non-zero once an animation has been started; gates the per-frame tick
    /// of slots 1..0x13.
    /* 0x474 */ s32 field_474;
    /// Animation id the slots were last reset to; -1 out of the spawn handler
    /// and whenever a new animation source is loaded.
    /* 0x478 */ s32 field_478;
    /// Index of the animation source last loaded from
    /// `D_actor_210700_801585C8`; -1 out of the spawn handler.
    /* 0x47C */ s32    field_47C;
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ byte   pad_4C0[0x78];
    /// Countdown reload value for the texture upload; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x538 */ s16 field_538;
    /// Frames left before the next texture-upload step.
    /* 0x53A */ u16 field_53A;
    /// Texture-upload step in progress, 0 when idle; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x53C */ s16 field_53C;
    /// Frames until the model buffers are freed; -1 when idle, latched to 2 by
    /// the 0x7D5 handler's mode 2.
    /* 0x53E */ s16 field_53E;
} Actor210700Work;
STATIC_ASSERT_SIZEOF(Actor210700Work, 0x540);

/// Payload of the 0x7D3 animation message. `field_0` indexes the animation
/// source table `D_actor_210700_801585C8`, `field_4` is the animation id every
/// slot 1..0x13 is reset to, and a non-zero `field_8` resets the slots through
/// `func_800B4114` instead of `Gp_AnimResetSlot`. Only the first three words
/// are read; the spawn handler's frame spaces its locals as if the block were
/// 0x18 bytes.
typedef struct _Actor210700Anim {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[0xC];
} Actor210700Anim;
STATIC_ASSERT_SIZEOF(Actor210700Anim, 0x18);

/// Payload of the 0x7D4 placement message: the world translation and the
/// Euler angles the model's root coordinate is set to.
typedef struct Actor210700Place {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor210700Place;
STATIC_ASSERT_SIZEOF(Actor210700Place, 0x18);

/// The model's root `GsCOORDINATE2` as the placement handler uses it: the
/// Euler angles are kept in the libgs `param` slot and handed straight to
/// `RotMatrix`.
typedef struct Actor210700Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor210700Coord;
STATIC_ASSERT_SIZEOF(Actor210700Coord, 0x4C);

/// The actor's three task states: spawn, per-frame tick and teardown.
extern TaskFuncTable3 D_actor_210700_80149E24;

/// Animation sources the 0x7D3 handler loads, indexed by its payload's
/// `field_0`.
extern void* D_actor_210700_801585C8[];

/// The actor's message table, parked in `Task::msgTable`: 0x7D3
/// `func_actor_210700_8014A224`, 0x7D4 `func_actor_210700_8014A344`, 0x7D5
/// `func_actor_210700_8014A3D4`, 0x7E0 `func_actor_210700_8014A4B0`.
extern GpMsgEntry D_actor_210700_801585D8[];

void func_actor_210700_80149E30(GpActorWork* arg0);
void func_actor_210700_8014A1E8(Task* task);
void func_actor_210700_8014A208(Task* arg0);
s32  func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3);
s32  func_actor_210700_8014A344(Task* task, s32 arg1, Actor210700Place* args, s32 arg3);

#endif
