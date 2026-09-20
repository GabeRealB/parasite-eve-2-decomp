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
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

/// `rtps` from `inline_c.h` assembles to a different word; spell it out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern TaskDesc ActorsShared80136280Desc;

extern u8 D_actor_160900_8013F240[];

s32 func_actor_160900_801326EC(Task* arg0)
{
    Actor160900Work*     work;
    Actor160900AnimStep* table;
    Actor160900AnimStep* entry;
    Actor160900AnimStep* entry2;
    GpAnimArg            msg;
    u16                  anim;
    u16                  anim2;

    work = (Actor160900Work*)arg0->work;
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
        work = (Actor160900Work*)arg0->work;
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
static inline void func_actor_160900_Reseed(Task* arg0, u16 anim)
{
    Actor160900Child3Work* work;
    u16                    i;
    u16                    id;

    i               = 1;
    work            = (Actor160900Child3Work*)arg0->work;
    work->field_4B8 = anim;
    work->field_4BA = 0;
    id              = anim;
    TOUCH_REG_USE2(id, work, work);
    for (; i < 0x14; i++) {
        func_800B4114(work, i, id, 0, 0xA);
    }
}

s32 func_actor_160900_80132844(Task* arg0)
{
    Actor160900Child3Work* work;
    Actor160900AnimStep*   table;
    u16                    i;
    u16                    done;

    work = (Actor160900Child3Work*)arg0->work;
    if (((TmdObject*)arg0->extra)->flags & 0x80) {
        return 0;
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            done = 0;
            break;
        }
    }
    table = (Actor160900AnimStep*)work->field_4B4;
    if (table[(u16)work->field_4B8].field_0 != 0) {
        if (work->field_4BA >= table[(u16)work->field_4B8].field_0) {
            if (table[(u16)work->field_4B8].field_2 >= 0) {
                func_actor_160900_Reseed(arg0, table[(u16)work->field_4B8].field_2);
            } else {
                return 1;
            }
        } else {
            work->field_4BA++;
        }
    } else if (done) {
        if (table[(u16)work->field_4B8].field_2 >= 0) {
            func_actor_160900_Reseed(arg0, table[(u16)work->field_4B8].field_2);
        } else {
            return 1;
        }
    }
    return 0;
}

/// Animation source `func_800B3F84` seeds the child's slots from, the table
/// published as `Actor160900Child3Work::field_4B4`, and the message table
/// published as `Task::msgTable`.
extern u8 D_actor_160900_8013F1C4[];
extern u8 D_actor_160900_8013F1F8[];
extern u8 D_actor_160900_8013F200[];

void func_actor_160900_80132A14(Task* arg0)
{
    VECTOR pos;

    if (arg0->state == 0) {
        TmdObject*             tmd    = arg0->extra;
        Task*                  parent = arg0->spawnArg2;
        GsCOORDINATE2*         coord  = tmd->coords;
        Actor160900Child3Work* work;
        Actor160900Child3Work* block;
        GpAreaPlace*           place;
        u8                     id;

        block      = Mem_Malloc(0x4BC, 0);
        arg0->work = (TaskIdMap*)block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1) {
            case 0:
                coord->sub = &((TmdObject*)parent->extra)->coords[12];
                break;
            case 1:
            case 2:
                coord->sub = &((TmdObject*)parent->extra)->coords[8];
                break;
        }
        Mem_Set(arg0->work, 0, 0x4BC);
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1 < 2) {
            place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            id    = place->entryId;
            while (id != 0xFF) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
        } else if (arg0->spawnArg1 == 2) {
            Gp_SetTmdBytes((TmdObject*)arg0->extra, 0, 0);
        }
        Task_Reparent(parent, arg0);
        arg0->msgTable = D_actor_160900_8013F200;
        arg0->state   += 1;
        return;
    } else {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

/// Binds the child's animation context and resets slots 1-19. Taking the model
/// as a parameter is what schedules its load after the work-block load.
static inline void func_actor_160900_InitAnim(Task* task, TmdObject* obj)
{
    Actor160900Child3Work* work;
    s32                    i;

    work = (Actor160900Child3Work*)task->work;
    func_800B3F84(&work->anim, D_actor_160900_8013F1C4, obj, work->aux, work->slots);
    work->field_4B4 = D_actor_160900_8013F1F8;
    work            = (Actor160900Child3Work*)task->work;
    i               = 1;
    work->field_4B8 = 0;
    work->field_4BA = 0;
    do {
        work->slots[(u16)i].rate = 0x10;
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
        obj        = (TmdObject*)task->extra;
        coord      = obj->coords;
        work       = (Actor160900Child3Work*)Mem_Malloc(0x4BC, false);
        task->work = (TaskIdMap*)work;
        if (work == NULL) {
            failed = 1;
        } else {
            coord->sub = &gGfxViewCoord;
            Mem_Set(task->work, 0, 0x4BC);
            obj->lightMtx  = &work->light;
            obj->colorMtx  = &work->color;
            obj->flags    |= 0x84;
            task->msgTable = D_actor_160900_8013F200;
            place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            while (place->entryId != 0xFF && place->entryId != 0x65) {
                place++;
            }
            Gp_SetTmdBytes((TmdObject*)task->extra, (s8)place->tpage, (s8)place->clut);
            Task_Reparent(D_actor_160900_8013FBB4, task);
            failed = 0;
        }
        if ((u16)failed) {
            taskKill(task);
            return;
        }
        func_actor_160900_InitAnim(task, task->extra);
        task->state++;
    }
    func_actor_160900_80132844(task);
    if (gGameSession->at4.loc.view == 0x2E) {
        Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[18].coord, 0x800, 1);
    } else {
        Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[18].coord, 0x79C, 1);
    }
    obj2   = (TmdObject*)task->extra;
    pos.vx = obj2->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(obj2, &pos, 0, 3);
}

