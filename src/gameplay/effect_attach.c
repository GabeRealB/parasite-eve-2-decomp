#include "gameplay/effect_tasks.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/random.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

/// Unreferenced nonzero tail; its original purpose is unknown.
extern u32 D_80114B7C;

/// Task bank 10; actors supply the last descriptor's model before spawning.
TaskDesc D_80114B34[6] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_TMD, 0x70 } }, effectBurstModelPartTask, { .model = NULL } },
};

/// Unreferenced nonzero tail; its original purpose is unknown.
u32 D_80114B7C = 0x323010CE;

void effectBurstModelPartTask(Task* task)
{
    enum {
        EFFECT_BURST_PART_INITIALIZE                = 0,
        EFFECT_BURST_PART_FLY                       = 1,
        EFFECT_BURST_PART_COLLAPSE                  = 2,
        EFFECT_BURST_PART_PUFF_SIZE_MASK            = 0xFFF,
        EFFECT_BURST_PART_DEFAULT_PUFF_SIZE         = 512,
        EFFECT_BURST_PART_LAUNCH_SPEED              = 256,
        EFFECT_BURST_PART_INITIAL_AMBIENT_Q12       = 2048,
        EFFECT_BURST_PART_LIGHT_COUNT               = 3,
        EFFECT_BURST_PART_LIGHT_UPDATE_MASK         = 3,
        EFFECT_BURST_PART_REPEAT_HIT_FRAMES         = 8,
        EFFECT_BURST_PART_COLLAPSE_SPEED_LIMIT      = 32,
        EFFECT_BURST_PART_FLIGHT_FRAMES             = 76,
        EFFECT_BURST_PART_DIM_START_FRAME           = 51,
        EFFECT_BURST_PART_LOW_ALTITUDE_MARGIN       = 256,
        EFFECT_BURST_PART_LOW_ALTITUDE_AGE_STEP     = 10,
        EFFECT_BURST_PART_GRAVITY_Q12               = 65536,
        EFFECT_BURST_PART_FLIGHT_AMBIENT_STEP_Q12   = 64,
        EFFECT_BURST_PART_COLLAPSE_AMBIENT_STEP_Q12 = 128,
        EFFECT_BURST_PART_COLLAPSE_FRAMES           = 16,
        EFFECT_BURST_PART_BURN_FRAME                = 8,
        EFFECT_BURST_PART_LARGE_BURN_PUFF_SIZE      = 256,
        EFFECT_BURST_PART_AIR_PUFF_ARG              = 0x11000,
        EFFECT_BURST_PART_IMPACT_PUFF_ARG           = 0x12200
    };
    SVECTOR     displacement;
    SVECTOR     probeEnd;
    SVECTOR     probeStartOrNormal;
    VECTOR      collapseScaleCopy;
    VECTOR      collapseScale;
    TmdObject*  model;
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoord;
    SVECTOR*    spin;
    MATRIX*     localMatrix;
    s32         state;
    s16         effectControl;
    s16         ambientLevel;
    s32         puffSize;

    /// Advances the burst part by a speed-scaled Q12 direction in parent axes.
    ///
    /// coord/work are side-effect-free live pointers evaluated repeatedly;
    /// displacement is a writable SVECTOR lvalue evaluated repeatedly. Retains
    /// GTE saturation, dirties the coordinate cache and captures no locals.
    /// Use as a standalone compound statement; undefined after this task.
#define EFFECT_STEP_BURST_MODEL_PART(coord, work, displacement) \
    {                                                           \
        gte_lddp((work)->scale);                                \
        gte_ldsv(&(work)->move);                                \
        gte_gpf12();                                            \
        gte_stsv(&(displacement));                              \
        (coord)->coord.t[0]  += (displacement).vx;              \
        (coord)->coord.t[1]  += (displacement).vy;              \
        (coord)->coord.t[2]  += (displacement).vz;              \
        (coord)->composeStamp = GRAPHICS_COORD_DIRTY;           \
    }

    model         = task->extra.tmd;
    work          = task->spawnArg2.pointer;
    coord         = model->coords;
    playerCoord   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    actorRenderComposeCoord(coord);
    work->age++;
    state = task->state;
    switch (state) {
        case EFFECT_BURST_PART_INITIALIZE:
            // move holds a normalized Q12 launch direction; pos becomes per-update spin.
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->scale   = EFFECT_BURST_PART_LAUNCH_SPEED;
            if (task->spawnArg1.value & EFFECT_BURST_PART_PUFF_SIZE_MASK) {
                puffSize = task->spawnArg1.halves.low & EFFECT_BURST_PART_PUFF_SIZE_MASK;
            } else {
                puffSize = EFFECT_BURST_PART_DEFAULT_PUFF_SIZE;
            }
            work->angle     = puffSize;
            work->period    = EFFECT_BURST_PART_INITIAL_AMBIENT_Q12;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x400 - ((gRandomLcgState >> 16) % 0xC00);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
            VectorNormalSS(&work->move, &work->move);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vx        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = EFFECT_BURST_PART_FLY;
            worldCoordSetModelLighting(model, coord->workm.t, 0, EFFECT_BURST_PART_LIGHT_COUNT);
            return;
        case EFFECT_BURST_PART_FLY:
            // Advance in parent axes, then probe the attempted segment in view space.
            localMatrix = &coord->coord;
            spin        = &work->pos;
            gfxRotMatrixXYZ(localMatrix, spin, GRAPHICS_ROTATION_COMPOSE);
            MatrixNormal(localMatrix, localMatrix);
            EFFECT_STEP_BURST_MODEL_PART(coord, work, displacement);
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&displacement);
            gte_rtv0();
            gte_stsv(&probeEnd);
            probeStartOrNormal.vx = coord->workm.t[0];
            probeStartOrNormal.vy = coord->workm.t[1];
            probeStartOrNormal.vz = coord->workm.t[2];
            probeEnd.vx          += probeStartOrNormal.vx;
            probeEnd.vy          += probeStartOrNormal.vy;
            probeEnd.vz          += probeStartOrNormal.vz;
            if (worldCollisionProbeGridSegment(&probeEnd, &probeStartOrNormal, &probeEnd, &probeStartOrNormal) == state) {
                coord->coord.t[0] -= displacement.vx;
                coord->coord.t[1] -= displacement.vy;
                coord->coord.t[2] -= displacement.vz;
                // The probe overwrites its start with the room-space Q12 surface normal.
                work->move.vx = (probeStartOrNormal.vx >> 1) + (work->move.vx >> 1);
                work->move.vy = probeStartOrNormal.vy + (work->move.vy >> 1);
                work->move.vz = (probeStartOrNormal.vz >> 1) + (work->move.vz >> 1);
                VectorNormalSS(&work->move, &work->move);
                work->scale >>= 1;
                EFFECT_STEP_BURST_MODEL_PART(coord, work, displacement);
                actorRenderComposeCoord(coord);
                if (!(work->age & EFFECT_BURST_PART_LIGHT_UPDATE_MASK)) {
                    worldCoordSetModelLighting(model, coord->workm.t, 0, EFFECT_BURST_PART_LIGHT_COUNT);
                }
                effectSpawn(EFFECT_HIT_PUFF, coord, work->angle + EFFECT_BURST_PART_IMPACT_PUFF_ARG, 0);
                gte_lddp(ONE / 2);
                gte_ldsv(spin);
                gte_gpf12();
                gte_stsv(spin);
                if ((work->age - work->step) < EFFECT_BURST_PART_REPEAT_HIT_FRAMES && work->scale < EFFECT_BURST_PART_COLLAPSE_SPEED_LIMIT) {
                    model->flags |= TMD_OBJECT_SEMI_TRANS;
                    work->age     = 0;
                    task->state   = EFFECT_BURST_PART_COLLAPSE;
                    return;
                }
                work->step = work->age;
                return;
            }
            if (work->scale == 0) {
                return;
            }
            if (work->age >= EFFECT_BURST_PART_FLIGHT_FRAMES) {
                break;
            }
            if (playerCoord->coord.t[1] + EFFECT_BURST_PART_LOW_ALTITUDE_MARGIN < coord->coord.t[1]) {
                work->age += EFFECT_BURST_PART_LOW_ALTITUDE_AGE_STEP;
            }
            actorRenderComposeCoord(coord);
            if (!(work->age & EFFECT_BURST_PART_LIGHT_UPDATE_MASK)) {
                worldCoordSetModelLighting(model, coord->workm.t, 0, EFFECT_BURST_PART_LIGHT_COUNT);
            }
            work->move.vy  += EFFECT_BURST_PART_GRAVITY_Q12 / work->scale;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (!((gRandomLcgState >> 16) & 3)) {
                effectSpawn(EFFECT_TRAIL_PUFF, coord, work->angle + EFFECT_BURST_PART_AIR_PUFF_ARG, 0);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (!((gRandomLcgState >> 16) & 7)) {
                effectSpawn(EFFECT_HIT_PUFF, coord, work->angle + EFFECT_BURST_PART_AIR_PUFF_ARG, 0);
            }
            if (work->age >= EFFECT_BURST_PART_DIM_START_FRAME) {
                model->flags |= TMD_OBJECT_SEMI_TRANS;
                if (work->period >= EFFECT_BURST_PART_FLIGHT_AMBIENT_STEP_Q12 + 1) {
                    ambientLevel = work->period - EFFECT_BURST_PART_FLIGHT_AMBIENT_STEP_Q12;
                    work->period = ambientLevel;
                    worldCoordSetModelAmbientColor(model, ambientLevel, ambientLevel, ambientLevel);
                    return;
                }
            }
            return;
        case EFFECT_BURST_PART_COLLAPSE:
            // Flatten vertically while dimming the ambient term; draw blending is separate.
            actorRenderComposeCoord(coord);
            if (!(work->age & EFFECT_BURST_PART_LIGHT_UPDATE_MASK)) {
                worldCoordSetModelLighting(model, coord->workm.t, 0, EFFECT_BURST_PART_LIGHT_COUNT);
            }
            if (work->age >= EFFECT_BURST_PART_COLLAPSE_FRAMES) {
                break;
            }
            memset(&collapseScale, 0, sizeof(collapseScale));
            collapseScale.vx  = ONE;
            collapseScale.vy  = (EFFECT_BURST_PART_COLLAPSE_FRAMES - work->age) << 8;
            collapseScale.vz  = ONE;
            collapseScaleCopy = collapseScale;
            ScaleMatrix(&coord->coord, &collapseScaleCopy);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->period >= EFFECT_BURST_PART_COLLAPSE_AMBIENT_STEP_Q12 + 1) {
                ambientLevel = work->period - EFFECT_BURST_PART_COLLAPSE_AMBIENT_STEP_Q12;
                work->period = ambientLevel;
                worldCoordSetModelAmbientColor(model, ambientLevel, ambientLevel, ambientLevel);
            }
            if (work->age == EFFECT_BURST_PART_BURN_FRAME) {
                effectSpawn(EFFECT_CORPSE_BURN, coord, work->angle >= EFFECT_BURST_PART_LARGE_BURN_PUFF_SIZE, 0);
            }
            return;
        default:
            return;
    }
    effectKillTask(work, task);
}

#undef EFFECT_STEP_BURST_MODEL_PART
