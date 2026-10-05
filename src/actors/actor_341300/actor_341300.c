#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#define ACTOR_341300_RAND() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

/// One step of gameplay's LCG, `state = state * 5 + 0x71357911`, as its high half.

/// Work block of a debris shard task: one flat triangle that tumbles and falls
/// under gravity until its origin drops below y 0.
///
/// The task allocates the block zeroed and fills it once, on its first frame,
/// from its own random draws; afterwards only `rot` and `vel.vy` change. The
/// shard's position is not kept here but in the task's coordinate, which is
/// parented to the view.
typedef struct {
    SVECTOR rot;      // Tumble angles (4096 = one turn), rebuilt into the coordinate's rotation each frame as Y, then X, then Z; `pad` is never set
    SVECTOR spin;     // Per-frame step of `rot`, 100 to 227 either way per axis; `pad` is never set
    SVECTOR vel;      // Per-frame translation of the coordinate, up to 31 either way per axis at spawn, with 8 added to `vy` each frame; `pad` is never set
    SVECTOR verts[3]; // The triangle's corners in the shard's own XY plane: equilateral about the origin at radius 20, each nonzero coordinate randomly pushed 2 further out
} _Actor341300ShardWork;
STATIC_ASSERT_SIZEOF(_Actor341300ShardWork, 0x30);

/// Spawn positions `func_actor_341300_80162878`'s shards start from, indexed
/// by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A38[];

/// Spawn positions `func_actor_341300_801631D4`'s shards start from, indexed
/// by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A58[];

/// Placement record the overlay's data table points at, read here only as the
/// target position's x/z pair.
extern ActorTransform D_actor_341300_80165330;

/// Task handles of the debris emitters the event scripts start and stop by
/// index.
///
/// Only slot 0 is ever written or read: the start and stop callbacks act on
/// index 0 alone and ignore the others. The scripts pass indices 0 to 2 and the
/// storage is three pointers long, which is why it is declared as one slot per
/// index; that the two words after slot 0 are the slots of indices 1 and 2 is
/// inferred from that, not from an access.
extern Task* D_actor_341300_80165A2C[3];

extern TaskDesc D_actor_341300_80165A68[];

extern TaskDesc D_actor_341300_80165208[];

extern Task* D_actor_341300_80165AA4;

static void func_actor_341300_8016398C(s32 arg0);
static void func_actor_341300_801639CC(s32 arg0);

void func_actor_341300_80162698(Task*);

void func_actor_341300_80163A10(Task*);

extern AnimationPlayRequest     D_actor_341300_80165260;
extern AnimationPlayRequest     D_actor_341300_80165274;
extern AnimationPlayRequest     D_actor_341300_80165288;
extern AnimationPlayRequest     D_actor_341300_8016529C;
extern AnimationPlayRequest     D_actor_341300_801652B0;
extern AnimationPlayRequest     D_actor_341300_801652F4;
extern AnimationPlayRequest     D_actor_341300_80165308;
extern AnimationPlayRequest     D_actor_341300_8016531C;
extern AnimationBankCopyRequest D_actor_341300_80165244;
extern ActorTransform           D_actor_341300_801652C4;
extern ActorTransform           D_actor_341300_801652DC;
void                            func_actor_341300_8016239C(void);
void                            func_actor_341300_801623BC(void);
void                            func_actor_341300_801623DC(void);
void                            func_actor_341300_801623FC(void);
void                            func_actor_341300_8016241C(void);
void                            func_actor_341300_80162450(void);
void                            func_actor_341300_80162530(void);
void                            func_actor_341300_80162564(s16);
void                            func_actor_341300_80162588(s16);
void                            func_actor_341300_801625AC(void);
void                            func_actor_341300_80162680(s8);

void func_actor_341300_80162278(Task*);
void func_actor_341300_80162478(Task*);

static AnimationPackedPose _gActor341300Animation01FACBank1[6] = {
#include "assets/actor_341300_animation_01FAC_bank1.inc"
};

static AnimationPackedRotation _gActor341300Animation01FACBank4[46] = {
#include "assets/actor_341300_animation_01FAC_bank4.inc"
};