void func_actor_160900_80132E80(Task* task)
{
    s16            xs[4];
    s16            ys[4];
    SVECTOR        origin;
    s32            sxy;
    s32            otz;
    GsCOORDINATE2* coord;
    SVECTOR*       verts;
    POLY_G4*       poly;
    DR_TPAGE*      tp;
    s16            i;

    coord = ((TmdObject*)task->extra)->coords;
    verts = &((Actor160900ChildWork*)task->work)->field_0;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vz = 0;
    origin.vy = 0;
    origin.vx = 0;
    gte_ldv0(&origin);
    gte_rtps_real();
    gte_stsxy(&sxy);
    gte_stszotz(&otz);
    for (i = 0; i < 4; i++) {
        gte_ldv0(&verts[i]);
        gte_rtps_real();
        gte_stsxy(&sxy);
        xs[i] = sxy;
        ys[i] = sxy >> 16;
    }

    poly           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 8);
    setcode(poly, 0x3A);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->r1 = 0;
    poly->g1 = 0;
    poly->b1 = 0;
    switch (task->spawnArg1) {
        case 0:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 1:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            break;
        case 2:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 3:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            break;
        case 4:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 5:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
    }
    poly->x0 = xs[0];
    poly->y0 = ys[0];
    poly->x1 = xs[1];
    poly->y1 = ys[1];
    poly->x2 = xs[2];
    poly->y2 = ys[2];
    poly->x3 = xs[3];
    poly->y3 = ys[3];
    addPrim(&gGpuCurrentOt[otz >> 4], poly);
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE1000220;
    addPrim(&gGpuCurrentOt[otz >> 4], tp);
}

static inline void func_actor_160900_SetAnim(Task* task, u16 anim)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    GpAnimArg*       p;

    work = (Actor160900Work*)task->work;
    p    = &msg;
    if (work->field_34 != NULL) {
        p->field_0     = D_actor_160900_8013F198;
        work->field_64 = anim;
        p->field_4     = anim;
        p->field_8     = 1;
        p->field_C     = 10;
        p->field_10    = 1;
        Gp_DispatchMsg(work->field_34, 0x3F4, (s32)p, 0);
        work->field_66 = 0;
    }
}

static inline void func_actor_160900_SetAnimZ(Task* task, u16 anim)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    GpAnimArg*       p;

    work = (Actor160900Work*)task->work;
    p    = &msg;
    if (work->field_34 != NULL) {
        p->field_0     = D_actor_160900_8013F198;
        work->field_64 = anim;
        p->field_4     = anim;
        p->field_8     = 0;
        p->field_C     = 0;
        p->field_10    = 1;
        Gp_DispatchMsg(work->field_34, 0x3F4, (s32)p, 0);
        work->field_66 = 0;
    }
}

