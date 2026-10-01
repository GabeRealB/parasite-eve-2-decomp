#include "pe/lifedrain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/glow_draw.h"

/// Per-level band row. `field_2` is the starting inner radius (also the per-frame
/// inner/outer step); `field_4` is the starting outer radius; `unk6` is the wedge
/// radius `func_lifedrain_8012FAF8` copies into `EffectWork.angle`. Indexed by
/// `(Gp_StateC08.field_0 % 10) - 1`.
typedef struct LifeDrainScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
} LifeDrainScale;
STATIC_ASSERT_SIZEOF(LifeDrainScale, 0xA);

static void func_lifedrain_801301AC(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Per-level tuning for the life drain: rows are PE levels 1-3.
static LifeDrainScale D_lifedrain_80130AB4[] = {
    { 0x0008, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 0x000C, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 0x0010, 0x00E0, 0x0300, 0x0600, 0x0050 },
};

/// Sound-script id of the drain's opening cue, indexed by `EffectWork.index`
/// when the cast has drained nothing yet and by `field_20 + 3` once there is
/// health banked in `Gp_StateF0.field_14`.
static s32 D_lifedrain_80130AD4[] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

/// One yaw per funnel wedge, `LifeDrainScale.unk0` of them, re-rolled as a
/// block when the cast starts and replayed every frame by
/// `glowDrawWedge`.
static s16 D_lifedrain_80130AEC[16] = { 0 };
/// The cast's collector task, published by `func_lifedrain_8012EF48`. Every
/// drain mote reparents itself onto it and adds its own `spawnArg1` to the
/// running total there.
static struct Task* D_lifedrain_80130B0C = NULL;

/// Runs one frame of the life-drain cast: a five-state machine driven by
/// `Task::state`, published in `D_lifedrain_80130B0C` so every mote can find
/// it. Cancelling (`Gp_StateC08.field_3 == -2` or `gRoomEffectState->peEffectControl >= 4`) releases
/// the work block, and states 0 and 1 first cash the banked `Gp_StateF0.field_14` into
/// `gPlayerStatus.hp`, clamped to the max in `field_1a`.
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
    EffectWork* mem;
    GfxCoord*   coord;
    s32         i;
    u8          rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.field_3 == -2) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        if ((arg0->state < 2) && (arg0->spawnArg1.value != 0)) {
            gPlayerStatus.hp = (u16)gPlayerStatus.hp + Gp_StateF0.field_14;
            if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                gPlayerStatus.hp = gPlayerStatus.hpMax;
            }
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0: {
            EffectWork* spawned;
            GpMtxWords* rot;

            D_lifedrain_80130B0C = arg0;
            rot                  = (GpMtxWords*)&coord->coord;
            coord->parent        = mem->parent;
            rot->m00_m01         = 0x1000;
            rot->m02_m10         = 0;
            rot->m11_m12         = 0x1000;
            rot->m20_m21         = 0;
            rot->m22             = 0x1000;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->composeStamp  = GRAPHICS_COORD_DIRTY;
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

                    rng                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_lifedrain_80130AEC[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    gRandomLcgState         = rng;
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
                mem->scale = mem->scale - 0x10;
                rgb[0]     = mem->scale >> 1;
                rgb[1]     = mem->scale >> 1;
                rgb[2]     = (u8)mem->scale;
                Gp_DrawFadeQuad(rgb, 1);
            }
            if (mem->age == 0x1E) {
                if (arg0->spawnArg1.value != 0) {
                    gPlayerStatus.hp = (u16)gPlayerStatus.hp + Gp_StateF0.field_14;
                    if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                        gPlayerStatus.hp = gPlayerStatus.hpMax;
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
            if (arg0->spawnArg1.value != 0) {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index + 3],
                                    (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index],
                                    (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            return;
        case 2: {
            LifeDrainScale* t2;
            EffectWork*     spawned;
            s16*            p;
            s32             val;

            Gp_UpdateCoord(coord);
            if (mem->period != 0) {
                mem->period = mem->period - 0x10;
                rgb[0]      = mem->period >> 1;
                rgb[1]      = mem->period >> 1;
                rgb[2]      = (u8)mem->period;
                Gp_DrawFadeQuad(rgb, 1);
            }
            val = mem->scale;
            if (val < (s16)D_lifedrain_80130AB4[mem->index].field_2) {
                val += 0x10;
            }
            mem->scale = val;
            mem->angle = mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            rgb[0]     = mem->scale >> 1;
            rgb[1]     = mem->scale >> 1;
            rgb[2]     = (u8)mem->scale;
            i          = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    glowDrawWedge(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)(mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if (mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)(mem->angle + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->step       = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & 0xFFF, 0);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            mem->move.vx = (rcos(mem->step) * mem->angle) >> 12;
            mem->move.vy = (rsin(mem->step) * mem->angle) >> 12;
            mem->move.vz = 0;
            spawned      = Gp_SpawnEff(0x600AD, coord, (s32)(D_lifedrain_80130AB4[mem->index].unk6),
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
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            if (mem->scale < 0x11) {
                arg0->state = 4;
            }
            rgb[0] = mem->scale >> 1;
            rgb[1] = mem->scale >> 1;
            rgb[2] = (u8)mem->scale;
            i      = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    glowDrawWedge(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)(mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if (mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)(mem->angle + D_lifedrain_80130AB4[2].field_4),
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
    EffectWork* mem;
    GfxCoord*   coord;
    s32         y;
    s32         state;
    s16         step;
    u16         spawn;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    state    = arg0->state;
    switch (state) {
        case 0:
            mem->move.vy    = 4;
            mem->move.vx    = 0;
            mem->move.vz    = 0;
            arg0->state     = 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
            spawn           = (u16)arg0->spawnArg1.value;
            mem->period     = 0x1000;
            mem->angle      = spawn & 0xFFF;
            return;
        case 1:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            Gp_UpdateCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            if (mem->index < 8) {
                if (mem->age & 1) {
                    Gp_DrawFxQuad(coord, mem->index, mem->angle,
                                  mem->scale | mem->period);
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
/// (`gRoomEffectState->peEffectControl >= 4`).
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
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   player;
    EffectWork* spawned;
    s16         val;
    s32         cur;
    VECTOR      vec;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.field_3 != -2) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_lifedrain_80130B0C, arg0);
                D_lifedrain_80130B0C->spawnArg1.value += arg0->spawnArg1.value;
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx                           = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy                           = 0xFFE0 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz                           = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                arg0->state                            = 1;
                mem->scale                             = (Gp_StateC08.field_0 % 10) - 1;
                val                                    = D_lifedrain_80130AB4[mem->step].unk6;
                mem->angle                             = val;
                mem->period                            = val - 0x100;
                /* fallthrough */
            case 1:
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, (s32)(mem->angle), NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                if (mem->age == 0xF) {
                    player = &(gameGetPtrSlot(3))->extra.tmd->coords[1];
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

                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (mem->age >= 0x1E) {
                    break;
                }
                if (mem->age & 1) {
                    mem->index = (mem->index + 1) & 3;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, (s32)(mem->angle), NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                player = &(gameGetPtrSlot(3))->extra.tmd->coords[1];
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
static void func_lifedrain_801301AC(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    s32            u0;
    s32            u1;
    s16            x;
    s16            y;
    u16            vz;

    head                                    = SCRATCH_STACK_CURSOR(u8);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(GpRingScratch)     = block;
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
        prim           = gGpuPrimCursor;
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
        block->step = (arg2 * 0x17) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        prim           = gGpuPrimCursor;
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
        block->step = ((s16)((arg2 * 2) / 3) * 0x37) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

#include "../../shared/glow_draw_wedge.inc.c"

void func_lifedrain_801308C0(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         kind;
    u16         val;
    u8          rgb[3];
    s32         scale;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->peEffectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1.value & 0xFFF, 0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        kind                = (Gp_StateC08.field_0 % 10U) - 1;
        mem->index          = kind;
        val                 = D_lifedrain_80130AB4[kind].field_2;
        mem->angle          = 0x80;
        mem->scale          = val;
        mem->period         = D_lifedrain_80130AB4[mem->index].field_4;
        arg0->state         = 1;
    }

    Gp_UpdateCoord(coord);
    mem->angle  = mem->angle + ((s16)D_lifedrain_80130AB4[mem->index].field_2 / 3);
    mem->period = mem->period + ((s16)D_lifedrain_80130AB4[mem->index].field_2 >> 1);
    rgb[0]      = mem->scale >> 1;
    rgb[1]      = mem->scale >> 1;
    rgb[2]      = (u8)mem->scale;
    Gp_DrawBandEx(coord, mem->angle, mem->period, rgb);

    scale      = (u16)mem->scale;
    scale     -= 8;
    mem->scale = scale;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
