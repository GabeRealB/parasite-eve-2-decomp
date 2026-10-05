#include "rooms/shelter_1f_bulwark.h"

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
#include "gameplay/hud_sprites.h"
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
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
// This room's fade symbol is the ScreenFade itself, with no following word.
#define ROOM_EVENT_FADE gRoomEventFade
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_bulwark_80180ECC[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_bulwark_80180ECC_value __asm__("D_shelter_1f_bulwark_80180ECC");

extern TaskDesc         D_shelter_1f_bulwark_80180320;
extern TaskMessageEntry D_shelter_1f_bulwark_8018032C[];
extern TaskDesc         D_shelter_1f_bulwark_80180354;
extern TaskDesc         D_shelter_1f_bulwark_80180360[];
extern SVECTOR          D_shelter_1f_bulwark_80180378[];
extern SVECTOR          D_shelter_1f_bulwark_80180398[];
extern ScreenFade       gRoomEventFade;
extern ScreenFade       D_shelter_1f_bulwark_80180EC0;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_1f_bulwark_8017DBD4(Task* task);
static void func_shelter_1f_bulwark_8017DC18(Task* task);

extern WorldCollisionGrid     D_shelter_1f_bulwark_80180648[1];
extern WorldCollisionOccluder D_shelter_1f_bulwark_80180E08[2];
extern WorldCollisionTrigger  D_shelter_1f_bulwark_80180A8C[2];
extern WorldCollisionTrigger  D_shelter_1f_bulwark_80180B24[8];
extern WorldCoordRoomLights   D_shelter_1f_bulwark_80180A74[1];

s32  func_shelter_1f_bulwark_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_1f_bulwark_8017DBBC(Task*, s32, s32, s32);
s32  func_shelter_1f_bulwark_8017DBC4(Task*, s32, s32, s32);
s32  func_shelter_1f_bulwark_8017DBCC(Task*, s32, s32, s32);
void func_shelter_1f_bulwark_8017DA60(Task*);
void func_shelter_1f_bulwark_8017DC78(Task*);
void func_shelter_1f_bulwark_8017DE04(Task*);

TaskDesc D_shelter_1f_bulwark_80180320 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_1f_bulwark_8018032C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_bulwark_8017D7B4 },
    { 5105, func_shelter_1f_bulwark_8017DBBC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_bulwark_8017DBCC },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_bulwark_8017DBC4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_1f_bulwark_80180354 = { { { TASK_BODY_NONE, 32 } }, func_shelter_1f_bulwark_8017DA60, { .value = 0 } };

TaskDesc D_shelter_1f_bulwark_80180360[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_1f_bulwark_8017DE04, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_1f_bulwark_8017DC78, { .value = 0 } },
};

SVECTOR D_shelter_1f_bulwark_80180378[4] = {
    { 3560, -6380, 2770, 0 },
    { 3560, -5130, -2810, 0 },
    { 2910, -4050, 1890, 0 },
    { 2910, -4050, -1930, 0 },
};

