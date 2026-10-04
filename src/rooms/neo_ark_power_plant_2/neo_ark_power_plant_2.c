#include "rooms/neo_ark_power_plant_2.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/glow_draw.h"

extern WorldCollisionOccluder     D_neo_ark_power_plant_2_80182E78[1];
extern WorldCoordRoomAmbientEntry D_neo_ark_power_plant_2_80182EB4[10];

extern TaskMessageEntry D_neo_ark_power_plant_2_801801F8[];
extern EvsCommand       D_neo_ark_power_plant_2_801802A8[];
extern EvsCommand       D_neo_ark_power_plant_2_80180560[];
extern SVECTOR          D_neo_ark_power_plant_2_80180668;
extern SVECTOR          D_neo_ark_power_plant_2_80180678;
extern AreaApplyRec     D_neo_ark_power_plant_2_80182F70[];
extern AreaApplyRec     D_neo_ark_power_plant_2_80182F94[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_power_plant_2_8017D6F4(Task* task);
static void func_neo_ark_power_plant_2_8017D758(Task* task);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, watch for the room's event trigger, then kill
/// the task.
static const TaskFuncTable3 D_neo_ark_power_plant_2_8017D5C4 = {
    { func_neo_ark_power_plant_2_8017D6F4, func_neo_ark_power_plant_2_8017D758, taskKill },
};

extern AreaResource D_neo_ark_power_plant_2_80182D80[3];
extern AreaResource D_neo_ark_power_plant_2_80182DA4[3];
extern AreaResource D_neo_ark_power_plant_2_80182DC8[2];

extern WorldCollisionGrid    D_neo_ark_power_plant_2_80180DC4[1];
extern WorldCollisionTrigger D_neo_ark_power_plant_2_801828C0[8];
extern WorldCollisionTrigger D_neo_ark_power_plant_2_80182B20[8];
extern WorldCoordRoomLights  D_neo_ark_power_plant_2_801828A8[1];

s32  func_neo_ark_power_plant_2_8017D5D0(Task*, s32, s32, s32);
s32  func_neo_ark_power_plant_2_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_power_plant_2_8017D61C(Task*, s32, s32, s32);
s32  func_neo_ark_power_plant_2_8017D694(Task*, s32, s32, s32);
void func_neo_ark_power_plant_2_8017D69C(void);
void func_neo_ark_power_plant_2_8017D6D4(void);

static AnimationPackedPose _gNeoArkPowerPlant2Animation02A4CBank1[3] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank1.inc"
};

static AnimationPackedRotation _gNeoArkPowerPlant2Animation02A4CBank4[28] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank4.inc"
};

static AnimationRecord _gNeoArkPowerPlant2Animation02A4CRecords[88] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_records.inc"
};

static u16 _gNeoArkPowerPlant2Animation02A4CIndices[20] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_indices.inc"
};

static AnimationSet _gNeoArkPowerPlant2Animation02A4C = {
    _gNeoArkPowerPlant2Animation02A4CRecords,
    _gNeoArkPowerPlant2Animation02A4CIndices,
    { NULL, _gNeoArkPowerPlant2Animation02A4CBank1, NULL, NULL, _gNeoArkPowerPlant2Animation02A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkPowerPlant2Animation02C10Bank1[2] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank1.inc"
};

static AnimationPackedRotation _gNeoArkPowerPlant2Animation02C10Bank4[22] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank4.inc"
};

static AnimationRecord _gNeoArkPowerPlant2Animation02C10Records[65] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_records.inc"
};

static u16 _gNeoArkPowerPlant2Animation02C10Indices[20] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_indices.inc"
};

