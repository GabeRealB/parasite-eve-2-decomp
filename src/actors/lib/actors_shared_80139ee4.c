#include "common.h"

#include "actors/actors_shared_80139ee4.h"

#include "gameplay/3A34.h"
#include "main/gfx.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Hover state of the enemy that rises out of view and slams back down: once a
/// step, rebuild the model's Y rotation from its own yaw (`ratan2` of
/// `-m[2][0]` over `m[2][2]`) and rescale it through a 0x34-byte block borrowed
/// from the scratchpad -- wide and flat (`0x4000, 0x66, 0x4000`) for the first
/// 0xA steps, then taller (`0x4C00, 0x199, 0x4C00`). The shadow marker's size
/// follows the step counter until it is pinned at `0x380`. After 0xC steps the
/// collision node is unlinked and the task steps on; either way the work
/// block's coordinate keeps tracking the model.
///
/// The absolute `G_SCRATCH_HEAD` accesses are written out: at `-O2` the
/// expander forces a constant address into a register (`explow.c`
/// `memory_address`), so the `lui $at` assembler-macro form the original
/// carries cannot come from plain C here.
void ActorsShared80139ee4(GpEnemy* enemy, ActorsShared80139ee4Task* task)
{
    ActorsShared80139ee4Work*    work;
    TmdObject*                   extra;
    GsCOORDINATE2*               coord;
    ActorsShared80139ee4Scratch* blk;
    u8*                          head;
    s16                          ang;

    work = task->field_1C;
    work->timer++;
    if ((s16)work->timer < 0xA) {
        u8* tail;
        s16 spin;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->field_8;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4000;
        blk->scale.vy = 0x66;
        blk->scale.vz = 0x4000;
        ScaleMatrix(&blk->m, &((ActorsShared80139ee4Scratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorsShared80139ee4Scratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        m22                  = *(u16*)&blk->m.m[2][2];
        coord->flg           = 0;
        coord->coord.m[2][2] = m22;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail = *(u8**)(tail + 0x3FC);
        tail = tail + 0x34;
        spin = (s16)work->timer * 0x40 + 0x100;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        TOUCH_REG(tail);
        work->obj.field_1C = spin;
    } else {
        u8* tail;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->field_8;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4C00;
        blk->scale.vy = 0x199;
        blk->scale.vz = 0x4C00;
        ScaleMatrix(&blk->m, &((ActorsShared80139ee4Scratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorsShared80139ee4Scratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail       = *(u8**)(tail + 0x3FC);
        m22        = *(u16*)&blk->m.m[2][2];
        coord->flg = 0;
        tail       = tail + 0x34;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        coord->coord.m[2][2] = m22;
        work->obj.field_1C   = 0x380;
    }

    Gp_ClearRec18Occupied(&work->rec);
    if ((s16)work->timer >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        task->state++;
    }

    work->coord.coord.t[0] = task->extra->field_8->coord.t[0];
    work->coord.coord.t[1] = task->extra->field_8->coord.t[1];
    work->coord.coord.t[2] = task->extra->field_8->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}
