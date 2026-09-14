#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"

extern s16 D_80071076;
extern u8  D_801153F4;
extern s32 D_actor_450900_80136B00;
extern s32 D_actor_450900_80136BD8;

/// The save-point capture task spawned by `func_actor_450900_80131E38`, kept
/// alive until `func_actor_450900_80132548` kills it. Script opcode 0xD reaches
/// both this and `func_actor_450900_80132678`, so its one argument is the
/// opcode's immediate.
extern Task* D_actor_450900_80136C9C;

/// This overlay's own spawn table, six `TaskDesc` entries. Index 0 is the exit
/// handler `Task_Kill`; 1..5 are the overlay's state handlers, and the "next
/// stage" of each is the next entry: `func_actor_450900_80131E38` spawns 5 on
/// its way through, and `func_actor_450900_80132834` spawns 4
/// (`func_actor_450900_8013235C`, the save-data teardown) when the ally has
/// walked past the trigger line.
extern TaskDesc D_actor_450900_80135E78;

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80131E38);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_8013207C);

void func_actor_450900_8013223C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_RunCapCmd(1, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                goto kill;
            }
            GameFlag_SetNibble(0xD8, 1);
            D_801153F4 = 1;
            Gp_RunCapCmd(2, 0);
            func_800E8614((s32)&D_actor_450900_80136B00, 0);
            task->state = task->state + 1;
            break;
        case 3:
            if (Game_Session->field_1 == 0) {
            kill:
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                Task_Kill(task);
            }
            break;
    }
}

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

/// Script callback: arms or disarms the save-point capture task's flag
/// (`Task::spawnArg1`, the value `func_actor_450900_80132548` tests to decide
/// which way the capture cursor sweeps).
void func_actor_450900_80132518(s32 arg0)
{
    if (D_actor_450900_80136C9C != NULL) {
        if (arg0 == 1) {
            D_actor_450900_80136C9C->spawnArg1 = 0;
            return;
        }
        D_actor_450900_80136C9C->spawnArg1 = 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132548);

void func_actor_450900_80132678(u8 arg0)
{
    D_801153F4 = arg0;
}

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132684);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132724);

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_801327A8);

/// Save-point gate: reads the ally actor's root coordinate and, once it has
/// walked past `Z < -0x76C`, hands over to the save-data teardown task instead
/// of starting the save-point capture. The chain is the one
/// `func_actor_450900_80132684` and `func_actor_161500_80132210` use: the ally
/// task's `Task::extra` is its `TmdObject`, whose `field_8` is the root
/// `GsCOORDINATE2`, so `coord.t[2]` is that coordinate's world Z.
void func_actor_450900_80132834(void)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)((Task*)Game_GetPtrSlot(0xA))->extra)->field_8;
    if (coord->coord.t[2] < -0x76C) {
        Task_SpawnFromTable(&D_actor_450900_80135E78, 4, 0, 0);
    } else {
        Gp_StartCapSlot(0xB, 1, 0);
    }
}
