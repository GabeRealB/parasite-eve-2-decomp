#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern s8  D_8007216C;
extern s16 D_80114D08;

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", func_actor_548100_80134DBC);

void func_actor_548100_80134E0C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    D_80114D08 = 0xA;
    Display_ReleaseRef();
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_8007216C             = 3;
    /* Without the barrier GCC fills Task_Kill's delay slot with the byte store. */
    SOFT_BARRIER();
    Task_Kill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", func_actor_548100_80134E94);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", func_actor_548100_80134F64);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", func_actor_548100_80134FEC);

void func_actor_548100_80135124(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        arg0->state = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", ActorsShared8013845cSub0);
