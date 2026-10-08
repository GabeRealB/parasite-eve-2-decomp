#include "rooms/acropolis_patio.h"

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
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern TaskDesc         D_acropolis_patio_801802BC[];
extern TaskDesc         D_acropolis_patio_80182800;
extern TaskMessageEntry D_acropolis_patio_8018028C[6];
extern ActorTransform   D_acropolis_patio_80180428;
extern s32              D_acropolis_patio_80180440;
extern s32              D_acropolis_patio_8018044C;
extern ActorTransform   D_acropolis_patio_8018046C;
extern Task*            D_acropolis_patio_80187060;
extern EvsCommand       D_acropolis_patio_80180DEC[];
extern EvsCommand       D_acropolis_patio_80180EDC[];
extern u8               D_acropolis_patio_80187064;
extern u8               D_acropolis_patio_80187065;
extern EvsCommand       D_acropolis_patio_80180484[];
extern EvsCommand       D_acropolis_patio_801806AC[];
extern EvsCommand       D_acropolis_patio_8018082C[];
extern EvsCommand       D_acropolis_patio_80180C64[];
extern EvsCommand       D_acropolis_patio_8018280C[];
extern EvsCommand       D_acropolis_patio_80182BE4[];

/// The 14 anchor points of the patio's fountain spray, in the room object's own
/// space. The first three double as the jitter centres for the mist burst.
extern SVECTOR D_acropolis_patio_80182DDC[14];

/// Per-anchor camera-view mask, one bit per 1-based `GameSession::location.loc.view`
/// view: anchor `i` only draws while the room is being seen from a view its
/// mask names.
extern u16 D_acropolis_patio_80182E4C[14];

static void func_acropolis_patio_8017D5EC(Task* arg0);
static void _acropolisPatioRoomIdleState(Task* task);

/// Key-item use message handled by the patio's room task.
enum { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Packed fountain-jet argument fields and the fixed sprite-sheet geometry.
enum {
    ACROPOLIS_PATIO_JET_ANCHOR_MASK   = 0xF,
    ACROPOLIS_PATIO_JET_CELL_SHIFT    = 8,
    ACROPOLIS_PATIO_JET_CELL_MASK     = 3,
    ACROPOLIS_PATIO_JET_SIZE_SHIFT    = 16,
    ACROPOLIS_PATIO_JET_SIZE_MASK     = 0xFFF,
    ACROPOLIS_PATIO_JET_DEFAULT_SIZE  = 640,
    ACROPOLIS_PATIO_JET_MIN_DEPTH     = 0x11,
    ACROPOLIS_PATIO_JET_TEXTURE_PAGE  = 0x2B,
    ACROPOLIS_PATIO_JET_CLUT_Y        = 0x10E,
    ACROPOLIS_PATIO_JET_CELL_SIZE     = 40,
    ACROPOLIS_PATIO_JET_FLICKER_SHIFT = 4
};

/// State table of the room's three-state task dispatcher
/// (`func_acropolis_patio_8017DF8C`): the entry tick, an idle state, then
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_patio_8017D5C4 = {
    { func_acropolis_patio_8017D5EC, _acropolisPatioRoomIdleState, taskKill },
};

extern WorldCollisionGrid     D_acropolis_patio_80183DF8[1];
extern WorldCollisionOccluder D_acropolis_patio_80184964[2];
extern WorldCollisionTrigger  D_acropolis_patio_80183E1C[14];
extern WorldCollisionTrigger  D_acropolis_patio_80184244[12];
extern WorldCollisionTrigger  D_acropolis_patio_801845D4[12];
extern WorldCoordRoomLights   D_acropolis_patio_80186D44[1];

extern AnimationPlayRequest D_acropolis_patio_8018261C;
extern ActorTransform       D_acropolis_patio_80182690;
extern ActorTransform       D_acropolis_patio_801827BC;
extern ActorTransform       D_acropolis_patio_801827D4;
void                        func_acropolis_patio_8017DFE4(s32);

extern AnimationPlayRequest     D_acropolis_patio_8018270C;
extern AnimationPlayRequest     D_acropolis_patio_80182720;
extern AnimationBankCopyRequest D_acropolis_patio_801825C4;
extern ActorTransform           D_acropolis_patio_80182630;
extern ActorTransform           D_acropolis_patio_80182678;
void                            func_acropolis_patio_8017DFE4(s32);
void                            func_acropolis_patio_8017E024(void);
static void                     _acropolisPatioPlayerTurnLeftTask(Task* task);

extern SpriteBatch  D_acropolis_patio_80184AF0[2];
extern SpriteBatch  D_acropolis_patio_80184D1C[7];
extern SpriteBatch  D_acropolis_patio_801850D8[15];
extern SpriteBatch  D_acropolis_patio_8018527C[5];
extern SpriteBatch  D_acropolis_patio_801852A4[2];
extern SpriteBatch  D_acropolis_patio_801853B8[4];
extern SpriteBatch  D_acropolis_patio_8018557C[5];
extern SpriteBatch  D_acropolis_patio_80185A04[20];
extern SpriteBatch  D_acropolis_patio_80185E50[15];
extern SpriteBatch  D_acropolis_patio_80185EC8[2];
extern SpriteBatch  D_acropolis_patio_80185ED8[2];
extern SpriteBatch  D_acropolis_patio_80185EE8[2];
extern SpriteBatch  D_acropolis_patio_80185F08[2];
extern SpriteBatch  D_acropolis_patio_80185F18[2];
extern SpriteBatch  D_acropolis_patio_80185F28[2];
extern SpriteSource D_acropolis_patio_80184B00[27];
extern SpriteSource D_acropolis_patio_80184D54[45];
extern SpriteSource D_acropolis_patio_80185150[15];
extern SpriteSource D_acropolis_patio_801852B4[13];
extern SpriteSource D_acropolis_patio_801853D8[21];
extern SpriteSource D_acropolis_patio_801855A4[56];
extern SpriteSource D_acropolis_patio_80185AA4[47];

extern AnimationPlayRequest D_acropolis_patio_8018037C;
extern AnimationSet*        D_acropolis_patio_80180364[6];
extern ActorTransform       D_acropolis_patio_801802EC;
extern ActorTransform       D_acropolis_patio_80180304;
extern ActorTransform       D_acropolis_patio_8018031C;
extern ActorTransform       D_acropolis_patio_80180334;
extern ActorTransform       D_acropolis_patio_8018034C;
static void                 _acropolisPatioSetPlayerHeadAimState(s32 headAimState);
static void                 _acropolisPatioActivateRoom2(void);
static void                 _acropolisPatioSetActorControl(u8 actorControl);

s32         func_acropolis_patio_8017D7D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32         func_acropolis_patio_8017DCE4(Task*, s32, s32, s32);
static s32  _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg);
void        func_acropolis_patio_8017DA5C(Task*);
s32         func_acropolis_patio_8017DBAC(Task*, s32, const void*, s32);
void        func_acropolis_patio_8017DD80(Task*);
static void _acropolisPatioPlayerHeadAimTask(Task* task);

/// States selected by the opening scene's player-head-aim callbacks.
enum {
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE        = 1,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_HOLD        = 2,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_START_SLIDE = 3,
};

static AnimationPackedPose _gAcropolisPatioAnimation018A0Bank1[2] = {
#include "assets/acropolis_patio_animation_018A0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation018A0Bank4[20] = {
#include "assets/acropolis_patio_animation_018A0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation018A0Records[57] = {
#include "assets/acropolis_patio_animation_018A0_records.inc"
};

