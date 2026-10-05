#include "pe/energyball.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/sprite_quad.h"
#include "../../shared/ground_glow.h"

/// Size and growth rate of one energy ball at one Parasite Energy level.
///
/// A ball selects its row by the level digit of the spell being cast
/// (`AttachmentState::attachId % 10 - 1`), so a higher level charges a larger
/// ball at a faster rate.
///
/// Both members are in the units of the ball's size, which the ball keeps in
/// `EffectWork::angle`: its glow sprite, ring and ground glow are drawn from
/// that size and its collision sphere takes half of it as its radius.
typedef struct {
    s16 fullSize; // Size the ball charges to from zero before it is launched; a ball that hits bursts from this size to twice it
    s16 sizeStep; // Size gained per frame while charging and bursting, and lost per frame by a ball fading out, which ends below one step; also the distance the ball rises per frame while it charges, and the upward speed it first steers toward once launched
} _EnergyballLevelTuning;
STATIC_ASSERT_SIZEOF(_EnergyballLevelTuning, 4);

/// Collision block of one energy ball: the sphere it strikes with and the one
/// contact that sphere can hold.
///
/// The ball's task allocates the block zeroed when it spawns and keeps it in
/// `Task::work`, so the task's teardown frees it. The sphere is armed only once
/// the ball is fully charged: it then follows the ball's coordinate with half
/// the ball's size as its radius and is linked on collision list 1 for pair
/// tests. Its packed key has contact category 2 and an identity counted from
/// 0x8000 by the digits of the spell being cast. An occupied contact of
/// category 3 bursts the ball. The sphere is unlinked before the ball bursts,
/// fades or is cancelled, which is harmless for a block that was never linked.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1 while the ball flies; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; a contact that does not burst the ball is cleared the frame it is found
} _EnergyballBody;
STATIC_ASSERT_SIZEOF(_EnergyballBody, 0x38);

static void func_energyball_8012FFD0(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_energyball_80130B54(GfxCoord* arg0, s16 arg1, s16 arg2);

/// The energy ball's sound-script ids. Only the first three are read, indexed by
/// the cast's level: the cast starts its entry with `sndEvtRequestScriptStart` and
/// later passes the same id to `sndEvtRequestScriptStop`.
static s32 D_energyball_8013117C[] = {
    0xE02B0002,
    0xE02E0002,
    0xE0310002,
    0xE02B0001,
    0xE02E0001,
    0xE0310001,
};

/// Per-level size tuning for the ball, one row per PE level 1-3, weakest
/// first.
static _EnergyballLevelTuning D_energyball_80131194[] = {
    { 0x0400, 0x0040 },
    { 0x0480, 0x0048 },
    { 0x0500, 0x0050 },
};

/// Sixteen 8-bit draws from `gRandomLcgState`, refilled once per cast by
/// `func_energyball_8012EF48` and consumed by the GTE pass in
/// `func_energyball_80130B54` as the per-vertex jitter of the ball's surface.
static s16 D_energyball_801311A0[16];

/// Fires the energy ball: on the first frame it picks the charge level from the
/// combo counter, plays the matching loop sound, refills the surface-jitter
/// table and spawns one ball per charge level, fanning them out by 0x555 of
/// yaw each while `gEnergyBallInFlightCount` (the number of balls already in flight) allows
/// it. Every later frame just releases the work block.

void func_energyball_8012EF48(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         i;
    s32         level;
    s32         rng;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->index = Gp_StateC08.attachId % 10 - 1;
            level      = mem->index;
            mem->angle = (level << 8) + 0x300;
            if (gEnergyBallInFlightCount < 0) {
                gEnergyBallInFlightCount = 0;
            }
            if (gEnergyBallInFlightCount == 0) {
                sndEvtRequestScriptStart(D_energyball_8013117C[mem->index], 0, 0);
            }
            for (i = 0; i < 0x10; i++) {
                rng                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_energyball_801311A0[i] = ((u32)rng >> 16) & 0xFF;
                gRandomLcgState          = rng;
            }
            for (i = 0; i < mem->index + 1; i++) {
                if (gEnergyBallInFlightCount + i >= 3) {
                    break;
                }
                mem->scale   = i * 0x555 - mem->index * 0x2AA;
                mem->move.vx = (mem->angle * rsin(mem->scale)) >> 12;
                mem->move.vz = (mem->angle * rcos(mem->scale)) >> 12;
                Gp_SpawnEff((EFFECT_ENERGY_BALL | EFFECT_SPAWN_UNLIMITED), coord, i, &mem->move);
            }
            arg0->state = 1;
            return;
        case 1:
            effectKillTask(mem, arg0);
            return;
    }
}

