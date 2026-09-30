#include "rooms/neo_ark_island.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_island_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"

/// Offsets from the parent coordinate of the two points whose trails
/// `func_neo_ark_island_80180600` records.
/// The second of those offsets, which the recording frames read by name.

TaskDesc D_neo_ark_island_80181B30 = { 0, 192, waterRefractionTask, { .model = NULL } };

TaskDesc D_neo_ark_island_80181B3C = { 0, 192, waterDistortBandTask, { .model = NULL } };

GpMsgEntry D_neo_ark_island_80181B48[6] = {
    { 5102, func_neo_ark_island_8017E968 },
    { 5105, func_neo_ark_island_8017E960 },
    { 5103, func_neo_ark_island_8017EA2C },
    { 5104, func_neo_ark_island_8017EA24 },
    { 5106, func_neo_ark_island_8017EA34 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_neo_ark_island_80181B78 = { 0, 32, func_neo_ark_island_8017E844, { .model = NULL } };

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_neo_ark_island_80181B94[1] = {
    { D_neo_ark_island_801826C8, D_neo_ark_island_80183CC8, D_neo_ark_island_80183DF8, NULL },
};

GpRoomCoordRec D_neo_ark_island_80181BA4[1] = {
    { D_neo_ark_island_80183CB0, NULL },
};

u8* D_neo_ark_island_80181BAC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_island_80181BB0[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_neo_ark_island_80181BB4[2] = {
    { { .words = { 2048, 3100, 0, 6464 } }, { 0, 0, 0, 0 }, { .words = { 2048, 3100, 0, 6464 } }, { 0, 0, 0, 0 }, 0x550E0002, 0x550E0001, 0, 2, 0, 0 },
    { { .words = { 0x7FFE, 5300, 0, -1900 } }, { 0, 0, 0, 0 }, { .words = { 0x7FFE, 5300, 0, -1900 } }, { 0, 0, 0, 0 }, 0x550E0004, 0, 0, 4, 2, 0 },
};

/// Room-effect task drawing a growing, fading quad through
/// `waterDrawSplash`. The first frame sets the brightness to
/// 0x40, takes the size from the spawn parameter's low 12 bits and turns the
/// coordinate to a random Y rotation. Every frame then grows the size by
/// 0x20, draws, and dims by 2, releasing the effect once the brightness falls
/// under 2. The coordinate is never rebuilt, so it keeps the frame the spawner
/// left. Once `gRoomEffectState->effectControl` leaves running it only draws,
/// and releases at cancellation.
void func_neo_ark_island_8017EB68(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        waterDrawSplash(coord, work->angle, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        waterDrawSplash(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

#include "../../shared/water_splash.inc.c"

/// Room-effect task that plays an eight-frame sprite animation. The
/// first frame takes the angle from the spawn parameter's low 12 bits, the
/// frame step from bits 12-15 and, from the top nibble, which of the two
/// sprite drawers to use; a zero velocity is seeded from the spawn kind
/// (random scatter, the stored direction, or none) and scaled to the requested
/// speed. Each later frame draws the sprite, moves the coordinate under a
/// constant downward pull while the speed is non-zero, and advances the frame
/// every `step` ticks, releasing the effect after the eighth. Once
/// `gRoomEffectState->effectControl` leaves running it only draws, and releases
/// at cancellation.
void func_neo_ark_island_8017EFE8(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         kind;
    s32         step;
    s32         state;
    s32         level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->state < 2) {
            waterDrawSpinU16(coord, (u16)work->index, work->scale, work->angle);
        } else {
            waterDrawTileU16(coord, (u16)work->index, work->scale);
        }
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            waterDrawSpinU16(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            waterDrawTileU16(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

void func_neo_ark_island_8017FB2C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DB;
        D_8011572C  = 0x601F7;
        D_80115750  = 0x60213;
        D_8011574C  = 0x60178;
        D_80115738  = 0x60179;
        arg0->state = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_island_8017FB9C(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_island_80180600(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_island_80180EE8(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
