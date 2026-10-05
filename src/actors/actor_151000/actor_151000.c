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
#include "main/gfx.h"
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

/// The clips the package's scene adds to the player's animation bank, with the
/// play requests stored after them.
///
/// The longer of the package's two event scripts sends the player a copy
/// request for this storage before it plays any of the clips; that request is
/// a separate object. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words
/// from the start of the storage, which is more than the clip table holds: the
/// three set pointers occupy extended ids 47-49, and the first 29 words of the
/// play requests are written into the bank after them. The scripts' requests
/// to the player select ids 47-49 and the bank's own id 1 only, so none of
/// those request words is played as a clip.
///
/// The play requests open the run of requests the package keeps for its
/// scripts, and are part of this object only because the copied span reaches
/// into the sixth; the rest of the run follows as separate objects. The run
/// mixes the two receivers: the scripts play the second to fourth on the
/// player and send the sixth to the package's walker, the character the
/// scripts walk around, which reads the animation id as an index into its own
/// clip table and ignores the bank selector.
typedef union {
    struct {
        AnimationSet*        sets[3];         // Player clips for extended ids 47-49
        AnimationPlayRequest playRequests[6]; // Resets for ids 47, 47, 48, 49, 0 and 16: the second to fourth play the extended clips on the player, the sixth plays the walker's clip 16; nothing references the first or the fifth
    } data;                                   // The records by name
    s32 words[33];                            // The same storage as the copy reads it; the last word lies beyond the copied span
} _Actor151000AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor151000AnimationBankExtensionStorage, 132);

extern _Actor151000AnimationBankExtensionStorage D_actor_151000_8013336C;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Reset argument the "start animation" opcode leaves behind:
/// `footstepWalkBlendAnim` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 gFootstepWalkBlendFrames;

/// Fade countdown: `func_actor_151000_80131EE0` seeds it, and the fade task
/// `func_actor_151000_80131E24` draws while it is non-zero.
extern s32 D_actor_151000_8013D378;

/// The enemy's work block, published by its spawn handler and by its task
/// body.
extern FootstepWalkWork* gFootstepWalkWork;

/// The enemy's task, published by its spawn handler so the visibility opcode
/// can reach its model.
extern Task* gFootstepWalkTask;

/// Picks the distance the runner walks the model each frame: 0 steps 0x3C
/// forward, 1 steps 0xF back, 2 steps 0x19 forward. Set by the "walk to"
/// opcode.
extern s16 gFootstepWalkMode;

/// Descriptor of the fade task `func_actor_151000_80131E24`.
extern TaskDesc D_actor_151000_80133360;

/// The enemy's message table and the animation data its work block's slots
/// are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gFootstepWalkMsgTable[];
extern u8               gFootstepWalkAnims[];

static void func_actor_151000_80132450(Enemy* enemy, Task* task);

static TmdSource _gActor151000AyaBreaBody;
void             func_actor_151000_801323F4(Task*);

