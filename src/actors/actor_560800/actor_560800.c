#include "common.h"

#include "actors/actor_560800.h"

#include "gameplay/1BC.h"

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
extern void     D_actor_560800_8016EB30;
extern void     D_actor_560800_8016ECC4;
extern void     D_actor_560800_8016F34C;

void func_actor_560800_80132498(Task* arg0);

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

s32 func_actor_560800_80132340(Task* arg0)
{
    Actor560800Work*     work;
    Actor560800AnimStep* table;
    Actor560800AnimStep* entry;
    Actor560800AnimStep* entry2;
    GpAnimArg            msg;
    u16                  anim;
    u16                  anim2;

    work = (Actor560800Work*)arg0->idMap;
    if (work->field_0 == NULL) {
        return 1;
    }
    table = D_actor_560800_8016EBE8;
    entry = &table[(u16)work->field_60];
    if (entry->field_0 != 0) {
        if (work->field_62 >= entry->field_0) {
            if (entry->field_2 < 0) {
                return 1;
            }
            anim           = entry->field_2;
            msg.field_0    = D_actor_560800_8016EA40;
            work->field_60 = anim;
            msg.field_4    = anim;
            msg.field_8    = 1;
            msg.field_C    = 0xA;
            msg.field_10   = 1;
            Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
            work->field_62 = 0;
        } else {
            work->field_62 += 1;
        }
    } else {
        if (Gp_DispatchMsg(work->field_0, 0x3ED, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_560800_8016EBE8[(u16)work->field_60];
        if (entry2->field_2 < 0) {
            return 1;
        }
        work = (Actor560800Work*)arg0->idMap;
        if (work->field_0 != NULL) {
            anim2          = entry2->field_2;
            msg.field_0    = D_actor_560800_8016EA40;
            work->field_60 = anim2;
            msg.field_4    = anim2;
            msg.field_8    = 1;
            msg.field_C    = 0xA;
            msg.field_10   = 1;
            Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
            work->field_62 = 0;
        }
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132498);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_801326C4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132A14);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80132C60);

void func_actor_560800_80132F64(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->idMap;
    SVECTOR              ofs;
    VECTOR               pos;

    if (arg0->state == 0) {
        u16 failed;
        {
            TmdObject*           tmd   = arg0->extra;
            GsCOORDINATE2*       coord = tmd->field_8;
            Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
            GpAreaPlace*         place;
            u8                   id;

            arg0->idMap = (TaskIdMap*)block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->sub = &Gfx_ViewCoord;
                Mem_Set(arg0->idMap, 0, 0x4CC);
                tmd->field_1C  = &block->light;
                tmd->field_20  = &block->color;
                arg0->field_24 = &D_actor_560800_8016F34C;
                place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
                id             = place->field_0;
                while (id != 0xFF) {
                    if (id == 0x22) {
                        break;
                    }
                    place++;
                    id = place->field_0;
                }
                Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->field_D, (s8)place->field_E);
                Task_Reparent(D_actor_560800_8017578C, arg0);
                failed = 0;
            }
        }
        if (failed) {
            Task_Kill(arg0);
            return;
        }
        work = (Actor560800AnimWork*)arg0->idMap;
        {
            GpAnimObj* obj = arg0->extra;
            func_800B3F84(&work->anim, &D_actor_560800_8016EB30, obj, work->animAux, work->slots);
        }
        work->field_4BA = 0x13;
        work->field_4B4 = &D_actor_560800_8016ECC4;
        {
            Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->idMap;
            u16                  i;
            s32                  fade = 0x10;

            w->field_4B8 = 0;
            w->field_4C8 = fade;
            w->field_4BE = 0;
            for (i = 1; i < w->field_4BA; i++) {
                w->slots[i].field_9 = fade;
                Gp_AnimResetSlot(&w->anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    if (!(((TmdObject*)arg0->extra)->field_C & 0x80) && work->field_4C2 == 0) {
        ofs.vx = 0;
        ofs.vy = 0x380;
        ofs.vz = 0;
        Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->field_8[1], 0x300, &ofs);
    }
    func_actor_560800_80132498(arg0);
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->field_8->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133204);

void func_actor_560800_80133540(u32 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;

    switch (arg0) {
        case 0:
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            break;
        case 1:
            Gp_DispatchMsg(work->field_4, 0x7D5, 1, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_8, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 1, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 1, 0);
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_C, 0x7D5, 1, 0);
            if (work->field_1C != NULL) {
                Gp_DispatchMsg(work->field_1C, 0x7D5, 1, 0);
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_20, 0x7D5, 1, 0);
            break;
        case 5:
            Gp_DispatchMsg(work->field_24, 0x7D5, 1, 0);
            break;
    }
}

void func_actor_560800_80133648(u32 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;

    switch (arg0) {
        case 0:
            Gp_DispatchMsg(work->field_0, 0x3F3, 2, 0);
            break;
        case 1:
            Gp_DispatchMsg(work->field_4, 0x7D5, 2, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_8, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 2, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 2, 0);
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_C, 0x7D5, 2, 0);
            if (work->field_1C != NULL) {
                Gp_DispatchMsg(work->field_1C, 0x7D5, 2, 0);
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_20, 0x7D5, 2, 0);
            break;
        case 5:
            Gp_DispatchMsg(work->field_24, 0x7D5, 2, 0);
            break;
    }
}

extern s32* D_actor_560800_8016F35C[];
extern s32* D_actor_560800_8016F3E4[];
extern s32* D_actor_560800_8016F46C[];
extern s32* D_actor_560800_8016F4F4[];

