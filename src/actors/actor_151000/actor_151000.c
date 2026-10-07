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
#define FOOTSTEP_WALK_WORK_T FootstepWalkWork
#include "../../shared/footstep_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

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

/// The scripts switch `_actor151000BlackoutTask` on and off through
/// `_actor151000SetBlackout`.
extern s32 D_actor_151000_8013D378;

static FootstepWalkWork* _gFootstepWalkWork;

/// The enemy's task, published by its spawn handler so the visibility opcode
/// can reach its model.
extern Task* gFootstepWalkTask;

static s16 _gFootstepWalkMode;

/// Descriptor of the blackout task `_actor151000BlackoutTask`.
extern TaskDesc D_actor_151000_80133360;

/// The enemy's message table and the animation data its work block's slots
/// are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gFootstepWalkMsgTable[];
extern u8               gFootstepWalkAnims[];

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);

static TmdSource _gActor151000AyaBreaBody;
void             func_actor_151000_801323F4(Task*);

static s32 _actor151000SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument);
static s32 _actor151000ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);

extern AnimationPlayRequest D_actor_151000_801333F0;
extern AnimationPlayRequest D_actor_151000_80133404;
static void                 _actor151000SetBlackout(s32 enabled);

static void _actor151000BlackoutTask(Task* task);

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

TaskDesc D_actor_151000_80133360 = { { { TASK_BODY_NONE, 192 } }, _actor151000BlackoutTask, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor151000SetWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor151000ApplyWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
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

/// Queues the scene's opaque black cover in the current frame's packet arena.
///
/// Requires space for one aligned `TILE` and a live ordering table with tag 10.
/// Coordinates and extent are pixels relative to the draw origin; the packet
/// belongs to the frame until GPU drawing completes.
static __inline__ void _actor151000DrawBlackoutTile(void)
{
    enum {
        ACTOR_151000_BLACKOUT_WIDTH_PIXELS  = 320,
        ACTOR_151000_BLACKOUT_HEIGHT_PIXELS = 256,
        ACTOR_151000_BLACKOUT_OT_TAG        = 10
    };

    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -ACTOR_151000_BLACKOUT_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_151000_BLACKOUT_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_151000_BLACKOUT_WIDTH_PIXELS;
    tile->h  = ACTOR_151000_BLACKOUT_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + ACTOR_151000_BLACKOUT_OT_TAG, tile);
}

/// Covers the scene in opaque black until its script clears the blackout switch.
///
/// Each enabled update queues one 320-by-256-pixel tile; there is no colour ramp
/// or countdown. The first disabled update kills this live task without drawing.
static void _actor151000BlackoutTask(Task* task)
{
    if (D_actor_151000_8013D378 != 0) {
        _actor151000DrawBlackoutTile();
    } else {
        taskKill(task);
    }
}

/// Switches the scene's opaque black cover on or off from an event script.
///
/// Zero disables drawing and lets existing cover tasks exit on their next update.
/// Every nonzero call stores the complete argument word and spawns another cover
/// task; callers normally enable once, then clear it on completion or skip.
/// The package and task descriptor must remain loaded while cover tasks are live.
static void _actor151000SetBlackout(s32 enabled)
{
    D_actor_151000_8013D378 = enabled;
    if (enabled != 0) {
        taskSpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// The enemy's task body: publishes the task's work block in
/// `_gFootstepWalkWork`, then runs the handler for the task's state from a
/// table built on the stack - the spawn handler `_footstepWalkSpawn`,
/// then the per-frame `_actorRenderWalkerFrame`.
void func_actor_151000_801323F4(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        _footstepWalkSpawn,
        _actorRenderWalkerFrame,
    };

    _gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
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
static s32 _actor151000SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument)
{
    enum {
        ACTOR_151000_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_151000_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1
    };

    TmdObject* model;

    model = gFootstepWalkTask->extra.tmd;
    if (flags & ACTOR_151000_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_151000_WALKER_DRAW_SKIP_AUTO_BUFFER) {
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
static s32 _actor151000ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        ACTOR_151000_WALKER_COMMAND_TURN             = 0,
        ACTOR_151000_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
        ACTOR_151000_WALKER_TURN_UPDATES             = 20
    };

    s32 commandId;

    commandId = command->command;
    switch (commandId) {
        case ACTOR_151000_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_151000_WALKER_TURN_UPDATES;
            break;
        case ACTOR_151000_WALKER_COMMAND_ENABLE_FOOTSTEPS:
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
