#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b2_breeding_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

#define D_shelter_b2_breeding_room_80180470 (D_shelter_b2_breeding_room_80180450 + 4)
#define D_shelter_b2_breeding_room_80180480 (D_shelter_b2_breeding_room_80180450 + 6)
#define D_shelter_b2_breeding_room_801804C0 (D_shelter_b2_breeding_room_80180450 + 14)

static void func_shelter_b2_breeding_room_8017DB90(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_breeding_room_8017E3D4(SVECTOR* arg0, s32 arg1, s32 arg2);

// Indexed views below share one contiguous table.

SVECTOR D_shelter_b2_breeding_room_80180450[32] = {
    { 3450, -2450, -2000, 0 },
    { 3000, -2450, -2620, 0 },
    { 880, -2450, -2740, 0 },
    { 2410, -2450, -2740, 0 },
    { -260, -2240, -1360, 0 },
    { -260, -2240, -660, 0 },
    { 7470, -2160, 5560, 0 },
    { 8200, -2160, 5560, 0 },
    { 8990, -2160, 5560, 0 },
    { 9720, -2160, 5560, 0 },
    { 12130, -1430, 6540, 0 },
    { 12480, -1430, 6540, 0 },
    { 1410, -2350, 90, 0 },
    { 1410, -2350, 1890, 0 },
    { 1410, -2350, 3140, 0 },
    { 1410, -2350, 4760, 0 },
    { 2650, -2350, 90, 0 },
    { 2650, -2350, 1890, 0 },
    { 2650, -2350, 3140, 0 },
    { 2650, -2350, 4760, 0 },
    { 4340, -2350, 3830, 0 },
    { 4340, -2350, 5060, 0 },
    { 6780, -2350, 3830, 0 },
    { 6780, -2350, 5060, 0 },
    { 9190, -2350, 3830, 0 },
    { 9190, -2350, 5060, 0 },
    { 11510, -2350, 3830, 0 },
    { 11510, -2350, 5060, 0 },
    { 1210, -2350, -1890, 0 },
    { 2450, -2350, -1890, 0 },
    { 1400, -2350, 620, 0 },
    { 2650, -2350, 620, 0 },
};

/// Room light task. Its first frame sets the effect ids in `D_80115734`,
/// `D_80115730` and `D_80115754`; every frame it draws the glows of the lights
/// the current camera view shows, from the room's light position tables.
void func_shelter_b2_breeding_room_8017D898(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6027C;
        D_80115730  = 0x6027D;
        D_80115754  = 0x6027E;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180470[0], 0x100, 0x142);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180470[24], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180470[25], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180470[26], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180470[27], 0x200, 0x444);
            break;
        case 3:
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[0], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[1], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180450[2], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[12], 0x200, 0x333);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[13], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[16], 0x200, 0x333);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180450[17], 0x200, 0x444);
            break;
        case 4:
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_801804C0[0], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_801804C0[1], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_801804C0[4], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_801804C0[5], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_801804C0[7], 0x200, 0x444);
            break;
        case 5:
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[8], 0x200, 0x111);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[9], 0x200, 0x111);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[12], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[13], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[14], 0x200, 0x333);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[15], 0x200, 0x333);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            break;
        case 6:
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180480[2], 0x100, 0x444);
            func_shelter_b2_breeding_room_8017DB90(&D_shelter_b2_breeding_room_80180480[4], 0x100, 0x142);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[18], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[19], 0x200, 0x222);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[20], 0x200, 0x111);
            func_shelter_b2_breeding_room_8017E3D4(&D_shelter_b2_breeding_room_80180480[21], 0x200, 0x111);
            break;
    }
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
static void func_shelter_b2_breeding_room_8017DB90(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
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
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc around the point `arg0`, projected through
/// `gGfxViewCoord.workm`, unless the projection flags an error: four gouraud
/// wedges lit at the projected centre and black at the rim, of screen radius
/// `arg1 * 64 / otz`. `arg2` is the colour as three 4-bit channels (0xRGB),
/// brightened slightly on odd frames.
static void func_shelter_b2_breeding_room_8017E3D4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw31Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomDraw31Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw31Scratch);
}
