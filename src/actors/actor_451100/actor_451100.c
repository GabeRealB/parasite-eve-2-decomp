#include <psyq/sys/types.h>
#include <psyq/libgte.h>

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
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#define FOOTSTEP_WALK_WORK_T FootstepWalkQuietWork
#include "../../shared/footstep_walk.h"
#include "../../shared/pair_walk.h"

static void _actor451100QuietWalkSpawn(Enemy* enemy, Task* task);
static void _actor451100PairWalkSpawn(Enemy* enemy, Task* task);
static void _footstepWalkQuietUpdate(Task* task);
static void _footstepWalkQuietResetAnim(void);
static void _footstepWalkQuietBlendAnim(void);
static void _footstepWalkTickAnim(void);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

/// The clips the package's scene adds to the player's animation bank, with the
/// records stored after them.
///
/// The scene script sends `data.copy` to the player before it plays any of the
/// clips. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the
/// start of the storage, which is more than the clip table holds: the four set
/// pointers occupy extended ids 47-50, and the copy request itself, all 25
/// words of the play requests and the opcode of the subroutine's first command
/// are written into the bank after them. The play requests select ids 47-50
/// only, so none of those words is played as a clip.
///
/// The subroutine is not animation-bank data and plays nothing on the player.
/// It is part of this object only because the copied span reaches its first
/// word.
typedef union {
    struct {
        AnimationSet*            sets[4];                     // Player clips for extended ids 47-50
        AnimationBankCopyRequest copy;                        // Installs the first `ANIMATION_BANK_EXTENSION_CAPACITY` words of this storage
        AnimationPlayRequest     playRequests[5];             // Requests for extended ids 47, 48, 48, 49 and 50; the first for id 48 blends in and the second resets
        EvsCommand               sceneChildClipSubroutine[6]; // Script subroutine the scene script calls six times: starts a clip on scene children 1 and 0, waits 31 frames, starts each one's next clip and returns
    } data;                                                   // The records by name
    s32 words[67];                                            // The same storage as the copy reads it; the last 35 words lie beyond the copied span
} _Actor451100AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor451100AnimationBankExtensionStorage, 268);

extern _Actor451100AnimationBankExtensionStorage D_actor_451100_8013510C;

static FootstepWalkQuietWork* _gFootstepWalkWork;

/// That same actor's task, stored by its spawn handler for the handlers that
/// need the task but are not given it.
extern Task* D_actor_451100_8014E748;

static s16 _gFootstepWalkMode;

