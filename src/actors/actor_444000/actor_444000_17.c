#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/tmd.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Landing state of the dropped enemy, the one after the descent in
/// `D_actor_444000_80131F1C`: each step rebuild the model's rotation about y
/// from its own yaw and flatten it through a 0x34-byte block borrowed from the
/// scratchpad -- scaled (`0x4000, 0x66, 0x4000`) for the first 0xA steps, then
/// (`0x4C00, 0x199, 0x4C00`). The collision object's radius grows with the step
/// counter over the first steps and is then held at 0x380. After 0xC steps the
/// object is unlinked and the task steps on; either way the work block's
/// coordinate keeps tracking the model.
///
/// The absolute `G_SCRATCH_HEAD` accesses are written out: at `-O2` the
/// expander forces a constant address into a register (`explow.c`
/// `memory_address`), so the `lui $at` assembler-macro form the original
/// carries cannot come from plain C here.
void func_actor_444000_80139EE4(GpEnemy* enemy, Actor444000Drop* task)
{
    Actor444000DropWork*   work;
    TmdObject*             extra;
    GsCOORDINATE2*         coord;
    Actor444000RotScratch* blk;
    u8*                    head;
    s16                    ang;

    work = task->field_1C;
    work->timer++;
    if ((s16)work->timer < 0xA) {
        u8* tail;
        s16 spin;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
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
        ScaleMatrix(&blk->m, &((Actor444000RotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((Actor444000RotScratch*)(head - 0x34))->m.m[0][0];
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
        work->obj.radius = spin;
    } else {
        u8* tail;
        u16 m22;

        extra = task->extra;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
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
        ScaleMatrix(&blk->m, &((Actor444000RotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((Actor444000RotScratch*)(head - 0x34))->m.m[0][0];
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
        work->obj.radius     = 0x380;
    }

    Gp_ClearRec18Occupied(&work->rec);
    if ((s16)work->timer >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        task->state++;
    }

    work->coord.coord.t[0] = task->extra->coords->coord.t[0];
    work->coord.coord.t[1] = task->extra->coords->coord.t[1];
    work->coord.coord.t[2] = task->extra->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}
