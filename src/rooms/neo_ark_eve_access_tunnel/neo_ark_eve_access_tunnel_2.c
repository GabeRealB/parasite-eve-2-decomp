#include "neo_ark_eve_access_tunnel_private.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "rooms/neo_ark_eve_access_tunnel.h"
#include "rooms/room_common.h"

#include "gameplay/display.h"
#include "gameplay/loading.h"
#include "gameplay/room_effects.h"

#include "gameplay/sprites.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "overlay.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/room.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

#include "gameplay/message.h"

static void func_neo_ark_eve_access_tunnel_8017E244(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Live emitters for the tunnel's views, in the shared data blob at the end of
/// the overlay. `D_..._8017EB48` doubles as case 2's three-entry run and case 6's
/// two-entry one, so both views share one base pointer.
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[];

extern GpGridParams D_neo_ark_eve_access_tunnel_8017F05C[1];
extern GpObj3A D_neo_ark_eve_access_tunnel_80180720[1];
extern GpObj4C D_neo_ark_eve_access_tunnel_801802EC[6];
extern GpObj4C D_neo_ark_eve_access_tunnel_801804B4[6];
extern GpRoomCoordSet D_neo_ark_eve_access_tunnel_801802D4[1];

TaskDesc D_neo_ark_eve_access_tunnel_8017EA88 = { 0, 32, func_neo_ark_eve_access_tunnel_8017D810, { .model = NULL } };

GpMsgEntry D_neo_ark_eve_access_tunnel_8017EA94[6] = {
    { 5102, func_neo_ark_eve_access_tunnel_8017DC6C },
    { 5105, func_neo_ark_eve_access_tunnel_8017DC64 },
    { 5103, func_neo_ark_eve_access_tunnel_8017DE1C },
    { 5104, func_neo_ark_eve_access_tunnel_8017DD70 },
    { 5106, func_neo_ark_eve_access_tunnel_8017DE9C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[3] = {
    { 0, 32, func_neo_ark_eve_access_tunnel_8017D980, { .model = NULL } },
    { 0, 32, func_neo_ark_eve_access_tunnel_8017DB18, { .model = NULL } },
    { 0, 32, func_neo_ark_eve_access_tunnel_8017DED0, { .model = NULL } },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[4] = {
    { -3797, -2535, 1155, 0 },
    { -2796, -2535, 1155, 0 },
    { -3797, -2535, 928, 0 },
    { -2796, -2535, 928, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[4] = {
    { -1355, -2535, 2897, 0 },
    { -1355, -2535, 3897, 0 },
    { -1123, -2535, 2897, 0 },
    { -1123, -2535, 3897, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[4] = {
    { -1355, -2535, 5762, 0 },
    { -1355, -2535, 6762, 0 },
    { -1123, -2535, 5762, 0 },
    { -1123, -2535, 6762, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[6] = {
    { -1874, -39, 1161, 0 },
    { -1874, -39, 667, 0 },
    { -267, -39, 1161, 0 },
    { -267, -39, 667, 0 },
    { -835, -39, 1836, 0 },
    { -1335, -39, 1836, 0 },
};

GpRoomObjRec D_neo_ark_eve_access_tunnel_8017EB78[1] = {
    { D_neo_ark_eve_access_tunnel_8017F05C, D_neo_ark_eve_access_tunnel_801802EC, D_neo_ark_eve_access_tunnel_801804B4, D_neo_ark_eve_access_tunnel_80180720 },
};

GpRoomCoordRec D_neo_ark_eve_access_tunnel_8017EB88[1] = {
    { D_neo_ark_eve_access_tunnel_801802D4, NULL },
};

u8 * D_neo_ark_eve_access_tunnel_8017EB90[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_eve_access_tunnel_8017EB94[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_neo_ark_eve_access_tunnel_8017EB98[2] = {
    { { .words = { 0, -800, 0, 850 } }, { 0, 0, 0, 0 }, { .words = { 0, -800, 0, 850 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 431 },
    { { .words = { 1024, -1167, 0, 4892 } }, { 0, 0, 0, 0 }, { .words = { 1024, -1167, 0, 4892 } }, { 0, 0, 0, 0 }, 0x55080004, 0x55080003, 0, 5, 0, 444 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EC08[19] = {
    { 3982, 0, -959, 0 },
    { 0, 4096, 0, 0 },
    { 3148, 0, 2621, 0 },
    { 3148, 0, -2621, 0 },
    { 3982, 0, 959, 0 },
    { -2441, 0, 3289, 0 },
    { -17, 0, -4096, 0 },
    { 3094, 0, 2684, 0 },
    { -421, 0, 4074, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { -1012, 0, -3969, 0 },
    { -4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 2683, 0, -3095, 0 },
    { -880, 0, -4000, 0 },
    { 2544, 0, 3210, 0 },
    { -3448, 0, -2210, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017ECA0[55] = {
    { -1801, 0, 4188, 0 },
    { -1801, -2800, 4188, 0 },
    { -1628, -2800, 4907, 0 },
    { -1628, 0, 4907, 0 },
    { -395, -2800, 1754, 0 },
    { -4333, -2800, 93, 0 },
    { -167, -2800, 93, 0 },
    { -167, -2800, 1695, 0 },
    { -2029, -2800, 1929, 0 },
    { -395, -2800, 7214, 0 },
    { -2029, -2800, 7214, 0 },
    { -1801, 0, 5625, 0 },
    { -1801, -2800, 5625, 0 },
    { -2361, -2800, 6298, 0 },
    { -2361, 0, 6298, 0 },
    { -2361, 0, 3515, 0 },
    { -2361, -2800, 3515, 0 },
    { -1951, 144, 290, 0 },
    { -1951, -822, 290, 0 },
    { -2522, -822, -134, 0 },
    { -2522, 144, -134, 0 },
    { 438, -822, -146, 0 },
    { 438, 144, -146, 0 },
    { -105, -822, 480, 0 },
    { -105, 144, 480, 0 },
    { -4333, -2800, 1929, 0 },
    { -4333, 0, 1929, 0 },
    { -4333, 0, 93, 0 },
    { -167, 0, 93, 0 },
    { -2029, 0, 1929, 0 },
    { -395, 0, 7214, 0 },
    { -2029, 0, 7214, 0 },
    { -167, 0, 1695, 0 },
    { -395, 0, 1754, 0 },
    { -500, 0, 2000, 0 },
    { -2000, 0, 2000, 0 },
    { -2000, 0, 7333, 0 },
    { -500, 0, 7333, 0 },
    { -1967, 0, -50, 0 },
    { -4452, 0, -50, 0 },
    { -4452, 0, 2000, 0 },
    { -48, 0, -50, 0 },
    { -48, 0, 2000, 0 },
    { -1548, 100, 1846, 0 },
    { -1848, 100, 1586, 0 },
    { -1848, -990, 1586, 0 },
    { -1548, -990, 1846, 0 },
    { -1548, -990, 2296, 0 },
    { -1548, 100, 2296, 0 },
    { -2348, 100, 1696, 0 },
    { -2348, -990, 1696, 0 },
    { -2078, -990, 2716, 0 },
    { -2078, 100, 2716, 0 },
    { -2598, 100, 2086, 0 },
    { -2598, -990, 2086, 0 },
};

GpGridFace D_neo_ark_eve_access_tunnel_8017EE58[28] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 4, 9, 8, 10 }, 1, 0 },
    { { 12, 13, 11, 14 }, 2, 0 },
    { { 16, 1, 15, 0 }, 3, 0 },
    { { 2, 12, 3, 11 }, 4, 0 },
    { { 18, 19, 17, 20 }, 5, 0 },
    { { 19, 21, 20, 22 }, 6, 0 },
    { { 24, 22, 23, 21 }, 7, 0 },
    { { 23, 18, 24, 17 }, 8, 0 },
    { { 26, 27, 25, 5 }, 9, 0 },
    { { 28, 6, 27, 5 }, 10, 0 },
    { { 8, 29, 25, 26 }, 11, 0 },
    { { 9, 30, 10, 31 }, 11, 0 },
    { { 33, 4, 32, 7 }, 12, 0 },
    { { 4, 33, 9, 30 }, 13, 0 },
    { { 29, 8, 31, 10 }, 9, 0 },
    { { 28, 32, 6, 7 }, 13, 0 },
    { { 8, 25, 4, 5 }, 1, 0 },
    { { 35, 36, 34, 37 }, 14, 1 },
    { { 38, 39, 35, 40 }, 14, 1 },
    { { 35, 34, 41, 42 }, 14, 2 },
    { { 38, 35, 41, 0xFFFF }, 14, 2 },
    { { 44, 45, 43, 46 }, 15, 0 },
    { { 46, 47, 43, 48 }, 9, 0 },
    { { 50, 45, 49, 44 }, 16, 0 },
    { { 47, 51, 48, 52 }, 17, 0 },
    { { 54, 50, 53, 49 }, 18, 0 },
};

s16 D_neo_ark_eve_access_tunnel_8017EFA8[28] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    -1,
};

s16 D_neo_ark_eve_access_tunnel_8017EFE0[18] = {
    0,
    2,
    3,
    4,
    5,
    12,
    13,
    15,
    16,
    18,
    19,
    20,
    21,
    22,
    24,
    26,
    27,
    -1,
};

s16 D_neo_ark_eve_access_tunnel_8017F004[25] = {
    0,
    1,
    2,
    4,
    6,
    7,
    8,
    9,
    11,
    12,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    -1,
};

s16 D_neo_ark_eve_access_tunnel_8017F038[10] = {
    0,
    2,
    3,
    4,
    5,
    13,
    15,
    16,
    19,
    -1,
};

s16 * D_neo_ark_eve_access_tunnel_8017F04C[4] = {
    D_neo_ark_eve_access_tunnel_8017EFA8,
    D_neo_ark_eve_access_tunnel_8017EFE0,
    D_neo_ark_eve_access_tunnel_8017F004,
    D_neo_ark_eve_access_tunnel_8017F038,
};

GpGridParams D_neo_ark_eve_access_tunnel_8017F05C[1] = {
    { NULL, D_neo_ark_eve_access_tunnel_8017EC08, D_neo_ark_eve_access_tunnel_8017ECA0, D_neo_ark_eve_access_tunnel_8017EE58, D_neo_ark_eve_access_tunnel_8017F04C, 4452, 146, 2, 2, 4000, 28 },
};

GpViewRec D_neo_ark_eve_access_tunnel_8017F080[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 2144, 0x363E, -3758 } }, 333 },
    { { { { 1138, 0, -3934 }, { 45, 4095, 13 }, { 3934, -47, 1138 } }, { 4412, 1126, -236 } }, 235 },
    { { { { 1280, 0, 3890 }, { 317, 4082, -104 }, { -3877, 334, 1275 } }, { 98, 1391, -212 } }, 235 },
    { { { { 4057, 0, 557 }, { 80, 4052, -589 }, { -551, 594, 4014 } }, { 914, 1449, -144 } }, 225 },
    { { { { 4058, 0, 556 }, { 116, 4004, -852 }, { -543, 860, 3967 } }, { 919, 1520, -2227 } }, 225 },
    { { { { -3951, 0, -1077 }, { -517, 3593, 1896 }, { 945, 1965, -3467 } }, { 1340, 1600, -1902 } }, 257 },
    { { { { 1700, 0, 3726 }, { 1226, 3867, -559 }, { -3518, 1348, 1605 } }, { 667, 1600, -4048 } }, 257 },
};

GpSprtCmd D_neo_ark_eve_access_tunnel_8017F17C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

/// Hides or shows sprite commands of the area's views through their
/// `GpSprtCmd::field_4`: `arg0` 0 drives command 4 of view 2, `arg0` 1 command
/// 3 of view 3 and command 2 of view 4. `arg1` 0 hides them and 1 shows them;
/// any other value changes nothing.
void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    GpSprtCmd* view;
    s32        run = arg0 & 0xFF;
    s32        flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view            = rec[2].field_4;
            view[4].field_4 = 1;
            return;
        }
        if (flag == 1) {
            view            = rec[2].field_4;
            view[4].field_4 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view            = rec[3].field_4;
            view[3].field_4 = run;
            view            = rec[4].field_4;
            view[2].field_4 = run;
            return;
        }
        if (flag == run) {
            view            = rec[3].field_4;
            view[3].field_4 = 0;
            view            = rec[4].field_4;
            view[2].field_4 = 0;
        }
    }
}

/// Draws whichever emitters the current view shows: two adjacent positions per
/// drawn wedge, stepping through the view's run. View 4 chains into view 5's
/// emitters (`D_..._8017EB08` then `D_..._8017EB28`); every other view stops at
/// its own.
void func_neo_ark_eve_access_tunnel_8017E15C(Task* unused)
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
/// projected through `gGfxViewCoord.workm`, and unless the GTE flags either
/// projection, gouraud `POLY_G4` wedges are queued around each end and a band
/// joins them, each tinted at the centre and black at the rim. `arg1` is the
/// radius in world units, scaled by each point's depth; `arg2` is the tint as
/// three 4-bit channels (red at bit 8, green at bit 4, blue at bit 0), with 8
/// added to each on odd display frames so the glow flickers.
static void func_neo_ark_eve_access_tunnel_8017E244(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
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
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
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
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}
