#include "common.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern SVECTOR D_dryfield_r08_8017F464[];
extern SVECTOR D_dryfield_r08_8017F4C4[];
extern s32     D_dryfield_r08_80180C24;

void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_dryfield_r08_8017D5F8(Task* task)
{
    s32 i;
    u8  view;

    if (task->state == 0) {
        D_dryfield_r08_80180C24 = 0;
        task->state             = task->state + 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x444);
            Room_Draw31(&q[2], 0x200, 0x444);
            Room_Draw31(&q[3], 0x200, 0x444);
            Room_Draw31(&q[6], 0x200, 0x444);
            Room_Draw31(&q[14], 0x200, 0x444);
            Room_Draw31(&q[15], 0x200, 0x444);
            Room_Draw31(&q[16], 0x200, 0x444);
            Room_Draw31(&q[17], 0x200, 0x444);
            break;
        }
        case 3:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 4:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 5: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x444);
            Room_Draw31(&q[1], 0x200, 0x444);
            Room_Draw31(&q[2], 0x200, 0x444);
            Room_Draw31(&q[7], 0x200, 0x444);
            Room_Draw31(&q[8], 0x200, 0x444);
            Room_Draw31(&q[21], 0x200, 0x400);
            Room_Draw31(&q[22], 0x200, 0x400);
            Room_Draw31(&q[23], 0x200, 0x400);
            Room_Draw31(&q[29], 0x200, 0x400);
            break;
        }
        case 6: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x433);
            Room_Draw31(&q[2], 0x200, 0x433);
            Room_Draw31(&q[3], 0x200, 0x433);
            Room_Draw31(&q[6], 0x200, 0x433);
            Room_Draw31(&q[14], 0x200, 0x433);
            Room_Draw31(&q[15], 0x200, 0x433);
            Room_Draw31(&q[16], 0x200, 0x433);
            Room_Draw31(&q[17], 0x200, 0x433);
            break;
        }
    }
}

/// The zero word the target keeps between this unit's jump table and
/// `func_dryfield_r08_8017D8B4`'s: the original TU's own tables were preceded
/// by `.align 3`, and this one pads the end of this function's table to the
/// 8-byte boundary the next one started on.
const u32 D_dryfield_r08_8017D5D8 SECTION(".rodata") = 0;

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08", func_dryfield_r08_8017D8B4);

/// Same projected, spinning `POLY_FT4` as `func_dryfield_r08_8017E36C`, with
/// its own texture window: the low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 + 0x68`), and the
/// top four bits select the clut - row `0x10E + sel` at column `cell & 0x3F`
/// for 0 and 1, the fixed clut 0x428F otherwise.
void func_dryfield_r08_8017DEFC(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw39Scratch* block;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                vz;
    s32                tex;
    u16                sel;
    s32                sine;
    RoomDraw39Scratch* tmp;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG(arg2);
    head = *scratch;
    tmp  = (RoomDraw39Scratch*)(head - 0x1C);
    SOFT_TOUCH_REG(tmp);
    block         = tmp;
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    sel           = arg1 >> 12;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw39Scratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw39Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw39Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2B;
        prim->code |= 3;
        if (sel >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((sel + 0x10E) << 6) | (tex & 0x3F);
        }
        ang = (s16)arg3;
        u0  = ((u16)tex % 5) * 0x30;
        v0  = ((u16)tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x69;
        v0  = v0 + 0x68;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = (s16)arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects `arg0`'s world translation through `GsWSMATRIX` and, unless the
/// GTE flag word is negative, queues one semi-transparent `POLY_FT4` centred on
/// the projected point. The low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 - 0x80`); any of
/// its top four bits set selects clut 0x428F instead of 0x43D0. The quad's
/// diagonals are `(s16)arg2 * 47 / otz` long, turned by `arg3` and
/// `arg3 + 0x400`, so it shrinks with distance and spins with the angle.
void func_dryfield_r08_8017E36C(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw39Scratch* block;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                vz;
    u16                tex;
    u16                sel;
    s32                sine;
    RoomDraw39Scratch* tmp;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG(arg2);
    head = *scratch;
    tmp  = (RoomDraw39Scratch*)(head - 0x1C);
    SOFT_TOUCH_REG(tmp);
    block         = tmp;
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    sel           = arg1 >> 12;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw39Scratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw39Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw39Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2C;
        prim->code |= 3;
        if (sel) {
            prim->clut = 0x428F;
        } else {
            prim->clut = 0x43D0;
        }
        ang = (s16)arg3;
        u0  = (tex % 5) * 0x30;
        v0  = (tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x51;
        v0  = v0 - 0x80;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = (s16)arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