static u16 _gAcropolisPatioAnimation018A0Indices[20] = {
#include "assets/acropolis_patio_animation_018A0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation018A0 = {
    _gAcropolisPatioAnimation018A0Records,
    _gAcropolisPatioAnimation018A0Indices,
    { NULL, _gAcropolisPatioAnimation018A0Bank1, NULL, NULL, _gAcropolisPatioAnimation018A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation01B48Bank1[2] = {
#include "assets/acropolis_patio_animation_01B48_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation01B48Bank4[41] = {
#include "assets/acropolis_patio_animation_01B48_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation01B48Records[103] = {
#include "assets/acropolis_patio_animation_01B48_records.inc"
};

static u16 _gAcropolisPatioAnimation01B48Indices[20] = {
#include "assets/acropolis_patio_animation_01B48_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation01B48 = {
    _gAcropolisPatioAnimation01B48Records,
    _gAcropolisPatioAnimation01B48Indices,
    { NULL, _gAcropolisPatioAnimation01B48Bank1, NULL, NULL, _gAcropolisPatioAnimation01B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation01CE4Bank1[2] = {
#include "assets/acropolis_patio_animation_01CE4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation01CE4Bank4[20] = {
#include "assets/acropolis_patio_animation_01CE4_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation01CE4Records[57] = {
#include "assets/acropolis_patio_animation_01CE4_records.inc"
};

static u16 _gAcropolisPatioAnimation01CE4Indices[20] = {
#include "assets/acropolis_patio_animation_01CE4_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation01CE4 = {
    _gAcropolisPatioAnimation01CE4Records,
    _gAcropolisPatioAnimation01CE4Indices,
    { NULL, _gAcropolisPatioAnimation01CE4Bank1, NULL, NULL, _gAcropolisPatioAnimation01CE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation022B8Bank1[12] = {
#include "assets/acropolis_patio_animation_022B8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation022B8Bank4[138] = {
#include "assets/acropolis_patio_animation_022B8_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation022B8Records[179] = {
#include "assets/acropolis_patio_animation_022B8_records.inc"
};

static u16 _gAcropolisPatioAnimation022B8Indices[20] = {
#include "assets/acropolis_patio_animation_022B8_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation022B8 = {
    _gAcropolisPatioAnimation022B8Records,
    _gAcropolisPatioAnimation022B8Indices,
    { NULL, _gAcropolisPatioAnimation022B8Bank1, NULL, NULL, _gAcropolisPatioAnimation022B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation02AD0Bank1[19] = {
#include "assets/acropolis_patio_animation_02AD0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation02AD0Bank4[189] = {
#include "assets/acropolis_patio_animation_02AD0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation02AD0Records[252] = {
#include "assets/acropolis_patio_animation_02AD0_records.inc"
};

static u16 _gAcropolisPatioAnimation02AD0Indices[20] = {
#include "assets/acropolis_patio_animation_02AD0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation02AD0 = {
    _gAcropolisPatioAnimation02AD0Records,
    _gAcropolisPatioAnimation02AD0Indices,
    { NULL, _gAcropolisPatioAnimation02AD0Bank1, NULL, NULL, _gAcropolisPatioAnimation02AD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation02CA4Bank1[2] = {
#include "assets/acropolis_patio_animation_02CA4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation02CA4Bank4[15] = {
#include "assets/acropolis_patio_animation_02CA4_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation02CA4Records[76] = {
#include "assets/acropolis_patio_animation_02CA4_records.inc"
};

static u16 _gAcropolisPatioAnimation02CA4Indices[20] = {
#include "assets/acropolis_patio_animation_02CA4_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation02CA4 = {
    _gAcropolisPatioAnimation02CA4Records,
    _gAcropolisPatioAnimation02CA4Indices,
    { NULL, _gAcropolisPatioAnimation02CA4Bank1, NULL, NULL, _gAcropolisPatioAnimation02CA4Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_acropolis_patio_8018028C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_patio_8017D7D0 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_patio_8017DCE4 },
    { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM, _acropolisPatioRefuseKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisPatioHandleSoundMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_patio_8017DBAC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_patio_801802BC[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_patio_8017DD80, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_patio_8017DA5C, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _acropolisPatioPlayerHeadAimTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

ActorTransform D_acropolis_patio_801802EC = { { -5255, 1, -124, 0 }, { 0, -800, 0, 0 } };

ActorTransform D_acropolis_patio_80180304 = { { -7195, 1, 484, 0 }, { 0, -800, 0, 0 } };

ActorTransform D_acropolis_patio_8018031C = { { -6852, 1, 345, 0 }, { 0, -800, 0, 0 } };

ActorTransform D_acropolis_patio_80180334 = { { -6612, 1, 315, 0 }, { 0, -800, 0, 0 } };

ActorTransform D_acropolis_patio_8018034C = { { 1452, 1, 593, 0 }, { 0, -1024, 0, 0 } };

AnimationSet* D_acropolis_patio_80180364[6] = {
    &_gAcropolisPatioAnimation018A0,
    &_gAcropolisPatioAnimation01CE4,
    &_gAcropolisPatioAnimation01B48,
    &_gAcropolisPatioAnimation02CA4,
    &_gAcropolisPatioAnimation022B8,
    &_gAcropolisPatioAnimation02AD0,
};

AnimationPlayRequest D_acropolis_patio_8018037C = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_patio_80180390 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_patio_801803A4 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_patio_801803B8 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_patio_801803CC = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_patio_801803E0 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_acropolis_patio_801803F4 = { { .sets = D_acropolis_patio_80180364 }, ARRAY_SIZE(D_acropolis_patio_80180364) };

AnimationPlayRequest D_acropolis_patio_801803FC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_patio_80180410 = { { -7960, 0, 320, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_patio_80180428 = { { -7720, 0, 290, 0 }, { 0, 2048, 0, 0 } };

s32 D_acropolis_patio_80180440 = 769;

ActorCommand D_acropolis_patio_80180444 = { { .loc = { 1, 3 } }, 1 };

ActorCommand D_acropolis_patio_80180448 = { { .loc = { 1, 3 } }, 2 };

s32 D_acropolis_patio_8018044C = 0x30301;

ActorCommand D_acropolis_patio_80180450 = { { .loc = { 1, 3 } }, 4 };

ActorTransform D_acropolis_patio_80180454 = { { -7960, 0, 320, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_patio_8018046C = { { -7720, 0, 290, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_acropolis_patio_80180484[23] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51030009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180444 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180454 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180410 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180444 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180428 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180450 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_8018034C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018046C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_801806AC[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180428 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180450 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_8018034C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018046C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_8018082C[45] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_patio_801803F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_HOLD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180410 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180450 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_801802EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_acropolis_patio_80180304 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_80180448 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_8018031C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_8018037C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &D_acropolis_patio_8018044C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_START_SLIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisPatioActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_80180334 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_80180428 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80180C64[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_80180334 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisPatioActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsSceneKey D_acropolis_patio_80180DE4 = { 1, 29, 11 };

EvsCommand D_acropolis_patio_80180DEC[10] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_patio_80180DE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80180EDC[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_801803FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gAcropolisPatioAnimation03CD0Bank1[6] = {
#include "assets/acropolis_patio_animation_03CD0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation03CD0Bank4[46] = {
#include "assets/acropolis_patio_animation_03CD0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation03CD0Records[109] = {
#include "assets/acropolis_patio_animation_03CD0_records.inc"
};

static u16 _gAcropolisPatioAnimation03CD0Indices[20] = {
#include "assets/acropolis_patio_animation_03CD0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation03CD0 = {
    _gAcropolisPatioAnimation03CD0Records,
    _gAcropolisPatioAnimation03CD0Indices,
    { NULL, _gAcropolisPatioAnimation03CD0Bank1, NULL, NULL, _gAcropolisPatioAnimation03CD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation04470Bank1[13] = {
#include "assets/acropolis_patio_animation_04470_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation04470Bank4[179] = {
#include "assets/acropolis_patio_animation_04470_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation04470Records[250] = {
#include "assets/acropolis_patio_animation_04470_records.inc"
};

static u16 _gAcropolisPatioAnimation04470Indices[20] = {
#include "assets/acropolis_patio_animation_04470_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation04470 = {
    _gAcropolisPatioAnimation04470Records,
    _gAcropolisPatioAnimation04470Indices,
    { NULL, _gAcropolisPatioAnimation04470Bank1, NULL, NULL, _gAcropolisPatioAnimation04470Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation0494CBank1[9] = {
#include "assets/acropolis_patio_animation_0494C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation0494CBank4[109] = {
#include "assets/acropolis_patio_animation_0494C_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation0494CRecords[155] = {
#include "assets/acropolis_patio_animation_0494C_records.inc"
};

static u16 _gAcropolisPatioAnimation0494CIndices[20] = {
#include "assets/acropolis_patio_animation_0494C_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation0494C = {
    _gAcropolisPatioAnimation0494CRecords,
    _gAcropolisPatioAnimation0494CIndices,
    { NULL, _gAcropolisPatioAnimation0494CBank1, NULL, NULL, _gAcropolisPatioAnimation0494CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation04FC8Bank1[11] = {
#include "assets/acropolis_patio_animation_04FC8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation04FC8Bank4[161] = {
#include "assets/acropolis_patio_animation_04FC8_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation04FC8Records[201] = {
#include "assets/acropolis_patio_animation_04FC8_records.inc"
};

static u16 _gAcropolisPatioAnimation04FC8Indices[20] = {
#include "assets/acropolis_patio_animation_04FC8_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation04FC8 = {
    _gAcropolisPatioAnimation04FC8Records,
    _gAcropolisPatioAnimation04FC8Indices,
    { NULL, _gAcropolisPatioAnimation04FC8Bank1, NULL, NULL, _gAcropolisPatioAnimation04FC8Bank4, NULL, NULL, NULL },
};

AnimationSet* D_acropolis_patio_801825B0[5] = {
    NULL,
    &_gAcropolisPatioAnimation03CD0,
    &_gAcropolisPatioAnimation04470,
    &_gAcropolisPatioAnimation0494C,
    &_gAcropolisPatioAnimation04FC8,
};

AnimationBankCopyRequest D_acropolis_patio_801825C4 = { { .sets = D_acropolis_patio_801825B0 }, ARRAY_SIZE(D_acropolis_patio_801825B0) };

AnimationPlayRequest D_acropolis_patio_801825CC[4] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_acropolis_patio_8018261C = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_patio_80182630 = { { -7100, 0, -1350, 0 }, { 0, 900, 0, 0 } };

ActorTransform D_acropolis_patio_80182648[2] = {
    { { -6590, 0, -1000, 0 }, { 0, 0, 0, 0 } },
    { { 3900, 0, 2300, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_acropolis_patio_80182678 = { { -2300, 0, 700, 0 }, { 0, 1200, 0, 0 } };

ActorTransform D_acropolis_patio_80182690 = { { -2300, 0, 700, 0 }, { 0, -848, 0, 0 } };

AnimationPlayRequest D_acropolis_patio_801826A8[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_acropolis_patio_8018270C = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_acropolis_patio_80182720 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_acropolis_patio_80182734[2] = {
    { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

ActorTransform D_acropolis_patio_8018275C = { { -6100, -3780, -1900, 0 }, { 0, -110, 0, 0 } };

ActorTransform D_acropolis_patio_80182774 = { 0 };

ActorTransform D_acropolis_patio_8018278C = { { -5100, -3780, -1900, 0 }, { 0, 80, 0, 0 } };

ActorTransform D_acropolis_patio_801827A4 = { { 0, 0, 0, 0 }, { 0, 512, 0, 0 } };

ActorTransform D_acropolis_patio_801827BC = { { -6100, 210, 20, 0 }, { 0, 896, 0, 0 } };

ActorTransform D_acropolis_patio_801827D4 = { { -3743, 210, -543, 0 }, { 0, 512, 0, 0 } };

ActorMotionWalkAnim D_acropolis_patio_801827EC = { .animationId = 7, .nextAnimId = 8 };

ActorCommand D_acropolis_patio_801827F4 = { { .loc = { 1, 3 } }, 0 };

ActorCommand D_acropolis_patio_801827F8 = { { .loc = { 0, 0 } }, 1 };

ActorCommand D_acropolis_patio_801827FC = { { .loc = { 0, 0 } }, 2 };

TaskDesc D_acropolis_patio_80182800 = { { { TASK_BODY_NONE, 192 } }, _acropolisPatioPlayerTurnLeftTask, { .value = 0 } };

EvsCommand D_acropolis_patio_8018280C[41] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_patio_801825C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_80182630 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018275C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018278C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_patio_8018270C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_patio_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_acropolis_patio_80182678 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_acropolis_patio_80182774 } }, { .message = { .pointer = &D_acropolis_patio_801827EC } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_acropolis_patio_801827A4 } }, { .message = { .pointer = &D_acropolis_patio_801827EC } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_8018261C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_acropolis_patio_8017E024 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_patio_8017DFE4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_801827BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_801827D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827FC } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80182BE4[21] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_patio_8017DFE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_801827BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_801827D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_patio_80182690 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_patio_8018261C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_patio_8017DFE4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_acropolis_patio_80182DDC[14] = {
    { 180, -1108, 3610, 0 },
    { -6250, -1108, 2580, 0 },
    { -9310, -1108, 1130, 0 },
    { 1040, -2120, -4500, 0 },
    { -2390, -2120, -4500, 0 },
    { -6360, -2120, -4570, 0 },
    { -7640, -2120, -4570, 0 },
    { 0, -210, 2950, 0 },
    { -2010, -210, 2600, 0 },
    { -4010, -210, 2210, 0 },
    { -6000, -210, 1720, 0 },
    { -8010, -210, 1190, 0 },
    { -8510, -210, 230, 0 },
    { -8520, -210, -1000, 0 },
};

u16 D_acropolis_patio_80182E4C[14] = {
    4,
    8,
    24,
    384,
    384,
    192,
    64,
    4,
    4,
    12,
    24,
    24,
    24,
    88,
};

WorldCollisionRoomResources D_acropolis_patio_80182E68[3] = {
    { D_acropolis_patio_80183DF8, D_acropolis_patio_80183E1C, D_acropolis_patio_80184244, D_acropolis_patio_80184964 },
    { D_acropolis_patio_80183DF8, D_acropolis_patio_80183E1C, D_acropolis_patio_801845D4, D_acropolis_patio_80184964 },
    { D_acropolis_patio_80183DF8, D_acropolis_patio_80183E1C, D_acropolis_patio_801845D4, D_acropolis_patio_80184964 },
};

u8 D_acropolis_patio_80182E98[20] = {
    1,
    2,
    3,
    17,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

u8 D_acropolis_patio_80182EAC[20] = {
    1,
    2,
    3,
    17,
    5,
    6,
    13,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

u8* D_acropolis_patio_80182EC0[3] = {
    gViewIdentityMap,
    D_acropolis_patio_80182E98,
    D_acropolis_patio_80182EAC,
};

ViewCount D_acropolis_patio_80182ECC[3] = { 19, 19, 19 };

WorldCoordRoomLighting D_acropolis_patio_80182ED4[3] = {
    { D_acropolis_patio_80186D44, NULL },
    { D_acropolis_patio_80186D44, NULL },
    { D_acropolis_patio_80186D44, NULL },
};

DirectionWarpEntry D_acropolis_patio_80182EEC[4] = {
    { { { .word = 2560 }, 3906, 1, 768 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 3906, 1, 768 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 4986, 1, -4079 }, { 0, 0, 0, 0 }, { { .word = 0 }, 4986, 1, -4079 }, { 0, 0, 0, 0 }, 0x51030002, 0x51030001, 0x51030007, 6, DIRECTION_WARP_FLAG_NONE, 496 },
    { { { .word = 0 }, -7014, -302, -4439 }, { 0, 0, 0, 0 }, { { .word = 0 }, -7014, -302, -4439 }, { 0, 0, 0, 0 }, 0x51030005, 0x51030004, 0x51030006, 7, DIRECTION_WARP_FLAG_NONE, 501 },
    { { { .word = 2304 }, 4223, -460, 1263 }, { 0, 0, 0, 0 }, { { .word = 2304 }, 4408, -600, 1264 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gAcropolisPatioCollision06838Normals[33] = {
#include "assets/acropolis_patio_collision_06838_normals.inc"
};

static SVECTOR _gAcropolisPatioCollision06838Verts[201] = {
#include "assets/acropolis_patio_collision_06838_verts.inc"
};

static WorldCollisionGridFace _gAcropolisPatioCollision06838Faces[70] = {
#include "assets/acropolis_patio_collision_06838_faces.inc"
};

static s16 _gAcropolisPatioCollision06838Cells[402] = {
#include "assets/acropolis_patio_collision_06838_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisPatioCollision06838Cells[i])
static s16* _gAcropolisPatioCollision06838Table[28] = {
#include "assets/acropolis_patio_collision_06838_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_patio_80183DF8[1] = {
    { NULL, _gAcropolisPatioCollision06838Normals, _gAcropolisPatioCollision06838Verts, _gAcropolisPatioCollision06838Faces, _gAcropolisPatioCollision06838Table, 9010, 7675, 7, 4, 4000, 70 },
};

WorldCollisionTrigger D_acropolis_patio_80183E1C[14] = {
    { NULL, NULL, NULL, { 4988, -2288, -1890, 0 }, { { -2356, -3568, 233, 0 }, { 2357, -3568, -233, 0 }, { -2356, 3568, 233, 0 }, { 2357, 3568, -233, 0 } }, { -406, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5052, -2528, -2276, 0 }, { { 2357, -3552, -233, 0 }, { -2356, -3552, 233, 0 }, { 2357, 3552, -233, 0 }, { -2356, 3552, 233, 0 } }, { 404, 0, 4087, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1938, -2400, 374, 0 }, { { 906, -3568, 2187, 0 }, { -905, -3568, -2187, 0 }, { 906, 3568, 2187, 0 }, { -905, 3568, -2187, 0 } }, { -3811, 0, 1577, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1557, -2464, 439, 0 }, { { -797, -3568, -2230, 0 }, { 797, -3568, 2231, 0 }, { -797, 3568, -2230, 0 }, { 797, 3568, 2231, 0 } }, { 3885, 0, -1389, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3872, -2432, 256, 0 }, { { 0, -3568, -2368, 0 }, { 0, -3568, 2368, 0 }, { 0, 3568, -2368, 0 }, { 0, 3568, 2368, 0 } }, { 4125, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3648, -2400, 192, 0 }, { { 0, -3568, 2368, 0 }, { 0, -3568, -2368, 0 }, { 0, 3568, 2368, 0 }, { 0, 3568, -2368, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7762, -2432, -2162, 0 }, { { 2171, -3568, -129, 0 }, { -2170, -3568, 130, 0 }, { 2171, 3568, -129, 0 }, { -2170, 3568, 130, 0 } }, { 244, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7562, -2432, -1771, 0 }, { { -2121, -3568, 239, 0 }, { 2122, -3568, -239, 0 }, { -2121, 3568, 239, 0 }, { 2122, 3568, -239, 0 } }, { -459, 0, -4071, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6435, -2400, -3410, 0 }, { { -107, -3568, -906, 0 }, { 107, -3568, 906, 0 }, { -107, 3568, -906, 0 }, { 107, 3568, 906, 0 } }, { 4068, 0, -481, 0 }, { 0, 0, 4096, 0 }, 3674, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6294, -2464, -3156, 0 }, { { 48, -3568, 561, 0 }, { -47, -3568, -560, 0 }, { 48, 3568, 561, 0 }, { -47, 3568, -560, 0 } }, { -4086, 0, 345, 0 }, { 0, 0, 4096, 0 }, 3611, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1537, -2496, -3841, 0 }, { { 333, -3568, 1513, 0 }, { -333, -3568, -1513, 0 }, { 333, 3568, 1513, 0 }, { -333, 3568, -1513, 0 } }, { -4003, 0, 880, 0 }, { 0, 0, 4096, 0 }, 3882, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2240, -2432, -3824, 0 }, { { -269, -3568, -1433, 0 }, { 269, -3568, 1433, 0 }, { -269, 3568, -1433, 0 }, { 269, 3568, 1433, 0 } }, { 4034, 0, -759, 0 }, { 0, 0, 4096, 0 }, 3848, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5937, 0, -4513, 0 }, { { 787, -3568, -1098, 0 }, { -786, -3568, 1099, 0 }, { 787, 3568, -1098, 0 }, { -786, 3568, 1099, 0 } }, { 3340, 0, 2391, 0 }, { 0, 0, 4096, 0 }, 3814, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5890, 0, -4385, 0 }, { { -463, -3568, 692, 0 }, { 463, -3568, -692, 0 }, { -463, 3568, 692, 0 }, { 463, 3568, -692, 0 } }, { -3411, 0, -2283, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_patio_80184244[12] = {
    { NULL, NULL, NULL, { 3903, -96, 895, 0 }, { { -669, 0, -11, 0 }, { 492, 0, -445, 0 }, { -491, 0, 446, 0 }, { 670, 0, 12, 0 } }, { 0, 4109, 0, 0 }, { -1189, 0, -3920, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -64, -4128, 0 }, { { -711, 0, -432, 0 }, { 697, 0, -432, 0 }, { -728, 0, 433, 0 }, { 713, 0, 433, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, 8, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7265, -352, -4704, 0 }, { { -1031, 0, -592, 0 }, { 1017, 0, -592, 0 }, { -1016, 0, 593, 0 }, { 1033, 0, 593, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4689, -384, -4768, 0 }, { { -1335, 0, -592, 0 }, { 1321, 0, -592, 0 }, { -1320, 0, 593, 0 }, { 1337, 0, 593, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -384, -3272, 0 }, { { -1431, 0, 184, 0 }, { 1001, 0, -1288, 0 }, { -1448, 0, 1465, 0 }, { 1881, 0, -359, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4097, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3999, -720, 1183, 0 }, { { -1187, -1164, 440, 0 }, { 1204, -1166, -401, 0 }, { -1204, 1166, 402, 0 }, { 1188, 1164, -440, 0 } }, { -1360, -74, -3864, 0 }, { -1189, 0, -3920, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -603, 1408, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { 1189, 0, 3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 66, 144, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1680, -384, -4256, 0 }, { { -1895, 0, -880, 0 }, { 1881, 0, -880, 0 }, { -1880, 0, 881, 0 }, { 1897, 0, 881, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8066, -64, -881, 0 }, { { 621, 0, -386, 0 }, { 639, 0, 374, 0 }, { -637, 0, -372, 0 }, { -620, 0, 387, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, -202, 0 }, 738, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -29, 31, 0 }, { { 325, 0, 1661, 0 }, { -997, 0, -1557, 0 }, { 933, 0, 1461, 0 }, { -261, 0, -1565, 0 } }, { 0, 4115, 0, 0 }, { 3856, 0, -1380, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6543, 0, 48, 0 }, { { -749, 0, -1052, 0 }, { 956, 0, -1517, 0 }, { -1372, 0, 1133, 0 }, { 1165, 0, 1436, 0 } }, { 0, 4099, 0, 0 }, { -1189, 0, -3920, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -7201, -384, -2240, 0 }, { { -1586, 0, -428, 0 }, { 1574, 0, -446, 0 }, { -1572, 0, 446, 0 }, { 1587, 0, 429, 0 } }, { 0, 4112, 0, 0 }, { -1189, 0, -3920, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_patio_801845D4[12] = {
    { NULL, NULL, NULL, { 3903, -96, 959, 0 }, { { -669, 0, -11, 0 }, { 492, 0, -445, 0 }, { -491, 0, 446, 0 }, { 670, 0, 12, 0 } }, { 0, 4109, 0, 0 }, { -1189, 0, -3920, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -64, -4128, 0 }, { { -711, 0, -432, 0 }, { 697, 0, -432, 0 }, { -728, 0, 433, 0 }, { 713, 0, 433, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, 8, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7265, -352, -4704, 0 }, { { -1031, 0, -592, 0 }, { 1017, 0, -592, 0 }, { -1016, 0, 593, 0 }, { 1033, 0, 593, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4689, -384, -4768, 0 }, { { -1335, 0, -592, 0 }, { 1321, 0, -592, 0 }, { -1320, 0, 593, 0 }, { 1337, 0, 593, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -384, -3272, 0 }, { { -1431, 0, 184, 0 }, { 1001, 0, -1288, 0 }, { -1448, 0, 1465, 0 }, { 1881, 0, -359, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4097, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4128, -464, 1472, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { -1189, 0, -3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4128, -475, 1472, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { 1189, 0, 3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 66, 152, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1680, -384, -4256, 0 }, { { -1895, 0, -880, 0 }, { 1881, 0, -880, 0 }, { -1880, 0, 881, 0 }, { 1897, 0, 881, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8066, -64, -849, 0 }, { { 621, 0, -418, 0 }, { 639, 0, 406, 0 }, { -637, 0, -404, 0 }, { -620, 0, 419, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, -202, 0 }, 754, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -29, 31, 0 }, { { 325, 0, 1661, 0 }, { -997, 0, -1557, 0 }, { 933, 0, 1461, 0 }, { -261, 0, -1565, 0 } }, { 0, 4115, 0, 0 }, { 3856, 0, -1380, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -8239, 0, 592, 0 }, { { -749, 0, -1052, 0 }, { 1788, 0, -1517, 0 }, { -1372, 0, 1133, 0 }, { 1997, 0, 1436, 0 } }, { 0, 4101, 0, 0 }, { -1189, 0, -3920, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 11, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7201, -384, -2240, 0 }, { { -1586, 0, -428, 0 }, { 1574, 0, -446, 0 }, { -1572, 0, 446, 0 }, { 1587, 0, 429, 0 } }, { 0, 4112, 0, 0 }, { -1189, 0, -3920, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_acropolis_patio_80184964[2] = {
    { NULL, NULL, { -1184, -1760, -2112, 0 }, { { -3904, -1984, 0, 0 }, { 3904, -1984, 0, 0 }, { -3904, 1984, 0, 0 }, { 3904, 1984, 0, 0 } }, { 0, 0, -4098, 0 }, 4374, 1, 0 },
    { NULL, NULL, { 2336, -1728, -5888, 0 }, { { 0, -1984, 3904, 0 }, { 0, -1984, -3904, 0 }, { 0, 1984, 3904, 0 }, { 0, 1984, -3904, 0 } }, { -4098, 0, 0, 0 }, 4374, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_acropolis_patio_801849DC[3] = {
    { 19, 19, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101900_80149120 },
    { 107, 122, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_312200_80169F7C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_patio_80184A00[3] = {
    { 10, 170, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_317000_8016CF44 },
    { 19, 19, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101900_80149120 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_patio_80184A24[3] = {
    { 7, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D06E60 },
    { 8, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_patio_80184A48[3] = {
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401000_80155004 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_patio_80184A6C[2] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401000_80155004 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_patio_80184A84[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_patio_80184A90[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017ACFC, D_acropolis_patio_801849DC },
    { D_map_akropolis_8017AD2C, D_acropolis_patio_80184A00 },
    { D_map_akropolis_8017AD7C, D_acropolis_patio_80184A24 },
    { D_map_akropolis_8017ADEC, D_acropolis_patio_80184A48 },
    { D_map_akropolis_8017AE3C, D_acropolis_patio_80184A6C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017AE6C, D_acropolis_patio_80184A84 },
    { NULL, NULL },
};

SpriteBatch D_acropolis_patio_80184AF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_80184B00[27] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -88, -40, 1275, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -120, -40, 1200, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 112 } }, -160, -40, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -104, 1200, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, -96, 1175, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -96, 1150, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -96, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, -112, 1075, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -152, -112, 1100, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 1150, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 32, 1125, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 56, 1125, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 1100, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 72, 1075, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 1125, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 32, 1125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 72, 1050, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 112, 56, 1075, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 112, 40, 1100, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 112, -48, 1100, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 112, -120, 1100, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, -16, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 144, -16, 975, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 152, -16, 950, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -88, 1250, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -80, 1275, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -88, 1300, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_80184D1C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 6, 0, 0, { 3, 0 } },
    { 9, 12, 0, 0, { 2, 0 } },
    { 21, 3, 0, 0, { 4, 0 } },
    { 24, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_80184D54[45] = {
    { 142, 0x3FC0, { .fields = { 120, 144 } }, -160, -120, 2625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -112, -24, 2625, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 2700, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, -24, 2750, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -48, -56, 2625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -24, -48, 2700, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -48, -48, 2725, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, -72, -120, 2650, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 64, -88, 1650, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 72, -112, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -120, 1375, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, -120, 1250, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -120, 1250, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 88, -104, 1375, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 16 } }, 80, -96, 1500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 16 } }, 72, -80, 1625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, -64, 1650, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 72, -64, 1650, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 2500, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, 8, 1650, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 88, -80, 1450, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 72, 16, 1625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, 32, 1600, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, 32, 1425, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, 16, 1600, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, 16, 1450, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 160 } }, 128, -120, 1050, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 40, 1050, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 88, 48, 1150, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 104, 64, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 80, 1075, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 120, 88, 1050, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 104, 48, 1050, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 112, 64, 1200, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 80, 1100, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 88, 1050, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -16, 1550, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -16, 1525, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -16, 1500, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -8, 1300, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 104, -8, 1300, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 112, -8, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -8, 1200, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 96, -16, 2500, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, 8, 1500, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_801850D8[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 2, 0 } },
    { 1, 5, 0, 0, { 7, 0 } },
    { 6, 1, 0, 0, { 6, 0 } },
    { 7, 1, 0, 0, { 8, 0 } },
    { 8, 9, 0, 0, { 0, 0 } },
    { 17, 3, 0, 0, { 9, 0 } },
    { 20, 6, 0, 0, { 5, 0 } },
    { 26, 10, 0, 0, { 10, 0 } },
    { 36, 0, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 11, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, 1, 0, 0, { 12, 0 } },
    { 44, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_80185150[15] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -152, -104, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -136, -72, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 24, 1450, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 40, 1425, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -152, 24, 1250, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 32, 1200, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 40, 1175, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 48, 1350, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -8, 1150, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 24, 1150, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, -8, 1100, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -8, 1050, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -152, -8, 1000, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, 0, 950, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 32, 1375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_8018527C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 6, 0, 0, { 2, 0 } },
    { 14, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_801852A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_801852B4[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 24, 1325, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -120, 1300, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 128, -96, 1300, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 120, -32, 1300, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 8, 1325, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 24, 1250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 112, 8, 1250, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 24, 1225, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -24, 1200, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 0, 1200, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, -24, 1175, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, -24, 1150, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -24, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_801853B8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_801853D8[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 32, 837, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 920, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 862, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 72, 887, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -152, 56, 912, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, 16, 920, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 56, 912, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 72, 887, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, 88, 862, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -88, 88, 862, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 72, 887, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 56, 912, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 40, 925, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -24, 920, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -64, 920, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -104, 920, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -136, -120, 920, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 950, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 950, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 40, 968, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -152, 24, 968, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_8018557C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 2, 0, 0, { 2, 0 } },
    { 19, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_801855A4[56] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -152, -120, 725, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -152, -80, 725, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -144, 8, 725, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 64, 775, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 725, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, 80, 725, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 96, 700, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -152, 64, 775, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 72, 750, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, 80, 725, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 24 } }, -160, 96, 700, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -96, -104, 1375, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -88, -80, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 32, 1375, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -96, 16, 1400, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -112, 24, 1375, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -112, 40, 1350, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, -80, 2250, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -64, 2250, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, -48, 2250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 0, 2250, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 8, 2250, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -64, 3125, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, -8, 3125, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, -8, 1200, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, -8, 1150, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -96, -8, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -120, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -24, 1950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -24, 1825, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -16, 1700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -24, 2350, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 2325, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, -64, 3125, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -56, 3125, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -24, 3125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, -8, 3125, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -24, 3125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 40, 850, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 32, 900, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, 8, 950, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 24, 975, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, 16, 1000, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 32, 1025, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 8, 1025, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1025, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 0, 1625, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -16, 1775, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 0, 1775, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 16, 1775, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, -8, 1950, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -8, 2200, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 2375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -48, -24, 2375, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 0, 2375, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -16, 2375, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_80185A04[20] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 17, 0 } },
    { 11, 6, 0, 0, { 2, 0 } },
    { 17, 5, 0, 0, { 9, 0 } },
    { 22, 2, 0, 0, { 3, 0 } },
    { 24, 4, 0, 0, { 11, 0 } },
    { 28, 3, 0, 0, { 0, 0 } },
    { 31, 2, 0, 0, { 12, 0 } },
    { 33, 4, 0, 0, { 1, 0 } },
    { 37, 1, 0, 0, { 15, 0 } },
    { 38, 2, 0, 0, { 4, 0 } },
    { 40, 3, 0, 0, { 13, 0 } },
    { 43, 3, 0, 0, { 7, 0 } },
    { 46, 1, 0, 0, { 10, 0 } },
    { 47, 3, 0, 0, { 8, 0 } },
    { 50, 1, 0, 0, { 14, 0 } },
    { 51, 1, 0, 0, { 5, 0 } },
    { 52, 3, 0, 0, { 16, 0 } },
    { 55, 1, 0, 0, { 6, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_80185AA4[47] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -88, 300, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -48, 300, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 300, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -160, 40, 300, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 96, 300, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -120, -120, 1000, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -112, -112, 1000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -104, -96, 1000, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -104, -72, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -104, 8, 1000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 24, 980, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, -104, 1575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -80, -88, 1575, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -96, -8, 1575, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 0, -104, 1612, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -80, 1612, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -16, -8, 1712, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -88, -8, 850, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -96, -8, 825, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -8, 700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -112, 0, 625, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, 16, 475, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -152, 24, 325, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -32, 1500, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -32, 1375, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, -24, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -48, -32, 1575, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 88, 80, 600, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 88, 64, 675, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 64, 675, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 56, 725, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 64, 725, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 48, 8, 875, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 16, 825, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 48, 24, 725, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 40, 825, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 56, 64, 875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, 72, 750, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 16, 950, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 32, 875, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 0, 1200, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 0, 1250, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -32, 1350, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -64, -24, 1350, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -8, 1350, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 8, 1350, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, -16, 1500, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_80185E50[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 2, 0 } },
    { 5, 6, 0, 0, { 7, 0 } },
    { 11, 3, 0, 0, { 6, 0 } },
    { 14, 3, 0, 0, { 8, 0 } },
    { 17, 6, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 9, 0 } },
    { 26, 1, 0, 0, { 5, 0 } },
    { 27, 5, 0, 0, { 10, 0 } },
    { 32, 6, 0, 0, { 1, 0 } },
    { 38, 2, 0, 0, { 11, 0 } },
    { 40, 2, 0, 0, { 4, 0 } },
    { 42, 4, 0, 0, { 12, 0 } },
    { 46, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185EC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185ED8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185EE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185EF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185F08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185F18[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185F28[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185F38[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_80185F48[20] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 800, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 800, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 80, 800, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 64, 800, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 800, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 40, 800, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 40, 800, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 800, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 800, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 800, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 800, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 800, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 800, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 800, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 800, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -80, 800, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 800, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 800, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 800, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_801860D8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_patio_801860F0[30] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -16, 1500, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -120, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -120, 750, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -120, 750, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -104, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -104, 750, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -104, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -88, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -88, 750, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -96, -88, 750, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -72, 1000, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -72, 1000, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -96, -72, 1000, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -56, 1000, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -56, 1000, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, -56, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -40, 1250, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -40, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, -40, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -24, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -24, 1500, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, -24, 1500, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -8, 1750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -8, 1750, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, -8, 1750, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -8, 1750, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 8, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 2000, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 2000, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -96, 8, 2000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_patio_80186348[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_patio_80186360[19] = {
    { { .empty = D_acropolis_patio_80184AF0 }, D_acropolis_patio_80184AF0, NULL },
    { { .elements = D_acropolis_patio_80184B00 }, D_acropolis_patio_80184D1C, NULL },
    { { .elements = D_acropolis_patio_80184D54 }, D_acropolis_patio_801850D8, NULL },
    { { .elements = D_acropolis_patio_80185150 }, D_acropolis_patio_8018527C, NULL },
    { { .empty = D_acropolis_patio_801852A4 }, D_acropolis_patio_801852A4, NULL },
    { { .elements = D_acropolis_patio_801852B4 }, D_acropolis_patio_801853B8, NULL },
    { { .elements = D_acropolis_patio_801853D8 }, D_acropolis_patio_8018557C, NULL },
    { { .elements = D_acropolis_patio_801855A4 }, D_acropolis_patio_80185A04, NULL },
    { { .elements = D_acropolis_patio_80185AA4 }, D_acropolis_patio_80185E50, NULL },
    { { .empty = D_acropolis_patio_80185EC8 }, D_acropolis_patio_80185EC8, NULL },
    { { .empty = D_acropolis_patio_80185ED8 }, D_acropolis_patio_80185ED8, NULL },
    { { .empty = D_acropolis_patio_80185EE8 }, D_acropolis_patio_80185EE8, NULL },
    { { .elements = D_acropolis_patio_801853D8 }, D_acropolis_patio_8018557C, NULL },
    { { .empty = D_acropolis_patio_80185F08 }, D_acropolis_patio_80185F08, NULL },
    { { .empty = D_acropolis_patio_80185F18 }, D_acropolis_patio_80185F18, NULL },
    { { .empty = D_acropolis_patio_80185F28 }, D_acropolis_patio_80185F28, NULL },
    { { .elements = D_acropolis_patio_80185150 }, D_acropolis_patio_8018527C, NULL },
    { { .elements = D_acropolis_patio_80185F48 }, D_acropolis_patio_801860D8, NULL },
    { { .elements = D_acropolis_patio_801860F0 }, D_acropolis_patio_80186348, NULL },
};

/// Point lights shared by the three patio room entries in every view.
///
/// Positions and falloff radii use integer room coordinates; RGB uses 12
/// fractional bits. The loaded overlay owns these writable records: coordinate
/// updates fill their parent and transform caches, and queries overwrite attenuation.
static WorldCoordPointLight _gAcropolisPatioPointLights[] = {
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -700, -200, 2400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -700, -200, 2400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2211, 2129 },
        },
        .inner = 150,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -3147, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -3147, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 200,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5250, -3150, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5250, -3150, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 200,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8050, -200, -50 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8050, -200, -50 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -200, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -200, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 5250,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -200, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -200, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2457, 2375 },
        },
        .inner = 150,
        .outer = 4200,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3900, -200, 800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3900, -200, 800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 4200,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5050, -200, -2950 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3194, 3194, 3112 },
        },
        .inner = 250,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5050, -200, -2950 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3194, 3194, 3112 },
        },
        .inner = 250,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1050, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1050, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2867, 2785, 2785 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1600, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1600, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2867, 2785, 2785 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4300, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4300, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2949, 2867, 2867 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7200, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7200, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2949, 2867, 2867 },
        },
        .inner = 500,
        .outer = 5000,
    },
};

WorldCoordRoomLights D_acropolis_patio_80186D44[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisPatioPointLights), _gAcropolisPatioPointLights, 0, NULL },
};

ViewCamera D_acropolis_patio_80186D5C[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1800, 0x61A8, 0 } }, 418 },
    { { { { 592, 0, -4052 }, { -336, 4081, -49 }, { 4038, 340, 590 } }, { 1580, 1410, 820 } }, 257 },
    { { { { 808, 0, -4015 }, { -361, 4079, -72 }, { 3999, 368, 805 } }, { 6730, 1610, 770 } }, 230 },
    { { { { 752, 0, 4026 }, { 459, 4069, -85 }, { -3999, 467, 747 } }, { 120, 1700, 660 } }, 257 },
    { { { { 613, 0, 4049 }, { 902, 3992, -136 }, { -3947, 912, 597 } }, { 2789, 1999, 659 } }, 257 },
    { { { { -3922, 0, -1180 }, { -135, 4069, 449 }, { 1173, 469, -3896 } }, { -3690, 1390, -1590 } }, 257 },
    { { { { -4025, 0, 756 }, { 139, 4025, 744 }, { -743, 757, -3956 } }, { 6520, 1860, -1120 } }, 246 },
    { { { { -511, 0, -4063 }, { -552, 4057, 69 }, { 4026, 557, -507 } }, { 8420, 1740, 3180 } }, 240 },
    { { { { -708, 0, -4034 }, { -947, 3981, 166 }, { 3921, 961, -688 } }, { 4300, 1970, 2910 } }, 240 },
    { { { { -106, 0, -4094 }, { -2554, 3200, 66 }, { 3199, 2555, -83 } }, { 250, 2210, 2790 } }, 289 },
    { { { { -4064, 0, -505 }, { -41, 4082, 331 }, { 503, 333, -4051 } }, { 5289, 2010, 2850 } }, 257 },
    { { { { -106, 0, -4094 }, { -2554, 3200, 66 }, { 3199, 2555, -83 } }, { 250, 2209, 2789 } }, 289 },
    { { { { -4025, 0, 756 }, { 139, 4025, 744 }, { -743, 757, -3956 } }, { 6520, 1860, -1120 } }, 246 },
    { { { { 1658, 0, -3745 }, { -2445, 3102, -1083 }, { 2836, 2674, 1256 } }, { 8700, 1400, 10 } }, 348 },
    { { { { 2052, 0, -3544 }, { 824, 3983, 477 }, { 3447, -952, 1995 } }, { 8980, 355, 460 } }, 235 },
    { { { { -3617, 0, -1920 }, { 592, 3896, -1116 }, { 1826, -1263, -3441 } }, { 8540, 95, -2040 } }, 289 },
    { { { { 752, 0, 4026 }, { 459, 4069, -85 }, { -3999, 467, 747 } }, { 120, 1700, 660 } }, 257 },
    { { { { 491, 0, 4066 }, { 4013, 659, -485 }, { -654, 4042, 79 } }, { 5020, 6500, 1060 } }, 304 },
    { { { { 362, 0, 4079 }, { -1549, 3789, 137 }, { -3774, -1555, 335 } }, { 220, 450, 460 } }, 216 },
};

WorldCollisionFootstepSounds D_acropolis_patio_80187008 = {
    0x10000001,
    0x10000003,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_patio_80187014[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_patio_8018701C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_patio_80187024[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_patio_8018702C[1] = { 0 };

WorldCollisionSurfaceProperties D_acropolis_patio_80187034[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_patio_80187008 },
};

WorldCollisionSurfaceProperties* D_acropolis_patio_8018703C[9] = {
    D_acropolis_patio_80187014,
    D_acropolis_patio_8018701C,
    D_acropolis_patio_80187024,
    D_acropolis_patio_8018702C,
    D_acropolis_patio_80187034,
    D_acropolis_patio_80187014,
    D_acropolis_patio_80187014,
    D_acropolis_patio_80187014,
    NULL,
};

Task* D_acropolis_patio_80187060;

u8 D_acropolis_patio_80187064;

u8 D_acropolis_patio_80187065;

/// Room entry task tick. Publishes the room's own record at
/// `Task::msgTable` / pointer slot 7, then re-issues the messages the room's
/// actors need for the current point in the story: the first visit
/// (`gameFlagGetNibble(0) < 2`) arms the two hotspots and spawns the arrival
/// cutscene, and the second-visit branches replace them according to
/// `gGameSession::location.loc.variant`.
static void func_acropolis_patio_8017D5EC(Task* arg0)
{
    ActorCommand msg;
    Task*        temp;

    arg0->msgTable = D_acropolis_patio_8018028C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(0) < 2) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room == 1) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), 0x7D4, &D_acropolis_patio_80180428, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_acropolis_patio_8018044C, 0);
            taskMessageDispatch(sceneFindPlacedActor(0), ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            D_acropolis_patio_80187060 = taskSpawnFromTable(D_acropolis_patio_801802BC, 2, 0, 0);
        }
        temp = sceneFindPlacedActor(1);
        if (temp != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(temp, 0x7D4, &D_acropolis_patio_8018046C, 0);
        }
    }
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < 2)) {
        temp = sceneFindPlacedActor(1);
        if (temp != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(temp, ACTOR_COMMAND_MESSAGE_APPLY, &D_acropolis_patio_80180440, 0);
        }
    }
    if ((gGameSession->location.loc.variant == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PATIO_026) == 0)) {
        msg.context.loc.stage = 1;
        msg.context.loc.area  = 3;
        msg.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(2), ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(3), ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
    }
    arg0->state = arg0->state + 1;
}

s32 func_acropolis_patio_8017D7D0(Task* arg0, s32 arg1, RoomEventMsg* arg2, RoomEventMsg* arg3)
{
    s32 var_v0;
    u16 temp_s1;

    *arg3 = *arg2;
    if (arg2->areaId == 8) {
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2) && (arg2->queryOnly == 0)) {
            arg3->room = 2;
        }
    }
    if (arg2->areaId == 4) {
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) < 2) {
            var_v0 = 0;
            if (arg2->queryOnly == 0) {
                capRunCommandWithTransition(3);
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, 1);
                gameFlagSetNibbleIfPresent(arg2->flagId, 2);
                return 0;
            }
            return var_v0;
        }
        if (gameFlagGetNibble(0) == 2) {
            var_v0 = 2;
            if (arg2->queryOnly == 0) {
                if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN) == 0) {
                    evsStartScriptWithSkip(D_acropolis_patio_80180DEC, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80180EDC);
                    gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN, 1);
                    return 2;
                }
                capRunCommandWithTransition(8);
                return 2;
            }
            return var_v0;
        }
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) == 2) {
            var_v0 = 2;
            if (arg2->queryOnly == 0) {
                taskSpawnFromTable(D_acropolis_patio_801802BC, 1, 0, 0);
                itemSetIdentified(0x101, 1);
                D_acropolis_patio_80187064 = arg2->warp;
                D_acropolis_patio_80187065 = arg2->room;
                return 2;
            }
            return var_v0;
        }
    }
    if ((arg2->areaId == 8) && (gameFlagGetNibble(0) < 5)) {
        var_v0 = 0;
        if (arg2->queryOnly == 0) {
            gameFlagSetNibbleIfPresent(arg2->flagId, 2);
            capRunCommandWithTransition(4);
            return 0;
        }
        return var_v0;
    }
    if ((arg2->queryOnly == 0) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == 3)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, 4);
    }
    temp_s1 = arg2->areaId;
    var_v0  = 1;
    if (temp_s1 == 4) {
        var_v0 = 1;
        if (arg2->queryOnly == 0) {
            if (gameFlagGetNibble(0) >= 3) {
                arg3->room = (s8)temp_s1;
            }
            var_v0 = 1;
            if (gameFlagGetNibble(0) == 2) {
                arg3->room = 3;
                var_v0     = 1;
            }
        }
    }
    return var_v0;
}

void func_acropolis_patio_8017DA5C(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(3);
            task->state = task->state + 1;
            return;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, 3);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 4), 0, 0);
                task->state = task->state + 1;
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
        case 3:
            if (sndScriptHasActiveId(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 4)) != 0) {
                return;
            }
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_ACROPOLIS_CAFETERIA;
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_acropolis_patio_80187064;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_acropolis_patio_80187065;
            taskSpawn(0, 0x11, 0, 0);
            taskKill(task);
            return;
    }
}

s32 func_acropolis_patio_8017DBAC(Task* arg0, s32 arg1, const void* arg2, s32 arg3)
{
    const DirectionActionRequest* request = arg2;
    u8                            state;

    if ((request->actionId == 0) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < 2)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, 3);
        evsStartScriptWithSkip(D_acropolis_patio_80180484, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_801806AC);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 3;
        gGameSession->flowFlags                             = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    if ((request->actionId == 1) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == 3) &&
        (taskMessageDispatch(sceneFindPlacedActor(1), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, 4);
        evsStartScriptWithSkip(D_acropolis_patio_8018082C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80180C64);
    }
    state = request->actionId;
    if ((state == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PATIO_026) == 0) && (gameFlagGetNibble(0) == state)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_PATIO_026, 1);
        evsStartScriptWithSkip(D_acropolis_patio_8018280C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80182BE4);
    }
    // No result is set; the sender of a room action ignores it.
}
s32 func_acropolis_patio_8017DCE4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 var_v0;

    if (arg2 == 1) {
        capSpawnEventIfIdle(1, CAP_EVENT_NO_FLAGS);
    }
    var_v0 = 2;
    if (arg2 == 2) {
        var_v0 = gameFlagGetNibble(0) < 2;
        if (var_v0 != 0) {
            capSpawnEventIfIdle(2, CAP_EVENT_NO_FLAGS);
            var_v0 = 0;
        }
    }
    return var_v0;
}

/// Refuses every key-item-use request (message 0x13F1), returning zero to the inventory.
///
/// The receiver, selected item ID and second payload word are unused.
static s32 _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_PATIO_KEY_ITEM_REFUSED;
}

/// Handles the room sound message: cue 3 queues area-bank entry 3; all cues return zero.
///
/// Queue admission failures are ignored. The receiver and second payload word are unused.
static s32 _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_SOUND_CUE = 3 };

    if (soundCue == ACROPOLIS_PATIO_SOUND_CUE) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, ACROPOLIS_PATIO_SOUND_CUE), 0, 0);
    }
    return 0;
}
void func_acropolis_patio_8017DD80(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            capRunCommandWithTransition(6);
            task->state = task->state + 1;
            return;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (capGetVariantKey() == 1) {
                gameFlagSetNibble(GAME_FLAG_015, 1);
            }
            taskKill(task);
            return;
    }
}

/// Aims the player's head at a scripted world point, optionally sliding it along Z.
///
/// States 0/1 initialize and idle, 2 holds the point (-8000, 0, 900), 3 starts
/// the slide, and 4 moves it 50 units per tick to Z -3196. `spawnArg1.value`
/// stores the slide distance (0..4096). Requires the live player's five-part
/// head chain; the task neither owns nor releases the player.
static void _acropolisPatioPlayerHeadAimTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_HEAD_AIM_INIT           = 0,
        ACROPOLIS_PATIO_HEAD_AIM_IDLE           = 1,
        ACROPOLIS_PATIO_HEAD_AIM_HOLD           = 2,
        ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE    = 3,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE          = 4,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW        = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH      = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP     = 50,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE = 4096
    };

    GfxCoord focus;        // World point carried in the translation; the other fields are unused
    byte     unused[0x28]; // Frame space no instruction touches; what the original declared here is unproven
    Task*    playerTask;
    s32      slideDistance;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    // The head turn reads nothing of its target frame but the translation, a
    // world point, so the rest of `focus` stays uninitialised.
    focus.coord.t[0] = -0x1F40;
    focus.coord.t[1] = 0;
    focus.coord.t[2] = 0x384;

    switch (task->state) {
        case ACROPOLIS_PATIO_HEAD_AIM_INIT:
            task->spawnArg1.value = 0;
            task->state           = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_IDLE:
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_HOLD:
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE:
            task->spawnArg1.value = 0;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_SLIDE:
            slideDistance         = task->spawnArg1.value + ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP;
            task->spawnArg1.value = slideDistance;
            if (slideDistance >= ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE + 1) {
                task->spawnArg1.value = ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE;
            }
            focus.coord.t[2] -= task->spawnArg1.value;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
    }
}

/// Selects the opening scene's player-head-aim phase without resetting its progress.
///
/// The retained aim task must already be spawned and live. Script choices are
/// 1 idle, 2 hold the fixed world point, and 3 begin its Z slide. The task itself
/// resets the slide distance when phase 3 next runs; this setter only stores
/// the supplied signed-word state.
static void _acropolisPatioSetPlayerHeadAimState(s32 headAimState)
{
    D_acropolis_patio_80187060->state = headAimState;
}

/// Selects room 2 in the live and saved location and requests room-object relinking.
static void _acropolisPatioActivateRoom2(void)
{
    enum { ACROPOLIS_PATIO_ROOM_AFTER_SCENE = 2 };

    gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACROPOLIS_PATIO_ROOM_AFTER_SCENE;
    gGameSession->roomObjsDirty                                                                  = true;
}

/// Sets the scene's actor update/visibility control from an event-script byte.
///
/// Accepts `SCENE_COMBAT_ACTORS_*`; the patio scripts use paused and running.
static void _acropolisPatioSetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// Keeps the room task alive without per-frame work; `task` is unused.
static void _acropolisPatioRoomIdleState(Task* task)
{
    byte unused[0x10]; // Untouched local storage retained for the original stack frame; its role is unproven
}

void func_acropolis_patio_8017DF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_patio_8017D5C4;
    sp.funcs[task->state](task);
}

void func_acropolis_patio_8017DFE4(s32 arg0)
{
    if (arg0 != 0) {
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        sceneEngageBattle(1);
        return;
    }
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
}
void func_acropolis_patio_8017E024(void)
{
    taskSpawnFromTable(&D_acropolis_patio_80182800, 0, 0, 0);
}

/// Turns the live player left by half a turn over 16 task ticks, then kills itself.
///
/// Yaw uses 4096 units per turn. States 0/1 start and turn; any other state
/// kills this bodyless controller. `killCountdown` holds the remaining ticks.
/// Start in state 0 with player yaw in [-2048, 2048).
static void _acropolisPatioPlayerTurnLeftTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_PLAYER_TURN_INIT     = 0,
        ACROPOLIS_PATIO_PLAYER_TURN_STEP     = 1,
        ACROPOLIS_PATIO_PLAYER_TURN_TICKS    = 16,
        ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP = ACTOR_TRANSFORM_ANGLE_HALF_TURN / ACROPOLIS_PATIO_PLAYER_TURN_TICKS
    };

    GameActor* playerActor;
    s16        yaw;

    playerActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (task->state) {
        case ACROPOLIS_PATIO_PLAYER_TURN_INIT:
            task->killCountdown = ACROPOLIS_PATIO_PLAYER_TURN_TICKS;
            task->state++;
            /* fallthrough */
        case ACROPOLIS_PATIO_PLAYER_TURN_STEP:
            yaw = playerActor->rotation.vy - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP;
            if (yaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yaw = playerActor->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP);
            }
            playerActor->rotation.vy = yaw;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            taskKill(task);
            break;
    }
}

/// Lights the patio fountain: a one-shot burst that seeds every jet and its
/// mist, then leaves the task idle for the rest of the room.
///
/// The 14 anchors of `D_acropolis_patio_80182DDC` are handed to `effectSpawn`
/// as three runs of effect 0x60087, each run differing only in the high bits of
/// the spawn argument - `0x03000200` for the three main jets, `0x02000000` for
/// the next four and a plain `0x100` for the remaining seven - so the anchor
/// index rides in the low nibble; bits 8..9 select the sprite cell and bits
/// 16..27 select its size. All three runs use the same additive texture page.
///
/// The three main jets then get three puffs of mist each (effect 0x6008F).
/// Every puff re-uses the task's own `EffectWork.move` triple as a scratch
/// offset: three 11-bit LCG draws centred on 0x400 give a `+/-0x400` jitter,
/// which is added to the jet's anchor before the spawn reads it. The work block
/// is scratch, not state - each spawn copies the vector out immediately - so
/// all nine puffs share it.
void func_acropolis_patio_8017E100(Task* task)
{
    GfxCoord*   objCoord;
    EffectWork* work;
    s32         i;
    s32         j;

    work     = (EffectWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (task->state == 0) {
        for (i = 0; i < 3; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, objCoord, i + 0x03000200, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 3; i < 7; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, objCoord, i + 0x02000000, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 7; i < 0xE; i++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, objCoord, i + 0x100, &D_acropolis_patio_80182DDC[i]);
        }
        task->state++;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x400 - ((gRandomLcgState >> 16) & 0x7FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x400 - ((gRandomLcgState >> 16) & 0x7FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0x400 - ((gRandomLcgState >> 16) & 0x7FF);
                work->move.vx  += D_acropolis_patio_80182DDC[i].vx;
                work->move.vy  += D_acropolis_patio_80182DDC[i].vy;
                work->move.vz  += D_acropolis_patio_80182DDC[i].vz;
                effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_MIST, objCoord, i, &work->move);
            }
        }
    }
}

/// Positions a patio light glow as a square around its projected screen centre.
///
/// Borrows a writable packet and a separate, read-only `projection` for this
/// call. Only `screenPos` and `halfExtent` need initialization in the projection;
/// both count whole pixels, with a nonnegative half-extent. Centre plus/minus
/// extent must fit signed 32-bit arithmetic. Each edge narrows to signed 16 bits
/// without clipping. Vertices 0..3 are top-left, top-right, bottom-left and
/// bottom-right. Writes only the eight coordinate halfwords.
static inline void _acropolisPatioSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left         = projection->screenPos.vx - projection->halfExtent;
    glowQuad->x2 = left;
    glowQuad->x0 = left;
    right        = projection->screenPos.vx + projection->halfExtent;
    glowQuad->x3 = right;
    glowQuad->x1 = right;
    top          = projection->screenPos.vy - projection->halfExtent;
    glowQuad->y1 = top;
    glowQuad->y0 = top;
    bottom       = projection->screenPos.vy + projection->halfExtent;
    glowQuad->y3 = bottom;
    glowQuad->y2 = bottom;
}

/// Projects a patio light's cached view position into a flickering additive glow.
///
/// `composedCoord->workm.t` supplies signed game coordinates, narrowed to s16
/// before projection through `GsWSMATRIX`. The caller supplies GTE projection
/// settings and a complete, word-aligned `spriteScratch` block. FLAG is not
/// tested. `glowWork->angle` selects cell/CLUT 0..2, `scale` is the size scale
/// (1..4095), and `period` is resting grey (48, 64 or 80); odd animation frames
/// add 16. Screen half-extent is scale * 39 / (SZ3 / 4), in integer pixels.
///
/// Reserves one `POLY_FT4` even when depth is below `ACROPOLIS_PATIO_JET_MIN_DEPTH`.
/// Accepted depths wrap into the current 1024-tag ordering table. The arena and
/// sprite texture/CLUT must remain live until GPU completion; inputs and scratch
/// are borrowed only for this call. Changes GTE state and uses the texture page's
/// additive blend mode without allocating a separate draw-mode command.
static inline void _acropolisPatioDrawLightGlow(const GfxCoord* composedCoord, const EffectWork* glowWork, RoomGlowSpriteScratch* spriteScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE             = 16,
        ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK       = 1,
        ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT = 2
    };

    POLY_FT4* glowQuad;
    u8        greyLevel;

    // Project the narrowed cached view translation into the sprite's centre.
    spriteScratch->worldPos.vx = composedCoord->workm.t[0];
    spriteScratch->worldPos.vy = composedCoord->workm.t[1];
    spriteScratch->worldPos.vz = composedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&spriteScratch->worldPos);
    gte_rtps();
    // Consume a packet even when the depth test will reject the projection.
    glowQuad       = gGpuPrimCursor;
    gGpuPrimCursor = glowQuad + 1;
    setPolyFT4(glowQuad);
    gte_stsxy(&spriteScratch->screenPos);
    gte_stszotz(&spriteScratch->otz);
    if (spriteScratch->otz >= ACROPOLIS_PATIO_JET_MIN_DEPTH) {
        greyLevel       = glowWork->period + (((u8)gDisplayState.animFrame & ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK) << ACROPOLIS_PATIO_JET_FLICKER_SHIFT);
        glowQuad->tpage = ACROPOLIS_PATIO_JET_TEXTURE_PAGE;
        setRGB0(glowQuad, greyLevel, greyLevel, greyLevel);
        setSemiTrans(glowQuad, true);
        setClut(glowQuad, glowWork->angle * ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE, ACROPOLIS_PATIO_JET_CLUT_Y);
        // Texture endpoints include the final texel of the fixed 40-by-40 cell.
        setUVWH(glowQuad, glowWork->angle * ACROPOLIS_PATIO_JET_CELL_SIZE, 0,
                ACROPOLIS_PATIO_JET_CELL_SIZE - 1, ACROPOLIS_PATIO_JET_CELL_SIZE - 1);

        spriteScratch->halfExtent = (glowWork->scale * (ACROPOLIS_PATIO_JET_CELL_SIZE - 1)) / spriteScratch->otz;
        _acropolisPatioSetLightGlowBounds(glowQuad, spriteScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)spriteScratch->otz << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                glowQuad);
    }
}

