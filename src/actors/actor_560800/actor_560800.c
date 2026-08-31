#include "common.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
extern TaskDesc D_actor_560800_801718F0;

void func_actor_560800_801321A0(Task* arg0)
{
    u8          slotParam[4];
    GBytes8     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = ((SessionBytesAt4*)Game_Session)->field_4;
    if (task->spawnArg1 != 0) {
        key.data[0] = 0x65;
    } else {
        key.data[0] = 0x64;
    }
    slot = Stream_FindSlot(key.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132340);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132498);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801326C4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132A14);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132C60);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132F64);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133204);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133540);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133648);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133750);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133970);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134258);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134384);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134B14);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134BFC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135AEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135BD8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135D54);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135F50);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135FA0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136094);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801361A0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801361F4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136280);

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_560800_801718F0, 3, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801362E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_8013631C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136358);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136378);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801363F8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801364A0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136548);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801365B0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801365D0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136678);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801366B0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801367C0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136818);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136878);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136910);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136930);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801369A0);

void func_actor_560800_801369E0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Task_Kill(arg0);
        Display_ResetHeapWrapper();
    }
}

void func_actor_560800_80136A20(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(1);
    func_800E6D4C(0x180, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136A54);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136A88);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136AA8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801376E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80137820);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80137BEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801384EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801386D4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80138A4C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80138BCC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80138D04);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80138FC8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80139360);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801393EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80139440);
