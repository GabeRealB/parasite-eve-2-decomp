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

static void _shelter1fVehicularAirlockDrawPulsingLight(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale, s32 packedColor);

enum { SHELTER_1F_VEHICULAR_AIRLOCK_MESSAGE_USE_KEY_ITEM = 0x13F1 };

s32        func_shelter_1f_vehicular_airlock_8017D7DC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelter1fVehicularAirlockRejectKeyItem(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg);
s32        func_shelter_1f_vehicular_airlock_8017D990(Task*, s32, s32, s32);
static s32 _shelter1fVehicularAirlockIgnoreDirectionAction(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

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
    { SHELTER_1F_VEHICULAR_AIRLOCK_MESSAGE_USE_KEY_ITEM, _shelter1fVehicularAirlockRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fVehicularAirlockIgnoreDirectionAction },
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
static void           _shelter1fVehicularAirlockIdleMessageTask(Task* unusedTask);

static void _glowDrawAngledCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Sets bit 0x80 of the task's model flags while the 2-bit game flag its spawn
/// argument names reads 2, and clears it otherwise.
void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (areaGetCurrentObjectState((u8)((Enemy*)task->spawnArg2.pointer)->placeKey) == 2) {
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
    mapNeoArkResolveRoomVariant(in, out);
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

/// Refuses key-item use in this room, returning 0 without consuming the item.
///
/// The message supplies an inventory item ID and a zero second payload word;
/// neither payload nor the receiver is accessed.
static s32 _shelter1fVehicularAirlockRejectKeyItem(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        if (areaGetCurrentObjectState(6) == 2 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

/// Ignores direction-trigger room actions and returns 0.
///
/// Dispatch borrows the request for the call and supplies a zero second
/// payload word; this handler reads neither and retains no pointer.
static s32 _shelter1fVehicularAirlockIgnoreDirectionAction(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
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

/// Leaves the initialized room-message task idle in state 1.
static void _shelter1fVehicularAirlockIdleMessageTask(Task* unusedTask)
{
}

/// The message task's three state handlers: publishing the room's message
/// table, idling and `taskKill`.
static const TaskFuncTable3 D_shelter_1f_vehicular_airlock_8017D5D8 = {
    { func_shelter_1f_vehicular_airlock_8017D9FC, _shelter1fVehicularAirlockIdleMessageTask, taskKill },
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_shelter_1f_vehicular_airlock_8017D9FC`), idling
/// (`_shelter1fVehicularAirlockIdleMessageTask`) and `taskKill`. The table is
/// copied onto the stack first, so the call goes through a local copy rather
/// than the rodata.
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_vehicular_airlock_8017D5D8;
    sp.funcs[task->state](task);
}

void shelter1fVehicularAirlockDrawLightsTask(Task* task)
{
    enum {
        LIGHTS_INITIALIZE,
        LIGHTS_DRAW,
        LIGHT_COLOR_RED   = 2 << 8,
        LIGHT_COLOR_AMBER = (2 << 8) | (1 << 4),
        LIGHT_COLOR_GREEN = (2 << 4) | 1,
        LIGHT_COLOR_WHITE = (1 << 8) | (1 << 4) | 1,
    };
    u8 view;

    if (task->state == LIGHTS_INITIALIZE) {
        // Supply the room's effects for actor-triggered flashes, trails and sparks.
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_SPARK_BURST;
        task->state             = LIGHTS_DRAW;
    }

    // Only the two mapped room views with visible light fixtures emit glows.
    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            const SVECTOR* lightPoints = D_shelter_1f_vehicular_airlock_8018206C;

            _glowDrawAngledCapsule(&lightPoints[0], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[2], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[6], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[8], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawFactorDisc(&lightPoints[12], 0x200, LIGHT_COLOR_RED);
            break;
        }
        case 3: {
            const SVECTOR* lightPoints = D_shelter_1f_vehicular_airlock_8018205C;

            _glowDrawAngledCapsule(&lightPoints[0], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[2], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[6], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[8], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[12], 0x200, 0, LIGHT_COLOR_WHITE);
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 1) {
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[15], 0x804, 0x140, LIGHT_COLOR_GREEN);
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[16], 0xC0, 0x120, LIGHT_COLOR_AMBER);
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[17], -0xC0, 0x120, LIGHT_COLOR_AMBER);
            } else {
                _glowDrawFactorDisc(&lightPoints[15], 0x180, LIGHT_COLOR_GREEN);
                _glowDrawFactorDisc(&lightPoints[16], 0x140, LIGHT_COLOR_AMBER);
                _glowDrawFactorDisc(&lightPoints[17], 0x140, LIGHT_COLOR_AMBER);
            }
            break;
        }
    }
}

#include "../../shared/glow_draw_angled_capsule.inc.c"

#include "../../shared/glow_draw_factor_disc.inc.c"

/// Initializes a Gouraud light diagonal with a coloured centre and black ends.
///
/// Borrows one writable `LINE_G3`; the caller retains ownership. The centre's
/// byte intensities wrap wider inputs modulo 256. Sets the packet length,
/// opaque line command and polyline terminator. The caller must then supply
/// all three screen positions, link the packet and enable blending for the glow.
static inline void _shelter1fVehicularAirlockInitLightDiagonal(LINE_G3* diagonal, u8 red, u8 green, u8 blue)
{
    setLineG3(diagonal);
    setRGB0(diagonal, 0, 0, 0);
    setRGB1(diagonal, red, green, blue);
    setRGB2(diagonal, 0, 0, 0);
}

/// Draws an additive pulsing diamond and two diagonals around a world point.
///
/// Borrows `worldPoint` through view projection. Camera Z / 4 must be at
/// least `GLOW_MIN_DEPTH`; GTE flags are not tested. The signed low halfwords
/// of `radiusScale` and `pulseRate` supply pixel radius = radiusScale * 32 / depth
/// and phase advance in 4096 units per turn per animation frame. Intensity is
/// rsin(phase) / 68 + 60, narrowed to a byte. `packedColor` supplies a signed
/// red factor in bits 8..15 and unsigned green/blue factors in bits 4..5 and
/// 0..1; channel stores wrap to bytes. Requires 20 free scratch-stack bytes,
/// current view matrices and room for four packets plus additive blend commands
/// in the frame arena; packets must stay live until GPU completion.
static void _shelter1fVehicularAirlockDrawPulsingLight(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale, s32 packedColor)
{
    enum { LIGHT_PULSE_DIVISOR      = 68,
           LIGHT_PULSE_BASE         = 60,
           LIGHT_DEPTH_TO_TAG_SHIFT = 4,
           LIGHT_DEPTH_TAG_MASK     = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt) };
    RoomGlowSpriteScratch* projection;
    POLY_G4*               diamond;
    LINE_G3*               diagonal;
    s32                    pulseSine;
    u8                     intensity;
    s32                    red;
    s32                    green;
    s32                    blue;
    s32                    screenRadius;
    s32                    segmentIndex;
    s32                    diagonalXFactor;
    s32                    diagonalScale;
    s32                    diamondYFactor;
    u16                    centerXBits;
    u16                    centerYBits;

    projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    // Project first so lights too near the view consume no packets.
    if (projection->otz >= GLOW_MIN_DEPTH) {
        pulseSine              = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius           = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / projection->otz;
        intensity              = pulseSine / LIGHT_PULSE_DIVISOR + LIGHT_PULSE_BASE;
        red                    = intensity * ((s16)packedColor >> 8);
        green                  = intensity * (((s16)packedColor >> 4) & 3);
        blue                   = intensity * (packedColor & 3);
        segmentIndex           = 0;
        projection->halfExtent = screenRadius;
        // Two centre-lit triangles form the diamond, with a black outer rim.
        do {
            diamond        = gGpuPrimCursor;
            gGpuPrimCursor = diamond + 1;
            _glowInitAngledCapsuleWedge(diamond, red, green, blue);
            diamond->x0    = projection->screenPos.vx - projection->halfExtent;
            centerXBits    = projection->screenPos.vx;
            diamond->x2    = centerXBits;
            diamond->x1    = centerXBits;
            diamond->x3    = projection->screenPos.vx + projection->halfExtent;
            centerYBits    = projection->screenPos.vy;
            diamond->y3    = centerYBits;
            diamond->y2    = centerYBits;
            diamond->y0    = centerYBits;
            diamondYFactor = segmentIndex * 2;
            diamond->y1    = (projection->screenPos.vy - projection->halfExtent) + (projection->halfExtent * diamondYFactor);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    diamond);
            gpuSetPrimitiveBlendMode(diamond, GPU_BLEND_ADD, projection->otz);
            segmentIndex++;
        } while (segmentIndex < 2);

        // Add two crossed diagonals; the second reaches twice the radius.
        segmentIndex = 0;
        do {
            diagonal       = gGpuPrimCursor;
            gGpuPrimCursor = diagonal + 1;
            _shelter1fVehicularAirlockInitLightDiagonal(diagonal, red, green, blue);
            diagonalXFactor = segmentIndex * 3 - 1;
            diagonalScale   = segmentIndex + 1;
            diagonal->x0    = projection->screenPos.vx + (projection->halfExtent * diagonalXFactor);
            diagonal->y0    = projection->screenPos.vy - (projection->halfExtent * diagonalScale);
            diagonal->x1    = projection->screenPos.vx;
            diagonal->y1    = projection->screenPos.vy;
            diagonal->x2    = projection->screenPos.vx - (projection->halfExtent * diagonalXFactor);
            diagonal->y2    = projection->screenPos.vy + (projection->halfExtent * diagonalScale);
            addPrim(&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> LIGHT_DEPTH_TO_TAG_SHIFT & LIGHT_DEPTH_TAG_MASK],
                    diagonal);
            gpuSetPrimitiveBlendMode(diagonal, GPU_BLEND_ADD, projection->otz);
            segmentIndex = diagonalScale;
        } while (segmentIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelter1fVehicularAirlockRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_1f_vehicular_airlock_80180008(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
