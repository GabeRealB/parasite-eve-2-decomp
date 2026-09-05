#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros
/// of those names assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;
extern s32 D_80115738;
extern s32 D_8011574C;

extern GpMsgEntry D_acropolis_bridge_801898FC[];
extern SVECTOR    D_acropolis_bridge_8018991C[7];
extern SVECTOR    D_acropolis_bridge_80189954[7];
extern SVECTOR    D_acropolis_bridge_8018998C[11];
extern SVECTOR    D_acropolis_bridge_801899E4;
extern u16        D_acropolis_bridge_801899EC[8];
extern u16        D_acropolis_bridge_801899FC[16];
extern u16        D_acropolis_bridge_80189A1C[11];
extern u16        D_acropolis_bridge_80189A32;
extern SVECTOR    D_acropolis_bridge_80189A34[2];
extern SVECTOR    D_acropolis_bridge_80189A44;
extern SVECTOR    D_acropolis_bridge_80189A4C;

extern void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
extern void func_acropolis_bridge_80182F8C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
extern void func_acropolis_bridge_801833A0(GsCOORDINATE2* arg0, u16 arg1, s16 arg2);

/// Per-frame driver for the bridge's ambient effect field, and the room's
/// message-table owner. On the first frame it publishes
/// `D_acropolis_bridge_801898FC` as slot 5's `Gp_DispatchMsg` table and seeds
/// `D_acropolis_bridge_80189A34` with the two tracked cable joints' world
/// positions.
///
/// Each frame it re-spawns the effects the current camera can see: the two
/// per-view bitmask tables (`D_acropolis_bridge_801899EC` /
/// `D_acropolis_bridge_80189A1C`) say which of the placed emitters are visible
/// from view `Gp_GetViewIndex()`, and each visible entry spawns its dust
/// (0x600B1 / 0x600B2) or spark (0x600B3) at the matching `SVECTOR`. View 9
/// lifts the dust 0x240 above the placed point.
///
/// On views 2, 5 and 6 (`bit & 0x62`) it also trails debris off the two moving
/// joints: `field_26` is the Manhattan distance the joint travelled since last
/// frame, biased by 0x20, and two `Gp_LcgState` rolls against that distance
/// decide whether this frame emits `D_8011574C` / `D_80115738`. The joint's
/// new position is written back for the next frame's delta.
///
/// `D_acropolis_bridge_801899FC` finally maps the view onto one of five
/// looping ambience effects (0x600B4..0x600B8): entering the view bursts 30
/// copies at once, staying in it emits one per frame, or one in two / one in
/// three while `Gp_State1C->field_16` says the scene is quiet.

