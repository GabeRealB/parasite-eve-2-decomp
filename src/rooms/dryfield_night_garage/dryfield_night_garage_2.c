#include "rooms/dryfield_night_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_garage_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

extern AreaApplyRec D_dryfield_night_garage_801875D8[];
extern AreaApplyRec D_dryfield_night_garage_80187620[];

/// The garage's two point-pair runs, 8-byte `SVECTOR`s laid back to back from
/// `801833A4`: four pairs the visit-3/15 case sweeps (`A4[0]`, `A4[2]`, `A4[4]`,
/// `A4[6]`), of which the last two are also what visit 11 sweeps from
/// `801833D4`. `801833D4` is named separately because visit 11 reaches it by
/// name where the visit-3/15 case reaches the same address as `A4[6]` -
/// indexing emits the base plus 0x30, naming it emits its own `lui`.
extern SVECTOR D_dryfield_night_garage_801833A4[];
extern SVECTOR D_dryfield_night_garage_801833D4;

extern AreaResource D_dryfield_night_garage_80187450[2];
extern AreaResource D_dryfield_night_garage_80187468[3];
extern AreaResource D_dryfield_night_garage_8018748C[2];
extern AreaResource D_dryfield_night_garage_801874A4[2];

extern TaskDesc D_actor_136300_8013B11C;

extern WorldCollisionGrid    D_dryfield_night_garage_801843D4[1];
extern WorldCollisionTrigger D_dryfield_night_garage_8018630C[14];
extern WorldCollisionTrigger D_dryfield_night_garage_80186734[12];

extern WorldCollisionTrigger      D_dryfield_night_garage_8018723C[7];
extern WorldCoordRoomAmbientEntry D_dryfield_night_garage_8018751C[16];
extern WorldCoordRoomLights       D_dryfield_night_garage_80186D64[1];

void func_dryfield_night_garage_80180B20(Task*);
void func_dryfield_night_garage_80180CEC(Task*);
void func_dryfield_night_garage_80180D4C(Task*);

void func_dryfield_night_garage_801809A4(Task*);
void func_dryfield_night_garage_80180AB0(void);

TaskDesc D_dryfield_night_garage_80182C98[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_garage_801809A4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_garage_801807E4, { .value = 0 } },
};

AnimationSet* D_dryfield_night_garage_80182CB0[5] = {
    NULL,
    &gDryfieldNightGarageAnimation04B80,
    &gDryfieldNightGarageAnimation04F54,
    &gDryfieldNightGarageAnimation05344,
    &gDryfieldNightGarageAnimation056B0,
};

AnimationBankCopyRequest D_dryfield_night_garage_80182CC4 = { { .sets = D_dryfield_night_garage_80182CB0 }, ARRAY_SIZE(D_dryfield_night_garage_80182CB0) };

AnimationPlayRequest D_dryfield_night_garage_80182CCC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182CE0 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182CF4 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182D08 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182D1C = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_garage_80182D30 = { { 5500, 0, 4500, 0 }, { 0, -1024, 0, 0 } };

