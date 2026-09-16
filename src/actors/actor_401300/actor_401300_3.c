#include "common.h"

#include "actors/actor_401300.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include <psyq/inline_c.h>

void func_actor_401300_80141758(Task* task)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = (Actor401300Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_D0C != NULL) {
            Task_Kill(work->field_D0C);
        }
        if (work->field_D10 != NULL) {
            Task_Kill(work->field_D10);
        }
        Gp_UnlinkObj(&work->field_BF0);
        Gp_UnlinkObj(&work->field_970);
        Gp_UnlinkObj(&work->field_AB0);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

/// Walks `p` up its parent chain to `Gfx_ViewCoord`, transforming `out` by each
/// coordinate; `out` is left unchanged if the chain ends before the view.
static __inline__ void Actor401300_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &Gfx_ViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

s32 func_actor_401300_801417F0(Actor401300* arg0)
{
    SVECTOR out;

    memset(&out, 0, 8);
    Actor401300_TransformToView(&arg0->field_2C->field_8[1], &out);
    return (u16)(out.vz + 0x12B) < 0xA27;
}

void func_actor_401300_8014192C(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                   = arg0->field_2C;
        enemy->node.field_4   = 1;
        obj->field_C          = (u16)(obj->field_C | 0x80);
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        return;
    }
    if (enemy->field_40 != -0x3E7 && work->field_C8A == 0 && arg0->field_36 == 2) {
        enemy->field_40 = -0x3E7;
    }
}

void func_actor_401300_801419B8(Actor401300* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 2;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141A60(Actor401300* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 3;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141B0C(Actor401300* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 0xB;
        work->field_8B6       = 0x20;
        work->field_8BA       = 8;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141BC8(Actor401300* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x12;
        work->field_8A2       = 0xD;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

void func_actor_401300_80141C80(void)
{
}

void func_actor_401300_80141C88(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 2;
        work->field_8A2          = 8;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        work->field_8A6          = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        if ((*(s32*)&Game_Session->field_4 & 0xFFFF0000) == 0x051D0000) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
}

void func_actor_401300_80141D50(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 2;
        work->field_8A2          = 0x16;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        work->field_8A6          = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141DF4);

void func_actor_401300_80141EF8(Task* task)
{
    Actor401300Work* work  = (Actor401300Work*)task->idMap;
    GpEnemy*         enemy = task->spawnArg2;

    if (enemy->field_40 != -0x3E7 && work->field_C8A == 0) {
        enemy->field_40 = -0x3E7;
    }
}