static AnimationSet _gNeoArkPowerPlant2Animation02C10 = {
    _gNeoArkPowerPlant2Animation02C10Records,
    _gNeoArkPowerPlant2Animation02C10Indices,
    { NULL, _gNeoArkPowerPlant2Animation02C10Bank1, NULL, NULL, _gNeoArkPowerPlant2Animation02C10Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_neo_ark_power_plant_2_801801F8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_power_plant_2_8017D5D8 },
    { 5105, func_neo_ark_power_plant_2_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_power_plant_2_8017D694 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_power_plant_2_8017D61C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_neo_ark_power_plant_2_80180220 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_neo_ark_power_plant_2_80180234 = { { .loc = { 5, 16 } }, 1 };

ActorCommand D_neo_ark_power_plant_2_80180238 = { { .loc = { 5, 16 } }, 2 };

ActorCommand D_neo_ark_power_plant_2_8018023C = { { .loc = { 5, 16 } }, 3 };

AnimationSet* D_neo_ark_power_plant_2_80180240[3] = {
    NULL,
    &_gNeoArkPowerPlant2Animation02A4C,
    &_gNeoArkPowerPlant2Animation02C10,
};

AnimationPlayRequest D_neo_ark_power_plant_2_8018024C = { { .sets = D_neo_ark_power_plant_2_80180240 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_neo_ark_power_plant_2_80180260 = { { .sets = D_neo_ark_power_plant_2_80180240 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_neo_ark_power_plant_2_80180274 = { { 4000, -5000, -1540, 0 }, { 0, -2275, 0, 0 } };

PadScriptCmd D_neo_ark_power_plant_2_8018028C[5] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 5) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 40), PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 15) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_neo_ark_power_plant_2_801802A0[2] = {
    { 236, 77, 4, 1 },
    { 0, 0, 2, 0 },
};

EvsCommand D_neo_ark_power_plant_2_801802A8[29] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_neo_ark_power_plant_2_8017D69C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_power_plant_2_80180220 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55100006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55100007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_neo_ark_power_plant_2_8018028C }, { .vibrationSegments = D_neo_ark_power_plant_2_801802A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_80180234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_power_plant_2_80180274 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1012 }, { .message = { .pointer = &D_neo_ark_power_plant_2_8018024C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1012 }, { .message = { .pointer = &D_neo_ark_power_plant_2_80180260 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_80180238 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_neo_ark_power_plant_2_80180560[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_power_plant_2_80180274 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_neo_ark_power_plant_2_8017D6D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_8018023C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_neo_ark_power_plant_2_80180668 = { 4000, -7000, -4400, 0 };

SVECTOR D_neo_ark_power_plant_2_80180670 = { 4000, -5000, -1540, 0 };

SVECTOR D_neo_ark_power_plant_2_80180678 = { 6140, -6085, -8500, 0 };

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_power_plant_2_80180690[1] = {
    { D_neo_ark_power_plant_2_80180DC4, D_neo_ark_power_plant_2_801828C0, D_neo_ark_power_plant_2_80182B20, D_neo_ark_power_plant_2_80182E78 },
};

WorldCoordRoomLighting D_neo_ark_power_plant_2_801806A0[1] = {
    { D_neo_ark_power_plant_2_801828A8, D_neo_ark_power_plant_2_80182EB4 },
};

u8* D_neo_ark_power_plant_2_801806A8[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_power_plant_2_801806AC[2] = { 9, 0 };

DirectionWarpEntry D_neo_ark_power_plant_2_801806B0[1] = {
    { { { .word = 3072 }, 4000, -5000, -400 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4000, -5000, -400 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkPowerPlant2Collision03804Normals[11] = {
#include "assets/neo_ark_power_plant_2_collision_03804_normals.inc"
};

static SVECTOR _gNeoArkPowerPlant2Collision03804Verts[82] = {
#include "assets/neo_ark_power_plant_2_collision_03804_verts.inc"
};

static WorldCollisionGridFace _gNeoArkPowerPlant2Collision03804Faces[41] = {
#include "assets/neo_ark_power_plant_2_collision_03804_faces.inc"
};

static s16 _gNeoArkPowerPlant2Collision03804Cells[236] = {
#include "assets/neo_ark_power_plant_2_collision_03804_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkPowerPlant2Collision03804Cells[i])
static s16* _gNeoArkPowerPlant2Collision03804Table[12] = {
#include "assets/neo_ark_power_plant_2_collision_03804_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_power_plant_2_80180DC4[1] = {
    { NULL, _gNeoArkPowerPlant2Collision03804Normals, _gNeoArkPowerPlant2Collision03804Verts, _gNeoArkPowerPlant2Collision03804Faces, _gNeoArkPowerPlant2Collision03804Table, 100, 0x2F44, 3, 4, 4000, 41 },
};

ViewCamera D_neo_ark_power_plant_2_80180DE8[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x55F0, 6000 } }, 289 },
    { { { { -1159, 0, -3928 }, { -2285, 3331, 674 }, { 3195, 2382, -943 } }, { -791, 8408, 1169 } }, 230 },
    { { { { -875, 0, 4001 }, { 1486, 3802, 325 }, { -3714, 1522, -812 } }, { -6791, 7658, 1319 } }, 257 },
    { { { { -1124, 0, 3938 }, { 333, 4081, 95 }, { -3924, 346, -1120 } }, { -8841, 6358, 1319 } }, 230 },
    { { { { 3952, 0, 1075 }, { -23, 4095, 85 }, { -1075, -88, 3951 } }, { -8341, 6098, 8139 } }, 257 },
    { { { { -4002, 0, 868 }, { -45, 4090, -211 }, { -867, -216, -3997 } }, { -8441, 5798, 2039 } }, 289 },
    { { { { -2378, 0, 3334 }, { 2574, 2603, 1836 }, { -2119, 3162, -1511 } }, { -8341, 7898, 6139 } }, 257 },
    { { { { 3869, 0, 1342 }, { -153, 4069, 441 }, { -1334, -467, 3844 } }, { -4702, 6286, 3621 } }, 680 },
    { { { { -4095, 0, -13 }, { 4, 3863, -1361 }, { 12, -1361, -3862 } }, { -3992, 5736, 1231 } }, 230 },
};

SpriteBatch D_neo_ark_power_plant_2_80180F2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_80180F3C[64] = {
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, -48, 1319, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -40, 1325, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, -40, 1325, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, -32, 1300, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -32, 1300, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -24, 1146, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -16, 1111, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -8, 1082, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, 0, 1070, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 0, 1033, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, 0, 1047, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, 8, 1084, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, 8, 1062, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 72, 8, 1080, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, 0, 1172, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -8, 1242, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -16, 1205, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 0, 1232, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -24, 1200, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, -24, 1225, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -16, 1086, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 72, -16, 1111, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -8, 1032, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 80, -8, 1092, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -40, 1573, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 1298, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -120, 1395, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, -120, 1622, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -104, 1644, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -32, 1163, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1191, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -32, 1534, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -24, 1514, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 88, 32, 1035, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -64, 1500, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -56, 1512, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -48, 1542, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -56, 1450, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, -56, 1450, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 120, 0, 1061, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -32, 1450, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -40, 1475, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -40, 1500, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -32, 1525, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 48, 1097, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 48, 1097, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 48, 1097, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 1097, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 1097, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -112, 40, 1163, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -112, 32, 1225, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 794, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -72, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -80, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -40, 1175, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, 16, 925, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, 16, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 1000, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 40, 1056, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 0, 1000, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 24, 1025, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 24, 925, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 32, 875, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 40, 875, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_8018143C[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 6, 0 } },
    { 24, 16, 0, 0, { 1, 0 } },
    { 40, 4, 0, 0, { 4, 0 } },
    { 44, 5, 0, 0, { 0, 0 } },
    { 49, 1, 0, 0, { 5, 0 } },
    { 50, 1, 0, 0, { 3, 0 } },
    { 51, 6, 0, 0, { 7, 0 } },
    { 57, 7, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_8018148C[70] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 96, 864, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 746, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, 64, 806, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 48, 750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 32, 810, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 24, 852, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 16, 888, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 16, 942, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 8, 1007, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, 0, 1029, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 40, 779, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, 64, 843, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, 64, 879, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 64, 874, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 971, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 48, 1049, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 16, 936, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 8, 998, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 24, 911, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 845, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 765, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -160, 0, 1044, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -8, 1093, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 1172, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 0, 1162, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 16, 1011, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 16, 1011, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 24, 949, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 32, 899, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 32, 899, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 32, 899, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 838, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 40, 997, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 48, 788, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 784, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 788, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 56, 754, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 56, 754, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 56, 929, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 64, 718, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 798, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 72, 686, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 686, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 72, 807, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 80, 657, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 80, 657, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 88, 631, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 631, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 96, 606, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 561, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 104, 584, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 104, 584, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 112, 559, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 112, 559, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 32, 859, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 40, 812, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 772, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 56, 743, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 728, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 72, 715, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 718, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 700, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 690, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 690, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 678, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 678, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 104, 667, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 112, 656, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181A04[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 2, 0, 0, { 0, 0 } },
    { 25, 30, 0, 0, { 2, 0 } },
    { 55, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_80181A34[30] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, -8, 1375, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -96, -8, 1375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -96, 0, 1200, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, 0, 1200, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -136, 8, 1150, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 1150, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -48, -8, 1375, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -88, -8, 1375, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -144, -16, 1375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 0, 1151, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 0, 950, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 0, 950, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 0, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 0, 900, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 8, 875, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 8, 875, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 24, 875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 250, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, 0, 1000, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 0, 750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 850, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 16, 0x9E59, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 64, 675, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 32, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 88, 525, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 48, 375, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 96, 425, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 64, 250, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 80, 250, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 104, 250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181C8C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 3, 0, 0, { 0, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, 13, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_80181CBC[28] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 0, 1350, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 8, 1350, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 16, 1350, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 16, 1350, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, 24, 1350, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 48, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 48, 1350, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 1350, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 48, 1350, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -128, 24, 1350, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -128, 64, 1350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -120, 575, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -96, 575, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, 0, 775, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, 56, 775, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 0, 850, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 56, 850, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 8, 875, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 56, 875, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 24, 950, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 64, 950, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 32, 1075, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 64, 1075, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 1100, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -112, 8, 2000, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, 40, 8, 2000, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, 8, 2000, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 8, 2000, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181EEC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 13, 0, 0, { 2, 0 } },
    { 24, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_80181F14[8] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -8, 869, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, -8, 903, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -8, 939, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 64, -40, 1575, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 48, -40, 1625, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 32, -40, 1700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 16, -40, 2000, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 8, -40, 2000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181FB4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_power_plant_2_80181FD4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_power_plant_2_80181FE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_power_plant_2_80181FF4[4] = {
    { 143, 0x3FC0, { .fields = { 80, 48 } }, -160, 72, 525, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 48 } }, 80, 72, 525, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, -80, 40, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, 0, 40, 475, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80182044[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_power_plant_2_8018205C[9] = {
    { { .empty = D_neo_ark_power_plant_2_80180F2C }, D_neo_ark_power_plant_2_80180F2C, NULL },
    { { .elements = D_neo_ark_power_plant_2_80180F3C }, D_neo_ark_power_plant_2_8018143C, NULL },
    { { .elements = D_neo_ark_power_plant_2_8018148C }, D_neo_ark_power_plant_2_80181A04, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181A34 }, D_neo_ark_power_plant_2_80181C8C, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181CBC }, D_neo_ark_power_plant_2_80181EEC, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181F14 }, D_neo_ark_power_plant_2_80181FB4, NULL },
    { { .empty = D_neo_ark_power_plant_2_80181FD4 }, D_neo_ark_power_plant_2_80181FD4, NULL },
    { { .empty = D_neo_ark_power_plant_2_80181FE4 }, D_neo_ark_power_plant_2_80181FE4, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181FF4 }, D_neo_ark_power_plant_2_80182044, NULL },
};

WorldCoordPointLight D_neo_ark_power_plant_2_801820C8[21] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -7350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -7350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6340, -7990, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4510, -5320, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, -7380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, -4670 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4840, -6520, -2600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -4670 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -7380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, 700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, 730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, 730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, 700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
};

WorldCoordRoomLights D_neo_ark_power_plant_2_801828A8[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_power_plant_2_801820C8), D_neo_ark_power_plant_2_801820C8, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_power_plant_2_801828C0[8] = {
    { NULL, NULL, NULL, { 3040, -7040, 128, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3135, -7104, 96, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4096, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3967, -7040, -2240, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -7104, -2336, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6382, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7887, -7072, -4785, 0 }, { { 2024, -2256, 142, 0 }, { -2023, -2256, -142, 0 }, { 2024, 2256, 142, 0 }, { -2023, 2256, -142, 0 } }, { -288, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7839, -7040, -4705, 0 }, { { -2024, -2256, -139, 0 }, { 2024, -2256, 140, 0 }, { -2024, 2256, -139, 0 }, { 2024, 2256, 140, 0 } }, { 281, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_power_plant_2_80182B20[8] = {
    { NULL, NULL, NULL, { 3744, -64, -1616, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_FACING, 5, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -5056, -480, 0 }, { { -336, 0, -496, 0 }, { 336, 0, -496, 0 }, { -336, 0, 496, 0 }, { 336, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 596, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 37, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4607, -769, -1600, 0 }, { { -16, -1391, -496, 0 }, { 17, 1392, -496, 0 }, { -16, -1391, 496, 0 }, { 17, 1392, 496, 0 } }, { -4101, 42, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -4673, -480, 0 }, { { -17, 1392, 496, 0 }, { 16, -1391, 496, 0 }, { -17, 1392, -496, 0 }, { 16, -1391, -496, 0 } }, { -4101, -49, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 33, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 256, -48, -1120, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6720, -5152, -7600, 0 }, { { -320, 0, -896, 0 }, { 320, 0, -896, 0 }, { -320, 0, 896, 0 }, { 320, 0, 896, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4288, -5152, -2160, 0 }, { { -2720, 0, -368, 0 }, { 2720, 0, -368, 0 }, { -2720, 0, 368, 0 }, { 2720, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 2733, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6864, -5120, -4256, 0 }, { { -400, 0, -2352, 0 }, { 400, 0, -2352, 0 }, { -400, 0, 2352, 0 }, { 400, 0, 2352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 2374, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_power_plant_2_80182D80[3] = {
    { 53, 53, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_8013D3FC },
    { 21, 21, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8014DC30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_power_plant_2_80182DA4[3] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gGolemPawnRookTasks },
    { 21, 21, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8014DC30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_power_plant_2_80182DC8[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_power_plant_2_80182DE0[19] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B7D0, D_neo_ark_power_plant_2_80182D80 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B850, D_neo_ark_power_plant_2_80182DA4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B8B0, D_neo_ark_power_plant_2_80182DC8 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_power_plant_2_80182E78[1] = {
    { NULL, NULL, { 6064, -6976, -6944, 0 }, { { 208, 2944, 1760, 0 }, { -208, 2944, -1760, 0 }, { 208, -2944, 1760, 0 }, { -208, -2944, -1760, 0 } }, { 4068, 0, -481, 0 }, 3434, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_neo_ark_power_plant_2_80182EB4[10] = {
    { .viewCount = ARRAY_SIZE(D_neo_ark_power_plant_2_80182EB4) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1639, 1232, 1229, 1384 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_neo_ark_power_plant_2_80182F04 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_neo_ark_power_plant_2_80182F10 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_neo_ark_power_plant_2_80182F1C = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_neo_ark_power_plant_2_80182F28[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_power_plant_2_80182F30[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_power_plant_2_80182F10 },
};

WorldCollisionSurfaceProperties D_neo_ark_power_plant_2_80182F38[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_power_plant_2_80182F40[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_power_plant_2_80182F04 },
};

WorldCollisionSurfaceProperties D_neo_ark_power_plant_2_80182F48[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_power_plant_2_80182F1C },
};

WorldCollisionSurfaceProperties* D_neo_ark_power_plant_2_80182F50[8] = {
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F30,
    D_neo_ark_power_plant_2_80182F38,
    D_neo_ark_power_plant_2_80182F40,
    D_neo_ark_power_plant_2_80182F48,
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F28,
};

AreaApplyRec D_neo_ark_power_plant_2_80182F70[9] = {
    { 5, 8, 11, 1 },
    { 5, 11, 4, 1 },
    { 5, 13, 4, 1 },
    { 5, 21, 4, 17 },
    { 5, 21, 7, 33 },
    { 5, 29, 3, 1 },
    { 5, 32, 3, 1 },
    { 4, 18, 11, 0 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_neo_ark_power_plant_2_80182F94[4] = {
    { 5, 12, 2, 1 },
    { 5, 14, 2, 1 },
    { 5, 30, 2, 1 },
    { 255, 0, 0, 0 },
};

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_power_plant_2_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both on to `func_map_neo_ark_80179B14`. Always returns 1.
s32 func_neo_ark_power_plant_2_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_power_plant_2_8017D61C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 cmd;

    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
                cmd = 2;
            } else {
                cmd = 5;
            }
            break;
        case 3:
            cmd = 7;
            if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED) {
                cmd = GameFlag_GetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN) != 0 ? 6 : 3;
            }
            break;
        default:
            goto done;
    }
    Gp_RunCapCmd1(cmd);
done:
    return 0;
}

s32 func_neo_ark_power_plant_2_8017D694(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

void func_neo_ark_power_plant_2_8017D69C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void func_neo_ark_power_plant_2_8017D6D4(void)
{
    Gp_HaltPadScripts();
}

static void func_neo_ark_power_plant_2_8017D6F4(Task* arg0)
{
    u8 temp_v1;

    arg0->msgTable = D_neo_ark_power_plant_2_801801F8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    temp_v1 = gGameSession->location.loc.variant;
    if (temp_v1 == 1) {
        gGameSession->flowFlags = temp_v1;
    }
    arg0->state = arg0->state + 1;
}

static void func_neo_ark_power_plant_2_8017D758(Task* task)
{
    Task* temp_v0;

    if (GameFlag_GetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
        temp_v0 = Gp_LookupSlot4(0);
        if ((temp_v0 != 0) && (taskMessageDispatch(temp_v0, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) &&
            (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
            GameFlag_SetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED, 1);
            GameFlag_SetNibble(GAME_FLAG_NEO_ARK_EVE_ELEVATOR_UNLOCKED, 1);
            GameFlag_SetNibble(GAME_FLAG_MAP_MARK_POWER_PLANT_2, 0);
            Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F70);
            if (GameFlag_GetNibble(GAME_FLAG_0F3) != 0) {
                Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F94);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x17;
            func_800E3FAC(0xA2, 0x2E);
            GameFlag_SetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            GameFlag_SetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 7);
            func_800E8634(D_neo_ark_power_plant_2_801802A8, 0, D_neo_ark_power_plant_2_80180560);
        }
    }
}

/// Dispatches the room's message-driven task through its three-state table
/// `D_neo_ark_power_plant_2_8017D5C4`, copied onto the stack before the call.
void func_neo_ark_power_plant_2_8017D854(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_power_plant_2_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_power_plant_2_8017D8AC(Task* arg0)
{
    u32                            rnd;
    u16                            intensity;
    WorldCoordPointLight*          pointLight;
    WorldCoordTransientPointLight* lightSlot;

    if (arg0->state == 0) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_POWER_PLANT_2_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_POWER_PLANT_2_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_POWER_PLANT_2_SPARK_BURST;
        arg0->state             = 1;
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 6:
            if (GameFlag_GetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN) != 0) {
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && GameFlag_GetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
                    rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rnd;
                    if (((rnd >> 16) & 7) == 0) {
                        Gp_SpawnEff(EFFECT_FLASH_BURST, NULL, 0x400, &D_neo_ark_power_plant_2_80180678);
                    }
                }
            } else {
                glowDrawDisc(&D_neo_ark_power_plant_2_80180678, 0x300, 0x334);
            }
            break;
        case 8:
            lightSlot                                          = &gWorldCoordTransientPointLights[4];
            lightSlot->framesLeft                              = 4;
            pointLight                                         = &lightSlot->light;
            pointLight->inner                                  = 0x400;
            pointLight->outer                                  = 0x4000;
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            rnd                                                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState                                    = rnd;
            intensity                                          = ((rnd >> 16) & 0x700) + 0x800;
            pointLight->head.color.b                           = intensity;
            pointLight->head.color.r                           = intensity >> 1;
            pointLight->head.color.g                           = intensity >> 1;
            pointLight->head.transform.lighting.local.t[0]     = D_neo_ark_power_plant_2_80180668.vx;
            pointLight->head.transform.lighting.local.t[1]     = D_neo_ark_power_plant_2_80180668.vy;
            pointLight->head.transform.lighting.local.t[2]     = D_neo_ark_power_plant_2_80180668.vz;
            break;
    }
}

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_power_plant_2_8017DDF4(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_power_plant_2_8017E858(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_power_plant_2_8017F140(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

/// Sets `field_4` of the third sprite command in the sixth record of the
/// current area's entry in the current stage's sprite table to the low byte of
/// `arg0`; values other than 0 and 1 leave it unchanged.
void func_neo_ark_power_plant_2_8017FD88(s32 arg0)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;
    s32              mode;

    batches = Gp_SprtTables[sess->stage - 1][0].areaViews[sess->area - 1][5].batches;
    mode    = arg0 & 0xFF;
    if (mode == 0) {
        batches[2].hidden = 0;
    } else if (mode == 1) {
        batches[2].hidden = 1;
    }
}