/// One ball of the energy ball cast; `spawnArg1` (0-2) selects shared transient
/// light slot `4 + spawnArg1` in `gWorldCoordTransientPointLights`, and
/// `spawnArg2` selects the `EffectWork` block. With nonzero
/// `gRoomEffectState->peEffectControl` it only redraws; cancellation at 4 or more
/// drops the ball. Otherwise it walks `Task::state`: 0 allocates the
/// `_EnergyballBody` collision block, picks the row of
/// `D_energyball_80131194` from the spell's level digit and seeds a random spin
/// `period`; 1 grows the ball by the row's `sizeStep` per frame until it
/// reaches `fullSize`, then links it on list 1 with a random direction; 2
/// flies it, re-aiming at the player every eighth frame and nudging each
/// velocity component by 0x10 on odd frames, bursting into three 0x600F9
/// effects on a hit (`Gp_CountRec18Hi`) or unlinking when the room's
/// `field_16` drops; 3 and 4 fade the burst out, growing to twice the row's
/// size or shrinking below one step. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD` or
/// PE effect control at 4 or more) anywhere but combo 0x2B lets the ball go: the last
/// ball in flight (`gEnergyBallInFlightCount`) queues the row's stop sound.
void func_energyball_8012F180(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    _EnergyballBody*               work;
    WorldCoordTransientPointLight* slot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    GfxCoord                       ground;
    VECTOR                         vec;
    GfxCoord*                      player;
    EffectWork*                    spawned;
    SVECTOR*                       dir;
    u16                            r;
    s32*                           snd;
    s16                            peEffectControl;
    s32                            cur;

    slot            = &gWorldCoordTransientPointLights[arg0->spawnArg1.value + 4];
    lightCoord      = &slot->light.head.transform.coord;
    pointLight      = &slot->light;
    coord           = arg0->extra.coordBody->coord;
    peEffectControl = gRoomEffectState->peEffectControl;
    work            = arg0->work;
    mem             = arg0->spawnArg2.pointer;
    if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (gEnergyBallInFlightCount > 0) {
                gEnergyBallInFlightCount -= 1;
                if (gEnergyBallInFlightCount == 0) {
                    sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                }
            }
            if (arg0->state != 0) {
                worldCollisionUnlinkBody(&work->body);
            }
            goto release;
        }
        actorRenderComposeCoord(coord);
        spriteQuadDrawFlicker(coord, mem->age, mem->angle, mem->period);
        func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
        if ((arg0->state < 3) && (gRoomEffectState->groundTraceEnabled != 0) &&
            (Gp_TraceGroundCoord(coord, &ground) == 1)) {
            groundGlowDraw(&ground, mem->angle);
        }
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            work = memCalloc(sizeof(_EnergyballBody), 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            arg0->work                = work;
            mem->index                = (Gp_StateC08.attachId % 10) - 1;
            mem->move.vx              = 0;
            gRandomLcgState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy              = -D_energyball_80131194[mem->index].sizeStep;
            mem->move.vz              = 0;
            mem->angle                = 0;
            mem->period               = (gRandomLcgState >> 16) & 0xFFF;
            gEnergyBallInFlightCount += 1;
            mem->scale                = 0xC0;
            mem->step                 = 0x20;
            arg0->state               = 1;
            /* fallthrough */
        case 1:
            if (mem->angle < D_energyball_80131194[mem->index].fullSize) {
                mem->angle          = mem->angle + D_energyball_80131194[mem->index].sizeStep;
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
            } else {
                actorRenderComposeCoord(coord);
                arg0->work                  = work;
                work->body.context.contacts = work->contacts;
                work->body.coord            = coord;
                work->body.key              = ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 +
                                 ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 +
                                 (u16)(Gp_StateC08.attachId % 10) + 0x28000;
                work->body.radius = mem->angle >> 1;
                work->body.flags  = WORLD_COLLISION_BODY_SPHERE;
                worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->body);
                dir                     = &mem->move;
                work->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
                work->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                arg0->state             = 2;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx            = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy            = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz            = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
                VectorNormalSS(dir, dir);
                gte_lddp(mem->step);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                mem->pos.vx = 0;
                mem->pos.vy = -D_energyball_80131194[mem->index].sizeStep;
                mem->pos.vz = 0;
            }
            slot->framesLeft         = 2;
            pointLight->inner        = 0x100;
            pointLight->outer        = 0x1000;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            r                        = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            pointLight->head.color.g = r;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> 1;
            pointLight->head.color.b = pointLight->head.color.g >> 1;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            spriteQuadDrawFlicker(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            if ((gRoomEffectState->groundTraceEnabled != 0) && (Gp_TraceGroundCoord(coord, &ground) == 1)) {
                groundGlowDraw(&ground, mem->angle);
            }
            coord->workm.t[1] += D_energyball_80131194[mem->index].sizeStep * mem->age;
            func_energyball_80130B54(coord, mem->angle,
                                     (D_energyball_80131194[mem->index].fullSize - mem->angle) / 5);
            coord->workm.t[1] -= D_energyball_80131194[mem->index].sizeStep * mem->age;
            if ((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        }
                    }
                    worldCollisionUnlinkBody(&work->body);
                    effectKillTask(mem, arg0);
                    return;
                }
            }
            return;
        case 2:
            if ((mem->age & 7) == 0) {
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
                gte_lddp(mem->index * 0x180 + 0xA00);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
            }
            if (mem->age & 1) {
                cur          = mem->move.vx;
                mem->move.vx = (cur < mem->pos.vx) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vy;
                mem->move.vy = (cur < mem->pos.vy) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vz;
                mem->move.vz = (cur < mem->pos.vz) ? cur + 0x10 : cur - 0x10;
            }
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            slot->framesLeft         = 2;
            pointLight->inner        = 0x100;
            pointLight->outer        = 0x1000;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            r                        = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            pointLight->head.color.g = r;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> 1;
            pointLight->head.color.b = pointLight->head.color.g >> 1;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            spriteQuadDrawFlicker(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    groundGlowDraw(&ground, mem->angle);
                }
            }
            if ((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        }
                    }
                    worldCollisionUnlinkBody(&work->body);
                    effectKillTask(mem, arg0);
                    return;
                }
            }
            if (Gp_CountRec18Hi(work->body.context.contacts, 0x30000) != 0) {
                spawned = Gp_SpawnEff(EFFECT_ENERGYBALL_IMPACT_RING, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                spawned = Gp_SpawnEff(EFFECT_ENERGYBALL_IMPACT_RING, coord, 0x2AA, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                spawned = Gp_SpawnEff(EFFECT_ENERGYBALL_IMPACT_RING, coord, 0x555, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                snd = D_energyball_8013117C;
                sndEvtRequestScriptStart(snd[mem->index + 3], 0, 0);
                worldCollisionUnlinkBody(&work->body);
                mem->angle  = D_energyball_80131194[mem->index].fullSize;
                arg0->state = 3;
                return;
            }
            if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                worldCollisionUnlinkBody(&work->body);
                arg0->state = 4;
                return;
            }
            worldCollisionClearContacts(work->contacts);
            return;
        case 3:
            actorRenderComposeCoord(coord);
            spriteQuadDrawFlicker(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->angle * 2, mem->scale >> 2);
            mem->angle = mem->angle + D_energyball_80131194[mem->index].sizeStep;
            if (((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(mem, arg0);
                return;
            }
            if (D_energyball_80131194[mem->index].fullSize * 2 < mem->angle) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(mem, arg0);
                return;
            }
            return;
        case 4:
            actorRenderComposeCoord(coord);
            spriteQuadDrawFlicker(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->angle * 2, mem->scale >> 2);
            mem->angle = mem->angle - D_energyball_80131194[mem->index].sizeStep;
            if (((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(mem, arg0);
                return;
            }
            if (mem->angle < D_energyball_80131194[mem->index].sizeStep) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[mem->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(mem, arg0);
                return;
            }
            return;
        default:
            return;
    }
release:
    effectKillTask(mem, arg0);
}

/// Overlay copy of `Gp_DrawRing` with a flat tint: draws an eight-segment
/// gouraud ring centred on `arg0`'s world position. The position is projected
/// through `GsWSMATRIX` by one `RTPS` and the ring is dropped when that sets a
/// negative `gte_stflg`. `arg1` is the radius in world units (scaled by 64 and
/// divided by the projected OTZ) and `arg2` the brightness: only the inner
/// vertex of each `POLY_G4` is lit, `(arg2 / 2, arg2, arg2 / 2)`, so every
/// wedge fades from green at the centre to black at the rim. Each wedge gets
/// the semi-transparent tpage of `gpuSetPrimitiveBlendMode` at its OTZ.
static void func_energyball_8012FFD0(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectCentreScratch* block;
    POLY_G4*             prim;
    s32                  ang;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->screenExtent = (arg1 * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2 >> 1, arg2, arg2 >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#define SPRITE_QUAD_SCALE 55
#define SPRITE_QUAD_ODD_LOOK(p) \
    setSemiTrans(p, 1);         \
    setShadeTex(p, 1);          \
    SPRITE_QUAD_CORE_CELL(p)
#define SPRITE_QUAD_EVEN_LOOK(p)  \
    setRGB0(p, 0x40, 0xC0, 0x60); \
    SPRITE_QUAD_RIM_CELL(p);      \
    setSemiTrans(p, 1)
#include "../../shared/sprite_quad_draw_flicker.inc.c"

#define GROUND_GLOW_R    0x20
#define GROUND_GLOW_G    0x30
#define GROUND_GLOW_B    0x20
#define GROUND_GLOW_CLUT 0x428C
#include "../../shared/ground_glow_draw.inc.c"

/// Draws the energy ball's surface: two 16-vertex rings of the same radius
/// sit `arg1 * 2` apart in `arg0`'s local Y, are rotated by its `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by the jitter table
/// `D_energyball_801311A0` plus the frame counter, the quad is tinted
/// `(arg2 >> 1, arg2, arg2 >> 1)`, and a negative `gte_stflg` drops the
/// segment.
static void func_energyball_80130B54(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_FT4*          prim;
    s32                i;
    s32                next;
    s32                ang;
    s32                u;
    s16                idx;

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (u32)(rsin(ang) * 3) >> 5;
        block->topRing[i].vy = -(arg1 * 2);
        block->topRing[i].vz = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx   += arg0->workm.t[0];
        block->topRing[i].vy   += arg0->workm.t[1];
        block->topRing[i].vz   += arg0->workm.t[2];
        block->bottomRing[i].vx = (u32)(rsin(ang) * 3) >> 5;
        op                      = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx += arg0->workm.t[0];
        op->vy                  += arg0->workm.t[1];
        op->vz                  += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        idx = (u32)(D_energyball_801311A0[i] + gDisplayState.animFrame) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setRGB0(prim, arg2 >> 1, arg2, arg2 >> 1);
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            setSemiTrans(prim, 1);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

void func_energyball_8013107C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    u8          rgb[3];
    s32         scale;
    s32         angle;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->peEffectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    if (arg0->state == 0) {
        gfxRotMatrixZ(&coord->coord, arg0->spawnArg1.value & 0xFFF, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->scale          = 0x80;
        mem->angle          = 0x100;
        arg0->state         = 1;
    }

    actorRenderComposeCoord(coord);
    rgb[0] = mem->scale >> 1;
    rgb[1] = (u8)mem->scale;
    rgb[2] = mem->scale >> 1;
    Gp_DrawBandEx(coord, mem->angle, 0x180, rgb);

    angle      = (u16)mem->angle;
    scale      = (u16)mem->scale;
    angle     += 0x80;
    scale     -= 8;
    mem->scale = scale;
    mem->angle = angle;
    if ((s16)scale < 9) {
        effectKillTask(mem, arg0);
    }
}
