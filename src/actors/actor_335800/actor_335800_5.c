#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_335800.h"

#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"

#include "main/task.h"

#include "main/tmd.h"

/// State handler 1 of the child block's table `D_actor_335800_80161E68`, the
/// step after the turn-to-face handler `func_actor_335800_80163BE0`: rotates
/// the constant forward offset `D_actor_335800_80161E78` through the root
/// part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the update loop, and advances `field_4C2` so
/// the dispatcher `func_actor_335800_80163B78` runs the next handler.
void func_actor_335800_80163CA0(Task* task)
{
    Actor335800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor335800Work*)task->work;

    vec = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

s32 func_actor_335800_80163E20(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3);

/// State handler 3 of `D_actor_335800_80161E68`, the child block's
/// turn-to-yaw step: Euler-extracts the root coordinate into `vec`, and while
/// the yaw gap to `work->field_4BA` is at least 0x41 it steps `vec.vy` toward
/// it by 0x40, taking the step on an `s32` widening of the extracted yaw;
/// otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `field_43F` byte, clearing the motion index and step.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
void func_actor_335800_80163D20(Task* arg0)
{
    Actor335800Work*      work;
    Actor335800MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor335800AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor335800Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_335800_80163E20(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor335800MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

extern void* D_actor_335800_80172E98[];

/// Animation preset handler, the same body as `func_actor_361100_801634D0`:
/// re-seeds the slot array off bank table `D_actor_335800_80172E98` when the
/// preset's bank index changes, then restarts or resets every slot and ticks them.
s32 func_actor_335800_80163E20(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3)
{
    Actor335800Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor335800Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        func_800B3F84(&work->anim, D_actor_335800_80172E98[work->field_43E], ext, work->field_30C,
                      work->slots);
    }
    work->field_43D = msg->field_4;
    if (msg->field_8 != 0 && work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_43D);
        }
    }
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_43C = 1;
    return 0;
}

/// Message 0x7D4 handler of the child block: places the root part at the
/// message's position and Euler angles, rebuilding the rotation from them and
/// clearing `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_335800_80163F3C(Task* task, s32 arg1, Actor335800Placement* args)
{
    Actor335800Coord* coord;

    coord             = (Actor335800Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_335800_80163FB8(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor335800Work*)task->work)->field_4C4 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_335800_80164098(void)
{
    return 0;
}
