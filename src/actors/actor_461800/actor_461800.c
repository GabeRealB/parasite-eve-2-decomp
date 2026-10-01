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
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
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

#include "rooms/neo_ark_r31.h"
#include "../../shared/footstep_walk.h"
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
#include "../../shared/scripted_walk.h"
#include "../../shared/walker.h"

/// Bytes at the scripted-walk mode's address. The first halfword is
/// `gScriptedWalkModeValue`. Nothing reads or writes the second halfword;
/// its role, including padding, is unproven.
extern s16 gScriptedWalkMode[2];

/// Approach mode the last `scriptedWalkTo` selected for the first variant
/// (0 faces the target, step 60; 1 faces away, step 15 backward; 2 faces the
/// target, step 25). Halfword view of `gScriptedWalkMode`, which is how the
/// walk code addresses the mode.
extern s16 gScriptedWalkModeValue __asm__("gScriptedWalkMode");

extern Actor461800Work* gScriptedWalkWork;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

extern Actor151000Work* gFootstepWalkWork;

/// The second variant's task, published by its spawn routine
/// `footstepWalkSpawn` so the handlers can reach its model.
extern Task* gFootstepWalkTask;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC[];

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32                (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        TaskMessageHandler call1;
        s32                (*call2)(Task*, s32, ActorTransform*);
        s32                (*call3)(Task*, s32, VECTOR*, s32);
        s32                (*call4)(Task*, s32, s32);
    } handler;
} Actor461800MessageEntry;
STATIC_ASSERT_SIZEOF(Actor461800MessageEntry, 8);

extern Actor461800MessageEntry D_actor_461800_80139F5C[6];
extern TaskDesc                D_actor_461800_80139F8C[];
extern AnimationSet*           D_actor_461800_80139FB0[6];

extern s32 D_actor_461800_80143884;
extern s32 D_actor_461800_80143888;
extern s32 D_actor_461800_8014388C;
extern s32 D_actor_461800_80143890;

extern Actor461800MessageEntry gFootstepWalkMsgTable[6];
extern AnimationSet*           gFootstepWalkAnims[35];

/// Reset argument the first variant forwards to every reseeded slot.
extern s16 gScriptedWalkBlendFrames;

/// Reset argument the second variant forwards to every reseeded slot.
extern s16 gFootstepWalkBlendFrames;

/// Approach mode the last `footstepWalkTo` call selected.
extern s16 gFootstepWalkMode;

static void func_actor_461800_80132A0C(Enemy* enemy, Task* task);
static void func_actor_461800_80132A90(Task* task);
static void func_actor_461800_801335B0(Enemy* enemy, Task* task);
static void func_actor_461800_80133B98(Task* task);

s32  func_actor_461800_80132D84(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_461800_80132E14(Task*, s32, s32);
s32  func_actor_461800_80132F20(Task*, s32, ActorCommand* request, s32);
s32  func_actor_461800_80133928(Task*, s32, s32);
s32  func_actor_461800_801339EC(Task*, s32, ActorCommand* msg, s32);
void func_actor_461800_801329B0(Task*);
void func_actor_461800_80132B74(Task*);
void func_actor_461800_80133554(Task*);

void func_actor_461800_80131E38(Task*);
void func_actor_461800_80132048(Task*);
void func_actor_461800_801321DC(s32);
void func_actor_461800_8013223C(s32);
void func_actor_461800_8013229C(void);

AnimationPackedPose D_actor_461800_80133C34[3] = {
#include "assets/actor_461800_animation_0206C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80133C58[27] = {
#include "assets/actor_461800_animation_0206C_bank4.inc"
};

AnimationRecord D_actor_461800_80133CC4[104] = {
#include "assets/actor_461800_animation_0206C_records.inc"
};

u16 D_actor_461800_80133E64[20] = {
#include "assets/actor_461800_animation_0206C_indices.inc"
};

AnimationSet D_actor_461800_80133E8C = {
    D_actor_461800_80133CC4,
    D_actor_461800_80133E64,
    { NULL, D_actor_461800_80133C34, NULL, NULL, D_actor_461800_80133C58, NULL, NULL, NULL },
};

Task* D_actor_461800_80133EB4 = NULL;

Task* D_actor_461800_80133EB8 = NULL;

TaskDesc D_actor_461800_80133EBC[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_461800_80132048, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_461800_80131E38, { .value = 0 } },
};

