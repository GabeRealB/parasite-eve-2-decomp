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

/// Spawn positions `_actor341300Emitter0ShardTask`'s shards start from, indexed
/// by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A38[];

/// Spawn positions `_actor341300Emitter2ShardTask`'s shards start from, indexed
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

/// Scene-event value written at the end of either event-script path.
enum { ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT = 12 };

/// Display-task states and update counts, preserving the script's halfword timer.
enum {
    ACTOR_341300_QUADS_EXIT             = -1,
    ACTOR_341300_QUADS_ACTIVE           = 0,
    ACTOR_341300_QUADS_BLINK            = 1,
    ACTOR_341300_QUADS_SKIPPED_FRAME    = 61,
    ACTOR_341300_QUADS_BLINK_FRAMES     = 30,
    ACTOR_341300_QUADS_BLINK_FIRST_GAP  = 10,
    ACTOR_341300_QUADS_BLINK_SECOND_GAP = 20
};

/// Shard states, world-unit motion/shape bounds and angle-unit spin bounds.
/// Gravity is world units per update squared; spin magnitude is 100..227.
enum {
    ACTOR_341300_SHARD_INIT              = 0,
    ACTOR_341300_SHARD_FALL              = 1,
    ACTOR_341300_SHARD_GRAVITY           = 8,
    ACTOR_341300_SHARD_VELOCITY_MASK     = 31,
    ACTOR_341300_SHARD_SPIN_MASK         = 127,
    ACTOR_341300_SHARD_MIN_SPIN          = 100,
    ACTOR_341300_SHARD_RADIUS            = 20,
    ACTOR_341300_SHARD_JITTER            = 2,
    ACTOR_341300_SHARD_DARK_RGB          = 16,
    ACTOR_341300_SHARD_MIDDLE_RGB        = 64,
    ACTOR_341300_SHARD_LIGHT_RGB         = 128,
    ACTOR_341300_SHARD_DEPTH_SHIFT       = 4,
    ACTOR_341300_EMITTER_2_SHARD_OT_SLOT = 1039
};

static void _actor341300StartDebrisEmitter(s16 emitterIndex);
static void _actor341300StopDebrisEmitter(s16 emitterIndex);

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
static void                     _actor341300StageSceneAudioStart(void);
static void                     _actor341300EnqueueScenePlayback(void);
static void                     _actor341300FinishSceneStream(void);
static void                     _actor341300CancelScene(void);
static void                     _actor341300StartTexturedQuads(void);
static void                     _actor341300RequestTexturedQuadsExit(void);
static void                     _actor341300StartPlayerFacingTask(void);
static void                     _actor341300StartDebrisEmitterCallback(s16 emitterIndex);
static void                     _actor341300StopDebrisEmitterCallback(s16 emitterIndex);
static void                     _actor341300SpawnPlayerHitPuffs(void);
static void                     _actor341300SetSceneEvent(s8 sceneEvent);

static void _actor341300TurnPlayerTowardScenePlacementTask(Task* task);
static void _actor341300TexturedQuadsTask(Task* task);

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
    { { { TASK_BODY_COORD, 192 } }, _actor341300TexturedQuadsTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor341300TurnPlayerTowardScenePlacementTask, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StartPlayerFacingTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541E0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_341300_8016534C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_341300_801652C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165274 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_341300_80165308 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300SpawnPlayerHitPuffs }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_341300_8016531C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165288 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StartTexturedQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_8016529C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300RequestTexturedQuadsExit }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_341300_80165260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_341300_801652DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_341300_80165348 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300FinishSceneStream }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor341300SetSceneEvent }, { .value = ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300RequestTexturedQuadsExit }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor341300SetSceneEvent }, { .value = ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

void        func_actor_341300_80162698(Task*);
static void _actor341300Emitter0ShardTask(Task* task);
void        func_actor_341300_80163028(Task*);
static void _actor341300Emitter2ShardTask(Task* task);

TaskDesc D_actor_341300_80165A68[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80162698, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _actor341300Emitter0ShardTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80163028, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _actor341300Emitter2ShardTask, { .value = 0 } },
};

TaskDesc D_actor_341300_80165A98 = { { { TASK_BODY_NONE, 192 } }, func_actor_341300_80163A10, { .value = 0 } };

