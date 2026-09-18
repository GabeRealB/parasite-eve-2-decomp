#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_335800.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_335800_80163AA0`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_335800_80172EA8[];

s32 func_actor_335800_801632A4(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3);

/// Turn-to-face handler: Euler-extracts the root coordinate into `vec`, and
/// while the yaw gap to `work->field_4F2` is at least 0x41 it steps `vec.vy`
/// toward it by 0x40, taking the step on an `s32` widening of the extracted
/// yaw; otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `field_477` byte, clearing the two body counters.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
void func_actor_335800_801631A4(Task* arg0)
{
    Actor335800MainWork*  work;
    Actor335800MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor335800AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor335800MainWork*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F2 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4F2;
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_335800_801632A4(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
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

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_335800_8016EAD8[];

/// Animation preset handler for the parent block, the 20-slot twin of
/// `func_actor_335800_80163E20`: re-seeds the slot array off bank table
/// `D_actor_335800_8016EAD8` when the preset's bank index changes, then
/// restarts or resets every slot and ticks them.
s32 func_actor_335800_801632A4(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3)
{
    Actor335800MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor335800MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_335800_8016EAD8[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801633C0);

s32 func_actor_335800_8016343C(Task* task, s32 arg1, s32 mode)
{
    Actor335800MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    s32                  ret;

    work = (Actor335800MainWork*)task->work;
    obj  = task->extra;
    objA = work->field_4FC->extra;
    objB = work->field_500->extra;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C   |= 0x80;
            work->field_506 = mode;
            obj->field_C   |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    objA->field_C = obj->field_C;
    objB->field_C = obj->field_C;
    return ret;
}

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, Actor335800Msg* arg2, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->work;
    if (arg2->field_2 == 0) {
        work->field_504 = 0;
    }
    return 0;
}

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_335800_80163B70(Task* arg0);
void func_actor_335800_80163B78(Task* arg0);

/// Per-frame tick of the child block: runs the motion handler `field_4C0`
/// selects, adds the 16.16 velocity `step` onto the accumulator `field_4A0`,
/// moves the coordinate by the integer part and keeps only the fraction, then
/// ticks the animation slots, draws the ground shadow and rebuilds the colour
/// matrix while visible, and counts `field_4C4` down to the buffer free.
void func_actor_335800_80163568(Task* task)
{
    TmdObject*       ext      = task->extra;
    Actor335800Work* work     = (Actor335800Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_335800_80163B70, func_actor_335800_80163B78 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    funcs[work->field_4C0](task);
    coord              = ((TmdObject*)task->extra)->field_8;
    work->field_4A0   += work->step.vx;
    work->field_4A4   += work->step.vy;
    work->field_4A8   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1] += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2] += (s16)(work->field_4A8 >> 16);
    coord->flg         = 0;
    work->field_4A0    = (u16)work->field_4A0;
    work->field_4A4    = (u16)work->field_4A4;
    work->field_4A8    = (u16)work->field_4A8;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
    }
    if (work->field_4C4 >= 0) {
        if (work->field_4C4 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C4--;
    }
}

s32 func_actor_335800_80163E20(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3);

/// Approach test for the child block, the twin of `func_actor_335800_80162B3C`:
/// once the X/Z distance to `target` stops shrinking below `limit`, plays anim
/// 0x7D3, clears `step` and advances the state; otherwise records the distance.
void func_actor_335800_8016373C(Task* arg0)
{
    Actor335800Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor335800AnimPreset preset;

    work  = (Actor335800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_335800_80163E20(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4C2++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

extern void* D_actor_335800_80172E98[];

/// Placement handler for the child block, the twin of
/// `func_actor_335800_80162C80`: stores the spawn position and rotation, then
/// applies a start preset exactly as `func_actor_335800_80163E20` does
/// (inlined here).
s32 func_actor_335800_80163880(Task* task, s32 arg1, Actor335800Placement* place, Actor335800SpawnAnim* anim)
{
    Actor335800Work*       work;
    Actor335800Work*       w;
    Actor335800AnimPreset  preset;
    Actor335800AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor335800Work*)task->work;
    w->field_4C0   = 1;
    w->field_4C2   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4B8   = place->rot.vx;
    w->field_4BA   = place->rot.vy;
    w->field_4BC   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_43F   = anim->field_4;
    } else {
        preset.field_4 = 0xD;
        w->field_43F   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor335800Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        func_800B3F84(&work->anim, D_actor_335800_80172E98[work->field_43E], (GpAnimObj*)ext, work->field_30C,
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

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163A34);

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the two -1 bytes,
/// the -1 halfword and three cleared words the work's own init expects,
/// republishes the light and colour matrices onto the display object, then
/// installs the message table and the exit handler. An allocation failure
/// ends the task instead of leaving a half-built actor behind.
void func_actor_335800_80163AA0(Task* arg0)
{
    Actor335800Work* work;

    work = Mem_Calloc(sizeof(Actor335800Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C4 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_335800_80163B54(arg0);

    arg0->field_24     = D_actor_335800_80172EA8;
    arg0->exitCallback = func_actor_335800_80163B34;
    arg0->state       += 1;
}

void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra;
    work          = (Actor335800Work*)arg0->work;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_335800_80163B70(Task* arg0)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163B78);
