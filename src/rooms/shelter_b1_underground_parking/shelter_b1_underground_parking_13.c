#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "rooms/shelter_b1_underground_parking.h"

void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color)
{
    POLY_G4* prim;
    s32      i;
    s32      ang;
    s32      t;
    u8       r;
    u8       g;
    u8       b;
    s32      base;
    s32      c;
    s32      rMask;
    s32      gMask;

    i     = 0;
    base  = (gDisplayState.animFrame & 1) * 12;
    c     = color;
    rMask = (c >> 4) & 0xF0;
    gMask = c & 0xF0;
    r     = base + rMask;
    g     = base + gMask;
    b     = base + ((color & 0xF) << 4);
    do {
        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = x + ((radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = y + ((radius * rcos(ang)) >> 12);
            prim->x1 = x + ((radius * rsin(t)) >> 12);
            prim->y1 = y + ((radius * rcos(t)) >> 12);
            t        = ang + 0x200;
            prim->x2 = x;
            prim->y2 = y;
            prim->x3 = x + ((radius * rsin(t)) >> 12);
            prim->y3 = y + ((radius * rcos(t)) >> 12);
            ang      = t;
            addPrim((u_long*)(((((u32)0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
        } while (ang < 0x1000);
        radius <<= 1;
        r      >>= 1;
        g      >>= 1;
        b      >>= 1;
        i++;
    } while (i < 3);
}
