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
#include "../../shared/footstep_walk.h"
#include "../../shared/walker.h"
#include "../../shared/pair_walk.h"

/// Reset argument the first enemy's "play animation" opcode leaves behind:
/// `footstepWalkBlendAnim` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 gFootstepWalkBlendFrames;

/// Fade countdown. `func_actor_535700_80131EF0` seeds it from its argument and
/// spawns the fade task from `D_actor_535700_8013346C`; that task
/// (`func_actor_535700_80131E24`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

/// The first enemy's work block, published by its spawn handler.
extern Actor151000Work* gFootstepWalkWork;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* gFootstepWalkTask;

/// Picks the distance `footstepWalkUpdate` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 gFootstepWalkMode;

/// Descriptor of the fade task `func_actor_535700_80131E24`.
extern TaskDesc D_actor_535700_8013346C;

/// The first enemy's message table and the animation data its work block's
/// slots are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call3)(Task*, s32, ActorCommand* request);
        s32 (*call4)(Task*, s32, ActorTransform*);
        s32 (*call5)(Task*, s32, VECTOR*);
        s32 (*call6)(Task*, s32, VECTOR*, s32);
        s32 (*call7)(Task*, s32, s32);
    } handler;
} Actor535700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor535700MsgEntry, 8);

extern Actor535700MsgEntry gFootstepWalkMsgTable[];
extern u8                  gFootstepWalkAnims[];

