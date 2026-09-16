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

void func_actor_548100_80134E94(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->idMap;
    s32              value;

    if (GameFlag_GetNibble(work->step + 0xBE) == 0) {
        if (GameFlag_GetNibble(0xC3) != 0) {
            Gp_StartCapSlot(6, 0, 1);
        } else {
            value = 2;
            if (work->collectBitId == 0x120) {
                value = 1;
            }
            SndEvt_EnqueueType6(0x5406000B, 0, 0);
            GameFlag_SetNibble(work->step + 0xBE, value);
            Gp_ClearCollectedBit(work->collectBitId);
        }
    } else {
        Gp_StartCapSlot(6, 0, 4);
    }
    work->collectBitId = 0;
    arg0->state        = 2;
}

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

void func_actor_548100_80134FEC(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->idMap;

    work->field_12 += 4;
    work->field_A  += 4;
    if (work->field_10 < work->field_12) {
        work->field_12 = work->field_10;
    }
    if (work->field_A > work->field_8) {
        work->field_A = work->field_8;
        if (work->field_12 == work->field_10) {
            SndEvt_EnqueueType7(0x5406000A, 1);
            if (D_actor_548100_80135B4C->flag_8 != 0) {
                SndEvt_EnqueueType6(0x5406000E, 0, 0);
                Gp_RunCapCmd(0xC, 0);
                arg0->state = 0xA;
            } else {
                arg0->state = 2;
            }
            return;
        }
    }
    work->field_E = work->field_C * work->field_A / work->field_8;
}

void func_actor_548100_80135124(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        arg0->state = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_3", ActorsShared8013845cSub0);