void func_actor_160900_80133238(Task* arg0)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    Actor160900Work* w;
    s32              v;
    s32              x;

    work = (Actor160900Work*)arg0->work;
    func_actor_160900_801326EC(arg0);
    switch ((u16)work->field_4C) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_34, 0x3E9, (s32)D_actor_160900_8013F210, 0);
            w = (Actor160900Work*)arg0->work;
            if (w->field_34 != NULL) {
                msg.field_0  = D_actor_160900_8013F198;
                w->field_64  = 10;
                msg.field_4  = 10;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 1;
                Gp_DispatchMsg(w->field_34, 0x3F4, (s32)&msg, 0);
                w->field_66 = 0;
            }
            break;
        case 2:
            w = (Actor160900Work*)arg0->work;
            if (w->field_34 != NULL) {
                msg.field_0  = D_actor_160900_8013F198;
                w->field_64  = 1;
                msg.field_4  = 1;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 1;
                Gp_DispatchMsg(w->field_34, 0x3F4, (s32)&msg, 0);
                w->field_66 = 0;
            }
            break;
        case 3:
            if ((u16)work->field_4E == 0) {
                x = D_80073BA9;
                if (D_8007218A == 1) {
                    v = x + 1;
                } else {
                    v = x + 0x22;
                }
                msg.field_0  = (u8*)v;
                msg.field_4  = 1;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
                Gp_DispatchMsg(work->field_34, 0x3E9, (s32)D_actor_160900_8013F228, 0);
                msg.field_0 = (u8*)-0x7D0;
                msg.field_4 = 0;
                msg.field_8 = 0xC80;
                Gp_DispatchMsg(work->field_34, 0x3FB, (s32)&msg, 0);
                work->field_4E++;
            }
            return;
        case 4:
            work->field_0 = 8;
            work->field_2 = 0x80;
            Task_SpawnFromTable(&D_actor_160900_8013F17C, 0, 0, (s32)work);
            work->field_4C = 0;
            return;
        case 5:
            work->field_4 = 2;
            break;
        case 6:
            func_actor_160900_SetAnimZ(arg0, 2);
            break;
        case 7:
            func_actor_160900_SetAnim(arg0, 6);
            break;
        case 8:
            func_actor_160900_SetAnim(arg0, 7);
            break;
        case 9:
            func_actor_160900_SetAnim(arg0, 8);
            break;
        case 10:
            func_actor_160900_SetAnim(arg0, 9);
            break;
    }
    work->field_4C = 0;
}

void func_actor_160900_8013358C(Task* arg0)
{
    Actor160900Work*       work;
    Actor160900Child3Work* child;
    SVECTOR                ofs;
    SVECTOR                ofs2;
    s32                    i;

    work = (Actor160900Work*)arg0->work;
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
            Gp_SpawnEff(0x60046, ((TmdObject*)work->field_38->extra)->coords, 0x20000100, &ofs);
            break;
        case 3:
            child            = (Actor160900Child3Work*)work->field_38->work;
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
            Gp_SpawnEff(0x60046, ((TmdObject*)work->field_38->extra)->coords, 0x20000100, &ofs2);
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

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0x3E8;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x5DC;
    work->field_0.vz                              = 0x3E8;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x5DC;
    work->field_8.vz                              = 0;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x3E8;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = 0;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[1]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0x3E8;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x5DC;
    work->field_0.vz                              = 0;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x5DC;
    work->field_8.vz                              = -0x3E8;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x3E8;
}
void func_actor_160900_80133A84(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 5, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = 0x1F4;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = -0x1F4;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x1F4;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x1F4;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 2, 0);
    data->field_C[1]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = 0x3E8;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = 0x1F4;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x3E8;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = 0x1F4;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 3, 0);
    data->field_C[2]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = -0x1F4;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = -0x3E8;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = -0x1F4;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x3E8;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 4, 0);
    data->field_C[3]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = 0x1F4;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = -0x1F4;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x1F4;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x1F4;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[4]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = 0x3E8;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = 0x1F4;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x3E8;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = 0x1F4;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[5]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = -0x1F4;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x3E8;
    work->field_0.vz                              = -0x1F4;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x3E8;
    work->field_8.vz                              = -0x3E8;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = -0x1F4;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x3E8;
}
void func_actor_160900_80133F90(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x5DC;
    work->field_0.vz                              = 0x3E8;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x5DC;
    work->field_8.vz                              = 0;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0x3E8;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = 0;
    task                                          = Task_SpawnFromTable(&ActorsShared80136280Desc, 7, 1, 0);
    data->field_C[1]                              = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    ((TmdObject*)task->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)task->extra)->coords->coord.t[0] = 0x1770;
    ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)task->extra)->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                              = 0;
    work->field_0.vy                              = -0x5DC;
    work->field_0.vz                              = 0;
    work->field_8.vx                              = 0;
    work->field_8.vy                              = -0x5DC;
    work->field_8.vz                              = -0x3E8;
    work->field_10.vx                             = 0;
    work->field_10.vy                             = 0;
    work->field_10.vz                             = 0;
    work->field_18.vx                             = 0;
    work->field_18.vy                             = 0;
    work->field_18.vz                             = -0x3E8;
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
            work       = (Actor160900Work*)Mem_Malloc(0x68, 0);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x68);
                work->field_34          = gameGetPtrSlot(3);
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
            if (gGameSession->eventState == 0) {
                D_8007272D = 0x1E;
                Task_RequestKill(arg0, 0);
            }
            break;
    }
    func_actor_160900_80133238(arg0);
    func_actor_160900_8013358C(arg0);
    data = (Actor160900Work*)arg0->work;
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

    work = (Actor160900FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor160900FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
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

    work = (Actor160900FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor160900FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
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
                taskKill(arg0);
            }
            break;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_160900/actor_160900", func_actor_160900_801345D0);