/// Controller for one piece of the bridge's blown debris: it drifts the task's
/// coordinate frame by a velocity it rolls once, and hands the frame to
/// `func_acropolis_bridge_80182F8C` (state 1) or `func_acropolis_bridge_801833A0`
/// (state 2) to be drawn. Everything the piece needs is packed into
/// `Task::spawnArg1`: bits 0-11 become `field_24`, bits 12-15 the number of
/// ticks each animation step lasts (`field_28`, 1 if zero), bits 16-23 the
/// speed the velocity is scaled to (`field_2A`, 0x40 if zero), bits 24-27 the
/// launch pattern and bits 28-31 pick which of the two draw helpers runs. The
/// first tick also rolls the 12-bit `field_26` out of `Gp_LcgState`; both it
/// and `field_24` are passed to the draw helper every tick.
///
/// The launch pattern rolls `field_10` when the caller left it zero: 1 spreads
/// X and Z evenly over +/-0x80 and biases Y to -0x40..-0xBF, 2 spreads all
/// three evenly over +/-0x80, 3 keeps X and Z inside +/-0x10 and drives Y to
/// 0..-0xFF, 5 copies the velocity the caller staged at `field_18`, and 0 stops
/// the piece from drifting at all by zeroing `field_2A`. The rolled direction
/// is then normalized and scaled back up to `field_2A` with one GTE `GPF`, so
/// the pattern only picks a direction and the packed speed sets the length.
///
/// Once running, a piece with a non-zero `field_2A` adds its velocity onto the
/// coordinate's translation each tick and bends Y by 6 as it goes, and every
/// `field_28` ticks steps `field_20`; the eighth step releases the work block.
/// While `Gp_State1C::field_4` is set the room is fading out, so the piece only
/// keeps drawing, and releases itself once the fade reaches 4.
void func_acropolis_bridge_80182AF8(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        func_acropolis_bridge_80182F8C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_acropolis_bridge_80182F8C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_acropolis_bridge_801833A0(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws one piece of the bridge's blown debris as a screen-facing quad. The
/// piece's world position is copied out of `coord->workm.t` and projected
/// through `GsWSMATRIX` with a single `RTPS`; a GTE error (`gte_stflg` sign
/// bit) drops the piece rather than drawing it. The `POLY_FT4` is centred on
/// the projected point, its two diagonals `size * 31 / otz` long and turned by
/// `angle` and `angle + 0x400`, so the quad shrinks with distance and spins
/// with the piece. `frame` picks the animation cell: the texture window is the
/// 0x1F-wide column starting at `frame * 0x20` on rows 0xE0..0xFF of tpage
/// 0x2B. The primitive is semi-transparent with texture blending off
/// (`code |= 3`) and links into the OT at the projected depth.
void func_acropolis_bridge_80182F8C(GsCOORDINATE2* coord, u16 frame, s16 size, s16 angle)
{
    void**                        scratch;
    u8*                           head;
    AcropolisBridgeSpriteScratch* block;
    POLY_FT4*                     prim;
    s32                           ang;
    AcropolisBridgeSpriteScratch* depth;
    s32                           u;
    s32                           uu;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (AcropolisBridgeSpriteScratch*)(head - sizeof(AcropolisBridgeSpriteScratch));
    depth   = block;

    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&coord->workm.t[2];
    *scratch      = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->vec);
    gte_rtps_real();

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->sx);
    gte_stflg(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->flag);

    if (block->flag >= 0) {
        gte_stszotz(&depth->otz);
        ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u           = frame << 5;
        uu          = u + 0x1F;
        setUV4(prim, u, 0xE0, uu, 0xE0, u, 0xFF, uu, 0xFF);
        setcode(prim, getcode(prim) | 3);

        ang       = angle;
        block->dx = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rsin(ang)) >> 12;
        block->dy = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rcos(ang)) >> 12;
        prim->x0  = block->sx + *(u16*)&block->dx;
        prim->x3  = block->sx - *(u16*)&block->dx;
        prim->y0  = block->sy - *(u16*)&block->dy;
        prim->y3  = block->sy + *(u16*)&block->dy;

        ang      += 0x400;
        block->dx = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rsin(ang)) >> 12;
        block->dy = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rcos(ang)) >> 12;
        prim->x1  = block->sx + *(u16*)&block->dx;
        prim->x2  = block->sx - *(u16*)&block->dx;
        prim->y1  = block->sy - *(u16*)&block->dy;
        prim->y2  = block->sy + *(u16*)&block->dy;

        addPrim((u_long*)(((((u32)((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + sizeof(AcropolisBridgeSpriteScratch);
}

/// Draws one piece of the bridge's blown debris as an upright screen-facing
/// quad - the unrotated counterpart of `func_acropolis_bridge_80182F8C`. The
/// piece's world position is copied out of `coord->workm.t` and projected
/// through `GsWSMATRIX` with a single `RTPS`; a GTE error (`gte_stflg` sign
/// bit) drops the piece rather than drawing it. The `POLY_FT4` is centred on
/// the projected point and is `size * 55 / otz` wide, half that tall above the
/// centre and half below, so it shrinks with distance without ever spinning.
/// `frame` picks the animation cell out of a 4x2 grid of 0x38x0x38 cells on
/// tpage 0x2B: bits 0-1 pick the column and bit 2 the row. The primitive is
/// semi-transparent with texture blending off (`code |= 3`) and links into the
/// OT at the projected depth.
void func_acropolis_bridge_801833A0(GsCOORDINATE2* coord, u16 frame, s16 size)
{
    void**                        scratch;
    u8*                           head;
    AcropolisBridgeDebrisScratch* block;
    POLY_FT4*                     prim;
    AcropolisBridgeDebrisScratch* depth;
    u32                           cell;
    s32                           u;
    s32                           v;
    s8                            vTop;
    s8                            vBot;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (AcropolisBridgeDebrisScratch*)(head - sizeof(AcropolisBridgeDebrisScratch));
    depth   = block;

    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&coord->workm.t[2];
    *scratch      = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->vec);
    gte_rtps_real();

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->sx);
    gte_stflg(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->flag);

    if (block->flag >= 0) {
        gte_stszotz(&depth->otz);
        ((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = frame;
        u           = (cell & 3) * 0x38;
        v           = ((cell & 7) >> 2) * 0x38;
        vTop        = v + 0x70;
        vBot        = v + 0x70 + 0x37;
        setUV4(prim, u, vTop, u + 0x37, vTop, u, vBot, u + 0x37, vBot);
        setcode(prim, getcode(prim) | 3);

        block->d = size * 55 / ((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz;

        prim->x0 = prim->x2 = block->sx - *(u16*)&block->d;
        prim->x1 = prim->x3 = block->sx + *(u16*)&block->d;
        prim->y0 = prim->y1 = block->sy - *(u16*)&block->d - (block->d >> 1);
        prim->y2 = prim->y3 = block->sy + (block->d >> 1);

        addPrim((u_long*)(((((u32)((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + sizeof(AcropolisBridgeDebrisScratch);
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_10", D_acropolis_bridge_8017D6CC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_10", D_acropolis_bridge_8017D6E8);
