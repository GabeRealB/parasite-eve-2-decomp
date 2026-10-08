#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/companion_load.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
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
// Exported instance: rooms spawn from this package's table by name.
#define gPairWalkTasks       gActor535700PairWalkTasks
#define FOOTSTEP_WALK_WORK_T FootstepWalkWork
#include "../../shared/footstep_walk.h"
#include "../../shared/pair_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);
static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

/// Blackout countdown. `_actor535700SetBlackoutFrames` seeds it from its argument and
/// spawns the blackout task from `D_actor_535700_8013346C`; that task
/// (`_actor535700BlackoutTask`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

static FootstepWalkWork* _gFootstepWalkWork;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* gFootstepWalkTask;

static s16 _gFootstepWalkMode;

/// Descriptor of the blackout task `_actor535700BlackoutTask`.
extern TaskDesc D_actor_535700_8013346C;

/// The first enemy's message table and the animation data its work block's
/// slots are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gFootstepWalkMsgTable[];
extern u8               gFootstepWalkAnims[];

/// The second enemy's message table, the `TaskDesc` table its sub-model task
/// comes from, and the animation data its work block's slots are seeded from.
extern TaskMessageEntry gPairWalkMessages[];
extern TaskDesc         gPairWalkTasks[];
extern u8               gPairWalkAnimParams[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gActor535700AyaBreaBody;
static void      _actor535700FootstepWalkerTask(Task* walkerTask);

static s32 _actor535700SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument);
static s32 _actor535700ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);

static TmdSource _gActor535700PawnGolemBody;
static TmdSource _gActor535700GolemBeamSword;
static s32       _actor535700IgnorePairWalkerCommand(Task* unusedTask, s32 messageId, s32 unusedArgument, s32 unusedSecondArgument);
void             func_actor_535700_80132F20(Task*);

static void _actor535700SetBlackoutFrames(s32 frames);
static void _actor535700FinishScene(void);

extern AnimationPlayRequest D_actor_535700_80133478;
extern AnimationPlayRequest D_actor_535700_801334A4;
extern AnimationPlayRequest D_actor_535700_801334B8;
extern AnimationPlayRequest D_actor_535700_801334CC;
extern AnimationPlayRequest D_actor_535700_801334F4;
extern AnimationPlayRequest D_actor_535700_80133508;
extern AnimationPlayRequest D_actor_535700_8013351C;
extern AnimationPlayRequest D_actor_535700_80133530;
extern AnimationPlayRequest D_actor_535700_80133544;
extern AnimationPlayRequest D_actor_535700_80133558;
extern AnimationPlayRequest D_actor_535700_8013356C;
extern AnimationPlayRequest D_actor_535700_80133580;
extern AnimationPlayRequest D_actor_535700_80133594;
extern AnimationPlayRequest D_actor_535700_801335A8;
extern AnimationPlayRequest D_actor_535700_80133684;
extern ActorCommand         D_actor_535700_8013348C;
extern ActorTransform       D_actor_535700_80133698;
extern ActorTransform       D_actor_535700_801336B0;
extern ActorTransform       D_actor_535700_801336C8;
extern ActorTransform       D_actor_535700_801336E0;
extern ActorTransform       D_actor_535700_801336F8;
extern ActorTransform       D_actor_535700_80133710;
extern ActorTransform       D_actor_535700_80133728;
extern ActorTransform       D_actor_535700_80133740;
extern ActorTransform       D_actor_535700_80133758;
extern ActorTransform       D_actor_535700_80133770;
extern ActorTransform       D_actor_535700_80133788;
extern ActorTransform       D_actor_535700_801337A0;
extern ActorTransform       D_actor_535700_801337B8;
extern ActorTransform       D_actor_535700_801337D0;
extern ActorTransform       D_actor_535700_801337E8;
extern ActorTransform       D_actor_535700_80133800;
extern ActorTransform       D_actor_535700_80133818;
static void                 _actor535700BlackoutTask(Task* task);

TaskDesc D_actor_535700_8013346C = { { { TASK_BODY_NONE, 192 } }, _actor535700BlackoutTask, { .value = 0 } };

AnimationPlayRequest D_actor_535700_80133478 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_actor_535700_8013348C = { { .loc = { 3, 8 } }, 0 };

