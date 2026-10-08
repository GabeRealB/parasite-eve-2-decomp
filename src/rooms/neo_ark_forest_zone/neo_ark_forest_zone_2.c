#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/stdio.h>

#include "common.h"

#include "neo_ark_forest_zone_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/roaming_enemies.h"

static void _roamerArmPoolB(Task* task);
static void _roamerArmPoolA(Task* task);

static s32  _roamerAmbushMsg(Task* task, s32 messageId, struct ActorCommand* msg, s32 unusedArg);
static void _roamerBankRetreat(Task* task, s32 messageId, s32 hp, s32 unusedArg);

/// Enemy parameters shared by the spawn-slot controllers.
extern EnemyParams  gRoamerParams;
extern DamageAttack D_neo_ark_forest_zone_80182D04[6];

/// How many spawns each session slot arms, indexed by
/// `gGameSession->location.loc.variant`, for the second and the first arming task
/// respectively; zero disables that task's work in the slot.
extern u8 gRoamerArmCountsB[];
extern u8 gRoamerArmCountsA[];

/// How many of the pending spawn slots are armed and scanned.
extern s16 gRoamerReserveCount;

/// Set when a spawn slot was filled while the `gSceneCombatState` reference could
/// not yet be released; the first arming task's tick releases it later.
extern s16 gRoamerReleasePending;

/// Message tables the two arming tasks install in `Task::msgTable`.
extern TaskMessageEntry gRoamerMsgTableA[];
extern TaskMessageEntry gRoamerMsgTableB[];

/// Spawn placements of the first and the second arming task, indexed by the
/// placement request minus one.
extern RoamerSpawnPoint gRoamerSpawnPointsA[];
extern RoamerSpawnPoint D_neo_ark_forest_zone_80182DE8[5];

/// `gSceneCombatState.battleRefs` as seen on the previous frame.
extern s16 gRoamerPrevBattleRefs;

static void func_neo_ark_forest_zone_8018141C(Task* arg0);

/// State table of the first arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D5E8 = { {
    _roamerArmPoolA,
    roamerTickPoolA,
    func_neo_ark_forest_zone_8018141C,
    taskKill,
} };

