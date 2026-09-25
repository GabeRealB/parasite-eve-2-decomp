#ifndef ACTOR_104600_H
#define ACTOR_104600_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// A `GpEnemy` seen through its node's flag byte alone: `field_14` is
/// `GpEnemy::node.flags`.
typedef struct Actor104600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor104600Ctx;

/// The 0x2E4-byte work block of the package's first enemy, which both of its
/// spawn handlers allocate with `memCalloc` and park in `Task::work`. After the
/// animation context and its three slots come the colour and light matrices the
/// model is pointed at, then four `GpObj` bodies, each followed by the
/// `GpRec18` table its `ctx.recs` names.
typedef struct Actor104600Work {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[3];
    /* 0x08C */ byte       field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX     field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX     field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ GpObj      objFC;
    /* 0x11C */ GpRec18    rec11C;
    /* 0x134 */ GpObj      obj134;
    /* 0x154 */ GpRec18    rec154[4]; // the body's contact table; also the enemy's `recs`
    /* 0x1B4 */ GpObj      obj1B4;
    /* 0x1D4 */ GpRec18    rec1D4;
    /* 0x1EC */ GpObj      obj1EC;
    /* 0x20C */ GpRec18    rec20C;
    /* 0x224 */ byte       pad_224[0x50];
    /* 0x274 */ VECTOR3    field_274; // root translation before the last step
    /* 0x280 */ byte       pad_280[4];
    /* 0x284 */ GpEffArg   field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX     field_28C; // root transform saved when the enemy dies
    /* 0x2AC */ s32        field_2AC; // scale factor of the model's second part
    /* 0x2B0 */ s16        field_2B0; // heading, stepped 0x20 a frame toward the player
    /* 0x2B2 */ s16        field_2B2; // reaction state the per-frame dispatch switches on
    /* 0x2B4 */ s16        field_2B4; // phase of the death sequence
    /* 0x2B6 */ s16        field_2B6; // frames spent in the death phase
    /* 0x2B8 */ s16        field_2B8; // animation id the work is playing
    /* 0x2BA */ s16        field_2BA; // id the two helper slots last saw
    /* 0x2BC */ u16        field_2BC; // frames spent on the current id
    /* 0x2BE */ s16        field_2BE; // step length along the facing
    /* 0x2C0 */ byte       pad_2C0[6];
    /* 0x2C6 */ s16        field_2C6;
    /* 0x2C8 */ s16        field_2C8; // live stage: 1 alive, 2 dying
    /* 0x2CA */ s16        field_2CA; // Y scale folded onto the saved transform
    /* 0x2CC */ s16        field_2CC;
    /* 0x2CE */ s16        field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16        field_2D0; // frames until the next idle sound
    /* 0x2D2 */ s16        field_2D2; // non-zero: the animation rebind is suppressed
    /* 0x2D4 */ u16        field_2D4; // frame or event counter of the dying stages
    /* 0x2D6 */ s16        field_2D6; // spawn arg's low half; picks the sound set
    /* 0x2D8 */ s16        field_2D8; // latched once the dormant enemy is touched
    /* 0x2DA */ s16        field_2DA; // non-zero: the death spawns a final effect
    /* 0x2DC */ s16        field_2DC; // spawn arg's high half
    /* 0x2DE */ s16        field_2DE; // fall speed while dropping into place
    /* 0x2E0 */ s16        field_2E0; // non-zero once the drop has hit something
    /* 0x2E2 */ s16        field_2E2; // non-zero: the drop has been armed
} Actor104600Work;
STATIC_ASSERT_SIZEOF(Actor104600Work, 0x2E4);

/// The 0x18 bytes `Actor04600_Fn00FD8` takes off the scratch stack: `vec` is
/// the offset from the model's coordinate to the player and `rot` the rotation
/// rebuilt from the new heading.
typedef struct Actor104600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor104600RotScratch;
STATIC_ASSERT_SIZEOF(Actor104600RotScratch, 0x18);

