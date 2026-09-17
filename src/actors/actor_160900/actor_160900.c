#include "common.h"

#include "actors/actor_160900.h"

#include "main/gameflow.h"

#include "main/gfx.h"

#include "main/mem.h"

#include "main/tmd.h"

#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"

extern TaskDesc ActorsShared80136280Desc;

void      func_800B4114(Actor160900Child3Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
extern u8 D_actor_160900_8013F240[];

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

void func_actor_160900_80132844(Task* arg0);

/// Animation source `func_800B3F84` seeds the child's slots from, the table
/// published as `Actor160900Child3Work::field_4B4`, and the message table
/// published as `Task::field_24`.
extern u8 D_actor_160900_8013F1C4[];
extern u8 D_actor_160900_8013F1F8[];
extern u8 D_actor_160900_8013F200[];

/// Binds the child's animation context and resets slots 1-19. Taking the model
/// as a parameter is what schedules its load after the work-block load.
static inline void func_actor_160900_InitAnim(Task* task, GpAnimObj* obj)
{
    Actor160900Child3Work* work;
    s32                    i;

    work = (Actor160900Child3Work*)task->idMap;
    func_800B3F84(&work->anim, D_actor_160900_8013F1C4, obj, work->aux, work->slots);
    work->field_4B4 = D_actor_160900_8013F1F8;
    work            = (Actor160900Child3Work*)task->idMap;
    i               = 1;
    work->field_4B8 = 0;
    work->field_4BA = 0;
    do {
        work->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&work->anim, (u16)i, 0);
        i++;
    } while ((u16)i < 0x14U);
}

void func_actor_160900_80132C08(Task* task)
{
    TmdObject*             obj;
    TmdObject*             obj2;
    GsCOORDINATE2*         coord;
    Actor160900Child3Work* work;
    GpAreaPlace*           place;
    VECTOR                 pos;
    s32                    failed;

    if (task->state == 0) {
        obj         = (TmdObject*)task->extra;
        coord       = obj->field_8;
        work        = (Actor160900Child3Work*)Mem_Malloc(0x4BC, false);
        task->idMap = (TaskIdMap*)work;
        if (work == NULL) {
            failed = 1;
        } else {
            coord->sub = &Gfx_ViewCoord;
            Mem_Set(task->idMap, 0, 0x4BC);
            obj->field_1C  = &work->light;
            obj->field_20  = &work->color;
            obj->field_C  |= 0x84;
            task->field_24 = D_actor_160900_8013F200;
            place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
            while (place->field_0 != 0xFF && place->field_0 != 0x65) {
                place++;
            }
            Gp_SetTmdBytes((TmdObject*)task->extra, (s8)place->field_D, (s8)place->field_E);
            Task_Reparent(D_actor_160900_8013FBB4, task);
            failed = 0;
        }
        if ((u16)failed) {
            Task_Kill(task);
            return;
        }
        func_actor_160900_InitAnim(task, (GpAnimObj*)task->extra);
        task->state++;
    }
    func_actor_160900_80132844(task);
    if (Game_Session->field_4 == 0x2E) {
        Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8[18].coord, 0x800, 1);
    } else {
        Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8[18].coord, 0x79C, 1);
    }
    obj2   = (TmdObject*)task->extra;
    pos.vx = obj2->field_8->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
    func_800D7A9C(obj2, &pos, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80132E80);

INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_80133238);

void func_actor_160900_8013358C(Task* arg0)
{
    Actor160900Work*       work;
    Actor160900Child3Work* child;
    SVECTOR                ofs;
    SVECTOR                ofs2;
    s32                    i;

    work = (Actor160900Work*)arg0->idMap;
    switch ((u16)work->field_54) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_38, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_38, 0x7D4, (s32)D_actor_160900_8013F240, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_3C, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_40, 0x7D5, 1, 0);
            if (work->field_44 != NULL) {
                Gp_DispatchMsg(work->field_44, 0x7D5, 1, 0);
            }
            Gp_DispatchMsg(work->field_38, 0x7D5, 1, 0);
            ofs.vx = -100;
            ofs.vy = 100;
            ofs.vz = -1200;
            Gp_SpawnEff(0x60046, ((TmdObject*)work->field_38->extra)->field_8, 0x20000100, &ofs);
            break;
        case 3:
            child            = (Actor160900Child3Work*)work->field_38->idMap;
            child->field_4B8 = 1;
            child->field_4BA = 0;
            do {
            } while (0);
            for (i = 1; (u16)i < 20; i++) {
                func_800B4114(child, (u16)i, 1, 0, 10);
            }
            break;
        case 4:
            ofs2.vx = -200;
            ofs2.vy = 100;
            ofs2.vz = -400;
            Gp_SpawnEff(0x60046, ((TmdObject*)work->field_38->extra)->field_8, 0x20000100, &ofs2);
            break;
    }
    work->field_54 = 0;
}