static AnimationRecord _gActor341300Animation01FACRecords[109] = {
#include "assets/actor_341300_animation_01FAC_records.inc"
};

static u16 _gActor341300Animation01FACIndices[20] = {
#include "assets/actor_341300_animation_01FAC_indices.inc"
};

static AnimationSet _gActor341300Animation01FAC = {
    _gActor341300Animation01FACRecords,
    _gActor341300Animation01FACIndices,
    { NULL, _gActor341300Animation01FACBank1, NULL, NULL, _gActor341300Animation01FACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341300Animation02810Bank1[20] = {
#include "assets/actor_341300_animation_02810_bank1.inc"
};

static AnimationPackedRotation _gActor341300Animation02810Bank4[200] = {
#include "assets/actor_341300_animation_02810_bank4.inc"
};

static AnimationRecord _gActor341300Animation02810Records[257] = {
#include "assets/actor_341300_animation_02810_records.inc"
};

static u16 _gActor341300Animation02810Indices[20] = {
#include "assets/actor_341300_animation_02810_indices.inc"
};

static AnimationSet _gActor341300Animation02810 = {
    _gActor341300Animation02810Records,
    _gActor341300Animation02810Indices,
    { NULL, _gActor341300Animation02810Bank1, NULL, NULL, _gActor341300Animation02810Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341300Animation02CE0Bank1[14] = {
#include "assets/actor_341300_animation_02CE0_bank1.inc"
};

static AnimationPackedRotation _gActor341300Animation02CE0Bank4[95] = {
#include "assets/actor_341300_animation_02CE0_bank4.inc"
};

static AnimationRecord _gActor341300Animation02CE0Records[151] = {
#include "assets/actor_341300_animation_02CE0_records.inc"
};

static u16 _gActor341300Animation02CE0Indices[20] = {
#include "assets/actor_341300_animation_02CE0_indices.inc"
};

static AnimationSet _gActor341300Animation02CE0 = {
    _gActor341300Animation02CE0Records,
    _gActor341300Animation02CE0Indices,
    { NULL, _gActor341300Animation02CE0Bank1, NULL, NULL, _gActor341300Animation02CE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341300Animation02F50Bank1[4] = {
#include "assets/actor_341300_animation_02F50_bank1.inc"
};

static AnimationPackedRotation _gActor341300Animation02F50Bank4[27] = {
#include "assets/actor_341300_animation_02F50_bank4.inc"
};

static AnimationRecord _gActor341300Animation02F50Records[97] = {
#include "assets/actor_341300_animation_02F50_records.inc"
};

static u16 _gActor341300Animation02F50Indices[20] = {
#include "assets/actor_341300_animation_02F50_indices.inc"
};

static AnimationSet _gActor341300Animation02F50 = {
    _gActor341300Animation02F50Records,
    _gActor341300Animation02F50Indices,
    { NULL, _gActor341300Animation02F50Bank1, NULL, NULL, _gActor341300Animation02F50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341300Animation033C0Bank1[10] = {
#include "assets/actor_341300_animation_033C0_bank1.inc"
};

static AnimationPackedRotation _gActor341300Animation033C0Bank4[95] = {
#include "assets/actor_341300_animation_033C0_bank4.inc"
};

static AnimationRecord _gActor341300Animation033C0Records[139] = {
#include "assets/actor_341300_animation_033C0_records.inc"
};

static u16 _gActor341300Animation033C0Indices[20] = {
#include "assets/actor_341300_animation_033C0_indices.inc"
};

static AnimationSet _gActor341300Animation033C0 = {
    _gActor341300Animation033C0Records,
    _gActor341300Animation033C0Indices,
    { NULL, _gActor341300Animation033C0Bank1, NULL, NULL, _gActor341300Animation033C0Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_341300_80165208[2] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_341300_80162478, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80162278, { .value = 0 } },
};

TaskDesc D_actor_341300_80165220 = { { { TASK_BODY_NONE, 192 } }, enemyTaskExit, { .value = 0 } };

AnimationSet* D_actor_341300_8016522C[6] = {
    NULL,
    &_gActor341300Animation01FAC,
    &_gActor341300Animation02810,
    &_gActor341300Animation02CE0,
    &_gActor341300Animation02F50,
    &_gActor341300Animation033C0,
};

AnimationBankCopyRequest D_actor_341300_80165244 = { { .sets = D_actor_341300_8016522C }, ARRAY_SIZE(D_actor_341300_8016522C) };

AnimationPlayRequest D_actor_341300_8016524C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_341300_80165260 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_341300_80165274 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_341300_80165288 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_341300_8016529C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_341300_801652B0 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_341300_801652C4 = { { 2000, 0, 2250, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_341300_801652DC = { { 2000, 0, 4000, 0 }, { 0, 2047, 0, 0 } };

AnimationPlayRequest D_actor_341300_801652F4 = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_341300_80165308 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_341300_8016531C = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_341300_80165330 = { { 2090, -3480, 1440, 0 }, { 0, 0, 2047, 0 } };

ActorCommand D_actor_341300_80165348 = { { .loc = { 4, 30 } }, 1 };

EvsSceneKey D_actor_341300_8016534C = { 4, 13, 11 };

EvsCommand D_actor_341300_80165354[52] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_341300_80165244 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_341300_80165330 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_341300_801652F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_801652B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_80162530 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162564 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541E0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_341300_8016534C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_8016239C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_341300_801652C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165274 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_341300_80165308 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_801623BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_801625AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_341300_8016531C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162564 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162588 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165288 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162564 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162588 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_8016241C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_8016529C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_80162450 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_341300_801652DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_341300_80165348 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_801623DC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_341300_80162680 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_341300_80165834[21] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_341300_801652DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_341300_80165244 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_341300_80165348 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162588 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162588 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341300_80162588 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_80162450 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341300_801623FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_341300_80162680 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

Task* D_actor_341300_80165A2C[3] = { NULL };

SVECTOR D_actor_341300_80165A38[4] = {
    { 2090, -2500, 1440, 0 },
    { 2390, -2500, 1140, 0 },
    { 1900, -2500, 1100, 0 },
    { 1900, -2500, 1000, 0 },
};

SVECTOR D_actor_341300_80165A58[2] = {
    { 1800, -2300, 900, 0 },
    { 2450, -2300, 900, 0 },
};

void func_actor_341300_80162698(Task*);
void func_actor_341300_80162878(Task*);
void func_actor_341300_80163028(Task*);
void func_actor_341300_801631D4(Task*);

TaskDesc D_actor_341300_80165A68[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80162698, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_341300_80162878, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80163028, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_341300_801631D4, { .value = 0 } },
};

TaskDesc D_actor_341300_80165A98 = { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80163A10, { .value = 0 } };

Task* D_actor_341300_80165AA4 = NULL;

static void func_actor_341300_80161E84(void);
void        func_actor_341300_8016268C(void);

/// Draws the two textured quads at fixed positions: each is four fixed
/// model-space corners projected through `gGfxViewCoord.workm`, emitted as a
/// POLY_FT4 at the depth `RotTransPers3` returns, and skipped when the
/// projection flags an error.
static void func_actor_341300_80161E84(void)
{
    s16     x[8];
    s16     y[8];
    long    sxy[8];
    s32     otz[2];
    SVECTOR v[8] = {
        { 0x80C, -0xBC6, 0x150 },
        { 0x83C, -0xBC6, 0x150 },
        { 0x80C, -0xBC6, 0x180 },
        { 0x83C, -0xBC6, 0x180 },
        { 0x747, -0xBC6, 0x150 },
        { 0x777, -0xBC6, 0x150 },
        { 0x747, -0xBC6, 0x180 },
        { 0x777, -0xBC6, 0x180 },
    };
    long      p;
    long      flag;
    s32       i;
    s32       j;
    long      k;
    POLY_FT4* prim;

    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    for (i = 0; i < 2; i++) {
        k = i * 4;
        RotTransPers(&v[k + 3], &sxy[k + 3], &p, &flag);
        otz[i] = RotTransPers3(&v[k], &v[k + 1], &v[k + 2], &sxy[k], &sxy[k + 1], &sxy[k + 2], &p, &flag);
        if (flag >= 0) {
            for (j = k; j < k + 4; j++) {
                x[j] = sxy[j];
                y[j] = sxy[j] >> 16;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2D);
            prim->x0 = x[k];
            prim->y0 = y[k];
            prim->x1 = x[k + 1];
            prim->y1 = y[k + 1];
            prim->x2 = x[k + 2];
            prim->y2 = y[k + 2];
            prim->x3 = x[k + 3];
            prim->y3 = y[k + 3];
            setUV4(prim, 0x23, 0xD1, 0x2F, 0xD1, 0x23, 0xDD, 0x2F, 0xDD);
            setRGB0(prim, 0x80, 0x80, 0x80);
            prim->clut  = 0x3E00;
            prim->tpage = 0x97;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(otz[i] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
    }
}

/// Per-frame task that turns the player (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the target's x/z pair minus the player's own
/// coordinate translation; the delta against `GameActor::rotation.vy` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `taskKill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_actor_341300_80162278(Task* task)
{
    Task*      player;
    GameActor* actor;
    Enemy*     enemy;
    GfxCoord*  self;
    VECTOR*    target;
    s32        angle;
    s32        delta;
    s32        magnitude;
    s32        step;
    s32        wrapped;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor  = (GameActor*)player->work;
    enemy  = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
    if ((enemy != NULL) && (gGameSession->eventState != 0)) {
        self      = player->extra.tmd->coords;
        target    = &D_actor_341300_80165330.pos;
        angle     = ratan2(target->vx - self->coord.t[0], target->vz - self->coord.t[2]);
        delta     = angle - actor->rotation.vy;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->rotation.vy = (s16)((u16)actor->rotation.vy + step);
            return;
        }
        actor->rotation.vy = angle;
    }
    taskKill(task);
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_341300_8016239C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_341300_801623BC(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_341300_801623DC(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_341300_801623FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_341300_8016241C(void)
{
    D_actor_341300_80165AA4 = taskSpawnFromTable(D_actor_341300_80165208, 0, 0, 0);
}

void func_actor_341300_80162450(void)
{
    if (D_actor_341300_80165AA4 != NULL) {
        D_actor_341300_80165AA4->state         = -1;
        D_actor_341300_80165AA4->killCountdown = 0;
        D_actor_341300_80165AA4                = NULL;
    }
}

void func_actor_341300_80162478(Task* arg0)
{
    s16 next;
    s16 count;

    switch (arg0->state) {
        case 0:
            next                = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = next;
            if (next != 0x3D) {
                func_actor_341300_80161E84();
                return;
            }
            return;
        case 1:
            count = arg0->killCountdown;
            if (count < 0x1E) {
                if ((count != 0xA) && (count != 0x14)) {
                    func_actor_341300_80161E84();
                }
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            taskKill(arg0);
            break;
    }
}

void func_actor_341300_80162530(void)
{
    D_actor_341300_80165AA4 = taskSpawnFromTable(D_actor_341300_80165208, 1, 0, 0);
}

void func_actor_341300_80162564(s16 arg0)
{
    func_actor_341300_8016398C(arg0);
}

void func_actor_341300_80162588(s16 arg0)
{
    func_actor_341300_801639CC(arg0);
}

/// Offset from the player's third coordinate that `func_actor_341300_801625AC`
/// spawns its four effects at.
static const SVECTOR D_actor_341300_80161E64 = { 100, -200, -100, 0 };

void func_actor_341300_801625AC(void)
{
    SVECTOR   vec   = D_actor_341300_80161E64;
    GfxCoord* coord = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];

    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x10013300, &vec);
    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x10112280, &vec);
    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x10112280, &vec);
    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x10112280, &vec);
}

void func_actor_341300_80162680(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

void func_actor_341300_80162698(Task* arg0)
{
    s16 i;
    s16 next;
    u16 count;

    switch (arg0->state) {
        case 0:
            i = 0;
            do {
                taskSpawnFromTable(D_actor_341300_80165A68, 1, 0, arg0);
                next = i + 1;
                i    = next;
            } while (next < 0xA);
            goto done;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x1F) {
                do {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 0, arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 1, arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 3:
        case 4:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 3, arg0);
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 1, arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
            done:
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 5:
            break;
    }
}

/// Falling debris shard. State 0 allocates the `_Actor341300ShardWork`,
/// parents the actor's coordinate to the view, places it at
/// `D_actor_341300_80165A38[spawnArg1]` and rolls a random velocity, spin and
/// triangle shape. State 1 applies gravity and velocity, draws the triangle as
/// a POLY_G3 and advances the spin, killing the task once the shard falls below
/// y 0.
void func_actor_341300_80162878(Task* arg0)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               prim;
    s16                    x[3];
    s16                    y[3];
    s32                    sxy;
    s32                    otz;
    s16                    i;
    s32                    v0;
    s32                    v1;
    s32                    v2;
    s32                    v3;

    work  = arg0->work;
    coord = arg0->extra.coordBody->coord;
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(sizeof(_Actor341300ShardWork), 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work          = arg0->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(arg0->work, 0, sizeof(*work));
            taskReparent(arg0->spawnArg2.pointer, arg0);
            coord->coord.t[0] = D_actor_341300_80165A38[arg0->spawnArg1.value].vx;
            coord->coord.t[1] = D_actor_341300_80165A38[arg0->spawnArg1.value].vy;
            coord->coord.t[2] = D_actor_341300_80165A38[arg0->spawnArg1.value].vz;
            work->vel.vx      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vy      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vz      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->spin.vx     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->spin.vy     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->spin.vz     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            if (work->spin.vx > 0) {
                work->spin.vx += 100;
            } else {
                work->spin.vx -= 100;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += 100;
            } else {
                work->spin.vy -= 100;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += 100;
            } else {
                work->spin.vz -= 100;
            }
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? 0x16 : 0x14;
            work->verts[0].vz = 0;
            v0                = rsin(0x2AA) * 20 / 4096;
            if (ACTOR_341300_RAND() & 1) {
                v0 += 2;
            }
            work->verts[1].vx = v0;
            v1                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v1 -= 2;
            }
            work->verts[1].vy = v1;
            work->verts[1].vz = 0;
            v2                = -(rsin(0x2AA) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v2 -= 2;
            }
            work->verts[2].vx = v2;
            v3                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v3 -= 2;
            }
            work->verts[2].vy = v3;
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            if (coord->coord.t[1] > 0) {
            kill:
                taskKill(arg0);
                break;
            }
            work->vel.vy      += 8;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            prim->r0 = prim->g0 = prim->b0 = 0x10;
            prim->r1 = prim->g1 = prim->b1 = 0x40;
            prim->r2 = prim->g2 = prim->b2 = 0x80;
            prim->x0                       = x[0];
            prim->y0                       = y[0];
            prim->x1                       = x[1];
            prim->y1                       = y[1];
            prim->x2                       = x[2];
            prim->y2                       = y[2];
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            work->rot.vx += work->spin.vx;
            work->rot.vy += work->spin.vy;
            work->rot.vz += work->spin.vz;
            gfxRotMatrixY(&coord->coord, work->rot.vy, 1);
            gfxRotMatrixX(&coord->coord, work->rot.vx, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->rot.vz, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

void func_actor_341300_80163028(Task* arg0)
{
    u16 count;

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            break;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 0, arg0);
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 1, arg0);
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 1, arg0);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 0, arg0);
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 0, arg0);
                taskSpawnFromTable(D_actor_341300_80165A68, 3, 1, arg0);
                arg0->state = arg0->state - 1;
            }
            break;
    }
    if (arg0->killCountdown >= 0x1F) {
        arg0->state = 3;
    }
}

void func_actor_341300_801631D4(Task* arg0)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               prim;
    s16                    x[3];
    s16                    y[3];
    s32                    sxy;
    s32                    otz;
    s16                    i;
    s32                    v0;
    s32                    v1;
    s32                    v2;
    s32                    v3;

    work  = arg0->work;
    coord = arg0->extra.coordBody->coord;
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(sizeof(_Actor341300ShardWork), 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work          = arg0->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(arg0->work, 0, sizeof(*work));
            taskReparent(arg0->spawnArg2.pointer, arg0);
            coord->coord.t[0] = D_actor_341300_80165A58[arg0->spawnArg1.value].vx;
            coord->coord.t[1] = D_actor_341300_80165A58[arg0->spawnArg1.value].vy;
            coord->coord.t[2] = D_actor_341300_80165A58[arg0->spawnArg1.value].vz;
            if (arg0->spawnArg1.value == 0) {
                work->vel.vx = ACTOR_341300_RAND() & 0x1F;
            } else {
                work->vel.vx = -(ACTOR_341300_RAND() & 0x1F);
            }
            work->vel.vy  = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vz  = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->spin.vx = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->spin.vy = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->spin.vz = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            if (work->spin.vx > 0) {
                work->spin.vx += 100;
            } else {
                work->spin.vx -= 100;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += 100;
            } else {
                work->spin.vy -= 100;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += 100;
            } else {
                work->spin.vz -= 100;
            }
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? 0x16 : 0x14;
            work->verts[0].vz = 0;
            v0                = rsin(0x2AA) * 20 / 4096;
            if (ACTOR_341300_RAND() & 1) {
                v0 += 2;
            }
            work->verts[1].vx = v0;
            v1                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v1 -= 2;
            }
            work->verts[1].vy = v1;
            work->verts[1].vz = 0;
            v2                = -(rsin(0x2AA) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v2 -= 2;
            }
            work->verts[2].vx = v2;
            v3                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v3 -= 2;
            }
            work->verts[2].vy = v3;
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            if (coord->coord.t[1] > 0) {
            kill:
                taskKill(arg0);
                break;
            }
            work->vel.vy      += 8;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            prim->r0 = prim->g0 = prim->b0 = 0x10;
            prim->r1 = prim->g1 = prim->b1 = 0x40;
            prim->r2 = prim->g2 = prim->b2 = 0x80;
            prim->x0                       = x[0];
            prim->y0                       = y[0];
            prim->x1                       = x[1];
            prim->y1                       = y[1];
            prim->x2                       = x[2];
            prim->y2                       = y[2];
            addPrim(&gGpuCurrentOt[1039], prim);
            work->rot.vx += work->spin.vx;
            work->rot.vy += work->spin.vy;
            work->rot.vz += work->spin.vz;
            gfxRotMatrixY(&coord->coord, work->rot.vy, 1);
            gfxRotMatrixX(&coord->coord, work->rot.vx, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->rot.vz, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

static void func_actor_341300_8016398C(s32 arg0)
{
    if ((arg0 << 0x10) == 0) {
        D_actor_341300_80165A2C[0] = taskSpawnFromTable(D_actor_341300_80165A68, 0, 0, 0);
    }
}

static void func_actor_341300_801639CC(s32 arg0)
{
    if (((arg0 << 0x10) == 0) && (D_actor_341300_80165A2C[0] != NULL)) {
        taskKill(D_actor_341300_80165A2C[0]);
        D_actor_341300_80165A2C[0] = NULL;
    }
}

void func_actor_341300_80163A10(Task* arg0)
{
    s16 i;

    if (arg0->state < 3) {
        if (arg0->state <= 0) {
            if (arg0->state == 0) {
                arg0->killCountdown = 0;
                arg0->state++;
            }
        } else if (++arg0->killCountdown >= 0x10) {
            for (i = 0; i < 0xA; i++) {
                taskSpawnFromTable(D_actor_341300_80165A68, 1, 0, arg0);
                taskSpawnFromTable(D_actor_341300_80165A68, 1, 1, arg0);
            }
            arg0->killCountdown = 0;
            arg0->state++;
        }
    }
}
