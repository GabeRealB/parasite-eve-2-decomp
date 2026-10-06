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
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffAttachTask37, { .model = NULL } },
};

/// Unreferenced nonzero tail; its original purpose is unknown.
u32 D_80114B7C = 0x323010CE;

void Gp_EffAttachTask37(Task* arg0)
{
    SVECTOR     delta;
    SVECTOR     dir;
    SVECTOR     pos;
    VECTOR      scale2;
    VECTOR      scale;
    TmdObject*  extra;
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   player;
    SVECTOR*    rot;
    MATRIX*     mtx;
    s32         state;
    s16         flag;
    s16         trans;
    s32         temp;

    extra  = arg0->extra.tmd;
    mem    = arg0->spawnArg2.pointer;
    coord  = extra->coords;
    player = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    flag   = gRoomEffectState->effectControl;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
        return;
    }
    actorRenderComposeCoord(coord);
    mem->age++;
    state = arg0->state;
    switch (state) {
        case 0:
            extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            mem->scale    = 0x100;
            if (arg0->spawnArg1.value & 0xFFF) {
                temp = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                temp = 0x200;
            }
            mem->angle      = temp;
            mem->period     = 0x800;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = 0x400 - ((gRandomLcgState >> 16) % 0xC00);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
            VectorNormalSS(&mem->move, &mem->move);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->pos.vx         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->pos.vy         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->pos.vz         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
            return;
        case 1:
            mtx = &coord->coord;
            rot = &mem->pos;
            gfxRotMatrixXYZ(mtx, rot, GRAPHICS_ROTATION_COMPOSE);
            MatrixNormal(mtx, mtx);
            gte_lddp(mem->scale);
            gte_ldsv(&mem->move);
            gte_gpf12();
            gte_stsv(&delta);
            coord->coord.t[0]  += delta.vx;
            coord->coord.t[1]  += delta.vy;
            coord->coord.t[2]  += delta.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&delta);
            gte_rtv0();
            gte_stsv(&dir);
            pos.vx  = coord->workm.t[0];
            pos.vy  = coord->workm.t[1];
            pos.vz  = coord->workm.t[2];
            dir.vx += pos.vx;
            dir.vy += pos.vy;
            dir.vz += pos.vz;
            if (worldCollisionProbeGridSegment(&dir, &pos, &dir, &pos) == state) {
                coord->coord.t[0] -= delta.vx;
                coord->coord.t[1] -= delta.vy;
                coord->coord.t[2] -= delta.vz;
                mem->move.vx       = (pos.vx >> 1) + (mem->move.vx >> 1);
                mem->move.vy       = pos.vy + (mem->move.vy >> 1);
                mem->move.vz       = (pos.vz >> 1) + (mem->move.vz >> 1);
                VectorNormalSS(&mem->move, &mem->move);
                mem->scale >>= 1;
                gte_lddp(mem->scale);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0]  += delta.vx;
                coord->coord.t[1]  += delta.vy;
                coord->coord.t[2]  += delta.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (!(mem->age & 3)) {
                    worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
                }
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, mem->angle + 0x12200, 0);
                gte_lddp(0x800);
                gte_ldsv(rot);
                gte_gpf12();
                gte_stsv(rot);
                if ((mem->age - mem->step) < 8 && mem->scale < 0x20) {
                    extra->flags |= TMD_OBJECT_SEMI_TRANS;
                    mem->age      = 0;
                    arg0->state   = 2;
                    return;
                }
                mem->step = mem->age;
                return;
            }
            if (mem->scale == 0) {
                return;
            }
            if (mem->age >= 0x4C) {
                break;
            }
            if (player->coord.t[1] + 0x100 < coord->coord.t[1]) {
                mem->age += 0xA;
            }
            actorRenderComposeCoord(coord);
            if (!(mem->age & 3)) {
                worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
            }
            mem->move.vy   += 0x10000 / mem->scale;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (!((gRandomLcgState >> 16) & 3)) {
                Gp_SpawnEff(EFFECT_TRAIL_PUFF, coord, mem->angle + 0x11000, 0);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (!((gRandomLcgState >> 16) & 7)) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, mem->angle + 0x11000, 0);
            }
            if (mem->age >= 0x33) {
                extra->flags |= TMD_OBJECT_SEMI_TRANS;
                if (mem->period >= 0x41) {
                    trans       = mem->period - 0x40;
                    mem->period = trans;
                    worldCoordSetModelAmbientColor(extra, trans, trans, trans);
                    return;
                }
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if (!(mem->age & 3)) {
                worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
            }
            if (mem->age >= 0x10) {
                break;
            }
            memset(&scale, 0, 0x10);
            scale.vx = 0x1000;
            scale.vy = (0x10 - mem->age) << 8;
            scale.vz = 0x1000;
            scale2   = scale;
            ScaleMatrix(&coord->coord, &scale2);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (mem->period >= 0x81) {
                trans       = mem->period - 0x80;
                mem->period = trans;
                worldCoordSetModelAmbientColor(extra, trans, trans, trans);
            }
            if (mem->age == 8) {
                Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, mem->angle >= 0x100, 0);
            }
            return;
        default:
            return;
    }
    effectKillTask(mem, arg0);
}
