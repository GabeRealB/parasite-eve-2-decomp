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

void func_actor_341700_8016A8EC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016A8F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016A98C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016AA58);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016AAB4);

void func_actor_341700_8016ABF4(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016AC0C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016AC64);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016C0F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016CC9C);

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

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016CEB4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016CF48);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016D018);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016D130);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016D2B8);

void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->idMap)->field_4 != 0) {
        model              = (TmdObject*)arg1->extra;
        arg0->node.field_4 = 1;
        model->field_C     = 0;
        Tmd_AllocBuffers(model);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_3", func_actor_341700_8016D32C);
