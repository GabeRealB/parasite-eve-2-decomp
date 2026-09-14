#ifndef ACTOR_141000_H
#define ACTOR_141000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

typedef struct Actor141000 Actor141000;

typedef struct Actor141000Point {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[4];
} Actor141000Point;

/// Work block allocated by `func_actor_141000_8013392C` (`Mem_Calloc(0x4CC)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_141000_801339DC` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C9 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. `target` is the world position the
/// actor turns to face, written by the state handler at 0x801336DC.
typedef struct Actor141000Work {
    /* 0x000 */ byte    pad_0[0x8];
    /* 0x008 */ byte    pad_8[0x2];
    /* 0x00A */ u16     field_A; // Z scale the state-0 ramp at 0x80132E24 climbs by 0x100 a frame and clamps at 0x1000
    /* 0x00C */ u16     field_C; // state index; `func_actor_141000_80132D3C` dispatches through it as `(s16)`
    /* 0x00E */ u16     field_E; // per-state frame counter
    /* 0x010 */ byte    pad_10[0x42D];
    /* 0x43D */ s8      field_43D;
    /* 0x43E */ s8      field_43E;
    /* 0x43F */ byte    pad_43F[0x1];
    /* 0x440 */ MATRIX  light;
    /* 0x460 */ MATRIX  color;
    /* 0x480 */ VECTOR3 target;
    /* 0x48C */ byte    pad_48C[0x4];
    /* 0x490 */ VECTOR3 step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte    pad_49C[0x4];
    /* 0x4A0 */ s32     field_4A0;
    /* 0x4A4 */ s32     field_4A4;
    /* 0x4A8 */ s32     field_4A8;
    /* 0x4AC */ byte    pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ byte    pad_4B8[0xA];
    /* 0x4C2 */ u16     field_4C2; // main-body state index; the dispatcher reads it back sign-extending
    /* 0x4C4 */ byte    pad_4C4[0x4];
    /* 0x4C8 */ s8      field_4C8; // variant the 0x7DB handler latches; 0 picks anim 10, non-zero anim 2
    /* 0x4C9 */ s8      field_4C9;
    /* 0x4CA */ byte    pad_4CA[0x2];
} Actor141000Work;
STATIC_ASSERT_SIZEOF(Actor141000Work, 0x4CC);

/// The local-space offset the actor's state-1 handler
/// (`func_actor_141000_80133B28`) rotates into `Actor141000Work::step`: straight
/// ahead along the part's own axis, halved first while `field_4C8` is clear.
/// Each overlay keeps its own copy in `.rodata` -- this one follows the handler
/// table `D_actor_141000_80131E58`, which is why `actor_141000_3` carries the
/// run -- so the address comes from the per-overlay symbol map.
extern VECTOR D_actor_141000_80131E68;

/// Work block of the overlay's controller task -- the one whose three `Task`
/// states are `D_actor_141000_80131E30`, which spawns the actor and then drives
/// its model through the four animation states at 0x80131E3C.
///
/// `func_actor_141000_80132C7C` allocates it with `Mem_Calloc(0x10, 0)` and
/// parks it in that task's `Task::idMap` slot, so the size below is the
/// allocation and not a guess; the slot is not a `TaskIdMap` here.
///
/// `field_0` is armed at 0xFFF by the spawn state, `frames` is the counter the
/// state at 0x80132EF4 masks with 7 to pace the actor's spawns, `scale` is the
/// Z scale the state at 0x80132E24 ramps by 0x100 a frame up to 0x1000, `state`
/// is the index `func_actor_141000_80132D3C` dispatches through, and `ticks` is
/// the per-state frame counter the state at 0x80132EB0 holds for 0x1F frames.
///
/// That head is the same `scale` / `state` / `ticks` halfword triple
/// `Actor141000Work` declares at 0xA/0xC/0xE: the two states that touch only
/// those three fields -- `func_actor_141000_80132E24` and
/// `func_actor_141000_80132EB0` -- reach this block as `(Actor141000Work*)`.
typedef struct Actor141000CtrlWork {
    /* 0x0 */ s32  field_0; // armed at 0xFFF by the spawn state
    /* 0x4 */ byte pad_4[0x4];
    /* 0x8 */ u16  frames;
    /* 0xA */ u16  scale;
    /* 0xC */ u16  state;
    /* 0xE */ u16  ticks;
} Actor141000CtrlWork;
STATIC_ASSERT_SIZEOF(Actor141000CtrlWork, 0x10);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg` and `Actor342400Msg`, whose
/// halfword at 0x2 chooses the variant this handler latches.
typedef struct Actor141000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor141000Msg;
STATIC_ASSERT_SIZEOF(Actor141000Msg, 0x4);

s32  func_actor_141000_80132FD0(GsCOORDINATE2* arg0, s32 arg1);
void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1);
void func_actor_141000_801339DC(Task* arg0);
void func_actor_141000_80131E94(Actor141000* arg0, Actor141000Point* arg1, s32 arg2);
void func_actor_141000_801323F0(Actor141000* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3);
void func_actor_141000_80133260(Actor141000* arg0);

#endif
