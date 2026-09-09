#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "decomp/common.h"
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

/// `rtps`. The `inline_c.h` macro of that name assembles
/// to a different word, so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Ten spawn offsets for the roof garden's ambient effects, indexed 0..9 by the
/// task's first-frame burst below.
extern SVECTOR D_acropolis_roof_garden_80184BF8[10];

/// Grey level of each of the three sprite variants the ambient sprite task
/// below can be spawned as.
extern RgSpriteLevels D_acropolis_roof_garden_8017D5D0;

/// Per-variant mask of camera views the ambient sprite is visible from, indexed
/// by the low nibble of `Task::spawnArg1`.
extern u16 D_acropolis_roof_garden_80184C48[];

extern s32 D_80070F70;
extern s16 D_acropolis_roof_garden_80184C5C[];

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

/// Pulsating roof-garden flare, projected into scratch memory and drawn with
/// Gouraud polygons and optional rays.
void func_acropolis_roof_garden_8017E29C(Task* arg0)
{
    GsCOORDINATE2*  coord;
    void*           mem;
    u8*             head;
    u8*             raw;
    RgFlareScratch* blk;
    POLY_G4*        prim;
    LINE_G3*        line;
    s32             i;
    s32             pulse;
    s32             level;
    s32             h;
    s16             lvl;
    s16             flip;
    u16             vz;
    s32             z;
    u32             tag;
    u_long*         ot;
    u8              red;
    s32             shift;
    u32             depth;

    coord = ((TmdObject*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x18;
    SOFT_TOUCH_REG(raw);
    blk                     = (RgFlareScratch*)raw;
    blk->vec.vx             = *(u16*)&coord->workm.t[0];
    blk->vec.vy             = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec.vz             = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RgFlareScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    gte_stsxy(&((RgFlareScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((RgFlareScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = D_80070F70;
        pulse *= arg0->spawnArg1 & 0xFF;
        flip   = (arg0->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level  = ~pulse;
            level &= 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        lvl   = level * 2;
        level = arg0->spawnArg1;
        if (level < 0) {
            h           = (level >> 8) & 0xFF;
            blk->radius = (h << 10) / blk->otz;
            blk->inner  = (h << 7) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (lvl * (flip ^ 1)) >> 1, (flip * lvl) >> 1, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i]) >> 12);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl * (flip ^ 1), flip * lvl, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            {
                s32 half = lvl >> 1;
                for (i = 2; i < 0x10; i += 8) {
                    do {
                        prim           = (POLY_G4*)Gpu_PrimCursor;
                        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        red = half * (flip ^ 1);
                        setRGB2(prim, red, flip * half, 0);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = blk->sx + ((blk->inner * D_acropolis_roof_garden_80184C5C[i]) >> 12);
                        prim->y0 = blk->sy + ((blk->inner * D_acropolis_roof_garden_80184C5C[i - 4]) >> 12);
                        prim->x1 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 4]) >> 11);
                        prim->y1 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i]) >> 11);
                        prim->x2 = blk->sx;
                        prim->y2 = blk->sy;
                        prim->x3 = blk->sx + ((blk->inner * D_acropolis_roof_garden_80184C5C[i + 8]) >> 12);
                        prim->y3 = blk->sy + ((blk->inner * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                        shift    = Display_State.field_128;
                        depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                        // Keep the shift and its source live through the first OT address.
                        __asm__("" : "+r"(depth) : "r"(shift), "m"(Display_State.field_128));
                        setaddr(prim, getaddr((u_long*)(depth + (s32)Gpu_CurrentOt)));
                        ot  = (u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt);
                        tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                        *ot = tag;
                        z   = blk->otz;
                        SOFT_TOUCH_REG(z);
                        SOFT_TOUCH_REG(z);
                        SOFT_TOUCH_REG_USE(z, tag);
                        SOFT_TOUCH_REG_USE(prim, z);
                        Gp_AddTpageShift((P_TAG*)prim, 1, z);

                        prim           = (POLY_G4*)Gpu_PrimCursor;
                        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, red, flip * half, 0);
                    } while (0);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->inner * D_acropolis_roof_garden_80184C5C[i + 4]) >> 13);
                    prim->y0 = blk->sy + ((blk->inner * D_acropolis_roof_garden_80184C5C[i]) >> 13);
                    prim->x1 = blk->sx + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 8]) >> 12);
                    prim->y1 = blk->sy + ((blk->radius * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->inner * D_acropolis_roof_garden_80184C5C[i + 12]) >> 13);
                    prim->y3 = blk->sy + ((blk->inner * D_acropolis_roof_garden_80184C5C[i + 8]) >> 13);
                    addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                            prim);
                    z = blk->otz;
                    __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_roof_garden_80184C5C[i]));
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);
                }
            }
        } else {
            blk->radius = (((level >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl * (flip ^ 1), flip * lvl, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->radius;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->radius;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->radius) + blk->radius * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (arg0->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, lvl * (flip ^ 1), flip * lvl, 0);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->radius * (i * 3 - 1);
                    line->y0 = blk->sy - blk->radius * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->radius * (i * 3 - 1);
                    line->y2 = blk->sy + blk->radius * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) +
                                      (s32)Gpu_CurrentOt),
                            line);
                    Gp_AddTpageShift((P_TAG*)line, 1, blk->otz);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    Gp_ReleaseState1CMem(mem, arg0);
}