Task* D_actor_341300_80165AA4 = NULL;

void func_actor_341300_8016268C(void);

/// Fills a three-corner POLY_G3 packet with the shard's dark-to-light grey ramp.
///
/// `triangle` must be a writable packet; screen arrays contain three s16 pixel
/// coordinates. Arguments must have no side effects: the packet and arrays are
/// evaluated repeatedly. Uses the source's shard colour constants and does not
/// allocate or link a packet. Expands as a statement sequence: invoke inside
/// an existing block, never as an unbraced control-flow body.
#define ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY)                  \
    setPolyG3(triangle);                                                              \
    (triangle)->r0 = (triangle)->g0 = (triangle)->b0 = ACTOR_341300_SHARD_DARK_RGB;   \
    (triangle)->r1 = (triangle)->g1 = (triangle)->b1 = ACTOR_341300_SHARD_MIDDLE_RGB; \
    (triangle)->r2 = (triangle)->g2 = (triangle)->b2 = ACTOR_341300_SHARD_LIGHT_RGB;  \
    (triangle)->x0                                   = (screenX)[0];                  \
    (triangle)->y0                                   = (screenY)[0];                  \
    (triangle)->x1                                   = (screenX)[1];                  \
    (triangle)->y1                                   = (screenY)[1];                  \
    (triangle)->x2                                   = (screenX)[2];                  \
    (triangle)->y2                                   = (screenY)[2];

/// Advances halfword tumble angles and rebuilds the shard's local Y-X-Z rotation.
///
/// Arguments must be live writable task storage without side effects: both are
/// evaluated repeatedly. Translation is preserved and the composed matrix is
/// marked dirty for the following frame. Captures no caller locals. Expands
/// as a statement sequence: invoke inside an existing block, never as an
/// unbraced control-flow body.
#define ACTOR_341300_ADVANCE_SHARD_TUMBLE(shardCoord, work)                         \
    (work)->rot.vx += (work)->spin.vx;                                              \
    (work)->rot.vy += (work)->spin.vy;                                              \
    (work)->rot.vz += (work)->spin.vz;                                              \
    gfxRotMatrixY(&(shardCoord)->coord, (work)->rot.vy, GRAPHICS_ROTATION_REPLACE); \
    gfxRotMatrixX(&(shardCoord)->coord, (work)->rot.vx, GRAPHICS_ROTATION_COMPOSE); \
    gfxRotMatrixZ(&(shardCoord)->coord, (work)->rot.vz, GRAPHICS_ROTATION_COMPOSE); \
    (shardCoord)->composeStamp = GRAPHICS_COORD_DIRTY;

