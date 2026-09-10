#include "common.h"

#include "actors/actor_503500.h"

#include "main/task.h"
#include "main/tmd.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern void* D_actor_503500_80176520[];

void func_actor_503500_80146508(Task* arg0)
{
    TmdObject*            ext;
    Actor503500Effect4CC* work;

    work          = (Actor503500Effect4CC*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_503500_80146524(void)
{
}

s32 func_actor_503500_8014652C(Task* task, s32 arg1, Actor503500AnimPreset* msg)
{
    Actor503500Effect4CC* work;
    TmdObject*            ext;
    s32                   i;

    work = (Actor503500Effect4CC*)task->idMap;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_503500_80176520[work->field_43E], (GpAnimObj*)ext, work->field_30C,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
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
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_8", func_actor_503500_80146664);

s32 func_actor_503500_801466E0(Task* task, s32 arg1, s32 mode)
{
    TmdObject* ext;
    s32        ret;

    ext = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            ext->field_C = (ext->field_C | 0x80) & ~4;
            break;
        case 1:
            ext->field_C &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->field_C &= ~4;
            break;
        case 2:
            ext->field_C                                   |= 0x80;
            ((Actor503500Effect4CC*)task->idMap)->field_4C8 = mode;
            ext->field_C                                   |= 4;
            break;
        case 3:
            ext->field_C = (ext->field_C & ~0x80) | 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_801467C0(Task* task, s32 arg1, Actor503500ModeMsg* msg)
{
    Actor503500Effect4CC* work;

    work = (Actor503500Effect4CC*)task->idMap;
    switch (msg->mode) {
        case 0:
            work->field_4B0 = 0;
            work->field_4B4 = 0;
            work->field_4B8 = 0;
            break;
        case 1:
            work->field_4B0 = 0x0100F4DE;
            work->field_4B4 = 0xFF6DE9BE;
            work->field_4B8 = 0x68590;
            break;
        case 2:
            work->field_4B0 = 0x1371C7;
            work->field_4B4 = 0xBAAAA;
            work->field_4B8 = 0;
            work->field_4C0 = 1;
            break;
        case 3:
            task->exitCallback(task);
            break;
    }
    return 0;
}
