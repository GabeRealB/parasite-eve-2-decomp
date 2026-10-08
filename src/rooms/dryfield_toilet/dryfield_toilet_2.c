#include "rooms/dryfield_toilet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_toilet_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"
#include "../../shared/room_visual_effects.h"

extern WorldCollisionOccluder D_dryfield_toilet_801826F0[1];
extern WorldCollisionTrigger  D_dryfield_toilet_8018227C[6];
extern WorldCollisionTrigger  D_dryfield_toilet_80182444[9];
extern WorldCoordRoomLights   D_dryfield_toilet_801828AC[1];

static WorldCollisionGridFace _gDryfieldToiletCollision03E44Faces[17];
static SVECTOR                _gDryfieldToiletCollision03E44Normals[7];
static SVECTOR                _gDryfieldToiletCollision03E44Verts[38];
static s16*                   _gDryfieldToiletCollision03E44Table[2];

extern AreaResource D_dryfield_toilet_801828C4[3];
extern AreaResource D_dryfield_toilet_801828E8[2];
extern AreaResource D_dryfield_toilet_80182900[2];

extern SVECTOR D_dryfield_toilet_8018662C[326];
extern SVECTOR D_dryfield_toilet_8018705C[1604];

/// Player clips for extended ids 47-49; NULL at id 47, which nothing plays.
///
/// Both of the room's event scripts send the player the copy request.
/// `D_dryfield_toilet_80180B98` copies four words starting here into the
/// player's bank, which is one word past the end of this array: the read runs
/// on through the first word of `D_dryfield_toilet_80180B98`, that request's
/// own source pointer. That overrun is the original's and is kept as it is: the
/// request carries a literal count larger than the table, while the table was
/// stored with only its own entries. One script then plays ids 48 and 49; the
/// other plays a base-bank clip. Id 50, which receives the source pointer, is
/// never played.
AnimationSet* D_dryfield_toilet_80180B8C[3] = { NULL, &gDryfieldToiletAnimation03054, &gDryfieldToiletAnimation035A4 };

// Installs the player's clips; the count is four, not the three entries of its source.
AnimationBankCopyRequest D_dryfield_toilet_80180B98 = { { .sets = D_dryfield_toilet_80180B8C }, 4 };

AnimationPlayRequest D_dryfield_toilet_80180BA0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_toilet_80180BB4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_toilet_80180BC8 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_toilet_80180BDC = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_toilet_80180BF0 = { { -1664, 0, 135, 0 }, { 0, -2047, 0, 0 } };

ActorCommand D_dryfield_toilet_80180C08 = { { .loc = { 2, 16 } }, 1 };

ActorCommand D_dryfield_toilet_80180C0C = { { .loc = { 2, 16 } }, 2 };

ActorCommand D_dryfield_toilet_80180C10 = { { .loc = { 2, 16 } }, 10 };

ActorCommand D_dryfield_toilet_80180C14 = { { .loc = { 2, 16 } }, 11 };

ActorCommand D_dryfield_toilet_80180C18[2] = {
    { { .loc = { 2, 16 } }, 12 },
    { { .loc = { 2, 16 } }, 13 },
};

ActorTransform D_dryfield_toilet_80180C20[2] = {
    { { -1664, 0, -1222, 0 }, { 0, -1024, 0, 0 } },
    { { -1664, 0, -1222, 0 }, { 0, 0, 0, 0 } },
};

EvsSceneKey D_dryfield_toilet_80180C50 = { 2, 33, 11 };

