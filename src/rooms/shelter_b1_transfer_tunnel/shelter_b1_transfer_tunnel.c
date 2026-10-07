#include "rooms/shelter_b1_transfer_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

/// The room's message table, installed in `Task::msgTable` by
/// `func_shelter_b1_transfer_tunnel_8017D62C`.
extern TaskMessageEntry D_shelter_b1_transfer_tunnel_801828C0[];

/// World-space points for the capsules and red disc drawn by
/// `shelterB1TransferTunnelDrawGlowsTask`, chosen by camera view.
extern SVECTOR D_shelter_b1_transfer_tunnel_801828E8[];
extern SVECTOR D_shelter_b1_transfer_tunnel_801828F8[];

/// Per-palette right shifts that turn the halo's level into its red, green
/// and blue channels.

/// The smoke trail's two spawn offsets: `[0]` places the effect's own
/// coordinate and `[1]` the second trail's origin. State 1 reads `[1]` again
/// under its own name.

static void func_shelter_b1_transfer_tunnel_8017D62C(Task* task);
static void _shelterB1TransferTunnelIdle(Task* task);

/// State handlers of the task `func_shelter_b1_transfer_tunnel_8017D678`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_transfer_tunnel_8017D5C4 = {
    { func_shelter_b1_transfer_tunnel_8017D62C, _shelterB1TransferTunnelIdle, taskKill }
};

static s32 _shelterB1TransferTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_shelter_b1_transfer_tunnel_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB1TransferTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB1TransferTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B1_TRANSFER_TUNNEL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_shelter_b1_transfer_tunnel_801828C0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_transfer_tunnel_8017D5D8 },
    { SHELTER_B1_TRANSFER_TUNNEL_MESSAGE_USE_KEY_ITEM, _shelterB1TransferTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1TransferTunnelIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1TransferTunnelIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b1_transfer_tunnel_801828E8[2] = {
    { 5017, -196, 1184, 0 },
    { 4172, -196, 1184, 0 },
};

SVECTOR D_shelter_b1_transfer_tunnel_801828F8[7] = {
    { 2313, -196, 1184, 0 },
    { 1527, -196, 1184, 0 },
    { 5017, -196, -1028, 0 },
    { 4172, -196, -1028, 0 },
    { 2313, -196, -1028, 0 },
    { 1527, -196, -1028, 0 },
    { 6559, -1206, -1163, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x001F }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b1_transfer_tunnel_80182954[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_transfer_tunnel_80182958[1] = { 4 };

DirectionWarpEntry D_shelter_b1_transfer_tunnel_8018295C[2] = {
    { { { .word = 3072 }, 6214, 0, 60 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 5570, 0, 20 }, { 0, 0, 0, 0 }, 0x54180002, 0x54180001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_R47_1C6 },
    { { { .word = 1024 }, 971, 0, 18 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 971, 0, 18 }, { 0, 0, 0, 0 }, 0x54180004, 0x54180003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1TransferTunnelCollision0552CNormals[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_normals.inc"
};

static SVECTOR _gShelterB1TransferTunnelCollision0552CVerts[12] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_verts.inc"
};

static WorldCollisionGridFace _gShelterB1TransferTunnelCollision0552CFaces[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_faces.inc"
};

static s16 _gShelterB1TransferTunnelCollision0552CCells[24] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1TransferTunnelCollision0552CCells[i])
static s16* _gShelterB1TransferTunnelCollision0552CTable[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_transfer_tunnel_80182AEC = { NULL, _gShelterB1TransferTunnelCollision0552CNormals, _gShelterB1TransferTunnelCollision0552CVerts, _gShelterB1TransferTunnelCollision0552CFaces, _gShelterB1TransferTunnelCollision0552CTable, 642, 1736, 3, 2, 4000, 6 };

ViewCamera D_shelter_b1_transfer_tunnel_80182B10[4] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3722, 8164, -144 } }, 235 },
    { { { { 440, 0, -4072 }, { -84, 4095, -9 }, { 4071, 85, 439 } }, { -3438, 1111, 45 } }, 225 },
    { { { { 510, 0, -4064 }, { 176, 4092, 22 }, { 4060, -177, 510 } }, { -982, 799, 335 } }, 235 },
    { { { { 1227, 0, 3907 }, { -265, 4086, 83 }, { -3898, -278, 1224 } }, { -4511, 770, 374 } }, 235 },
};