void func_actor_560800_80133750(s32 arg0)
{
    Actor560800Work* work;
    s32*             msg;

    work = (Actor560800Work*)D_actor_560800_8017578C->idMap;
    if (work->field_66 == 0) {
        Gp_PulseState1C();
    }
    if (work->field_0 != NULL) {
        msg = D_actor_560800_8016F35C[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(0);
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(0);
        }
    }
    if (work->field_8 != NULL) {
        msg = D_actor_560800_8016F46C[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(2);
            Gp_DispatchMsg(work->field_8, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(2);
        }
    }
    if (work->field_C != NULL) {
        msg = D_actor_560800_8016F3E4[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(3);
            Gp_DispatchMsg(work->field_C, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(3);
        }
    }
    if (work->field_4 != NULL) {
        msg = D_actor_560800_8016F4F4[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(1);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(1);
        }
    }
    if (work->field_20 != NULL) {
        // No table of its own: reuses the payload picked for field_4.
        if (*msg != 0) {
            func_actor_560800_80133540(4);
            Gp_DispatchMsg(work->field_20, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(4);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80133970);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134258);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134384);

/// Reseeds the sub-task's animation slots from clip 0x20 -- writing the slot
/// count with the 0x10 restart rate and every slot's `field_9` -- then spawns
/// effect 0x6002B on the ninth per-part coordinate of the task at `field_8`
/// and posts the pad event that releases the input lock.
///
/// The rate is held in a local rather than written as two literals: both uses
/// have to reach the same register, and 0x10 is live across the loop's
/// `Gp_AnimResetSlot` call. `unused` is declared and never referenced - the
/// ROM's frame is 0x30 and the local is what reserves its 8 bytes.
void func_actor_560800_80134B14(void)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    SVECTOR              unused;
    u16                  i;
    u16                  rate;

    work = (Actor560800Work*)D_actor_560800_8017578C->idMap;
    anim = (Actor560800AnimWork*)work->field_8->idMap;

    anim->field_4B8 = 0x20;
    rate            = 0x10;
    anim->field_4C8 = rate;
    anim->field_4BE = 0;
    i               = 1;
    if (i < anim->field_4BA) {
        do {
            anim->slots[i].field_9 = rate;
            Gp_AnimResetSlot(&anim->anim, i, 0x20);
            i++;
        } while (i < anim->field_4BA);
    }
    Gp_SpawnEff(0x6002B, &((TmdObject*)work->field_8->extra)->field_8[8], 0x21, NULL);
    Pad_PostEvent(0, 1, 0xFF, 2);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800", func_actor_560800_80134BFC);

void func_actor_560800_80135AEC(s32 arg0)
{
    if (arg0 == 1) {
        if ((u32)D_actor_560800_8017579C > (u32)Display_State.field_0) {
            D_actor_560800_80175790 = Display_State.field_0 - (D_actor_560800_8017579C + 1);
        } else {
            D_actor_560800_80175790 = Display_State.field_0 - D_actor_560800_8017579C;
        }
    } else if (arg0 == 2) {
        if ((u32)D_actor_560800_801757A0 > (u32)Display_State.field_0) {
            D_actor_560800_80175794 = Display_State.field_0 - (D_actor_560800_801757A0 + 1);
        } else {
            D_actor_560800_80175794 = Display_State.field_0 - D_actor_560800_801757A0;
        }
    } else if (arg0 == 3) {
        if ((u32)D_actor_560800_801757A4 > (u32)Display_State.field_0) {
            D_actor_560800_80175798 = Display_State.field_0 - (D_actor_560800_801757A4 + 1);
        } else {
            D_actor_560800_80175798 = Display_State.field_0 - D_actor_560800_801757A4;
        }
    }
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}

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

void func_actor_560800_80135D54(Task* arg0)
{
    s32              msg[5];
    Actor560800Work* work;
    s32              val;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            func_actor_560800_80135BD8(arg0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(0);
            func_800E6D4C(0x180, 0);
            arg0->state++;
        case 1:
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            func_800E8634((s32)&D_actor_560800_8016F5E0, 1, (s32)&D_actor_560800_80171800);
            arg0->state++;
            break;
        case 2:
            if (Game_Session->field_1 == 0) {
                Gp_LcgState = D_actor_560800_801757A8;
                Gp_PulseState1C();
                val    = D_80073BA9;
                msg[0] = (D_8007218A == 1) ? val + 1 : val + 0x22;
                msg[1] = 1;
                msg[2] = 0;
                msg[3] = 0;
                msg[4] = 0;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)msg, 0);
                Task_RequestKill(arg0, 0);
                return;
            }
            break;
    }
    func_actor_560800_80133970(arg0);
    func_actor_560800_80134258(arg0);
    func_actor_560800_80134384(arg0);
    func_actor_560800_80134BFC(arg0);
    work = (Actor560800Work*)arg0->idMap;
    switch ((u16)work->field_58) {
        case 0:
            break;
        case 1:
            if (work->field_4 != NULL) {
                Task_Kill(work->field_4);
            }
            Display_SpawnWithOt(&ActorsShared80136280Desc, 0xC, 0, 0);
            break;
    }
    work->field_58 = 0;
}

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

void func_actor_560800_80136094(Task* arg0)
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
            work->b = 0xFF;
            work->g = 0xFF;
            work->r = 0xFF;
            Task_Reparent(D_actor_560800_8017578C, arg0);
            goto state_inc;
        case 6:
            SetDispMask(1);
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        state_inc:
            arg0->state += 1;
            /* fallthrough */
        case 7:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                Task_Kill(arg0);
            }
            break;
    }
}

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
