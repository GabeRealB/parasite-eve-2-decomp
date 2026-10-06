#include "rooms/dryfield_driveway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"
#include "../../shared/dryfield_driveway.h"

/// Set once the driveway event has been spawned.
extern u8 gDrivewayEventSpawned;

extern EvsCommand gDrivewayCutsceneScript[];
extern EvsCommand gDrivewayBlackoutScript[];
extern EvsCommand gDrivewayBlackoutTail[];

extern TaskDesc gRoomEventStagedTaskDesc;
extern TaskDesc gDrivewayCutsceneTasks[];

extern TaskMessageEntry D_dryfield_driveway_8017E754[];

extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

extern AnimationPlayRequest     D_dryfield_driveway_8017E330;
extern AnimationPlayRequest     D_dryfield_driveway_8017E358;
extern AnimationBankCopyRequest D_dryfield_driveway_8017E328;
extern WorldCollisionGrid       D_dryfield_driveway_8017ED74[1];
extern WorldCollisionTrigger    D_dryfield_driveway_8017FC98[6];
extern WorldCollisionTrigger    D_dryfield_driveway_801802F8[11];
extern WorldCoordRoomLights     D_dryfield_driveway_801802E0[1];
extern TaskDesc                 Actor00100_D1BA84;
s32                             func_dryfield_driveway_8017DCC0(Task*, s32, s32, s32);
static s32                      _dryfieldDrivewayIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg);
static s32                      _dryfieldDrivewayIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequestWord, s32 unusedSecondArg);
static void                     _dryfieldDrivewaySetEncounterWave(s32 waveStage);
static void                     _dryfieldDrivewayIdleRoomTask(Task* unusedTask);
void                            func_dryfield_driveway_8017DC64(u8);

/// The script releases actor 03700's staged entrance before combat waves advance.
enum { DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN = 1 };

static AnimationPackedPose _gDryfieldDrivewayAnimation00D08Bank1[10] = {
#include "assets/dryfield_driveway_animation_00D08_bank1.inc"
};

static AnimationPackedRotation _gDryfieldDrivewayAnimation00D08Bank4[95] = {
#include "assets/dryfield_driveway_animation_00D08_bank4.inc"
};

static AnimationRecord _gDryfieldDrivewayAnimation00D08Records[139] = {
#include "assets/dryfield_driveway_animation_00D08_records.inc"
};

static u16 _gDryfieldDrivewayAnimation00D08Indices[20] = {
#include "assets/dryfield_driveway_animation_00D08_indices.inc"
};

static AnimationSet _gDryfieldDrivewayAnimation00D08 = {
    _gDryfieldDrivewayAnimation00D08Records,
    _gDryfieldDrivewayAnimation00D08Indices,
    { NULL, _gDryfieldDrivewayAnimation00D08Bank1, NULL, NULL, _gDryfieldDrivewayAnimation00D08Bank4, NULL, NULL, NULL },
};

