#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern Actor143000Rect  D_actor_143000_80134580[];
extern TaskDesc         D_actor_143000_801350B0;
extern Actor143000Spawn D_actor_143000_80135C08;
extern u8               D_actor_143000_80135C0C;
extern s16              D_80114D08;
extern s8               D_8007216C;
extern u8               D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801336E8);

void func_actor_143000_80133800(Task* arg0)
{
    Actor143000Work* work = (Actor143000Work*)arg0->idMap;

    Display_ReleaseRef();
    Game_Session->field_66 = 0;
    if (work->field_C == 0) {
        D_80114D08             = 0xA;
        Game_Session->field_1  = 0;
        Game_Session->field_68 = 0;
        D_801153F4             = 0;
        D_8007216C             = D_actor_143000_80135C0C;
        Gp_MsgPlayer3F3(1);
    } else {
        Task_SpawnFromTable(&D_actor_143000_801350B0, 1, 0, (s32)&D_actor_143000_80135C08);
    }
    Task_Kill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, work->field_C);
}

void func_actor_143000_801338C8(Actor143000* arg0)
{
    arg0->field_1C->field_4 = 0;
    arg0->field_30          = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801338E0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801339CC);

void func_actor_143000_80133AC0(Actor143000* arg0)
{
    u16 count = (u16)arg0->field_2A - 1;

    arg0->field_2A = count;
    if ((s16)count <= 0) {
        arg0->field_30 = 5;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133AE8);

void func_actor_143000_80133C2C(void)
{
    Actor143000Rect* p = D_actor_143000_80134580;

    if (p->field_8 != -1) {
        do {
            func_actor_143000_80133334(p, 0, 0xFF, 0);
            p++;
        } while (p->field_8 != -1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", ActorsShared8013845cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133CF0);
