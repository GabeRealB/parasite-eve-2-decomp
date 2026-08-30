#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/unknown_syms.h"

extern s16 D_80071076;
extern s32 D_actor_450900_80136BD8;

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80131E38);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_8013207C);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_8013223C);

void func_actor_450900_8013235C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StartCapSlot(0xB, 1, 1);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                Task_Kill(task);
                Gp_MsgPlayerWeapon(1);
            } else {
                func_800E8614((s32)&D_actor_450900_80136BD8, 0);
                task->state = task->state + 1;
            }
            break;
        case 3:
            if (Game_Session->field_1 == 2) {
                task->state = task->state + 1;
            }
            break;
        case 4:
            GameFlag_SetNibble(0x4D, 0);
            GameFlag_SetNibble(0xFC, 1);
            GameFlag_SetNibble(0xA5, 0);
            GameFlag_SetNibble(0xD9, 0);
            GameFlag_SetNibble(0xAB, 1);
            GameFlag_SetNibble(0x1C7, 0);
            GameFlag_SetNibble(0xD2, 0);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 8);
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_6  = 0xF;
            Mc_SaveData.field_8  = 3;
            Mc_SaveData.field_13 = 0;
            Mc_SaveData.field_5  = 1;
            D_80071076           = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Gp_RestoreStreamRng();
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132518);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132548);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132678);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132684);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132724);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_801327A8);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132834);
