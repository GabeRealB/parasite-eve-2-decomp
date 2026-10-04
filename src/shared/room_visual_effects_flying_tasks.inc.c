#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// A glowing disc attached to its parent at the work block's position. In
/// state 1 the disc grows, and every fourth tick the task spawns the effect
/// `gRoomEffectFlyingSparkId` names at a random joint of the player's model and adopts it
/// as a child task; state 2 adds a flickering half-bright second disc; state 3
/// drifts the disc away while it fades inside an expanding ring, then releases
/// the work block. The spawn argument picks the disc's colour shifts. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
static inline void RoomFx_GlowDiscTask(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    EffectWork* spawned;
    MATRIX*     mtx;
    u8          col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->parent                    = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            actorRenderComposeCoord(coord);
            if (!(mem->age & 3)) {
                Task* player    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                spawned         = Gp_SpawnEff(gRoomEffectFlyingSparkId, &player->extra.tmd->coords[((gRandomLcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].rShift;
            col[1] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].gShift;
            col[2] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].bShift;
            RoomFx_DrawFlyingDisc(coord, mem->angle, col);
            break;
        case 2:
            actorRenderComposeCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].rShift;
            col[1] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].gShift;
            col[2] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].bShift;
            RoomFx_DrawFlyingDisc(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                RoomFx_DrawFlyingDisc(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            actorRenderComposeCoord(coord);
            col[0] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].rShift;
            col[1] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].gShift;
            col[2] = mem->scale >> RoomFx_DiscShades[arg0->spawnArg1.value].bShift;
            RoomFx_DrawFlyingDisc(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            RoomFx_DrawFlyingRing(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            effectKillTask(mem, arg0);
            break;
        case 4:
            effectKillTask(mem, arg0);
            break;
    }
}

/// A spark that flies to another frame. The first tick takes the offset from
/// its own frame to the target frame the spawn argument names, in its own
/// axes, and keeps 0xCC/0x1000 of it as its step. Each later tick moves it by
/// that step and, every other tick, draws it as a textured square at the next
/// animation frame; it releases its work block after 20 ticks. It pauses while
/// the room's event state is set and releases the block when that state
/// reaches 4.
static inline void RoomFx_FlyingSparkTask(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   target;
    VECTOR      delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    target = task->spawnArg1.pointer;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    RoomFx_DrawFlyingSpark(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    effectKillTask(work, task);
                }
                break;
        }
    } else if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
    }
}
