#include "rooms/shelter_1f_vehicular_airlock.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_vehicular_airlock_80182AB0[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_vehicular_airlock_80182AB0_value __asm__("D_shelter_1f_vehicular_airlock_80182AB0");

extern TaskDesc D_shelter_1f_vehicular_airlock_80182028;

/// The room's message table, which its cap scripts index.
extern TaskMessageEntry D_shelter_1f_vehicular_airlock_80182034[];

extern SVECTOR D_shelter_1f_vehicular_airlock_8018205C[];
extern SVECTOR D_shelter_1f_vehicular_airlock_8018206C[];

/// The trail's two ends as offsets from the parent coordinate: `[0]` places
/// the object itself and `[1]` the trail's far end.

/// The far end's offset, `RoomFx_TrailOffsets[1]` reached
/// by its own name, as the task does on every tick after the first.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_1f_vehicular_airlock_8017D988(Task*, s32, s32, s32);
s32 func_shelter_1f_vehicular_airlock_8017D990(Task*, s32, s32, s32);
s32 func_shelter_1f_vehicular_airlock_8017D9F4(Task*, s32, s32, s32);

static u32     _gShelter1fVehicularAirlockModel03A58PartVerts[1];
static SVECTOR _gShelter1fVehicularAirlockModel03A58Verts[116];
static TmdBone _gShelter1fVehicularAirlockModel03A58Skeleton[1];
static u32     _gShelter1fVehicularAirlockModel03A58Stream[1019];

extern WorldCollisionGrid         D_shelter_1f_vehicular_airlock_80182438[1];
extern WorldCollisionTrigger      D_shelter_1f_vehicular_airlock_80182714[2];
extern WorldCollisionTrigger      D_shelter_1f_vehicular_airlock_801827AC[7];
extern WorldCoordRoomAmbientEntry D_shelter_1f_vehicular_airlock_801829C0[4];
extern WorldCoordRoomLights       D_shelter_1f_vehicular_airlock_801826FC[1];

static TmdBone _gShelter1fVehicularAirlockModel03A58Skeleton[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_skeleton.inc"
};

static u32 _gShelter1fVehicularAirlockModel03A58PartVerts[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_partVerts.inc"
};

static SVECTOR _gShelter1fVehicularAirlockModel03A58Verts[116] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_verts.inc"
};

static u32 _gShelter1fVehicularAirlockModel03A58Stream[1019] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_stream.inc"
};

TmdSource gShelter1fVehicularAirlockModel03A58 = {
    0,
    6672,
    0,
    1,
    _gShelter1fVehicularAirlockModel03A58PartVerts,
    _gShelter1fVehicularAirlockModel03A58Verts,
    &_gShelter1fVehicularAirlockModel03A58Verts[116],
    _gShelter1fVehicularAirlockModel03A58Skeleton,
    _gShelter1fVehicularAirlockModel03A58Stream,
};

TaskDesc D_shelter_1f_vehicular_airlock_80182028 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_1f_vehicular_airlock_80182034[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_vehicular_airlock_8017D7DC },
    { 5105, func_shelter_1f_vehicular_airlock_8017D988 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_vehicular_airlock_8017D9F4 },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_vehicular_airlock_8017D990 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_1f_vehicular_airlock_8018205C[2] = {
    { -0x2774, -3090, 1790, 0 },
    { -8900, -3090, 1790, 0 },
};