void acropolisPatioFountainJetTask(Task* task)
{
    enum { ACROPOLIS_PATIO_JET_INIT = 0 };

    RoomGlowSpriteScratch* block;
    EffectWork*            work;
    GfxCoord*              coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((D_acropolis_patio_80182E4C[task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK] >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == ACROPOLIS_PATIO_JET_INIT) {
            // Decode the fixed sheet cell and size only on the first visible tick.
            u8 levels[3] = { 0x50, 0x30, 0x40 };

            if (task->spawnArg1.value & (ACROPOLIS_PATIO_JET_SIZE_MASK << ACROPOLIS_PATIO_JET_SIZE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_SIZE_SHIFT) & ACROPOLIS_PATIO_JET_SIZE_MASK;
            } else {
                work->scale = ACROPOLIS_PATIO_JET_DEFAULT_SIZE;
            }
            work->angle           = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_CELL_SHIFT) & ACROPOLIS_PATIO_JET_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK;
            work->period          = levels[work->angle];
            task->state           = task->state + 1;
        }
        _acropolisPatioDrawLightGlow(coord, work, block);
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// A non-zero padding byte the original toolchain left. Nothing refers to it.
///
/// It fills out the word holding the three grey levels the function above
/// initialises its local array from, so it has to stay directly below that
/// function.
static const u8 D_acropolis_patio_8017D5EB = 0xF2;

/// Projects a cached light-mote position into a random-grey, average-blended pixel.
///
/// Reads only `cachedCoord->workm.t`, in signed game units narrowed to s16.
/// This cache is the snapshot composed before the caller moved the local node;
/// a dirty composition stamp does not invalidate that snapshot for this draw.
/// Requires configured GTE projection settings, `GsWSMATRIX`, and a complete,
/// word-aligned `tileScratch` block. FLAG is not tested; depth is SZ3 / 4.
///
/// The word-aligned arena needs one `TILE_1` even on depth rejection and one
/// additional `DR_TPAGE` at `EFFECT_POINT_TILE_MIN_DEPTH` or further. Only an
/// accepted depth advances the shared LCG, selecting grey 0..191. Queues the
/// blend command before the pixel in the current 1024-tag ordering table and
/// leaves average draw mode active. Packet storage lives until GPU completion;
/// neither input nor scratch is retained. Changes GTE state.
static inline void _acropolisPatioDrawLightMote(const GfxCoord* cachedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS             = 192,
        ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT = 2
    };

    TILE_1* moteTile;
    u32     greyLevel;

    // Draw the pre-movement view cache; the dirty stamp defers recomposition.
    tileScratch->viewPoint.vx = cachedCoord->workm.t[0];
    tileScratch->viewPoint.vy = cachedCoord->workm.t[1];
    tileScratch->viewPoint.vz = cachedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile before rejecting depth, without consuming random state.
    moteTile       = gGpuPrimCursor;
    gGpuPrimCursor = moteTile + 1;
    setTile1(moteTile);
    gte_stsxy(&moteTile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        greyLevel       = gRandomLcgState >> 16;
        greyLevel      %= ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS;
        setRGB0(moteTile, greyLevel, greyLevel, greyLevel);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                moteTile);
        // Prepending at the same depth makes the GPU set blending before the tile.
        gpuSetPrimitiveBlendMode(moteTile, GPU_BLEND_AVERAGE, tileScratch->depth);
    }
}

void acropolisPatioFountainMistTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_MIST_INIT            = 0,
        ACROPOLIS_PATIO_MIST_DRIFT           = 0,
        ACROPOLIS_PATIO_MIST_GATHER          = 1,
        ACROPOLIS_PATIO_MIST_DRIFT_CENTRE    = 16,
        ACROPOLIS_PATIO_MIST_DRIFT_MASK      = 31,
        ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK = 3,
        ACROPOLIS_PATIO_MIST_GATHER_WEIGHT   = ONE / 128,
        ACROPOLIS_PATIO_MIST_JITTER_MASK     = 15,
        ACROPOLIS_PATIO_MIST_JITTER_CENTRE   = 8,
        ACROPOLIS_PATIO_MIST_DRIFT_CHANCE    = 120,
        ACROPOLIS_PATIO_MIST_GATHER_CHANCE   = 60
    };

    EffectWork*             work;
    GfxCoord*               coord;
    EffectPointTileScratch* tileScratch;
    SVECTOR*                velocity;
    SVECTOR*                anchors;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((D_acropolis_patio_80182E4C[task->spawnArg1.value] >> (gGameSession->location.loc.view - 1)) & 1)) {
        tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
        actorRenderComposeCoord(coord);
        if (task->state == ACROPOLIS_PATIO_MIST_INIT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            task->state++;
        }
        // Gather re-aims on a random quarter of ticks, rather than periodically.
        if (work->index != ACROPOLIS_PATIO_MIST_DRIFT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK) == 0) {
                anchors       = D_acropolis_patio_80182DDC;
                velocity      = &work->move;
                work->move.vx = (u16)anchors[task->spawnArg1.value].vx -
                                (u16)coord->coord.t[0];
                work->move.vy = (u16)anchors[task->spawnArg1.value].vy -
                                (u16)coord->coord.t[1];
                work->move.vz = (u16)anchors[task->spawnArg1.value].vz -
                                (u16)coord->coord.t[2];
                VectorNormalSS(velocity, velocity);
                gte_lddp(ACROPOLIS_PATIO_MIST_GATHER_WEIGHT);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_DRIFT_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_DRIFT;
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_GATHER_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_GATHER;
            }
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        _acropolisPatioDrawLightMote(coord, tileScratch);
        SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    }
}