AnimationPlayRequest D_actor_535700_80133490 = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801334A4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801334B8 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801334CC = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801334E0 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801334F4 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133508 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_8013351C = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133530 = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133544 = { { .index = 0 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133558 = { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_8013356C = { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133580 = { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_80133594 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801335A8 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_535700_801335BC[10] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_535700_80133684 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_535700_80133698 = { { 0x7530, 0, 0x7530, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_801336B0 = { { 50, 0, -0x37C8, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_801336C8 = { { 50, 0, -0x2CC4, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_801336E0 = { { 50, 0, -0x2CC4, 0 }, { 0, -398, 0, 0 } };

ActorTransform D_actor_535700_801336F8 = { { 0, 0, -0x2DC8, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_535700_80133710 = { { 0, 0, -0x2710, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_80133728 = { { -1300, 0, -0x2710, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_80133740 = { { 0, 0, -0x2710, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_535700_80133758 = { { -300, 0, -0x283C, 0 }, { 0, -1991, 0, 0 } };

ActorTransform D_actor_535700_80133770 = { { 0, 0, -0x4650, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_80133788 = { { 0, 0, -0x3BC4, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_535700_801337A0 = { { -3670, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_535700_801337B8 = { { -400, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_535700_801337D0 = { { 3670, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_535700_801337E8 = { { 400, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_535700_80133800 = { { 0, 0, -500, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_535700_80133818 = { { 0, 0, -4500, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_535700_80133830[2] = {
    { { 3500, 0, -2000, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, -2000, 0 }, { 0, -1024, 0, 0 } },
};

ActorTransform D_actor_535700_80133860 = { { 0, 0, -0x38A4, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_535700_80133878 = { { 0, 0, -0x3322, 0 }, { 0, -1024, 0, 0 } };

EvsSceneKey D_actor_535700_80133890 = { 3, 57, 11 };

EvsCommand D_actor_535700_80133898[99] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_535700_80133890 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_535700_80133698 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_535700_80133478 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801336B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_8013356C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_801336C8 } }, { .value = 2 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133558 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801336C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133544 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133558 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801336E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133544 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801334A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_80133770 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801335A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133594 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133788 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801336F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801334B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133710 } }, { .value = 1 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801334CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_535700_8013348C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133580 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133728 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801334B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133710 } }, { .value = 1 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801337A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133684 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_801337B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801337D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133684 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_801337E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_801337D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133684 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_801337E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_80133800 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133684 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133818 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_80133860 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133684 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2013 }, { .message = { .pointer = &D_actor_535700_80133878 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_80133740 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_8013351C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133530 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_535700_80133758 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_801334F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_535700_80133508 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor535700FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_535700_801341E0[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor535700FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor535700AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor535700AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor535700AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor535700AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor535700AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor535700AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor535700AyaBreaBodyPartVerts,
    _gActor535700AyaBreaBodyVerts,
    _gActor535700AyaBreaBodyNormals,
    _gActor535700AyaBreaBodySkeleton,
    _gActor535700AyaBreaBodyStream,
};

static AnimationPackedPose _gActor535700Animation07E5CBank1[2] = {
#include "assets/actor_535700_animation_07E5C_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation07E5CBank4[23] = {
#include "assets/actor_535700_animation_07E5C_bank4.inc"
};

static AnimationRecord _gActor535700Animation07E5CRecords[84] = {
#include "assets/actor_535700_animation_07E5C_records.inc"
};

static u16 _gActor535700Animation07E5CIndices[20] = {
#include "assets/actor_535700_animation_07E5C_indices.inc"
};

static AnimationSet _gActor535700Animation07E5C = {
    _gActor535700Animation07E5CRecords,
    _gActor535700Animation07E5CIndices,
    { NULL, _gActor535700Animation07E5CBank1, NULL, NULL, _gActor535700Animation07E5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation08254Bank1[7] = {
#include "assets/actor_535700_animation_08254_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation08254Bank4[75] = {
#include "assets/actor_535700_animation_08254_bank4.inc"
};

static AnimationRecord _gActor535700Animation08254Records[138] = {
#include "assets/actor_535700_animation_08254_records.inc"
};

static u16 _gActor535700Animation08254Indices[20] = {
#include "assets/actor_535700_animation_08254_indices.inc"
};

static AnimationSet _gActor535700Animation08254 = {
    _gActor535700Animation08254Records,
    _gActor535700Animation08254Indices,
    { NULL, _gActor535700Animation08254Bank1, NULL, NULL, _gActor535700Animation08254Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation08E38Bank1[22] = {
#include "assets/actor_535700_animation_08E38_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation08E38Bank4[298] = {
#include "assets/actor_535700_animation_08E38_bank4.inc"
};

static AnimationRecord _gActor535700Animation08E38Records[377] = {
#include "assets/actor_535700_animation_08E38_records.inc"
};

static u16 _gActor535700Animation08E38Indices[20] = {
#include "assets/actor_535700_animation_08E38_indices.inc"
};

static AnimationSet _gActor535700Animation08E38 = {
    _gActor535700Animation08E38Records,
    _gActor535700Animation08E38Indices,
    { NULL, _gActor535700Animation08E38Bank1, NULL, NULL, _gActor535700Animation08E38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation090A0Bank1[4] = {
#include "assets/actor_535700_animation_090A0_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation090A0Bank4[48] = {
#include "assets/actor_535700_animation_090A0_bank4.inc"
};

static AnimationRecord _gActor535700Animation090A0Records[74] = {
#include "assets/actor_535700_animation_090A0_records.inc"
};

static u16 _gActor535700Animation090A0Indices[20] = {
#include "assets/actor_535700_animation_090A0_indices.inc"
};

static AnimationSet _gActor535700Animation090A0 = {
    _gActor535700Animation090A0Records,
    _gActor535700Animation090A0Indices,
    { NULL, _gActor535700Animation090A0Bank1, NULL, NULL, _gActor535700Animation090A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation094A8Bank1[5] = {
#include "assets/actor_535700_animation_094A8_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation094A8Bank4[76] = {
#include "assets/actor_535700_animation_094A8_bank4.inc"
};

static AnimationRecord _gActor535700Animation094A8Records[147] = {
#include "assets/actor_535700_animation_094A8_records.inc"
};

static u16 _gActor535700Animation094A8Indices[20] = {
#include "assets/actor_535700_animation_094A8_indices.inc"
};

static AnimationSet _gActor535700Animation094A8 = {
    _gActor535700Animation094A8Records,
    _gActor535700Animation094A8Indices,
    { NULL, _gActor535700Animation094A8Bank1, NULL, NULL, _gActor535700Animation094A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation098B4Bank1[5] = {
#include "assets/actor_535700_animation_098B4_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation098B4Bank4[89] = {
#include "assets/actor_535700_animation_098B4_bank4.inc"
};

static AnimationRecord _gActor535700Animation098B4Records[135] = {
#include "assets/actor_535700_animation_098B4_records.inc"
};

static u16 _gActor535700Animation098B4Indices[20] = {
#include "assets/actor_535700_animation_098B4_indices.inc"
};

static AnimationSet _gActor535700Animation098B4 = {
    _gActor535700Animation098B4Records,
    _gActor535700Animation098B4Indices,
    { NULL, _gActor535700Animation098B4Bank1, NULL, NULL, _gActor535700Animation098B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation09BC4Bank1[4] = {
#include "assets/actor_535700_animation_09BC4_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation09BC4Bank4[59] = {
#include "assets/actor_535700_animation_09BC4_bank4.inc"
};

static AnimationRecord _gActor535700Animation09BC4Records[105] = {
#include "assets/actor_535700_animation_09BC4_records.inc"
};

static u16 _gActor535700Animation09BC4Indices[20] = {
#include "assets/actor_535700_animation_09BC4_indices.inc"
};

static AnimationSet _gActor535700Animation09BC4 = {
    _gActor535700Animation09BC4Records,
    _gActor535700Animation09BC4Indices,
    { NULL, _gActor535700Animation09BC4Bank1, NULL, NULL, _gActor535700Animation09BC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation09E1CBank1[2] = {
#include "assets/actor_535700_animation_09E1C_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation09E1CBank4[30] = {
#include "assets/actor_535700_animation_09E1C_bank4.inc"
};

static AnimationRecord _gActor535700Animation09E1CRecords[94] = {
#include "assets/actor_535700_animation_09E1C_records.inc"
};

static u16 _gActor535700Animation09E1CIndices[20] = {
#include "assets/actor_535700_animation_09E1C_indices.inc"
};

static AnimationSet _gActor535700Animation09E1C = {
    _gActor535700Animation09E1CRecords,
    _gActor535700Animation09E1CIndices,
    { NULL, _gActor535700Animation09E1CBank1, NULL, NULL, _gActor535700Animation09E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0A0ACBank1[2] = {
#include "assets/actor_535700_animation_0A0AC_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0A0ACBank4[48] = {
#include "assets/actor_535700_animation_0A0AC_bank4.inc"
};

static AnimationRecord _gActor535700Animation0A0ACRecords[90] = {
#include "assets/actor_535700_animation_0A0AC_records.inc"
};

static u16 _gActor535700Animation0A0ACIndices[20] = {
#include "assets/actor_535700_animation_0A0AC_indices.inc"
};

static AnimationSet _gActor535700Animation0A0AC = {
    _gActor535700Animation0A0ACRecords,
    _gActor535700Animation0A0ACIndices,
    { NULL, _gActor535700Animation0A0ACBank1, NULL, NULL, _gActor535700Animation0A0ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0A494Bank1[5] = {
#include "assets/actor_535700_animation_0A494_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0A494Bank4[59] = {
#include "assets/actor_535700_animation_0A494_bank4.inc"
};

static AnimationRecord _gActor535700Animation0A494Records[156] = {
#include "assets/actor_535700_animation_0A494_records.inc"
};

static u16 _gActor535700Animation0A494Indices[20] = {
#include "assets/actor_535700_animation_0A494_indices.inc"
};

static AnimationSet _gActor535700Animation0A494 = {
    _gActor535700Animation0A494Records,
    _gActor535700Animation0A494Indices,
    { NULL, _gActor535700Animation0A494Bank1, NULL, NULL, _gActor535700Animation0A494Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0A744Bank1[3] = {
#include "assets/actor_535700_animation_0A744_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0A744Bank4[29] = {
#include "assets/actor_535700_animation_0A744_bank4.inc"
};

static AnimationRecord _gActor535700Animation0A744Records[114] = {
#include "assets/actor_535700_animation_0A744_records.inc"
};

static u16 _gActor535700Animation0A744Indices[20] = {
#include "assets/actor_535700_animation_0A744_indices.inc"
};

static AnimationSet _gActor535700Animation0A744 = {
    _gActor535700Animation0A744Records,
    _gActor535700Animation0A744Indices,
    { NULL, _gActor535700Animation0A744Bank1, NULL, NULL, _gActor535700Animation0A744Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0A900Bank1[2] = {
#include "assets/actor_535700_animation_0A900_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0A900Bank4[20] = {
#include "assets/actor_535700_animation_0A900_bank4.inc"
};

static AnimationRecord _gActor535700Animation0A900Records[65] = {
#include "assets/actor_535700_animation_0A900_records.inc"
};

static u16 _gActor535700Animation0A900Indices[20] = {
#include "assets/actor_535700_animation_0A900_indices.inc"
};

static AnimationSet _gActor535700Animation0A900 = {
    _gActor535700Animation0A900Records,
    _gActor535700Animation0A900Indices,
    { NULL, _gActor535700Animation0A900Bank1, NULL, NULL, _gActor535700Animation0A900Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0ACE8Bank1[3] = {
#include "assets/actor_535700_animation_0ACE8_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0ACE8Bank4[65] = {
#include "assets/actor_535700_animation_0ACE8_bank4.inc"
};

static AnimationRecord _gActor535700Animation0ACE8Records[156] = {
#include "assets/actor_535700_animation_0ACE8_records.inc"
};

static u16 _gActor535700Animation0ACE8Indices[20] = {
#include "assets/actor_535700_animation_0ACE8_indices.inc"
};

static AnimationSet _gActor535700Animation0ACE8 = {
    _gActor535700Animation0ACE8Records,
    _gActor535700Animation0ACE8Indices,
    { NULL, _gActor535700Animation0ACE8Bank1, NULL, NULL, _gActor535700Animation0ACE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0B698Bank1[18] = {
#include "assets/actor_535700_animation_0B698_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0B698Bank4[232] = {
#include "assets/actor_535700_animation_0B698_bank4.inc"
};

static AnimationRecord _gActor535700Animation0B698Records[314] = {
#include "assets/actor_535700_animation_0B698_records.inc"
};

static u16 _gActor535700Animation0B698Indices[20] = {
#include "assets/actor_535700_animation_0B698_indices.inc"
};

static AnimationSet _gActor535700Animation0B698 = {
    _gActor535700Animation0B698Records,
    _gActor535700Animation0B698Indices,
    { NULL, _gActor535700Animation0B698Bank1, NULL, NULL, _gActor535700Animation0B698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0BA14Bank1[4] = {
#include "assets/actor_535700_animation_0BA14_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0BA14Bank4[68] = {
#include "assets/actor_535700_animation_0BA14_bank4.inc"
};

static AnimationRecord _gActor535700Animation0BA14Records[123] = {
#include "assets/actor_535700_animation_0BA14_records.inc"
};

static u16 _gActor535700Animation0BA14Indices[20] = {
#include "assets/actor_535700_animation_0BA14_indices.inc"
};

static AnimationSet _gActor535700Animation0BA14 = {
    _gActor535700Animation0BA14Records,
    _gActor535700Animation0BA14Indices,
    { NULL, _gActor535700Animation0BA14Bank1, NULL, NULL, _gActor535700Animation0BA14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation0BC60Bank1[2] = {
#include "assets/actor_535700_animation_0BC60_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation0BC60Bank4[27] = {
#include "assets/actor_535700_animation_0BC60_bank4.inc"
};

static AnimationRecord _gActor535700Animation0BC60Records[94] = {
#include "assets/actor_535700_animation_0BC60_records.inc"
};

static u16 _gActor535700Animation0BC60Indices[20] = {
#include "assets/actor_535700_animation_0BC60_indices.inc"
};

static AnimationSet _gActor535700Animation0BC60 = {
    _gActor535700Animation0BC60Records,
    _gActor535700Animation0BC60Indices,
    { NULL, _gActor535700Animation0BC60Bank1, NULL, NULL, _gActor535700Animation0BC60Bank4, NULL, NULL, NULL },
};

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor535700SetWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor535700ApplyWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_535700_8013DADC = { { { TASK_BODY_TMD, 192 } }, _actor535700FootstepWalkerTask, { .model = &_gActor535700AyaBreaBody } };

u8 gFootstepWalkAnims[140] = {
    0,
    0,
    0,
    0,
    192,
    174,
    19,
    128,
    200,
    178,
    19,
    128,
    212,
    182,
    19,
    128,
    228,
    185,
    19,
    128,
    60,
    188,
    19,
    128,
    204,
    190,
    19,
    128,
    180,
    194,
    19,
    128,
    100,
    197,
    19,
    128,
    0,
    0,
    0,
    0,
    32,
    199,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    124,
    156,
    19,
    128,
    116,
    160,
    19,
    128,
    88,
    172,
    19,
    128,
    8,
    203,
    19,
    128,
    184,
    212,
    19,
    128,
    128,
    218,
    19,
    128,
    52,
    216,
    19,
    128,
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

static TmdBone _gActor535700PawnGolemBodySkeleton[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

static u32 _gActor535700PawnGolemBodyPartVerts[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

static SVECTOR _gActor535700PawnGolemBodyVerts[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

static SVECTOR _gActor535700PawnGolemBodyNormals[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

static u32 _gActor535700PawnGolemBodyStream[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

static TmdSource _gActor535700PawnGolemBody = {
    0,
    20476,
    5672,
    19,
    _gActor535700PawnGolemBodyPartVerts,
    _gActor535700PawnGolemBodyVerts,
    _gActor535700PawnGolemBodyNormals,
    _gActor535700PawnGolemBodySkeleton,
    _gActor535700PawnGolemBodyStream,
};

static TmdBone _gActor535700GolemBeamSwordSkeleton[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

static u32 _gActor535700GolemBeamSwordPartVerts[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

static SVECTOR _gActor535700GolemBeamSwordVerts[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

static SVECTOR _gActor535700GolemBeamSwordNormals[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

static u32 _gActor535700GolemBeamSwordStream[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

static TmdSource _gActor535700GolemBeamSword = {
    0,
    1436,
    0,
    1,
    _gActor535700GolemBeamSwordPartVerts,
    _gActor535700GolemBeamSwordVerts,
    _gActor535700GolemBeamSwordNormals,
    _gActor535700GolemBeamSwordSkeleton,
    _gActor535700GolemBeamSwordStream,
};

static AnimationPackedPose _gActor535700Animation121B0Bank1[21] = {
#include "assets/actor_535700_animation_121B0_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation121B0Bank4[317] = {
#include "assets/actor_535700_animation_121B0_bank4.inc"
};

static AnimationRecord _gActor535700Animation121B0Records[382] = {
#include "assets/actor_535700_animation_121B0_records.inc"
};

static u16 _gActor535700Animation121B0Indices[20] = {
#include "assets/actor_535700_animation_121B0_indices.inc"
};

static AnimationSet _gActor535700Animation121B0 = {
    _gActor535700Animation121B0Records,
    _gActor535700Animation121B0Indices,
    { NULL, _gActor535700Animation121B0Bank1, NULL, NULL, _gActor535700Animation121B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation12918Bank1[12] = {
#include "assets/actor_535700_animation_12918_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation12918Bank4[187] = {
#include "assets/actor_535700_animation_12918_bank4.inc"
};

static AnimationRecord _gActor535700Animation12918Records[231] = {
#include "assets/actor_535700_animation_12918_records.inc"
};

static u16 _gActor535700Animation12918Indices[20] = {
#include "assets/actor_535700_animation_12918_indices.inc"
};

static AnimationSet _gActor535700Animation12918 = {
    _gActor535700Animation12918Records,
    _gActor535700Animation12918Indices,
    { NULL, _gActor535700Animation12918Bank1, NULL, NULL, _gActor535700Animation12918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation13508Bank1[20] = {
#include "assets/actor_535700_animation_13508_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation13508Bank4[321] = {
#include "assets/actor_535700_animation_13508_bank4.inc"
};

static AnimationRecord _gActor535700Animation13508Records[363] = {
#include "assets/actor_535700_animation_13508_records.inc"
};

static u16 _gActor535700Animation13508Indices[20] = {
#include "assets/actor_535700_animation_13508_indices.inc"
};

static AnimationSet _gActor535700Animation13508 = {
    _gActor535700Animation13508Records,
    _gActor535700Animation13508Indices,
    { NULL, _gActor535700Animation13508Bank1, NULL, NULL, _gActor535700Animation13508Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation13E70Bank1[16] = {
#include "assets/actor_535700_animation_13E70_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation13E70Bank4[237] = {
#include "assets/actor_535700_animation_13E70_bank4.inc"
};

static AnimationRecord _gActor535700Animation13E70Records[297] = {
#include "assets/actor_535700_animation_13E70_records.inc"
};

static u16 _gActor535700Animation13E70Indices[20] = {
#include "assets/actor_535700_animation_13E70_indices.inc"
};

static AnimationSet _gActor535700Animation13E70 = {
    _gActor535700Animation13E70Records,
    _gActor535700Animation13E70Indices,
    { NULL, _gActor535700Animation13E70Bank1, NULL, NULL, _gActor535700Animation13E70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor535700Animation14998Bank1[26] = {
#include "assets/actor_535700_animation_14998_bank1.inc"
};

static AnimationPackedRotation _gActor535700Animation14998Bank4[224] = {
#include "assets/actor_535700_animation_14998_bank4.inc"
};

static AnimationRecord _gActor535700Animation14998Records[392] = {
#include "assets/actor_535700_animation_14998_records.inc"
};

static u16 _gActor535700Animation14998Indices[20] = {
#include "assets/actor_535700_animation_14998_indices.inc"
};

static AnimationSet _gActor535700Animation14998 = {
    _gActor535700Animation14998Records,
    _gActor535700Animation14998Indices,
    { NULL, _gActor535700Animation14998Bank1, NULL, NULL, _gActor535700Animation14998Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gPairWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor535700IgnorePairWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _pairWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gPairWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_535700_80132F20, { .model = &_gActor535700PawnGolemBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _pairWalkSubModelTask, { .model = &_gActor535700GolemBeamSword } },
};

u8 gPairWalkAnimParams[24] = {
    0,
    0,
    0,
    0,
    208,
    63,
    20,
    128,
    56,
    71,
    20,
    128,
    40,
    83,
    20,
    128,
    144,
    92,
    20,
    128,
    184,
    103,
    20,
    128,
};

s32 D_actor_535700_80146840;

/// Borrowed pointer to the sound walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkWork* _gFootstepWalkWork;

Task* gFootstepWalkTask;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Queues a centered 320-by-256-pixel opaque black cover for the scene.
///
/// Borrows `sizeof(TILE)` writable, word-aligned bytes at `gGpuPrimCursor` and
/// advances the cursor by that extent without checking capacity. The rectangle
/// starts at (-160, -128) relative to the draw origin and links at tag index 10
/// of `gGpuCurrentOt`, which must be live and contain that entry. The packet
/// and table must survive GPU drawing; no task storage or countdown is changed.
static __inline__ void _actor535700DrawBlackoutTile(void)
{
    enum {
        ACTOR_535700_BLACKOUT_WIDTH_PIXELS  = 320,
        ACTOR_535700_BLACKOUT_HEIGHT_PIXELS = 256,
        ACTOR_535700_BLACKOUT_OT_TAG        = 10
    };

    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -ACTOR_535700_BLACKOUT_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_535700_BLACKOUT_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_535700_BLACKOUT_WIDTH_PIXELS;
    tile->h  = ACTOR_535700_BLACKOUT_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + ACTOR_535700_BLACKOUT_OT_TAG, tile);
}

/// Covers the scene in opaque black while its shared countdown is nonzero.
///
/// Each live-task update with a nonzero count queues one 320-by-256-pixel tile.
/// The first zero-count update kills the task without drawing. Every update
/// decrements the shared count, including the terminating update, which leaves -1.
/// The package must remain loaded, and drawing requires the frame resources
/// described by `_actor535700DrawBlackoutTile`.
static void _actor535700BlackoutTask(Task* task)
{
    if (D_actor_535700_80146840 != 0) {
        _actor535700DrawBlackoutTile();
    } else {
        taskKill(task);
    }
    // The terminating update still consumes the shared countdown after teardown.
    D_actor_535700_80146840--;
}

/// Sets the scene's opaque black countdown and spawns a cover task when nonzero.
///
/// `frames` is the complete signed update count; scripts use 300 to start and
/// zero to stop. Zero lets existing tasks exit on their next update. Every
/// nonzero call spawns another task sharing the same countdown, so a single
/// task covers exactly `frames` updates for a positive count. Negative counts
/// are accepted without clamping. Spawn failure is ignored after storing the count.
/// Keep this package and its task descriptor loaded while cover tasks are live.
static void _actor535700SetBlackoutFrames(s32 frames)
{
    D_actor_535700_80146840 = frames;
    if (frames != 0) {
        taskSpawnFromTable(&D_actor_535700_8013346C, 0, 0, 0);
    }
}

/// Queues the scene's return to the Dryfield night motel balcony.
///
/// Normal completion and skipping select room 2, arrival 5, display resource
/// variant 1 and scene event 6, then finish scene streaming and restore its RNG.
/// Demo 9 leaves all state unchanged. Requires this scene to have been selected
/// successfully and the live save to belong to the Dryfield night stage.
static void _actor535700FinishScene(void)
{
    enum {
        ACTOR_535700_DEMO_SCENE              = 9,
        ACTOR_535700_BALCONY_ARRIVAL         = 5,
        ACTOR_535700_BALCONY_ROOM            = 2,
        ACTOR_535700_SPRITE_VARIANT          = 1,
        ACTOR_535700_POST_SCENE_EVENT        = 6,
        ACTOR_535700_SESSION_TASK_BANK       = GAME_FLOW_RELOAD_TASK_BANK,
        ACTOR_535700_SESSION_TRANSITION_TASK = GAME_FLOW_RELOAD_TASK_SLOT
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_535700_DEMO_SCENE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = ACTOR_535700_BALCONY_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACTOR_535700_BALCONY_ROOM;
        gDisplayState.spriteVariant                                = ACTOR_535700_SPRITE_VARIANT;
        taskSpawn(ACTOR_535700_SESSION_TASK_BANK, ACTOR_535700_SESSION_TRANSITION_TASK, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_535700_POST_SCENE_EVENT;
        streamFinishScene();
    }
}

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// Initializes and updates the scene's footstep-enabled nineteen-part walker.
///
/// `walkerTask` must own a live TMD model and an `Enemy` in `spawnArg2.pointer`.
/// Its state is an unchecked index: 0 allocates and initializes the work block,
/// then enters state 1; 1 refreshes lighting, advances motion and animation, and
/// draws the ground shadow. State 0 requires no existing work; state 1 requires
/// the initialized task-owned `FootstepWalkWork` and borrowed animation data.
/// The package, model, clip data and frame resources must remain live.
///
/// Publishes the task's work before dispatch for the singleton animation and
/// message handlers. Initialization replaces that pointer with its allocation;
/// allocation failure destroys the enemy and starts task teardown. Published
/// pointers are not cleared at teardown and must not be used afterwards.
static void _actor535700FootstepWalkerTask(Task* walkerTask)
{
    const EnemyTaskFunc stateHandlers[] = {
        _footstepWalkSpawn,
        _actorRenderWalkerFrame,
    };

    // Singleton helpers must see this task's work before its state runs.
    _gFootstepWalkWork = walkerTask->work;
    stateHandlers[walkerTask->state](walkerTask->spawnArg2.pointer, walkerTask);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and begins teardown of its task and model.
///
/// `task` must be live with its owned `Enemy` in `spawnArg2.pointer`.
/// Enemy storage is invalid on return; task work and the model follow
/// `taskKill`'s immediate/deferred release rules. Do not use the task afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_play_steps.inc.c"

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_reset_anim.inc.c"

#include "../../shared/footstep_walk_blend_anim.inc.c"

#include "../../shared/footstep_walk_play.inc.c"

/// Replaces the published walker's model flags for `ACTOR_MESSAGE_SET_MODEL_DRAW`.
///
/// Requires a live TMD model in `gFootstepWalkTask`. Bit 0 permits active drawing;
/// without it the model is excluded. Bit 1 suppresses automatic buffer allocation.
/// All other model flags are cleared and other request bits are ignored. No buffer
/// is allocated or released. The receiver, message ID and second payload are
/// ignored. Returns 0.
static s32 _actor535700SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument)
{
    enum {
        ACTOR_535700_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_535700_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1
    };

    TmdObject* model;

    model = gFootstepWalkTask->extra.tmd;
    if (flags & ACTOR_535700_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_535700_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Applies turn or footstep commands to the published walker.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with live `_gFootstepWalkWork` and a
/// command borrowed only for this call. Command 0 schedules twenty turning
/// updates while the turn clip plays; it does not select that clip. Command 1
/// enables footstep sounds until work teardown. Other commands do nothing.
/// Context tags, receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor535700ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        ACTOR_535700_WALKER_COMMAND_TURN             = 0,
        ACTOR_535700_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
        ACTOR_535700_WALKER_TURN_UPDATES             = 20
    };

    s32 commandId;

    commandId = command->command;
    switch (commandId) {
        case ACTOR_535700_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_535700_WALKER_TURN_UPDATES;
            break;
        case ACTOR_535700_WALKER_COMMAND_ENABLE_FOOTSTEPS:
            _gFootstepWalkWork->playFootsteps = commandId;
            break;
    }
    return 0;
}

#include "../../shared/footstep_walk_to.inc.c"

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/pair_walk_spawn.inc.c"

#include "../../shared/pair_walk_update_model.inc.c"

/// The second enemy's task body: runs the handler for the task's state from a
/// table built on the stack - the spawn handler `_pairWalkSpawn`,
/// then the per-frame `_actorRenderWalkerFrameSecond`.
void func_actor_535700_80132F20(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        _pairWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _pairWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/pair_walk_exit.inc.c"

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

/// Ignores `ACTOR_COMMAND_MESSAGE_APPLY` sent to the carried-model walker.
///
/// The receiver, message ID and both payload words are unused; no payload is
/// dereferenced or retained. Returns 0 without changing either model.
static s32 _actor535700IgnorePairWalkerCommand(Task* unusedTask, s32 messageId, s32 unusedArgument, s32 unusedSecondArgument)
{
    return 0;
}

#include "../../shared/pair_walk_to.inc.c"

#include "../../shared/pair_walk_sub_model.inc.c"
