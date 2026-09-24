#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_g_r_kitchen.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Four local-space points forming two light beams, laid end to end. splat
/// names this address because it is the one the code forms in a register; the
/// table itself starts one SVECTOR earlier, so the first beam here runs from
/// `[1]` back to `[0]` and the second from `[3]` to `[2]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EBF0[];
/// Same four-point layout, but both beams run forwards: `[0]` to `[1]` and
/// `[2]` to `[3]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EC08[];

void func_dryfield_g_r_kitchen_8017E27C(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// The same tapered light beam as `func_dryfield_g_r_kitchen_8017D9FC`,
/// between two points of `arg0`'s local space, with every angle turned back a
/// quarter turn. The two
/// `RTPS` projections, the drop when the far end's `otz` is below 0x11, the
/// clamp of the near end's `otz` to 0x10 and the radii `(s16)arg3 * 64 / otz`
/// are unchanged.
///
/// The near cap's wedges cover -0x400..0x400, the far cap's the opposite half
/// walked backwards from 0xC00 to 0x400, and the side quads join the two
/// circles at -0x400 and 0x400. The centre colour is 0x10 or 0x20 on the
/// parity of `gDisplayState.animFrame`, one step darker than
/// `func_dryfield_g_r_kitchen_8017D9FC`'s.
void func_dryfield_g_r_kitchen_8017E27C(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0_real();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0_real();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x10;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang - 0x400)) >> 12);
            t        = ang - 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang - 0x400)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xA00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xA00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0x800 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0x800 - ang)) >> 12);
            ang     += 0x400;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}

/// Draws two light beams under the coordinate of the model in `arg0->extra`,
/// picked by the current view `gGameSession->at4.loc.view`: in view 2 the
/// beams of `D_dryfield_g_r_kitchen_8017EBF0` through
/// `func_dryfield_g_r_kitchen_8017D9FC`, in view 3 those of
/// `D_dryfield_g_r_kitchen_8017EC08` through
/// `func_dryfield_g_r_kitchen_8017E27C`. Any other view draws nothing.
void func_dryfield_g_r_kitchen_8017EB04(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->coords;
    if (gGameSession->at4.loc.view == 2) {
        func_dryfield_g_r_kitchen_8017D9FC(coord, &D_dryfield_g_r_kitchen_8017EBF0[0], &D_dryfield_g_r_kitchen_8017EBF0[-1], 0x100);
        func_dryfield_g_r_kitchen_8017D9FC(coord, &D_dryfield_g_r_kitchen_8017EBF0[2], &D_dryfield_g_r_kitchen_8017EBF0[1], 0x100);
    } else if (gGameSession->at4.loc.view == 3) {
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[0], &D_dryfield_g_r_kitchen_8017EC08[1], 0x100);
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[2], &D_dryfield_g_r_kitchen_8017EC08[3], 0x100);
    }
}