static s32 _neoArkForestZoneIgnorePoolAActorCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedSecondArg);
static s32 _roamerExtendCooldown(Task* unusedTask, s32 unusedMessageId, s32 unusedEventValue, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolA(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolB(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

extern WorldCollisionGrid    D_neo_ark_forest_zone_80182274[1];
extern WorldCollisionTrigger D_neo_ark_forest_zone_801826B4[6];
extern WorldCollisionTrigger D_neo_ark_forest_zone_801829D0[10];
extern WorldCoordRoomLights  D_neo_ark_forest_zone_8018269C[1];

void func_neo_ark_forest_zone_80181430(Task*);
void func_neo_ark_forest_zone_8018151C(Task*);

static AnimationPackedPose _gNeoArkForestZoneAnimation04364Bank1[6] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank1.inc"
};

static AnimationPackedRotation _gNeoArkForestZoneAnimation04364Bank4[64] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank4.inc"
};

static AnimationRecord _gNeoArkForestZoneAnimation04364Records[141] = {
#include "assets/neo_ark_forest_zone_animation_04364_records.inc"
};

static u16 _gNeoArkForestZoneAnimation04364Indices[20] = {
#include "assets/neo_ark_forest_zone_animation_04364_indices.inc"
};

static AnimationSet _gNeoArkForestZoneAnimation04364 = {
    _gNeoArkForestZoneAnimation04364Records,
    _gNeoArkForestZoneAnimation04364Indices,
    { NULL, _gNeoArkForestZoneAnimation04364Bank1, NULL, NULL, _gNeoArkForestZoneAnimation04364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkForestZoneAnimation047D4Bank1[10] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank1.inc"
};

static AnimationPackedRotation _gNeoArkForestZoneAnimation047D4Bank4[95] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank4.inc"
};

static AnimationRecord _gNeoArkForestZoneAnimation047D4Records[139] = {
#include "assets/neo_ark_forest_zone_animation_047D4_records.inc"
};

static u16 _gNeoArkForestZoneAnimation047D4Indices[20] = {
#include "assets/neo_ark_forest_zone_animation_047D4_indices.inc"
};

static AnimationSet _gNeoArkForestZoneAnimation047D4 = {
    _gNeoArkForestZoneAnimation047D4Records,
    _gNeoArkForestZoneAnimation047D4Indices,
    { NULL, _gNeoArkForestZoneAnimation047D4Bank1, NULL, NULL, _gNeoArkForestZoneAnimation047D4Bank4, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_forest_zone_80181DBC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_forest_zone_80181DC8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_forest_zone_8017D7E4 },
    { NEO_ARK_FOREST_ZONE_MESSAGE_USE_KEY_ITEM, neoArkForestZoneRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_forest_zone_8017D958 },
    { ROOM_MESSAGE_COMMAND, neoArkForestZoneIgnoreCommandMessage },
    { ROOM_MESSAGE_ACTOR_EVENT, neoArkForestZoneForwardActorEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_neo_ark_forest_zone_80181DF8[2] = {
    &_gNeoArkForestZoneAnimation047D4,
    &_gNeoArkForestZoneAnimation04364,
};

AnimationBankCopyRequest D_neo_ark_forest_zone_80181E00 = { { .sets = D_neo_ark_forest_zone_80181DF8 }, ARRAY_SIZE(D_neo_ark_forest_zone_80181DF8) };

AnimationPlayRequest D_neo_ark_forest_zone_80181E08 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_neo_ark_forest_zone_80181E1C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

s32 D_neo_ark_forest_zone_80181E30 = 2821;

ActorCommand D_neo_ark_forest_zone_80181E34 = { { .loc = { 5, 11 } }, 1 };

s32 D_neo_ark_forest_zone_80181E38 = 0x20B05;

AnimationPlayRequest D_neo_ark_forest_zone_80181E3C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_neo_ark_forest_zone_80181E50 = { { 2888, 128, -95, 0 }, { 0, -1024, 0, 0 } };

Task* D_neo_ark_forest_zone_80181E68 = NULL;

EvsCommand D_neo_ark_forest_zone_80181E6C[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x550B000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_forest_zone_80181E34 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E38 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = neoArkForestZoneStartRoamerAmbush }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_forest_zone_801820A4[1] = {
    { D_neo_ark_forest_zone_80182274, D_neo_ark_forest_zone_801826B4, D_neo_ark_forest_zone_801829D0, NULL },
};

WorldCoordRoomLighting D_neo_ark_forest_zone_801820B4[1] = {
    { D_neo_ark_forest_zone_8018269C, NULL },
};

u8* D_neo_ark_forest_zone_801820BC[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_forest_zone_801820C0[1] = { 6 };

DirectionWarpEntry D_neo_ark_forest_zone_801820C4[3] = {
    { { { .word = 3072 }, 8995, 0, -128 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 8995, 0, -128 }, { 0, 0, 0, 0 }, 0x550B0002, 0x550B0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POWER_PLANT_1 },
    { { { .word = 1024 }, -7000, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -7000, 0, 0 }, { 0, 0, 0, 0 }, 0x550B000C, 0x550B000B, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -3000, 0, -770 }, { 0, 0, 0, 0 }, { { .word = 0 }, -3000, 0, -770 }, { 0, 0, 0, 0 }, 0x550B0004, 0x550B0003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkForestZoneCollision04CB4Normals[6] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_normals.inc"
};

static SVECTOR _gNeoArkForestZoneCollision04CB4Verts[8] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_verts.inc"
};

static WorldCollisionGridFace _gNeoArkForestZoneCollision04CB4Faces[6] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_faces.inc"
};

static s16 _gNeoArkForestZoneCollision04CB4Cells[30] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkForestZoneCollision04CB4Cells[i])
static s16* _gNeoArkForestZoneCollision04CB4Table[5] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_forest_zone_80182274[1] = {
    { NULL, _gNeoArkForestZoneCollision04CB4Normals, _gNeoArkForestZoneCollision04CB4Verts, _gNeoArkForestZoneCollision04CB4Faces, _gNeoArkForestZoneCollision04CB4Table, 8000, 1300, 5, 1, 4000, 6 },
};

ViewCamera D_neo_ark_forest_zone_80182298[6] = {
    { { { { 4096, 0, 0 }, { 0, 78, -4095 }, { 0, 4095, 78 } }, { -1000, 0x7530, 575 } }, 380 },
    { { { { -1370, 0, -3859 }, { -488, 4063, 173 }, { 3828, 518, -1359 } }, { -3940, 1460, -810 } }, 230 },
    { { { { -1441, 0, -3833 }, { 76, 4095, -28 }, { 3833, -81, -1441 } }, { 1990, 870, -910 } }, 230 },
    { { { { -1589, 0, -3775 }, { 258, 4086, -108 }, { 3766, -280, -1585 } }, { 6420, 670, -1080 } }, 230 },
    { { { { -899, 0, 3996 }, { -547, 4057, -123 }, { -3958, -560, -890 } }, { 2120, 500, -690 } }, 230 },
    { { { { -905, 0, 3994 }, { -83, 4095, -19 }, { -3993, -86, -905 } }, { -1010, 1270, -860 } }, 329 },
};

SpriteBatch D_neo_ark_forest_zone_80182370[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_80182380[4] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -104, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 88 } }, -160, -120, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -32, 1000, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 80 } }, -160, 40, 1000, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_801823D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_801823E8[5] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 1604, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 32 } }, -160, -120, 1550, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, -160, -88, 1571, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, -160, -40, 1611, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, 40, 1502, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_8018244C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_80182464[5] = {
    { 143, 0x3FC0, { .fields = { 96, 96 } }, 64, -120, 1050, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, 96, -24, 1050, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 80, 24, 1050, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, 96, 32, 1050, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 64, 1050, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_801824C8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_801824E0[7] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -40, 1342, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, 64, -40, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 48 } }, 56, 8, 1125, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 64 } }, 80, 56, 1125, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 80 } }, 24, -120, 1314, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -120, 1125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 1125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_8018256C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_forest_zone_80182584[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_forest_zone_80182594[6] = {
    { { .empty = D_neo_ark_forest_zone_80182370 }, D_neo_ark_forest_zone_80182370, NULL },
    { { .elements = D_neo_ark_forest_zone_80182380 }, D_neo_ark_forest_zone_801823D0, NULL },
    { { .elements = D_neo_ark_forest_zone_801823E8 }, D_neo_ark_forest_zone_8018244C, NULL },
    { { .elements = D_neo_ark_forest_zone_80182464 }, D_neo_ark_forest_zone_801824C8, NULL },
    { { .elements = D_neo_ark_forest_zone_801824E0 }, D_neo_ark_forest_zone_8018256C, NULL },
    { { .empty = D_neo_ark_forest_zone_80182584 }, D_neo_ark_forest_zone_80182584, NULL },
};

WorldCoordPointLight D_neo_ark_forest_zone_801825DC[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4220, -1742, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3641, 3828, 3683 }, { 0, 0 } }, 6362, 0x3E20 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -252, -969, 623 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 6, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4100, 4096 }, { 0, 0 } }, 1601, 2781 },
};

