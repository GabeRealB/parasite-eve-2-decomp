#include "common.h"

#include "actors/actor_560800.h"

#include "gameplay/3A34.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

#include "main/display.h"

#include "main/fs.h"

#include "main/gameflow.h"

#include "main/mem.h"

#include "main/pad.h"

#include "main/session.h"

#include "main/stream.h"

#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc ActorsShared80136280Desc;
extern TaskDesc D_actor_560800_8016EA28;
extern TaskDesc D_actor_560800_8017575C;

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

void func_actor_560800_80135BD8(Task* arg0)
{
    Actor560800Work* work;
    Task*            sub5;
    Task*            sub6;
    SVECTOR          vec;

    work        = (Actor560800Work*)Mem_Malloc(0x68, 0);
    arg0->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x68);
    work->field_0           = Game_GetPtrSlot(3);
    D_actor_560800_8017578C = arg0;
    work->field_4           = Task_SpawnFromTable(&ActorsShared80136280Desc, 4, 0, 0);
    sub5                    = Task_SpawnFromTable(&ActorsShared80136280Desc, 5, 0, 0);
    work->field_8           = sub5;
    work->field_10          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, (s32)sub5);
    work->field_14          = Task_SpawnFromTable(&ActorsShared80136280Desc, 8, 0, (s32)work->field_8);
    work->field_18          = Task_SpawnFromTable(&ActorsShared80136280Desc, 9, 2, (s32)work->field_8);
    sub6                    = Task_SpawnFromTable(&ActorsShared80136280Desc, 6, 0, 0);
    work->field_C           = sub6;
    work->field_1C          = Task_SpawnFromTable(&ActorsShared80136280Desc, 0xA, 3, (s32)sub6);
    work->field_20          = Task_SpawnFromTable(&D_actor_560800_8017575C, 0, 0, (s32)arg0);
    work->field_24          = Task_SpawnFromTable(&D_actor_560800_8017575C, 2, 0, (s32)arg0);
    vec.vx                  = 0x5A0;
    vec.vy                  = 0x5A0;
    vec.vz                  = 0x5A0;
    Gp_SetOverrideVec(&vec);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80135D54);

void func_actor_560800_80135F50(Task* arg0)
{
    Display_SpawnWithOt(&D_actor_560800_8016EA28, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

void func_actor_560800_80135FA0(Task* arg0)
{
    Actor560800FadeWork* work;
    Actor560800FadeWork* alloc;

    work = (Actor560800FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor560800FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work    = alloc;
            work->b = 0;
            work->g = 0;
            work->r = 0;
            Task_Reparent(D_actor_560800_8017578C, arg0);
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if ((s16)work->r >= 0x100) {
                SetDispMask(0);
                Task_Kill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80136094);

void func_actor_560800_801361A0(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801361F4);
