#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_17", func_actor_342400_8016B5B0);

void func_actor_342400_8016B744(Task* arg0)
{
    Actor342400Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor342400Work*)arg0->idMap;
    if (D_actor_342400_80173A84[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_17", func_actor_342400_8016B84C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_17", func_actor_342400_8016B914);

void func_actor_342400_8016B9A4(Task* arg0)
{
    Actor342400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor342400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

void func_actor_342400_8016BA3C(Task* arg0)
{
    Actor342400Work* work;
    s16              anim;
    s16              next;

    work = (Actor342400Work*)arg0->idMap;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = D_actor_342400_80173A98[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_17", func_actor_342400_8016BAF4);