/// The 0x2B0-byte work block of the package's second enemy, allocated by its
/// spawn handler and parked in `Task::work`. It carries three `GpObj` bodies:
/// the first points its `ctx.d4rec` at the `GpActorD4Rec` after it, the other
/// two point at their own `GpRec18` tables.
typedef struct Actor104600Enemy2Work {
    /* 0x000 */ GpAnimCtx    context;
    /* 0x014 */ GpAnimSlot   slots[3];
    /* 0x08C */ byte         field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX       field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX       field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ GpObj        field_FC;
    /* 0x11C */ GpActorD4Rec field_11C;
    /* 0x134 */ GpRec18      field_134[1];
    /* 0x14C */ GpObj        field_14C;
    /* 0x16C */ GpRec18      field_16C[1];
    /* 0x184 */ GpObj        field_184;
    /* 0x1A4 */ GpRec18      field_1A4[4]; // the enemy's `recs`
    /* 0x204 */ byte         pad_204[0x50];
    /* 0x254 */ s32          field_254;    // position restored when the push-back conflicts
    /* 0x258 */ s32          field_258;
    /* 0x25C */ s32          field_25C;
    /* 0x260 */ byte         pad_260[4];
    /* 0x264 */ MATRIX       field_264; // root transform the dying enemy refolds
    /* 0x284 */ byte         pad_284[2];
    /* 0x286 */ s16          field_286; // reaction state
    /* 0x288 */ s16          field_288; // non-zero once the death has unlinked the bodies
    /* 0x28A */ s16          field_28A; // frames spent in the current state
    /* 0x28C */ s16          field_28C; // animation id the work is playing
    /* 0x28E */ s16          field_28E; // id the two helper slots last saw
    /* 0x290 */ s16          field_290; // frames spent on the current id
    /* 0x292 */ s16          field_292;
    /* 0x294 */ byte         pad_294[6];
    /* 0x29A */ s16          field_29A;
    /* 0x29C */ byte         pad_29C[4];
    /* 0x2A0 */ s16          field_2A0; // Y scale folded onto the saved transform
    /* 0x2A2 */ byte         pad_2A2[2];
    /* 0x2A4 */ s16          field_2A4; // light blend, 0..0x12
    /* 0x2A6 */ s16          field_2A6; // non-zero: the blend is rising
    /* 0x2A8 */ s16          field_2A8; // frames until the next blend turn
    /* 0x2AA */ s16          field_2AA; // latched by a hit
    /* 0x2AC */ s16          field_2AC; // placement mode; picks the sound set
    /* 0x2AE */ byte         pad_2AE[2];
} Actor104600Enemy2Work;
STATIC_ASSERT_SIZEOF(Actor104600Enemy2Work, 0x2B0);

/// 0x38-byte block `Actor04600_Fn0346C` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player.
typedef struct Actor104600HitScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
} Actor104600HitScratch;
STATIC_ASSERT_SIZEOF(Actor104600HitScratch, 0x38);

/// Free-running random state: `state = state * 5 + 0x71357911`, read back
/// through the high halfword.
extern u32 Gp_LcgState;

extern u8 D_801153F4;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

void Actor04600_Fn003D4(Task* arg0);
void Actor04600_Fn00978(Task* arg0);
void Actor04600_Fn01E0C(Task* arg0);
void Actor04600_Fn02618(Task* arg0);
void Actor04600_Fn0272C(Task* task);
void Actor04600_Fn027BC(Task* arg0);
void Actor04600_Fn02870(void* arg0, Task* arg1);
void Actor04600_Fn028E0(Task* task);
void Actor04600_Fn0294C(Task* arg0, GsCOORDINATE2* arg1);
void Actor04600_Fn02B14(Task* arg0);
void Actor04600_Fn02C08(Task* task);
void Actor04600_Fn02CD4(Task* task);
void Actor04600_Fn030A8(Task* arg0);
void Actor04600_Fn0346C(Task* arg0);
void Actor04600_Fn03CEC(Task* arg0);
void Actor04600_Fn03D54(Task* task);
void Actor04600_Fn03E10(Task* arg0);
void Actor04600_Fn03EC0(void* arg0, Task* arg1);
void Actor04600_Fn03F30(Task* task);
void Actor04600_Fn0400C(Task* arg0);
void Actor04600_Fn04100(Task* task);

/// Rebinds the first enemy's animation id to its two helper slots unless
/// `field_2D2` suppresses it: a changed id is remembered, its frame count
/// restarts and both slots switch to it; otherwise the count ticks and the
/// slots advance.
static __inline__ void Actor04600_TickAnim(Task* task)
{
    Actor104600Work* work = (Actor104600Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
    }
}

/// Colours the enemy from the world position of `coord`, staged in a `VECTOR`
/// taken off the scratch stack.
static __inline__ void Actor04600_UpdateColor(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block         = (VECTOR*)(*(u8**)0x1F8003FC - 0x10);
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    *(VECTOR**)0x1F8003FC = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}

#endif