ActorTransform D_actor_461800_80133ED4 = { { 7710, 980, 6290, 0 }, { 0, -1479, 0, 0 } };

ActorTransform D_actor_461800_80133EEC = { { 7030, 980, 7530, 0 }, { 0, -1820, 0, 0 } };

AnimationPlayRequest D_actor_461800_80133F04 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F18 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F2C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F40 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F54 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F68 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_actor_461800_80133F7C[1] = {
    &D_actor_461800_80133E8C,
};

GpCopyArg D_actor_461800_80133F80 = { { .sets = D_actor_461800_80133F7C }, 1 };

EvsSceneKey D_actor_461800_80133F88 = { 6, 18, 11 };

EvsCommand D_actor_461800_80133F90[52] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_461800_80133F88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_461800_80133F80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_461800_80133ED4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_461800_80133EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_461800_80133F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_461800_80134470[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TmdBone D_actor_461800_80134530[20] = {
#include "assets/actor_461800_model_07230_skeleton.inc"
};

u32 D_actor_461800_80134800[20] = {
#include "assets/actor_461800_model_07230_partVerts.inc"
};

SVECTOR D_actor_461800_80134850[300] = {
#include "assets/actor_461800_model_07230_verts.inc"
};

SVECTOR D_actor_461800_801351B0[298] = {
#include "assets/actor_461800_model_07230_normals.inc"
};

u32 D_actor_461800_80135B00[3412] = {
#include "assets/actor_461800_model_07230_stream.inc"
};

TmdSource D_actor_461800_80139050 = {
    0,
    18224,
    5696,
    20,
    D_actor_461800_80134800,
    D_actor_461800_80134850,
    D_actor_461800_801351B0,
    D_actor_461800_80134530,
    D_actor_461800_80135B00,
};

TmdBone D_actor_461800_80139074[1] = {
#include "assets/actor_461800_model_075F8_skeleton.inc"
};

u32 D_actor_461800_80139098[1] = {
#include "assets/actor_461800_model_075F8_partVerts.inc"
};

SVECTOR D_actor_461800_8013909C[18] = {
#include "assets/actor_461800_model_075F8_verts.inc"
};

SVECTOR D_actor_461800_8013912C[18] = {
#include "assets/actor_461800_model_075F8_normals.inc"
};

u32 D_actor_461800_801391BC[151] = {
#include "assets/actor_461800_model_075F8_stream.inc"
};

TmdSource D_actor_461800_80139418 = {
    0,
    1000,
    0,
    1,
    D_actor_461800_80139098,
    D_actor_461800_8013909C,
    D_actor_461800_8013912C,
    D_actor_461800_80139074,
    D_actor_461800_801391BC,
};

TmdBone D_actor_461800_8013943C[1] = {
#include "assets/actor_461800_model_07AE8_skeleton.inc"
};

u32 D_actor_461800_80139460[1] = {
#include "assets/actor_461800_model_07AE8_partVerts.inc"
};

SVECTOR D_actor_461800_80139464[27] = {
#include "assets/actor_461800_model_07AE8_verts.inc"
};

SVECTOR D_actor_461800_8013953C[27] = {
#include "assets/actor_461800_model_07AE8_normals.inc"
};

u32 D_actor_461800_80139614[189] = {
#include "assets/actor_461800_model_07AE8_stream.inc"
};

TmdSource D_actor_461800_80139908 = {
    0,
    1328,
    0,
    1,
    D_actor_461800_80139460,
    D_actor_461800_80139464,
    D_actor_461800_8013953C,
    D_actor_461800_8013943C,
    D_actor_461800_80139614,
};

AnimationPackedPose D_actor_461800_8013992C[2] = {
#include "assets/actor_461800_animation_07D34_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139944[26] = {
#include "assets/actor_461800_animation_07D34_bank4.inc"
};

AnimationRecord D_actor_461800_801399AC[96] = {
#include "assets/actor_461800_animation_07D34_records.inc"
};

u16 D_actor_461800_80139B2C[20] = {
#include "assets/actor_461800_animation_07D34_indices.inc"
};

AnimationSet D_actor_461800_80139B54 = {
    D_actor_461800_801399AC,
    D_actor_461800_80139B2C,
    { NULL, D_actor_461800_8013992C, NULL, NULL, D_actor_461800_80139944, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80139B7C[2] = {
#include "assets/actor_461800_animation_07F58_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139B94[31] = {
#include "assets/actor_461800_animation_07F58_bank4.inc"
};

AnimationRecord D_actor_461800_80139C10[80] = {
#include "assets/actor_461800_animation_07F58_records.inc"
};

u16 D_actor_461800_80139D50[20] = {
#include "assets/actor_461800_animation_07F58_indices.inc"
};

AnimationSet D_actor_461800_80139D78 = {
    D_actor_461800_80139C10,
    D_actor_461800_80139D50,
    { NULL, D_actor_461800_80139B7C, NULL, NULL, D_actor_461800_80139B94, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80139DA0[2] = {
#include "assets/actor_461800_animation_08110_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139DB8[20] = {
#include "assets/actor_461800_animation_08110_bank4.inc"
};

AnimationRecord D_actor_461800_80139E08[64] = {
#include "assets/actor_461800_animation_08110_records.inc"
};

u16 D_actor_461800_80139F08[20] = {
#include "assets/actor_461800_animation_08110_indices.inc"
};

AnimationSet D_actor_461800_80139F30 = {
    D_actor_461800_80139E08,
    D_actor_461800_80139F08,
    { NULL, D_actor_461800_80139DA0, NULL, NULL, D_actor_461800_80139DB8, NULL, NULL, NULL },
};

s16 gScriptedWalkBlendFrames = 8;

Actor461800MessageEntry D_actor_461800_80139F5C[6] = {
    { 2003, { .call0 = func_actor_461800_80132D84 } },
    { 2005, { .call4 = func_actor_461800_80132E14 } },
    { 2004, { .call2 = scriptedWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_461800_80132F20 } },
    { 2013, { .call3 = scriptedWalkTo } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_461800_80139F8C[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_801329B0, { .model = &D_actor_461800_80139050 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80132B74, { .model = &D_actor_461800_80139908 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80132B74, { .model = &D_actor_461800_80139418 } },
};

AnimationSet* D_actor_461800_80139FB0[6] = {
    NULL,
    &D_actor_461800_80139B54,
    &D_actor_461800_80139D78,
    &D_actor_461800_80139F30,
    NULL,
    NULL,
};

TmdBone D_actor_461800_80139FC8[19] = {
#include "assets/actor_461800_model_0D95C_skeleton.inc"
};

u32 D_actor_461800_8013A274[19] = {
#include "assets/actor_461800_model_0D95C_partVerts.inc"
};

SVECTOR D_actor_461800_8013A2C0[365] = {
#include "assets/actor_461800_model_0D95C_verts.inc"
};

SVECTOR D_actor_461800_8013AE28[385] = {
#include "assets/actor_461800_model_0D95C_normals.inc"
};

u32 D_actor_461800_8013BA30[3923] = {
#include "assets/actor_461800_model_0D95C_stream.inc"
};

TmdSource D_actor_461800_8013F77C = {
    0,
    21760,
    5992,
    19,
    D_actor_461800_8013A274,
    D_actor_461800_8013A2C0,
    D_actor_461800_8013AE28,
    D_actor_461800_80139FC8,
    D_actor_461800_8013BA30,
};

AnimationPackedPose D_actor_461800_8013F7A0[2] = {
#include "assets/actor_461800_animation_0DB6C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013F7B8[23] = {
#include "assets/actor_461800_animation_0DB6C_bank4.inc"
};

AnimationRecord D_actor_461800_8013F814[84] = {
#include "assets/actor_461800_animation_0DB6C_records.inc"
};

u16 D_actor_461800_8013F964[20] = {
#include "assets/actor_461800_animation_0DB6C_indices.inc"
};

AnimationSet D_actor_461800_8013F98C = {
    D_actor_461800_8013F814,
    D_actor_461800_8013F964,
    { NULL, D_actor_461800_8013F7A0, NULL, NULL, D_actor_461800_8013F7B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8013F9B4[7] = {
#include "assets/actor_461800_animation_0DF64_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013FA08[75] = {
#include "assets/actor_461800_animation_0DF64_bank4.inc"
};

AnimationRecord D_actor_461800_8013FB34[138] = {
#include "assets/actor_461800_animation_0DF64_records.inc"
};

u16 D_actor_461800_8013FD5C[20] = {
#include "assets/actor_461800_animation_0DF64_indices.inc"
};

AnimationSet D_actor_461800_8013FD84 = {
    D_actor_461800_8013FB34,
    D_actor_461800_8013FD5C,
    { NULL, D_actor_461800_8013F9B4, NULL, NULL, D_actor_461800_8013FA08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8013FDAC[22] = {
#include "assets/actor_461800_animation_0EB48_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013FEB4[298] = {
#include "assets/actor_461800_animation_0EB48_bank4.inc"
};

AnimationRecord D_actor_461800_8014035C[377] = {
#include "assets/actor_461800_animation_0EB48_records.inc"
};

u16 D_actor_461800_80140940[20] = {
#include "assets/actor_461800_animation_0EB48_indices.inc"
};

AnimationSet D_actor_461800_80140968 = {
    D_actor_461800_8014035C,
    D_actor_461800_80140940,
    { NULL, D_actor_461800_8013FDAC, NULL, NULL, D_actor_461800_8013FEB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80140990[4] = {
#include "assets/actor_461800_animation_0EDB0_bank1.inc"
};

AnimationPackedRotation D_actor_461800_801409C0[48] = {
#include "assets/actor_461800_animation_0EDB0_bank4.inc"
};

AnimationRecord D_actor_461800_80140A80[74] = {
#include "assets/actor_461800_animation_0EDB0_records.inc"
};

u16 D_actor_461800_80140BA8[20] = {
#include "assets/actor_461800_animation_0EDB0_indices.inc"
};

AnimationSet D_actor_461800_80140BD0 = {
    D_actor_461800_80140A80,
    D_actor_461800_80140BA8,
    { NULL, D_actor_461800_80140990, NULL, NULL, D_actor_461800_801409C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80140BF8[5] = {
#include "assets/actor_461800_animation_0F1B8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80140C34[76] = {
#include "assets/actor_461800_animation_0F1B8_bank4.inc"
};

AnimationRecord D_actor_461800_80140D64[147] = {
#include "assets/actor_461800_animation_0F1B8_records.inc"
};

u16 D_actor_461800_80140FB0[20] = {
#include "assets/actor_461800_animation_0F1B8_indices.inc"
};

AnimationSet D_actor_461800_80140FD8 = {
    D_actor_461800_80140D64,
    D_actor_461800_80140FB0,
    { NULL, D_actor_461800_80140BF8, NULL, NULL, D_actor_461800_80140C34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141000[5] = {
#include "assets/actor_461800_animation_0F5C4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014103C[89] = {
#include "assets/actor_461800_animation_0F5C4_bank4.inc"
};

AnimationRecord D_actor_461800_801411A0[135] = {
#include "assets/actor_461800_animation_0F5C4_records.inc"
};

u16 D_actor_461800_801413BC[20] = {
#include "assets/actor_461800_animation_0F5C4_indices.inc"
};

AnimationSet D_actor_461800_801413E4 = {
    D_actor_461800_801411A0,
    D_actor_461800_801413BC,
    { NULL, D_actor_461800_80141000, NULL, NULL, D_actor_461800_8014103C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014140C[4] = {
#include "assets/actor_461800_animation_0F8D4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014143C[59] = {
#include "assets/actor_461800_animation_0F8D4_bank4.inc"
};

AnimationRecord D_actor_461800_80141528[105] = {
#include "assets/actor_461800_animation_0F8D4_records.inc"
};

u16 D_actor_461800_801416CC[20] = {
#include "assets/actor_461800_animation_0F8D4_indices.inc"
};

AnimationSet D_actor_461800_801416F4 = {
    D_actor_461800_80141528,
    D_actor_461800_801416CC,
    { NULL, D_actor_461800_8014140C, NULL, NULL, D_actor_461800_8014143C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014171C[2] = {
#include "assets/actor_461800_animation_0FB2C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80141734[30] = {
#include "assets/actor_461800_animation_0FB2C_bank4.inc"
};

AnimationRecord D_actor_461800_801417AC[94] = {
#include "assets/actor_461800_animation_0FB2C_records.inc"
};

u16 D_actor_461800_80141924[20] = {
#include "assets/actor_461800_animation_0FB2C_indices.inc"
};

AnimationSet D_actor_461800_8014194C = {
    D_actor_461800_801417AC,
    D_actor_461800_80141924,
    { NULL, D_actor_461800_8014171C, NULL, NULL, D_actor_461800_80141734, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141974[2] = {
#include "assets/actor_461800_animation_0FDBC_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014198C[48] = {
#include "assets/actor_461800_animation_0FDBC_bank4.inc"
};

AnimationRecord D_actor_461800_80141A4C[90] = {
#include "assets/actor_461800_animation_0FDBC_records.inc"
};

u16 D_actor_461800_80141BB4[20] = {
#include "assets/actor_461800_animation_0FDBC_indices.inc"
};

AnimationSet D_actor_461800_80141BDC = {
    D_actor_461800_80141A4C,
    D_actor_461800_80141BB4,
    { NULL, D_actor_461800_80141974, NULL, NULL, D_actor_461800_8014198C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141C04[5] = {
#include "assets/actor_461800_animation_101A4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80141C40[59] = {
#include "assets/actor_461800_animation_101A4_bank4.inc"
};

AnimationRecord D_actor_461800_80141D2C[156] = {
#include "assets/actor_461800_animation_101A4_records.inc"
};

u16 D_actor_461800_80141F9C[20] = {
#include "assets/actor_461800_animation_101A4_indices.inc"
};

AnimationSet D_actor_461800_80141FC4 = {
    D_actor_461800_80141D2C,
    D_actor_461800_80141F9C,
    { NULL, D_actor_461800_80141C04, NULL, NULL, D_actor_461800_80141C40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141FEC[3] = {
#include "assets/actor_461800_animation_10454_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80142010[29] = {
#include "assets/actor_461800_animation_10454_bank4.inc"
};

AnimationRecord D_actor_461800_80142084[114] = {
#include "assets/actor_461800_animation_10454_records.inc"
};

u16 D_actor_461800_8014224C[20] = {
#include "assets/actor_461800_animation_10454_indices.inc"
};

AnimationSet D_actor_461800_80142274 = {
    D_actor_461800_80142084,
    D_actor_461800_8014224C,
    { NULL, D_actor_461800_80141FEC, NULL, NULL, D_actor_461800_80142010, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014229C[2] = {
#include "assets/actor_461800_animation_10610_bank1.inc"
};

AnimationPackedRotation D_actor_461800_801422B4[20] = {
#include "assets/actor_461800_animation_10610_bank4.inc"
};

AnimationRecord D_actor_461800_80142304[65] = {
#include "assets/actor_461800_animation_10610_records.inc"
};

u16 D_actor_461800_80142408[20] = {
#include "assets/actor_461800_animation_10610_indices.inc"
};

AnimationSet D_actor_461800_80142430 = {
    D_actor_461800_80142304,
    D_actor_461800_80142408,
    { NULL, D_actor_461800_8014229C, NULL, NULL, D_actor_461800_801422B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80142458[3] = {
#include "assets/actor_461800_animation_109F8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014247C[65] = {
#include "assets/actor_461800_animation_109F8_bank4.inc"
};

AnimationRecord D_actor_461800_80142580[156] = {
#include "assets/actor_461800_animation_109F8_records.inc"
};

u16 D_actor_461800_801427F0[20] = {
#include "assets/actor_461800_animation_109F8_indices.inc"
};

AnimationSet D_actor_461800_80142818 = {
    D_actor_461800_80142580,
    D_actor_461800_801427F0,
    { NULL, D_actor_461800_80142458, NULL, NULL, D_actor_461800_8014247C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80142840[18] = {
#include "assets/actor_461800_animation_113A8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80142918[232] = {
#include "assets/actor_461800_animation_113A8_bank4.inc"
};

AnimationRecord D_actor_461800_80142CB8[314] = {
#include "assets/actor_461800_animation_113A8_records.inc"
};

u16 D_actor_461800_801431A0[20] = {
#include "assets/actor_461800_animation_113A8_indices.inc"
};

AnimationSet D_actor_461800_801431C8 = {
    D_actor_461800_80142CB8,
    D_actor_461800_801431A0,
    { NULL, D_actor_461800_80142840, NULL, NULL, D_actor_461800_80142918, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_801431F0[4] = {
#include "assets/actor_461800_animation_11724_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80143220[68] = {
#include "assets/actor_461800_animation_11724_bank4.inc"
};

AnimationRecord D_actor_461800_80143330[123] = {
#include "assets/actor_461800_animation_11724_records.inc"
};

u16 D_actor_461800_8014351C[20] = {
#include "assets/actor_461800_animation_11724_indices.inc"
};

AnimationSet D_actor_461800_80143544 = {
    D_actor_461800_80143330,
    D_actor_461800_8014351C,
    { NULL, D_actor_461800_801431F0, NULL, NULL, D_actor_461800_80143220, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014356C[2] = {
#include "assets/actor_461800_animation_11970_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80143584[27] = {
#include "assets/actor_461800_animation_11970_bank4.inc"
};

AnimationRecord D_actor_461800_801435F0[94] = {
#include "assets/actor_461800_animation_11970_records.inc"
};

u16 D_actor_461800_80143768[20] = {
#include "assets/actor_461800_animation_11970_indices.inc"
};

AnimationSet D_actor_461800_80143790 = {
    D_actor_461800_801435F0,
    D_actor_461800_80143768,
    { NULL, D_actor_461800_8014356C, NULL, NULL, D_actor_461800_80143584, NULL, NULL, NULL },
};

s16 gFootstepWalkBlendFrames = 8;

Actor461800MessageEntry gFootstepWalkMsgTable[6] = {
    { 2003, { .call0 = footstepWalkPlay } },
    { 2005, { .call4 = func_actor_461800_80133928 } },
    { 2004, { .call2 = footstepWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_461800_801339EC } },
    { 2013, { .call3 = footstepWalkTo } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_461800_801437EC = { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80133554, { .model = &D_actor_461800_8013F77C } };

AnimationSet* gFootstepWalkAnims[35] = {
    NULL,
    &D_actor_461800_80140BD0,
    &D_actor_461800_80140FD8,
    &D_actor_461800_801413E4,
    &D_actor_461800_801416F4,
    &D_actor_461800_8014194C,
    &D_actor_461800_80141BDC,
    &D_actor_461800_80141FC4,
    &D_actor_461800_80142274,
    NULL,
    &D_actor_461800_80142430,
    NULL,
    NULL,
    &D_actor_461800_8013F98C,
    &D_actor_461800_8013FD84,
    &D_actor_461800_80140968,
    &D_actor_461800_80142818,
    &D_actor_461800_801431C8,
    &D_actor_461800_80143790,
    &D_actor_461800_80143544,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

s32 D_actor_461800_80143884 = 0;

s32 D_actor_461800_80143888 = 0;

s32 D_actor_461800_8014388C = 0;

s32 D_actor_461800_80143890 = 0;

Actor461800Work* gScriptedWalkWork = NULL;

Task* D_actor_461800_80143898 = NULL;

s16 gScriptedWalkMode[2] = {
    0,
    -0x3658,
};

Actor151000Work* gFootstepWalkWork;

Task* gFootstepWalkTask;

s16 gFootstepWalkMode;

static void func_actor_461800_80132390(Enemy* enemy, Task* task);

void func_actor_461800_80131E38(Task* task)
{
    switch (task->state) {
        case 0:
            D_actor_461800_80143884 = 0x10;
            break;
        case 1:
            if (D_actor_461800_80143888 != 0) {
                if (D_actor_461800_80143884 < 0x10) {
                    D_actor_461800_80143884++;
                } else {
                    D_actor_461800_80143888 = 0;
                }
            } else if (D_actor_461800_80143884 > 0) {
                D_actor_461800_80143884--;
            } else {
                D_actor_461800_80143888 = 1;
            }
            if (D_actor_461800_8014388C != 0) {
                if (D_actor_461800_80143890 < 0x13) {
                    D_actor_461800_80143890++;
                } else {
                    D_actor_461800_8014388C = 0;
                }
            } else if (D_actor_461800_80143890 > 0) {
                D_actor_461800_80143890--;
            } else {
                D_actor_461800_8014388C = 1;
            }
            break;
        case 2:
            if (D_actor_461800_80143884 < 0x10) {
                D_actor_461800_80143884++;
            }
            if (D_actor_461800_80143890 < 0x13) {
                D_actor_461800_80143890++;
            }
            if (D_actor_461800_80143884 == 0x10 && D_actor_461800_80143890 == 0x13) {
                task->state = 3;
            }
            break;
        case 3:
            if (!(task->killCountdown & 3)) {
                if (D_actor_461800_80143884 > 0) {
                    D_actor_461800_80143884--;
                }
                if (D_actor_461800_80143890 > 0) {
                    D_actor_461800_80143890--;
                }
            }
            break;
        case 4:
            displaySetShakeY(0);
            D_neo_ark_r31_8017DC54 = -1;
            return;
    }
    task->killCountdown++;
    D_neo_ark_r31_8017DC54 = D_actor_461800_80143884 / 3 + 3;
}

/// Full-screen fade overlay: `Task::state` picks the ramp (0 snaps it to 90,
/// 1 clears it, 2 counts down, 3 counts up) held in `Task::killCountdown`, which
/// then scales a grey semi-transparent `TILE` linked with its `DR_TPAGE` into
/// OT slot 5.
void func_actor_461800_80132048(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        c;

    switch (task->state) {
        case 1:
            task->killCountdown = 0;
            break;
        case 3:
            if (task->killCountdown < 90) {
                task->killCountdown++;
            }
            break;
        case 2:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case 0:
            task->killCountdown = 90;
            break;
    }
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    c              = (task->killCountdown * 0xFF) / 90;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x80;
    tile->w  = 0x140;
    tile->h  = 0x100;
    tile->r0 = c;
    tile->g0 = c;
    tile->b0 = c;
    addPrim(gGpuCurrentOt + 5, tile);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
    addPrim(gGpuCurrentOt + 5, dr);
}

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = Task_SpawnFromTable(D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = Task_SpawnFromTable(D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

/// Exit path taken when the player leaves through this actor: two flag awards
/// first, then one of two endings depending on whether the two event flags have
/// been seen. With neither seen the session bails out (`restartMode` / `deathFadeFrames`
/// are the stage-load sentinels); otherwise the save header is primed and the
/// boot loader started, with the stream RNG restored behind it. Skipped whole
/// when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` (the current screen id) is 9.
void func_actor_461800_8013229C(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        if (GameFlag_GetNibble(0xEA) == 2) {
            Gp_SetCollectedBit(0x130);
        }
        if (GameFlag_GetNibble(0x113) != 0) {
            Gp_SetCollectedBit(0x12F);
        }
        if (GameFlag_GetNibble(0x112) == 0 && GameFlag_GetNibble(0x113) == 0) {
            gGameSession->restartMode     = GAME_SESSION_RESTART_ENDING;
            gGameSession->deathFadeFrames = 0xF;
            return;
        }
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = 4;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = 0x24;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
        Gp_RestoreStreamRng();
    }
}

/// Spawn tick of the first actor variant: allocates the work block, hangs the
/// model off the view, seeds the animation context and starts the two helper
/// tasks. Each helper takes its texture page and CLUT row from the nested area
/// record the actor's spawn index selects, and is streamed twice once its aux
/// buffer exists.
static void func_actor_461800_80132390(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    Task*      spawned1;
    Task*      spawned2;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (gScriptedWalkWork = memCalloc(0x4F8, false));
    if (gScriptedWalkWork == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_461800_80132A90;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    obj->lightMtx                    = &gScriptedWalkWork->light;
    obj->colorMtx                    = &gScriptedWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_461800_80143898          = task;
    vec.vz                           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&gScriptedWalkWork->rig.anim, D_actor_461800_80139FB0, obj,
                  gScriptedWalkWork->rig.poses, gScriptedWalkWork->rig.slots);
    gScriptedWalkWork->st.animId = 1;
    gScriptedWalkWork->st.state  = 2;

    spawned1 = Task_SpawnFromTable(D_actor_461800_80139F8C, 1, 8, 0);
    if (spawned1 != NULL) {
        gScriptedWalkWork->helper1 = spawned1;
        actorTintModel(spawned1->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned2 = Task_SpawnFromTable(D_actor_461800_80139F8C, 2, 0xC, 0);
    if (spawned2 != NULL) {
        gScriptedWalkWork->helper2 = spawned2;
        actorTintModel(spawned2->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    gScriptedWalkWork->st.travel  = 0;
    gScriptedWalkWork->turnFrames = 0;
    task->msgTable                = D_actor_461800_80139F5C;
    scriptedWalkUpdate(task);
    task->state++;
}

#include "../../shared/scripted_walk_update.inc.c"

/// Two-state dispatcher: publishes the task's work block in
/// `gScriptedWalkWork` on the way through, then calls the handler its
/// state selects.
void func_actor_461800_801329B0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_461800_80132390,
        func_actor_461800_80132A0C,
    };

    gScriptedWalkWork = (Actor461800Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_461800_80132A0C
#define walkerUpdate     scriptedWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` of the first variant: hands the task's `Enemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`, then kills the two helper tasks the spawn routine
/// started.
static void func_actor_461800_80132A90(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
    taskKill(work->helper1);
    taskKill(work->helper2);
}

#include "../../shared/walker_shadow_shaded.inc.c"

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_461800_80143898->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            coord->parent       = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the reset argument is either
/// taken from the preset or left at 2, then the whole slot array is re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 6) {
        gScriptedWalkWork->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            gScriptedWalkWork->st.state = 1;
            gScriptedWalkBlendFrames    = preset->blendFrames;
        } else {
            gScriptedWalkWork->st.state = 2;
        }
        gScriptedWalkWork->st.field_6 = 0;
        scriptedWalkUpdate(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Sets `TmdObject.flags` on the three model objects this actor owns:
/// the one on its own task and the two helper tasks' models in the work block.
/// `arg2 & 1` shows them (flags 0); otherwise each gets `TMD_OBJECT_SKIP_ACTIVE_DRAW`.
/// `arg2 & 2` also sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on each. These are object
/// flags, not `Tmd_Create`'s buffer-flag argument.
s32 func_actor_461800_80132E14(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_461800_80143898->extra.tmd;
    TmdObject* first  = gScriptedWalkWork->helper1->extra.tmd;
    TmdObject* second = gScriptedWalkWork->helper2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        first->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        second->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        own->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        first->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        second->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    if (request->command == 0) {
        gScriptedWalkWork->turnFrames = 0x14;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `gFootstepWalkWork` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        footstepWalkSpawn,
        func_actor_461800_801335B0,
    };

    gFootstepWalkWork = (Actor151000Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_461800_801335B0
#define walkerUpdate     footstepWalkUpdate
#define walkerDrawShadow func_actor_461800_80133B98
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` of the second variant: hands the task's `Enemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`.
void footstepWalkExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_play_steps.inc.c"

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_reset_anim.inc.c"

#include "../../shared/footstep_walk_blend_anim.inc.c"

#include "../../shared/footstep_walk_play.inc.c"

/// Visibility message of the second variant: applies `arg2` to the model of
/// the task published in `gFootstepWalkTask` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_461800_80133928(Task* task, s32 arg1, s32 arg2)
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

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    s32 id;

    id = msg->command;
    switch (id) {
        case 0:
            gFootstepWalkWork->turnFrames = 0x14;
            break;
        case 1:
            gFootstepWalkWork->footsteps = id;
            break;
    }
    return 0;
}

#include "../../shared/footstep_walk_to.inc.c"

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadowShaded func_actor_461800_80133B98
#include "../../shared/walker_shadow_shaded.inc.c"
#undef walkerDrawShadowShaded