/// The second enemy's message table, the `TaskDesc` table its sub-model task
/// comes from, and the animation data its work block's slots are seeded from.
extern Actor535700MsgEntry gPairWalkMessages[];
extern TaskDesc            gPairWalkTasks[];
extern u8                  gPairWalkAnimParams[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_535700_801324D4(Enemy* enemy, Task* task);
static void func_actor_535700_80132F74(Enemy* enemy, Task* task);
static void func_actor_535700_80133020(Task* task);

extern TmdSource D_actor_535700_80139A6C;
void             func_actor_535700_80132478(Task*);

s32 func_actor_535700_8013284C(Task*, s32, s32);
s32 func_actor_535700_80132910(Task*, s32, ActorCommand* msg);

extern TmdSource D_actor_535700_80142E58;
extern TmdSource D_actor_535700_8014339C;
s32              func_actor_535700_8013332C(void);
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

TmdBone D_actor_535700_801342B8[19] = {
#include "assets/actor_535700_model_07C4C_skeleton.inc"
};

u32 D_actor_535700_80134564[19] = {
#include "assets/actor_535700_model_07C4C_partVerts.inc"
};

SVECTOR D_actor_535700_801345B0[365] = {
#include "assets/actor_535700_model_07C4C_verts.inc"
};

SVECTOR D_actor_535700_80135118[385] = {
#include "assets/actor_535700_model_07C4C_normals.inc"
};

u32 D_actor_535700_80135D20[3923] = {
#include "assets/actor_535700_model_07C4C_stream.inc"
};

TmdSource D_actor_535700_80139A6C = {
    0,
    21760,
    5992,
    19,
    D_actor_535700_80134564,
    D_actor_535700_801345B0,
    D_actor_535700_80135118,
    D_actor_535700_801342B8,
    D_actor_535700_80135D20,
};

AnimationPackedPose D_actor_535700_80139A90[2] = {
#include "assets/actor_535700_animation_07E5C_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80139AA8[23] = {
#include "assets/actor_535700_animation_07E5C_bank4.inc"
};

AnimationRecord D_actor_535700_80139B04[84] = {
#include "assets/actor_535700_animation_07E5C_records.inc"
};

u16 D_actor_535700_80139C54[20] = {
#include "assets/actor_535700_animation_07E5C_indices.inc"
};

AnimationSet D_actor_535700_80139C7C = {
    D_actor_535700_80139B04,
    D_actor_535700_80139C54,
    { NULL, D_actor_535700_80139A90, NULL, NULL, D_actor_535700_80139AA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_80139CA4[7] = {
#include "assets/actor_535700_animation_08254_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80139CF8[75] = {
#include "assets/actor_535700_animation_08254_bank4.inc"
};

AnimationRecord D_actor_535700_80139E24[138] = {
#include "assets/actor_535700_animation_08254_records.inc"
};

u16 D_actor_535700_8013A04C[20] = {
#include "assets/actor_535700_animation_08254_indices.inc"
};

AnimationSet D_actor_535700_8013A074 = {
    D_actor_535700_80139E24,
    D_actor_535700_8013A04C,
    { NULL, D_actor_535700_80139CA4, NULL, NULL, D_actor_535700_80139CF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013A09C[22] = {
#include "assets/actor_535700_animation_08E38_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013A1A4[298] = {
#include "assets/actor_535700_animation_08E38_bank4.inc"
};

AnimationRecord D_actor_535700_8013A64C[377] = {
#include "assets/actor_535700_animation_08E38_records.inc"
};

u16 D_actor_535700_8013AC30[20] = {
#include "assets/actor_535700_animation_08E38_indices.inc"
};

AnimationSet D_actor_535700_8013AC58 = {
    D_actor_535700_8013A64C,
    D_actor_535700_8013AC30,
    { NULL, D_actor_535700_8013A09C, NULL, NULL, D_actor_535700_8013A1A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013AC80[4] = {
#include "assets/actor_535700_animation_090A0_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013ACB0[48] = {
#include "assets/actor_535700_animation_090A0_bank4.inc"
};

AnimationRecord D_actor_535700_8013AD70[74] = {
#include "assets/actor_535700_animation_090A0_records.inc"
};

u16 D_actor_535700_8013AE98[20] = {
#include "assets/actor_535700_animation_090A0_indices.inc"
};

AnimationSet D_actor_535700_8013AEC0 = {
    D_actor_535700_8013AD70,
    D_actor_535700_8013AE98,
    { NULL, D_actor_535700_8013AC80, NULL, NULL, D_actor_535700_8013ACB0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013AEE8[5] = {
#include "assets/actor_535700_animation_094A8_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013AF24[76] = {
#include "assets/actor_535700_animation_094A8_bank4.inc"
};

AnimationRecord D_actor_535700_8013B054[147] = {
#include "assets/actor_535700_animation_094A8_records.inc"
};

u16 D_actor_535700_8013B2A0[20] = {
#include "assets/actor_535700_animation_094A8_indices.inc"
};

AnimationSet D_actor_535700_8013B2C8 = {
    D_actor_535700_8013B054,
    D_actor_535700_8013B2A0,
    { NULL, D_actor_535700_8013AEE8, NULL, NULL, D_actor_535700_8013AF24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013B2F0[5] = {
#include "assets/actor_535700_animation_098B4_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013B32C[89] = {
#include "assets/actor_535700_animation_098B4_bank4.inc"
};

AnimationRecord D_actor_535700_8013B490[135] = {
#include "assets/actor_535700_animation_098B4_records.inc"
};

u16 D_actor_535700_8013B6AC[20] = {
#include "assets/actor_535700_animation_098B4_indices.inc"
};

AnimationSet D_actor_535700_8013B6D4 = {
    D_actor_535700_8013B490,
    D_actor_535700_8013B6AC,
    { NULL, D_actor_535700_8013B2F0, NULL, NULL, D_actor_535700_8013B32C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013B6FC[4] = {
#include "assets/actor_535700_animation_09BC4_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013B72C[59] = {
#include "assets/actor_535700_animation_09BC4_bank4.inc"
};

AnimationRecord D_actor_535700_8013B818[105] = {
#include "assets/actor_535700_animation_09BC4_records.inc"
};

u16 D_actor_535700_8013B9BC[20] = {
#include "assets/actor_535700_animation_09BC4_indices.inc"
};

AnimationSet D_actor_535700_8013B9E4 = {
    D_actor_535700_8013B818,
    D_actor_535700_8013B9BC,
    { NULL, D_actor_535700_8013B6FC, NULL, NULL, D_actor_535700_8013B72C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013BA0C[2] = {
#include "assets/actor_535700_animation_09E1C_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BA24[30] = {
#include "assets/actor_535700_animation_09E1C_bank4.inc"
};

AnimationRecord D_actor_535700_8013BA9C[94] = {
#include "assets/actor_535700_animation_09E1C_records.inc"
};

u16 D_actor_535700_8013BC14[20] = {
#include "assets/actor_535700_animation_09E1C_indices.inc"
};

AnimationSet D_actor_535700_8013BC3C = {
    D_actor_535700_8013BA9C,
    D_actor_535700_8013BC14,
    { NULL, D_actor_535700_8013BA0C, NULL, NULL, D_actor_535700_8013BA24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013BC64[2] = {
#include "assets/actor_535700_animation_0A0AC_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BC7C[48] = {
#include "assets/actor_535700_animation_0A0AC_bank4.inc"
};

AnimationRecord D_actor_535700_8013BD3C[90] = {
#include "assets/actor_535700_animation_0A0AC_records.inc"
};

u16 D_actor_535700_8013BEA4[20] = {
#include "assets/actor_535700_animation_0A0AC_indices.inc"
};

AnimationSet D_actor_535700_8013BECC = {
    D_actor_535700_8013BD3C,
    D_actor_535700_8013BEA4,
    { NULL, D_actor_535700_8013BC64, NULL, NULL, D_actor_535700_8013BC7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013BEF4[5] = {
#include "assets/actor_535700_animation_0A494_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BF30[59] = {
#include "assets/actor_535700_animation_0A494_bank4.inc"
};

AnimationRecord D_actor_535700_8013C01C[156] = {
#include "assets/actor_535700_animation_0A494_records.inc"
};

u16 D_actor_535700_8013C28C[20] = {
#include "assets/actor_535700_animation_0A494_indices.inc"
};

AnimationSet D_actor_535700_8013C2B4 = {
    D_actor_535700_8013C01C,
    D_actor_535700_8013C28C,
    { NULL, D_actor_535700_8013BEF4, NULL, NULL, D_actor_535700_8013BF30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013C2DC[3] = {
#include "assets/actor_535700_animation_0A744_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C300[29] = {
#include "assets/actor_535700_animation_0A744_bank4.inc"
};

AnimationRecord D_actor_535700_8013C374[114] = {
#include "assets/actor_535700_animation_0A744_records.inc"
};

u16 D_actor_535700_8013C53C[20] = {
#include "assets/actor_535700_animation_0A744_indices.inc"
};

AnimationSet D_actor_535700_8013C564 = {
    D_actor_535700_8013C374,
    D_actor_535700_8013C53C,
    { NULL, D_actor_535700_8013C2DC, NULL, NULL, D_actor_535700_8013C300, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013C58C[2] = {
#include "assets/actor_535700_animation_0A900_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C5A4[20] = {
#include "assets/actor_535700_animation_0A900_bank4.inc"
};

AnimationRecord D_actor_535700_8013C5F4[65] = {
#include "assets/actor_535700_animation_0A900_records.inc"
};

u16 D_actor_535700_8013C6F8[20] = {
#include "assets/actor_535700_animation_0A900_indices.inc"
};

AnimationSet D_actor_535700_8013C720 = {
    D_actor_535700_8013C5F4,
    D_actor_535700_8013C6F8,
    { NULL, D_actor_535700_8013C58C, NULL, NULL, D_actor_535700_8013C5A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013C748[3] = {
#include "assets/actor_535700_animation_0ACE8_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C76C[65] = {
#include "assets/actor_535700_animation_0ACE8_bank4.inc"
};

AnimationRecord D_actor_535700_8013C870[156] = {
#include "assets/actor_535700_animation_0ACE8_records.inc"
};

u16 D_actor_535700_8013CAE0[20] = {
#include "assets/actor_535700_animation_0ACE8_indices.inc"
};

AnimationSet D_actor_535700_8013CB08 = {
    D_actor_535700_8013C870,
    D_actor_535700_8013CAE0,
    { NULL, D_actor_535700_8013C748, NULL, NULL, D_actor_535700_8013C76C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013CB30[18] = {
#include "assets/actor_535700_animation_0B698_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013CC08[232] = {
#include "assets/actor_535700_animation_0B698_bank4.inc"
};

AnimationRecord D_actor_535700_8013CFA8[314] = {
#include "assets/actor_535700_animation_0B698_records.inc"
};

u16 D_actor_535700_8013D490[20] = {
#include "assets/actor_535700_animation_0B698_indices.inc"
};

AnimationSet D_actor_535700_8013D4B8 = {
    D_actor_535700_8013CFA8,
    D_actor_535700_8013D490,
    { NULL, D_actor_535700_8013CB30, NULL, NULL, D_actor_535700_8013CC08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013D4E0[4] = {
#include "assets/actor_535700_animation_0BA14_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013D510[68] = {
#include "assets/actor_535700_animation_0BA14_bank4.inc"
};

AnimationRecord D_actor_535700_8013D620[123] = {
#include "assets/actor_535700_animation_0BA14_records.inc"
};

u16 D_actor_535700_8013D80C[20] = {
#include "assets/actor_535700_animation_0BA14_indices.inc"
};

AnimationSet D_actor_535700_8013D834 = {
    D_actor_535700_8013D620,
    D_actor_535700_8013D80C,
    { NULL, D_actor_535700_8013D4E0, NULL, NULL, D_actor_535700_8013D510, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_8013D85C[2] = {
#include "assets/actor_535700_animation_0BC60_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013D874[27] = {
#include "assets/actor_535700_animation_0BC60_bank4.inc"
};

AnimationRecord D_actor_535700_8013D8E0[94] = {
#include "assets/actor_535700_animation_0BC60_records.inc"
};

u16 D_actor_535700_8013DA58[20] = {
#include "assets/actor_535700_animation_0BC60_indices.inc"
};

AnimationSet D_actor_535700_8013DA80 = {
    D_actor_535700_8013D8E0,
    D_actor_535700_8013DA58,
    { NULL, D_actor_535700_8013D85C, NULL, NULL, D_actor_535700_8013D874, NULL, NULL, NULL },
};

s16 gFootstepWalkBlendFrames = 8;

Actor535700MsgEntry gFootstepWalkMsgTable[6] = {
    { 2003, { .call2 = footstepWalkPlay } },
    { 2005, { .call7 = func_actor_535700_8013284C } },
    { 2004, { .call4 = footstepWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call3 = func_actor_535700_80132910 } },
    { 2013, { .call6 = footstepWalkTo } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_535700_8013DADC = { { { TASK_BODY_TMD, 192 } }, func_actor_535700_80132478, { .model = &D_actor_535700_80139A6C } };

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

TmdBone D_actor_535700_8013DB74[19] = {
#include "assets/actor_535700_model_11038_skeleton.inc"
};

u32 D_actor_535700_8013DE20[19] = {
#include "assets/actor_535700_model_11038_partVerts.inc"
};

SVECTOR D_actor_535700_8013DE6C[339] = {
#include "assets/actor_535700_model_11038_verts.inc"
};

SVECTOR D_actor_535700_8013E904[346] = {
#include "assets/actor_535700_model_11038_normals.inc"
};

u32 D_actor_535700_8013F3D4[3745] = {
#include "assets/actor_535700_model_11038_stream.inc"
};

TmdSource D_actor_535700_80142E58 = {
    0,
    20476,
    5672,
    19,
    D_actor_535700_8013DE20,
    D_actor_535700_8013DE6C,
    D_actor_535700_8013E904,
    D_actor_535700_8013DB74,
    D_actor_535700_8013F3D4,
};

TmdBone D_actor_535700_80142E7C[1] = {
#include "assets/actor_535700_model_1157C_skeleton.inc"
};

u32 D_actor_535700_80142EA0[1] = {
#include "assets/actor_535700_model_1157C_partVerts.inc"
};

SVECTOR D_actor_535700_80142EA4[29] = {
#include "assets/actor_535700_model_1157C_verts.inc"
};

SVECTOR D_actor_535700_80142F8C[24] = {
#include "assets/actor_535700_model_1157C_normals.inc"
};

u32 D_actor_535700_8014304C[212] = {
#include "assets/actor_535700_model_1157C_stream.inc"
};

TmdSource D_actor_535700_8014339C = {
    0,
    1436,
    0,
    1,
    D_actor_535700_80142EA0,
    D_actor_535700_80142EA4,
    D_actor_535700_80142F8C,
    D_actor_535700_80142E7C,
    D_actor_535700_8014304C,
};

AnimationPackedPose D_actor_535700_801433C0[21] = {
#include "assets/actor_535700_animation_121B0_bank1.inc"
};

AnimationPackedRotation D_actor_535700_801434BC[317] = {
#include "assets/actor_535700_animation_121B0_bank4.inc"
};

AnimationRecord D_actor_535700_801439B0[382] = {
#include "assets/actor_535700_animation_121B0_records.inc"
};

u16 D_actor_535700_80143FA8[20] = {
#include "assets/actor_535700_animation_121B0_indices.inc"
};

AnimationSet D_actor_535700_80143FD0 = {
    D_actor_535700_801439B0,
    D_actor_535700_80143FA8,
    { NULL, D_actor_535700_801433C0, NULL, NULL, D_actor_535700_801434BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_80143FF8[12] = {
#include "assets/actor_535700_animation_12918_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80144088[187] = {
#include "assets/actor_535700_animation_12918_bank4.inc"
};

AnimationRecord D_actor_535700_80144374[231] = {
#include "assets/actor_535700_animation_12918_records.inc"
};

u16 D_actor_535700_80144710[20] = {
#include "assets/actor_535700_animation_12918_indices.inc"
};

AnimationSet D_actor_535700_80144738 = {
    D_actor_535700_80144374,
    D_actor_535700_80144710,
    { NULL, D_actor_535700_80143FF8, NULL, NULL, D_actor_535700_80144088, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_80144760[20] = {
#include "assets/actor_535700_animation_13508_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80144850[321] = {
#include "assets/actor_535700_animation_13508_bank4.inc"
};

AnimationRecord D_actor_535700_80144D54[363] = {
#include "assets/actor_535700_animation_13508_records.inc"
};

u16 D_actor_535700_80145300[20] = {
#include "assets/actor_535700_animation_13508_indices.inc"
};

AnimationSet D_actor_535700_80145328 = {
    D_actor_535700_80144D54,
    D_actor_535700_80145300,
    { NULL, D_actor_535700_80144760, NULL, NULL, D_actor_535700_80144850, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_80145350[16] = {
#include "assets/actor_535700_animation_13E70_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80145410[237] = {
#include "assets/actor_535700_animation_13E70_bank4.inc"
};

AnimationRecord D_actor_535700_801457C4[297] = {
#include "assets/actor_535700_animation_13E70_records.inc"
};

u16 D_actor_535700_80145C68[20] = {
#include "assets/actor_535700_animation_13E70_indices.inc"
};

AnimationSet D_actor_535700_80145C90 = {
    D_actor_535700_801457C4,
    D_actor_535700_80145C68,
    { NULL, D_actor_535700_80145350, NULL, NULL, D_actor_535700_80145410, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_535700_80145CB8[26] = {
#include "assets/actor_535700_animation_14998_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80145DF0[224] = {
#include "assets/actor_535700_animation_14998_bank4.inc"
};

AnimationRecord D_actor_535700_80146170[392] = {
#include "assets/actor_535700_animation_14998_records.inc"
};

u16 D_actor_535700_80146790[20] = {
#include "assets/actor_535700_animation_14998_indices.inc"
};

AnimationSet D_actor_535700_801467B8 = {
    D_actor_535700_80146170,
    D_actor_535700_80146790,
    { NULL, D_actor_535700_80145CB8, NULL, NULL, D_actor_535700_80145DF0, NULL, NULL, NULL },
};

Actor535700MsgEntry gPairWalkMessages[6] = {
    { 2003, { .call1 = pairWalkPlay } },
    { 2005, { .call7 = pairWalkSetVisibility } },
    { 2004, { .call4 = pairWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_535700_8013332C } },
    { 2013, { .call5 = pairWalkTo } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc gPairWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_535700_80132F20, { .model = &D_actor_535700_80142E58 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, pairWalkSubModelTask, { .model = &D_actor_535700_8014339C } },
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

Actor151000Work* gFootstepWalkWork;

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
        Task_SpawnFromTable(&D_actor_535700_8013346C, 0, 0, 0);
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
#define walkerUpdate     footstepWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the first enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void footstepWalkExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_play_steps.inc.c"

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_reset_anim.inc.c"

#include "../../shared/footstep_walk_blend_anim.inc.c"

#include "../../shared/footstep_walk_play.inc.c"

/// Visibility opcode of the first enemy: applies `arg2` to the model of the
/// task published in `gFootstepWalkTask` - bit 0 shows it (flags 0)
/// rather than hiding it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_535700_8013284C(Task* task, s32 arg1, s32 arg2)
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
/// `turnFrames` at 0x14 frames, message 1 sets `footsteps`, which turns the
/// footsteps on. Anything else does nothing.
s32 func_actor_535700_80132910(Task* task, s32 arg1, ActorCommand* msg)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            gFootstepWalkWork->turnFrames = 0x14;
            break;
        case 1:
            gFootstepWalkWork->footsteps = kind;
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
#define walkerUpdate     pairWalkUpdate
#define walkerDrawShadow func_actor_535700_80133020
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the second enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void pairWalkExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadowShaded func_actor_535700_80133020
#include "../../shared/walker_shadow_shaded.inc.c"
#undef walkerDrawShadowShaded

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

s32 func_actor_535700_8013332C(void)
{
    return 0;
}

#include "../../shared/pair_walk_to.inc.c"

#include "../../shared/pair_walk_sub_model.inc.c"
