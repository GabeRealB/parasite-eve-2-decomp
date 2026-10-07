#include "common.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Volume last asked of the warehouse's ambient track, or 0 when none is
/// playing. Written by `func_dryfield_warehouse_8017D5E8` and cleared by state 0
/// of the same task.
s32 D_dryfield_warehouse_801821B8;

#include "rooms/dryfield_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "dryfield_warehouse_private.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

/// Cutscene task spawned by state 0, polled by `taskPollKill` in state 1 and
/// killed along with its parent in state 2.
extern Task* D_dryfield_warehouse_801821B4;

SpriteBatch D_dryfield_warehouse_801815F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181608[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181618[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181628[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_warehouse_80181638[9] = {
    { { .empty = D_dryfield_warehouse_801811A0 }, D_dryfield_warehouse_801811A0, NULL },
    { { .elements = D_dryfield_warehouse_801811B0 }, D_dryfield_warehouse_80181354, NULL },
    { { .elements = D_dryfield_warehouse_80181384 }, D_dryfield_warehouse_80181578, NULL },
    { { .elements = D_dryfield_warehouse_801815A8 }, D_dryfield_warehouse_801815D0, NULL },
    { { .empty = D_dryfield_warehouse_801815E8 }, D_dryfield_warehouse_801815E8, NULL },
    { { .elements = D_dryfield_warehouse_80181384 }, D_dryfield_warehouse_80181578, NULL },
    { { .elements = D_dryfield_warehouse_801815A8 }, D_dryfield_warehouse_801815D0, NULL },
    { { .empty = D_dryfield_warehouse_80181618 }, D_dryfield_warehouse_80181618, NULL },
    { { .empty = D_dryfield_warehouse_80181628 }, D_dryfield_warehouse_80181628, NULL },
};

WorldCollisionTrigger D_dryfield_warehouse_801816A4[4] = {
    { NULL, NULL, NULL, { 2927, -1584, -2065, 0 }, { { -487, 1744, 1952, 0 }, { 487, 1744, -1951, 0 }, { -487, -1744, 1952, 0 }, { 487, -1744, -1951, 0 } }, { 3985, 0, 994, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3072, -960, -2114, 0 }, { { 487, 1984, -1951, 0 }, { -487, 1984, 1952, 0 }, { 487, -1984, -1951, 0 }, { -487, -1984, 1952, 0 } }, { -3980, 0, -994, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4703, -1104, -1858, 0 }, { { 1097, 2128, -1688, 0 }, { -1133, 2128, 1658, 0 }, { 1097, -2128, -1688, 0 }, { -1133, -2128, 1658, 0 } }, { -3424, 0, -2283, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4607, -1168, -1922, 0 }, { { -1133, 2192, 1655, 0 }, { 1097, 2192, -1692, 0 }, { -1133, -2192, 1655, 0 }, { 1097, -2192, -1692, 0 } }, { 3424, 0, 2281, 0 }, { 0, 0, 4096, 0 }, 2974, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_warehouse_801817D4[13] = {
    { NULL, NULL, NULL, { 2303, -63, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5056, -61, -880, 0 }, { { 352, 0, -687, 0 }, { 352, 0, 688, 0 }, { -352, 0, -687, 0 }, { -352, 0, 688, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5456, -64, -1824, 0 }, { { 528, 0, -703, 0 }, { 528, 0, 704, 0 }, { -528, 0, -703, 0 }, { -528, 0, 704, 0 } }, { 0, 4110, 0, 0 }, { -2897, 0, -2896, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -64, -416, 0 }, { { -1231, 0, -1008, 0 }, { 1232, 0, -1008, 0 }, { -1231, 0, 400, 0 }, { 1232, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1456, -64, -1216, 0 }, { { -383, 0, -1168, 0 }, { 384, 0, -1168, 0 }, { -383, 0, 1168, 0 }, { 384, 0, 1168, 0 } }, { 0, 4100, 0, 0 }, { 4095, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, -2656, 0 }, { { -1119, 0, -240, 0 }, { 1120, 0, -240, 0 }, { -1119, 0, 240, 0 }, { 1120, 0, 240, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4095, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -64, -3264, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, 4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2944, -64, -3000, 0 }, { { -415, 0, -312, 0 }, { 416, 0, -312, 0 }, { -415, 0, 168, 0 }, { 416, 0, 456, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 617, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1632, -64, -192, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, -256, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5216, -64, -1200, 0 }, { { 800, 0, -1119, 0 }, { 800, 0, 1120, 0 }, { -800, 0, -1119, 0 }, { -800, 0, 1120, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5888, -64, -1664, 0 }, { { 320, 0, -1119, 0 }, { 320, 0, 1664, 0 }, { -1472, 0, -1119, 0 }, { -1472, 0, 1664, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4091, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5344, -64, -1904, 0 }, { { 896, 0, -911, 0 }, { 896, 0, 912, 0 }, { -896, 0, -911, 0 }, { -896, 0, 912, 0 } }, { 0, 4095, 0, 0 }, { 201, 0, -4091, 0 }, 1273, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_warehouse_80181BB0[10] = {
    { NULL, NULL, NULL, { 2303, -48, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5728, -48, -864, 0 }, { { 352, 0, -671, 0 }, { 352, 0, 672, 0 }, { -352, 0, -671, 0 }, { -352, 0, 672, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5120, -64, -2256, 0 }, { { 352, 0, -655, 0 }, { 352, 0, 656, 0 }, { -352, 0, -655, 0 }, { -352, 0, 656, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 743, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -64, -416, 0 }, { { -1231, 0, -1008, 0 }, { 1232, 0, -1008, 0 }, { -1231, 0, 400, 0 }, { 1232, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1456, -64, -1216, 0 }, { { -383, 0, -1168, 0 }, { 384, 0, -1168, 0 }, { -383, 0, 1168, 0 }, { 384, 0, 1168, 0 } }, { 0, 4100, 0, 0 }, { 4095, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, -2656, 0 }, { { -1119, 0, -240, 0 }, { 1120, 0, -240, 0 }, { -1119, 0, 240, 0 }, { 1120, 0, 240, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4095, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3120, -64, -2944, 0 }, { { -367, 0, -368, 0 }, { 368, 0, -368, 0 }, { -367, 0, 368, 0 }, { 368, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 519, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -64, -3168, 0 }, { { -847, 0, -208, 0 }, { 848, 0, -208, 0 }, { -847, 0, 208, 0 }, { 848, 0, 208, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1664, -64, -288, 0 }, { { -847, 0, -208, 0 }, { 848, 0, -208, 0 }, { -847, 0, 208, 0 }, { 848, 0, 208, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4736, -64, -336, 0 }, { { -847, 0, -256, 0 }, { 848, 0, -256, 0 }, { -847, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 884, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_warehouse_80181EA8[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2870, -1533, -81 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 968, 3039 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1060, -710, -1699 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3280, 3280, 3280 }, { 0, 0 } }, 710, 1060 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3317, -270, -901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1232, 1232, 1232 }, { 0, 0 } }, 561, 755 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3416, -158, -3078 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2050, 2050, 2050 }, { 0, 0 } }, 1163, 1602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3067, -1528, -79 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2050, 2052, 2052 }, { 0, 0 } }, 1300, 3031 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3159, -1641, -3270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1941, 3832 },
};

WorldCoordRoomLights D_dryfield_warehouse_801820E8[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_warehouse_80181EA8), D_dryfield_warehouse_80181EA8, 0, NULL },
};

AreaVariant D_dryfield_warehouse_80182100[13] = { 0 };

WorldCollisionFootstepSounds D_dryfield_warehouse_80182168 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_80182174[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_8018217C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_80182184[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_8018218C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_warehouse_80182168 },
};

WorldCollisionSurfaceProperties* D_dryfield_warehouse_80182194[8] = {
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_8018217C,
    D_dryfield_warehouse_80182184,
    D_dryfield_warehouse_8018218C,
    D_dryfield_warehouse_80182174,
};

Task* D_dryfield_warehouse_801821B4 = NULL;

Task* D_dryfield_warehouse_801821BC;

Task* D_dryfield_warehouse_801821C0;

s16 D_dryfield_warehouse_801821C4;

static void func_dryfield_warehouse_8017D99C(Task* arg0);
static void func_dryfield_warehouse_8017D9F8(Task* task);

/// Warehouse ambience: state 0 clears the recorded volume and advances, state 1
/// maps `gGameSession->location.loc.view` (the area id) to a target volume - 0x32/0x3C/0x64
/// for areas 2/3/4, 0 elsewhere - and, whenever that differs from the recorded
/// one, enqueues the matching fade event: type 6 to start the track, type 7 to
/// stop it, type A to retune it, then records the new volume.
void func_dryfield_warehouse_8017D5E8(Task* task)
{
    s32 vol;

    switch (task->state) {
        case 0:
            D_dryfield_warehouse_801821B8 = 0;
            task->state                   = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    vol = 0;
    if (gGameSession->eventState == 0) {
        switch (gGameSession->location.loc.view) {
            case 4:
                vol = 0x64;
                break;
            case 3:
                vol = 0x3C;
                break;
            case 2:
                vol = 0x32;
                break;
            default:
                vol = 0;
                break;
        }
    }

    if (vol == D_dryfield_warehouse_801821B8) {
        return;
    }
    if (D_dryfield_warehouse_801821B8 == 0) {
        sndEvtRequestScriptStart(SOUND_WAREHOUSE_AMBIENCE, 0, (s8)(((0x64 - vol) * 0x7F) / 100));
    } else if (vol == 0) {
        sndEvtRequestScriptStop(SOUND_WAREHOUSE_AMBIENCE, 0x1E);
    } else {
        sndEvtRequestScriptMix(SOUND_WAREHOUSE_AMBIENCE, 0, (s8)(((0x64 - vol) * 0x7F) / 100));
    }
    D_dryfield_warehouse_801821B8 = vol;
}

/// Message handler: on msg 0x111, walks the `Gp_PendingObj4C` list looking for
/// a room-action trigger whose `parameter0` is 0xFF and whose `hit` is set, and
/// on a hit sets event nibble 0x3C, flips `gGameSession->eventState` and spawns the
/// warehouse cutscene task. Answers 1 only when it found one.
s32 func_dryfield_warehouse_8017D764(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    WorldCollisionTrigger* node;
    s32                    found;

    if (arg2 == 0x111) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && node->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && node->hit != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gameFlagSetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN, 1);
            gGameSession->eventState = 1;
            taskSpawnFromTableOnDefaultList(D_dryfield_warehouse_8017F56C, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

/// Copies the room message, then answers msg 9 by running CAP command 3 and
/// setting the event's nibble. queryOnly suppresses the side effects (the
/// handler only reports what *would* happen); any other message plays the
/// "refused" sound instead.
s32 func_dryfield_warehouse_8017D824(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE) {
        if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) != 0) {
            return 1;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(3);
            gameFlagSetNibbleIfPresent(in->flagId, 2);
        }
        return 0;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_WAREHOUSE_AMBIENCE, 0xF);
    }
    return 1;
}

/// Warehouse cutscene state machine: state 0 blanks the display and spawns the
/// cutscene task, state 1 waits for it to finish, and state 2 kills this task
/// once it has.
void func_dryfield_warehouse_8017D8D4(Task* arg0)
{
    s32 sp10;

    switch (arg0->state) {
        case 0:
            SetDispMask(0);
            D_dryfield_warehouse_801821B4 = taskSpawnFromTable(D_dryfield_warehouse_8017FB08, 0, 0, 0);
            arg0->state                  += 1;
            return;
        case 1:
            if (taskPollKill(D_dryfield_warehouse_801821B4, &sp10) != 0) {
                arg0->state += 1;
                return;
            }
            return;
        case 2:
            taskKill(arg0);
            break;
    }
}

/// State 0 of the room's main task: installs the room's message table,
/// publishes the task in game pointer slot 7, spawns the ambience task (entry 1
/// of the room's task table) and advances.
static void func_dryfield_warehouse_8017D99C(Task* arg0)
{
    arg0->msgTable = D_dryfield_warehouse_8017F554;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_dryfield_warehouse_8017F56C, 1, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the room's main task: does nothing.
static void func_dryfield_warehouse_8017D9F8(Task* task)
{
}

/// The three states of the room's main task, run by
/// `func_dryfield_warehouse_8017DA00`: set-up, the idle per-frame step and the
/// kill.
static const TaskFuncTable3 D_dryfield_warehouse_8017D5C4 = {
    { func_dryfield_warehouse_8017D99C, func_dryfield_warehouse_8017D9F8, taskKill },
};

/// Dispatches the room's main task through its three-state table, copied onto
/// the stack first.
void func_dryfield_warehouse_8017DA00(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_warehouse_8017D5C4;
    sp.funcs[task->state](task);
}
