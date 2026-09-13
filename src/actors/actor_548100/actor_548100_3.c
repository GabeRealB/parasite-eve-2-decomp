#include "common.h"

#include "actors/actor_548100.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s8  D_8007216C;
extern s16 D_80114D08;

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

void func_actor_548100_80134F64(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->idMap;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCurBit2Flag(work->bit2Slot) == 2) {
            /* The nibble at 0xBE + step is this actor's per-step progress flag. */
            GameFlag_SetNibble(work->step + 0xBE, 0);
            GameFlag_SetNibble(0x110, 1);
            SndEvt_EnqueueType6(0x5406000B, 0, 0);
        }
        arg0->state = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", func_actor_548100_80134FEC);

void func_actor_548100_80135124(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        arg0->state = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", ActorsShared8013845cSub0);
