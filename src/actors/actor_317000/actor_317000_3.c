#include "common.h"

#include "actors/actor_317000.h"

#include "gameplay/3A34.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_317000_80162A10(Task* arg0, s32 arg1, Actor317000AnimPreset* arg2, s32 arg3);

void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*       ext;
    Actor317000Work* work;

    ext           = arg0->extra;
    work          = (Actor317000Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_317000_80162760(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162768);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_801627D0);

/// State handler at index 1 of `D_actor_317000_80161E30`, reached by the
/// `field_4C2` advance this body ends with: rotates the constant local-space
/// offset `D_actor_317000_80161E40` through the root part's matrix into
/// `work->step`, then raises the flag at 0x4C4 and moves the dispatcher on.
/// The same body as `func_actor_335800_80163CA0` / `ActorsShared80132920`,
/// minus their per-axis stop threshold.
void func_actor_317000_801628D8(Task* task)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = (Actor317000Work*)task->idMap;

    vec = D_actor_317000_80161E40;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->field_4C4 = 1;
    work->field_4C2++;
}

/// State handler at index 2 of `D_actor_317000_80161E30`, the step after
/// `func_actor_317000_801628D8` and reached by the `field_4C2` advance that
/// body ends with. While the root coordinate's Y is below -0x30 it does
/// nothing; above it the rise is over: the local-space `work->step` the
/// previous body wrote is cleared, the animation is re-applied through message
/// 0x7D3 with the latched `field_43F` state, and sound 0x400A000B is queued
/// panned and attenuated from the root coordinate's matrix. The `field_4C4`
/// flag the previous body raised is cleared and the dispatcher advances again.
void func_actor_317000_80162950(Task* arg0)
{
    GsCOORDINATE2*        coord;
    Actor317000Work*      work;
    Actor317000AnimPreset preset;
    s32                   pan;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor317000Work*)arg0->idMap;
    if (coord->coord.t[1] < -0x30) {
        return;
    }
    preset.field_0  = 0;
    preset.field_4  = work->field_43F;
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 0;
    func_actor_317000_80162A10(arg0, 0x7D3, &preset, 0);
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(0x400A000B, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));

    work->step.vx   = 0;
    work->step.vy   = 0;
    work->step.vz   = 0;
    work->field_4C4 = 0;
    work->field_4C2++;
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162A10);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E50);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E60);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E64);