AnimationPlayRequest D_dryfield_night_garage_80182D48[2] = {
    { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_garage_80182D70 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182D84 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182D98 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_garage_80182DAC = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_garage_80182DC0 = { { 4700, 0, 5000, 0 }, { 0, -1024, 0, 0 } };

ActorCommand D_dryfield_night_garage_80182DD8 = { { .loc = { 3, 24 } }, 0 };

ActorCommand D_dryfield_night_garage_80182DDC = { { .loc = { 3, 24 } }, 1 };

s32 D_dryfield_night_garage_80182DE0 = 0x21803;

s32 D_dryfield_night_garage_80182DE4 = 0x31803;

ActorCommand D_dryfield_night_garage_80182DE8 = { { .loc = { 3, 24 } }, 4 };

ActorCommand D_dryfield_night_garage_80182DEC = { { .loc = { 3, 24 } }, 5 };

EvsSceneKey D_dryfield_night_garage_80182DF0 = { 3, 55, 11 };

EvsCommand D_dryfield_night_garage_80182DF8[40] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_dryfield_night_garage_80182DF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_garage_80182CC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_garage_80182CE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_garage_80182D08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_garage_80182D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_garage_80182DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_garage_80182D70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_garage_80182DD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_garage_80182D84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_garage_80182DE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_garage_80182D98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_night_garage_80180414 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180AB0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_garage_801831B8[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_garage_80182D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_garage_80182DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_garage_80182DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_garage_80182DEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180984 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_night_garage_80180414 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_garage_80180AB0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_night_garage_80183380[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_garage_80180D4C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_garage_80180B20, { .value = 0 } },
};

TaskDesc D_dryfield_night_garage_80183398 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_garage_80180CEC, { .value = 0 } };

SVECTOR D_dryfield_night_garage_801833A4[6] = {
    { 1950, -3260, 6199, 0 },
    { 1950, -3260, 5142, 0 },
    { 4034, -3260, 6199, 0 },
    { 4034, -3260, 5142, 0 },
    { 1950, -3260, 2714, 0 },
    { 1950, -3260, 1662, 0 },
};

SVECTOR D_dryfield_night_garage_801833D4 = { 4034, -3260, 2714, 0 };

SVECTOR D_dryfield_night_garage_801833DC[3] = {
    { 4034, -3260, 1662, 0 },
    { 6940, -3260, 2714, 0 },
    { 6940, -3260, 1662, 0 },
};

WorldCoordRoomLighting D_dryfield_night_garage_801833F4[2] = {
    { D_dryfield_night_garage_80186D64, D_dryfield_night_garage_8018751C },
    { D_dryfield_night_garage_80186D64, D_dryfield_night_garage_8018751C },
};

WorldCollisionRoomResources D_dryfield_night_garage_80183404[2] = {
    { &D_dryfield_night_garage_80183DD4, D_dryfield_night_garage_8018630C, D_dryfield_night_garage_80186D7C, NULL },
    { D_dryfield_night_garage_801843D4, D_dryfield_night_garage_80186734, D_dryfield_night_garage_8018723C, NULL },
};

u8 D_dryfield_night_garage_80183424[16] = {
    1,
    14,
    15,
    4,
    5,
    14,
    14,
    8,
    9,
    10,
    11,
    12,
    2,
    7,
    3,
    0,
};

u8* D_dryfield_night_garage_80183434[2] = {
    gViewIdentityMap,
    D_dryfield_night_garage_80183424,
};

ViewCount D_dryfield_night_garage_8018343C[2] = { 15, 15 };

DirectionWarpEntry D_dryfield_night_garage_80183440[2] = {
    { { { .word = 1024 }, 853, 0, 2741 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 853, 0, 2741 }, { 0, 0, 0, 0 }, 0x53180004, 0x53180003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 473 },
    { { { .word = 2048 }, 912, 0, 7143 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 912, 0, 7143 }, { 0, 0, 0, 0 }, 0x53180002, 0x53180001, 0x53180007, 6, DIRECTION_WARP_FLAG_NONE, 472 },
};

static SVECTOR _gDryfieldNightGarageCollision06814Normals[40] = {
#include "assets/dryfield_night_garage_collision_06814_normals.inc"
};

static SVECTOR _gDryfieldNightGarageCollision06814Verts[110] = {
#include "assets/dryfield_night_garage_collision_06814_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageCollision06814Faces[55] = {
#include "assets/dryfield_night_garage_collision_06814_faces.inc"
};

static s16 _gDryfieldNightGarageCollision06814Cells[222] = {
#include "assets/dryfield_night_garage_collision_06814_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightGarageCollision06814Cells[i])
static s16* _gDryfieldNightGarageCollision06814Table[9] = {
#include "assets/dryfield_night_garage_collision_06814_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_80183DD4 = { NULL, _gDryfieldNightGarageCollision06814Normals, _gDryfieldNightGarageCollision06814Verts, _gDryfieldNightGarageCollision06814Faces, _gDryfieldNightGarageCollision06814Table, 150, 0, 3, 3, 4000, 55 };

static SVECTOR _gDryfieldNightGarageCollision06E14Normals[29] = {
#include "assets/dryfield_night_garage_collision_06E14_normals.inc"
};

static SVECTOR _gDryfieldNightGarageCollision06E14Verts[74] = {
#include "assets/dryfield_night_garage_collision_06E14_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageCollision06E14Faces[34] = {
#include "assets/dryfield_night_garage_collision_06E14_faces.inc"
};

static s16 _gDryfieldNightGarageCollision06E14Cells[116] = {
#include "assets/dryfield_night_garage_collision_06E14_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightGarageCollision06E14Cells[i])
static s16* _gDryfieldNightGarageCollision06E14Table[9] = {
#include "assets/dryfield_night_garage_collision_06E14_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_801843D4[1] = {
    { NULL, _gDryfieldNightGarageCollision06E14Normals, _gDryfieldNightGarageCollision06E14Verts, _gDryfieldNightGarageCollision06E14Faces, _gDryfieldNightGarageCollision06E14Table, 150, 0, 3, 3, 4000, 34 },
};

ViewCamera D_dryfield_night_garage_801843F8[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5508, 0x33C2, -3974 } }, 257 },
    { { { { 3382, 0, 2309 }, { 756, 3870, -1107 }, { -2182, 1340, 3196 } }, { -3863, 2161, -312 } }, 257 },
    { { { { 1352, 0, 3866 }, { 657, 4036, -230 }, { -3809, 696, 1332 } }, { -0x294D, 1921, -1936 } }, 257 },
    { { { { 4042, 0, 661 }, { 142, 3999, -870 }, { -646, 882, 3947 } }, { -9784, 1851, -1821 } }, 257 },
    { { { { 2349, 0, 3354 }, { 3299, 742, -2310 }, { -607, 4028, 425 } }, { -9916, 5826, -2247 } }, 257 },
    { { { { 3533, 0, -2071 }, { -1751, 2188, -2987 }, { 1106, 3462, 1887 } }, { -997, 4246, -3702 } }, 257 },
    { { { { 1971, 0, 3590 }, { 498, 4056, -274 }, { -3555, 569, 1952 } }, { -6690, 1477, -1070 } }, 257 },
    { { { { 3432, 0, 2234 }, { 1426, 3153, -2190 }, { -1720, 2614, 2642 } }, { -6703, 3966, -2006 } }, 334 },
    { { { { -3944, 0, -1104 }, { -131, 4067, 468 }, { 1096, 486, -3916 } }, { -4475, 1566, -6928 } }, 376 },
    { { { { 3672, 0, -1814 }, { 226, 4064, 457 }, { 1800, -510, 3643 } }, { -252, 440, -656 } }, 447 },
    { { { { -2395, 0, -3322 }, { -444, 4059, 320 }, { 3292, 547, -2374 } }, { 862, 1531, -6010 } }, 541 },
    { { { { 3252, 0, 2489 }, { 731, 3915, -955 }, { -2379, 1203, 3108 } }, { -4345, 2379, -1634 } }, 447 },
    { { { { 3382, 0, 2309 }, { 756, 3870, -1107 }, { -2182, 1340, 3196 } }, { -3863, 2161, -312 } }, 257 },
    { { { { 1971, 0, 3590 }, { 498, 4056, -274 }, { -3555, 569, 1952 } }, { -6690, 1477, -1070 } }, 257 },
    { { { { 1352, 0, 3866 }, { 657, 4036, -230 }, { -3809, 696, 1332 } }, { -0x294D, 1921, -1936 } }, 257 },
};

SpriteBatch D_dryfield_night_garage_80184614[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80184624[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 96, 786, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 96, 618, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 608, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 596, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 545, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 542, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 533, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 524, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 514, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 750, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 590, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 580, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 569, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 520, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 518, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 728, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 717, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 564, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 554, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 16, 618, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 16, 593, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 24, 549, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 32, 522, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 64, 541, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, 64, 572, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 56, 598, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 56, 620, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 56, 637, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 48, 687, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 16, 640, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, 32, 504, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 0, 663, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 48, -16, 784, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, -24, 887, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -32, 1006, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -48, 1329, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -56, 1171, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -64, 1133, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -64, 1081, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 16, 570, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 0, 875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, -32, 986, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, -32, 992, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, -16, 843, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, -16, 821, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, 0, 738, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 636, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 0, 602, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 56, 821, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 853, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 24, 805, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 8, 792, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 72, 823, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 72, 854, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80184A5C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 48, 0, 0, { 1, 0 } },
    { 48, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80184A7C[52] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 1900, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 0, 1620, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -40, 2082, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 2075, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -32, 1882, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, -16, 1924, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 2062, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, -32, 1783, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -16, 1775, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -64, -16, 1559, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -16, 1674, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, -16, 1710, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 0, 2033, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -8, 1832, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -8, 1898, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, -8, 1771, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 0, 1687, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -24, 0, 1662, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, -24, 1906, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, -24, 1980, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 932, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 56, 909, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 56, 914, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 64, 894, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 64, 902, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 64, 927, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 56, 842, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 56, 842, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 56, 836, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 80, 883, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 852, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 80, 844, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 80, 838, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 32, 1603, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 1584, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 1638, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 24, 1583, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 8, 1579, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 8, 1594, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 24, 1592, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 24, 788, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 32, 762, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 32, 746, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 24, 818, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 56, 805, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 64, 792, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 72, 769, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 761, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 104, 780, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 96, 800, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 88, 787, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 24, 790, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80184E8C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 6, 0, 0, { 3, 0 } },
    { 26, 7, 0, 0, { 2, 0 } },
    { 33, 7, 0, 0, { 4, 0 } },
    { 40, 12, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80184EC4[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 0, 857, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 0, 888, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 8, 823, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 8, 807, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 8, 807, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 8, 807, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 8, 794, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 0, 897, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 0, 836, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 48, 873, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 8, 807, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 16, 808, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 56, 832, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 16, 786, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 56, 811, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 16, 770, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 56, 798, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 16, 774, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 56, 806, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 16, 793, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 912, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80185068[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80185080[25] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -48, 1466, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -48, 1468, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, -56, 1438, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, -48, 1436, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, -40, 1445, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -48, 1374, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -40, 1402, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -40, 1462, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -48, 1384, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -32, 1484, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -32, 1510, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -96, 1343, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -88, 1369, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -80, 1558, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -80, 1437, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -80, 1436, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -80, 1442, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -96, 1308, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -104, 1316, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -112, 1325, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -104, 1318, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -64, 1514, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -80, 1425, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, -96, 1313, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, -88, 1305, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80185274[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 6, 0, 0, { 2, 0 } },
    { 11, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_8018529C[78] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -72, 1097, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 799, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 72, 804, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 56, 832, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 40, 865, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 88, 886, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 814, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 72, 947, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 24, 980, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 56, 998, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -80, 1371, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -64, 1243, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -88, 1225, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, -88, 1142, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 8, -112, 1272, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, -104, 1223, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, -104, 1206, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -104, 1200, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -96, 975, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 24, 1068, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 16, 1160, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -72, 988, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -64, 1051, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -40, 1004, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -40, 1064, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -16, 1182, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1268, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -24, 1206, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 0, 1136, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 8, 1112, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -88, 1139, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, -40, 967, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -64, 936, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -48, 1113, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -24, 943, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 921, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -104, 1017, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -104, 1015, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -80, 941, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -72, 956, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -56, 1141, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -88, 942, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -64, 1115, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -56, 918, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, -40, 1087, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -24, 1062, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -24, 1047, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -24, 1092, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 8, 854, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -8, 889, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 774, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -56, 956, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -40, 923, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -56, 904, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -56, 1179, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -56, 1068, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -8, 1117, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 872, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -48, 977, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 781, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 80, 724, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 64, 760, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 64, 743, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 40, 805, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 24, 809, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 112, 24, 827, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 96, 8, 850, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 96, -8, 900, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, -24, 938, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -16, 932, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, -32, 955, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -72, 928, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, -72, 911, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -72, 919, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -56, 879, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -40, 948, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, -88, 966, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -88, 959, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_801858B4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 78, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_801858CC[46] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -24, 963, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, -24, 1035, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, -24, 994, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -32, 1079, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -32, 1142, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -32, 1176, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -32, 1090, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -32, 1016, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -48, 1301, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -48, 1219, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -48, 1147, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -48, 1068, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -32, 1345, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -32, 1260, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -32, 1186, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, -32, 1118, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, -32, 1057, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -24, 925, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -24, 882, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -24, 841, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -24, 805, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, -24, 1077, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -24, 1048, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 1071, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 8, 1129, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, 8, 788, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 96, 8, 894, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 96, 32, 820, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 56, 828, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -16, 795, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 16, 771, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 48, 829, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 48, 878, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, -16, 793, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -16, 931, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -24, -16, 869, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 48, 853, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 0, -16, 822, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 24, -16, 799, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 80, 701, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 727, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 96, 636, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 80, 651, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 64, 657, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 72, 751, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 64, 762, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80185C64[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 39, 0, 0, { 1, 0 } },
    { 39, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_garage_80185C84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80185C94[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 712, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 0, 96, 223, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 24, 812, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 24, 771, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 40, 670, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, 40, 661, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 104, 717, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 72, 325, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 298, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -16, 1616, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -24, 1035, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -16, 565, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -16, 529, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -16, 845, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 96, 265, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 72, 307, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 48, 363, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 40, 377, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 32, 368, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 16, 356, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 0, 364, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, -16, 359, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, -32, 373, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, -48, 386, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -32, 311, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -56, 368, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 0, 308, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 16, 287, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 32, 276, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 56, 252, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 80, 211, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 104, 208, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, 40, 676, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 80, 1109, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 64, 764, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 56, 840, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80185F64[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_garage_80185F7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_garage_80185F8C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_80185F9C[23] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -32, 1725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, 88, 568, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -128, 104, 535, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -64, 72, 600, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -48, 56, 645, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -16, 40, 700, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 16, 24, 767, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 48, 0, 854, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 8, 868, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 120, -48, 978, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 136, -56, 948, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, 88, 604, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -64, 104, 599, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -24, 72, 572, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 56, 650, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 88, 536, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, 40, 694, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, 80, 611, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, 24, 755, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, 24, 706, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 24, 706, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 72, 64, 639, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, 64, 631, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80186168[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_garage_80186180[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_garage_80186190[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_garage_801861A0[8] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 855, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 48, 837, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 80, 864, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 24, 797, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 797, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 24, 790, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 32, 736, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 751, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_garage_80186240[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_garage_80186258[15] = {
    { { .empty = D_dryfield_night_garage_80184614 }, D_dryfield_night_garage_80184614, NULL },
    { { .elements = D_dryfield_night_garage_80184624 }, D_dryfield_night_garage_80184A5C, NULL },
    { { .elements = D_dryfield_night_garage_80184A7C }, D_dryfield_night_garage_80184E8C, NULL },
    { { .elements = D_dryfield_night_garage_80184EC4 }, D_dryfield_night_garage_80185068, NULL },
    { { .elements = D_dryfield_night_garage_80185080 }, D_dryfield_night_garage_80185274, NULL },
    { { .elements = D_dryfield_night_garage_8018529C }, D_dryfield_night_garage_801858B4, NULL },
    { { .elements = D_dryfield_night_garage_801858CC }, D_dryfield_night_garage_80185C64, NULL },
    { { .empty = D_dryfield_night_garage_80185C84 }, D_dryfield_night_garage_80185C84, NULL },
    { { .elements = D_dryfield_night_garage_80185C94 }, D_dryfield_night_garage_80185F64, NULL },
    { { .empty = D_dryfield_night_garage_80185F7C }, D_dryfield_night_garage_80185F7C, NULL },
    { { .empty = D_dryfield_night_garage_80185F8C }, D_dryfield_night_garage_80185F8C, NULL },
    { { .elements = D_dryfield_night_garage_80185F9C }, D_dryfield_night_garage_80186168, NULL },
    { { .empty = D_dryfield_night_garage_80186180 }, D_dryfield_night_garage_80186180, NULL },
    { { .empty = D_dryfield_night_garage_80186190 }, D_dryfield_night_garage_80186190, NULL },
    { { .elements = D_dryfield_night_garage_801861A0 }, D_dryfield_night_garage_80186240, NULL },
};

WorldCollisionTrigger D_dryfield_night_garage_8018630C[14] = {
    { NULL, NULL, NULL, { 1455, -1648, 1967, 0 }, { { -1965, -2672, -1394, 0 }, { 1966, -2672, 1395, 0 }, { -1965, 2672, -1394, 0 }, { 1966, 2672, 1395, 0 } }, { 2375, 0, -3350, 0 }, { 0, 0, 4096, 0 }, 3593, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1566, -1584, 1886, 0 }, { { 1966, -2608, 1395, 0 }, { -1965, -2608, -1394, 0 }, { 1966, 2608, 1395, 0 }, { -1965, 2608, -1394, 0 } }, { -2377, 0, 3348, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1664, 4894, 0 }, { { 2101, -2688, -1181, 0 }, { -2101, -2688, 1181, 0 }, { 2101, 2688, -1181, 0 }, { -2101, 2688, 1181, 0 } }, { 2010, 0, 3577, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1024, -1600, 5023, 0 }, { { -2101, -2624, 1181, 0 }, { 2101, -2624, -1181, 0 }, { -2101, 2624, 1181, 0 }, { 2101, 2624, -1181, 0 } }, { -2011, 0, -3577, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7891, -1552, 5098, 0 }, { { -532, -2576, 52, 0 }, { 532, -2576, -51, 0 }, { -532, 2576, 52, 0 }, { 532, 2576, -51, 0 } }, { -397, 0, -4085, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7849, -1760, 4970, 0 }, { { 587, -2784, -43, 0 }, { -586, -2784, 44, 0 }, { 587, 2784, -43, 0 }, { -586, 2784, 44, 0 } }, { 303, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7326, -1536, 2718, 0 }, { { 1076, -2560, 2157, 0 }, { -1076, -2560, -2156, 0 }, { 1076, 2560, 2157, 0 }, { -1076, 2560, -2156, 0 } }, { -3672, 0, 1831, 0 }, { 0, 0, 4096, 0 }, 3510, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7199, -1728, 2782, 0 }, { { -1076, -2752, -2156, 0 }, { 1076, -2752, 2157, 0 }, { -1076, 2752, -2156, 0 }, { 1076, 2752, 2157, 0 } }, { 3664, 0, -1829, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9680, -1840, 4735, 0 }, { { -1379, -2864, 332, 0 }, { 1379, -2864, -332, 0 }, { -1379, 2864, 332, 0 }, { 1379, 2864, -332, 0 } }, { -960, 0, -3984, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9664, -1792, 4640, 0 }, { { 1379, -2816, -332, 0 }, { -1379, -2816, 332, 0 }, { 1379, 2816, -332, 0 }, { -1379, 2816, 332, 0 } }, { 958, 0, 3982, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6299, -1792, 6443, 0 }, { { 1088, -2784, -1524, 0 }, { -1088, -2784, 1524, 0 }, { 1088, 2784, -1524, 0 }, { -1088, 2784, 1524, 0 } }, { 3347, 0, 2390, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6427, -1760, 6490, 0 }, { { -1047, -2784, 1415, 0 }, { 1047, -2784, -1414, 0 }, { -1047, 2784, 1415, 0 }, { 1047, 2784, -1414, 0 } }, { -3299, 0, -2442, 0 }, { 0, 0, 4096, 0 }, 3288, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4561, -1760, 1200, 0 }, { { 131, -2672, -2370, 0 }, { -130, -2672, 2371, 0 }, { 131, 2672, -2370, 0 }, { -130, 2672, 2371, 0 } }, { 4093, 0, 225, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 3, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4673, -1760, 1168, 0 }, { { -141, -2672, 2542, 0 }, { 142, -2672, -2542, 0 }, { -141, 2672, 2542, 0 }, { 142, 2672, -2542, 0 } }, { -4095, 0, -229, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 7, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_garage_80186734[12] = {
    { NULL, NULL, NULL, { 7955, -1552, 5050, 0 }, { { -564, -2576, 68, 0 }, { 564, -2576, -67, 0 }, { -564, 2576, 68, 0 }, { 564, 2576, -67, 0 } }, { -489, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7881, -1760, 4986, 0 }, { { 555, -2784, -59, 0 }, { -554, -2784, 60, 0 }, { 555, 2784, -59, 0 }, { -554, 2784, 60, 0 } }, { 435, 0, 4075, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7326, -1536, 2718, 0 }, { { 1076, -2560, 2157, 0 }, { -1076, -2560, -2156, 0 }, { 1076, 2560, 2157, 0 }, { -1076, 2560, -2156, 0 } }, { -3672, 0, 1831, 0 }, { 0, 0, 4096, 0 }, 3510, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7199, -1728, 2782, 0 }, { { -1076, -2752, -2156, 0 }, { 1076, -2752, 2157, 0 }, { -1076, 2752, -2156, 0 }, { 1076, 2752, 2157, 0 } }, { 3664, 0, -1829, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9680, -1840, 4735, 0 }, { { -1379, -2864, 332, 0 }, { 1379, -2864, -332, 0 }, { -1379, 2864, 332, 0 }, { 1379, 2864, -332, 0 } }, { -960, 0, -3984, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9664, -1792, 4640, 0 }, { { 1379, -2816, -332, 0 }, { -1379, -2816, 332, 0 }, { 1379, 2816, -332, 0 }, { -1379, 2816, 332, 0 } }, { 958, 0, 3982, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6365, -1792, 6493, 0 }, { { 1054, -2784, -1478, 0 }, { -1058, -2784, 1474, 0 }, { 1054, 2784, -1478, 0 }, { -1058, 2784, 1474, 0 } }, { 3345, 0, 2394, 0 }, { 0, 0, 4096, 0 }, 3318, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6413, -1760, 6493, 0 }, { { -1033, -2784, 1412, 0 }, { 1029, -2784, -1417, 0 }, { -1033, 2784, 1412, 0 }, { 1029, 2784, -1417, 0 } }, { -3318, 0, -2419, 0 }, { 0, 0, 4096, 0 }, 3278, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3873, -1760, 1071, 0 }, { { -1288, -2672, -2491, 0 }, { 1289, -2672, 2492, 0 }, { -1288, 2672, -2491, 0 }, { 1289, 2672, 2492, 0 } }, { 3640, 0, -1884, 0 }, { 0, 0, 4096, 0 }, 3873, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4001, -1760, 767, 0 }, { { 1426, -2672, 2837, 0 }, { -1426, -2672, -2836, 0 }, { 1426, 2672, 2837, 0 }, { -1426, 2672, -2836, 0 } }, { -3673, 0, 1845, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3311, -1664, 6112, 0 }, { { 1880, -2672, -2600, 0 }, { -1883, -2672, 2596, 0 }, { 1880, 2672, -2600, 0 }, { -1883, 2672, 2596, 0 } }, { 3337, 0, 2416, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3583, -1664, 6207, 0 }, { { -1882, -2672, 2589, 0 }, { 1840, -2672, -2629, 0 }, { -1882, 2672, 2589, 0 }, { 1840, 2672, -2629, 0 } }, { -3352, 0, -2392, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Seven point lights shared by both nighttime garage room entries, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each inner radius is zero, so strength falls with squared
/// distance from the source to zero at its outer radius. The loaded room overlay
/// owns this writable array: coordinate updates parent and compose the transforms,
/// and lighting queries overwrite attenuation. Borrowed pointers must not outlive
/// the overlay.
static WorldCoordPointLight _gDryfieldNightGaragePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7636, -1881, 6062 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 2664,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8399, -1740, 3071 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3287 },
        },
        .inner = 0,
        .outer = 2930,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5719, -2018, 3332 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3689, 3689, 3689 },
        },
        .inner = 0,
        .outer = 3084,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5081, -2118, 5769 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3690, 3690, 3690 },
        },
        .inner = 0,
        .outer = 2502,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 957, -426, 5440 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3442, 2839, 1557 },
        },
        .inner = 0,
        .outer = 1762,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3025, -1659, 6264 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 0,
        .outer = 2924,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3110, -1735, 2693 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3277, 3281, 3290 },
        },
        .inner = 0,
        .outer = 4007,
    },
};

