#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_roof_garden.h"
#include "rooms/room_common.h"

/// `rtps` / `rtpt` / `mvmva`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 Gp_LcgState;

void func_acropolis_roof_garden_8017F560(GsCOORDINATE2* coord, s32 arg1, s16 arg2);

/// Ten spawn offsets for the roof garden's ambient effects, indexed 0..9 by the
/// task's first-frame burst below.
extern SVECTOR D_acropolis_roof_garden_80184BF8[10];

/// Grey level of each of the three sprite variants the ambient sprite task
/// below can be spawned as.
extern RgSpriteLevels D_acropolis_roof_garden_8017D5D0;

/// Per-variant mask of camera views the ambient sprite is visible from, indexed
/// by the low nibble of `Task::spawnArg1`.
extern u16 D_acropolis_roof_garden_80184C48[];

/// Roof-garden ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_roof_garden_80184BF8` - two with a 0x02000000 flavour,
/// one flagged 0x04000102, then seven more - and every frame after that it adds
/// the two view-dependent effects: one while the current view is 5 or 6 (the
/// `0x30 >> view - 1` bit test) and one while it is 7.
void func_acropolis_roof_garden_8017DCDC(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            i;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x6008A, coord, i + 0x2000000, &D_acropolis_roof_garden_80184BF8[i]);
        }
        vec = D_acropolis_roof_garden_80184BF8;
        Gp_SpawnEff(0x6008A, coord, 0x4000102, &vec[2]);
        for (i = 3; i < 10; i++) {
            Gp_SpawnEff(0x6008A, coord, i + 0x200, &vec[i]);
        }
        task->state = task->state + 1;
    }
    if (Gp_State1C->field_4 < 4) {
        if ((0x30 >> ((u8)Game_Session->field_4 - 1)) & 1) {
            work->field_10.vx = -0x12A2;
            work->field_10.vy = -0xDC;
            work->field_10.vz = -0xF19;
            Gp_SpawnEff(0x60090, coord, 0x60E, &work->field_10);
        }
        if ((u8)Game_Session->field_4 == 7) {
            work->field_10.vx = -0x12A2;
            work->field_10.vy = -0xDC;
            work->field_10.vz = -0xF19;
            Gp_SpawnEff(0x60090, coord, 0x8000030E, &work->field_10);
        }
    }
}

/// One of the roof garden's ambient sprites. It is only drawn while the scene
/// is still on `Gp_State1C->field_4` 0 or 1 and the current camera view is one
/// the variant's mask in `D_acropolis_roof_garden_80184C48` allows; otherwise
/// the frame is skipped entirely.
///
/// When it does draw, the task's coordinate is refreshed and projected through
/// `GsWSMATRIX` into a 0x14-byte `RoomShaftScratch` block taken from
/// `G_SCRATCH_HEAD`, and the projected point becomes the centre of a
/// semi-transparent `POLY_FT4` on tpage 0x2B whose half-extent is
/// `field_24 * 0x27 / otz`, so the sprite shrinks with distance and is dropped
/// entirely inside `otz` 0x11. `Task::spawnArg1` is unpacked once, on the first
/// frame: bits 16..27 are the sprite's size (defaulting to 0x280 when zero),
/// bits 8..9 pick one of three 0x28x0x27 cells across the sheet -- and, through
/// `getClut`, the matching 16-colour palette -- and only the low nibble is
/// kept, as the index into the view mask. The grey level is the variant's own
/// level from `D_acropolis_roof_garden_8017D5D0`, brightened by 0x10 on odd
/// frames so the sprite flickers.
void func_acropolis_roof_garden_8017DE90(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    RoomShaftScratch* blk;
    POLY_FT4*         prim;
    RgSpriteLevels    base;
    s32               param;
    s32               lvl;
    s32               flicker;
    s16               x;
    s16               y;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 < 2) {
        if ((D_acropolis_roof_garden_80184C48[arg0->spawnArg1 & 0xF] >> ((u8)Game_Session->field_4 - 1)) & 1) {
            Gp_UpdateCoord(coord);
            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            *scratch = head - sizeof(RoomShaftScratch);
            blk      = (RoomShaftScratch*)(head - sizeof(RoomShaftScratch));
            if (arg0->state == 0) {
                base            = D_acropolis_roof_garden_8017D5D0;
                param           = arg0->spawnArg1;
                mem->field_24   = (param & 0x0FFF0000) ? ((param >> 16) & 0xFFF) : 0x280;
                mem->field_26   = (arg0->spawnArg1 >> 8) & 3;
                arg0->spawnArg1 = arg0->spawnArg1 & 0xF;
                mem->field_28   = base.v[mem->field_26];
                arg0->state++;
            }
            blk->vec.vx = *(u16*)&coord->workm.t[0];
            blk->vec.vy = *(u16*)&coord->workm.t[1];
            blk->vec.vz = *(u16*)&coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&blk->vec);
            gte_rtps_real();
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            gte_stsxy(&blk->sx);
            gte_stszotz(&blk->otz);
            if (blk->otz >= 0x11) {
                flicker     = ((u8)Display_State.field_8 & 1) * 0x10;
                lvl         = (u8)mem->field_28 + flicker;
                prim->tpage = 0x2B;
                setRGB0(prim, lvl, lvl, lvl);
                prim->code |= 2;
                prim->clut  = getClut(mem->field_26 * 0x10, 0x10E);
                prim->u0    = mem->field_26 * 0x28;
                prim->v0    = 0;
                prim->u1    = mem->field_26 * 0x28 + 0x27;
                prim->v1    = 0;
                prim->u2    = mem->field_26 * 0x28;
                prim->v2    = 0x27;
                prim->u3    = mem->field_26 * 0x28 + 0x27;
                prim->v3    = 0x27;

                blk->halfWidth = (mem->field_24 * 0x27) / blk->otz;
                x              = blk->sx - (u16)blk->halfWidth;
                prim->x2       = x;
                prim->x0       = x;
                x              = blk->sx + (u16)blk->halfWidth;
                prim->x3       = x;
                prim->x1       = x;
                y              = blk->sy - (u16)blk->halfWidth;
                prim->y1       = y;
                prim->y0       = y;
                y              = blk->sy + (u16)blk->halfWidth;
                prim->y3       = y;
                prim->y2       = y;
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
            }
            *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomShaftScratch);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017E29C);