s32 func_actor_151000_801327C8(Task*, s32, s32, s32);
s32 func_actor_151000_8013288C(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

extern AnimationPlayRequest D_actor_151000_801333F0;
extern AnimationPlayRequest D_actor_151000_80133404;
void                        func_actor_151000_80131EE0(s32);

void func_actor_151000_80131E24(Task*);

static AnimationPackedPose _gActor151000Animation00ED8Bank1[2] = {
#include "assets/actor_151000_animation_00ED8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation00ED8Bank4[25] = {
#include "assets/actor_151000_animation_00ED8_bank4.inc"
};

static AnimationRecord _gActor151000Animation00ED8Records[96] = {
#include "assets/actor_151000_animation_00ED8_records.inc"
};

static u16 _gActor151000Animation00ED8Indices[20] = {
#include "assets/actor_151000_animation_00ED8_indices.inc"
};

static AnimationSet _gActor151000Animation00ED8 = {
    _gActor151000Animation00ED8Records,
    _gActor151000Animation00ED8Indices,
    { NULL, _gActor151000Animation00ED8Bank1, NULL, NULL, _gActor151000Animation00ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation012A0Bank1[6] = {
#include "assets/actor_151000_animation_012A0_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation012A0Bank4[76] = {
#include "assets/actor_151000_animation_012A0_bank4.inc"
};

static AnimationRecord _gActor151000Animation012A0Records[128] = {
#include "assets/actor_151000_animation_012A0_records.inc"
};

static u16 _gActor151000Animation012A0Indices[20] = {
#include "assets/actor_151000_animation_012A0_indices.inc"
};

static AnimationSet _gActor151000Animation012A0 = {
    _gActor151000Animation012A0Records,
    _gActor151000Animation012A0Indices,
    { NULL, _gActor151000Animation012A0Bank1, NULL, NULL, _gActor151000Animation012A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation01518Bank1[3] = {
#include "assets/actor_151000_animation_01518_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation01518Bank4[45] = {
#include "assets/actor_151000_animation_01518_bank4.inc"
};

static AnimationRecord _gActor151000Animation01518Records[84] = {
#include "assets/actor_151000_animation_01518_records.inc"
};

static u16 _gActor151000Animation01518Indices[20] = {
#include "assets/actor_151000_animation_01518_indices.inc"
};

static AnimationSet _gActor151000Animation01518 = {
    _gActor151000Animation01518Records,
    _gActor151000Animation01518Indices,
    { NULL, _gActor151000Animation01518Bank1, NULL, NULL, _gActor151000Animation01518Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_151000_80133360 = { { { TASK_BODY_NONE, 192 } }, func_actor_151000_80131E24, { .value = 0 } };

_Actor151000AnimationBankExtensionStorage D_actor_151000_8013336C = { .data = { { &_gActor151000Animation00ED8, &_gActor151000Animation012A0, &_gActor151000Animation01518 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_151000_801333F0 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_151000_80133404 = { { .index = 1 }, 19, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_151000_80133418[2] = {
    { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_151000_80133440 = { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_151000_80133454 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_actor_151000_80133468 = { { .loc = { 5, 15 } }, 1 };

ActorTransform D_actor_151000_8013346C = { { -7700, 0, -0x46B4, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_151000_80133484 = { { -6300, 0, -0x4330, 0 }, { 0, 227, 0, 0 } };

ActorTransform D_actor_151000_8013349C = { { -7480, 0, -0x44B6, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_151000_801334B4 = { { -4900, 0, -0x3CBE, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_151000_801334CC = { { -3600, 0, -0x30D4, 0 }, { 0, 0, 0, 0 } };

AnimationBankCopyRequest D_actor_151000_801334E4 = { { .words = D_actor_151000_8013336C.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

EvsCommand D_actor_151000_801334EC[47] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_151000_801334E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_151000_8013346C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_151000_8013349C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_151000_80133468 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_151000_80131EE0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_8013336C.data.playRequests[5] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x550F0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x550F0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_151000_80131EE0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_80133404 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_801333F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_151000_80133484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_151000_801334B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_80133440 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_151000_801334CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_8013336C.data.playRequests[5] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_80133454 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_151000_80133954[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_151000_80131EE0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_151000_8013336C.data.playRequests[5] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_80133454 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_151000_80133484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor151000AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor151000AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor151000AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor151000AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor151000AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor151000AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor151000AyaBreaBodyPartVerts,
    _gActor151000AyaBreaBodyVerts,
    _gActor151000AyaBreaBodyNormals,
    _gActor151000AyaBreaBodySkeleton,
    _gActor151000AyaBreaBodyStream,
};

static AnimationPackedPose _gActor151000Animation07660Bank1[2] = {
#include "assets/actor_151000_animation_07660_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation07660Bank4[23] = {
#include "assets/actor_151000_animation_07660_bank4.inc"
};

static AnimationRecord _gActor151000Animation07660Records[84] = {
#include "assets/actor_151000_animation_07660_records.inc"
};

static u16 _gActor151000Animation07660Indices[20] = {
#include "assets/actor_151000_animation_07660_indices.inc"
};

static AnimationSet _gActor151000Animation07660 = {
    _gActor151000Animation07660Records,
    _gActor151000Animation07660Indices,
    { NULL, _gActor151000Animation07660Bank1, NULL, NULL, _gActor151000Animation07660Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation07A58Bank1[7] = {
#include "assets/actor_151000_animation_07A58_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation07A58Bank4[75] = {
#include "assets/actor_151000_animation_07A58_bank4.inc"
};

static AnimationRecord _gActor151000Animation07A58Records[138] = {
#include "assets/actor_151000_animation_07A58_records.inc"
};

static u16 _gActor151000Animation07A58Indices[20] = {
#include "assets/actor_151000_animation_07A58_indices.inc"
};

static AnimationSet _gActor151000Animation07A58 = {
    _gActor151000Animation07A58Records,
    _gActor151000Animation07A58Indices,
    { NULL, _gActor151000Animation07A58Bank1, NULL, NULL, _gActor151000Animation07A58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0863CBank1[22] = {
#include "assets/actor_151000_animation_0863C_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0863CBank4[298] = {
#include "assets/actor_151000_animation_0863C_bank4.inc"
};

static AnimationRecord _gActor151000Animation0863CRecords[377] = {
#include "assets/actor_151000_animation_0863C_records.inc"
};

static u16 _gActor151000Animation0863CIndices[20] = {
#include "assets/actor_151000_animation_0863C_indices.inc"
};

static AnimationSet _gActor151000Animation0863C = {
    _gActor151000Animation0863CRecords,
    _gActor151000Animation0863CIndices,
    { NULL, _gActor151000Animation0863CBank1, NULL, NULL, _gActor151000Animation0863CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation088A4Bank1[4] = {
#include "assets/actor_151000_animation_088A4_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation088A4Bank4[48] = {
#include "assets/actor_151000_animation_088A4_bank4.inc"
};

static AnimationRecord _gActor151000Animation088A4Records[74] = {
#include "assets/actor_151000_animation_088A4_records.inc"
};

static u16 _gActor151000Animation088A4Indices[20] = {
#include "assets/actor_151000_animation_088A4_indices.inc"
};

static AnimationSet _gActor151000Animation088A4 = {
    _gActor151000Animation088A4Records,
    _gActor151000Animation088A4Indices,
    { NULL, _gActor151000Animation088A4Bank1, NULL, NULL, _gActor151000Animation088A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation08CACBank1[5] = {
#include "assets/actor_151000_animation_08CAC_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation08CACBank4[76] = {
#include "assets/actor_151000_animation_08CAC_bank4.inc"
};

static AnimationRecord _gActor151000Animation08CACRecords[147] = {
#include "assets/actor_151000_animation_08CAC_records.inc"
};

static u16 _gActor151000Animation08CACIndices[20] = {
#include "assets/actor_151000_animation_08CAC_indices.inc"
};

static AnimationSet _gActor151000Animation08CAC = {
    _gActor151000Animation08CACRecords,
    _gActor151000Animation08CACIndices,
    { NULL, _gActor151000Animation08CACBank1, NULL, NULL, _gActor151000Animation08CACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation090B8Bank1[5] = {
#include "assets/actor_151000_animation_090B8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation090B8Bank4[89] = {
#include "assets/actor_151000_animation_090B8_bank4.inc"
};

static AnimationRecord _gActor151000Animation090B8Records[135] = {
#include "assets/actor_151000_animation_090B8_records.inc"
};

static u16 _gActor151000Animation090B8Indices[20] = {
#include "assets/actor_151000_animation_090B8_indices.inc"
};

static AnimationSet _gActor151000Animation090B8 = {
    _gActor151000Animation090B8Records,
    _gActor151000Animation090B8Indices,
    { NULL, _gActor151000Animation090B8Bank1, NULL, NULL, _gActor151000Animation090B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation093C8Bank1[4] = {
#include "assets/actor_151000_animation_093C8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation093C8Bank4[59] = {
#include "assets/actor_151000_animation_093C8_bank4.inc"
};

static AnimationRecord _gActor151000Animation093C8Records[105] = {
#include "assets/actor_151000_animation_093C8_records.inc"
};

static u16 _gActor151000Animation093C8Indices[20] = {
#include "assets/actor_151000_animation_093C8_indices.inc"
};

static AnimationSet _gActor151000Animation093C8 = {
    _gActor151000Animation093C8Records,
    _gActor151000Animation093C8Indices,
    { NULL, _gActor151000Animation093C8Bank1, NULL, NULL, _gActor151000Animation093C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09620Bank1[2] = {
#include "assets/actor_151000_animation_09620_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09620Bank4[30] = {
#include "assets/actor_151000_animation_09620_bank4.inc"
};

static AnimationRecord _gActor151000Animation09620Records[94] = {
#include "assets/actor_151000_animation_09620_records.inc"
};

static u16 _gActor151000Animation09620Indices[20] = {
#include "assets/actor_151000_animation_09620_indices.inc"
};

static AnimationSet _gActor151000Animation09620 = {
    _gActor151000Animation09620Records,
    _gActor151000Animation09620Indices,
    { NULL, _gActor151000Animation09620Bank1, NULL, NULL, _gActor151000Animation09620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation098B0Bank1[2] = {
#include "assets/actor_151000_animation_098B0_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation098B0Bank4[48] = {
#include "assets/actor_151000_animation_098B0_bank4.inc"
};

static AnimationRecord _gActor151000Animation098B0Records[90] = {
#include "assets/actor_151000_animation_098B0_records.inc"
};

static u16 _gActor151000Animation098B0Indices[20] = {
#include "assets/actor_151000_animation_098B0_indices.inc"
};

static AnimationSet _gActor151000Animation098B0 = {
    _gActor151000Animation098B0Records,
    _gActor151000Animation098B0Indices,
    { NULL, _gActor151000Animation098B0Bank1, NULL, NULL, _gActor151000Animation098B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09C98Bank1[5] = {
#include "assets/actor_151000_animation_09C98_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09C98Bank4[59] = {
#include "assets/actor_151000_animation_09C98_bank4.inc"
};

static AnimationRecord _gActor151000Animation09C98Records[156] = {
#include "assets/actor_151000_animation_09C98_records.inc"
};

static u16 _gActor151000Animation09C98Indices[20] = {
#include "assets/actor_151000_animation_09C98_indices.inc"
};

static AnimationSet _gActor151000Animation09C98 = {
    _gActor151000Animation09C98Records,
    _gActor151000Animation09C98Indices,
    { NULL, _gActor151000Animation09C98Bank1, NULL, NULL, _gActor151000Animation09C98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09F48Bank1[3] = {
#include "assets/actor_151000_animation_09F48_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09F48Bank4[29] = {
#include "assets/actor_151000_animation_09F48_bank4.inc"
};

static AnimationRecord _gActor151000Animation09F48Records[114] = {
#include "assets/actor_151000_animation_09F48_records.inc"
};

static u16 _gActor151000Animation09F48Indices[20] = {
#include "assets/actor_151000_animation_09F48_indices.inc"
};

static AnimationSet _gActor151000Animation09F48 = {
    _gActor151000Animation09F48Records,
    _gActor151000Animation09F48Indices,
    { NULL, _gActor151000Animation09F48Bank1, NULL, NULL, _gActor151000Animation09F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0A104Bank1[2] = {
#include "assets/actor_151000_animation_0A104_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0A104Bank4[20] = {
#include "assets/actor_151000_animation_0A104_bank4.inc"
};

static AnimationRecord _gActor151000Animation0A104Records[65] = {
#include "assets/actor_151000_animation_0A104_records.inc"
};

static u16 _gActor151000Animation0A104Indices[20] = {
#include "assets/actor_151000_animation_0A104_indices.inc"
};

static AnimationSet _gActor151000Animation0A104 = {
    _gActor151000Animation0A104Records,
    _gActor151000Animation0A104Indices,
    { NULL, _gActor151000Animation0A104Bank1, NULL, NULL, _gActor151000Animation0A104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0A4ECBank1[3] = {
#include "assets/actor_151000_animation_0A4EC_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0A4ECBank4[65] = {
#include "assets/actor_151000_animation_0A4EC_bank4.inc"
};

static AnimationRecord _gActor151000Animation0A4ECRecords[156] = {
#include "assets/actor_151000_animation_0A4EC_records.inc"
};

static u16 _gActor151000Animation0A4ECIndices[20] = {
#include "assets/actor_151000_animation_0A4EC_indices.inc"
};

static AnimationSet _gActor151000Animation0A4EC = {
    _gActor151000Animation0A4ECRecords,
    _gActor151000Animation0A4ECIndices,
    { NULL, _gActor151000Animation0A4ECBank1, NULL, NULL, _gActor151000Animation0A4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0AE9CBank1[18] = {
#include "assets/actor_151000_animation_0AE9C_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0AE9CBank4[232] = {
#include "assets/actor_151000_animation_0AE9C_bank4.inc"
};

static AnimationRecord _gActor151000Animation0AE9CRecords[314] = {
#include "assets/actor_151000_animation_0AE9C_records.inc"
};

static u16 _gActor151000Animation0AE9CIndices[20] = {
#include "assets/actor_151000_animation_0AE9C_indices.inc"
};

static AnimationSet _gActor151000Animation0AE9C = {
    _gActor151000Animation0AE9CRecords,
    _gActor151000Animation0AE9CIndices,
    { NULL, _gActor151000Animation0AE9CBank1, NULL, NULL, _gActor151000Animation0AE9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0B218Bank1[4] = {
#include "assets/actor_151000_animation_0B218_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0B218Bank4[68] = {
#include "assets/actor_151000_animation_0B218_bank4.inc"
};

static AnimationRecord _gActor151000Animation0B218Records[123] = {
#include "assets/actor_151000_animation_0B218_records.inc"
};

static u16 _gActor151000Animation0B218Indices[20] = {
#include "assets/actor_151000_animation_0B218_indices.inc"
};

static AnimationSet _gActor151000Animation0B218 = {
    _gActor151000Animation0B218Records,
    _gActor151000Animation0B218Indices,
    { NULL, _gActor151000Animation0B218Bank1, NULL, NULL, _gActor151000Animation0B218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0B464Bank1[2] = {
#include "assets/actor_151000_animation_0B464_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0B464Bank4[27] = {
#include "assets/actor_151000_animation_0B464_bank4.inc"
};

static AnimationRecord _gActor151000Animation0B464Records[94] = {
#include "assets/actor_151000_animation_0B464_records.inc"
};

static u16 _gActor151000Animation0B464Indices[20] = {
#include "assets/actor_151000_animation_0B464_indices.inc"
};

static AnimationSet _gActor151000Animation0B464 = {
    _gActor151000Animation0B464Records,
    _gActor151000Animation0B464Indices,
    { NULL, _gActor151000Animation0B464Bank1, NULL, NULL, _gActor151000Animation0B464Bank4, NULL, NULL, NULL },
};

s16 gFootstepWalkBlendFrames = 8;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, footstepWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_151000_801327C8 },
    { ACTOR_MESSAGE_PLACE, footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_151000_8013288C },
    { ACTOR_MESSAGE_WALK_TO, footstepWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_151000_8013D2E0 = { { { TASK_BODY_TMD, 192 } }, func_actor_151000_801323F4, { .model = &_gActor151000AyaBreaBody } };

u8 gFootstepWalkAnims[140] = {
    0,
    0,
    0,
    0,
    196,
    166,
    19,
    128,
    204,
    170,
    19,
    128,
    216,
    174,
    19,
    128,
    232,
    177,
    19,
    128,
    64,
    180,
    19,
    128,
    208,
    182,
    19,
    128,
    184,
    186,
    19,
    128,
    104,
    189,
    19,
    128,
    0,
    0,
    0,
    0,
    36,
    191,
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
    128,
    148,
    19,
    128,
    120,
    152,
    19,
    128,
    92,
    164,
    19,
    128,
    12,
    195,
    19,
    128,
    188,
    204,
    19,
    128,
    132,
    210,
    19,
    128,
    56,
    208,
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

s32 D_actor_151000_8013D378;

FootstepWalkWork* gFootstepWalkWork;

Task* gFootstepWalkTask;

s16 gFootstepWalkMode;

/// The fade task: while the countdown `D_actor_151000_8013D378` is non-zero,
/// draws a full-screen black `TILE` into ordering table slot 0xA; once it is
/// zero the task kills itself.
void func_actor_151000_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_151000_8013D378 != 0) {
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
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_151000_80131EE0(s32 frames)
{
    D_actor_151000_8013D378 = frames;
    if (frames != 0) {
        taskSpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// The enemy's task body: publishes the task's work block in
/// `gFootstepWalkWork`, then runs the handler for the task's state from a
/// table built on the stack - the spawn handler `footstepWalkSpawn`,
/// then the per-frame `func_actor_151000_80132450`.
void func_actor_151000_801323F4(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        footstepWalkSpawn,
        func_actor_151000_80132450,
    };

    gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_151000_80132450
#define walkerUpdate     footstepWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the spawn handler installs on the enemy's task: tears down
/// the enemy the task was spawned for.
void footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_play_steps.inc.c"

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_reset_anim.inc.c"

#include "../../shared/footstep_walk_blend_anim.inc.c"

#include "../../shared/footstep_walk_play.inc.c"

/// Visibility opcode: applies `arg2` to the model of the task published in
/// `gFootstepWalkTask` - bit 0 shows it (flags 0) rather than hiding it
/// (0x80), and bit 1 ORs in 0x4.
s32 func_actor_151000_801327C8(Task* task, s32 arg1, s32 arg2, s32 arg3)
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

/// Message handler: message 0 arms the turn countdown `turnFrames` at 0x14
/// frames, message 1 sets `playFootsteps`, which turns the footsteps on. Anything else does nothing.
s32 func_actor_151000_8013288C(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
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
