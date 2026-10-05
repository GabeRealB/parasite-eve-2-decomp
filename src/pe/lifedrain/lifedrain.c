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
#include "gameplay/scene_combat.h"
#include "gameplay/world_coords.h"

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
#include "../../shared/rising_spark.h"

/// Visual tuning of the life drain cast for one Parasite Energy level.
///
/// The cast draws a funnel around the caster: a fan of glow wedges, two rings
/// and one or more gradient rings that all grow with one radius, shedding a
/// spark each frame, while three bands expand from the centre. The cast task
/// and each band select their row with the level digit of the attachment id,
/// less one. A mote selects its row with an `EffectWork` halfword it never
/// writes, so it reads the first row at every level.
///
/// Radii and widths are world units ahead of the perspective divide.
/// `brightness` is a blue channel value; red and green are half of it.
typedef struct {
    s16 wedgeCount;  // Glow wedges fanned around the funnel; the yaw table holds 16
    s16 brightness;  // Brightness the opening flash fades from and the funnel rises back to; a band starts at it and grows its inner radius by a third of it and its width by half of it each frame
    s16 outerOffset; // Width a band starts with; from level 2, also how far outside the funnel's gradient ring a second, dimmer one is drawn
    s16 radiusLimit; // Funnel radius that ends the growth, and the radius of each spark the funnel sheds; a mote's sparks take it too, and its own sprite is sized 0x100 less
    s16 radiusStep;  // Funnel radius gained per frame, while it grows and while it fades
} _LifedrainLevelTuning;
STATIC_ASSERT_SIZEOF(_LifedrainLevelTuning, 0xA);

