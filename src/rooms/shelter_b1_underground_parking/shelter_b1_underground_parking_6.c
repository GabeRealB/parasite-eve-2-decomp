#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern u8  D_8007216D;
extern s32 D_shelter_b1_underground_parking_801872D8;
extern s32 D_shelter_b1_underground_parking_801873DC;
extern s32 D_shelter_b1_underground_parking_80187544;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_6", func_shelter_b1_underground_parking_801834D4);

void func_shelter_b1_underground_parking_80183560(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                arg0->state += 1;
            }
            return;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                gGameSession->at4.loc.room  = 6;
                D_8007216D                  = 6;
                gGameSession->roomObjsDirty = state;
                func_800E8614((s32)&D_shelter_b1_underground_parking_801872D8, 1);
                GameFlag_SetNibble(0xF4, 1);
                Gp_SetItemSeenBit(0x123, 1);
            } else {
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_underground_parking_8018363C(Task* arg0)
{
    if (arg0->state == 0) {
        SetDispMask(0);
        D_80115768            = 1;
        gGameSession->hideHud = 1;
        func_800E8634((s32)&D_shelter_b1_underground_parking_801873DC, 0, (s32)&D_shelter_b1_underground_parking_80187544);
        GameFlag_SetNibble(0xF4, 2);
        GameFlag_SetNibble(0x1B4, 0);
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_shelter_b1_underground_parking_801836D8(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        Gp_MsgPlayerWeapon(1);
        taskKill(arg0);
    }
}

void func_shelter_b1_underground_parking_80183714(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x300, 0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

void func_shelter_b1_underground_parking_801837D8(u8 arg0)
{
    gGameSession->at4.loc.room  = arg0;
    D_8007216D                  = arg0;
    gGameSession->roomObjsDirty = 1;
    gGameSession->viewDirty     = 1;
}