TaskDesc gRoomEventStagedTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gDrivewayCutsceneTasks[3] = {
    { { { TASK_BODY_NONE, 32 } }, drivewayBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, drivewayCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationSet* D_dryfield_driveway_8017E320[2] = {
    &_gDryfieldDrivewayAnimation00D08,
    NULL,
};

AnimationBankCopyRequest D_dryfield_driveway_8017E328 = { { .sets = D_dryfield_driveway_8017E320 }, ARRAY_SIZE(D_dryfield_driveway_8017E320) };

AnimationPlayRequest D_dryfield_driveway_8017E330 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_driveway_8017E344 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_driveway_8017E358 = { { .index = 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_driveway_8017E36C = { { -1067, 0, 1407, 0 }, { 0, 0, 0, 0 } };

EvsCommand gDrivewayCutsceneScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_driveway_8017E328 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_driveway_8017E358 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5219000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDrivewaySetEncounterWave }, { .value = DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_driveway_8017E330 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_dryfield_driveway_8017E4D4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_driveway_8017E4E8 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand gDrivewayBlackoutScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_dryfield_driveway_8017DC64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_driveway_8017E4D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x52190009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = drivewaySetViewDirty }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_driveway_8017E4E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDrivewayBlackoutTail[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = drivewaySetViewDirty }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_dryfield_driveway_8017E754[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, drivewayResolveEvent },
    { 5105, func_dryfield_driveway_8017DCC0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldDrivewayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldDrivewayIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, drivewayScriptSound },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_driveway_8017E784[2] = {
    { D_dryfield_driveway_8017ED74, D_dryfield_driveway_8017FC98, D_dryfield_driveway_801802F8, NULL },
    { D_dryfield_driveway_8017ED74, D_dryfield_driveway_8017FC98, D_dryfield_driveway_801802F8, NULL },
};

u8 D_dryfield_driveway_8017E7A4[8] = {
    1,
    2,
    3,
    7,
    5,
    6,
    4,
    0,
};

u8* D_dryfield_driveway_8017E7AC[2] = {
    gViewIdentityMap,
    D_dryfield_driveway_8017E7A4,
};

WorldCoordRoomLighting D_dryfield_driveway_8017E7B4[2] = {
    { D_dryfield_driveway_801802E0, NULL },
    { D_dryfield_driveway_801802E0, NULL },
};

ViewCount D_dryfield_driveway_8017E7C4[2] = { 7, 7 };

DirectionWarpEntry D_dryfield_driveway_8017E7C8[3] = {
    { { { .word = 1024 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -3400, 0, 1250 }, { 0, 0, 0, 0 }, 0x52190004, 0x52190003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 200, 0, 1650 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 200, 0, 1650 }, { 0, 0, 0, 0 }, 0x52190002, 0x52190001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 482 },
};

static SVECTOR _gDryfieldDrivewayCollision017B4Normals[13] = {
#include "assets/dryfield_driveway_collision_017B4_normals.inc"
};

static SVECTOR _gDryfieldDrivewayCollision017B4Verts[64] = {
#include "assets/dryfield_driveway_collision_017B4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldDrivewayCollision017B4Faces[23] = {
#include "assets/dryfield_driveway_collision_017B4_faces.inc"
};

static s16 _gDryfieldDrivewayCollision017B4Cells[154] = {
#include "assets/dryfield_driveway_collision_017B4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldDrivewayCollision017B4Cells[i])
static s16* _gDryfieldDrivewayCollision017B4Table[21] = {
#include "assets/dryfield_driveway_collision_017B4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_driveway_8017ED74[1] = {
    { NULL, _gDryfieldDrivewayCollision017B4Normals, _gDryfieldDrivewayCollision017B4Verts, _gDryfieldDrivewayCollision017B4Faces, _gDryfieldDrivewayCollision017B4Table, 0x2B16, 5210, 7, 3, 4000, 23 },
};

AreaResource D_dryfield_driveway_8017ED98[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_driveway_8017EDB0[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_driveway_8017EDBC[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_driveway_8017EDD4[11] = {
    { NULL, NULL },
    { D_map_dryfield_8017B7E4, D_dryfield_driveway_8017ED98 },
    { D_map_dryfield_8017B984, D_dryfield_driveway_8017EDB0 },
    { D_map_dryfield_8017B994, D_dryfield_driveway_8017EDBC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

ViewCamera D_dryfield_driveway_8017EE2C[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1950, 0x7530, -2670 } }, 380 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { -781, 0, -4020 }, { -1703, 3710, 331 }, { 3642, 1735, -707 } }, { 1820, 2220, -2312 } }, 230 },
    { { { { 287, 0, 4085 }, { 2747, 3031, -193 }, { -3024, 2754, 213 } }, { 29, 1719, -2342 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
};

SpriteBatch D_dryfield_driveway_8017EF28[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_driveway_8017EF38[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_driveway_8017EFEC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_driveway_8017F00C[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_driveway_8017F160[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_driveway_8017F198[2] = {
    { { 0, 0, 231, 239 }, 925 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_driveway_8017F1AC[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1125, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 8, 1125, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 885, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -40, -120, 1800, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -16, -120, 1825, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 8, -120, 1850, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 850, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 876, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 850, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 495, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 758, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 575, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 908, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 883, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 883, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 883, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 883, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1064, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 1064, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, 0, 1064, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 0, 1064, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 8, 1064, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 80, -96, 885, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, 40, 871, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 48, 883, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 56, 883, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 88, 48, 883, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 883, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_driveway_8017F594[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 7, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 3, 0, 0, { 6, 0 } },
    { 8, 3, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 9, 0 } },
    { 18, 9, 0, 0, { 0, 0 } },
    { 27, 3, 0, 0, { 10, 0 } },
    { 30, 4, 0, 0, { 5, 0 } },
    { 34, 5, 0, 0, { 8, 0 } },
    { 39, 5, 0, 0, { 4, 0 } },
    { 44, 1, 0, 0, { 11, 0 } },
    { 45, 5, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_driveway_8017F604[12] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 948, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 16, 888, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 895, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 969, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 32, 890, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 40, 885, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 48, 880, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 56, 880, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 650, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 80, 650, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, 32, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 72, 681, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_driveway_8017F6F4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_driveway_8017F714[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_driveway_8017F724[60] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1125, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 8, 1125, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 885, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -40, -120, 1800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -16, -120, 1825, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 8, -120, 1664, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 850, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 850, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 850, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 758, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 851, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -56, 837, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -96, 847, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -64, 845, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -56, 837, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -32, 837, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -56, 845, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -40, 845, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -24, 845, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 1082, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -16, 837, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 507, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 908, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 883, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 883, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 883, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 858, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1064, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 1064, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, 0, 1064, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 0, 1064, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 8, 1064, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 40, 883, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 48, 883, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 56, 883, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 48, 883, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 883, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_driveway_8017FBD4[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 7, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 3, 0, 0, { 6, 0 } },
    { 8, 3, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 9, 0 } },
    { 18, 9, 0, 0, { 0, 0 } },
    { 27, 3, 0, 0, { 10, 0 } },
    { 30, 11, 0, 0, { 5, 0 } },
    { 41, 4, 0, 0, { 8, 0 } },
    { 45, 5, 0, 0, { 4, 0 } },
    { 50, 5, 0, 0, { 11, 0 } },
    { 55, 5, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_driveway_8017FC44[7] = {
    { { .empty = D_dryfield_driveway_8017EF28 }, D_dryfield_driveway_8017EF28, NULL },
    { { .elements = D_dryfield_driveway_8017EF38 }, D_dryfield_driveway_8017EFEC, NULL },
    { { .elements = D_dryfield_driveway_8017F00C }, D_dryfield_driveway_8017F160, D_dryfield_driveway_8017F198 },
    { { .elements = D_dryfield_driveway_8017F1AC }, D_dryfield_driveway_8017F594, NULL },
    { { .elements = D_dryfield_driveway_8017F604 }, D_dryfield_driveway_8017F6F4, NULL },
    { { .empty = D_dryfield_driveway_8017F714 }, D_dryfield_driveway_8017F714, NULL },
    { { .elements = D_dryfield_driveway_8017F724 }, D_dryfield_driveway_8017FBD4, NULL },
};

WorldCollisionTrigger D_dryfield_driveway_8017FC98[6] = {
    { NULL, NULL, NULL, { -6369, -1536, -1504, 0 }, { { 0, -2560, 1024, 0 }, { 0, -2560, -1024, 0 }, { 0, 2560, 1024, 0 }, { 0, 2560, -1024, 0 } }, { -4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6816, -1472, -1568, 0 }, { { 0, -2496, -1024, 0 }, { 0, -2496, 1024, 0 }, { 0, 2496, -1024, 0 }, { 0, 2496, 1024, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2770, -1600, -856, 0 }, { { -1521, -2624, 0, 0 }, { 1522, -2624, 1, 0 }, { -1521, 2624, 0, 0 }, { 1522, 2624, 1, 0 } }, { 1, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2675, -1376, -1017, 0 }, { { 1554, -2400, 1, 0 }, { -1553, -2400, 0, 0 }, { 1554, 2400, 1, 0 }, { -1553, 2400, 0, 0 } }, { -3, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 294, -1520, 1474, 0 }, { { -435, -2544, 2215, 0 }, { 435, -2544, -2214, 0 }, { -435, 2544, 2215, 0 }, { 435, 2544, -2214, 0 } }, { -4034, 0, -793, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -26, -1488, 1301, 0 }, { { 435, -2512, -2213, 0 }, { -435, -2512, 2214, 0 }, { 435, 2512, -2213, 0 }, { -435, 2512, 2214, 0 } }, { 4031, 0, 792, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_driveway_8017FE60[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x28AE, -2000, 2764 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 1966, 2048 }, { 0, 0 } }, 0x61A7, 0x61A8 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4104 }, { 0, 0 } }, 499, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4990, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4098, 4107 }, { 0, 0 } }, 339, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5990, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4106, 4108, 4107 }, { 0, 0 } }, 438, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6892, -2000, 2764 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 1966, 2048 }, { 0, 0 } }, 0x61A7, 0x61A8 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4508, 4508, 4506 }, { 0, 0 } }, 737, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4099, 4097 }, { 0, 0 } }, 499, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4106, 4122, 4158 }, { 0, 0 } }, 398, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2990, -2000, 2220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4112, 4096, 4105 }, { 0, 0 } }, 498, 2000 },
};