WorldCoordRoomLights D_dryfield_night_garage_80186D64[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldNightGaragePointLights), _gDryfieldNightGaragePointLights, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_garage_80186D7C[16] = {
    { NULL, NULL, NULL, { 3616, -64, 2864, 0 }, { { -1344, 0, -1424, 0 }, { 1184, 0, -1424, 0 }, { -1888, 0, 400, 0 }, { 1952, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1991, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 752, -48, 7600, 0 }, { { -1104, 0, -544, 0 }, { 1104, 0, -544, 0 }, { -1104, 0, 544, 0 }, { 1104, 0, 544, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_WARP, 26, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 640, -48, 2928, 0 }, { { 272, 0, -976, 0 }, { 272, 0, 976, 0 }, { -272, 0, -976, 0 }, { -272, 0, 976, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1011, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4032, -50, 4896, 0 }, { { -16, 0, -1056, 0 }, { 2192, 0, -1056, 0 }, { -16, 0, 1056, 0 }, { 2192, 0, 1056, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2428, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1696, -64, 5456, 0 }, { { -752, 0, -336, 0 }, { 752, 0, -336, 0 }, { -752, 0, 336, 0 }, { 752, 0, 336, 0 } }, { 0, 4099, 0, 0 }, { -4092, 0, 201, 0 }, 822, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5104, -64, 5920, 0 }, { { -1504, 0, -1184, 0 }, { 1504, 0, -1184, 0 }, { -1504, 0, 1184, 0 }, { 1504, 0, 1184, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1911, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1344, -64, 1664, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -864, 0 }, { -816, 0, 864, 0 }, { 816, 0, 224, 0 } }, { 0, 4096, 0, 0 }, { 1567, 0, 3784, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 55, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9472, -64, 1568, 0 }, { { -1408, 0, -640, 0 }, { 1408, 0, -640, 0 }, { -1408, 0, 640, 0 }, { 1408, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1546, WORLD_COLLISION_TRIGGER_ACTION_CAP, 52, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8377, -64, 4879, 0 }, { { -1634, 0, 80, 0 }, { 70, 0, -1640, 0 }, { -73, 0, 1641, 0 }, { 1638, 0, -79, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_CAP, 53, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4992, -64, 5728, 0 }, { { -464, 0, -2512, 0 }, { 464, 0, -2512, 0 }, { -464, 0, 2512, 0 }, { 464, 0, 2512, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 2547, WORLD_COLLISION_TRIGGER_ACTION_CAP, 54, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9456, -64, 6592, 0 }, { { -544, 0, -1552, 0 }, { 544, 0, -1552, 0 }, { -544, 0, 1552, 0 }, { 544, 0, 1552, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 1644, WORLD_COLLISION_TRIGGER_ACTION_CAP, 51, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6704, -64, 896, 0 }, { { -1200, 0, -32, 0 }, { 1200, 0, -32, 0 }, { -1200, 0, 1088, 0 }, { 1200, 0, 1088, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 56, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2000, -64, 6096, 0 }, { { -448, 0, -320, 0 }, { 448, 0, -320, 0 }, { -448, 0, 320, 0 }, { 448, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { -2, 0, 4097, 0 }, 550, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1952, -64, 4768, 0 }, { { -448, 0, -320, 0 }, { 448, 0, -320, 0 }, { -448, 0, 320, 0 }, { 448, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { 2, 0, -4097, 0 }, 550, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2368, -64, 5600, 0 }, { { -464, 0, -2512, 0 }, { 464, 0, -2512, 0 }, { -464, 0, 2512, 0 }, { 464, 0, 2512, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 2547, WORLD_COLLISION_TRIGGER_ACTION_CAP, 54, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2640, -64, 5440, 0 }, { { -1728, 0, -976, 0 }, { -704, 0, -976, 0 }, { -1728, 0, 976, 0 }, { -704, 0, 976, 0 } }, { 0, 4100, 0, 0 }, { -4092, 0, 201, 0 }, 1982, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_garage_8018723C[7] = {
    { NULL, NULL, NULL, { 368, -48, 7600, 0 }, { { -1104, 0, -544, 0 }, { 1104, 0, -544, 0 }, { -1104, 0, 544, 0 }, { 1104, 0, 544, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_WARP, 26, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 640, -48, 2928, 0 }, { { 272, 0, -976, 0 }, { 272, 0, 976, 0 }, { -272, 0, -976, 0 }, { -272, 0, 976, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1011, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1344, -64, 1664, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -864, 0 }, { -816, 0, 864, 0 }, { 816, 0, 224, 0 } }, { 0, 4096, 0, 0 }, { 1567, 0, 3784, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 55, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9472, -64, 1568, 0 }, { { -1408, 0, -640, 0 }, { 1408, 0, -640, 0 }, { -1408, 0, 640, 0 }, { 1408, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1546, WORLD_COLLISION_TRIGGER_ACTION_CAP, 52, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8377, -64, 4879, 0 }, { { -1634, 0, 80, 0 }, { 70, 0, -1640, 0 }, { -73, 0, 1641, 0 }, { 1638, 0, -79, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_CAP, 53, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9456, -64, 6592, 0 }, { { -544, 0, -1552, 0 }, { 544, 0, -1552, 0 }, { -544, 0, 1552, 0 }, { 544, 0, 1552, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 1644, WORLD_COLLISION_TRIGGER_ACTION_CAP, 51, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6704, -64, 896, 0 }, { { -1200, 0, -32, 0 }, { 1200, 0, -32, 0 }, { -1200, 0, 1088, 0 }, { 1200, 0, 1088, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 56, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_garage_80187450[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_garage_80187468[3] = {
    { 106, 354, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_135400_8013A4AC },
    { 114, 354, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_135400_8013F8D8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_garage_8018748C[2] = {
    { 101, 363, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_136300_8013B11C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_garage_801874A4[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_garage_801874BC[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C4E8, D_dryfield_night_garage_80187450 },
    { D_map_dryfield_full_8017C538, D_dryfield_night_garage_80187468 },
    { D_map_dryfield_full_8017C568, D_dryfield_night_garage_8018748C },
    { D_map_dryfield_full_8017C578, D_dryfield_night_garage_801874A4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_night_garage_8018751C[16] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_garage_8018751C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 200, 200, 200, 200 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 140, 140, 140, 140 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_garage_8018759C = {
    0x10000051,
    0x10000053,
    0x10000051,
};

WorldCollisionSurfaceProperties D_dryfield_night_garage_801875A8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_garage_801875B0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_garage_8018759C },
};

WorldCollisionSurfaceProperties* D_dryfield_night_garage_801875B8[8] = {
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875B0,
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875A8,
    D_dryfield_night_garage_801875A8,
};

AreaApplyRec D_dryfield_night_garage_801875D8[18] = {
    { 3, 1, 3, 17 },
    { 3, 1, 7, 33 },
    { 3, 2, 3, 0 },
    { 3, 6, 3, 1 },
    { 3, 7, 3, 1 },
    { 3, 15, 2, 17 },
    { 3, 15, 7, 33 },
    { 3, 16, 3, 1 },
    { 3, 18, 1, 0 },
    { 3, 19, 3, 1 },
    { 3, 20, 2, 1 },
    { 3, 22, 3, 1 },
    { 3, 25, 3, 1 },
    { 3, 29, 6, 0 },
    { 3, 31, 3, 1 },
    { 3, 32, 2, 1 },
    { 3, 34, 3, 1 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_dryfield_night_garage_80187620[2] = {
    { 3, 38, 3, 1 },
    { 255, 0, 0, 0 },
};

s32 Shop_Data_80187628;

EquipmentWeaponSupply* Shop_Data_8018762C;

void func_dryfield_night_garage_801809A4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_RunCapCmd(arg0->spawnArg1.value, 0);
            TASK_MESSAGE_DISPATCH_POINTER(func_dryfield_night_garage_80180A64(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE0, 0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_MsgPlayerWeapon(1);
                TASK_MESSAGE_DISPATCH_POINTER(func_dryfield_night_garage_80180A64(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE4, 0);
                break;
            }
            return;
    }
    taskKill(arg0);
}

Task* func_dryfield_night_garage_80180A64(s32 arg0)
{
    Enemy* enemy;
    Task*  task;

    enemy = Gp_FindWorkById(gGameSession->location.loc.area | ((arg0 << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT)));
    task  = NULL;
    if (enemy != NULL) {
        task = enemy->task;
    }
    return task;
}

void func_dryfield_night_garage_80180AB0(void)
{
    Gp_ApplyAreaRecs(D_dryfield_night_garage_801875D8);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, 0);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, 0);
    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 5);
    gameFlagSetNibble(GAME_FLAG_SALOON_PARKING_LOT_DOOR_UNLOCKED, 1);
    if (gameFlagGetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_garage_80187620);
    }
}

void func_dryfield_night_garage_80180B20(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            return;
        case 1:
            key = gGameSession->location;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                if (task->spawnArg1.value != 0) {
                    key.loc.view = 0x67;
                } else {
                    key.loc.view = 0x65;
                }
            } else {
                if (task->spawnArg1.value != 0) {
                    key.loc.view = 0x66;
                } else {
                    key.loc.view = 0x64;
                }
            }
            slot         = streamFindMovieSlot(&key.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state++;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state++;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state++;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            taskKill(task);
            displayResumeGameLoop();
    }
}

/// Fades the screen to white: draws a white overlay whose level, kept in
/// `killCountdown`, rises by 4 each frame, and kills the task once it reaches
/// 0x100.
void func_dryfield_night_garage_80180CEC(Task* arg0)
{
    u16 temp_v0;

    fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

/// Spawns `D_dryfield_night_garage_80183380` with an ordering table, passing
/// on the task's `spawnArg1`, sets `gDisplayState.control.flags.flipMode`, spawns the view tasks and
/// kills itself.
void func_dryfield_night_garage_80180D4C(Task* arg0)
{
    Display_SpawnWithOt(D_dryfield_night_garage_80183380, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

#include "../../shared/glow_draw_grey_capsule.inc.c"

/// Garage room draw: sweeps the glowing strip the current visit
/// (`gGameSession->location.loc.view`) selects. Visits 3 and 15 sweep all four of the
/// room's run, 7 and 14 only its first pair, and 11 the last pair of the run
/// with its own blend (`arg2` 0x800 instead of 0). Each case names its own last
/// draw, which `jump.c` cross-jumps into one tail block after the last case.
void func_dryfield_night_garage_80181518(Task* unused)
{
    switch (gGameSession->location.loc.view) {
        case 3:
        case 15: {
            SVECTOR* p = D_dryfield_night_garage_801833A4;
            glowDrawGreyCapsule(&p[0], 0x200, 0);
            glowDrawGreyCapsule(&p[2], 0x200, 0);
            glowDrawGreyCapsule(&p[4], 0x200, 0);
            glowDrawGreyCapsule(&p[6], 0x200, 0);
            break;
        }
        case 7:
        case 14:
            glowDrawGreyCapsule(&D_dryfield_night_garage_801833A4[0], 0x200, 0);
            break;
        case 11: {
            SVECTOR* p = &D_dryfield_night_garage_801833D4;
            glowDrawGreyCapsule(&p[0], 0x200, 0x800);
            glowDrawGreyCapsule(&p[2], 0x200, 0x800);
            break;
        }
    }
}
