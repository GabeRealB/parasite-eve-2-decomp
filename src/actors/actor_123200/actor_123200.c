#include "common.h"

#include "actors/actor_123200.h"

#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013215C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801324A4);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801329F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80132B94);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801332E0);

s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013352C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133820);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801339F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133BA0);

s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor123200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor123200Work*)task->idMap;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
        case 4:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133EDC);