extern AnimationSet* D_actor_451100_8013F740[37];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_451100_8013F704[];
extern TaskDesc         D_actor_451100_8014E6E4[];
extern TaskMessageEntry D_actor_451100_8014E6B4[];
extern u8               D_actor_451100_8014E6FC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor451100QuietWalkExit(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actor451100PairWalkExit(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gActor451100No9GolemDryfieldBody;
static TmdSource _gActor451100Model1436C;
static void      _actor451100PairWalkTask(Task* task);
static void      _actor451100CarriedModelTask(Task* task);

static s32 _actor451100PairWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor451100PairWalkIgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static s32 _actor451100PairWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 unusedArgument);

static s32  _actor451100QuietWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor451100QuietWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor451100QuietWalkApplyCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void _actor451100QuietWalkTask(Task* task);

extern AnimationPlayRequest D_actor_451100_80134D98;
extern AnimationPlayRequest D_actor_451100_80134DAC;
extern AnimationPlayRequest D_actor_451100_80134DC0;
extern AnimationPlayRequest D_actor_451100_80134DD4;
extern AnimationPlayRequest D_actor_451100_80134DE8;
extern AnimationPlayRequest D_actor_451100_80134DFC;
extern AnimationPlayRequest D_actor_451100_80134E10;
extern AnimationPlayRequest D_actor_451100_80134E24;
extern AnimationPlayRequest D_actor_451100_80134E38;
extern AnimationPlayRequest D_actor_451100_80134E4C;
extern AnimationPlayRequest D_actor_451100_80134E60;
extern AnimationPlayRequest D_actor_451100_80134E74;
extern AnimationPlayRequest D_actor_451100_80134E88;
extern AnimationPlayRequest D_actor_451100_80134E9C;
extern AnimationPlayRequest D_actor_451100_80134EB0;
extern AnimationPlayRequest D_actor_451100_80134EC4;
extern AnimationPlayRequest D_actor_451100_80134ED8;
extern AnimationPlayRequest D_actor_451100_80134EEC;
extern AnimationPlayRequest D_actor_451100_80134F00;
extern AnimationPlayRequest D_actor_451100_80134F14;
extern AnimationPlayRequest D_actor_451100_80134F28;
extern AnimationPlayRequest D_actor_451100_80134F3C;
extern AnimationPlayRequest D_actor_451100_80134F50;
extern AnimationPlayRequest D_actor_451100_80134F64;
extern AnimationPlayRequest D_actor_451100_80134F78;
extern AnimationPlayRequest D_actor_451100_80134F8C;
extern AnimationPlayRequest D_actor_451100_80134FA0;
extern AnimationPlayRequest D_actor_451100_80134FB4;
extern AnimationPlayRequest D_actor_451100_80134FC8;
extern AnimationPlayRequest D_actor_451100_80134FDC;
extern AnimationPlayRequest D_actor_451100_80134FF0;
extern AnimationPlayRequest D_actor_451100_80135004;
extern AnimationPlayRequest D_actor_451100_80135018;
extern AnimationPlayRequest D_actor_451100_8013502C;
extern AnimationPlayRequest D_actor_451100_801350D0;
extern AnimationPlayRequest D_actor_451100_801350E4;
extern AnimationPlayRequest D_actor_451100_801350F8;
extern ActorTransform       D_actor_451100_80135040;
extern ActorTransform       D_actor_451100_80135058;
extern ActorTransform       D_actor_451100_80135070;
extern ActorTransform       D_actor_451100_80135088;
extern ActorTransform       D_actor_451100_801350A0;
extern ActorTransform       D_actor_451100_801350B8;

static AnimationPackedPose _gActor451100Animation015DCBank1[6] = {
#include "assets/actor_451100_animation_015DC_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation015DCBank4[46] = {
#include "assets/actor_451100_animation_015DC_bank4.inc"
};

static AnimationRecord _gActor451100Animation015DCRecords[109] = {
#include "assets/actor_451100_animation_015DC_records.inc"
};

static u16 _gActor451100Animation015DCIndices[20] = {
#include "assets/actor_451100_animation_015DC_indices.inc"
};

static AnimationSet _gActor451100Animation015DC = {
    _gActor451100Animation015DCRecords,
    _gActor451100Animation015DCIndices,
    { NULL, _gActor451100Animation015DCBank1, NULL, NULL, _gActor451100Animation015DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation01CD8Bank1[10] = {
#include "assets/actor_451100_animation_01CD8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation01CD8Bank4[147] = {
#include "assets/actor_451100_animation_01CD8_bank4.inc"
};

static AnimationRecord _gActor451100Animation01CD8Records[250] = {
#include "assets/actor_451100_animation_01CD8_records.inc"
};

static u16 _gActor451100Animation01CD8Indices[20] = {
#include "assets/actor_451100_animation_01CD8_indices.inc"
};

static AnimationSet _gActor451100Animation01CD8 = {
    _gActor451100Animation01CD8Records,
    _gActor451100Animation01CD8Indices,
    { NULL, _gActor451100Animation01CD8Bank1, NULL, NULL, _gActor451100Animation01CD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation01F50Bank1[4] = {
#include "assets/actor_451100_animation_01F50_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation01F50Bank4[51] = {
#include "assets/actor_451100_animation_01F50_bank4.inc"
};

static AnimationRecord _gActor451100Animation01F50Records[75] = {
#include "assets/actor_451100_animation_01F50_records.inc"
};

static u16 _gActor451100Animation01F50Indices[20] = {
#include "assets/actor_451100_animation_01F50_indices.inc"
};

static AnimationSet _gActor451100Animation01F50 = {
    _gActor451100Animation01F50Records,
    _gActor451100Animation01F50Indices,
    { NULL, _gActor451100Animation01F50Bank1, NULL, NULL, _gActor451100Animation01F50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation02F50Bank1[44] = {
#include "assets/actor_451100_animation_02F50_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation02F50Bank4[370] = {
#include "assets/actor_451100_animation_02F50_bank4.inc"
};

static AnimationRecord _gActor451100Animation02F50Records[502] = {
#include "assets/actor_451100_animation_02F50_records.inc"
};

static u16 _gActor451100Animation02F50Indices[20] = {
#include "assets/actor_451100_animation_02F50_indices.inc"
};

static AnimationSet _gActor451100Animation02F50 = {
    _gActor451100Animation02F50Records,
    _gActor451100Animation02F50Indices,
    { NULL, _gActor451100Animation02F50Bank1, NULL, NULL, _gActor451100Animation02F50Bank4, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_451100_80134D98 = { { .index = 0 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DAC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DC0 = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DD4 = { { .index = 0 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DE8 = { { .index = 0 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DFC = { { .index = 0 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E10 = { { .index = 0 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E24 = { { .index = 0 }, 25, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E38 = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E4C = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E60 = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E74 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E88 = { { .index = 0 }, 30, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E9C = { { .index = 0 }, 31, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EB0 = { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EC4 = { { .index = 0 }, 33, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134ED8 = { { .index = 0 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EEC = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F00 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F14 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F28 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F3C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F50 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F64 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F78 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F8C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FA0 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FB4 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FC8 = { { .index = 0 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FDC = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FF0 = { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80135004 = { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80135018 = { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_8013502C = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_451100_80135040 = { { -1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135058 = { { -1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135070 = { { 1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135088 = { { 1000, 3000, 200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_451100_801350A0 = { { 1500, 3000, 0, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_451100_801350B8 = { { 1000, 3000, 200, 0 }, { 0, 1024, 0, 0 } };

AnimationPlayRequest D_actor_451100_801350D0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_451100_801350E4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_451100_801350F8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

_Actor451100AnimationBankExtensionStorage D_actor_451100_8013510C = {
    .data = {
        { &_gActor451100Animation02F50, &_gActor451100Animation015DC, &_gActor451100Animation01CD8, &_gActor451100Animation01F50 },
        { { .words = D_actor_451100_8013510C.words }, ANIMATION_BANK_EXTENSION_CAPACITY },
        {
            { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE },
            { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE },
            { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
            { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE },
            { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE },
        },
        {
            { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_451100_80134FC8 } }, { .value = 0 } },
            { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_451100_80134E88 } }, { .value = 0 } },
            { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
            { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_451100_80134E9C } }, { .value = 0 } },
            { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_451100_80134FDC } }, { .value = 0 } },
            { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
        },
    },
};

EvsSceneKey D_actor_451100_80135218 = { 5, 11, 11 };

EvsCommand D_actor_451100_80135220[146] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_451100_80135218 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_451100_8013510C.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_451100_80135040 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_451100_80135070 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_451100_801350A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134D98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134FA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134FB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_451100_8013510C.data.sceneChildClipSubroutine }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134FF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134FDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DFC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_8013502C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134EC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80135004 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_451100_80135058 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_451100_80135088 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_451100_801350B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80135018 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134E4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_451100_80134F8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_451100_80135040 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_451100_80135FD0[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_451100_80135040 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_451100_80136108[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x550C0004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor451100AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor451100AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor451100AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor451100AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor451100AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor451100AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor451100AyaBreaBodyPartVerts,
    _gActor451100AyaBreaBodyVerts,
    _gActor451100AyaBreaBodyNormals,
    _gActor451100AyaBreaBodySkeleton,
    _gActor451100AyaBreaBodyStream,
};

static AnimationPackedPose _gActor451100Animation09E1CBank1[17] = {
#include "assets/actor_451100_animation_09E1C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation09E1CBank4[16] = {
#include "assets/actor_451100_animation_09E1C_bank4.inc"
};

static AnimationRecord _gActor451100Animation09E1CRecords[96] = {
#include "assets/actor_451100_animation_09E1C_records.inc"
};

static u16 _gActor451100Animation09E1CIndices[20] = {
#include "assets/actor_451100_animation_09E1C_indices.inc"
};

static AnimationSet _gActor451100Animation09E1C = {
    _gActor451100Animation09E1CRecords,
    _gActor451100Animation09E1CIndices,
    { NULL, _gActor451100Animation09E1CBank1, NULL, NULL, _gActor451100Animation09E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0A428Bank1[66] = {
#include "assets/actor_451100_animation_0A428_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0A428Bank4[16] = {
#include "assets/actor_451100_animation_0A428_bank4.inc"
};

static AnimationRecord _gActor451100Animation0A428Records[153] = {
#include "assets/actor_451100_animation_0A428_records.inc"
};

static u16 _gActor451100Animation0A428Indices[20] = {
#include "assets/actor_451100_animation_0A428_indices.inc"
};

static AnimationSet _gActor451100Animation0A428 = {
    _gActor451100Animation0A428Records,
    _gActor451100Animation0A428Indices,
    { NULL, _gActor451100Animation0A428Bank1, NULL, NULL, _gActor451100Animation0A428Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0A780Bank1[31] = {
#include "assets/actor_451100_animation_0A780_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0A780Bank4[16] = {
#include "assets/actor_451100_animation_0A780_bank4.inc"
};

static AnimationRecord _gActor451100Animation0A780Records[85] = {
#include "assets/actor_451100_animation_0A780_records.inc"
};

static u16 _gActor451100Animation0A780Indices[20] = {
#include "assets/actor_451100_animation_0A780_indices.inc"
};

static AnimationSet _gActor451100Animation0A780 = {
    _gActor451100Animation0A780Records,
    _gActor451100Animation0A780Indices,
    { NULL, _gActor451100Animation0A780Bank1, NULL, NULL, _gActor451100Animation0A780Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0AA68Bank1[24] = {
#include "assets/actor_451100_animation_0AA68_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0AA68Bank4[16] = {
#include "assets/actor_451100_animation_0AA68_bank4.inc"
};

static AnimationRecord _gActor451100Animation0AA68Records[78] = {
#include "assets/actor_451100_animation_0AA68_records.inc"
};

static u16 _gActor451100Animation0AA68Indices[20] = {
#include "assets/actor_451100_animation_0AA68_indices.inc"
};

static AnimationSet _gActor451100Animation0AA68 = {
    _gActor451100Animation0AA68Records,
    _gActor451100Animation0AA68Indices,
    { NULL, _gActor451100Animation0AA68Bank1, NULL, NULL, _gActor451100Animation0AA68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0AE40Bank1[37] = {
#include "assets/actor_451100_animation_0AE40_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0AE40Bank4[16] = {
#include "assets/actor_451100_animation_0AE40_bank4.inc"
};

static AnimationRecord _gActor451100Animation0AE40Records[99] = {
#include "assets/actor_451100_animation_0AE40_records.inc"
};

static u16 _gActor451100Animation0AE40Indices[20] = {
#include "assets/actor_451100_animation_0AE40_indices.inc"
};

static AnimationSet _gActor451100Animation0AE40 = {
    _gActor451100Animation0AE40Records,
    _gActor451100Animation0AE40Indices,
    { NULL, _gActor451100Animation0AE40Bank1, NULL, NULL, _gActor451100Animation0AE40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0B1A8Bank1[32] = {
#include "assets/actor_451100_animation_0B1A8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0B1A8Bank4[16] = {
#include "assets/actor_451100_animation_0B1A8_bank4.inc"
};

static AnimationRecord _gActor451100Animation0B1A8Records[86] = {
#include "assets/actor_451100_animation_0B1A8_records.inc"
};

static u16 _gActor451100Animation0B1A8Indices[20] = {
#include "assets/actor_451100_animation_0B1A8_indices.inc"
};

static AnimationSet _gActor451100Animation0B1A8 = {
    _gActor451100Animation0B1A8Records,
    _gActor451100Animation0B1A8Indices,
    { NULL, _gActor451100Animation0B1A8Bank1, NULL, NULL, _gActor451100Animation0B1A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0B558Bank1[31] = {
#include "assets/actor_451100_animation_0B558_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0B558Bank4[17] = {
#include "assets/actor_451100_animation_0B558_bank4.inc"
};

static AnimationRecord _gActor451100Animation0B558Records[106] = {
#include "assets/actor_451100_animation_0B558_records.inc"
};

static u16 _gActor451100Animation0B558Indices[20] = {
#include "assets/actor_451100_animation_0B558_indices.inc"
};

static AnimationSet _gActor451100Animation0B558 = {
    _gActor451100Animation0B558Records,
    _gActor451100Animation0B558Indices,
    { NULL, _gActor451100Animation0B558Bank1, NULL, NULL, _gActor451100Animation0B558Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0BCD4Bank1[97] = {
#include "assets/actor_451100_animation_0BCD4_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0BCD4Bank4[17] = {
#include "assets/actor_451100_animation_0BCD4_bank4.inc"
};

static AnimationRecord _gActor451100Animation0BCD4Records[151] = {
#include "assets/actor_451100_animation_0BCD4_records.inc"
};

static u16 _gActor451100Animation0BCD4Indices[20] = {
#include "assets/actor_451100_animation_0BCD4_indices.inc"
};

static AnimationSet _gActor451100Animation0BCD4 = {
    _gActor451100Animation0BCD4Records,
    _gActor451100Animation0BCD4Indices,
    { NULL, _gActor451100Animation0BCD4Bank1, NULL, NULL, _gActor451100Animation0BCD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0C1E0Bank1[30] = {
#include "assets/actor_451100_animation_0C1E0_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0C1E0Bank4[69] = {
#include "assets/actor_451100_animation_0C1E0_bank4.inc"
};

static AnimationRecord _gActor451100Animation0C1E0Records[144] = {
#include "assets/actor_451100_animation_0C1E0_records.inc"
};

static u16 _gActor451100Animation0C1E0Indices[20] = {
#include "assets/actor_451100_animation_0C1E0_indices.inc"
};

static AnimationSet _gActor451100Animation0C1E0 = {
    _gActor451100Animation0C1E0Records,
    _gActor451100Animation0C1E0Indices,
    { NULL, _gActor451100Animation0C1E0Bank1, NULL, NULL, _gActor451100Animation0C1E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0C45CBank1[17] = {
#include "assets/actor_451100_animation_0C45C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0C45CBank4[16] = {
#include "assets/actor_451100_animation_0C45C_bank4.inc"
};

static AnimationRecord _gActor451100Animation0C45CRecords[72] = {
#include "assets/actor_451100_animation_0C45C_records.inc"
};

static u16 _gActor451100Animation0C45CIndices[20] = {
#include "assets/actor_451100_animation_0C45C_indices.inc"
};

static AnimationSet _gActor451100Animation0C45C = {
    _gActor451100Animation0C45CRecords,
    _gActor451100Animation0C45CIndices,
    { NULL, _gActor451100Animation0C45CBank1, NULL, NULL, _gActor451100Animation0C45CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0C600Bank1[3] = {
#include "assets/actor_451100_animation_0C600_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0C600Bank4[16] = {
#include "assets/actor_451100_animation_0C600_bank4.inc"
};

static AnimationRecord _gActor451100Animation0C600Records[60] = {
#include "assets/actor_451100_animation_0C600_records.inc"
};

static u16 _gActor451100Animation0C600Indices[20] = {
#include "assets/actor_451100_animation_0C600_indices.inc"
};

static AnimationSet _gActor451100Animation0C600 = {
    _gActor451100Animation0C600Records,
    _gActor451100Animation0C600Indices,
    { NULL, _gActor451100Animation0C600Bank1, NULL, NULL, _gActor451100Animation0C600Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0C7F0Bank1[7] = {
#include "assets/actor_451100_animation_0C7F0_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0C7F0Bank4[16] = {
#include "assets/actor_451100_animation_0C7F0_bank4.inc"
};

static AnimationRecord _gActor451100Animation0C7F0Records[67] = {
#include "assets/actor_451100_animation_0C7F0_records.inc"
};

static u16 _gActor451100Animation0C7F0Indices[20] = {
#include "assets/actor_451100_animation_0C7F0_indices.inc"
};

static AnimationSet _gActor451100Animation0C7F0 = {
    _gActor451100Animation0C7F0Records,
    _gActor451100Animation0C7F0Indices,
    { NULL, _gActor451100Animation0C7F0Bank1, NULL, NULL, _gActor451100Animation0C7F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0C9B8Bank1[5] = {
#include "assets/actor_451100_animation_0C9B8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0C9B8Bank4[16] = {
#include "assets/actor_451100_animation_0C9B8_bank4.inc"
};

static AnimationRecord _gActor451100Animation0C9B8Records[63] = {
#include "assets/actor_451100_animation_0C9B8_records.inc"
};

static u16 _gActor451100Animation0C9B8Indices[20] = {
#include "assets/actor_451100_animation_0C9B8_indices.inc"
};

static AnimationSet _gActor451100Animation0C9B8 = {
    _gActor451100Animation0C9B8Records,
    _gActor451100Animation0C9B8Indices,
    { NULL, _gActor451100Animation0C9B8Bank1, NULL, NULL, _gActor451100Animation0C9B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0D2B4Bank1[71] = {
#include "assets/actor_451100_animation_0D2B4_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0D2B4Bank4[100] = {
#include "assets/actor_451100_animation_0D2B4_bank4.inc"
};

static AnimationRecord _gActor451100Animation0D2B4Records[242] = {
#include "assets/actor_451100_animation_0D2B4_records.inc"
};

static u16 _gActor451100Animation0D2B4Indices[20] = {
#include "assets/actor_451100_animation_0D2B4_indices.inc"
};

static AnimationSet _gActor451100Animation0D2B4 = {
    _gActor451100Animation0D2B4Records,
    _gActor451100Animation0D2B4Indices,
    { NULL, _gActor451100Animation0D2B4Bank1, NULL, NULL, _gActor451100Animation0D2B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0D61CBank1[32] = {
#include "assets/actor_451100_animation_0D61C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0D61CBank4[16] = {
#include "assets/actor_451100_animation_0D61C_bank4.inc"
};

static AnimationRecord _gActor451100Animation0D61CRecords[86] = {
#include "assets/actor_451100_animation_0D61C_records.inc"
};

static u16 _gActor451100Animation0D61CIndices[20] = {
#include "assets/actor_451100_animation_0D61C_indices.inc"
};

static AnimationSet _gActor451100Animation0D61C = {
    _gActor451100Animation0D61CRecords,
    _gActor451100Animation0D61CIndices,
    { NULL, _gActor451100Animation0D61CBank1, NULL, NULL, _gActor451100Animation0D61CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation0D8B8Bank1[18] = {
#include "assets/actor_451100_animation_0D8B8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation0D8B8Bank4[16] = {
#include "assets/actor_451100_animation_0D8B8_bank4.inc"
};

static AnimationRecord _gActor451100Animation0D8B8Records[77] = {
#include "assets/actor_451100_animation_0D8B8_records.inc"
};

static u16 _gActor451100Animation0D8B8Indices[20] = {
#include "assets/actor_451100_animation_0D8B8_indices.inc"
};

static AnimationSet _gActor451100Animation0D8B8 = {
    _gActor451100Animation0D8B8Records,
    _gActor451100Animation0D8B8Indices,
    { NULL, _gActor451100Animation0D8B8Bank1, NULL, NULL, _gActor451100Animation0D8B8Bank4, NULL, NULL, NULL },
};

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_451100_8013F704[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor451100QuietWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor451100QuietWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor451100QuietWalkApplyCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_451100_8013F734 = { { { TASK_BODY_TMD, 192 } }, _actor451100QuietWalkTask, { .model = &_gActor451100AyaBreaBody } };

AnimationSet* D_actor_451100_8013F740[37] = {
    NULL,
    &_gActor451100Animation09E1C,
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
    NULL,
    NULL,
    NULL,
    &_gActor451100Animation09E1C,
    &_gActor451100Animation0A428,
    &_gActor451100Animation0A780,
    &_gActor451100Animation0AA68,
    &_gActor451100Animation0AE40,
    &_gActor451100Animation0B1A8,
    &_gActor451100Animation0B558,
    &_gActor451100Animation0BCD4,
    &_gActor451100Animation0C1E0,
    &_gActor451100Animation0C45C,
    &_gActor451100Animation0C600,
    &_gActor451100Animation0C7F0,
    &_gActor451100Animation0C9B8,
    &_gActor451100Animation0D2B4,
    &_gActor451100Animation0D61C,
    &_gActor451100Animation0D8B8,
    NULL,
};

static TmdBone _gActor451100No9GolemDryfieldBodySkeleton[19] = {
#include "assets/no9_golem_dryfield_body_skeleton.inc"
};

static u32 _gActor451100No9GolemDryfieldBodyPartVerts[19] = {
#include "assets/no9_golem_dryfield_body_partVerts.inc"
};

static SVECTOR _gActor451100No9GolemDryfieldBodyVerts[432] = {
#include "assets/no9_golem_dryfield_body_verts.inc"
};

static SVECTOR _gActor451100No9GolemDryfieldBodyNormals[444] = {
#include "assets/no9_golem_dryfield_body_normals.inc"
};

static u32 _gActor451100No9GolemDryfieldBodyStream[4749] = {
#include "assets/no9_golem_dryfield_body_stream.inc"
};

static TmdSource _gActor451100No9GolemDryfieldBody = {
    0,
    26564,
    6624,
    19,
    _gActor451100No9GolemDryfieldBodyPartVerts,
    _gActor451100No9GolemDryfieldBodyVerts,
    _gActor451100No9GolemDryfieldBodyNormals,
    _gActor451100No9GolemDryfieldBodySkeleton,
    _gActor451100No9GolemDryfieldBodyStream,
};

static TmdBone _gActor451100Model1436CSkeleton[1] = {
#include "assets/actor_451100_model_1436C_skeleton.inc"
};

static u32 _gActor451100Model1436CPartVerts[1] = {
#include "assets/actor_451100_model_1436C_partVerts.inc"
};

static SVECTOR _gActor451100Model1436CVerts[14] = {
#include "assets/actor_451100_model_1436C_verts.inc"
};

static SVECTOR _gActor451100Model1436CNormals[14] = {
#include "assets/actor_451100_model_1436C_normals.inc"
};

static u32 _gActor451100Model1436CStream[99] = {
#include "assets/actor_451100_model_1436C_stream.inc"
};

static TmdSource _gActor451100Model1436C = {
    0,
    636,
    0,
    1,
    _gActor451100Model1436CPartVerts,
    _gActor451100Model1436CVerts,
    _gActor451100Model1436CNormals,
    _gActor451100Model1436CSkeleton,
    _gActor451100Model1436CStream,
};

static AnimationPackedPose _gActor451100Animation14A28Bank1[14] = {
#include "assets/actor_451100_animation_14A28_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation14A28Bank4[81] = {
#include "assets/actor_451100_animation_14A28_bank4.inc"
};

static AnimationRecord _gActor451100Animation14A28Records[190] = {
#include "assets/actor_451100_animation_14A28_records.inc"
};

static u16 _gActor451100Animation14A28Indices[20] = {
#include "assets/actor_451100_animation_14A28_indices.inc"
};

static AnimationSet _gActor451100Animation14A28 = {
    _gActor451100Animation14A28Records,
    _gActor451100Animation14A28Indices,
    { NULL, _gActor451100Animation14A28Bank1, NULL, NULL, _gActor451100Animation14A28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation159B4Bank1[46] = {
#include "assets/actor_451100_animation_159B4_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation159B4Bank4[255] = {
#include "assets/actor_451100_animation_159B4_bank4.inc"
};

static AnimationRecord _gActor451100Animation159B4Records[582] = {
#include "assets/actor_451100_animation_159B4_records.inc"
};

static u16 _gActor451100Animation159B4Indices[20] = {
#include "assets/actor_451100_animation_159B4_indices.inc"
};

static AnimationSet _gActor451100Animation159B4 = {
    _gActor451100Animation159B4Records,
    _gActor451100Animation159B4Indices,
    { NULL, _gActor451100Animation159B4Bank1, NULL, NULL, _gActor451100Animation159B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation16110Bank1[23] = {
#include "assets/actor_451100_animation_16110_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation16110Bank4[150] = {
#include "assets/actor_451100_animation_16110_bank4.inc"
};

static AnimationRecord _gActor451100Animation16110Records[232] = {
#include "assets/actor_451100_animation_16110_records.inc"
};

static u16 _gActor451100Animation16110Indices[20] = {
#include "assets/actor_451100_animation_16110_indices.inc"
};

static AnimationSet _gActor451100Animation16110 = {
    _gActor451100Animation16110Records,
    _gActor451100Animation16110Indices,
    { NULL, _gActor451100Animation16110Bank1, NULL, NULL, _gActor451100Animation16110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation166E8Bank1[6] = {
#include "assets/actor_451100_animation_166E8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation166E8Bank4[148] = {
#include "assets/actor_451100_animation_166E8_bank4.inc"
};

static AnimationRecord _gActor451100Animation166E8Records[188] = {
#include "assets/actor_451100_animation_166E8_records.inc"
};

static u16 _gActor451100Animation166E8Indices[20] = {
#include "assets/actor_451100_animation_166E8_indices.inc"
};

static AnimationSet _gActor451100Animation166E8 = {
    _gActor451100Animation166E8Records,
    _gActor451100Animation166E8Indices,
    { NULL, _gActor451100Animation166E8Bank1, NULL, NULL, _gActor451100Animation166E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation16F24Bank1[7] = {
#include "assets/actor_451100_animation_16F24_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation16F24Bank4[205] = {
#include "assets/actor_451100_animation_16F24_bank4.inc"
};

static AnimationRecord _gActor451100Animation16F24Records[281] = {
#include "assets/actor_451100_animation_16F24_records.inc"
};

static u16 _gActor451100Animation16F24Indices[20] = {
#include "assets/actor_451100_animation_16F24_indices.inc"
};

static AnimationSet _gActor451100Animation16F24 = {
    _gActor451100Animation16F24Records,
    _gActor451100Animation16F24Indices,
    { NULL, _gActor451100Animation16F24Bank1, NULL, NULL, _gActor451100Animation16F24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation176C8Bank1[32] = {
#include "assets/actor_451100_animation_176C8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation176C8Bank4[149] = {
#include "assets/actor_451100_animation_176C8_bank4.inc"
};

static AnimationRecord _gActor451100Animation176C8Records[224] = {
#include "assets/actor_451100_animation_176C8_records.inc"
};

static u16 _gActor451100Animation176C8Indices[20] = {
#include "assets/actor_451100_animation_176C8_indices.inc"
};

static AnimationSet _gActor451100Animation176C8 = {
    _gActor451100Animation176C8Records,
    _gActor451100Animation176C8Indices,
    { NULL, _gActor451100Animation176C8Bank1, NULL, NULL, _gActor451100Animation176C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation17D50Bank1[13] = {
#include "assets/actor_451100_animation_17D50_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation17D50Bank4[109] = {
#include "assets/actor_451100_animation_17D50_bank4.inc"
};

static AnimationRecord _gActor451100Animation17D50Records[250] = {
#include "assets/actor_451100_animation_17D50_records.inc"
};

static u16 _gActor451100Animation17D50Indices[20] = {
#include "assets/actor_451100_animation_17D50_indices.inc"
};

static AnimationSet _gActor451100Animation17D50 = {
    _gActor451100Animation17D50Records,
    _gActor451100Animation17D50Indices,
    { NULL, _gActor451100Animation17D50Bank1, NULL, NULL, _gActor451100Animation17D50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1924CBank1[97] = {
#include "assets/actor_451100_animation_1924C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1924CBank4[380] = {
#include "assets/actor_451100_animation_1924C_bank4.inc"
};

static AnimationRecord _gActor451100Animation1924CRecords[652] = {
#include "assets/actor_451100_animation_1924C_records.inc"
};

static u16 _gActor451100Animation1924CIndices[20] = {
#include "assets/actor_451100_animation_1924C_indices.inc"
};

static AnimationSet _gActor451100Animation1924C = {
    _gActor451100Animation1924CRecords,
    _gActor451100Animation1924CIndices,
    { NULL, _gActor451100Animation1924CBank1, NULL, NULL, _gActor451100Animation1924CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation19C78Bank1[25] = {
#include "assets/actor_451100_animation_19C78_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation19C78Bank4[231] = {
#include "assets/actor_451100_animation_19C78_bank4.inc"
};

static AnimationRecord _gActor451100Animation19C78Records[325] = {
#include "assets/actor_451100_animation_19C78_records.inc"
};

static u16 _gActor451100Animation19C78Indices[20] = {
#include "assets/actor_451100_animation_19C78_indices.inc"
};

static AnimationSet _gActor451100Animation19C78 = {
    _gActor451100Animation19C78Records,
    _gActor451100Animation19C78Indices,
    { NULL, _gActor451100Animation19C78Bank1, NULL, NULL, _gActor451100Animation19C78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1A2CCBank1[17] = {
#include "assets/actor_451100_animation_1A2CC_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1A2CCBank4[139] = {
#include "assets/actor_451100_animation_1A2CC_bank4.inc"
};

static AnimationRecord _gActor451100Animation1A2CCRecords[195] = {
#include "assets/actor_451100_animation_1A2CC_records.inc"
};

static u16 _gActor451100Animation1A2CCIndices[20] = {
#include "assets/actor_451100_animation_1A2CC_indices.inc"
};

static AnimationSet _gActor451100Animation1A2CC = {
    _gActor451100Animation1A2CCRecords,
    _gActor451100Animation1A2CCIndices,
    { NULL, _gActor451100Animation1A2CCBank1, NULL, NULL, _gActor451100Animation1A2CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1A4F8Bank1[2] = {
#include "assets/actor_451100_animation_1A4F8_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1A4F8Bank4[29] = {
#include "assets/actor_451100_animation_1A4F8_bank4.inc"
};

static AnimationRecord _gActor451100Animation1A4F8Records[84] = {
#include "assets/actor_451100_animation_1A4F8_records.inc"
};

static u16 _gActor451100Animation1A4F8Indices[20] = {
#include "assets/actor_451100_animation_1A4F8_indices.inc"
};

static AnimationSet _gActor451100Animation1A4F8 = {
    _gActor451100Animation1A4F8Records,
    _gActor451100Animation1A4F8Indices,
    { NULL, _gActor451100Animation1A4F8Bank1, NULL, NULL, _gActor451100Animation1A4F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1A7D0Bank1[6] = {
#include "assets/actor_451100_animation_1A7D0_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1A7D0Bank4[33] = {
#include "assets/actor_451100_animation_1A7D0_bank4.inc"
};

static AnimationRecord _gActor451100Animation1A7D0Records[111] = {
#include "assets/actor_451100_animation_1A7D0_records.inc"
};

static u16 _gActor451100Animation1A7D0Indices[20] = {
#include "assets/actor_451100_animation_1A7D0_indices.inc"
};

static AnimationSet _gActor451100Animation1A7D0 = {
    _gActor451100Animation1A7D0Records,
    _gActor451100Animation1A7D0Indices,
    { NULL, _gActor451100Animation1A7D0Bank1, NULL, NULL, _gActor451100Animation1A7D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1AA90Bank1[2] = {
#include "assets/actor_451100_animation_1AA90_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1AA90Bank4[52] = {
#include "assets/actor_451100_animation_1AA90_bank4.inc"
};

static AnimationRecord _gActor451100Animation1AA90Records[98] = {
#include "assets/actor_451100_animation_1AA90_records.inc"
};

static u16 _gActor451100Animation1AA90Indices[20] = {
#include "assets/actor_451100_animation_1AA90_indices.inc"
};

static AnimationSet _gActor451100Animation1AA90 = {
    _gActor451100Animation1AA90Records,
    _gActor451100Animation1AA90Indices,
    { NULL, _gActor451100Animation1AA90Bank1, NULL, NULL, _gActor451100Animation1AA90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1B91CBank1[18] = {
#include "assets/actor_451100_animation_1B91C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1B91CBank4[332] = {
#include "assets/actor_451100_animation_1B91C_bank4.inc"
};

static AnimationRecord _gActor451100Animation1B91CRecords[525] = {
#include "assets/actor_451100_animation_1B91C_records.inc"
};

static u16 _gActor451100Animation1B91CIndices[20] = {
#include "assets/actor_451100_animation_1B91C_indices.inc"
};

static AnimationSet _gActor451100Animation1B91C = {
    _gActor451100Animation1B91CRecords,
    _gActor451100Animation1B91CIndices,
    { NULL, _gActor451100Animation1B91CBank1, NULL, NULL, _gActor451100Animation1B91CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1C1E4Bank1[32] = {
#include "assets/actor_451100_animation_1C1E4_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1C1E4Bank4[180] = {
#include "assets/actor_451100_animation_1C1E4_bank4.inc"
};

static AnimationRecord _gActor451100Animation1C1E4Records[266] = {
#include "assets/actor_451100_animation_1C1E4_records.inc"
};

static u16 _gActor451100Animation1C1E4Indices[20] = {
#include "assets/actor_451100_animation_1C1E4_indices.inc"
};

static AnimationSet _gActor451100Animation1C1E4 = {
    _gActor451100Animation1C1E4Records,
    _gActor451100Animation1C1E4Indices,
    { NULL, _gActor451100Animation1C1E4Bank1, NULL, NULL, _gActor451100Animation1C1E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor451100Animation1C86CBank1[14] = {
#include "assets/actor_451100_animation_1C86C_bank1.inc"
};

static AnimationPackedRotation _gActor451100Animation1C86CBank4[130] = {
#include "assets/actor_451100_animation_1C86C_bank4.inc"
};

static AnimationRecord _gActor451100Animation1C86CRecords[226] = {
#include "assets/actor_451100_animation_1C86C_records.inc"
};

static u16 _gActor451100Animation1C86CIndices[20] = {
#include "assets/actor_451100_animation_1C86C_indices.inc"
};

static AnimationSet _gActor451100Animation1C86C = {
    _gActor451100Animation1C86CRecords,
    _gActor451100Animation1C86CIndices,
    { NULL, _gActor451100Animation1C86CBank1, NULL, NULL, _gActor451100Animation1C86CBank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_451100_8014E6B4[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor451100PairWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor451100PairWalkIgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor451100PairWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_451100_8014E6E4[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor451100PairWalkTask, { .model = &_gActor451100No9GolemDryfieldBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor451100CarriedModelTask, { .model = &_gActor451100Model1436C } },
};

u8 D_actor_451100_8014E6FC[72] = {
    0,
    0,
    0,
    0,
    72,
    104,
    20,
    128,
    212,
    119,
    20,
    128,
    48,
    127,
    20,
    128,
    8,
    133,
    20,
    128,
    68,
    141,
    20,
    128,
    232,
    148,
    20,
    128,
    112,
    155,
    20,
    128,
    108,
    176,
    20,
    128,
    152,
    186,
    20,
    128,
    236,
    192,
    20,
    128,
    24,
    195,
    20,
    128,
    240,
    197,
    20,
    128,
    176,
    200,
    20,
    128,
    60,
    215,
    20,
    128,
    4,
    224,
    20,
    128,
    140,
    230,
    20,
    128,
    0,
    0,
    0,
    0,
};

/// Borrowed pointer to the quiet walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkQuietWork* _gFootstepWalkWork = NULL;

Task* D_actor_451100_8014E748;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Binds the quiet walker's borrowed nineteen-part rig and queues startup clip 20.
///
/// The published work, model, native clip table and loaded tracks 1 to 18 must
/// remain live through playback. Clears travel and turning; the caller applies
/// the reset before ticking any driven slot.
static inline void _actor451100QuietWalkPrepareAnimation(TmdObject* model)
{
    enum { ACTOR_451100_QUIET_WALK_STARTUP_CLIP = 20 };

    animationInitContext(&_gFootstepWalkWork->rig.anim, D_actor_451100_8013F740, model,
                         _gFootstepWalkWork->rig.poses, _gFootstepWalkWork->rig.slots);
    _gFootstepWalkWork->st.animId  = ACTOR_451100_QUIET_WALK_STARTUP_CLIP;
    _gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gFootstepWalkWork->st.travel  = 0;
    _gFootstepWalkWork->turnFrames = 0;
}

/// Initializes the package's quiet nineteen-part scene walker in task state 0.
///
/// Requires a live TMD task with its `Enemy` in `spawnArg2` and no work allocation.
/// The primary heap, view, lighting query and animation scratch/GTE state must be
/// ready; startup clip 20 and tracks 1 to 18 must be loaded. The task owns the
/// zeroed `FootstepWalkQuietWork`; its model borrows the matrices and rig storage.
/// Publishes the work and task for singleton handlers, resets the startup clip
/// without a pose tick, and advances to state 1 with travel and turning cleared.
/// Allocation failure destroys the enemy and begins task teardown immediately.
/// The exit callback releases the enemy and starts task/work/model teardown;
/// the published pointers must not be used afterwards.
static void _actor451100QuietWalkSpawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_451100_QUIET_WALK_OT_OFFSET             = 1,
           ACTOR_451100_QUIET_WALK_LIGHT_SAMPLE_Y_OFFSET = 800 };

    VECTOR3                lightingSample;
    FootstepWalkQuietWork* allocatedWork;
    TmdObject*             model;
    GfxCoord*              rootCoord;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    allocatedWork      = memCalloc(sizeof(*allocatedWork), false);
    _gFootstepWalkWork = allocatedWork;
    task->work         = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the block; the model borrows its lighting matrices.
    task->exitCallback               = _actor451100QuietWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_451100_QUIET_WALK_OT_OFFSET;
    model->lightMtx                  = &_gFootstepWalkWork->light;
    model->colorMtx                  = &_gFootstepWalkWork->color;
    // Preserve the existing cached position and its composition frame.
    lightingSample.vx       = rootCoord->workm.t[0];
    lightingSample.vy       = rootCoord->workm.t[1] - ACTOR_451100_QUIET_WALK_LIGHT_SAMPLE_Y_OFFSET;
    D_actor_451100_8014E748 = task;
    lightingSample.vz       = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, ARRAY_SIZE(model->lightMtx->m));
    // Reset driven slots now; the first frame writes their poses.
    _actor451100QuietWalkPrepareAnimation(model);
    task->msgTable = D_actor_451100_8013F704;
    _footstepWalkQuietUpdate(task);
    task->state += 1;
}

#include "../../shared/footstep_walk_quiet_update.inc.c"

/// Dispatches initialization and frames for the package's quiet scene walker.
///
/// Task state must be 0 (spawn) or 1 (lighting, movement/animation and shadow).
/// Requires a live nineteen-part TMD model and `Enemy` in `spawnArg2`. Refreshes the
/// published work before every state call; after spawning, the model, work,
/// message table and clips must stay live. Only one published walker can receive
/// singleton messages at a time. A failed spawn may tear the task down.
static void _actor451100QuietWalkTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor451100QuietWalkSpawn,
        _actorRenderWalkerFrame,
    };

    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(`Enemy`*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkQuietUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the quiet scene walker's enemy and begins task teardown.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2`. The task releases
/// its work and model through default teardown; the published work and task
/// pointers remain dangling and must not be used after this call.
static void _actor451100QuietWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_quiet_reset_anim.inc.c"

#include "../../shared/footstep_walk_quiet_blend_anim.inc.c"

/// Applies an animation request synchronously to the published quiet walker.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` with live published task/work and a
/// borrowed, word-aligned request. Returns -1 for IDs 37 and above without
/// changing playback; accepted IDs must still select loaded clips (1 or 20..35)
/// with tracks 1 to 18. Negative IDs and empty entries are not checked.
/// Nonzero blend captures initialized poses and narrows `blendFrames` to a signed
/// halfword in whole normal-rate frames (0..2047 avoids blend-time overflow).
/// Reset ignores that duration. Source-bank and collision options are ignored.
/// Returns 0 after applying the reset/blend; retains no request pointer.
/// The receiver, message ID and second payload are ignored.
static s32 _actor451100QuietWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_451100_QUIET_WALK_CLIP_LIMIT = ARRAY_SIZE(D_actor_451100_8013F740) };

    if (request->animationId < ACTOR_451100_QUIET_WALK_CLIP_LIMIT) {
        _gFootstepWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gFootstepWalkBlendFrames    = request->blendFrames;
        } else {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gFootstepWalkWork->st.field_6 = 0;
        _footstepWalkQuietUpdate(D_actor_451100_8014E748);
        return 0;
    }
    return -1;
}

/// Replaces the published quiet walker's model flags from visibility bits.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with a live published TMD task.
/// `ACTOR_MESSAGE_PAIR_SHOW` clears all flags; without it, only active-draw
/// exclusion remains. `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` adds automatic-buffer
/// exclusion. Other bits are ignored, and no buffer is allocated or released.
/// The receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor451100QuietWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* model;

    model = D_actor_451100_8014E748->extra.tmd;
    if (drawFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Schedules twenty turning updates for command 0 on the published quiet walker.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with live published work and a borrowed,
/// two-byte-aligned `ActorCommand`. Context tags are ignored. Command 0 replaces
/// the remaining turn count; other commands do nothing. Turning consumes one
/// count and adds 51/4096 turns only while the turn clip plays in tick state;
/// this request neither selects that clip nor updates the model immediately.
/// The receiver, message ID and second payload are ignored. Always returns 0.
static s32 _actor451100QuietWalkApplyCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum { ACTOR_451100_QUIET_WALK_COMMAND_TURN = 0,
           ACTOR_451100_QUIET_WALK_TURN_UPDATES = 20 };

    if (command->command == ACTOR_451100_QUIET_WALK_COMMAND_TURN) {
        _gFootstepWalkWork->turnFrames = ACTOR_451100_QUIET_WALK_TURN_UPDATES;
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

/// Binds a pair walker's borrowed rig and queues its idle clip for a reset.
///
/// Requires a live zeroed work block, nineteen-part model and loaded clip 1
/// with tracks 1 to 18. Model coordinates, clip data, slots and poses remain
/// borrowed through playback; the caller applies the reset before slot ticks.
static inline void _actor451100PairWalkPrepareAnimation(PairWalkWork* work, TmdObject* model)
{
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_451100_8014E6FC, model, work->rig.poses,
                         work->rig.slots);
    work->st.animId = PAIR_WALK_ANIM_IDLE;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
}

/// Initializes the package's paired scene walker and its carried model.
///
/// Requires a live nineteen-part TMD task with its `Enemy` in `spawnArg2` and no
/// work allocation. The primary heap, view, lighting query and animation
/// scratch/GTE state must be ready, with idle clip 1 and tracks 1 to 18 loaded.
/// The task owns its zeroed `PairWalkWork`. Descriptor 1 supplies a carried-model
/// task parented under this task; child spawning is assumed to succeed.
/// Both models borrow the parent's light/color matrices. Resets the idle clip
/// without a pose tick and advances to state 1. Work-allocation failure destroys
/// the enemy and begins task teardown. Default teardown exits the child before
/// releasing the parent work; model and clip storage must remain live meanwhile.
static void _actor451100PairWalkSpawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_451100_PAIR_WALK_OT_OFFSET             = 1,
           ACTOR_451100_PAIR_WALK_LIGHT_SAMPLE_Y_OFFSET = 800,
           ACTOR_451100_CARRIED_MODEL_DESCRIPTOR        = 1 };

    VECTOR3       lightingSample;
    PairWalkWork* work;
    GfxCoord*     rootCoord;
    TmdObject*    model;
    Enemy*        carriedEnemy;
    PairWalkWork* allocatedWork;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memCalloc(sizeof(*allocatedWork), false);
    work          = allocatedWork;
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the work and the carried model belongs to its teardown tree.
    task->exitCallback               = _actor451100PairWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_451100_PAIR_WALK_OT_OFFSET;
    work->enemy                      = enemy;
    carriedEnemy                     = enemySpawnFromTable(D_actor_451100_8014E6E4, ACTOR_451100_CARRIED_MODEL_DESCRIPTOR, 0, enemy);
    taskReparent(task, carriedEnemy->task);
    work->pairTask  = carriedEnemy->task;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    // Sample the cached root position before the first animation update.
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - ACTOR_451100_PAIR_WALK_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, ARRAY_SIZE(model->lightMtx->m));
    _actor451100PairWalkPrepareAnimation(work, model);
    task->msgTable = D_actor_451100_8014E6B4;
    _pairWalkUpdate(task);
    task->state += 1;
}

#include "../../shared/pair_walk_update.inc.c"

/// Dispatches initialization and frames for the package's paired scene walker.
///
/// Task state must be 0 (spawn) or 1 (lighting, movement/animation and shadow).
/// Requires a live nineteen-part TMD task and its `Enemy` in `spawnArg2`; after
/// spawning, `PairWalkWork` and the carried model must remain live. Frame updates
/// use this task's work directly. A failed work allocation may tear the task down.
static void _actor451100PairWalkTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor451100PairWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(`Enemy`*, Task*) function declared in the prologue.
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

/// Releases the paired scene walker's enemy and begins task teardown.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2`. Default teardown
/// exits the carried model before releasing the parent's work, then arranges
/// model release. Neither the task nor enemy may be accessed afterwards.
static void _actor451100PairWalkExit(Task* task)
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

/// Applies an animation request synchronously to the paired scene walker.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` on a live TMD task with `PairWalkWork` and
/// its rig bound to loaded tracks 1 to 18. Borrows the word-aligned request only
/// during dispatch. Returns -1 for IDs 18 and above without changing playback;
/// accepted IDs must select loaded clips 1..16. Negative IDs and empty entries
/// are not checked. Source-bank and collision options are ignored.
/// Nonzero blend captures initialized poses and narrows `blendFrames` to a signed
/// halfword in whole normal-rate frames (0..2047 avoids blend-time overflow).
/// Reset ignores that duration. Returns 0 after applying the reset/blend.
/// The message ID and second payload are ignored.
static s32 _actor451100PairWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_451100_PAIR_WALK_CLIP_LIMIT = 18 };
    PairWalkWork* work;

    work = task->work;
    if (request->animationId < ACTOR_451100_PAIR_WALK_CLIP_LIMIT) {
        work->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = request->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _pairWalkUpdate(task);
        return 0;
    }
    return -1;
}

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

/// Accepts paired-walker actor commands without taking any action.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` and returns 0. All arguments are ignored,
/// including the borrowed command pointer; no payload storage is read or retained.
static s32 _actor451100PairWalkIgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

/// Faces the paired walker toward a destination and records its travel ticks.
///
/// Handles ACTOR_MESSAGE_WALK_TO on a live TMD task with `PairWalkWork`. Borrows a
/// word-aligned target through dispatch, reading only X/Z in the root parent's
/// coordinate frame. Y is ignored. Replaces the rotation with a unit-scale yaw
/// and records the signed-halfword heading in 4096 units per turn.
/// Stores floor(horizontal distance / 17) in signed-halfword travel, without
/// clamping. Differences and their squared sum must fit signed 32 bits; the tick
/// count must fit 0..32767. Does not invalidate composition or select a walk
/// clip; a separate animation request starts movement. The target pointer is not
/// retained. The message ID and second payload are ignored. Returns 0.
static s32 _actor451100PairWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 unusedArgument)
{
    enum { ACTOR_451100_PAIR_WALK_DISTANCE_PER_TICK = 17 };
    GfxCoord*     rootCoord;
    PairWalkWork* work;
    s32           deltaX;
    s32           deltaZ;
    s16           yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    deltaX       = target->vx - rootCoord->coord.t[0];
    deltaZ       = target->vz - rootCoord->coord.t[2];
    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / ACTOR_451100_PAIR_WALK_DISTANCE_PER_TICK;
    return 0;
}

/// Attaches the paired walker's carried model to part 8 and keeps it following.
///
/// Requires a live TMD child task whose parent has a nineteen-part TMD model and
/// `PairWalkWork`; both outlive the child. State 0 borrows the parent's light/color
/// matrices, links the child's root to part 8 and enters state 1. Both states
/// invalidate the root composition each frame. The child owns no animation work
/// or lighting matrices; the parent's teardown tree controls its lifetime.
static void _actor451100CarriedModelTask(Task* task)
{
    enum { ACTOR_451100_CARRIED_MODEL_ATTACH = 0,
           ACTOR_451100_CARRIED_MODEL_FOLLOW = 1,
           ACTOR_451100_CARRIED_MODEL_PART   = 8 };

    char          unusedStack[0x10]; // Retains the otherwise unused 16-byte stack frame.
    Task*         parentTask      = task->parent;
    TmdObject*    model           = task->extra.tmd;
    GfxCoord*     rootCoord       = model->coords;
    GfxCoord*     attachmentCoord = &parentTask->extra.tmd->coords[ACTOR_451100_CARRIED_MODEL_PART];
    PairWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case ACTOR_451100_CARRIED_MODEL_ATTACH:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            model->lightMtx         = &parentWork->light;
            model->colorMtx         = &parentWork->color;
            rootCoord->parent       = attachmentCoord;
            task->state++;
            break;
        case ACTOR_451100_CARRIED_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
