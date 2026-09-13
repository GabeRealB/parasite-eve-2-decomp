#include "common.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014DEAC(Task* task);
void func_actor_206100_8014F8BC(Task* task);
void func_actor_206100_8014F970(Task* task);
void func_actor_206100_8014F9C4(Task* task);
void func_actor_206100_8014FA08(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014F524);

void func_actor_206100_8014F59C(void)
{
}

void func_actor_206100_8014F5A4(void)
{
}

void func_actor_206100_8014F5AC(void)
{
}

void func_actor_206100_8014F5B4(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->idMap;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F8BC,
        func_actor_206100_8014F970,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F608(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->idMap;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F9C4,
        func_actor_206100_8014FA08,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F65C(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    func_actor_206100_8014DEAC(task);
    Gp_MsgPlayerWeapon(0);
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F69C(Task* task)
{
    u16              timer;
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    func_actor_206100_8014DEAC(task);
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5A) {
        work->field_51E = 0;
        work->field_522 = work->field_522 + 1;
    }
}

void func_actor_206100_8014F6F8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* anim;

    work            = (Actor206100Work*)task->idMap;
    work->field_51E = 0;
    anim            = (Actor206100Work*)task->idMap;
    anim->field_524 = 8;
    anim->field_51A = 8;
    anim->field_510 = 7;
    anim->field_50C = 1;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F738(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_524 = 0xA;
    work->field_51A = 0x10;
    work->field_510 = 1;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F770(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->idMap;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5B) {
        next            = (Actor206100Work*)task->idMap;
        next->field_520 = 3;
        next->field_522 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014F7B4);

void func_actor_206100_8014F878(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->idMap;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x3D) {
        next            = (Actor206100Work*)task->idMap;
        next->field_520 = 1;
        next->field_522 = 0;
    }
}

void func_actor_206100_8014F8BC(Task* task)
{
    Actor206100Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor206100Work*)task->idMap;
    work->field_524 = 6;
    work->field_51A = 0x10;
    work->field_510 = 0xA;
    work->field_50C = 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
    work->field_522 = work->field_522 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014F970);

void func_actor_206100_8014F9C4(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_524 = 8;
    work->field_51A = 0x10;
    work->field_510 = 0xE;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_526 = work->field_536;
    work->field_522 = work->field_522 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014FA08);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014FAE4);

void func_actor_206100_8014FBE4(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work  = (Actor206100Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    SndEvt_EnqueueType7(0x551E0002, 1);
    Gp_ApplyAreaRecs(&D_8018590C);
    work->field_526 = work->field_536;
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)task, 0);
    GameFlag_SetNibble(0xF3, 1);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj_364);
    Gp_UnlinkObj(&work->obj_414);
    work->field_51E = 0;
    work->field_520 = work->field_520 + 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014FCD4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014FDE8);
