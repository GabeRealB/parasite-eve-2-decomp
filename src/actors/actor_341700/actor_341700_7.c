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

void func_actor_341700_8016966C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C  = model->field_C | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void func_actor_341700_801696C8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void func_actor_341700_801696E0(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169724);

void func_actor_341700_801697B8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void func_actor_341700_801697D4(Task* arg0)
{
    Actor341700Work* work;
    u16              ticks;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((Game_Session->field_7 == 4) && ((u32)(Game_Session->field_6 - 0x27) < 2U) && (Game_Session->field_9 == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169888);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_8016999C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169AB0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169B40);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169BC8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169C50);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169CC4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_7", func_actor_341700_80169D54);
