#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern s32      D_actor_136100_8013F180[];
extern TaskDesc ActorsShared80134898Desc;
extern Task*    D_actor_136100_8014078C;
extern s8       D_8007272D;

void func_actor_136100_801348C8(void)
{
    Task_SpawnFromTable(&ActorsShared80134898Desc, 5, 9, 0);
}

void func_actor_136100_801348F8(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

void func_actor_136100_80134924(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    if (work->field_4EC == 0) {
        work->field_4EC = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_136100_80134964(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    if (work->field_4EC != 0) {
        Gp_SpawnWeaponEff();
        work->field_4EC = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_136100_801349B4(s32 arg0)
{
    GameFlag_SetNibble(0x46, 0);
    GameFlag_SetNibble(0x4C, 3);
    if (arg0 == 0) {
        GameFlag_SetNibble(0x4B, 6);
    } else {
        D_8007272D = 8;
        GameFlag_SetNibble(0x4B, 0);
    }
}

/// Reports the live entries of the actor's pointer table to the slot-3 task:
/// counts the leading non-null words of `D_actor_136100_8013F180` and hands
/// the table and that count to message 0x3F7.
void func_actor_136100_80134A18(Task* arg0)
{
    Actor136100Work*  work = (Actor136100Work*)arg0->idMap;
    Actor136100Msg3F7 msg;
    s32               n;

    n = 0;
    while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {
        n += 1;
    }
    msg.table = &D_actor_136100_8013F180[0];
    msg.count = n & 0xFFFF;
    Gp_DispatchMsg(work->field_4B4, 0x3F7, (s32)&msg, 0);
}