SVECTOR D_shelter_1f_vehicular_airlock_8018206C[16] = {
    { -6600, -3090, 1790, 0 },
    { -5400, -3090, 1790, 0 },
    { -3100, -3090, 1790, 0 },
    { -1900, -3090, 1790, 0 },
    { -0x2774, -3090, -1790, 0 },
    { -8900, -3090, -1790, 0 },
    { -6600, -3090, -1790, 0 },
    { -5400, -3090, -1790, 0 },
    { -3100, -3090, -1790, 0 },
    { -1900, -3090, -1790, 0 },
    { -9400, -1960, -2250, 0 },
    { -8600, -1960, -2250, 0 },
    { -5000, -2130, 2240, 0 },
    { -0x28AA, -3490, 100, 0 },
    { -0x2882, -3610, 900, 0 },
    { -0x2882, -3610, -900, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_shelter_1f_vehicular_airlock_801820FC[1] = {
    { D_shelter_1f_vehicular_airlock_80182438, D_shelter_1f_vehicular_airlock_80182714, D_shelter_1f_vehicular_airlock_801827AC, NULL },
};

WorldCoordRoomLighting D_shelter_1f_vehicular_airlock_8018210C[1] = {
    { D_shelter_1f_vehicular_airlock_801826FC, D_shelter_1f_vehicular_airlock_801829C0 },
};

u8* D_shelter_1f_vehicular_airlock_80182114[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_vehicular_airlock_80182118[1] = { 3 };

DirectionWarpEntry D_shelter_1f_vehicular_airlock_8018211C[3] = {
    { { { .word = 2048 }, -5000, 0, 1570 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -4960, 0, 1380 }, { 0, 0, 0, 0 }, 0x55020002, 0x55020001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8900, 0, -1730 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -4960, 0, 1380 }, { 0, 0, 0, 0 }, 0x55020002, 0x55020001, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -0x2710, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x2710, 0, 0 }, { 0, 0, 0, 0 }, 0x55020004, 0x55020003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, 428 },
};

static SVECTOR _gShelter1fVehicularAirlockCollision04E78Normals[10] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_normals.inc"
};

static SVECTOR _gShelter1fVehicularAirlockCollision04E78Verts[33] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_verts.inc"
};

static WorldCollisionGridFace _gShelter1fVehicularAirlockCollision04E78Faces[12] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_faces.inc"
};

static s16 _gShelter1fVehicularAirlockCollision04E78Cells[52] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fVehicularAirlockCollision04E78Cells[i])
static s16* _gShelter1fVehicularAirlockCollision04E78Table[9] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_vehicular_airlock_80182438[1] = {
    { NULL, _gShelter1fVehicularAirlockCollision04E78Normals, _gShelter1fVehicularAirlockCollision04E78Verts, _gShelter1fVehicularAirlockCollision04E78Faces, _gShelter1fVehicularAirlockCollision04E78Table, 0x2CEC, 3000, 3, 3, 4000, 12 },
};

ViewCamera D_shelter_1f_vehicular_airlock_8018245C[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 723, 0, -4031 }, { -887, 3995, -159 }, { 3932, 901, 706 } }, { 9160, 2690, 790 } }, 257 },
    { { { { 596, 0, 4052 }, { -95, 4094, 14 }, { -4051, -97, 596 } }, { 1760, 1260, 790 } }, 257 },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_vehicular_airlock_801824F8[3] = {
    { { .empty = D_shelter_1f_vehicular_airlock_801824C8 }, D_shelter_1f_vehicular_airlock_801824C8, NULL },
    { { .empty = D_shelter_1f_vehicular_airlock_801824D8 }, D_shelter_1f_vehicular_airlock_801824D8, NULL },
    { { .empty = D_shelter_1f_vehicular_airlock_801824E8 }, D_shelter_1f_vehicular_airlock_801824E8, NULL },
};

WorldCoordPointLight D_shelter_1f_vehicular_airlock_8018251C[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2130, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 1638, 1638 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9500, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -1635, -1950 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 500, 1000 },
};

WorldCoordRoomLights D_shelter_1f_vehicular_airlock_801826FC[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_vehicular_airlock_8018251C), D_shelter_1f_vehicular_airlock_8018251C, 0, NULL },
};

