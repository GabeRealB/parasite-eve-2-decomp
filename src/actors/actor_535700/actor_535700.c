#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

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
#include "../../shared/walker.h"
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

/// Reset argument the first enemy's "play animation" opcode leaves behind:
/// `_footstepWalkBlendAnim` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 gFootstepWalkBlendFrames;

/// Fade countdown. `func_actor_535700_80131EF0` seeds it from its argument and
/// spawns the fade task from `D_actor_535700_8013346C`; that task
/// (`func_actor_535700_80131E24`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

/// The first enemy's work block, published by its spawn handler.
extern FootstepWalkWork* gFootstepWalkWork;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* gFootstepWalkTask;

/// Picks the distance `_footstepWalkUpdate` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 gFootstepWalkMode;

/// Descriptor of the fade task `func_actor_535700_80131E24`.
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

static void func_actor_535700_801324D4(Enemy* enemy, Task* task);
static void func_actor_535700_80132F74(Enemy* enemy, Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gActor535700AyaBreaBody;
void             func_actor_535700_80132478(Task*);

s32 func_actor_535700_8013284C(Task*, s32, s32, s32);
s32 func_actor_535700_80132910(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

static TmdSource _gActor535700PawnGolemBody;
static TmdSource _gActor535700GolemBeamSword;
s32              func_actor_535700_8013332C(Task*, s32, s32, s32);
void             func_actor_535700_80132F20(Task*);

void func_actor_535700_80131EF0(s32);
void func_actor_535700_80131F2C(void);

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
void                        func_actor_535700_80131EF0(s32);
void                        func_actor_535700_80131F2C(void);

void func_actor_535700_80131E24(Task*);

TaskDesc D_actor_535700_8013346C = { { { TASK_BODY_NONE, 192 } }, func_actor_535700_80131E24, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_535700_80131EF0 }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_535700_80131F2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_535700_80131EF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_535700_801341E0[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_535700_80131EF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_535700_80131F2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

s16 gFootstepWalkBlendFrames = 8;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_535700_8013284C },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_535700_80132910 },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_535700_8013DADC = { { { TASK_BODY_TMD, 192 } }, func_actor_535700_80132478, { .model = &_gActor535700AyaBreaBody } };

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
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_535700_8013332C },
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

FootstepWalkWork* gFootstepWalkWork;

Task* gFootstepWalkTask;

s16 gFootstepWalkMode;

/// The fade task: while `D_actor_535700_80146840` is non-zero, draws a
/// full-screen black `TILE` into ordering table slot 0xA; once it reaches zero
/// the task kills itself. The count drops by one every frame.
void func_actor_535700_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_535700_80146840 != 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(gGpuCurrentOt + 0xA, tile);
    } else {
        taskKill(task);
    }
    D_actor_535700_80146840--;
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_535700_80131EF0(s32 frames)
{
    D_actor_535700_80146840 = frames;
    if (frames != 0) {
        taskSpawnFromTable(&D_actor_535700_8013346C, 0, 0, 0);
    }
}

void func_actor_535700_80131F2C(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0x1D;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 5;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
        gDisplayState.spriteVariant                                = 1;
        Task_Spawn(0, 0x11, 0, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 6;
        Gp_RestoreStreamRng();
    }
}

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// The first enemy's task body: publishes the task's work block in
/// `gFootstepWalkWork`, then runs the handler for the task's state from
/// a table built on the stack - the spawn handler `footstepWalkSpawn`,
/// then the per-frame `func_actor_535700_801324D4`.
void func_actor_535700_80132478(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        footstepWalkSpawn,
        func_actor_535700_801324D4,
    };

    gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_535700_801324D4
#define walkerUpdate     _footstepWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

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

/// Visibility opcode of the first enemy: applies `arg2` to the model of the
/// task published in `gFootstepWalkTask` - bit 0 shows it (flags 0)
/// rather than hiding it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_535700_8013284C(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = gFootstepWalkTask->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Message handler of the first enemy: message 0 arms the turn countdown
/// `turnFrames` at 0x14 frames, message 1 sets `playFootsteps`, which turns the
/// footsteps on. Anything else does nothing.
s32 func_actor_535700_80132910(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            gFootstepWalkWork->turnFrames = 0x14;
            break;
        case 1:
            gFootstepWalkWork->playFootsteps = kind;
            break;
    }
    return 0;
}

#include "../../shared/footstep_walk_to.inc.c"

#include "../../shared/walker_shadow_shaded.inc.c"

#include "../../shared/pair_walk_spawn.inc.c"

#include "../../shared/pair_walk_update_model.inc.c"

/// The second enemy's task body: runs the handler for the task's state from a
/// table built on the stack - the spawn handler `pairWalkSpawn`,
/// then the per-frame `func_actor_535700_80132F74`.
void func_actor_535700_80132F20(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        pairWalkSpawn,
        func_actor_535700_80132F74,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_535700_80132F74
#define walkerUpdate     _pairWalkUpdate
#define walkerDrawShadow _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the second enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void pairWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

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

s32 func_actor_535700_8013332C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/pair_walk_to.inc.c"

#include "../../shared/pair_walk_sub_model.inc.c"