SVECTOR D_shelter_1f_bulwark_80180398[1] = {
    { -4180, -1940, 2720, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_shelter_1f_bulwark_801803B0[1] = {
    { D_shelter_1f_bulwark_80180648, D_shelter_1f_bulwark_80180A8C, D_shelter_1f_bulwark_80180B24, D_shelter_1f_bulwark_80180E08 },
};

WorldCoordRoomLighting D_shelter_1f_bulwark_801803C0[1] = {
    { D_shelter_1f_bulwark_80180A74, NULL },
};

u8* D_shelter_1f_bulwark_801803C8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_bulwark_801803CC[1] = { 3 };

DirectionWarpEntry D_shelter_1f_bulwark_801803D0[2] = {
    { { { .word = 3072 }, 4000, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 3130, 0, 0 }, { 0, 0, 0, 0 }, 0x55030002, 0x55030001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 428 },
    { { { .word = 1024 }, -3800, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -3800, 0, 0 }, { 0, 0, 0, 0 }, 0x55030004, 0x55030003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fBulwarkCollision03088Normals[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_normals.inc"
};

static SVECTOR _gShelter1fBulwarkCollision03088Verts[22] = {
#include "assets/shelter_1f_bulwark_collision_03088_verts.inc"
};

static WorldCollisionGridFace _gShelter1fBulwarkCollision03088Faces[12] = {
#include "assets/shelter_1f_bulwark_collision_03088_faces.inc"
};

static s16 _gShelter1fBulwarkCollision03088Cells[64] = {
#include "assets/shelter_1f_bulwark_collision_03088_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fBulwarkCollision03088Cells[i])
static s16* _gShelter1fBulwarkCollision03088Table[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_bulwark_80180648[1] = {
    { NULL, _gShelter1fBulwarkCollision03088Normals, _gShelter1fBulwarkCollision03088Verts, _gShelter1fBulwarkCollision03088Faces, _gShelter1fBulwarkCollision03088Table, 4250, 3500, 3, 2, 4000, 12 },
};

ViewCamera D_shelter_1f_bulwark_8018066C[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x38D6, 0 } }, 257 },
    { { { { 888, 0, -3998 }, { 1332, 3861, 296 }, { 3769, -1364, 837 } }, { 2810, 260, 870 } }, 257 },
    { { { { 902, 0, 3995 }, { 3827, 1174, -864 }, { -1145, 3924, 258 } }, { 1110, 5890, 370 } }, 207 },
};

SpriteBatch D_shelter_1f_bulwark_801806D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_bulwark_801806E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_bulwark_801806F8[8] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 32, 1225, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -104, 40, 1225, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -120, 48, 1225, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 48, 1400, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 64, 1400, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 96, 1425, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -112, 64, 1225, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -112, 96, 1425, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_bulwark_80180798[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_bulwark_801807B0[3] = {
    { { .empty = D_shelter_1f_bulwark_801806D8 }, D_shelter_1f_bulwark_801806D8, NULL },
    { { .empty = D_shelter_1f_bulwark_801806E8 }, D_shelter_1f_bulwark_801806E8, NULL },
    { { .elements = D_shelter_1f_bulwark_801806F8 }, D_shelter_1f_bulwark_80180798, NULL },
};

WorldCoordPointLight D_shelter_1f_bulwark_801807D4[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3994, -1930, 2718 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 0, 0 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2500, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2500, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -6000, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 1638, 1228 }, { 0, 0 } }, 6000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -6000, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 1638, 1228 }, { 0, 0 } }, 6000, 7000 },
};

WorldCoordRoomLights D_shelter_1f_bulwark_80180A74[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_bulwark_801807D4), D_shelter_1f_bulwark_801807D4, 0, NULL },
};