WorldCollisionTrigger D_shelter_1f_vehicular_airlock_80182714[2] = {
    { NULL, NULL, NULL, { -5709, -3248, -16, 0 }, { { -255, -3712, -3633, 0 }, { 254, -3712, 3632, 0 }, { -255, 3712, -3633, 0 }, { 254, 3712, 3632, 0 } }, { 4087, 0, -287, 0 }, { 0, 0, 4096, 0 }, 5196, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5519, -3312, -32, 0 }, { { 238, -3712, 3884, 0 }, { -245, -3712, -3892, 0 }, { 238, 3712, 3884, 0 }, { -245, 3712, -3892, 0 } }, { -4090, 0, 253, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_vehicular_airlock_801827AC[7] = {
    { NULL, NULL, NULL, { -4800, -48, 2080, 0 }, { { -576, 0, -416, 0 }, { 576, 0, -416, 0 }, { -576, 0, 416, 0 }, { 576, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 709, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8864, -48, -2112, 0 }, { { -800, 0, -416, 0 }, { 800, 0, -416, 0 }, { -800, 0, 416, 0 }, { 800, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 6, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2820, -48, 16, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 2534, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9072, -64, 704, 0 }, { { -1520, 0, -928, 0 }, { 1520, 0, 96, 0 }, { -1520, 0, -96, 0 }, { 1520, 0, 928, 0 } }, { 0, 4116, 0, 0 }, { 1380, 0, -3857, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7168, -64, 1736, 0 }, { { -416, 0, -936, 0 }, { 416, 0, -648, 0 }, { -416, 0, 792, 0 }, { 416, 0, 792, 0 } }, { 0, 4100, 0, 0 }, { 4052, 0, 601, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1920, -64, 0, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2534, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7312, -64, 1072, 0 }, { { -880, 0, -1080, 0 }, { 880, 0, -696, 0 }, { -880, 0, 840, 0 }, { 880, 0, 936, 0 } }, { 0, 4097, 0, 0 }, { 3166, 0, -2599, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_vehicular_airlock_801829C0[4] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_vehicular_airlock_801829C0) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 700, 700, 700, 700 } },
    { .color = { 700, 700, 700, 700 } },
};

AreaResource D_shelter_1f_vehicular_airlock_801829E0[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_1f_vehicular_airlock_80182A04[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AED0, D_shelter_1f_vehicular_airlock_801829E0 },
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

WorldCollisionFootstepSounds D_shelter_1f_vehicular_airlock_80182A64 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_shelter_1f_vehicular_airlock_80182A70[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_vehicular_airlock_80182A78[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_vehicular_airlock_80182A64 },
};

WorldCollisionSurfaceProperties* D_shelter_1f_vehicular_airlock_80182A80[8] = {
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A78,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_shelter_1f_vehicular_airlock_80182AB0[4] = {
    0,
    2,
    -16,
    65,
};

RoomLatchedEvent gRoomEventLatched = { 0 };

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_1f_vehicular_airlock_8017D9FC(Task* task);
static void           func_shelter_1f_vehicular_airlock_8017DA40(Task* task);

/// Sets bit 0x80 of the task's model flags while the 2-bit game flag its spawn
/// argument names reads 2, and clears it otherwise.
void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (Gp_GetCurBit2Flag((u8)((Enemy*)task->spawnArg2.pointer)->placeKey) == 2) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

#include "../../shared/room_event_staged_task.inc.c"

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_vehicular_airlock_80182AB0_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_1f_vehicular_airlock_80182028, 0, 0, 0);
            D_shelter_1f_vehicular_airlock_80182AB0_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId == GAME_AREA_SHELTER_1F_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        event.capCmd   = 4;
        event.stageSnd = 0x55020003;
        event.flagId   = GAME_FLAG_VEHICULAR_AIRLOCK_TO_BULWARK_SCENE;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_1F_AIRLOCK) {
        event.capCmd   = 6;
        event.stageSnd = 0x55020001;
        event.flagId   = GAME_FLAG_VEHICULAR_AIRLOCK_TO_AIRLOCK_SCENE;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D988(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's message task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_shelter_1f_vehicular_airlock_8017D9FC(Task* task)
{
    task->msgTable = D_shelter_1f_vehicular_airlock_80182034;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's message task: does nothing.
static void func_shelter_1f_vehicular_airlock_8017DA40(Task* task)
{
}

/// The message task's three state handlers: publishing the room's message
/// table, idling and `taskKill`.
static const TaskFuncTable3 D_shelter_1f_vehicular_airlock_8017D5D8 = {
    { func_shelter_1f_vehicular_airlock_8017D9FC, func_shelter_1f_vehicular_airlock_8017DA40, taskKill },
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_shelter_1f_vehicular_airlock_8017D9FC`), idling
/// (`func_shelter_1f_vehicular_airlock_8017DA40`) and `taskKill`. The table is
/// copied onto the stack first, so the call goes through a local copy rather
/// than the rodata.
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_vehicular_airlock_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_1f_vehicular_airlock_8017DAA0(Task* task)
{
    u8 view;

    if (task->state == 0) {
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_SPARK_BURST;
        task->state             = 1;
    }

    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_vehicular_airlock_8018206C;

            glowDrawAngledCapsule(&p[0], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[2], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[6], 0x200, 0, 0x210);
            glowDrawAngledCapsule(&p[8], 0x200, 0, 0x210);
            glowDrawFactorDisc(&p[12], 0x200, 0x200);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_1f_vehicular_airlock_8018205C;

            glowDrawAngledCapsule(&p[0], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[2], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[6], 0x200, 0, 0x210);
            glowDrawAngledCapsule(&p[8], 0x200, 0, 0x210);
            glowDrawAngledCapsule(&p[12], 0x200, 0, 0x111);
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 1) {
                func_shelter_1f_vehicular_airlock_8017E80C(&p[15], 0x804, 0x140, 0x21);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[16], 0xC0, 0x120, 0x210);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[17], -0xC0, 0x120, 0x210);
            } else {
                glowDrawFactorDisc(&p[15], 0x180, 0x21);
                glowDrawFactorDisc(&p[16], 0x140, 0x210);
                glowDrawFactorDisc(&p[17], 0x140, 0x210);
            }
            break;
        }
    }
}

#include "../../shared/glow_draw_angled_capsule.inc.c"

#include "../../shared/glow_draw_factor_disc.inc.c"

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, if
/// the resulting OTZ is at least 0x11, queues two gouraud `POLY_G4` diamonds
/// and two gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1`
/// scales the display frame counter into `rsin`, giving a pulse of
/// `rsin(...) / 68 + 0x3C`; `arg3` packs the lit vertex's colour as per-channel
/// multipliers of that pulse, red in bits 8 and up, green in bits 4-5 and blue
/// in bits 0-1.
static void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    RoomGlowSpriteScratch* block;
    POLY_G4*               prim;
    LINE_G3*               line;
    s32                    sine;
    u8                     pulse;
    s32                    r;
    s32                    g;
    s32                    b;
    s32                    radius;
    s32                    i;
    s32                    t1;
    s32                    t2;
    s32                    twice;
    u16                    sx;
    u16                    sy;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        sine              = rsin(gDisplayState.animFrame * (s16)arg1);
        radius            = ((s16)arg2 * 32) / block->otz;
        pulse             = sine / 68 + 0x3C;
        r                 = pulse * ((s16)arg3 >> 8);
        g                 = pulse * (((s16)arg3 >> 4) & 3);
        b                 = pulse * (arg3 & 3);
        i                 = 0;
        block->halfExtent = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx - block->halfExtent;
            sx       = block->screenPos.vx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->screenPos.vx + block->halfExtent;
            sy       = block->screenPos.vy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->screenPos.vy - block->halfExtent) + (block->halfExtent * twice);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, r, g, b);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->screenPos.vx + (block->halfExtent * t1);
            line->y0 = block->screenPos.vy - (block->halfExtent * t2);
            line->x1 = block->screenPos.vx;
            line->y1 = block->screenPos.vy;
            line->x2 = block->screenPos.vx - (block->halfExtent * t1);
            line->y2 = block->screenPos.vy + (block->halfExtent * t2);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_1f_vehicular_airlock_8017ECBC(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_1f_vehicular_airlock_8017F720(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_1f_vehicular_airlock_80180008(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