WorldCoordRoomLights D_neo_ark_forest_zone_8018269C[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_forest_zone_801825DC), D_neo_ark_forest_zone_801825DC, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_forest_zone_801826B4[6] = {
    { NULL, NULL, NULL, { 6141, -2640, 12, 0 }, { { -242, -2976, -3011, 0 }, { 231, -2976, 3006, 0 }, { -242, 2976, -3011, 0 }, { 231, 2976, 3006, 0 } }, { 4092, 0, -323, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6407, -2672, -38, 0 }, { { 217, -2976, 2875, 0 }, { -219, -2976, -2877, 0 }, { 217, 2976, 2875, 0 }, { -219, 2976, -2877, 0 } }, { -4085, 0, 309, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 303, -2625, -17, 0 }, { { -927, -3088, -2918, 0 }, { 922, -3088, 2912, 0 }, { -927, 3088, -2918, 0 }, { 922, 3088, 2912, 0 } }, { 3905, 0, -1239, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 511, -2674, -49, 0 }, { { 900, -2976, 2902, 0 }, { -907, -2976, -2908, 0 }, { 900, 2977, 2902, 0 }, { -907, 2977, -2908, 0 } }, { -3927, 0, 1221, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4322, -2641, 94, 0 }, { { -289, -3104, -3782, 0 }, { 264, -3104, 3755, 0 }, { -289, 3105, -3782, 0 }, { 264, 3105, 3755, 0 } }, { 4086, 0, -301, 0 }, { 0, 0, 4096, 0 }, 4884, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4176, -2657, 110, 0 }, { { 192, -3056, 3755, 0 }, { -200, -3056, -3764, 0 }, { 192, 3057, 3755, 0 }, { -200, 3057, -3764, 0 } }, { -4098, 0, 213, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_forest_zone_8018287C[3] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { 10, 561, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_356100_80173294 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_801828A0[2] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_801828B8[2] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_801828D0[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_801828E8[3] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { 20, 20, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_302000_80177DF0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_8018290C[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205600_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_forest_zone_80182930[2] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_neo_ark_forest_zone_80182948[2] = {
    { 13, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_neo_ark_forest_zone_80182968[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B190, D_neo_ark_forest_zone_8018287C },
    { D_map_neo_ark_8017B1C0, D_neo_ark_forest_zone_801828A0 },
    { D_map_neo_ark_8017B1F0, D_neo_ark_forest_zone_801828B8 },
    { D_map_neo_ark_8017B210, D_neo_ark_forest_zone_801828D0 },
    { D_map_neo_ark_8017B2C0, D_neo_ark_forest_zone_801828E8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_neo_ark_forest_zone_80182948, D_neo_ark_forest_zone_80182930 },
    { D_map_neo_ark_8017B300, D_neo_ark_forest_zone_8018290C },
    { NULL, NULL },
};

WorldCollisionTrigger D_neo_ark_forest_zone_801829D0[10] = {
    { NULL, NULL, NULL, { 9488, -48, 0, 0 }, { { -688, 0, -1024, 0 }, { 688, 0, -1024, 0 }, { -688, 0, 1024, 0 }, { 688, 0, 1024, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7680, -48, -16, 0 }, { { -592, 0, -880, 0 }, { 592, 0, -880, 0 }, { -592, 0, 880, 0 }, { 592, 0, 880, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2480, -48, -928, 0 }, { { 1040, 0, -400, 0 }, { 1008, 0, 400, 0 }, { -1008, 0, -400, 0 }, { -1040, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6768, -64, -96, 0 }, { { -480, 0, -1792, 0 }, { 224, 0, -1792, 0 }, { -224, 0, 1792, 0 }, { 480, 0, 1792, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1854, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4399, -64, -33, 0 }, { { -1851, 0, -1772, 0 }, { 1541, 0, -1796, 0 }, { -1541, 0, 1796, 0 }, { 1851, 0, 1772, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -626, -64, 46, 0 }, { { -1476, 0, -1706, 0 }, { 371, 0, -1800, 0 }, { -370, 0, 1801, 0 }, { 1477, 0, 1707, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6624, -64, -16, 0 }, { { -528, 0, -1744, 0 }, { 528, 0, -1744, 0 }, { -528, 0, 1744, 0 }, { 528, 0, 1744, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1819, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 4, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4959, -64, 0, 0 }, { { -720, 0, -1696, 0 }, { 464, 0, -1696, 0 }, { -464, 0, 1696, 0 }, { 720, 0, 1696, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1841, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 5, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -64, 1184, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4097, 0 }, 9328, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, -1120, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4095, 0 }, 9328, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_forest_zone_80182CC8 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionSurfaceProperties D_neo_ark_forest_zone_80182CD4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_forest_zone_80182CDC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_forest_zone_80182CC8 },
};

WorldCollisionSurfaceProperties* D_neo_ark_forest_zone_80182CE4[8] = {
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CDC,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
};

DamageAttack D_neo_ark_forest_zone_80182D04[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams gRoamerParams = { D_neo_ark_forest_zone_80182D04, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_forest_zone_80182D2C[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 gRoamerArmCountsB[16] = {
    0,
    1,
    3,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 gRoamerArmCountsA[14] = {
    0,
    3,
    2,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Signed countdown shared by the pools, gating spawn requests and battle reset.
///
/// Active per-frame states decrement positive values; zero is ready and -1
/// pauses the countdown. Arming or ambush restart sets 90 frames, spawning or
/// retreat adds 90, and battle completion sets 150. Arithmetic stores narrow
/// back to a signed halfword.
static s16 _gRoamerCooldownFrames = ROAMER_INITIAL_COOLDOWN_FRAMES;

s16 gRoamerReserveCount = 0;

/// One-based spawn-point selector pending for the next active pool tick.
///
/// Zero means none. Room actions select 1..5 here; pool A uses that table row,
/// while pool B uses its fifth row for selectors above four. Each active tick
/// clears the selector even if no enemy was revived. The latch also clears it
/// for repeated actions or while the cooldown is not zero.
static s16 _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;

/// Last room-action ID observed by either pool's spawn-request latch.
///
/// Starts at zero and records the borrowed request's byte even when cooldown
/// suppresses it, so the same action must change before it can request a spawn.
static s16 _gRoamerLastActionId = ROAMER_SPAWN_POINT_NONE;

s16 gRoamerReleasePending = 0;

TaskMessageEntry gRoamerMsgTableA[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolA },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerBankRetreat },
    { ACTOR_COMMAND_MESSAGE_APPLY, _neoArkForestZoneIgnorePoolAActorCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint gRoamerSpawnPointsA[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 gRoamerPrevBattleRefs = 0;

TaskMessageEntry gRoamerMsgTableB[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolB },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerExtendCooldown },
    { ACTOR_COMMAND_MESSAGE_APPLY, _roamerAmbushMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint D_neo_ark_forest_zone_80182DE8[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_forest_zone_80182E10[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_forest_zone_80182E18 = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_forest_zone_8018151C, { .value = 0 } };

TaskDesc D_neo_ark_forest_zone_80182E24 = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_forest_zone_80181430, { .value = 0 } };

static void func_neo_ark_forest_zone_80180D24(Task* arg0);

static void _neoArkForestZoneFinishRoamerPoolBState(Task* task);

#include "../../shared/roaming_enemies_bank_retreat.inc.c"

#include "../../shared/roaming_enemies_seed_reserve_hp.inc.c"

#include "../../shared/roaming_enemies_arm_pool_a.inc.c"

#include "../../shared/roaming_enemies_tick_pool_a.inc.c"

#include "../../shared/roaming_enemies_ambush_msg.inc.c"

#include "../../shared/roaming_enemies_arm_pool_b.inc.c"

static void func_neo_ark_forest_zone_80180D24(Task* arg0)
{
    s16    i;
    s16    count;
    s32    a;
    s32    b;
    Enemy* obj;
    s16    j;
    s16    k;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        b     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
        a     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B);
        b     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    if (gGameSession->battleResetPending == 1 && _gRoamerCooldownFrames == ROAMER_COOLDOWN_READY) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && _gRoamerPendingSpawnPoint != ROAMER_SPAWN_POINT_NONE) {
        gRoamerCommand.context.loc.stage = 5;
        gRoamerCommand.context.loc.area  = 0xB;
        gRoamerCommand.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (sceneFindPlacedActor(i) == 0) {
                break;
            }
            obj = sceneFindPlacedActor(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < gRoamerReserveCount; j++) {
                    if (((s16*)gRoamerReserveHp)[j] > 0) {
                        obj->hp             = gRoamerReserveHp[j];
                        obj->reactionFlags  = 0;
                        gRoamerReserveHp[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    sceneAcquireBattleRef(0);
                    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(i), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    switch ((s16)(_gRoamerPendingSpawnPoint - 1)) {
                        case 0:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_forest_zone_80182DE8[0].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1]   = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_forest_zone_80182DE8[0].z;
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[0].yaw, 1);
                            break;
                        case 1:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_forest_zone_80182DE8[1].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1]   = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_forest_zone_80182DE8[1].z;
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[1].yaw, 1);
                            break;
                        case 2:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[2].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[2].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[2].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 3:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[3].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[3].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[3].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 4:
                        default:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[4].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[4].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[4].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                    }
                }
                break;
            }
        }
    }
    _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;
}

/// Ignores actor commands sent to the forest's first reserve-enemy pool.
///
/// Returns zero without changing the pool or receiver. All arguments are
/// ignored; the borrowed command is neither accessed nor retained.
static s32 _neoArkForestZoneIgnorePoolAActorCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Selects the pool-A room-action latch emitted by the shared fragment.
///
/// The callback type and inclusion requirements are documented in
/// `roaming_enemies_latch_request.inc.c`.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolA
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

static void func_neo_ark_forest_zone_8018141C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// The first arming task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
void func_neo_ark_forest_zone_80181430(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_forest_zone_8017D5E8;
    handlers.funcs[task->state](task);
}

#include "../../shared/roaming_enemies_extend_cooldown.inc.c"

/// Selects the private pool-B room-action latch for this fragment inclusion.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolB
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the forest pool-B controller from its finishing state to teardown.
///
/// State 2 only increments the task's signed state; state 3 releases it on the
/// next dispatch. This state does not clear shared reserves or release actors.
static void _neoArkForestZoneFinishRoamerPoolBState(Task* task)
{
    task->state = task->state + 1;
}

/// State table of the second arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D634 = { {
    _roamerArmPoolB,
    func_neo_ark_forest_zone_80180D24,
    _neoArkForestZoneFinishRoamerPoolBState,
    taskKill,
} };

/// The second arming task: runs the state handler its state selects, through
/// a copy of the state table on the stack.
void func_neo_ark_forest_zone_8018151C(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_forest_zone_8017D634;
    handlers.funcs[task->state](task);
}