/// Draws two fixed world-space quads with an unmodulated 8-bit texture.
///
/// Requires a composed view coordinate, space for two POLY_FT4 packets, and
/// the texture at VRAM (448, 256) with its CLUT at (0, 248). Quads with a
/// negative projection flag are skipped; accepted depths use the display's
/// ordering-table scaling. Screen XY stays packed in SDK `long` words.
static void _actor341300DrawTexturedQuads(void)
{
    enum {
        ACTOR_341300_QUAD_VERTEX_COUNT = 4,
        ACTOR_341300_QUAD_U_MIN        = 35,
        ACTOR_341300_QUAD_U_MAX        = 47,
        ACTOR_341300_QUAD_V_MIN        = 209,
        ACTOR_341300_QUAD_V_MAX        = 221,
        ACTOR_341300_QUAD_NEUTRAL_RGB  = 128
    };

    s16     screenX[8];
    s16     screenY[8];
    long    packedScreenXY[8];
    s32     quadDepth[2];
    SVECTOR corners[8] = {
        { 0x80C, -0xBC6, 0x150 },
        { 0x83C, -0xBC6, 0x150 },
        { 0x80C, -0xBC6, 0x180 },
        { 0x83C, -0xBC6, 0x180 },
        { 0x747, -0xBC6, 0x150 },
        { 0x777, -0xBC6, 0x150 },
        { 0x747, -0xBC6, 0x180 },
        { 0x777, -0xBC6, 0x180 },
    };
    long      perspective;
    long      projectionFlags;
    s32       quadIndex;
    s32       cornerIndex;
    long      firstCorner;
    POLY_FT4* quad;

    // Project both world-space quads using the current view.
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    for (quadIndex = 0; quadIndex < (s32)ARRAY_SIZE(quadDepth); quadIndex++) {
        firstCorner = quadIndex * ACTOR_341300_QUAD_VERTEX_COUNT;
        RotTransPers(&corners[firstCorner + 3], &packedScreenXY[firstCorner + 3], &perspective, &projectionFlags);
        quadDepth[quadIndex] = RotTransPers3(&corners[firstCorner], &corners[firstCorner + 1], &corners[firstCorner + 2], &packedScreenXY[firstCorner], &packedScreenXY[firstCorner + 1], &packedScreenXY[firstCorner + 2], &perspective, &projectionFlags);
        if (projectionFlags >= 0) {
            for (cornerIndex = firstCorner; cornerIndex < firstCorner + ACTOR_341300_QUAD_VERTEX_COUNT; cornerIndex++) {
                screenX[cornerIndex] = packedScreenXY[cornerIndex];
                screenY[cornerIndex] = packedScreenXY[cornerIndex] >> 16;
            }
            // Raw texture mode bypasses RGB modulation; the packet still stores neutral RGB.
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setShadeTex(quad, 1);
            quad->x0 = screenX[firstCorner];
            quad->y0 = screenY[firstCorner];
            quad->x1 = screenX[firstCorner + 1];
            quad->y1 = screenY[firstCorner + 1];
            quad->x2 = screenX[firstCorner + 2];
            quad->y2 = screenY[firstCorner + 2];
            quad->x3 = screenX[firstCorner + 3];
            quad->y3 = screenY[firstCorner + 3];
            setUV4(quad, ACTOR_341300_QUAD_U_MIN, ACTOR_341300_QUAD_V_MIN, ACTOR_341300_QUAD_U_MAX, ACTOR_341300_QUAD_V_MIN,
                   ACTOR_341300_QUAD_U_MIN, ACTOR_341300_QUAD_V_MAX, ACTOR_341300_QUAD_U_MAX, ACTOR_341300_QUAD_V_MAX);
            setRGB0(quad, ACTOR_341300_QUAD_NEUTRAL_RGB, ACTOR_341300_QUAD_NEUTRAL_RGB, ACTOR_341300_QUAD_NEUTRAL_RGB);
            quad->clut  = getClut(0, 248);
            quad->tpage = getTPage(1, 0, 448, 256);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(quadDepth[quadIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        }
    }
}

/// Turns the player toward the scene's fixed placement while an event is active.
///
/// Steps yaw by 128 angle units per update (4096 per turn), taking the shorter
/// turn and snapping when at most one step remains. Ends on arrival, an idle
/// event, or a missing area-placement actor; that actor only gates the turn.
/// Requires a live player with `GameActor` work and a model root coordinate.
/// Uses the placement's x/z and the player's stored root translation.
static void _actor341300TurnPlayerTowardScenePlacementTask(Task* task)
{
    enum { ACTOR_341300_PLAYER_YAW_STEP = 128 };

    Task*      playerTask;
    GameActor* playerActor;
    Enemy*     areaActor;
    GfxCoord*  playerCoord;
    VECTOR*    targetPosition;
    s32        targetYaw;
    s32        yawDelta;
    s32        absoluteYawDelta;
    s32        yawStep;
    s32        wrappedYawDelta;

    /// Unwraps a yaw difference by at most one 4096-unit turn, retaining half-turn ties.
    ///
    /// Arguments must be distinct writable s32 locals without side effects; they
    /// are evaluated repeatedly. `magnitude` and `wrapped` are scratch outputs.
#define ACTOR_341300_UNWRAP_YAW_DELTA(delta, magnitude, wrapped)  \
    do {                                                          \
        (magnitude) = ABS(delta);                                 \
        if ((magnitude) >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) { \
            (wrapped) = (delta) - ACTOR_TRANSFORM_ANGLE_TURN;     \
            if ((delta) < 0) {                                    \
                (wrapped) = (delta) + ACTOR_TRANSFORM_ANGLE_TURN; \
            }                                                     \
            (delta) = (wrapped);                                  \
        }                                                         \
    } while (0)

    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerActor = playerTask->work;
    areaActor   = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
    if ((areaActor != NULL) && (gGameSession->eventState != 0)) {
        playerCoord    = playerTask->extra.tmd->coords;
        targetPosition = &D_actor_341300_80165330.pos;
        targetYaw      = ratan2(targetPosition->vx - playerCoord->coord.t[0], targetPosition->vz - playerCoord->coord.t[2]);
        yawDelta       = targetYaw - playerActor->rotation.vy;
        ACTOR_341300_UNWRAP_YAW_DELTA(yawDelta, absoluteYawDelta, wrappedYawDelta);
#undef ACTOR_341300_UNWRAP_YAW_DELTA
        absoluteYawDelta = ABS(yawDelta);
        if (absoluteYawDelta >= ACTOR_341300_PLAYER_YAW_STEP + 1) {
            yawStep = ACTOR_341300_PLAYER_YAW_STEP;
            if (yawDelta < 0) {
                yawStep = -ACTOR_341300_PLAYER_YAW_STEP;
            }
            // Preserve the stored yaw's 16-bit wrap.
            playerActor->rotation.vy = (s16)((u16)playerActor->rotation.vy + yawStep);
            return;
        }
        playerActor->rotation.vy = targetYaw;
    }
    taskKill(task);
}

/// Event callback that stages an audio-start request for the selected scene.
///
/// Requires a selected scene descriptor. An audio-free scene queues no request.
static void _actor341300StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Event callback that queues selected-scene audio playback or marks an audio-free scene playing.
static void _actor341300EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Event callback that ends the selected scene stream and restores its saved RNG state.
///
/// Requires a prior scene selection; stream buffers and tasks remain owned by
/// their existing callers.
static void _actor341300FinishSceneStream(void)
{
    streamFinishScene();
}

/// Cancels and finishes the selected scene when the event script is skipped.
///
/// Discards its deferred CD request, requests queue cancellation and finishes
/// the scene stream, restoring the RNG state saved at scene selection.
static void _actor341300CancelScene(void)
{
    cdCmdCancelScene();
}

/// Starts the textured-quad task and stores its handle in the script's shared task slot.
///
/// Replaces the saved handle without killing a previously stored task.
static void _actor341300StartTexturedQuads(void)
{
    enum { ACTOR_341300_QUADS_TASK_INDEX = 0 };

    D_actor_341300_80165AA4 = taskSpawnFromTable(D_actor_341300_80165208, ACTOR_341300_QUADS_TASK_INDEX, 0, NULL);
}

/// Requests immediate exit of the saved quad task and forgets its handle.
///
/// Sets state -1 and clears its counter; teardown occurs on its next update.
/// The shared slot may be NULL and must otherwise hold a live task.
static void _actor341300RequestTexturedQuadsExit(void)
{
    if (D_actor_341300_80165AA4 != NULL) {
        D_actor_341300_80165AA4->state         = ACTOR_341300_QUADS_EXIT;
        D_actor_341300_80165AA4->killCountdown = 0;
        D_actor_341300_80165AA4                = NULL;
    }
}

/// Draws the fixed quads in the task's active or blinking phase.
///
/// State 0 draws except at counter 61; it does not transition or saturate the
/// signed-halfword counter. State 1 draws for counter values below 30 except
/// 10 and 20, then releases the task. Other states, including -1, exit at once.
/// This package starts state 0 and requests -1; it never selects state 1.
static void _actor341300TexturedQuadsTask(Task* task)
{
    s16 nextFrame;
    s16 frame;

    switch (task->state) {
        case ACTOR_341300_QUADS_ACTIVE:
            // Increment with halfword wrap; only count 61 suppresses this frame.
            nextFrame           = (u16)task->killCountdown + 1;
            task->killCountdown = nextFrame;
            if (nextFrame != ACTOR_341300_QUADS_SKIPPED_FRAME) {
                _actor341300DrawTexturedQuads();
                return;
            }
            return;
        case ACTOR_341300_QUADS_BLINK:
            frame = task->killCountdown;
            if (frame < ACTOR_341300_QUADS_BLINK_FRAMES) {
                if ((frame != ACTOR_341300_QUADS_BLINK_FIRST_GAP) && (frame != ACTOR_341300_QUADS_BLINK_SECOND_GAP)) {
                    _actor341300DrawTexturedQuads();
                }
                task->killCountdown = (u16)task->killCountdown + 1;
                return;
            }
        default:
            taskKill(task);
            break;
    }
}

/// Starts the player-facing task and stores its handle in the script's shared task slot.
///
/// Replaces the saved handle without killing a previously stored task.
static void _actor341300StartPlayerFacingTask(void)
{
    enum { ACTOR_341300_PLAYER_FACING_TASK_INDEX = 1 };

    D_actor_341300_80165AA4 = taskSpawnFromTable(D_actor_341300_80165208, ACTOR_341300_PLAYER_FACING_TASK_INDEX, 0, NULL);
}

/// Event callback that starts debris emitter 0; other signed-halfword indices are ignored.
static void _actor341300StartDebrisEmitterCallback(s16 emitterIndex)
{
    _actor341300StartDebrisEmitter(emitterIndex);
}

/// Event callback that releases debris emitter 0 and its child shards; other indices are ignored.
static void _actor341300StopDebrisEmitterCallback(s16 emitterIndex)
{
    _actor341300StopDebrisEmitter(emitterIndex);
}

/// Offset from the player's third coordinate that `_actor341300SpawnPlayerHitPuffs`
/// spawns its four effects at.
static const SVECTOR D_actor_341300_80161E64 = { 100, -200, -100, 0 };

/// Emits the scene's four hit puffs from the player's model part 2.
///
/// Requires a live player TMD task and part 2's initialized coordinate chain.
/// All four sample offset (100, -200, -100) in part-local units during spawn.
/// The first uses size 768 and a three-tick texture period; the other three
/// use size 640, a two-tick period and random initial velocity. All select the
/// alternate palette and additive blend. The puff callback does not follow the
/// retained stack offset; allocation failures are ignored and no handle is saved.
static void _actor341300SpawnPlayerHitPuffs(void)
{
    enum { ACTOR_341300_HIT_PART            = 2,
           ACTOR_341300_HIT_PALETTE         = 0x10000000,
           ACTOR_341300_HIT_RANDOM_VELOCITY = 0x100000,
           ACTOR_341300_HIT_PERIOD_SHIFT    = 12,
           ACTOR_341300_HIT_BLEND_SHIFT     = 16 };
    enum {
        ACTOR_341300_MAIN_HIT_PUFF_ARG  = (ACTOR_341300_HIT_PALETTE | (GPU_BLEND_ADD << ACTOR_341300_HIT_BLEND_SHIFT) | (3 << ACTOR_341300_HIT_PERIOD_SHIFT) | 768),
        ACTOR_341300_SMALL_HIT_PUFF_ARG = (ACTOR_341300_HIT_PALETTE | ACTOR_341300_HIT_RANDOM_VELOCITY | (GPU_BLEND_ADD << ACTOR_341300_HIT_BLEND_SHIFT) | (2 << ACTOR_341300_HIT_PERIOD_SHIFT) | 640)
    };
    SVECTOR   hitOffset       = D_actor_341300_80161E64;
    GfxCoord* playerPartCoord = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[ACTOR_341300_HIT_PART];

    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_MAIN_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
}

/// Stores the event script's signed-byte scene event in the live save's music-selection state.
static void _actor341300SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

void func_actor_341300_80162698(Task* arg0)
{
    s16 i;

    switch (arg0->state) {
        case 0:
            for (i = 0; i < 0xA; i++) {
                taskSpawnFromTable(D_actor_341300_80165A68, 1, 0, arg0);
            }
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            break;
        case 1:
            arg0->killCountdown++;
            if (arg0->killCountdown >= 0x1F) {
                for (i = 0; i < 0xA; i++) {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 0, arg0);
                }
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 2:
            arg0->killCountdown++;
            if (arg0->killCountdown >= 0x10) {
                for (i = 0; i < 0xA; i++) {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 1, arg0);
                }
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 3:
        case 4:
            arg0->killCountdown++;
            if (arg0->killCountdown >= 0x10) {
                for (i = 0; i < 0xA; i++) {
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 3, arg0);
                    taskSpawnFromTable(D_actor_341300_80165A68, 1, 1, arg0);
                }
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 5:
            break;
    }
}

/// Updates one falling, tumbling debris triangle from emitter 0.
///
/// Requires a coordinate-body task, a live emitter in `spawnArg2.pointer`, and
/// a spawn-position index in `spawnArg1.value` (the emitter uses 0, 1 and 3;
/// table index 2 is also valid). Owns its allocated work until task teardown.
/// Uses world units for translation and 4096 angle units per turn. Positive
/// Y is downward; releases the shard on an update starting below world Y zero.
/// Needs one POLY_G3 packet per drawn update and the normal frame ordering
/// table. The third vertex's unsigned SZ3 depth divided by 64 selects tag 0..1023.
static void _actor341300Emitter0ShardTask(Task* task)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               triangle;
    s16                    screenX[3];
    s16                    screenY[3];
    s32                    packedScreenXY;
    s32                    quarterDepth;
    s16                    vertexIndex;
    s32                    rightCornerX;
    s32                    rightCornerY;
    s32                    leftCornerX;
    s32                    leftCornerY;

    work  = task->work;
    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_341300_SHARD_INIT:
            // Own the work block and attach this shard to its emitter's teardown tree.
            task->work = memCalloc(sizeof(*work), false);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            work          = task->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            taskReparent(task->spawnArg2.pointer, task);
            coord->coord.t[0] = D_actor_341300_80165A38[task->spawnArg1.value].vx;
            coord->coord.t[1] = D_actor_341300_80165A38[task->spawnArg1.value].vy;
            coord->coord.t[2] = D_actor_341300_80165A38[task->spawnArg1.value].vz;
            work->vel.vx      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vy      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vz      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->spin.vx     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vy     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vz     = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            if (work->spin.vx > 0) {
                work->spin.vx += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vx -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vy -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vz -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            // Form a radius-20 triangle, jittering each nonzero coordinate outward.
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? ACTOR_341300_SHARD_RADIUS + ACTOR_341300_SHARD_JITTER : ACTOR_341300_SHARD_RADIUS;
            work->verts[0].vz = 0;
            rightCornerX      = rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE;
            if (ACTOR_341300_RAND() & 1) {
                rightCornerX += ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vx = rightCornerX;
            rightCornerY      = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                rightCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vy = rightCornerY;
            work->verts[1].vz = 0;
            leftCornerX       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                leftCornerX -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vx = leftCornerX;
            leftCornerY       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                leftCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vy = leftCornerY;
            work->verts[2].vz = 0;
            task->state++;
            break;
        case ACTOR_341300_SHARD_FALL:
            if (coord->coord.t[1] > 0) {
                taskKill(task);
                break;
            }
            // Advance in world space, then project through the view parent.
            work->vel.vy      += ACTOR_341300_SHARD_GRAVITY;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (vertexIndex = 0; vertexIndex < (s32)ARRAY_SIZE(work->verts); vertexIndex++) {
                gte_ldv0(&work->verts[vertexIndex]);
                gte_rtps();
                gte_stsxy(&packedScreenXY);
                gte_stszotz(&quarterDepth);
                screenX[vertexIndex] = packedScreenXY;
                screenY[vertexIndex] = packedScreenXY >> 16;
            }
            triangle       = gGpuPrimCursor;
            gGpuPrimCursor = triangle + 1;
            ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY);
            addPrim(&gGpuCurrentOt[quarterDepth >> ACTOR_341300_SHARD_DEPTH_SHIFT], triangle);
            // Build the next frame's Y-X-Z tumble after drawing the current rotation.
            ACTOR_341300_ADVANCE_SHARD_TUMBLE(coord, work);
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

/// Updates one falling debris triangle for the unstarted emitter in table slot 2.
///
/// Requires a coordinate-body task, a live emitter in `spawnArg2.pointer`, and
/// `spawnArg1.value` 0 or 1. X velocity is positive at spawn 0 and negative at
/// spawn 1. Owns its work until teardown; translation uses world units and
/// spin uses 4096 angle units per turn. Releases after falling below Y zero.
/// Needs one POLY_G3 packet per drawn update and the normal frame ordering
/// table, including its reserved background entry 1039.
/// The package has a spawning callback for these shards but never starts it.
static void _actor341300Emitter2ShardTask(Task* task)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               triangle;
    s16                    screenX[3];
    s16                    screenY[3];
    s32                    packedScreenXY;
    s32                    quarterDepth;
    s16                    vertexIndex;
    s32                    rightCornerX;
    s32                    rightCornerY;
    s32                    leftCornerX;
    s32                    leftCornerY;

    work  = task->work;
    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_341300_SHARD_INIT:
            // Own the work block and attach this shard to its emitter's teardown tree.
            task->work = memCalloc(sizeof(*work), false);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            work          = task->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            taskReparent(task->spawnArg2.pointer, task);
            coord->coord.t[0] = D_actor_341300_80165A58[task->spawnArg1.value].vx;
            coord->coord.t[1] = D_actor_341300_80165A58[task->spawnArg1.value].vy;
            coord->coord.t[2] = D_actor_341300_80165A58[task->spawnArg1.value].vz;
            if (task->spawnArg1.value == 0) {
                work->vel.vx = ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK;
            } else {
                work->vel.vx = -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            }
            work->vel.vy  = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vz  = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->spin.vx = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vy = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vz = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_RAND() & ACTOR_341300_SHARD_SPIN_MASK);
            if (work->spin.vx > 0) {
                work->spin.vx += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vx -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vy -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vz -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            // Form a radius-20 triangle, jittering each nonzero coordinate outward.
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? ACTOR_341300_SHARD_RADIUS + ACTOR_341300_SHARD_JITTER : ACTOR_341300_SHARD_RADIUS;
            work->verts[0].vz = 0;
            rightCornerX      = rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE;
            if (ACTOR_341300_RAND() & 1) {
                rightCornerX += ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vx = rightCornerX;
            rightCornerY      = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                rightCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vy = rightCornerY;
            work->verts[1].vz = 0;
            leftCornerX       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                leftCornerX -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vx = leftCornerX;
            leftCornerY       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_RAND() & 1) {
                leftCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vy = leftCornerY;
            work->verts[2].vz = 0;
            task->state++;
            break;
        case ACTOR_341300_SHARD_FALL:
            if (coord->coord.t[1] > 0) {
                taskKill(task);
                break;
            }
            // Advance in world space, then project through the view parent.
            work->vel.vy      += ACTOR_341300_SHARD_GRAVITY;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (vertexIndex = 0; vertexIndex < (s32)ARRAY_SIZE(work->verts); vertexIndex++) {
                gte_ldv0(&work->verts[vertexIndex]);
                gte_rtps();
                gte_stsxy(&packedScreenXY);
                gte_stszotz(&quarterDepth);
                screenX[vertexIndex] = packedScreenXY;
                screenY[vertexIndex] = packedScreenXY >> 16;
            }
            triangle       = gGpuPrimCursor;
            gGpuPrimCursor = triangle + 1;
            ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY);
            addPrim(&gGpuCurrentOt[ACTOR_341300_EMITTER_2_SHARD_OT_SLOT], triangle);
            // Build the next frame's Y-X-Z tumble after drawing the current rotation.
            ACTOR_341300_ADVANCE_SHARD_TUMBLE(coord, work);
            break;
    }
}

#undef ACTOR_341300_ADVANCE_SHARD_TUMBLE
#undef ACTOR_341300_FILL_SHARD_TRIANGLE

/// Starts debris emitter 0; other signed-halfword indices do nothing.
///
/// Replaces its saved handle without releasing an earlier emitter. Other
/// indices do nothing; allocation failure stores NULL.
static void _actor341300StartDebrisEmitter(s16 emitterIndex)
{
    if (emitterIndex == 0) {
        D_actor_341300_80165A2C[0] = taskSpawnFromTable(D_actor_341300_80165A68, 0, 0, NULL);
    }
}

/// Releases debris emitter 0 and its child shards; other signed-halfword indices do nothing.
///
/// A NULL saved handle is accepted; a non-NULL handle must still be live.
/// Clears the handle after teardown. Other indices do nothing.
static void _actor341300StopDebrisEmitter(s16 emitterIndex)
{
    if ((emitterIndex == 0) && (D_actor_341300_80165A2C[0] != NULL)) {
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
