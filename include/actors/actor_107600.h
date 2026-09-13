#ifndef ACTOR_107600_H
#define ACTOR_107600_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block this overlay hangs off `Actor107600.field_1C` (the task's
/// `Task::idMap` slot, which is not a `TaskIdMap` here). The display node at
/// +0x60 is the one the exit callback `func_actor_107600_80134920` hands back
/// to `Gp_UnlinkObj`. The state pair at +0x158/+0x15A is what
/// `func_actor_107600_80134B98` writes: the new state in `field_158` and its
/// sub-state counter cleared. `field_13E` is a free-running counter that
/// `func_actor_107600_80132CB8` bumps by one, and `field_144` is the phase the
/// destroy callback `func_actor_107600_80132AC0` tests against 2. The three
/// angles at +0x40 are what `func_actor_107600_80132B7C` rebuilds the model
/// root's rotation from, the same trio `ActorsShared80139948` reads at +0x80.
/// The trio at +0x50 is a second rotation set: `func_actor_107600_80134A50`
/// wraps each to 12 bits and feeds them to `Gfx_RotMatrixX/Y/Z` in turn.
///
/// `rec18` is the collision table `obj.field_C` points at and
/// `func_actor_107600_80134958` hands to `Gp_InitRec18Table` with count 8, so
/// it really runs to +0x140 and `field_13E` sits inside its last record.
/// `field_162` is the spawn variant `func_actor_107600_80132ED0` takes from
/// the low nibble of the task's own `Task::spawnArg1` high halfword
/// (`lhu 0x36` then `andi 0xF`): `func_actor_107600_80134958` picks the
/// display node's `field_1C` from it and `func_actor_107600_80134C54`
/// switches on it.
typedef struct Actor107600Work {
    /* 0x000 */ byte    pad_0[0x40];
    /* 0x040 */ u16     pitch;    // fed to RotMatrixX
    /* 0x042 */ u16     yaw;      // fed to func_8004BFF8
    /* 0x044 */ u16     roll;     // fed to RotMatrixZ
    /* 0x046 */ byte    pad_46[0xA];
    /* 0x050 */ u16     field_50; // fed to Gfx_RotMatrixX
    /* 0x052 */ u16     field_52; // fed to Gfx_RotMatrixY
    /* 0x054 */ u16     field_54; // fed to Gfx_RotMatrixZ
    /* 0x056 */ byte    pad_56[0xA];
    /* 0x060 */ GpObj   obj;
    /* 0x080 */ GpRec18 rec18[1]; // collision table; count 8 passed to Gp_InitRec18Table
    /* 0x098 */ byte    pad_98[0xA6];
    /* 0x13E */ u16     field_13E;
    /* 0x140 */ byte    pad_140[0x4];
    /* 0x144 */ s16     field_144;
    /* 0x146 */ byte    pad_146[0x12];
    /* 0x158 */ s16     field_158;
    /* 0x15A */ s16     field_15A;
    /* 0x15C */ byte    pad_15C[0x6];
    /* 0x162 */ s16     field_162; // spawn variant; 1 selects the 0x220 obj.field_1C
    /* 0x164 */ byte    pad_164[0x7];
    /* 0x16B */ u8      field_16B;
} Actor107600Work;

typedef struct Actor107600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor107600Work* field_1C;
} Actor107600;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

void func_actor_107600_801328CC(Task* arg0);
void func_actor_107600_80132A7C(Task* arg0);
void func_actor_107600_80132AC0(Task* arg0);
void func_actor_107600_80132B0C(Task* arg0);
void func_actor_107600_80132B7C(Task* arg0);
void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst);
void func_actor_107600_80132CB8(Actor107600* arg0);
void func_actor_107600_80134608(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_107600_801348A0(Task* arg0);
void func_actor_107600_80134904(Task* arg0);
void func_actor_107600_80134920(Task* arg0);
void func_actor_107600_801349E0(Task* arg0);
void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst);
void func_actor_107600_80134B98(Actor107600* arg0, s16 arg1);
void func_actor_107600_80134D10(Actor107600* arg0);
void func_actor_107600_80134D30(Actor107600* arg0);
void func_actor_107600_80134D50(Actor107600* arg0);
void func_actor_107600_80134D70(Actor107600* arg0);
void func_actor_107600_80134E5C(GsCOORDINATE2* arg0);

#endif