void func_actor_160900_80133758(SVECTOR* pts)
{
    SVECTOR pos;
    s32     x;
    u32     seed;
    s32     flags;

    if (!(D_80070F70 & 7) && pts->pad != -1) {
        flags = 0x81203400;
        do {
            seed        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = seed;
            x           = pts->vx + (((seed >> 16) & 1) ? ((Gp_LcgState = seed * 5 + 0x71357911) >> 16) & 7
                                                        : -(((Gp_LcgState = seed * 5 + 0x71357911) >> 16) & 7)) *
                              100;
            pos.vx = x;
            pos.vy = pts->vy;
            pos.vz = pts->vz;
            Gp_SpawnEff(0x601B4, NULL, flags, &pos);
            pts++;
        } while (pts->pad != -1);
    }
}

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
void func_actor_160900_80133A84(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->idMap;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 5, 0);
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = 0x1F4;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = -0x1F4;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x1F4;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x1F4;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 2, 0);
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = 0x3E8;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = 0x1F4;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x3E8;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = 0x1F4;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 3, 0);
    data->field_C[2]                               = task;
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = -0x1F4;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = -0x3E8;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = -0x1F4;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x3E8;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 4, 0);
    data->field_C[3]                               = task;
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = 0x1F4;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = -0x1F4;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x1F4;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x1F4;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[4]                               = task;
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = 0x3E8;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = 0x1F4;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = 0x3E8;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = 0x1F4;
    task                                           = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[5]                               = task;
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
    ((TmdObject*)task->extra)->field_8->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
    work->field_0.vx                               = 0;
    work->field_0.vy                               = -0x3E8;
    work->field_0.vz                               = -0x1F4;
    work->field_8.vx                               = 0;
    work->field_8.vy                               = -0x3E8;
    work->field_8.vz                               = -0x3E8;
    work->field_10.vx                              = 0;
    work->field_10.vy                              = 0;
    work->field_10.vz                              = -0x1F4;
    work->field_18.vx                              = 0;
    work->field_18.vy                              = 0;
    work->field_18.vz                              = -0x3E8;
}
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
void func_actor_160900_8013418C(Task* arg0)
{
    Actor160900Work* work;
    Actor160900Work* data;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            work        = (Actor160900Work*)Mem_Malloc(0x68, 0);
            arg0->idMap = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(work, 0, 0x68);
                work->field_34          = Game_GetPtrSlot(3);
                D_actor_160900_8013FBB4 = arg0;
                work->field_38          = Task_SpawnFromTable(&ActorsShared80136280Desc, 3, 0, (s32)arg0);
                work->field_3C          = Task_SpawnFromTable(&ActorsShared80136280Desc, 5, 1, (s32)work->field_38);
                work->field_40          = Task_SpawnFromTable(&ActorsShared80136280Desc, 6, 0, (s32)work->field_38);
            }
            Gp_CapFile = 0;
            Gp_LoadCapFile(3);
            func_800E6D4C(0x180, 0);
            arg0->state += 1;
            return;
        case 1:
            func_800E8634((s32)D_actor_160900_8013F538, 0, (s32)D_actor_160900_8013FAA8);
            arg0->state += 1;
            break;
        case 2:
            if (Game_Session->field_1 == 0) {
                D_8007272D = 0x1E;
                Task_RequestKill(arg0, 0);
            }
            break;
    }
    func_actor_160900_80133238(arg0);
    func_actor_160900_8013358C(arg0);
    data = (Actor160900Work*)arg0->idMap;
    switch ((u16)data->field_5C) {
        case 1:
            func_actor_160900_80133758(D_actor_160900_8013F258);
            break;
        case 2:
            func_actor_160900_80133758(D_actor_160900_8013F2E0);
            break;
        case 3:
            func_actor_160900_80133758(D_actor_160900_8013F3B0);
            break;
        case 4:
            func_actor_160900_80133758(D_actor_160900_8013F400);
            break;
        case 5:
            func_actor_160900_80133758(D_actor_160900_8013F458);
            break;
        case 0:
        default:
            data->field_5C = 0;
            break;
    }
}

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
