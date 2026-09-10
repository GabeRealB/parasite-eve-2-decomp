#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_8016AA58(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       model;

    work  = (Actor341700Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;

    work->field_412++;
    if ((s16)work->field_412 >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420++;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016AAB4);

void func_actor_341700_8016ABF4(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016AC0C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016AC64);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016C0F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016CC9C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_10", ActorsShared80135df4Table);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2)
{
    TmdObject* obj = arg0->field_2C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->field_C |= 4;
            break;
        case 3:
            obj->field_C = 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_8016CEB4);
