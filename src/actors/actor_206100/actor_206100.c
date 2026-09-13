#include "common.h"

#include "main/task.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014DEAC(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_80149ED0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014A70C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014AB3C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014AF74);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B0AC);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B698);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B8B4);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", ActorsShared80138404Table);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014BAA8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014BEC4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014C274);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E5C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014C458);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CB68);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CD08);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CE60);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CFF4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D14C);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E94);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D380);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D574);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D6F4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D8E8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DA28);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DD3C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DEAC);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E0C0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E228);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E7D4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E964);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014EA8C);

void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_556 = 1;
    work->field_554 = 1;
    work->field_544 = arg1;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014EB60);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014EC54);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014ED3C);

extern TaskDesc D_80147E48;

GpEnemy* func_actor_206100_8014EE2C(s32 arg0)
{
    GpEnemy*   enemy;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(&D_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->field_8  = arg0 << 12;
        enemy->field_3C = &D_actor_206100_80155134[(s16)arg0];
        obj             = (TmdObject*)enemy->task->extra;
        obj->field_24   = 0;
        obj->field_25   = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        return enemy;
    }
    return NULL;
}

void func_actor_206100_8014EEC0(Task* task)
{
    Actor206100ChildWork* child;
    GpRec18*              rec;
    GsCOORDINATE2*        coord;

    child               = (Actor206100ChildWork*)task->idMap;
    coord               = ((TmdObject*)task->extra)->field_8;
    task->killCountdown = 0;
    child->field_64     = 0x100;
    child->field_60     = 0;
    coord->sub          = &Gfx_ViewCoord;
    coord->flg          = 0;
    child->obj.field_18 = Gp_PackPair(&D_actor_206100_80155194, 0);
    child->obj.field_8  = ((TmdObject*)task->extra)->field_8;
    rec                 = child->rec;
    child->obj.field_C  = rec;
    child->obj.field_10 = 0;
    child->obj.field_12 = 0;
    child->obj.field_14 = 0;
    child->obj.field_1C = 0x140;
    child->obj.flags    = 1;
    Gp_LinkObj(3, &child->obj);
    Gp_InitRec18Table(rec, 2, 0);
    child->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    func_actor_206100_8014A70C(coord, (u16)child->field_60, 0, child->field_64 + 0x10002000);
    task->state++;
}
