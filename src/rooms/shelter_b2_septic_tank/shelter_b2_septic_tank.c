#include "rooms/shelter_b2_septic_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
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

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/water_effects.h"
#include "../../shared/room_events.h"

/// The room's message table, installed by its first task state.
extern TaskMessageEntry D_shelter_b2_septic_tank_80182F4C[];
extern EvsCommand       D_shelter_b2_septic_tank_80183004[];
extern EvsCommand       D_shelter_b2_septic_tank_8018310C[];

/// Tasks the room's first task state spawns.
extern TaskDesc D_shelter_b2_septic_tank_801832C0[];

/// The room's water surfaces, as two lists drawn by separate functions.
extern RoomWaterSurface D_shelter_b2_septic_tank_801832CC[];
extern RoomWaterSurface D_shelter_b2_septic_tank_801832F0[];

extern SVECTOR D_shelter_b2_septic_tank_80183314[];
extern SVECTOR D_shelter_b2_septic_tank_80183344[];
extern SVECTOR D_shelter_b2_septic_tank_80183374[];
extern SVECTOR D_shelter_b2_septic_tank_80183514[];
extern SVECTOR D_shelter_b2_septic_tank_80183524[];
extern SVECTOR D_shelter_b2_septic_tank_80183534[];

/// The two points, relative to the effect's parent coordinate, that the trail
/// effect's two edges follow. The second is also read by its own name.

extern u8 D_shelter_b2_septic_tank_80187045;

static void waterDrawWaveStrips(Task* task);
static void waterDrawWaveStrips2(Task* task);
static void func_shelter_b2_septic_tank_8017EAB8(Task* arg0);
static void func_shelter_b2_septic_tank_8017EAF8(Task* task);

extern TaskDesc         D_shelter_b2_septic_tank_80182F40;
extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern u8               D_shelter_b2_septic_tank_80187044;
extern RoomLatchedEvent gRoomEventLatched;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b2_septic_tank_80187054;

void func_shelter_b2_septic_tank_8017EA50(Task*);

extern AnimationPlayRequest     D_shelter_b2_septic_tank_80182F80;
extern AnimationPlayRequest     D_shelter_b2_septic_tank_80182FC0;
extern ActorCommand             D_shelter_b2_septic_tank_80182FA0;
extern ActorCommand             D_shelter_b2_septic_tank_80182FA4;
extern ActorCommand             D_shelter_b2_septic_tank_80182FA8;
extern AnimationBankCopyRequest D_shelter_b2_septic_tank_80182F78;
extern ActorTransform           D_shelter_b2_septic_tank_80182FD4;
extern ActorTransform           D_shelter_b2_septic_tank_80182FEC;
void                            func_shelter_b2_septic_tank_8017D97C(s32);
void                            func_shelter_b2_septic_tank_8017D9A0(void);

extern TaskDesc D_actor_100400_80147E48;

s32 func_shelter_b2_septic_tank_8017D7AC(Task*, s32, s32, s32);
s32 func_shelter_b2_septic_tank_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b2_septic_tank_8017D904(Task*, s32, s32, s32);
s32 func_shelter_b2_septic_tank_8017D90C(Task*, s32, RoomEventMsg*, s32);

static AnimationPackedPose _gShelterB2SepticTankAnimation05958Bank1[6] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank1.inc"
};

static AnimationPackedRotation _gShelterB2SepticTankAnimation05958Bank4[64] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank4.inc"
};

static AnimationRecord _gShelterB2SepticTankAnimation05958Records[141] = {
#include "assets/shelter_b2_septic_tank_animation_05958_records.inc"
};

static u16 _gShelterB2SepticTankAnimation05958Indices[20] = {
#include "assets/shelter_b2_septic_tank_animation_05958_indices.inc"
};

static AnimationSet _gShelterB2SepticTankAnimation05958 = {
    _gShelterB2SepticTankAnimation05958Records,
    _gShelterB2SepticTankAnimation05958Indices,
    { NULL, _gShelterB2SepticTankAnimation05958Bank1, NULL, NULL, _gShelterB2SepticTankAnimation05958Bank4, NULL, NULL, NULL },
};

TaskDesc D_shelter_b2_septic_tank_80182F40 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_septic_tank_80182F4C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_septic_tank_8017D7B4 },
    { 5105, func_shelter_b2_septic_tank_8017D7AC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_septic_tank_8017D90C },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_septic_tank_8017D904 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_shelter_b2_septic_tank_80182F74[1] = {
    &_gShelterB2SepticTankAnimation05958,
};

AnimationBankCopyRequest D_shelter_b2_septic_tank_80182F78 = { { .sets = D_shelter_b2_septic_tank_80182F74 }, ARRAY_SIZE(D_shelter_b2_septic_tank_80182F74) };

AnimationPlayRequest D_shelter_b2_septic_tank_80182F80 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_shelter_b2_septic_tank_80182F94[3] = {
    { { .loc = { 4, 34 } }, 0 },
    { { .loc = { 4, 34 } }, 1 },
    { { .loc = { 4, 34 } }, 2 },
};

ActorCommand D_shelter_b2_septic_tank_80182FA0 = { { .loc = { 4, 34 } }, 3 };

ActorCommand D_shelter_b2_septic_tank_80182FA4 = { { .loc = { 4, 34 } }, 4 };

ActorCommand D_shelter_b2_septic_tank_80182FA8 = { { .loc = { 4, 34 } }, 5 };

