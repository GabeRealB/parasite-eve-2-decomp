#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "rooms/neo_ark_eve_access_tunnel.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// One of the per-view objects the tunnel's sprite-table record points at. Its
/// flag sits at a different offset in each of the record's three pointers --
/// 0x24 for `field_1C`, 0x1C for `field_28` and 0x14 for `field_34`, eight
/// bytes apart, one `GpSprtCmd` record each, the same shape the gas station's
/// `DryfieldNightGasStationSprtView` has. `func_neo_ark_eve_access_tunnel_8017E090`
/// is the only writer: it sets the flag to 1 or clears it to 0.
typedef struct NaetSprtView {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x7];
    /* 0x1C */ u8   field_1C;
    /* 0x1D */ byte pad_1D[0x7];
    /* 0x24 */ u8   field_24;
} NaetSprtView;
STATIC_ASSERT_SIZEOF(NaetSprtView, 0x25);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at,
/// reached through a cast for the same reason the neighbouring rooms' records
/// are: it is a room-sized block, far larger than the 0xC-byte `GpSprtRec` the
/// table's element type declares. `field_1C` / `field_28` / `field_34` are a
/// run twelve bytes apart, one `GpSprtRec` each, and each points at one of the
/// views above -- the first alone, the second and third as a pair.
typedef struct NaetSprtRec {
    /* 0x00 */ byte          pad_0[0x1C];
    /* 0x1C */ NaetSprtView* field_1C;
    /* 0x20 */ byte          pad_20[0x8];
    /* 0x28 */ NaetSprtView* field_28;
    /* 0x2C */ byte          pad_2C[0x8];
    /* 0x34 */ NaetSprtView* field_34;
} NaetSprtRec;
STATIC_ASSERT_SIZEOF(NaetSprtRec, 0x38);

void func_neo_ark_eve_access_tunnel_8017E244(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Live emitters for the tunnel's views, in the shared data blob at the end of
/// the overlay. `D_..._8017EB48` doubles as case 2's three-entry run and case 6's
/// two-entry one, so both views share one base pointer.
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[];

void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1)
{
    GpAreaKey*    sess = &gGameSession->at4.loc;
    NaetSprtRec*  rec  = (NaetSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    NaetSprtView* view;
    s32           run = arg0 & 0xFF;
    s32           flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view           = rec->field_1C;
            view->field_24 = 1;
            return;
        }
        if (flag == 1) {
            view           = rec->field_1C;
            view->field_24 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view           = rec->field_28;
            view->field_1C = run;
            view           = rec->field_34;
            view->field_14 = run;
            return;
        }
        if (flag == run) {
            view           = rec->field_28;
            view->field_1C = 0;
            view           = rec->field_34;
            view->field_14 = 0;
        }
    }
}

/// Draws whichever emitters the current view shows: two adjacent positions per
/// drawn wedge, stepping through the view's run. View 4 chains into view 5's
/// emitters (`D_..._8017EB08` then `D_..._8017EB28`); every other view stops at
/// its own.
void func_neo_ark_eve_access_tunnel_8017E15C(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB48[4], 0x180, 0x444);
            break;
        case 3:
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EAE8[0], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EAE8[2], 0x180, 0x444);
            break;
        case 4:
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB08[0], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB08[2], 0x180, 0x444);
            /* fallthrough */
        case 5:
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB28[0], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB28[2], 0x180, 0x444);
            break;
        case 6:
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            func_neo_ark_eve_access_tunnel_8017E244(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            break;
    }
}

/// Draws a glow between the two world points `arg0[0]` and `arg0[1]`: both are
/// projected through `Gfx_ViewWorldMtx`, and unless the GTE flags either
/// projection, gouraud `POLY_G4` wedges are queued around each end and a band
/// joins them, each tinted at the centre and black at the rim. `arg1` is the
/// radius in world units, scaled by each point's depth; `arg2` is the tint as
/// three 4-bit channels (red at bit 8, green at bit 4, blue at bit 0), with 8
/// added to each on odd display frames so the glow flickers.
void func_neo_ark_eve_access_tunnel_8017E244(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps_real();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