EvsCommand D_dryfield_toilet_80180C58[31] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_dryfield_toilet_80180C50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletStageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_toilet_80180B98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_toilet_80180BB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_toilet_80180BC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x52100006 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletStartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 132 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_toilet_80180BF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C0C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletMoveEventCollision }, { .value = DRYFIELD_TOILET_EVENT_COLLISION_MOVE_ASIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletFinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_toilet_80180F40[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletCancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_toilet_80180BF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_toilet_80180B98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_toilet_80180BDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_toilet_80180C0C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletMoveEventCollision }, { .value = DRYFIELD_TOILET_EVENT_COLLISION_MOVE_ASIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room_visual_effects_disc_data.inc.c"

WorldCollisionRoomResources D_dryfield_toilet_8018112C[1] = {
    { &D_dryfield_toilet_80181404, D_dryfield_toilet_8018227C, D_dryfield_toilet_80182444, D_dryfield_toilet_801826F0 },
};

u8* D_dryfield_toilet_8018113C[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_toilet_80181140[1] = { 11 };

WorldCoordRoomLighting D_dryfield_toilet_80181144[1] = {
    { D_dryfield_toilet_801828AC, NULL },
};

DirectionWarpEntry D_dryfield_toilet_8018114C[1] = {
    { { { .word = 2048 }, -1660, 0, 1653 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -1660, 0, 1653 }, { 0, 0, 0, 0 }, 0x52100002, 0x52100001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 480 },
};

static SVECTOR _gDryfieldToiletCollision03E44Normals[7] = {
#include "assets/dryfield_toilet_collision_03E44_normals.inc"
};

static SVECTOR _gDryfieldToiletCollision03E44Verts[38] = {
#include "assets/dryfield_toilet_collision_03E44_verts.inc"
};

static WorldCollisionGridFace _gDryfieldToiletCollision03E44Faces[17] = {
#include "assets/dryfield_toilet_collision_03E44_faces.inc"
};

static s16 _gDryfieldToiletCollision03E44Cells[34] = {
#include "assets/dryfield_toilet_collision_03E44_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldToiletCollision03E44Cells[i])
static s16* _gDryfieldToiletCollision03E44Table[2] = {
#include "assets/dryfield_toilet_collision_03E44_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_toilet_80181404 = { NULL, _gDryfieldToiletCollision03E44Normals, _gDryfieldToiletCollision03E44Verts, _gDryfieldToiletCollision03E44Faces, _gDryfieldToiletCollision03E44Table, 2970, 2300, 1, 2, 4000, 17 };

ViewCamera D_dryfield_toilet_80181428[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 1524 },
    { { { { 4000, 0, -880 }, { -628, 2868, -2855 }, { 616, 2924, 2801 } }, { 2049, 2467, 57 } }, 230 },
    { { { { -3969, 0, -1008 }, { -446, 3672, 1757 }, { 904, 1813, -3559 } }, { 2094, 1926, -1868 } }, 230 },
    { { { { -4005, 0, 858 }, { 107, 4063, 502 }, { -851, 513, -3973 } }, { -515, 1179, -1469 } }, 207 },
    { { { { 3989, 0, 927 }, { -344, 3803, 1481 }, { -861, -1521, 3704 } }, { -488, 203, 1520 } }, 207 },
    { { { { 863, 0, -4003 }, { -3600, 1791, -776 }, { 1751, 3683, 377 } }, { -1314, 2467, 1776 } }, 207 },
    { { { { -152, 0, -4093 }, { -3445, 2211, 128 }, { 2210, 3447, -82 } }, { -1153, 2360, -518 } }, 207 },
    { { { { -977, 0, -3977 }, { -3279, 2317, 805 }, { 2250, 3377, -552 } }, { -1153, 2360, -1708 } }, 207 },
    { { { { -4095, 0, -66 }, { -26, 3759, 1626 }, { 60, 1626, -3758 } }, { 1655, 1744, -488 } }, 230 },
    { { { { -4003, 0, 865 }, { -160, 4025, -742 }, { -850, -759, -3934 } }, { 1455, 734, -688 } }, 329 },
    { { { { -3984, 0, 947 }, { 393, 3725, 1656 }, { -862, 1702, -3624 } }, { 1635, 1684, 31 } }, 338 },
};

SpriteBatch D_dryfield_toilet_801815B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_toilet_801815C4[45] = {
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 80, 392, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -88, 96, 350, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -80, 72, 300, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 72, 325, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 32, -120, 687, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 24, -120, 700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -48, 716, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -120, 413, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 48, 478, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 88, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -56, 435, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, -120, 374, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -56, 379, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 388, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 48, 426, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 88, 443, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -120, 342, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, -56, 348, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 345, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 48, 389, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 397, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 315, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -56, 330, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 0, 337, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 48, 351, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 370, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 56, -120, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -24, 663, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -120, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -24, 618, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -16, 582, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -120, 509, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 80, -8, 551, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, 16, 667, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -32, 710, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 48, -120, 582, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -120, 583, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 40, -40, 757, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 465, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 452, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -56, 453, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -120, 448, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -120, 484, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -72, 472, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_toilet_80181948[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 40, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_toilet_80181968[47] = {
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 40, 0, 525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 24, 16, 400, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 56, 650, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, 72, 625, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, -24, 800, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -40, 825, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 40, 16, 450, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 40, 40, 375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 64, 300, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, 40, 80, 300, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 56, -16, 600, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, -40, 825, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -40, -120, 691, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -32, -96, 766, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -48, -120, 659, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -56, -120, 611, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -64, -120, 569, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -72, -120, 533, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -80, -120, 501, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -88, -120, 472, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 40, 761, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 40, 715, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 40, 660, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 610, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 565, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 528, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 494, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, -8, 442, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -8, 376, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -8, 327, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -64, 325, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -64, 369, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -64, 425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -120, 315, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -120, 356, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -120, 409, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 334, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 40, 390, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 40, 442, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 64, 338, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 64, 377, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -120, 64, 415, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 64, 450, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 336, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 366, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 403, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 88, 432, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_toilet_80181D14[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_toilet_80181D2C[13] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 56, -32, 575, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, 64, -104, 513, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, 72, -104, 495, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 80, -112, 457, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, 88, -112, 445, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 96, -120, 421, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 104, -120, 389, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 112, -120, 364, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 120, -120, 341, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 128, -120, 316, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 136, -120, 276, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 144, -120, 263, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 152, -120, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_toilet_80181E30[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_toilet_80181E48[39] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 112, 402, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 48, 765, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 40, 600, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -96, 24, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -160, 8, 327, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -128, 16, 402, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 96, 397, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -56, 700, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -8, 650, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -64, 612, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -8, 587, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -80, 616, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -88, 577, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -16, 510, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -16, 550, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -104, 600, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -40, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -40, 475, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -56, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -120, 481, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -120, 362, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, -160, -56, 337, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -56, 362, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -56, 400, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -120, 375, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -120, 425, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -8, 56, 850, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 836, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 880, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -8, -24, 956, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -24, 1020, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -24, 883, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, 0, 650, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -24, 24, 662, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, -24, 821, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 16, 685, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -16, 24, 902, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, 0, 8400, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_toilet_80182154[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 2, 0 } },
    { 27, 12, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_toilet_80182184[2] = {
    { { 150, 0, 168, 239 }, 875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_dryfield_toilet_80182198[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_toilet_801821A8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_toilet_801821B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_toilet_801821C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_toilet_801821D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_toilet_801821E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_toilet_801821F8[11] = {
    { { .empty = D_dryfield_toilet_801815B4 }, D_dryfield_toilet_801815B4, NULL },
    { { .elements = D_dryfield_toilet_801815C4 }, D_dryfield_toilet_80181948, NULL },
    { { .elements = D_dryfield_toilet_80181968 }, D_dryfield_toilet_80181D14, NULL },
    { { .elements = D_dryfield_toilet_80181D2C }, D_dryfield_toilet_80181E30, NULL },
    { { .elements = D_dryfield_toilet_80181E48 }, D_dryfield_toilet_80182154, D_dryfield_toilet_80182184 },
    { { .empty = D_dryfield_toilet_80182198 }, D_dryfield_toilet_80182198, NULL },
    { { .empty = D_dryfield_toilet_801821A8 }, D_dryfield_toilet_801821A8, NULL },
    { { .empty = D_dryfield_toilet_801821B8 }, D_dryfield_toilet_801821B8, NULL },
    { { .empty = D_dryfield_toilet_801821C8 }, D_dryfield_toilet_801821C8, NULL },
    { { .empty = D_dryfield_toilet_801821D8 }, D_dryfield_toilet_801821D8, NULL },
    { { .empty = D_dryfield_toilet_801821E8 }, D_dryfield_toilet_801821E8, NULL },
};

WorldCollisionTrigger D_dryfield_toilet_8018227C[6] = {
    { NULL, NULL, NULL, { 127, -992, 6, 0 }, { { -1024, -2016, 0, 0 }, { 1025, -2016, 0, 0 }, { -1024, 2016, 0, 0 }, { 1025, 2016, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2260, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 128, -1056, -96, 0 }, { { 1024, -2080, 0, 0 }, { -1024, -2080, 0, 0 }, { 1024, 2080, 0, 0 }, { -1024, 2080, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1184, -1088, -1568, 0 }, { { -157, -2112, -1018, 0 }, { 145, -2112, 1008, 0 }, { -157, 2112, -1018, 0 }, { 145, 2112, 1008, 0 } }, { 4052, 0, -606, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1120, -800, -1600, 0 }, { { 149, -1824, 1011, 0 }, { -152, -1824, -1014, 0 }, { 149, 1824, 1011, 0 }, { -152, 1824, -1014, 0 } }, { -4051, 0, 601, 0 }, { 0, 0, 4096, 0 }, 2079, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1857, -736, 799, 0 }, { { -1004, -1760, 199, 0 }, { 1005, -1760, -199, 0 }, { -1004, 1760, 199, 0 }, { 1005, 1760, -199, 0 } }, { -799, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2035, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1857, -1072, 638, 0 }, { { 1016, -2096, -53, 0 }, { -1027, -2096, 45, 0 }, { 1016, 2096, -53, 0 }, { -1027, 2096, 45, 0 } }, { 196, 0, 4115, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_toilet_80182444[9] = {
    { NULL, NULL, NULL, { -1808, -48, 1728, 0 }, { { -656, 0, -256, 0 }, { 656, 0, -256, 0 }, { -656, 0, 256, 0 }, { 656, 0, 256, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 729, -64, -1472, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 765, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 764, -64, 448, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 768, -64, 1408, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1824, -64, -1296, 0 }, { { -432, 0, -112, 0 }, { 848, 0, -112, 0 }, { -432, 0, 1232, 0 }, { 848, 0, 1232, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -400, -64, 544, 0 }, { { -512, 0, -1472, 0 }, { 512, 0, -1472, 0 }, { -512, 0, 1472, 0 }, { 512, 0, 1472, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1088, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1856, -64, -1344, 0 }, { { -320, 0, -704, 0 }, { 320, 0, -704, 0 }, { -320, 0, 704, 0 }, { 320, 0, 704, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_toilet_801826F0[1] = {
    { NULL, NULL, { -864, -1488, 624, 0 }, { { 0, 1904, -1520, 0 }, { 0, -1904, -1520, 0 }, { 0, 1904, 1520, 0 }, { 0, -1904, 1520, 0 } }, { 4109, 0, 0, 0 }, 2428, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_toilet_8018272C[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -810, -1260, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2290, -1470, -510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -310, -1470, -1790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 110, -1470, 1770 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
};

WorldCoordRoomLights D_dryfield_toilet_801828AC[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_toilet_8018272C), D_dryfield_toilet_8018272C, 0, NULL },
};

AreaResource D_dryfield_toilet_801828C4[3] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401000_80155004 },
    { 9, 233, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_323300_8017255C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_toilet_801828E8[2] = {
    { 12, 12, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101200_80138E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_toilet_80182900[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_toilet_80182918[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B2A4, D_dryfield_toilet_801828C4 },
    { D_map_dryfield_8017B2D4, D_dryfield_toilet_801828E8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017B354, D_dryfield_toilet_80182900 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

SVECTOR D_dryfield_toilet_80182980[325] = {
#include "assets/dryfield_toilet_morph_053C0.inc"
};

SVECTOR D_dryfield_toilet_801833A8[1605] = {
#include "assets/dryfield_toilet_morph_05DE8.inc"
};

ModelMorph D_dryfield_toilet_801865D0 = { D_dryfield_toilet_80182980, D_dryfield_toilet_801833A8, D_dryfield_toilet_8018662C, D_dryfield_toilet_8018705C, 325, 1604, 0, 325 };

WorldCollisionFootstepSounds D_dryfield_toilet_801865E8 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_toilet_801865F4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_toilet_801865FC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_toilet_801865E8 },
};

WorldCollisionSurfaceProperties D_dryfield_toilet_80186604[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_toilet_801865E8 },
};

WorldCollisionSurfaceProperties* D_dryfield_toilet_8018660C[8] = {
    D_dryfield_toilet_801865F4,
    D_dryfield_toilet_801865FC,
    D_dryfield_toilet_80186604,
    D_dryfield_toilet_801865F4,
    D_dryfield_toilet_801865F4,
    D_dryfield_toilet_801865F4,
    D_dryfield_toilet_801865F4,
    D_dryfield_toilet_801865F4,
};

SVECTOR D_dryfield_toilet_8018662C[326] = { 0 };

SVECTOR D_dryfield_toilet_8018705C[1604] = { 0 };

void func_dryfield_toilet_8017DCF0(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    EffectWork* spawned;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
            return;
        }
        if (arg0->state == 0) {
            coord->parent       = mem->parent;
            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state         = 1;
            mem->scale          = 0x30;
            mem->angle          = arg0->spawnArg1.value;
            if ((mem->pos.vx | mem->pos.vy | mem->pos.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vx     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vy     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vz     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS(&mem->pos, &mem->move);
        }
        actorRenderComposeCoord(coord);
        spawned = effectSpawn(EFFECT_DRYFIELD_TOILET_JET_PUFF, coord, 0x11180, 0);
        if (spawned != NULL) {
            gte_lddp(mem->scale - mem->age);
            gte_ldsv(&mem->move);
            gte_gpf12();
            gte_stsv(&spawned->move);
        }
        mem->age++;
        if (mem->age < mem->angle) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

/// Sets the screen corners of a jet puff's rotated square around its projected centre.
///
/// `projection` must be a live scratch block with a screen centre and positive
/// depth (SZ3 / 4). The signed halfword sources supply size and an angle in 4096
/// units per turn; this puff initializes both to 0..4095. Size times the cell's
/// 31-texel UV span divided by depth gives the half-diagonal in pixels before
/// rotation.
/// Each component rereads the sources and truncates the signed division before
/// its Q12 trig product. The second diagonal leaves its offsets in `projection`;
/// screen coordinates narrow to packet halfwords. No pointers are retained.
static inline void _dryfieldToiletSetJetPuffCorners(POLY_FT4* quad, OverlaySpriteScratch* projection, const s16* sizeSource, const s16* angleSource)
{
    enum { PUFF_SIZE_PROJECTION_SCALE = 31 }; // UV span of the 32-texel puff cell

    // Place opposite corners on each of two perpendicular screen-space diagonals.
    projection->cornerDx = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rsin(*angleSource)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    projection->cornerDy = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rcos(*angleSource)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    quad->x0             = projection->screenPos.vx + projection->cornerDx;
    quad->x3             = projection->screenPos.vx - projection->cornerDx;
    quad->y0             = projection->screenPos.vy - projection->cornerDy;
    quad->y3             = projection->screenPos.vy + projection->cornerDy;
    projection->cornerDx = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rsin(*angleSource + ROOM_VISUAL_EFFECTS_FULL_TURN / 4)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    projection->cornerDy = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rcos(*angleSource + ROOM_VISUAL_EFFECTS_FULL_TURN / 4)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    quad->x1             = projection->screenPos.vx + projection->cornerDx;
    quad->x2             = projection->screenPos.vx - projection->cornerDx;
    quad->y1             = projection->screenPos.vy - projection->cornerDy;
    quad->y2             = projection->screenPos.vy + projection->cornerDy;
}

void dryfieldToiletJetPuffTask(Task* task)
{
    enum {
        PUFF_INITIALIZE,
        PUFF_STATIONARY,
        PUFF_DRIFTING,
        PUFF_SIZE_MASK               = 0xFFF,
        PUFF_PERIOD_MASK             = 0xF000,
        PUFF_PERIOD_SHIFT            = 12,
        PUFF_RANDOM_MOVE_FLAG        = 0x100000,
        PUFF_SCALE_MOVE_FLAG         = 0x01000000,
        PUFF_RANDOM_MOVE_MASK        = 0x1F,
        PUFF_RANDOM_MOVE_BIAS        = 16,
        PUFF_FALL_ACCELERATION       = 3,
        PUFF_MIN_DEPTH               = 17,
        PUFF_TEXTURE_FRAME_COUNT     = 6,
        PUFF_TEXTURE_CELL_SHIFT      = 5,
        PUFF_TEXTURE_ROW             = 0x40,
        PUFF_TEXTURE_CELL_LAST       = 31,
        PUFF_TEXTURE_PAGE            = 0x2B,
        PUFF_TEXTURE_PALETTE         = 0x43C0,
        PUFF_RAW_TEXTURE_BLEND_FLAGS = 3
    };

    EffectWork*           work;
    GfxCoord*             coord;
    OverlaySpriteScratch* scratchEnd;
    OverlaySpriteScratch* projection;
    POLY_FT4*             quad;
    s32                   randomBits;
    s16                   framePeriod;
    SVECTOR*              velocity;
    u16                   worldX;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Project the centre and reserve its packet before advancing the effect.
    actorRenderComposeCoord(coord);
    scratchEnd = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    worldX     = coord->workm.t[0];
    SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
    projection              = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    projection->worldPos.vx = worldX;
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(scratchEnd - 1)->worldPos);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&(scratchEnd - 1)->screenPos);
    gte_stszotz(&projection->otz);
    if ((scratchEnd - 1)->otz >= PUFF_MIN_DEPTH) {
        if (task->state == PUFF_INITIALIZE) {
            randomBits  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale = task->spawnArg1.value & PUFF_SIZE_MASK;
            work->angle = ((u32)randomBits >> 16) & (ROOM_VISUAL_EFFECTS_FULL_TURN - 1);
            // Keep the signed halfword: nibble values 8..15 decode as negative.
            framePeriod     = task->spawnArg1.value & PUFF_PERIOD_MASK;
            gRandomLcgState = randomBits;
            if (framePeriod != 0) {
                framePeriod = framePeriod >> PUFF_PERIOD_SHIFT;
            } else {
                framePeriod = 1;
            }
            work->period = framePeriod;
            if (task->spawnArg1.value & PUFF_RANDOM_MOVE_FLAG) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = ((gRandomLcgState >> 16) & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = ((gRandomLcgState >> 16) & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = ((gRandomLcgState >> 16) & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
            }
            if (task->spawnArg1.value & PUFF_SCALE_MOVE_FLAG) {
                gte_lddp(work->scale << 2);
                velocity = &work->move;
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            task->state = PUFF_STATIONARY;
            if (work->move.vx | work->move.vy | work->move.vz) {
                task->state = PUFF_DRIFTING;
            }
        }
        quad->tpage = PUFF_TEXTURE_PAGE;
        quad->clut  = PUFF_TEXTURE_PALETTE;
        quad->code |= PUFF_RAW_TEXTURE_BLEND_FLAGS;
        setUVWH(quad, (work->age / work->period) << PUFF_TEXTURE_CELL_SHIFT, PUFF_TEXTURE_ROW,
                PUFF_TEXTURE_CELL_LAST, PUFF_TEXTURE_CELL_LAST);
        _dryfieldToiletSetJetPuffCorners(quad, projection, &work->scale, &work->angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
    // Pausing freezes age and drift after drawing; cancellation also draws once.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
            return;
        }
        if (task->state == PUFF_DRIFTING) {
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->move.vy      += PUFF_FALL_ACCELERATION;
        }
        work->age++;
        if (work->age <= work->period * PUFF_TEXTURE_FRAME_COUNT - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void dryfieldToiletConfigureEffectsTask(Task* task)
{
    enum { EFFECTS_UNREGISTERED,
           EFFECTS_REGISTERED };

    if (task->state == EFFECTS_UNREGISTERED) {
        gRoomEffectGlowDiscId     = EFFECT_DRYFIELD_TOILET_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_DRYFIELD_TOILET_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_DRYFIELD_TOILET_ORANGE_BURST_2;
        task->state               = EFFECTS_REGISTERED;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_dryfield_toilet_8017E69C(Task* arg0)
{
    _roomVisualEffectsGlowDiscTask(arg0);
}

void dryfieldToiletFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void dryfieldToiletFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