static void func_lifedrain_801301AC(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Per-level tuning for the life drain, one row per PE level 1-3, weakest
/// first.
static _LifedrainLevelTuning D_lifedrain_80130AB4[] = {
    { 0x0008, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 0x000C, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 0x0010, 0x00E0, 0x0300, 0x0600, 0x0050 },
};

/// Sound-script id of the drain's opening cue, indexed by `EffectWork.index`
/// when the cast has drained nothing yet and by `field_20 + 3` once there is
/// health banked in `gSceneCombatState.lifeDrainHp`.
static s32 D_lifedrain_80130AD4[] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

/// One yaw per funnel wedge, `_LifedrainLevelTuning::wedgeCount` of them, re-rolled as a
/// block when the cast starts and replayed every frame by
/// `glowDrawWedge`.
static s16 D_lifedrain_80130AEC[16] = { 0 };
/// The cast's collector task, published by `func_lifedrain_8012EF48`. Every
/// drain mote reparents itself onto it and adds its own `spawnArg1` to the
/// running total there.
static struct Task* D_lifedrain_80130B0C = NULL;

/// Runs one frame of the life-drain cast: a five-state machine driven by
/// `Task::state`, published in `D_lifedrain_80130B0C` so every mote can find
/// it. Cancelling (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD` or `gRoomEffectState->peEffectControl >= 4`) releases
/// the work block, and states 0 and 1 first cash the banked `gSceneCombatState.lifeDrainHp` into
/// `gPlayerStatus.hp`, clamped to the max in `field_1a`.
///
/// State 0 parents the effect coordinate at the origin with an identity
/// rotation, seeds the combo level `index` from `Gp_StateC08.attachId`, sets
/// both flash levels `scale` / `period` to that row's `brightness` in
/// `D_lifedrain_80130AB4`, rolls one yaw per wedge into `D_lifedrain_80130AEC`
/// and spawns the three `0x600EA` motes 0x2AA apart around the circle. State 1
/// fades the entry quad out 0x10 a frame, plays the row's cue on tick 3 and on
/// tick 0x1E either banks the drain and moves to state 2 or, with nothing
/// banked, skips straight to the state-4 release.
///
/// State 2 is the funnel proper: it grows `scale` towards the row's
/// `brightness`, steps `angle` by `radiusStep`, redraws the wedges, the two rings
/// and the arcs, and each frame throws one `0x600AD` spark on an LCG yaw at
/// `angle` radius. Once `angle` reaches the row's `radiusLimit` it moves to state
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
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        if ((arg0->state < 2) && (arg0->spawnArg1.value != 0)) {
            gPlayerStatus.hp = (u16)gPlayerStatus.hp + gSceneCombatState.lifeDrainHp;
            if (gPlayerStatus.hp > gPlayerStatus.hpMax) {
                gPlayerStatus.hp = gPlayerStatus.hpMax;
            }
        }
        effectKillTask(mem, arg0);
        return;
    }
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0: {
            EffectWork*       spawned;
            GfxRotationWords* rot;

            D_lifedrain_80130B0C = arg0;
            rot                  = (GfxRotationWords*)&coord->coord;
            coord->parent        = mem->parent;
            rot->m00M01          = ONE;
            rot->m02M10          = 0;
            rot->m11M12          = ONE;
            rot->m20M21          = 0;
            rot->m22             = ONE;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->composeStamp  = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            mem->index  = (Gp_StateC08.attachId % 10) - 1;
            mem->scale  = D_lifedrain_80130AB4[mem->index].brightness;
            mem->angle  = 0x80;
            mem->period = D_lifedrain_80130AB4[mem->index].brightness;
            i           = 0;
            if (D_lifedrain_80130AB4[mem->index].wedgeCount > 0) {
                do {
                    s32 rng;

                    rng                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_lifedrain_80130AEC[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    gRandomLcgState         = rng;
                } while (++i < D_lifedrain_80130AB4[mem->index].wedgeCount);
            }
            i = 0;
            do {
                spawned = Gp_SpawnEff(EFFECT_LIFEDRAIN_RING, coord, i, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            return;
        }
        case 1:
            if (mem->scale != 0) {
                mem->scale = mem->scale - 0x10;
                rgb[0]     = mem->scale >> 1;
                rgb[1]     = mem->scale >> 1;
                rgb[2]     = (u8)mem->scale;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            if (mem->age == 0x1E) {
                if (arg0->spawnArg1.value != 0) {
                    gPlayerStatus.hp = (u16)gPlayerStatus.hp + gSceneCombatState.lifeDrainHp;
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
            actorRenderComposeCoord(coord);
            if (arg0->spawnArg1.value != 0) {
                sndEvtRequestScriptStart(D_lifedrain_80130AD4[mem->index + 3],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestScriptStart(D_lifedrain_80130AD4[mem->index],
                                         (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            return;
        case 2: {
            _LifedrainLevelTuning* tuning;
            EffectWork*            spawned;
            s16*                   p;
            s32                    val;

            actorRenderComposeCoord(coord);
            if (mem->period != 0) {
                mem->period = mem->period - 0x10;
                rgb[0]      = mem->period >> 1;
                rgb[1]      = mem->period >> 1;
                rgb[2]      = (u8)mem->period;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            }
            val = mem->scale;
            if (val < D_lifedrain_80130AB4[mem->index].brightness) {
                val += 0x10;
            }
            mem->scale = val;
            mem->angle = mem->angle + D_lifedrain_80130AB4[mem->index].radiusStep;
            rgb[0]     = mem->scale >> 1;
            rgb[1]     = mem->scale >> 1;
            rgb[2]     = (u8)mem->scale;
            i          = 0;
            if (D_lifedrain_80130AB4[mem->index].wedgeCount > 0) {
                tuning = D_lifedrain_80130AB4;
                p      = D_lifedrain_80130AEC;
                do {
                    glowDrawWedge(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < tuning[mem->index].wedgeCount);
            }
            effectDrawGouraudDisc(coord, mem->angle >> 1, rgb);
            effectDrawGouraudDisc(coord, mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            effectDrawOuterGlowBand(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                effectDrawOuterGlowBand(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                effectDrawOuterGlowBand(coord,
                                        (s16)(mem->angle + D_lifedrain_80130AB4[mem->index].outerOffset),
                                        0x80, rgb);
                if (mem->index == 2) {
                    if (mem->age & 1) {
                        effectDrawOuterGlowBand(coord, 0x80,
                                                (s16)(mem->angle + D_lifedrain_80130AB4[2].outerOffset),
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
            spawned      = Gp_SpawnEff(EFFECT_LIFEDRAIN_SPARK, coord, (s32)D_lifedrain_80130AB4[mem->index].radiusLimit,
                                       &mem->move);
            if (spawned != NULL) {
                taskReparent(arg0, spawned->task);
            }
            if (mem->angle >= D_lifedrain_80130AB4[mem->index].radiusLimit) {
                arg0->state = 3;
            }
            return;
        }
        case 3: {
            _LifedrainLevelTuning* tuning;
            s16*                   p;

            actorRenderComposeCoord(coord);
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + D_lifedrain_80130AB4[mem->index].radiusStep;
            if (mem->scale < 0x11) {
                arg0->state = 4;
            }
            rgb[0] = mem->scale >> 1;
            rgb[1] = mem->scale >> 1;
            rgb[2] = (u8)mem->scale;
            i      = 0;
            if (D_lifedrain_80130AB4[mem->index].wedgeCount > 0) {
                tuning = D_lifedrain_80130AB4;
                p      = D_lifedrain_80130AEC;
                do {
                    glowDrawWedge(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < tuning[mem->index].wedgeCount);
            }
            effectDrawGouraudDisc(coord, mem->angle >> 1, rgb);
            effectDrawGouraudDisc(coord, mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            effectDrawOuterGlowBand(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                effectDrawOuterGlowBand(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                effectDrawOuterGlowBand(coord,
                                        (s16)(mem->angle + D_lifedrain_80130AB4[mem->index].outerOffset),
                                        0x80, rgb);
                if (mem->index == 2) {
                    if (mem->age & 1) {
                        effectDrawOuterGlowBand(coord, 0x80,
                                                (s16)(mem->angle + D_lifedrain_80130AB4[2].outerOffset),
                                                rgb);
                    }
                }
            }
            return;
        }
        case 4:
            effectKillTask(mem, arg0);
            return;
    }
}

#include "../../shared/rising_spark_task.inc.c"

/// Life Drain's spark billboard (see rising_spark.h), spawned through
/// gameplay's effect table.
void func_lifedrain_8012F9A8(Task* task)
{
    _risingSparkTask(task);
}

/// Runs one frame of a life-drain mote. Any state releases the work block once
/// the player is dying (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD`) or the room is fading
/// (`gRoomEffectState->peEffectControl >= 4`).
///
/// State 0 reparents the mote onto the cast's collector task
/// `D_lifedrain_80130B0C`, hands it this task's `spawnArg1`, and draws a random
/// drift out of three LCG steps: `move` / `move.vz` in `0x40 - [0, 0x7F]`
/// and `move.vy` in `0xFFE0 - [0, 0x3F]`, so the mote starts moving up and
/// away. `scale` is the combo level and `angle` the spark radius, a
/// `radiusLimit` of `D_lifedrain_80130AB4`, `period` trailing it by `0x100`.
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
    s16         sparkRadius;
    s32         cur;
    VECTOR      vec;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                taskReparent(D_lifedrain_80130B0C, arg0);
                D_lifedrain_80130B0C->spawnArg1.value += arg0->spawnArg1.value;
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx                           = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy                           = 0xFFE0 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz                           = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                arg0->state                            = 1;
                mem->scale                             = (Gp_StateC08.attachId % 10) - 1;
                sparkRadius                            = D_lifedrain_80130AB4[mem->step].radiusLimit;
                mem->angle                             = sparkRadius;
                mem->period                            = sparkRadius - 0x100;
                /* fallthrough */
            case 1:
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(mem->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(arg0, spawned->task);
                        }
                    }
                }
                if (mem->age == 0xF) {
                    player = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];
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
                actorRenderComposeCoord(coord);
                if (mem->age >= 0x1E) {
                    break;
                }
                if (mem->age & 1) {
                    mem->index = (mem->index + 1) & 3;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(EFFECT_LIFEDRAIN_SPARK, coord, (s32)(mem->angle), NULL);
                        if (spawned != NULL) {
                            taskReparent(arg0, spawned->task);
                        }
                    }
                }
                player = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];
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
    effectKillTask(mem, arg0);
}

/// Links two axis-aligned `POLY_FT4`s at `arg0`'s world position: the position
/// is projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner sprite is one of
/// four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by `arg1 & 3`, sized
/// `arg2 * 23 / depth`. The outer sprite is the 0x38-wide cell on tpage 0x29 whose
/// CLUT is `0x4310 + (arg1 & 1)`, sized `((arg2 * 2) / 3) * 55 / depth`. Same
/// 0x18-byte scratch and axis-aligned corners as gameplay `effectSpriteTask8D`.
static void func_lifedrain_801301AC(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s32                  u0;
    s32                  u1;
    s16                  x;
    s16                  y;
    u16                  vz;

    head                                                                        = SCRATCH_STACK_CURSOR(u8);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)arg0->workm.t[1];
    vz                                                                          = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch)                                   = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x2A;
        prim->clut     = 0x42C5;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        u0                  = (arg1 & 3) * 0x18;
        u1                  = u0 + 0x17;
        prim->u1            = u1;
        prim->u0            = u0;
        prim->u2            = u0;
        prim->u3            = u1;
        prim->v2            = 0x17;
        prim->v3            = 0x17;
        prim->v0            = 0;
        prim->v1            = 0;
        block->screenExtent = (arg2 * 0x17) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->u0            = 0x38;
        prim->v0            = 0xC8;
        prim->u1            = 0x6F;
        prim->v1            = 0xC8;
        prim->v2            = 0xFF;
        prim->v3            = 0xFF;
        prim->u2            = 0x38;
        prim->u3            = 0x6F;
        block->screenExtent = ((s16)((arg2 * 2) / 3) * 0x37) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/glow_draw_wedge.inc.c"

void func_lifedrain_801308C0(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         kind;
    u8          rgb[3];
    s32         scale;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->peEffectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        gfxRotMatrixZ(&coord->coord, arg0->spawnArg1.value & 0xFFF, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        kind                = (Gp_StateC08.attachId % 10U) - 1;
        mem->index          = kind;
        mem->scale          = D_lifedrain_80130AB4[kind].brightness;
        mem->angle          = 0x80;
        mem->period         = D_lifedrain_80130AB4[mem->index].outerOffset;
        arg0->state         = 1;
    }

    actorRenderComposeCoord(coord);
    mem->angle  = mem->angle + (D_lifedrain_80130AB4[mem->index].brightness / 3);
    mem->period = mem->period + (D_lifedrain_80130AB4[mem->index].brightness >> 1);
    rgb[0]      = mem->scale >> 1;
    rgb[1]      = mem->scale >> 1;
    rgb[2]      = (u8)mem->scale;
    effectDrawInnerGlowBand(coord, mem->angle, mem->period, rgb);

    scale      = (u16)mem->scale;
    scale     -= 8;
    mem->scale = scale;
    if ((s16)scale < 9) {
        effectKillTask(mem, arg0);
    }
}