SpriteBatch D_shelter_b1_transfer_tunnel_80182BA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_transfer_tunnel_80182BB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_transfer_tunnel_80182BC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_transfer_tunnel_80182BD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_transfer_tunnel_80182BE0[4] = {
    { { .empty = D_shelter_b1_transfer_tunnel_80182BA0 }, D_shelter_b1_transfer_tunnel_80182BA0, NULL },
    { { .empty = D_shelter_b1_transfer_tunnel_80182BB0 }, D_shelter_b1_transfer_tunnel_80182BB0, NULL },
    { { .empty = D_shelter_b1_transfer_tunnel_80182BC0 }, D_shelter_b1_transfer_tunnel_80182BC0, NULL },
    { { .empty = D_shelter_b1_transfer_tunnel_80182BD0 }, D_shelter_b1_transfer_tunnel_80182BD0, NULL },
};

WorldCoordPointLight D_shelter_b1_transfer_tunnel_80182C10[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5609, -223, 118 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 799, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3663, -526, 7 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2720, 2900, 3122 }, { 0, 0 } }, 1460, 3061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2537, -85, -1 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2416, 2539, 2560 }, { 0, 0 } }, 1721, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1219, -223, -250 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1101, 2000 },
};

WorldCoordRoomLights D_shelter_b1_transfer_tunnel_80182D90 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_transfer_tunnel_80182C10), D_shelter_b1_transfer_tunnel_80182C10, 0, NULL };

WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182DA8[4] = {
    { NULL, NULL, NULL, { 5257, -576, 234, 0 }, { { 124, -2016, -2746, 0 }, { -123, -2016, 2746, 0 }, { 124, 2016, -2746, 0 }, { -123, 2016, 2746, 0 } }, { 4104, 0, 184, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5441, -576, 272, 0 }, { { -164, -2016, 2742, 0 }, { 165, -2016, -2742, 0 }, { -164, 2016, 2742, 0 }, { 165, 2016, -2742, 0 } }, { -4100, 0, -247, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2784, -512, 160, 0 }, { { -164, -2016, 2742, 0 }, { 165, -2016, -2742, 0 }, { -164, 2016, 2742, 0 }, { 165, 2016, -2742, 0 } }, { -4100, 0, -247, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2560, -576, 128, 0 }, { { 124, -2016, -2746, 0 }, { -123, -2016, 2746, 0 }, { 124, 2016, -2746, 0 }, { -123, 2016, 2746, 0 } }, { 4104, 0, 184, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182ED8[2] = {
    { NULL, NULL, NULL, { 6416, -48, 160, 0 }, { { -592, 0, -1024, 0 }, { 592, 0, -1024, 0 }, { -592, 0, 1024, 0 }, { 592, 0, 1024, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 864, -48, 96, 0 }, { { -592, 0, -1024, 0 }, { 592, 0, -1024, 0 }, { -592, 0, 1024, 0 }, { 592, 0, 1024, 0 } }, { 0, 4109, 0, 0 }, { 4096, 0, 0, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 36, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_transfer_tunnel_80182F70[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 3, 3, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_200300_80160110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_transfer_tunnel_80182F94[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_transfer_tunnel_80182FB8[8] = {
    { 21, 3, 0, 600, -1900, 770, 1024, 0, 0, 2, 4 },
    { 21, 3, 0, 600, -1900, -700, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, 1900, -1900, 970, 2048, 0, 0, 2, 2 },
    { 21, 1, 0, 1900, -1900, -800, 0, 0, 0, 2, 2 },
    { 21, 1, 0, 2700, -1900, 970, 2048, 0, 0, 2, 1 },
    { 21, 1, 0, 2700, -1900, -800, 0, 0, 0, 2, 1 },
    { 3, 0, 0, 4500, 0, 80, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_transfer_tunnel_80183038[8] = {
    { 21, 3, 0, 600, -1900, 770, 1024, 0, 0, 2, 4 },
    { 21, 3, 0, 600, -1900, -700, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, 1900, -1900, 970, 2048, 0, 0, 2, 2 },
    { 21, 1, 0, 1900, -1900, -800, 0, 0, 0, 2, 2 },
    { 21, 1, 0, 2700, -1900, 970, 2048, 0, 0, 2, 1 },
    { 21, 1, 0, 2700, -1900, -800, 0, 0, 0, 2, 1 },
    { 23, 4, 1, 1500, 0, 0, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_transfer_tunnel_801830B8[22] = {
    { NULL, NULL },
    { D_shelter_b1_transfer_tunnel_80182FB8, D_shelter_b1_transfer_tunnel_80182F70 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_transfer_tunnel_80183038, D_shelter_b1_transfer_tunnel_80182F94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_shelter_b1_transfer_tunnel_80183168 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_transfer_tunnel_80183174[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_transfer_tunnel_8018317C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_transfer_tunnel_80183168 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_transfer_tunnel_80183184[8] = {
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_8018317C,
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_80183174,
    D_shelter_b1_transfer_tunnel_80183174,
};

/// Refuses every key item without changing the room, returning zero to the item menu.
///
/// `itemId` is a collected item ID; neither payload word is read or retained.
static s32 _shelterB1TransferTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `mapShelterRoomVariantResolve` and returns 1.
s32 func_shelter_b1_transfer_tunnel_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

/// Ignores room commands from CAP and direction triggers, returning zero.
///
/// `commandId` and `commandArg` are receiver-specific integers; neither is read.
static s32 _shelterB1TransferTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room action requests from direction triggers, returning zero.
///
/// The borrowed request is neither dereferenced nor retained.
static s32 _shelterB1TransferTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// State 0 of the room's task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to state 1.
static void func_shelter_b1_transfer_tunnel_8017D62C(Task* task)
{
    task->msgTable = D_shelter_b1_transfer_tunnel_801828C0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the initialized room task available for messages without advancing its state.
static void _shelterB1TransferTunnelIdle(Task* task)
{
}

/// Runs the room's task through its three-state handler table, copied onto
/// the stack before the call.
void func_shelter_b1_transfer_tunnel_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_transfer_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}

/// Binds shared actor effect requests to this room's counted bank-6 tasks.
///
/// Installs seven packed bank/type IDs for `Gp_SpawnEff` after the room-effect
/// controller clears its selectors. Each ID persists until overwritten or the
/// controller is reinitialized. Keep this overlay loaded while the selected
/// effects are requested or running.
static inline void _shelterB1TransferTunnelBindEffects(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_BURST;
}

void shelterB1TransferTunnelDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           GLOWS_RADIUS_SCALE = 512 };

    u8 viewIndex;

    if (task->state == GLOWS_INITIALIZE) {
        _shelterB1TransferTunnelBindEffects();
        task->state = GLOWS_DRAW;
    }
    // The mapped view selects which fixed world-space light glows are visible.
    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2: {
            const SVECTOR* glowPoints = D_shelter_b1_transfer_tunnel_801828E8;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawRedDisc(&glowPoints[8], GLOWS_RADIUS_SCALE);
        } break;
        case 3: {
            const SVECTOR* glowPoints = D_shelter_b1_transfer_tunnel_801828E8;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOWS_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawRedDisc(&glowPoints[8], GLOWS_RADIUS_SCALE);
        } break;
        case 4: {
            const SVECTOR* glowPoints = D_shelter_b1_transfer_tunnel_801828F8;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOWS_RADIUS_SCALE, GLOW_HALF_TURN);
        } break;
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB1TransferTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_transfer_tunnel_801807F8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_transfer_tunnel_80181C78(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
