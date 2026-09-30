#include "rooms/shelter_b2_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_north_maintenance_walkway_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"
#include "../../shared/room_visual_effects.h"

/// Anchor points of the glows the room task draws.
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[];

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.

/// Offsets from the anchor of the two points the smoke trail follows. The
/// second is also reached under its own name.

static void func_shelter_b2_north_maintenance_walkway_8017E0DC(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_north_maintenance_walkway_8017E858(SVECTOR* arg0, s16 arg1);
static void func_shelter_b2_north_maintenance_walkway_8017EBB4(SVECTOR* arg0, s32 arg1, s32 arg2);

TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48 = { 0, 32, func_shelter_b2_north_maintenance_walkway_8017D61C, { .model = NULL } };

TaskDesc D_shelter_b2_north_maintenance_walkway_80183B54 = { 0, 32, func_shelter_b2_north_maintenance_walkway_8017D918, { .model = NULL } };

GpMsgEntry D_shelter_b2_north_maintenance_walkway_80183B60[6] = {
    { 5102, func_shelter_b2_north_maintenance_walkway_8017DA88 },
    { 5105, func_shelter_b2_north_maintenance_walkway_8017DC44 },
    { 5103, func_shelter_b2_north_maintenance_walkway_8017DC54 },
    { 5104, func_shelter_b2_north_maintenance_walkway_8017DC4C },
    { 5106, func_shelter_b2_north_maintenance_walkway_8017DCE4 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[4] = {
    { 894, -197, -2271, 0 },
    { 894, -197, -3102, 0 },
    { 894, -197, 376, 0 },
    { 894, -197, -396, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[14] = {
    { 894, -197, 2648, 0 },
    { 894, -197, 2036, 0 },
    { 3106, -197, -2271, 0 },
    { 3106, -197, -3102, 0 },
    { 3106, -197, 376, 0 },
    { 3106, -197, -396, 0 },
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[1] = {
    { 749, -1283, 2310, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[1] = {
    { 1107, -1181, -4935, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[1] = {
    { 1107, -1125, -4900, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x9620 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

/// The room's per-frame glow task. Its first tick sets the gameplay effect ids
/// the room's effects use; every tick then draws the flares, discs and stars
/// visible from the current camera view. One star turns from red to blue once
/// flag 0xA8, the one the event gate writes for message 0x1D, is set.
void func_shelter_b2_north_maintenance_walkway_8017DDE8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024B;
        D_80115744  = 0x60257;
        D_8011573C  = 0x60262;
        D_80115720  = 0x6026E;
        D_80115758  = 0x601D2;
        D_8011572C  = 0x601EE;
        D_80115750  = 0x6020A;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA8) != 0) {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            p = D_shelter_b2_north_maintenance_walkway_80183B90;
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[0], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[6], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-18], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-16], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-12], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-10], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-8], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-8], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-6], 0x200, 0x800);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-2], 0x200, -0x400);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-2], 0x200, 0);
            break;
        }
        case 7:
            if (GameFlag_GetNibble(0xA8) != 0) {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            break;
        case 8:
            func_shelter_b2_north_maintenance_walkway_8017E0DC(D_shelter_b2_north_maintenance_walkway_80183BB0, 0x200, 0x400);
            break;
    }
}

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn
/// when the second point lies nearer than OTZ 0x11.
static void func_shelter_b2_north_maintenance_walkway_8017E0DC(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
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
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Projects `arg0` through `gGfxViewCoord.workm` and, when its OTZ is above 0x10,
/// queues four gouraud `POLY_G4` wedges forming a red disc around it, of radius
/// `arg1 * 64 / otz`. The centre's red level alternates between 0x20 and 0x28
/// on odd and even frames.
static void func_shelter_b2_north_maintenance_walkway_8017E858(SVECTOR* arg0, s16 arg1)
{
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                radius;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomDraw25Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        radius        = (arg1 * 64) / block->otz;
        rgb           = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        ang           = 0;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, 0, 0);
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
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw25Scratch);
}

/// Queues a tinted gouraud star at the projected point `arg0`: a disc of
/// radius `arg1` scaled by depth drawn at half and full brightness, plus four
/// spikes, two of them reaching twice the disc's radius. `arg2` packs the tint as four nibbles - a flicker
/// shift, then red, green and blue - and the frame counter's low bit, shifted
/// by the first nibble, is added to every channel. Nothing is drawn when the
/// projection overflows.
static void func_shelter_b2_north_maintenance_walkway_8017EBB4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                packed;
    s32                blend;
    s32                size;
    s32                otz;
    s32                rOuter;
    s32                rInner;
    s32                ang;
    s32                t;
    s32                t2;
    s32                r;
    s32                g;
    s32                b;
    s32                rh;
    s32                gh;
    s32                bh;

    {
        void** scratch;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        block   = (RoomDraw05Scratch*)(*scratch = head - 0x14);
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        size          = (s16)arg1;
        otz           = block->otz + 1;
        rOuter        = (size * 64) / otz;
        block->otz    = otz;
        ds            = &gDisplayState;
        blend         = ds->animFrame;
        block->rOuter = rOuter;
        rInner        = (size * 8) / block->otz;
        packed        = arg2 << 16;
        blend         = blend & 1;
        blend         = blend << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = rInner;
        ang           = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            rh = (u8)r >> 1;
            gh = (u8)g >> 1;
            bh = (u8)b >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rh, gh, bh);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang - 0x400)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ang - 0x400)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x400)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x400)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang + 0x400)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang + 0x400)) >> 11);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x800)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x800)) >> 12);
            ang     += 0x800;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
}

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b2_north_maintenance_walkway_8017F590(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b2_north_maintenance_walkway_801802D8(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b2_north_maintenance_walkway_80180670(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_north_maintenance_walkway_80181A80(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b2_north_maintenance_walkway_80181BB4(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b2_north_maintenance_walkway_80182618(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_north_maintenance_walkway_80182F00(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
