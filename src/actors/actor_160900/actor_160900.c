#include "common.h"

#include "actors/actor_160900.h"

#include "main/gameflow.h"

#include "main/gfx.h"

#include "main/mem.h"

#include "main/tmd.h"

#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

extern TaskDesc ActorsShared80136280Desc;

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80131EB0);

s32 func_actor_160900_801326EC(Task* arg0)
{
    Actor160900Work*     work;
    Actor160900AnimStep* table;
    Actor160900AnimStep* entry;
    Actor160900AnimStep* entry2;
    GpAnimArg            msg;
    u16                  anim;
    u16                  anim2;

    work = (Actor160900Work*)arg0->idMap;
    if (work->field_34 == NULL) {
        return 1;
    }
    table = D_actor_160900_8013F1CC;
    entry = &table[work->field_64];
    if (entry->field_0 != 0) {
        if ((s16)work->field_66 >= entry->field_0) {
            if (entry->field_2 < 0) {
                return 1;
            }
            anim           = entry->field_2;
            msg.field_0    = D_actor_160900_8013F198;
            work->field_64 = anim;
            msg.field_4    = anim;
            msg.field_8    = 1;
            msg.field_C    = 0xA;
            msg.field_10   = 1;
            Gp_DispatchMsg(work->field_34, 0x3F4, (s32)&msg, 0);
            work->field_66 = 0;
        } else {
            work->field_66 += 1;
        }
    } else {
        if (Gp_DispatchMsg(work->field_34, 0x3ED, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_160900_8013F1CC[work->field_64];
        if (entry2->field_2 < 0) {
            return 1;
        }
        work = (Actor160900Work*)arg0->idMap;
        if (work->field_34 != NULL) {
            anim2          = entry2->field_2;
            msg.field_0    = D_actor_160900_8013F198;
            work->field_64 = anim2;
            msg.field_4    = anim2;
            msg.field_8    = 1;
            msg.field_C    = 0xA;
            msg.field_10   = 1;
            Gp_DispatchMsg(work->field_34, 0x3F4, (s32)&msg, 0);
            work->field_66 = 0;
        }
    }
    return 0;
}
INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132844);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132A14);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132C08);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132E80);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_8013358C);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133758);

void func_actor_160900_80133880(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc       = (Actor160900ChildWork*)Mem_Calloc(0x20, 0);
    task->idMap = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        Task_Kill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->field_8->sub        = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0x3E8;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xBB8;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x5DC;
    work->field_0.vz                               = 0x3E8;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x5DC;
    work->field_8.vz                               = 0;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x3E8;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = 0;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[1]                               = task;
    if (task == NULL) {
        return;
    }
    alloc       = (Actor160900ChildWork*)Mem_Calloc(0x20, 0);
    task->idMap = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        Task_Kill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->field_8->sub        = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0x3E8;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xBB8;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x5DC;
    work->field_0.vz                               = 0;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x5DC;
    work->field_8.vz                               = -0x3E8;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x3E8;
}
INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133A84);
void func_actor_160900_80133F90(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc       = (Actor160900ChildWork*)Mem_Calloc(0x20, 0);
    task->idMap = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        Task_Kill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->field_8->sub        = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xBB8;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x5DC;
    work->field_0.vz                               = 0x3E8;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x5DC;
    work->field_8.vz                               = 0;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x3E8;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = 0;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[1]                               = task;
    if (task == NULL) {
        return;
    }
    alloc       = (Actor160900ChildWork*)Mem_Calloc(0x20, 0);
    task->idMap = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        Task_Kill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->field_8->sub        = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xBB8;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x5DC;
    work->field_0.vz                               = 0;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x5DC;
    work->field_8.vz                               = -0x3E8;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x3E8;
}
INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_8013418C);

void func_actor_160900_801343E4(Task* arg0)
{
    Actor160900FadeWork* work;
    Actor160900FadeWork* alloc;

    work = (Actor160900FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor160900FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if ((s16)work->r >= 0x100) {
                work->b = 0xFF;
                work->g = 0xFF;
                work->r = 0xFF;
            }
            break;
    }
}
void func_actor_160900_801344D8(Task* arg0)
{
    Actor160900FadeWork* work;
    Actor160900FadeWork* alloc;

    work = (Actor160900FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor160900FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work    = alloc;
            work->b = 0xFF;
            work->g = 0xFF;
            work->r = 0xFF;
            goto state_inc;
        case 3:
            SetDispMask(1);
        case 1:
        case 2:
        state_inc:
            arg0->state += 1;
            /* fallthrough */
        case 4:
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
INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801345D0);
