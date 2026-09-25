#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

extern SVECTOR D_shelter_b1_sleeping_quarters_8018054C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018055C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018056C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018058C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018059C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805BC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805EC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018060C[];

void func_shelter_b1_sleeping_quarters_8017DB50(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b1_sleeping_quarters_8017E338(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_shelter_b1_sleeping_quarters_8017D8E0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x60220;
        D_80115730  = 0x6022B;
        D_80115754  = 0x60236;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b1_sleeping_quarters_8017DB50(D_shelter_b1_sleeping_quarters_8018058C, 0x200, 0, 0x111);
        case 2:
            func_shelter_b1_sleeping_quarters_8017DB50(D_shelter_b1_sleeping_quarters_8018054C, 0x200, 0, 0x10);
            break;
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            func_shelter_b1_sleeping_quarters_8017DB50(&p[0], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[2], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[6], 0x200, -0x400, 0x111);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018059C;
            func_shelter_b1_sleeping_quarters_8017DB50(&p[0], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[6], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[8], 0x200, 0, 0x111);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_801805EC;
            func_shelter_b1_sleeping_quarters_8017DB50(&p[0], 0x200, 0x400, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[2], 0x200, 0x400, 0x111);
            break;
        }
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            func_shelter_b1_sleeping_quarters_8017DB50(&p[0], 0x200, -0x400, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[2], 0x200, 0, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[8], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[12], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[14], 0x200, 0, 0x111);
            break;
        }
        case 8:
            func_shelter_b1_sleeping_quarters_8017DB50(D_shelter_b1_sleeping_quarters_801805BC, 0x200, 0x800, 0x111);
        case 9:
            func_shelter_b1_sleeping_quarters_8017E338(D_shelter_b1_sleeping_quarters_8018055C, 0x300, 0x100);
            break;
        case 10: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018060C;
            func_shelter_b1_sleeping_quarters_8017DB50(&p[0], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[2], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[4], 0x200, 0x800, 0x111);
            func_shelter_b1_sleeping_quarters_8017DB50(&p[6], 0x200, 0x800, 0x111);
            break;
        }
    }
}

/// Queues a gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The centre colour
/// takes its red, green and blue from bits 8-15, 4 and 0 of `arg3`, scaled by
/// a brightness alternating between 0x20 and 0x28 on successive frames.
/// Nothing is drawn when the second point lies nearer than OTZ 0x11.
void func_shelter_b1_sleeping_quarters_8017DB50(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Queues a gouraud disc of four quads at the projected point `arg0`, of
/// radius `arg1` scaled by depth. The centre colour takes its red, green and
/// blue from bits 8-15, 4 and 0 of `arg2`, scaled by a brightness alternating
/// between 0x20 and 0x28 on successive frames. Nothing is drawn nearer than
/// OTZ 0x11.
void func_shelter_b1_sleeping_quarters_8017E338(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = (((u8)((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 1);
        b             = blend * (arg2 & 1);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0xC);
}