AnimationPlayRequest D_shelter_b2_septic_tank_80182FAC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b2_septic_tank_80182FC0 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_shelter_b2_septic_tank_80182FD4 = { { 256, 0, -6802, 0 }, { 0, -1536, 0, 0 } };

ActorTransform D_shelter_b2_septic_tank_80182FEC = { 0 };

EvsCommand D_shelter_b2_septic_tank_80183004[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b2_septic_tank_80182F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_septic_tank_80182F80 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_shelter_b2_septic_tank_80182FA0 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b2_septic_tank_8017D97C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b2_septic_tank_8018310C[18] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_septic_tank_80182FC0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b2_septic_tank_8017D9A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_shelter_b2_septic_tank_80182FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_shelter_b2_septic_tank_80182FA4 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_shelter_b2_septic_tank_80182FA8 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b2_septic_tank_80182FD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b2_septic_tank_8017D97C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s16 D_shelter_b2_septic_tank_801832BC = 150;

TaskDesc D_shelter_b2_septic_tank_801832C0[1] = {
    { { { TASK_BODY_NONE, 96 } }, func_shelter_b2_septic_tank_8017EA50, { .value = 0 } },
};

RoomWaterSurface D_shelter_b2_septic_tank_801832CC[3] = {
    { -3500, -0x34BC, 2500, 7000, 0 },
    { 1100, -0x34BC, 2400, 7000, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomWaterSurface D_shelter_b2_septic_tank_801832F0[3] = {
    { -3500, -6500, 2500, 7000, 0 },
    { 1100, -6500, 2400, 7000, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

SVECTOR D_shelter_b2_septic_tank_80183314[6] = {
    { -813, -33, -0x31FC, 0 },
    { -813, -33, -0x3022, 0 },
    { -813, -33, -0x29AC, 0 },
    { -813, -33, -0x27DA, 0 },
    { -813, -33, -8572, 0 },
    { -813, -33, -8114, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183344[6] = {
    { -813, -33, -6503, 0 },
    { -813, -33, -6051, 0 },
    { -813, -33, -4335, 0 },
    { -813, -33, -3968, 0 },
    { -813, -33, -2372, 0 },
    { -813, -33, -1929, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183374[52] = {
    { -1941, -33, -404, 0 },
    { -1941, -33, 38, 0 },
    { 813, -33, -0x31FC, 0 },
    { 813, -33, -0x3022, 0 },
    { 813, -33, -0x29AC, 0 },
    { 813, -33, -0x27DA, 0 },
    { 813, -33, -8572, 0 },
    { 813, -33, -8114, 0 },
    { 813, -33, -6503, 0 },
    { 813, -33, -6051, 0 },
    { 813, -33, -4335, 0 },
    { 813, -33, -3968, 0 },
    { 813, -33, -2372, 0 },
    { 813, -33, -1929, 0 },
    { 1962, -33, -404, 0 },
    { 1962, -33, 38, 0 },
    { -3238, -3345, -0x32B1, 0 },
    { -3238, -3345, -0x2F8D, 0 },
    { -3238, -3345, -0x2CF9, 0 },
    { -3238, -3345, -0x29D5, 0 },
    { -3238, -3345, -9590, 0 },
    { -3238, -3345, -8678, 0 },
    { -3238, -3345, -7933, 0 },
    { -3238, -3345, -7018, 0 },
    { -3238, -3345, -5910, 0 },
    { -3238, -3345, -5104, 0 },
    { -3238, -3345, -4443, 0 },
    { -3238, -3345, -3639, 0 },
    { -3238, -3345, -2527, 0 },
    { -3238, -3345, -1613, 0 },
    { -3238, -3345, -868, 0 },
    { -3238, -3345, 42, 0 },
    { 3238, -3345, -0x32B1, 0 },
    { 3238, -3345, -0x2F8D, 0 },
    { 3238, -3345, -0x2CF9, 0 },
    { 3238, -3345, -0x29D5, 0 },
    { 3238, -3345, -9590, 0 },
    { 3238, -3345, -8678, 0 },
    { 3238, -3345, -7933, 0 },
    { 3238, -3345, -7018, 0 },
    { 3238, -3345, -5910, 0 },
    { 3238, -3345, -5104, 0 },
    { 3238, -3345, -4443, 0 },
    { 3238, -3345, -3639, 0 },
    { 3238, -3345, -2527, 0 },
    { 3238, -3345, -1613, 0 },
    { 3238, -3345, -868, 0 },
    { 3238, -3345, 42, 0 },
    { -2971, -2126, -0x352D, 0 },
    { -2223, -2126, -0x352D, 0 },
    { 2153, -2126, -0x352D, 0 },
    { 2891, -2126, -0x352D, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183514[2] = {
    { -2739, -2387, 577, 0 },
    { -1999, -2387, 577, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183524[2] = {
    { 1527, -2387, 577, 0 },
    { 2265, -2387, 577, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183534[5] = {
    { -368, -2420, -105, 0 },
    { 370, -2420, -105, 0 },
    { 29, -3157, -0x34E8, 0 },
    { 407, -4428, -0x34E8, 0 },
    { -2557, -2725, -0x34E8, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b2_septic_tank_8018356C[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_septic_tank_80183570[1] = { 6 };

DirectionWarpEntry D_shelter_b2_septic_tank_80183574[2] = {
    { { { .word = 0 }, -89, 0, -0x3395 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 400, 0, -0x3106 }, { 0, 0, 0, 0 }, 0x54220002, 0x54220001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, -89, 0, -112 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 800, 0, -300 }, { 0, 0, 0, 0 }, 0x54220004, 0x54220003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_801835E4[8] = {
    { -1700, 1500, -6500, 0 },
    { 1700, 1500, -9000, 0 },
    { -1700, 1500, -9000, 0 },
    { -1700, 1500, -3900, 0 },
    { 1700, 1500, -6500, 0 },
    { -1700, 1500, -9000, 0 },
    { 1700, 1500, -9000, 0 },
    { 1700, 1500, -3900, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_80183624[8] = {
    { 1700, 3000, -8000, 0 },
    { -1700, 3000, -5500, 0 },
    { -1700, 3000, -0x2AF8, 0 },
    { 1700, 3000, -5500, 0 },
    { -1700, 3000, -5500, 0 },
    { 1700, 3000, -0x2AF8, 0 },
    { -1700, 3000, -8000, 0 },
    { 1700, 3000, -5500, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_80183664[8] = {
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -3000, 0 },
    { -1700, 2200, -3000, 0 },
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -0x2710, 0 },
    { -1700, 2200, -0x2710, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_shelter_b2_septic_tank_801836A4[4] = {
    D_shelter_b2_septic_tank_801835E4,
    D_shelter_b2_septic_tank_80183624,
    D_shelter_b2_septic_tank_80183664,
    D_shelter_b2_septic_tank_80183624,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_801836B4[12] = {
    { 0, 0, 0, 0 },
    { -1700, 0, -3900, 0 },
    { 1700, 0, -3900, 0 },
    { -1700, 0, -6000, 0 },
    { 1700, 0, -6000, 0 },
    { -1700, 0, -8000, 0 },
    { 1700, 0, -8000, 0 },
    { -1700, 0, -0x2904, 0 },
    { 1700, 0, -0x2904, 0 },
    { -1700, 0, -0x2FA8, 0 },
    { 1700, 0, -0x2FA8, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gShelterB2SepticTankCollision0684CNormals[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_normals.inc"
};

static SVECTOR _gShelterB2SepticTankCollision0684CVerts[82] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_verts.inc"
};

static WorldCollisionGridFace _gShelterB2SepticTankCollision0684CFaces[31] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_faces.inc"
};

static s16 _gShelterB2SepticTankCollision0684CCells[270] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2SepticTankCollision0684CCells[i])
static s16* _gShelterB2SepticTankCollision0684CTable[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_septic_tank_80183E0C = { NULL, _gShelterB2SepticTankCollision0684CNormals, _gShelterB2SepticTankCollision0684CVerts, _gShelterB2SepticTankCollision0684CFaces, _gShelterB2SepticTankCollision0684CTable, 5374, 0x47A9, 3, 6, 4000, 31 };

ViewCamera D_shelter_b2_septic_tank_80183E30[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 34, 0x510F, 6334 } }, 230 },
    { { { { -4042, 0, -661 }, { -25, 4093, 153 }, { 661, 155, -4039 } }, { 821, 1121, 8316 } }, 275 },
    { { { { -4032, 0, -719 }, { 3, 4095, -19 }, { 719, -19, -4032 } }, { 792, 1135, 3708 } }, 269 },
    { { { { 4049, 0, -613 }, { -63, 4074, -418 }, { 609, 422, 4028 } }, { 750, 1507, 9189 } }, 269 },
    { { { { 3860, 0, -1369 }, { -607, 3671, -1712 }, { 1227, 1816, 3459 } }, { 1731, 2887, 3765 } }, 257 },
    { { { { -4024, 0, 763 }, { -152, 4013, -804 }, { -748, -819, -3942 } }, { 1196, 493, 5408 } }, 297 },
};

SpriteBatch D_shelter_b2_septic_tank_80183F08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_septic_tank_80183F18[83] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 112, 525, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 104, 535, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 96, 534, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 88, 574, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 80, 794, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 890, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 64, 937, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 56, 1000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 48, 1048, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 40, 1092, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 112, 674, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 104, 662, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, 104, 667, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 96, 678, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 96, 662, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 88, 666, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 88, 670, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 32, 1335, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 1314, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 1247, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 48, 1264, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1217, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1039, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 56, 1108, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 80, 734, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 858, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 799, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 72, 947, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 64, 879, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 64, 1036, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 64, 1025, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 56, 1037, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 48, 1176, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 40, 1460, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 1408, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1050, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 48, 1125, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 40, 1370, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 112, 895, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 112, 895, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 112, 911, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -104, 104, 936, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 104, 936, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 104, 936, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 96, 983, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 983, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 96, 983, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 88, 1036, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 88, 1036, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 88, 1036, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 80, 1099, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 1083, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 80, 1099, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 72, 1173, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 72, 1173, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 72, 1157, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1247, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 64, 1247, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 56, 1340, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 56, 1340, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 48, 1428, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1444, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 40, 1564, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1386, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 112, 788, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 104, 818, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 96, 864, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 88, 936, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 80, 996, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 72, 1057, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 64, 1141, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 56, 1242, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1363, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -152, 112, 783, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -144, 104, 816, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -136, 96, 869, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -120, 88, 931, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, 80, 992, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 72, 1056, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 1130, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 56, 1238, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 48, 1235, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_septic_tank_80184594[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 3, 0 } },
    { 30, 9, 0, 0, { 0, 0 } },
    { 39, 26, 0, 0, { 2, 0 } },
    { 65, 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_septic_tank_801845C4[127] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 64, 1575, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 1575, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 72, 1575, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 80, 1575, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 88, 1575, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 96, 1575, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 96, 1575, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 112, 666, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 104, 709, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 96, 760, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 88, 819, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 80, 890, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 72, 979, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 64, 1098, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 56, 1200, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 48, 1392, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 40, 1632, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 1964, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 2363, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 24, 2528, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 2382, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 32, 1928, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 40, 1953, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1632, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 48, 1589, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 48, 1367, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1351, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 1216, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 64, 1226, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 64, 1053, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 72, 1113, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 72, 951, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 80, 956, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 915, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 88, 895, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 88, 844, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 96, 800, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 96, 801, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 104, 754, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 104, 759, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 112, 717, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 112, 700, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 24, 2070, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 24, 2139, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 80, 1278, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 72, 1164, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 64, 1165, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 56, 1165, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 56, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 40, 0, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 48, 1352, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 1240, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 1224, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 40, 1352, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 32, 0, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 24, 1988, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 4569, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1605, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 40, 1604, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 48, 1604, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 1604, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 1988, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 40, 1988, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1987, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 0, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -96, 112, 917, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 24, 2631, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 32, 2384, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -32, 112, 917, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -80, 104, 966, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 104, 966, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -72, 96, 1023, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 96, 1039, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -64, 88, 1090, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 88, 1090, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 80, 1170, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 80, 1170, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -48, 72, 1267, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 72, 1267, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 1386, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 64, 1386, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 56, 1538, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 56, 1538, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 48, 1736, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 48, 1736, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 40, 1991, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -128, 112, 890, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 104, 927, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 24, 2636, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 32, 2269, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1982, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1710, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 56, 1505, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1352, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 72, 1231, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -80, 80, 1138, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 88, 1052, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, 96, 991, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 2472, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 2175, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 40, 1987, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1696, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1489, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 64, 1352, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 72, 1231, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 80, 1140, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 88, 1036, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 96, 991, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 104, 933, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 112, 896, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 72, 1446, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 64, 1500, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 56, 1372, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 48, 1372, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 40, 1372, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 32, 1372, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 24, 1373, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 24, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 1750, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 32, 1750, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 24, 1766, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 16, 1776, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 8, 1767, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 0, 4276, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 64, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 56, 1940, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_septic_tank_80184FB0[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 6, 0 } },
    { 7, 35, 0, 0, { 1, 0 } },
    { 42, 2, 0, 0, { 4, 0 } },
    { 44, 21, 0, 0, { 0, 0 } },
    { 65, 21, 0, 0, { 5, 0 } },
    { 86, 12, 0, 0, { 3, 0 } },
    { 98, 12, 0, 0, { 7, 0 } },
    { 110, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_septic_tank_80185000[144] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 3599, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, 64, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 3202, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 48, 1612, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 48, 1691, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 96, 1065, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 96, 1102, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1603, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 1110, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 112, 1111, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 80, 1640, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 72, 1650, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 64, 1613, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 112, 676, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 104, 714, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 96, 756, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 804, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 80, 858, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 72, 922, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 1000, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 1242, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 48, 1233, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 1328, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 1325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 24, 1687, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 1923, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 16, 2027, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 8, 2385, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 2014, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 72, 1302, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 64, 1285, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1250, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 48, 1246, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1243, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 24, 3770, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 8, 3764, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 0, 4069, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 1574, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 0, 1985, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 16, 1994, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 24, 2000, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1594, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 32, 1567, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 32, 2118, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 40, 1683, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 48, 1710, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 8, 2377, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 16, 2086, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 8, 2406, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 16, 2008, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 16, 2086, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 8, 2481, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -24, 112, 784, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, 112, 784, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 24, 104, 826, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 104, 826, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 96, 872, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 96, 856, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 88, 908, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 88, 908, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 16, 80, 968, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 80, 968, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 72, 1037, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 72, 1037, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 64, 1117, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 64, 1117, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 56, 1212, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 56, 1212, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1325, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1325, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 40, 1464, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 40, 1464, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 32, 1636, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 32, 1636, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 24, 1873, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 8, 8, 4590, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -32, 8, 4772, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -88, 8, 4772, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 112, 807, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 104, 832, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 56, 96, 884, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 88, 944, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 80, 993, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 1063, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, 64, 1149, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 56, 1241, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, 48, 1367, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 40, 1513, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 32, 1666, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 24, 1908, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 16, 2069, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 64, 1488, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 56, 1354, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 48, 1335, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 40, 1881, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 32, 1748, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 24, 1743, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 16, 1771, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 8, 1782, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 40, 1324, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 32, 1368, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 24, 1364, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 48, 1085, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 8, 1157, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 2022, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 24, 1874, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1658, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 40, 1479, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 48, 1337, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1222, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 64, 1128, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 72, 1058, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 80, 976, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 88, 937, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 877, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 104, 817, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 112, 787, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 112, 811, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 96, 104, 833, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 128, 112, 724, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 104, 762, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 96, 865, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 96, 804, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 96, 88, 852, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 88, 937, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, 80, 1004, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 80, 906, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 72, 970, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1087, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 64, 1153, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, 64, 1043, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 56, 1130, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 56, 1267, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 48, 1297, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1448, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 40, 1566, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, 40, 1360, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1474, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, 32, 1786, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 24, 1991, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1691, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 16, 2024, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 0, 2461, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 24, 16, 2172, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_septic_tank_80185B40[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 16, 0, 0, { 5, 0 } },
    { 29, 17, 0, 0, { 4, 0 } },
    { 46, 6, 0, 0, { 6, 0 } },
    { 52, 26, 0, 0, { 1, 0 } },
    { 78, 13, 0, 0, { 7, 0 } },
    { 91, 13, 0, 0, { 3, 0 } },
    { 104, 13, 0, 0, { 8, 0 } },
    { 117, 27, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_septic_tank_80185B98[143] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 810, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 136, 112, 887, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 104, 885, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 104, 915, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 96, 952, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 136, 96, 915, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 112, 842, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 104, 869, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 96, 899, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 88, 898, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 916, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 96, 939, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 88, 946, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 80, 980, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 88, 985, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 80, 1026, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 112, 903, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 112, 876, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 104, 837, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 104, 849, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 104, 914, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 96, 870, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 96, 851, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 96, 931, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 88, 1042, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 80, 1057, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 72, 1050, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 64, 1040, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 1116, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 48, 1161, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 40, 1196, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1204, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 24, 1238, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 88, 883, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 878, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 88, 0, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 1076, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 72, 1016, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 64, 1046, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1105, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 56, 1086, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 48, 1117, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 56, 1121, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 56, 1162, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 1213, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1129, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 1156, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1260, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 1177, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 32, 1308, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 32, 1316, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1406, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 16, 0, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 16, 1480, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 16, 1316, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 8, 1580, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 8, 1490, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 0, 1553, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -8, 1525, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 24, 1325, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 112, 939, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 104, 961, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 96, 991, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 88, 1486, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 80, 1062, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 72, 1088, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 64, 1140, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 112, 932, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 104, 1561, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 96, 1006, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 88, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 80, 1060, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 96, 999, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 88, 1020, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 88, 986, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 80, 1056, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 80, 1021, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 112, 928, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 112, 928, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 104, 957, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, 104, 957, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 96, 987, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 56, 96, 987, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 56, 88, 1020, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 88, 1020, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 80, 1056, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 80, 1056, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 72, 1062, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 72, 1062, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 72, 1062, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 64, 1135, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1135, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -40, 56, 1180, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 1275, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1228, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 40, 1234, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 32, 1302, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 32, 1326, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 32, 1341, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 32, 1325, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 32, 1293, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 24, 1312, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 24, 1397, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 24, 1389, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 16, 16, 1469, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 16, 1428, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 8, 1479, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 40, 1202, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 40, 1234, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1373, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 1389, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 1413, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 16, 1429, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 1468, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 8, 1516, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 1213, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1229, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1164, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 56, 1180, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 40, 1282, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 40, 1282, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 1325, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1341, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 56, 1180, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 56, 1200, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 56, 1180, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1180, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1229, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 48, 1229, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 48, 1229, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 48, 1245, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 48, 1229, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 48, 1229, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 64, 1135, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 64, 1135, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 64, 1119, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 64, 1135, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 1078, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 72, 1094, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 80, 1056, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 80, 1040, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 48, 1226, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1180, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_septic_tank_801866C4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 60, 0, 0, { 1, 0 } },
    { 60, 83, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_septic_tank_801866E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_septic_tank_801866F4[6] = {
    { { .empty = D_shelter_b2_septic_tank_80183F08 }, D_shelter_b2_septic_tank_80183F08, NULL },
    { { .elements = D_shelter_b2_septic_tank_80183F18 }, D_shelter_b2_septic_tank_80184594, NULL },
    { { .elements = D_shelter_b2_septic_tank_801845C4 }, D_shelter_b2_septic_tank_80184FB0, NULL },
    { { .elements = D_shelter_b2_septic_tank_80185000 }, D_shelter_b2_septic_tank_80185B40, NULL },
    { { .elements = D_shelter_b2_septic_tank_80185B98 }, D_shelter_b2_septic_tank_801866C4, NULL },
    { { .empty = D_shelter_b2_septic_tank_801866E4 }, D_shelter_b2_septic_tank_801866E4, NULL },
};

WorldCoordPointLight D_shelter_b2_septic_tank_8018673C[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 20, -41, -4221 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 941, 2662 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 44, -41, -6238 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 961, 2502 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -21, -41, -8592 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1121, 2522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 31, -41, -0x28AE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 921, 2502 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -18, -41, -0x3112 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1061, 2542 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -499, -7288 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2846, 2876, 2873 }, { 0, 0 } }, 1501, 0x359B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1441, -388 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3038, 1650, 1695 }, { 0, 0 } }, 1961, 4841 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 78, -41, -2140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 861, 4123 },
};

WorldCoordRoomLights D_shelter_b2_septic_tank_80186A3C = { 0, NULL, ARRAY_SIZE(D_shelter_b2_septic_tank_8018673C), D_shelter_b2_septic_tank_8018673C, 0, NULL };

WorldCollisionTrigger D_shelter_b2_septic_tank_80186A54[6] = {
    { NULL, NULL, NULL, { -48, -1872, -1809, 0 }, { { 2576, -3904, -176, 0 }, { -2576, -3904, 176, 0 }, { 2576, 3904, -176, 0 }, { -2576, 3904, 176, 0 } }, { 279, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 80, -1985, -1712, 0 }, { { -2400, -3904, 192, 0 }, { 2400, -3904, -192, 0 }, { -2400, 3904, 192, 0 }, { 2400, 3904, -192, 0 } }, { -328, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1889, -6671, 0 }, { { 2576, -3904, 0, 0 }, { -2576, -3904, 1, 0 }, { 2576, 3904, 0, 0 }, { -2576, 3904, 1, 0 } }, { 0, 0, 4118, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1953, -6463, 0 }, { { -2400, -3904, 0, 0 }, { 2400, -3904, 0, 0 }, { -2400, 3904, 0, 0 }, { 2400, 3904, 0, 0 } }, { 0, 0, -4118, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1920, -0x2BD1, 0 }, { { 2576, -3904, 336, 0 }, { -2576, -3904, -335, 0 }, { 2576, 3904, 336, 0 }, { -2576, 3904, -335, 0 } }, { -531, 0, 4072, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1952, -0x2B40, 0 }, { { -2400, -3904, -320, 0 }, { 2400, -3904, 320, 0 }, { -2400, 3904, -320, 0 }, { 2400, 3904, 320, 0 } }, { 542, 0, -4066, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_septic_tank_80186C1C[4] = {
    { NULL, NULL, NULL, { 0, -48, -0x3290, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_WARP, 33, 22, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -48, 192, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_WARP, 35, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -1793, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -6080, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_septic_tank_80186D4C[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_septic_tank_80186D64[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_septic_tank_80186D7C[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_septic_tank_80186DA0[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_septic_tank_80186DB8[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 4, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_200400_8015FE48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_septic_tank_80186DDC[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 20, 20, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02000_D15FD0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_septic_tank_80186E00[3] = {
    { 4, 0, 4, -1952, 3000, -7000, 0, 0, 0, 2, 0 },
    { 4, 0, 5, 1792, 3000, -0x2E60, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_septic_tank_80186E30[3] = {
    { 4, 0, 1, 1700, 1500, -3900, 2500, 0, 0, 2, 0 },
    { 4, 0, 17, -1700, 3000, -5500, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_septic_tank_80186E60[3] = {
    { 4, 0, 33, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { 49, 1, 0, 300, 0, -5500, 2200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_septic_tank_80186E90[2] = {
    { 4, 0, 33, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_septic_tank_80186EB0[5] = {
    { 21, 4, 0, -700, -2400, 300, 2048, 0, 0, 2, 5 },
    { 21, 4, 0, 700, -2400, 300, 2048, 0, 0, 2, 5 },
    { 4, 0, 7, 1700, 0, -8300, 0, 0, 2, 4, 0 },
    { 4, 0, 7, -1800, 0, -8000, 1600, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_septic_tank_80186F00[4] = {
    { 21, 4, 0, -700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { 21, 4, 0, 700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { 20, 9, 1, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_septic_tank_80186F40[22] = {
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186E00, D_shelter_b2_septic_tank_80186D4C },
    { D_shelter_b2_septic_tank_80186E30, D_shelter_b2_septic_tank_80186D64 },
    { D_shelter_b2_septic_tank_80186E60, D_shelter_b2_septic_tank_80186D7C },
    { D_shelter_b2_septic_tank_80186E90, D_shelter_b2_septic_tank_80186DA0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186EB0, D_shelter_b2_septic_tank_80186DB8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186F00, D_shelter_b2_septic_tank_80186DDC },
};

WorldCollisionFootstepSounds D_shelter_b2_septic_tank_80186FF0 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b2_septic_tank_80186FFC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_septic_tank_80187004[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_septic_tank_8018700C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_septic_tank_80186FF0 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_septic_tank_80187014[8] = {
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80187004,
    D_shelter_b2_septic_tank_8018700C,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_septic_tank_80187044 = 0;

u8 D_shelter_b2_septic_tank_80187045 = 0;

u16 D_shelter_b2_septic_tank_80187046 = 0x5868;

RoomLatchedEvent gRoomEventLatched;

u8* D_shelter_b2_septic_tank_80187054;

static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_b2_septic_tank_8017DA18(Task* arg0);
static void           func_shelter_b2_septic_tank_8017DA74(Task* task);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_septic_tank_80187044 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_septic_tank_80182F40, 0, 0, 0);
            D_shelter_b2_septic_tank_80187044 = 1;
        }
        return 2;
    }
    return 1;
}

#include "../../shared/room_event_staged_task.inc.c"

s32 func_shelter_b2_septic_tank_8017D7AC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Message 0x21 starts the room's event on flag 0x131; any
/// other message answers 1.
s32 func_shelter_b2_septic_tank_8017D7B4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        return 1;
    }
    event.capCmd   = 3;
    event.stageSnd = 0x54220001;
    event.flagId   = GAME_FLAG_B2_SEPTIC_TANK_TO_CORRIDOR_SCENE;
    event.fade     = 0;
    return _shelterB2SepticTankStartEvent(out, &event);
}

s32 func_shelter_b2_septic_tank_8017D904(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b2_septic_tank_8017D90C(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    u8  kind;
    s32 flag;

    kind = arg2->warp;
    if (kind == 2) {
        flag = GameFlag_GetNibble(GAME_FLAG_0EB);
        if (flag == 1 && D_shelter_b2_septic_tank_80187045 == flag) {
            func_800E8614(D_shelter_b2_septic_tank_8018310C, 0);
            D_shelter_b2_septic_tank_80187045 = kind;
        }
    }
    return 0;
}

void func_shelter_b2_septic_tank_8017D97C(s32 arg0)
{
    GameFlag_SetNibble(GAME_FLAG_0EB, arg0);
}

void func_shelter_b2_septic_tank_8017D9A0(void)
{
    Task*     target;
    GfxCoord* player;
    GfxCoord* coords;

    target = Gp_LookupSlot4(0);
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    if (target != NULL) {
        coords = target->extra.tmd->coords;
        D_shelter_b2_septic_tank_80182FEC.rot.vy =
            (ratan2(coords->coord.t[0] - player->coord.t[0], coords->coord.t[2] - player->coord.t[2]) + 0x1000) & 0xFFF;
    }
}

static void func_shelter_b2_septic_tank_8017DA18(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_septic_tank_80182F4C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    Task_SpawnFromTable(D_shelter_b2_septic_tank_801832C0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_shelter_b2_septic_tank_8017DA74(Task* task)
{
    s32 place;

    if (gGameSession->location.loc.view == 4) {
        place = gGameSession->location.loc.variant;
        if (place == 1 && Gp_StateC08.mode != place && gDisplayState.pendingMode == DISPLAY_MODE_NONE && D_shelter_b2_septic_tank_80187045 == 0) {
            if (GameFlag_GetNibble(GAME_FLAG_0EB) == 0) {
                func_800E8614(D_shelter_b2_septic_tank_80183004, 0);
            }
            D_shelter_b2_septic_tank_80187045 = place;
        }
    }
}

/// The room task's state table, dispatched by
/// `func_shelter_b2_septic_tank_8017DB10` from a stack copy.
static const TaskFuncTable3 D_shelter_b2_septic_tank_8017D5D8 = {
    {
        func_shelter_b2_septic_tank_8017DA18,
        func_shelter_b2_septic_tank_8017DA74,
        taskKill,
    },
};

/// The room task: copies the three-state table `D_shelter_b2_septic_tank_8017D5D8`
/// onto the stack and runs the entry for the task's current state.
void func_shelter_b2_septic_tank_8017DB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_septic_tank_8017D5D8;
    sp.funcs[task->state](task);
}

#define WATER_WAVE_STRIPS_SURFACES    D_shelter_b2_septic_tank_801832CC
#define WATER_WAVE_STRIPS_HEIGHT      D_shelter_b2_septic_tank_801832BC
#define WATER_WAVE_STRIPS_PRIM_CURSOR D_shelter_b2_septic_tank_80187054
/// Scales the seam's sine displacement to -128..128 world-coordinate Y units.
///
/// Integer shift count for `water_wave_strips.inc.c`; see its configuration
/// contract. The include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 5
/// Sets the first X strip's blue vertex colours for subtractive blending.
static inline void _shelterB2SepticTankSetFirstWaterStripColours(POLY_G4* quad)
{
    setRGB0(quad, 0, 0x40, 0x80);
    setRGB1(quad, 0, 0x40, 0x80);
    setRGB2(quad, 0, 0x10, 0x20);
    setRGB3(quad, 0, 0x10, 0x20);
}

/// Binds the first X strip's blue vertex colours for subtractive blending.
///
/// A writable `POLY_G4*` is evaluated once. The flat outer edge (vertices
/// 0/1) is (0, 0x40, 0x80); the waving seam (vertices 2/3) is (0, 0x10, 0x20),
/// in unsigned RGB bytes. Other packet fields are preserved; no caller locals
/// are captured and no packet pointer is retained. Psy-Q's `POLY_G4` and
/// `setRGB0` through `setRGB3` must be visible. `water_wave_strips.inc.c`
/// consumes and undefines this override.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _shelterB2SepticTankSetFirstWaterStripColours(quad)
/// Sets the second X strip's blue vertex colours for subtractive blending.
static inline void _shelterB2SepticTankSetSecondWaterStripColours(POLY_G4* quad)
{
    setRGB0(quad, 0, 0x40, 0x80);
    setRGB1(quad, 0, 0x40, 0x80);
    setRGB2(quad, 0, 0x10, 0x20);
    setRGB3(quad, 0, 0x10, 0x20);
}

/// Binds the second X strip's blue vertex colours for subtractive blending.
///
/// A writable `POLY_G4*` is evaluated once. The seam (vertices 0/1) is
/// (0, 0x40, 0x80); the outer edge (vertices 2/3) is (0, 0x10, 0x20), in
/// unsigned RGB bytes. Other packet fields are preserved; no caller locals
/// are captured. Psy-Q's `POLY_G4` and `setRGB0` through `setRGB3` must be
/// visible. `water_wave_strips.inc.c` consumes and undefines this override.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _shelterB2SepticTankSetSecondWaterStripColours(quad)
#include "../../shared/water_wave_strips.inc.c"

#define WATER_WAVE_STRIPS_FUNC        waterDrawWaveStrips2
#define WATER_WAVE_STRIPS_SURFACES    D_shelter_b2_septic_tank_801832F0
#define WATER_WAVE_STRIPS_HEIGHT      D_shelter_b2_septic_tank_801832BC
#define WATER_WAVE_STRIPS_PRIM_CURSOR D_shelter_b2_septic_tank_80187054
/// Rebinds the second surface list's seam displacement to -128..128 Y units.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 5
/// Rebinds the second surface list to the same outer-edge-to-seam blue palette.
///
/// Has the same `POLY_G4*`, single-evaluation and packet-preservation contract
/// as the first instance; the preceding include has undefined that binding.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _shelterB2SepticTankSetFirstWaterStripColours(quad)
/// Rebinds the second surface list to the same seam-to-edge blue palette.
///
/// Has the same `POLY_G4*`, single-evaluation and packet-preservation contract
/// as the first instance; the preceding include has undefined that binding.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _shelterB2SepticTankSetSecondWaterStripColours(quad)
#include "../../shared/water_wave_strips.inc.c"

/// The water task: runs its current state - `func_shelter_b2_septic_tank_8017EAB8`
/// once, then `func_shelter_b2_septic_tank_8017EAF8`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b2_septic_tank_8017EA50(Task* task)
{
    TaskFunc states[2] = { func_shelter_b2_septic_tank_8017EAB8, func_shelter_b2_septic_tank_8017EAF8 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b2_septic_tank_801832BC;
}

/// Clears the session's `field_80` or `field_7E`, chosen by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, and
/// advances the task to its next state.
static void func_shelter_b2_septic_tank_8017EAB8(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b2_septic_tank_80187054` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, then draws both
/// lists of water surfaces.
static void func_shelter_b2_septic_tank_8017EAF8(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_shelter_b2_septic_tank_80187054 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b2_septic_tank_80187054 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    waterDrawWaveStrips(task);
    waterDrawWaveStrips2(task);
}

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gRoomEffectFlashId       = EFFECT_SHELTER_B2_SEPTIC_TANK_FLASH;
            gRoomEffectTwinTrailId   = EFFECT_SHELTER_B2_SEPTIC_TANK_TWIN_TRAIL;
            gRoomEffectSparkBurstId  = EFFECT_SHELTER_B2_SEPTIC_TANK_SPARK_BURST;
            gRoomEffectWaterRippleId = EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_SPRAY;
            arg0->state              = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    glowDrawBeam(&p[0], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[2], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[14], 0x200, 0, 0x111);
                    glowDrawBeam(&p[16], 0x200, 0, 0x111);
                    glowDrawBeam(&p[60], 0x200, 0, 0x111);
                    glowDrawBeam(&p[62], 0x200, 0, 0x111);
                    glowDrawBitDisc(&p[70], 0x300, 0x10);
                    glowDrawBitDisc(&p[72], 0x300, 0x100);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    glowDrawBeam(&p[0], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[2], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[4], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[6], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[14], 0x200, 0, 0x111);
                    glowDrawBeam(&p[16], 0x200, 0, 0x111);
                    glowDrawBeam(&p[18], 0x200, 0, 0x111);
                    glowDrawBeam(&p[20], 0x200, 0, 0x111);
                    glowDrawBeam(&p[28], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[30], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[44], 0x200, 0, 0x111);
                    glowDrawBeam(&p[46], 0x200, 0, 0x111);
                    glowDrawBeam(&p[48], 0x200, 0, 0x111);
                    glowDrawBeam(&p[60], 0x200, 0, 0x111);
                    glowDrawBeam(&p[62], 0x200, 0, 0x111);
                    glowDrawBitDisc(&p[70], 0x300, 0x10);
                    glowDrawBitDisc(&p[71], 0x300, 0x100);
                    glowDrawBitDisc(&p[72], 0x300, 0x100);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183344;
                    glowDrawBeam(&p[0], 0x200, -0x400, 0x111);
                    glowDrawBeam(&p[2], 0x200, -0x400, 0x111);
                    glowDrawBeam(&p[4], 0x200, -0x400, 0x111);
                    glowDrawBeam(&p[6], 0x200, -0x400, 0x111);
                    glowDrawBeam(&p[14], 0x200, 0, 0x111);
                    glowDrawBeam(&p[16], 0x200, 0, 0x111);
                    glowDrawBeam(&p[18], 0x200, 0, 0x111);
                    glowDrawBeam(&p[20], 0x200, 0, 0x111);
                    glowDrawBeam(&p[34], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[36], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[48], 0x200, 0, 0x111);
                    glowDrawBeam(&p[50], 0x200, 0, 0x111);
                    glowDrawBeam(&p[52], 0x200, 0, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 5: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183374;
                    glowDrawBeam(&p[0], 0x200, -0x400, 0x111);
                    glowDrawBeam(&p[14], 0x200, 0, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    glowDrawBeam(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 6: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    glowDrawBeam(&p[0], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[2], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[4], 0x200, 0x400, 0x111);
                    glowDrawBeam(&p[14], 0x200, 0, 0x111);
                    glowDrawBeam(&p[28], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[30], 0x200, 0x800, 0x111);
                    glowDrawBeam(&p[60], 0x200, 0, 0x111);
                    glowDrawBeam(&p[62], 0x200, 0, 0x111);
                    glowDrawBitDisc(&p[70], 0x300, 0x10);
                    glowDrawBitDisc(&p[71], 0x300, 0x100);
                    glowDrawBitDisc(&p[72], 0x300, 0x100);
                    break;
                }
            }
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b2_septic_tank_8017F040(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task.inc.c"

void func_shelter_b2_septic_tank_8017F4C8(Task* task)
{
    waterDriftTask(task);
}

#include "../../shared/water_spin.inc.c"

#include "../../shared/water_tile.inc.c"

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_bit_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b2_septic_tank_80180BE0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b2_septic_tank_80181644(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_septic_tank_80181F2C(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
