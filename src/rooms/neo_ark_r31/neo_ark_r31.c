#include "common.h"
#include "main/display.h"
#include "main/task.h"

extern s32 D_neo_ark_r31_8017DC54;
extern u8  D_80071071[];
extern s32 D_8007107C;

void func_neo_ark_r31_8017D5D0(Task* task)
{
    POLY_FT4* poly;
    DR_STP*   stp;
    s32       buf;
    s32       otz;
    s32       x;
    s32       y;
    s32       sx;
    s32       sy;
    s32       px;

    otz = 6;
    buf = D_8007107C;
    if (task->state == 0) {
        D_neo_ark_r31_8017DC54 = 3;
        task->state++;
    }
    if (D_neo_ark_r31_8017DC54 < 0) {
        Task_CallExit(task);
        return;
    }
    for (x = 0; x < 0x140; x += 0xA0) {
        sx = x - 0xA0;
        for (y = 0; y < 0xF0; y += 0xF0) {
            sy              = y - 0x78;
            poly            = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor += sizeof(POLY_FT4);
            poly->tpage     = getTPage(2, 0, x & ~0x3F, buf << 8);
            poly->y0 = poly->y1 = sy;
            poly->v0 = poly->v1 = D_80071071[0] + (y + (buf << 4));
            if (poly->v0 < 0x10) {
                poly->y2 = poly->y3 = y + 0x78;
                poly->v2 = poly->v3 = poly->v0 + 0xF0;
            } else {
                s32 d    = 0xFF - poly->v0;
                poly->y2 = poly->y3 = sy + d;
                poly->v2 = poly->v3 = poly->v0 + d;
            }
            px       = sx - D_neo_ark_r31_8017DC54;
            poly->x0 = poly->x2 = px;
            poly->u0 = poly->u2 = x & 0x3F;
            if (poly->u0 < 0x60) {
                poly->x1 = poly->x3 = poly->x0 + 0xA0;
                poly->u1 = poly->u3 = poly->u0 + 0xA0;
            } else {
                s32 d    = 0xFF - poly->u0;
                poly->x1 = poly->x3 = poly->x0 + d;
                poly->u1 = poly->u3 = poly->u0 + d;
            }
            setlen(poly, 9);
            setcode(poly, 0x2F);
            addPrim(gGpuCurrentOt + otz, poly);
        }
    }
    stp             = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor += sizeof(DR_STP);
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + otz, stp);
    stp             = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor += sizeof(DR_STP);
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 0x3FF, stp);
}

s32 func_neo_ark_r31_8017D8B0(void)
{
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/neo_ark_r31/neo_ark_r31", RoomsShared8017d878Table);
