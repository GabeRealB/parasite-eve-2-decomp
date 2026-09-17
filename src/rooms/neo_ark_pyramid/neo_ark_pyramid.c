#include "common.h"

#include "main/display.h"

#include <psyq/libgpu.h>

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid", D_neo_ark_pyramid_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid", func_neo_ark_pyramid_8017D600);

void func_neo_ark_pyramid_8017D7F4(s32 arg0)
{
    POLY_FT4* prim;
    s16       src[4][2];
    s16       dst[4][2];
    s32       i;

    src[0][0] = -0x57;
    src[0][1] = -0x57;
    src[1][0] = 0x57;
    src[1][1] = -0x57;
    src[2][0] = -0x57;
    src[2][1] = 0x57;
    src[3][0] = 0x57;
    src[3][1] = 0x57;
    for (i = 0; i < 4; i++) {
        dst[i][0] = (src[i][0] * rcos(arg0) - src[i][1] * rsin(arg0)) >> 12;
        dst[i][1] = (src[i][0] * rsin(arg0) + src[i][1] * rcos(arg0)) >> 12;
    }
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2D);
    prim->x0    = dst[0][0];
    prim->y0    = dst[0][1];
    prim->x1    = dst[1][0];
    prim->y1    = dst[1][1];
    prim->x2    = dst[2][0];
    prim->y2    = dst[2][1];
    prim->x3    = dst[3][0];
    prim->y3    = dst[3][1];
    prim->u0    = 1;
    prim->v0    = 1;
    prim->u1    = 0xAF;
    prim->v1    = 1;
    prim->u2    = 1;
    prim->v2    = 0xAF;
    prim->u3    = 0xAF;
    prim->v3    = 0xAF;
    prim->clut  = 0x3FC0;
    prim->tpage = 0x8E;
    addPrim(Gpu_CurrentOt + 0xC, prim);
}

s32 func_neo_ark_pyramid_8017D9F0(void)
{
    return 0;
}