WorldCoordRoomLights D_dryfield_driveway_801802E0[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_driveway_8017FE60), D_dryfield_driveway_8017FE60, 0, NULL },
};

WorldCollisionTrigger D_dryfield_driveway_801802F8[11] = {
    { NULL, NULL, NULL, { -3680, -48, 480, 0 }, { { -352, 0, -640, 0 }, { 352, 0, -640, 0 }, { -352, 0, 640, 0 }, { 352, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1488, -48, 2400, 0 }, { { -1104, 0, -1120, 0 }, { 1104, 0, -1120, 0 }, { -1104, 0, 1120, 0 }, { 1104, 0, 1120, 0 } }, { 0, 4117, 0, 0 }, { 4096, 0, 0, 0 }, 1567, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 33, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9664, -48, -1440, 0 }, { { -352, 0, -352, 0 }, { 352, 0, -352, 0 }, { -352, 0, 352, 0 }, { 352, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 497, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 55, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, -64, 1296, 0 }, { { -352, 0, -1072, 0 }, { 352, 0, -1072, 0 }, { -352, 0, 1072, 0 }, { 352, 0, 1072, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -64, 2720, 0 }, { { -400, 0, -832, 0 }, { 400, 0, -832, 0 }, { -400, 0, 832, 0 }, { 400, 0, 832, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 923, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -64, 2720, 0 }, { { -400, 0, -800, 0 }, { 400, 0, -800, 0 }, { -400, 0, 800, 0 }, { 400, 0, 800, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1456, -64, 1648, 0 }, { { -672, 0, -400, 0 }, { 672, 0, -400, 0 }, { -672, 0, 400, 0 }, { 672, 0, 400, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 781, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1504, -64, 1632, 0 }, { { -1072, 0, -1008, 0 }, { 1104, 0, -1008, 0 }, { -1104, 0, 400, 0 }, { 1072, 0, 400, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -64, 2512, 0 }, { { -1040, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -1072, 0, 1024, 0 }, { 368, 0, 1024, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1481, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -64, 2464, 0 }, { { -400, 0, -1024, 0 }, { 912, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 912, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1504, -64, 2400, 0 }, { { -1744, 0, -1760, 0 }, { 1712, 0, -1760, 0 }, { -1744, 0, 1120, 0 }, { 1712, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 2468, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_dryfield_driveway_8018063C = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_dryfield_driveway_80180648[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_driveway_80180650[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_driveway_80180658[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_driveway_8018063C },
};

WorldCollisionSurfaceProperties* D_dryfield_driveway_80180660[8] = {
    D_dryfield_driveway_80180648,
    D_dryfield_driveway_80180650,
    D_dryfield_driveway_80180658,
    D_dryfield_driveway_80180648,
    D_dryfield_driveway_80180648,
    D_dryfield_driveway_80180648,
    D_dryfield_driveway_80180648,
    D_dryfield_driveway_80180648,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 gDrivewayEventSpawned = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_dryfield_driveway_80180691 = 2;

u8 D_dryfield_driveway_80180692 = 240;

u8 D_dryfield_driveway_80180693 = 207;

RoomLatchedEvent gRoomEventLatched = { 0 };

static void func_dryfield_driveway_8017DDC0(Task* task);

#include "../../shared/room_event_staged_task.inc.c"

#include "../../shared/dryfield_driveway_resolve.inc.c"

#include "../../shared/dryfield_driveway_blackout.inc.c"

#include "../../shared/dryfield_driveway_cutscene.inc.c"

/// Sets the actor 03700 encounter's entrance and wave stage from an event script.
///
/// Stores the argument's low signed byte without clamping: 0 is the initial stage,
/// 1 begins wave progression, and 2..5 release successive waves. The script passes
/// `DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN`; actor tasks advance the later stages.
static void _dryfieldDrivewaySetEncounterWave(s32 waveStage)
{
    gSceneCombatState.actor03700Wave = waveStage;
}

#include "../../shared/dryfield_driveway_set_view_dirty.inc.c"

/// Script callback: stores its argument in the gameplay byte `D_80115768`.
void func_dryfield_driveway_8017DC64(u8 arg0)
{
    D_80115768 = arg0;
}

#include "../../shared/dryfield_driveway_script_sound.inc.c"

/// Answers 1 when a pending room-action trigger with `parameter0` 0xFF was hit.
static inline s32 _dryfieldDrivewayRoomTriggerHit(void)
{
    WorldCollisionTrigger* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && node->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && node->hit != 0) {
            return 1;
        }
    }
    return 0;
}

/// Message handler for message 0x114: while flag nibble 0x3A is 1, looks for a
/// room-action trigger with `parameter0` 0xFF and a non-zero `hit`;
/// when one exists it advances the nibble to 2, spawns the first cutscene task
/// of `gDrivewayCutsceneTasks`, moves the session to room 2 with the HUD
/// hidden and an event running, and reports the message handled.
s32 func_dryfield_driveway_8017DCC0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 0x114) {
        if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) == 1) {
            if (_dryfieldDrivewayRoomTriggerHit() != 0) {
                gameFlagSetNibble(GAME_FLAG_DRIVEWAY_PROGRESS, 2);
                taskSpawnFromTableOnDefaultList(gDrivewayCutsceneTasks, 0, 0, 0);
                gGameSession->location.loc.room = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2);
                gGameSession->hideHud           = 1;
                gGameSession->eventState        = 1;
                return 1;
            }
        }
    }
    return 0;
}

/// Answers `ROOM_MESSAGE_COMMAND` with zero without executing a room command.
///
/// All four callback arguments are ignored; no task or payload is accessed.
static s32 _dryfieldDrivewayIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg)
{
    return 0;
}

/// Answers `DIRECTION_MESSAGE_ROOM_ACTION` with zero without executing an action.
///
/// The borrowed action request stays an unused argument word; it is never
/// dereferenced or retained. All other callback arguments are ignored too.
static s32 _dryfieldDrivewayIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequestWord, s32 unusedSecondArg)
{
    return 0;
}

/// State 0 of the room task: attach the room's message table, publish the task
/// in pointer slot 7 and advance to the next state.
static void func_dryfield_driveway_8017DDC0(Task* task)
{
    task->msgTable = D_dryfield_driveway_8017E754;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the room task idle after its message table and room slot are installed.
///
/// Ignores the task argument and leaves the task in this state until another
/// owner changes its state or destroys it.
static void _dryfieldDrivewayIdleRoomTask(Task* unusedTask)
{
    // Retain the target's unused 16-byte stack frame; it has no accessed payload.
    char stackReservation[0x10];
}

/// The room task's state table, dispatched by `func_dryfield_driveway_8017DE14`
/// from a stack copy.
static const TaskFuncTable3 D_dryfield_driveway_8017D5D8 = {
    {
        func_dryfield_driveway_8017DDC0,
        _dryfieldDrivewayIdleRoomTask,
        taskKill,
    },
};

/// The room task: dispatches through its three-state table, copied onto the
/// stack first.
void func_dryfield_driveway_8017DE14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_driveway_8017D5D8;
    sp.funcs[task->state](task);
}

void dryfieldDrivewayEnableAmbientEffectsTask(Task* unusedTask)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