/// One drifting mote of the roof garden's ambient effect. State 0 seeds the
/// mote from `Gp_LcgState`: a 0x20 brightness, a tilt pair (`field_28` /
/// `field_2A`) and a per-frame drift vector in `field_10`. State 1 flies it -
/// the drift is added to the coordinate's translation, the tilt drives
/// `Gfx_RotMatrixX` / `Gfx_RotMatrixZ`, and each axis of the drift walks back
/// towards zero one unit per frame, re-rolling to a fresh multiple of 8 once it
/// reaches it, so the mote wanders instead of settling. Once it has risen past
/// the origin (`t[1] > 0`) state 2 fades the mote in and state 3 fades it out,
/// releasing the work block when the ramp runs out.
void func_acropolis_roof_garden_8017F10C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            vy;
    s32            vx;
    s32            vz;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24    = 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_2A    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state       = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->field_10.vx;
            coord->coord.t[1] += work->field_10.vy;
            coord->coord.t[2] += work->field_10.vz;
            Gfx_RotMatrixX(&coord->coord, (s16)work->field_28, 0);
            Gfx_RotMatrixZ(&coord->coord, (s16)work->field_2A, 0);
            coord->flg = 0;

            vy = work->field_10.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->field_10.vy = vy;

            vx = work->field_10.vx;
            if (vx == 0) {
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->field_10.vx = vx;
            }

            vz = work->field_10.vz;
            if (vz == 0) {
                work->field_10.vz += (s16)work->field_2A % 32;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->field_10.vz = vz;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_28 += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_acropolis_roof_garden_8017F560(coord, (s16)work->field_24, 0);
            break;
        case 2:
            if ((s16)work->field_26 < 0x80) {
                work->field_26 += 0x10;
            } else {
                task->state = 3;
            }
            func_acropolis_roof_garden_8017F560(coord, (s16)work->field_24, 0);
            break;
        case 3:
            if ((s16)work->field_26 >= 0x11) {
                work->field_26 -= 0x10;
                func_acropolis_roof_garden_8017F560(coord, (s16)work->field_24, (s16)work->field_26);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one mote of the roof garden's ambient effect: the unit quad
/// `D_80111E38` scaled to `arg1` half-size, rotated by the mote's own
/// `GsCOORDINATE2` and then projected through `GsWSMATRIX` into a 0x24-byte
/// `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and the other
/// three through `rtpt`; motes inside `otz` 0x11 are dropped.
///
/// `arg2` is the fade level: zero draws the mote as a raw texture
/// (`setShadeTex`), otherwise it is the grey the quad is modulated by and the
/// quad is drawn semi-transparent, which is how the task's fade-out step
/// (`func_acropolis_roof_garden_8017F10C` state 3) dims it away.
void func_acropolis_roof_garden_8017F560(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    register GsCOORDINATE2* coord asm("t7");
    register void**         scratch asm("a0");
    u8*                     head;
    RoomQuadScratch*        blk;
    POLY_FT4*               prim;
    GpQuadCorner*           tbl;
    SVECTOR*                sv;
    MATRIX*                 wm;
    s32                     i;

    coord   = arg0;
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &coord->workm;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - sizeof(RoomQuadScratch);
    /* `head` and `blk` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the one the
       rest of the function uses. */
    SOFT_TOUCH_REG(head);
    blk      = (RoomQuadScratch*)head;
    *scratch = blk;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        *(u16*)&blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&sv->vz = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomQuadScratch);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017F870);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017FA14);

/// Item-pickup model task step: the item's mesh is only visible from views 5
/// through 7, and stays hidden once the item's 2-bit flag reads 2 (already
/// taken). The three hidden cases are written as separate tests so the two view
/// comparisons are not folded into one unsigned range check.
void func_acropolis_roof_garden_80180160(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;
    s32         view;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view >= 8) {
        tmd->field_C = 0x80;
    } else if (view < 5) {
        tmd->field_C = 0x80;
    } else if (flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}
