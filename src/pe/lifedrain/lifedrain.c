#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "pe/lifedrain.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Per-level tuning for the life drain: rows are PE levels 1-3.
LifeDrainScale D_lifedrain_80130AB4[] = {
    { 0x0008, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 0x000C, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 0x0010, 0x00E0, 0x0300, 0x0600, 0x0050 },
};

/// The `SndEvt_EnqueueType6` ids: three drain cues then three release cues.
s32 D_lifedrain_80130AD4[] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

extern s8 D_80114C0B;

/// Scratch for the drain ribbon, plus the task handle it spawns.
s16          D_lifedrain_80130AEC[16] = { 0 };
struct Task* D_lifedrain_80130B0C     = NULL;

/// Runs one frame of the life-drain cast: a five-state machine driven by
/// `Task::state`, published in `D_lifedrain_80130B0C` so every mote can find
/// it. Cancelling (`D_80114C0B == -2` or `Gp_State1C->fadeState >= 4`) releases
/// the work block, and states 0 and 1 first cash the banked `Gp_StateF0.field_14` into
/// `Player_Status.hp`, clamped to the max in `field_1a`.
///
/// State 0 parents the effect coordinate at the origin with an identity
/// rotation, seeds the combo level `index` from `Gp_StateC08.field_0`, takes
/// the funnel radii `scale` / `period` from that row of
/// `D_lifedrain_80130AB4`, rolls one yaw per wedge into `D_lifedrain_80130AEC`
/// and spawns the three `0x600EA` motes 0x2AA apart around the circle. State 1
/// fades the entry quad out 0x10 a frame, plays the row's cue on tick 3 and on
/// tick 0x1E either banks the drain and moves to state 2 or, with nothing
/// banked, skips straight to the state-4 release.
///
/// State 2 is the funnel proper: it grows `scale` towards the row's
/// `field_2` cap, steps `angle` by `unk8`, redraws the wedges, the two rings
/// and the arcs, and each frame throws one `0x600AD` spark on an LCG yaw at
/// `angle` radius. Once `angle` passes the row's `unk6` it moves to state
/// 3, which shrinks `scale` by 0x10 a frame and redraws the same funnel
/// until it drops below 0x11, then releases through state 4.
void func_lifedrain_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((D_80114C0B == -2) || (Gp_State1C->fadeState >= 4)) {
        if ((arg0->state < 2) && (arg0->spawnArg1 != 0)) {
            Player_Status.hp = (u16)Player_Status.hp + Gp_StateF0.field_14;
            if (Player_Status.hp > Player_Status.hpMax) {
                Player_Status.hp = Player_Status.hpMax;
            }
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age = (u16)mem->age + 1;
    switch (arg0->state) {
        case 0: {
            GpEffWork*  spawned;
            GpMtxWords* rot;

            D_lifedrain_80130B0C = arg0;
            rot                  = (GpMtxWords*)&coord->coord;
            coord->sub           = mem->parent;
            rot->w0              = 0x1000;
            rot->w1              = 0;
            rot->w2              = 0x1000;
            rot->w3              = 0;
            rot->h4              = 0x1000;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->flg           = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            mem->index  = (Gp_StateC08.field_0 % 10) - 1;
            mem->scale  = D_lifedrain_80130AB4[mem->index].field_2;
            mem->angle  = 0x80;
            mem->period = D_lifedrain_80130AB4[mem->index].field_2;
            i           = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                do {
                    s32 rng;

                    rng                     = Gp_LcgState * 5 + 0x71357911;
                    D_lifedrain_80130AEC[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    Gp_LcgState             = rng;
                } while (++i < D_lifedrain_80130AB4[mem->index].unk0);
            }
            i = 0;
            do {
                spawned = Gp_SpawnEff(0x600EA, coord, i, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            Gp_StateC08.field_6 |= 8;
            return;
        }
        case 1:
            if (mem->scale != 0) {
                mem->scale = (u16)mem->scale - 0x10;
                rgb[0]     = (u16)mem->scale >> 1;
                rgb[1]     = (u16)mem->scale >> 1;
                rgb[2]     = *(u8*)&mem->scale;
                Gp_DrawFadeQuad(rgb, 1);
            }
            if (mem->age == 0x1E) {
                if (arg0->spawnArg1 != 0) {
                    Player_Status.hp = (u16)Player_Status.hp + Gp_StateF0.field_14;
                    if (Player_Status.hp > Player_Status.hpMax) {
                        Player_Status.hp = Player_Status.hpMax;
                    }
                    arg0->state = 2;
                } else {
                    arg0->state = 4;
                }
                return;
            }
            if (mem->age != 3) {
                return;
            }
            Gp_UpdateCoord(coord);
            if (arg0->spawnArg1 != 0) {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index + 3],
                                    (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index],
                                    (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            return;
        case 2: {
            LifeDrainScale* t2;
            GpEffWork*      spawned;
            s16*            p;
            s32             val;

            Gp_UpdateCoord(coord);
            if (mem->period != 0) {
                mem->period = (u16)mem->period - 0x10;
                rgb[0]      = (u16)mem->period >> 1;
                rgb[1]      = (u16)mem->period >> 1;
                rgb[2]      = *(u8*)&mem->period;
                Gp_DrawFadeQuad(rgb, 1);
            }
            val = mem->scale;
            if (val < (s16)D_lifedrain_80130AB4[mem->index].field_2) {
                val += 0x10;
            }
            mem->scale = val;
            mem->angle = (u16)mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            rgb[0]     = (u16)mem->scale >> 1;
            rgb[1]     = (u16)mem->scale >> 1;
            rgb[2]     = *(u8*)&mem->scale;
            i          = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    func_lifedrain_801305C0(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if ((u16)mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if ((u16)mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->angle + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->step   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 0);
            gte_SetRotMatrix((MATRIX*)&coord->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            mem->move.vx = (rcos(mem->step) * mem->angle) >> 12;
            mem->move.vy = (rsin(mem->step) * mem->angle) >> 12;
            mem->move.vz = 0;
            spawned      = Gp_SpawnEff(0x600AD, coord, D_lifedrain_80130AB4[mem->index].unk6,
                                       &mem->move);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->task);
            }
            if (mem->angle >= D_lifedrain_80130AB4[mem->index].unk6) {
                arg0->state = 3;
            }
            return;
        }
        case 3: {
            LifeDrainScale* t2;
            s16*            p;

            Gp_UpdateCoord(coord);
            mem->scale = (u16)mem->scale - 0x10;
            mem->angle = (u16)mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            if (mem->scale < 0x11) {
                arg0->state = 4;
            }
            rgb[0] = (u16)mem->scale >> 1;
            rgb[1] = (u16)mem->scale >> 1;
            rgb[2] = *(u8*)&mem->scale;
            i      = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    func_lifedrain_801305C0(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if ((u16)mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if ((u16)mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->angle + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            return;
        }
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

/// Spark billboard, identical to Healing's `func_healing_8012F494` and, like it,
/// spawned through gameplay's effect table. State 0 seeds the spin and colour
/// from the spawn argument and the LCG; state 1 lifts the frame and draws the
/// additive quad on odd ticks until the animation runs out.
void func_lifedrain_8012F9A8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s32            state;
    s16            step;
    u16            spawn;

    mem      = arg0->spawnArg2;
    coord    = ((TmdObject*)arg0->extra)->coords;
    mem->age = (u16)mem->age + 1;
    state    = arg0->state;
    switch (state) {
        case 0:
            mem->move.vy = 4;
            mem->move.vx = 0;
            mem->move.vz = 0;
            arg0->state  = 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            spawn        = (u16)arg0->spawnArg1;
            mem->period  = 0x1000;
            mem->angle   = spawn & 0xFFF;
            return;
        case 1:
            step              = mem->move.vy;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->age & 1)) {
                mem->index = (u16)mem->index + 1;
            }
            if (mem->index < 8) {
                if ((u16)mem->age & 1) {
                    Gp_DrawFxQuad(coord, (u16)mem->index, mem->angle,
                                  (u16)mem->scale | (u16)mem->period);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
    }
}

/// Runs one frame of a life-drain mote. Any state releases the work block once
/// the player is dying (`Gp_StateC08.field_3 == -2`) or the room is fading
/// (`Gp_State1C->fadeState >= 4`).
///
/// State 0 reparents the mote onto the cast's collector task
/// `D_lifedrain_80130B0C`, hands it this task's `spawnArg1`, and draws a random
/// drift out of three LCG steps: `move` / `move.vz` in `0x40 - [0, 0x7F]`
/// and `move.vy` in `0xFFE0 - [0, 0x3F]`, so the mote starts moving up and
/// away. `scale` is the combo level and `angle` the wedge radius from
/// `D_lifedrain_80130AB4`, `period` trailing it by `0x100`.
///
/// State 1 walks the coordinate by that drift and, every other tick, draws a
/// wedge through `func_lifedrain_801301AC` and one time in four parents a
/// `0x600AD` spark. On tick 0xF it aims: the player's part-1 translation minus
/// its own, rotated into the mote's frame by `workm` and by `coord`, becomes
/// the unit heading in `pos`, scaled by GPF with `0x1200 / (0x1E - tick)`
/// so later ticks pull harder. State 2 then steps each drift component 0x10
/// toward that heading every frame, re-aiming as it goes, and releases at tick
/// 0x1E.
void func_lifedrain_8012FAF8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GpEffWork*     spawned;
    s16            val;
    s32            cur;
    VECTOR         vec;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((Gp_StateC08.field_3 != -2) && (Gp_State1C->fadeState < 4)) {
        mem->age = (u16)mem->age + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_lifedrain_80130B0C, arg0);
                D_lifedrain_80130B0C->spawnArg1 += arg0->spawnArg1;
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx                     = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy                     = 0xFFE0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz                     = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                arg0->state                      = 1;
                mem->scale                       = (Gp_StateC08.field_0 % 10) - 1;
                val                              = D_lifedrain_80130AB4[mem->step].unk6;
                mem->angle                       = val;
                mem->period                      = val - 0x100;
                /* fallthrough */
            case 1:
                coord->coord.t[0] += mem->move.vx;
                coord->coord.t[1] += mem->move.vy;
                coord->coord.t[2] += mem->move.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->age & 1) {
                    mem->index = (u16)mem->index + 1;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->angle, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                if (mem->age == 0xF) {
                    player = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1];
                    vec.vx = player->workm.t[0] - coord->workm.t[0];
                    vec.vy = player->workm.t[1] - coord->workm.t[1];
                    vec.vz = player->workm.t[2] - coord->workm.t[2];
                    ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                    mem->pos.vx = vec.vx;
                    mem->pos.vy = vec.vy;
                    mem->pos.vz = vec.vz;
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->pos);
                    gte_rtv0();
                    gte_stsv(&mem->pos);
                    gte_lddp(0x1200 / (0x1E - mem->age));
                    gte_ldsv(&mem->pos);
                    gte_gpf12();
                    gte_stsv(&mem->pos);
                    arg0->state = 2;
                }
                return;
            case 2:
                cur          = mem->move.vx;
                mem->move.vx = (cur < mem->pos.vx) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vy;
                mem->move.vy = (cur < mem->pos.vy) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vz;
                mem->move.vz = (cur < mem->pos.vz) ? cur + 0x10 : cur - 0x10;

                coord->coord.t[0] += mem->move.vx;
                coord->coord.t[1] += mem->move.vy;
                coord->coord.t[2] += mem->move.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->age >= 0x1E) {
                    break;
                }
                if (mem->age & 1) {
                    mem->index = ((u16)mem->index + 1) & 3;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->angle, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                player = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1];
                vec.vx = player->workm.t[0] - coord->workm.t[0];
                vec.vy = player->workm.t[1] - coord->workm.t[1];
                vec.vz = player->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                mem->pos.vx = vec.vx;
                mem->pos.vy = vec.vy;
                mem->pos.vz = vec.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->pos);
                gte_rtv0();
                gte_stsv(&mem->pos);
                gte_lddp(0x1200 / (0x1E - mem->age));
                gte_ldsv(&mem->pos);
                gte_gpf12();
                gte_stsv(&mem->pos);
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Links two axis-aligned `POLY_FT4`s at `arg0`'s world position: the position
/// is projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner sprite is one of
/// four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by `arg1 & 3`, sized
/// `arg2 * 23 / otz`. The outer sprite is the 0x38-wide cell on tpage 0x29 whose
/// CLUT is `0x4310 + (arg1 & 1)`, sized `((arg2 * 2) / 3) * 55 / otz`. Same
/// 0x18-byte scratch and axis-aligned corners as gameplay `Gp_EffSprTask8D`.
void func_lifedrain_801301AC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s16              x;
    s16              y;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpEffFt4Scratch*)(head - 0x18);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x2A;
        prim->clut     = 0x42C5;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        u0          = (arg1 & 3) * 0x18;
        u1          = u0 + 0x17;
        prim->u1    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->u3    = u1;
        prim->v2    = 0x17;
        prim->v3    = 0x17;
        prim->v0    = 0;
        prim->v1    = 0;
        block->size = (arg2 * 0x17) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->u0    = 0x38;
        prim->v0    = 0xC8;
        prim->u1    = 0x6F;
        prim->v1    = 0xC8;
        prim->v2    = 0xFF;
        prim->v3    = 0xFF;
        prim->u2    = 0x38;
        prim->u3    = 0x6F;
        block->size = ((s16)((arg2 * 2) / 3) * 0x37) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws one wedge of the drain funnel as a Gouraud triangle. `arg0`'s origin
/// is projected once through `GsWSMATRIX`; the two outer corners sit `arg1`
/// screen units away at `arg2 - 0x20` and `arg2 + 0x20`, so the wedge is a
/// 0x40-wide fan blade about `arg2`. Only the apex carries `rgb`, the rim
/// fading to black. A negative `gte_stflg` drops the wedge.
void func_lifedrain_801305C0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    SVECTOR*       vec;
    POLY_G3*       prim;
    s32            ang;
    s32            ang2;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->step = ((s16)arg1 * 128) / block->otz;
        ang         = (s16)arg2;
        ang2        = ang - 0x20;
        prim->x0    = *(u16*)&block->sx;
        prim->y0    = *(u16*)&block->sy;
        prim->x1    = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
        prim->y1    = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
        ang        += 0x20;
        prim->x2    = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
        prim->y2    = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_lifedrain_801308C0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            kind;
    u16            val;
    u8             rgb[3];
    s32            scale;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->fadeState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg  = 0;
        kind        = (Gp_StateC08.field_0 % 10U) - 1;
        mem->index  = kind;
        val         = D_lifedrain_80130AB4[kind].field_2;
        mem->angle  = 0x80;
        mem->scale  = val;
        mem->period = D_lifedrain_80130AB4[mem->index].field_4;
        arg0->state = 1;
    }

    Gp_UpdateCoord(coord);
    mem->angle  = (u16)mem->angle + ((s16)D_lifedrain_80130AB4[mem->index].field_2 / 3);
    mem->period = (u16)mem->period + ((s16)D_lifedrain_80130AB4[mem->index].field_2 >> 1);
    rgb[0]      = (u16)mem->scale >> 1;
    rgb[1]      = (u16)mem->scale >> 1;
    rgb[2]      = *(u8*)&mem->scale;
    Gp_DrawBandEx(coord, mem->angle, mem->period, rgb);

    scale      = (u16)mem->scale;
    scale     -= 8;
    mem->scale = scale;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
