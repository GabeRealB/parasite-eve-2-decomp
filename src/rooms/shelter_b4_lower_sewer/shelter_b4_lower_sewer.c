#include "rooms/shelter_b4_lower_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "types.h"

#include "shelter_b4_lower_sewer_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

extern TaskMessageEntry D_shelter_b4_lower_sewer_80181E44[];

extern TaskDesc                D_shelter_b4_lower_sewer_80181E70[];
extern RoomCompactWaterSurface D_shelter_b4_lower_sewer_80181E7C[];
extern RoomCompactWaterSurface D_shelter_b4_lower_sewer_80181E90[];

static void func_shelter_b4_lower_sewer_8017E33C(Task* arg0);
static void func_shelter_b4_lower_sewer_8017E37C(Task* task);

s32  func_shelter_b4_lower_sewer_8017D608(Task*, s32, s32, s32);
s32  func_shelter_b4_lower_sewer_8017D610(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_lower_sewer_8017D654(Task*, s32, s32, s32);
s32  func_shelter_b4_lower_sewer_8017D65C(Task*, s32, s32, s32);
void func_shelter_b4_lower_sewer_8017E2D4(Task*);

TaskMessageEntry D_shelter_b4_lower_sewer_80181E44[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_lower_sewer_8017D610 },
    { 5105, func_shelter_b4_lower_sewer_8017D608 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b4_lower_sewer_8017D65C },
    { ROOM_MESSAGE_COMMAND, func_shelter_b4_lower_sewer_8017D654 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 D_shelter_b4_lower_sewer_80181E6C = -1700;

TaskDesc D_shelter_b4_lower_sewer_80181E70[1] = {
    { { { TASK_BODY_NONE, 96 } }, func_shelter_b4_lower_sewer_8017E2D4, { .value = 0 } },
};

RoomCompactWaterSurface D_shelter_b4_lower_sewer_80181E7C[2] = {
    { -0x28A0, 0, 0x5B68, 2900, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomCompactWaterSurface D_shelter_b4_lower_sewer_80181E90[2] = {
    { -0x32C8, 0, 3600, 1450, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

static void func_shelter_b4_lower_sewer_8017D664(Task* task);
static void func_shelter_b4_lower_sewer_8017D6CC(Task* task);
static void func_shelter_b4_lower_sewer_8017D72C(Task* task);
static void func_shelter_b4_lower_sewer_8017DE8C(Task* task);

/// Handler for message 0x13F1 in the room's message table
/// `D_shelter_b4_lower_sewer_80181E44`: does nothing and returns 0.
s32 func_shelter_b4_lower_sewer_8017D608(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both on to `func_map_shelter_80179A04`. Always returns 1.
s32 func_shelter_b4_lower_sewer_8017D610(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// returns 0.
s32 func_shelter_b4_lower_sewer_8017D654(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// returns 0.
s32 func_shelter_b4_lower_sewer_8017D65C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: installs the room's message table, takes
/// game pointer slot 7 and, once GameFlag nibble 0xB7 is set, spawns the
/// tasks of `D_shelter_b4_lower_sewer_80181E70`.
static void func_shelter_b4_lower_sewer_8017D664(Task* task)
{
    task->msgTable = D_shelter_b4_lower_sewer_80181E44;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        Task_SpawnFromTable(D_shelter_b4_lower_sewer_80181E70, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state.
static void func_shelter_b4_lower_sewer_8017D6CC(Task* task)
{
}

/// State handlers of the room task `func_shelter_b4_lower_sewer_8017D6D4`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b4_lower_sewer_8017D5C4 = {
    { func_shelter_b4_lower_sewer_8017D664, func_shelter_b4_lower_sewer_8017D6CC, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b4_lower_sewer_8017D5C4`, copying the table onto the stack and
/// calling the entry for the task's current state.
void func_shelter_b4_lower_sewer_8017D6D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_lower_sewer_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E7C` at height
/// `D_shelter_b4_lower_sewer_80181E6C` as two strips of 32 semi-transparent
/// Gouraud quads laid side by side along Z, each strip running along X. The
/// seam between the strips is lifted by a sine wave whose phase advances with
/// the frame counter. The outer edges are coloured (0, 0x20, 0x80) and the seam
/// (0x20, 0x20, 0x20); each quad is followed by a draw-mode packet selecting
/// blend mode 2. Quads the projection flags as invalid are skipped. Called
/// from the water task's drawing state with the task, which it does not read.
static void func_shelter_b4_lower_sewer_8017D72C(Task* task)
{
    SVECTOR                  v0, v1, v2, v3;
    long                     sxy0, sxy1, sxy2, sxy3;
    long                     p, flag;
    RoomCompactWaterSurface* surface;
    WaterQuadScratch*        scratchEnd;
    WaterQuadScratch*        scratch;
    s32                      phase;
    POLY_G4*                 poly;
    DR_MODE*                 dr;
    s32                      otz;
    s32                      i;

    surface                    = D_shelter_b4_lower_sewer_80181E7C;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    phase                      = -(gDisplayState.animFrame * 16);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width / 32;
        scratch->dz = surface->depth / 2;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < 32; i++) {
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y;
            v1.vz            = scratch->z;
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0;
                poly->g0                       = 0x20;
                poly->b0                       = 0x80;
                poly->r1                       = 0;
                poly->g1                       = 0x20;
                poly->b1                       = 0x80;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        dr);
            }
        }
        for (i = 0; i < 32; i++) {
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y + scratch->yOffset;
            v0.vz            = scratch->z + scratch->dz;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y + scratch->yOffset;
            v1.vz            = scratch->z + scratch->dz;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y;
            v2.vz            = scratch->z + scratch->dz * 2;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y;
            v3.vz            = scratch->z + scratch->dz * 2;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r2                       = 0;
                poly->g2                       = 0x20;
                poly->b2                       = 0x80;
                poly->r3                       = 0;
                poly->g3                       = 0x20;
                poly->b3                       = 0x80;
                poly->r0                       = 0x20;
                poly->g0                       = 0x20;
                poly->b0                       = 0x20;
                poly->r1                       = 0x20;
                poly->g1                       = 0x20;
                poly->b1                       = 0x20;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        dr);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E90` as a strip of 8
/// Gouraud semi-transparent quads laid along X at height
/// `D_shelter_b4_lower_sewer_80181E6C`. The far edge of each quad is lifted by
/// a sine wave whose phase advances with the frame counter, so the surface
/// ripples. Each quad is followed by a draw-mode packet selecting blend mode 2;
/// quads the projection flags as invalid are skipped. Called from the water
/// task's drawing state with the task, which it does not read.
static void func_shelter_b4_lower_sewer_8017DE8C(Task* task)
{
    SVECTOR                  v0, v1, v2, v3;
    long                     sxy0, sxy1, sxy2, sxy3;
    long                     p, flag;
    s32                      phase;
    WaterQuadScratch*        scratchEnd;
    WaterQuadScratch*        scratch;
    RoomCompactWaterSurface* surface;
    POLY_G4*                 poly;
    DR_MODE*                 dr;
    s32                      otz;
    s32                      i;

    surface                    = D_shelter_b4_lower_sewer_80181E90;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    phase                      = -(gDisplayState.animFrame * 16);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width / 8;
        scratch->dz = surface->depth;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < 8; i++) {
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y;
            v1.vz            = scratch->z;
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0;
                poly->g0                       = 0x20;
                poly->b0                       = 0x80;
                poly->r1                       = 0;
                poly->g1                       = 0x20;
                poly->b1                       = 0x80;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt) + 1]),
                        dr);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// The room's water task: runs its state (`func_shelter_b4_lower_sewer_8017E33C`
/// once, then `func_shelter_b4_lower_sewer_8017E37C` every frame) and publishes
/// `D_shelter_b4_lower_sewer_80181E6C` as the session's water height.
void func_shelter_b4_lower_sewer_8017E2D4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_lower_sewer_8017E33C, func_shelter_b4_lower_sewer_8017E37C };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_lower_sewer_80181E6C;
}

/// First state of the water task: clears the session's `field_80` or
/// `field_7E`, chosen by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, and advances to the next state.
static void func_shelter_b4_lower_sewer_8017E33C(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Drawing state of the water task: points the primitive cursor
/// `D_shelter_b4_lower_sewer_80183E14` at `Fs_ActorLoadBase2` or `Fs_ActorLoadBase1`, chosen
/// by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, plus 0xC000 bytes per `gDisplayState.otBuffer`, then draws both sets
/// of water surfaces.
static void func_shelter_b4_lower_sewer_8017E37C(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    func_shelter_b4_lower_sewer_8017D72C(task);
    func_shelter_b4_lower_sewer_8017DE8C(task);
}