WorldCollisionTrigger D_shelter_1f_bulwark_80180A8C[2] = {
    { NULL, NULL, NULL, { -620, -3248, 96, 0 }, { { -313, -3712, -2468, 0 }, { 310, -3712, 2465, 0 }, { -313, 3712, -2468, 0 }, { 310, 3712, 2465, 0 } }, { 4075, 0, -516, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -478, -3312, 191, 0 }, { { 291, -3712, 2444, 0 }, { -302, -3712, -2455, 0 }, { 291, 3712, 2444, 0 }, { -302, 3712, -2455, 0 } }, { -4074, 0, 492, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_bulwark_80180B24[8] = {
    { NULL, NULL, NULL, { 3696, -53, 0, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3776, -53, 112, 0 }, { { -496, 0, -1776, 0 }, { 496, 0, -1776, 0 }, { -496, 0, 1776, 0 }, { 496, 0, 1776, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 1841, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -64, -1200, 0 }, { { -3552, 0, -368, 0 }, { 3552, 0, -368, 0 }, { -3552, 0, 368, 0 }, { 3552, 0, 368, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 3565, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -304, -64, 992, 0 }, { { -1744, 0, -368, 0 }, { 1744, 0, -368, 0 }, { -1744, 0, 368, 0 }, { 1744, 0, 368, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, -4096, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1600, -64, 2144, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2656, -64, -2176, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3808, -64, -2704, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 496, 0 }, { 496, 0, 496, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, 1024, 0 }, { { -1328, 0, -368, 0 }, { 1328, 0, -368, 0 }, { -1328, 0, 368, 0 }, { 1328, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_1f_bulwark_80180D84[3] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_1f_bulwark_80180DA8[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AF00, D_shelter_1f_bulwark_80180D84 },
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

WorldCollisionOccluder D_shelter_1f_bulwark_80180E08[2] = {
    { NULL, NULL, { 1840, -16, 2576, 0 }, { { -2864, 1936, -1104, 0 }, { 2864, 1936, 1104, 0 }, { -2864, -1936, -1104, 0 }, { 2864, -1936, 1104, 0 } }, { -1477, 0, 3827, 0 }, 3620, 1, 0 },
    { NULL, NULL, { 2624, 0, -3120, 0 }, { { -3216, 1936, 896, 0 }, { 3216, 1936, -896, 0 }, { -3216, -1936, 896, 0 }, { 3216, -1936, -896, 0 } }, { 1102, 0, 3957, 0 }, 3857, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_1f_bulwark_80180E80 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_shelter_1f_bulwark_80180E8C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_bulwark_80180E94[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_bulwark_80180E80 },
};

WorldCollisionSurfaceProperties* D_shelter_1f_bulwark_80180E9C[8] = {
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E94,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
};

ScreenFade gRoomEventFade = { 0 };

ScreenFade D_shelter_1f_bulwark_80180EC0 = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_shelter_1f_bulwark_80180ECC[4] = {
    0,
    33,
    -78,
    -119,
};

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 Bulwark_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

#include "../../shared/room_event_staged_task.inc.c"

static __inline__ s32 Bulwark_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_bulwark_80180ECC_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_1f_bulwark_80180320, 0, 0, 0);
            D_shelter_1f_bulwark_80180ECC_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_bulwark_8017D7B4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    RoomLatchedEvent event;

    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_1F_HELIPORT) {
        if (gameFlagGetNibble(GAME_FLAG_BULWARK_HELIPORT_UNBLOCKED) == 0) {
            Gp_SpawnIfCapIdle(1, 0);
            return 2;
        }
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 6) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 6);
                Gp_MsgPlayerWeapon(0);
                taskSpawnFromTable(&D_shelter_1f_bulwark_80180354, 0, 0, 0);
            }
            return 0;
        }
        event.capCmd   = 1;
        event.stageSnd = 0x55030003;
        event.flagId   = 0;
        event.fade     = 1;
        return Bulwark_StartEvent(dst, &event);
    }
    if (src->areaId == GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK) {
        event.capCmd   = 6;
        event.stageSnd = 0x55030001;
        event.flagId   = GAME_FLAG_BULWARK_TO_VEHICULAR_AIRLOCK_SCENE;
        event.fade     = 0;
        return Bulwark_StartEvent(dst, &event);
    }
    return 1;
}

/// The controller task's states: set up, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_bulwark_8017D5D8 = {
    {
        func_shelter_1f_bulwark_8017DBD4,
        func_shelter_1f_bulwark_8017DC18,
        taskKill,
    },
};

void func_shelter_1f_bulwark_8017DA60(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(1, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 2:
            D_shelter_1f_bulwark_80180EC0.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_1f_bulwark_80180EC0.phase      = SCREEN_FADE_RUNNING;
            D_shelter_1f_bulwark_80180EC0.rampFrames = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_1f_bulwark_80180EC0);
            arg0->killCountdown = 0;
            sndEvtRequestScriptStart(SOUND_SHELTER_1F_BULWARK_TO_HELIPORT, 0, 0);
            goto advance;
        case 3:
            arg0->killCountdown++;
            if (arg0->killCountdown < 0x1F) {
                break;
            }
            goto advance;
        case 5:
            gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 6);
            taskSpawnFromTable(D_shelter_1f_bulwark_80180360, 0, 0, 0);
        case 4:
        case 6:
        advance:
            arg0->state++;
            break;
        case 7:
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_1f_bulwark_8017DBBC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBC4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBCC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room's controller task: installs the room's message
/// table, registers the task in pointer slot 7 and advances to the idle state.
static void func_shelter_1f_bulwark_8017DBD4(Task* task)
{
    task->msgTable = D_shelter_1f_bulwark_8018032C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

static void func_shelter_1f_bulwark_8017DC18(Task* task)
{
}

/// The room's controller task: copies its three-entry state table (set up,
/// idle, kill) to the stack and runs the entry for the current state.
void func_shelter_1f_bulwark_8017DC20(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_bulwark_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_1f_bulwark_8017DC78(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->location;
    key.loc.view = 0x64;
    slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
    cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    displayResumeGameLoop();
}

void func_shelter_1f_bulwark_8017DE04(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Display_SpawnWithOt(D_shelter_1f_bulwark_80180360, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            Gp_SpawnViewTasks();
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state = arg0->state + 1;
            break;
        case 3:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_NEO_ARK_R26;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

#include "../../shared/glow_draw_factor_disc.inc.c"

void func_shelter_1f_bulwark_8017E2A4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_BULWARK_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_BULWARK_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_BULWARK_SPARK_BURST;
        arg0->state             = 1;
    }

    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_bulwark_80180378;
            _glowDrawFactorDisc(&p[0], 0x300, 0x210);
            _glowDrawFactorDisc(&p[1], 0x300, 0x210);
            _glowDrawFactorDisc(&p[2], 0x300, 0x111);
            _glowDrawFactorDisc(&p[3], 0x300, 0x111);
            break;
        }
        case 3:
            _glowDrawFactorDisc(&D_shelter_1f_bulwark_80180398[0], 0x200, 0x200);
            break;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_1f_bulwark_8017E38C(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_1f_bulwark_8017EDF0(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_1f_bulwark_8017F6D8(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
