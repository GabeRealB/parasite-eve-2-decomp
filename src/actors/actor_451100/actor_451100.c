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
#include "../../shared/footstep_walk.h"
#include "../../shared/walker.h"
#include "../../shared/pair_walk.h"

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

/// Work block of the actor `func_actor_451100_801322D4` dispatches, published
/// by its spawn handler so the actor's message handlers and animation helpers
/// reach it without the task.
extern FootstepWalkQuietWork* gFootstepWalkWork;

/// That same actor's task, stored by its spawn handler for the handlers that
/// need the task but are not given it.
extern Task* D_actor_451100_8014E748;

/// Reset argument the first actor forwards to every reseeded slot.
extern s16 gFootstepWalkBlendFrames;

/// Picks the distance `footstepWalkQuietUpdate` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 gFootstepWalkMode;

extern AnimationSet* D_actor_451100_8013F740[37];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_451100_8013F704[];
extern TaskDesc         D_actor_451100_8014E6E4[];
extern TaskMessageEntry D_actor_451100_8014E6B4[];
extern u8               D_actor_451100_8014E6FC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_451100_80132330(Enemy* enemy, Task* task);
static void func_actor_451100_801323B4(Task* task);
static void func_actor_451100_80132C28(Enemy* enemy, Task* task);
static void func_actor_451100_80132CAC(Task* task);
static void func_actor_451100_80132CD4(Task* task);

static TmdSource _gActor451100No9GolemDryfieldBody;
static TmdSource _gActor451100Model1436C;
void             func_actor_451100_80132BD4(Task*);
void             func_actor_451100_801330B0(Task*);

s32 func_actor_451100_80132E98(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_451100_80132FE0(Task*, s32, s32, s32);
s32 func_actor_451100_80132FE8(Task*, s32, VECTOR*, s32);

s32  func_actor_451100_80132538(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_451100_801325C8(Task*, s32, s32, s32);
s32  func_actor_451100_8013268C(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void func_actor_451100_801322D4(Task*);

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

s16 gFootstepWalkBlendFrames = 8;

TaskMessageEntry D_actor_451100_8013F704[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_451100_80132538 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_451100_801325C8 },
    { ACTOR_MESSAGE_PLACE, footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_451100_8013268C },
    { ACTOR_MESSAGE_WALK_TO, footstepWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_451100_8013F734 = { { { TASK_BODY_TMD, 192 } }, func_actor_451100_801322D4, { .model = &_gActor451100AyaBreaBody } };

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
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_451100_80132E98 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_451100_80132FE0 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_451100_80132FE8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_451100_8014E6E4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_451100_80132BD4, { .model = &_gActor451100No9GolemDryfieldBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_451100_801330B0, { .model = &_gActor451100Model1436C } },
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

FootstepWalkQuietWork* gFootstepWalkWork = NULL;

Task* D_actor_451100_8014E748;

s16 gFootstepWalkMode;

static void func_actor_451100_80131E24(Enemy* enemy, Task* task);
static void func_actor_451100_801328A8(Enemy* enemy, Task* task);

/// State 0 of the `func_actor_451100_801322D4` dispatcher: allocates the work
/// block, publishes it in `gFootstepWalkWork` and on the task's work
/// slot, points the model's light and color matrices and its animation context
/// at it, then runs the per-frame update once and advances the task to state 1.
///
/// Every access to the block after the null check goes through
/// `gFootstepWalkWork` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
static void func_actor_451100_80131E24(Enemy* enemy, Task* task)
{
    VECTOR                 vec;
    FootstepWalkQuietWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(sizeof(FootstepWalkQuietWork), 0);
    gFootstepWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_451100_801323B4;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->lightMtx                    = &gFootstepWalkWork->light;
    obj->colorMtx                    = &gFootstepWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_451100_8014E748          = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&gFootstepWalkWork->rig.anim, D_actor_451100_8013F740, obj,
                         gFootstepWalkWork->rig.poses, gFootstepWalkWork->rig.slots);
    gFootstepWalkWork->st.animId  = 0x14;
    gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    gFootstepWalkWork->st.travel  = 0;
    gFootstepWalkWork->turnFrames = 0;
    task->msgTable                = D_actor_451100_8013F704;
    footstepWalkQuietUpdate(task);
    task->state += 1;
}

#include "../../shared/footstep_walk_quiet_update.inc.c"

/// Task handler of the actor whose work block this overlay publishes: runs
/// the handler for the task's state from a two-entry table built on the stack
/// (0 spawns, 1 runs a frame), refreshing `gFootstepWalkWork` from the
/// task's work slot first so the handlers can reach the block without the
/// task.
void func_actor_451100_801322D4(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_451100_80131E24,
        func_actor_451100_80132330,
    };

    gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_451100_80132330
#define walkerUpdate     footstepWalkQuietUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the `func_actor_451100_801322D4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
static void func_actor_451100_801323B4(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_quiet_reset_anim.inc.c"

#include "../../shared/footstep_walk_quiet_blend_anim.inc.c"

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x25 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_451100_80132538(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    if (args->animationId < 0x25) {
        gFootstepWalkWork->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            gFootstepWalkBlendFrames    = args->blendFrames;
        } else {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        gFootstepWalkWork->st.field_6 = 0;
        footstepWalkQuietUpdate(D_actor_451100_8014E748);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of `D_actor_451100_8013F704`: bit 0 of `arg2` clears
/// `TmdObject::flags` on the model of the task in `D_actor_451100_8014E748`,
/// showing it, and its absence sets 0x80, hiding it; bit 1 additionally ORs in
/// 0x4.
s32 func_actor_451100_801325C8(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = D_actor_451100_8014E748->extra.tmd;
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

/// Message 0x7DB handler of `D_actor_451100_8013F704`: a zero payload
/// halfword sets the published block's `turnFrames` to 0x14, the count of frames
/// the step routine turns the model while clip 3 plays.
s32 func_actor_451100_8013268C(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    if (msg->command == 0) {
        gFootstepWalkWork->turnFrames = 0x14;
    }
    return 0;
}

#include "../../shared/footstep_walk_to.inc.c"

#include "../../shared/walker_shadow_shaded.inc.c"

/// State 0 of the `func_actor_451100_80132BD4` dispatcher: allocates the
/// actor's 0x4C0-byte `PairWalkWork` block and hangs it off the task, spawns
/// entry 1 of `D_actor_451100_8014E6E4` (the sub-model task
/// `func_actor_451100_801330B0`), hands it to `taskReparent` with this task
/// and keeps it in `pairTask`, then seeds the animation and runs the step
/// routine once.
///
/// `memCalloc`'s result goes through an untyped `block` that `work` is copied
/// from: the raw pointer is what the `Task::work` store and the null test read,
/// so it stays a short-lived `$v0` quantity while the typed copy takes the
/// callee-saved home it needs across the calls below. Assigning the call result
/// straight to `work` collapses the two into one pseudo and puts `$s1` in all
/// three places.
static void func_actor_451100_801328A8(Enemy* enemy, Task* task)
{
    VECTOR        vec;
    PairWalkWork* work;
    GfxCoord*     coord;
    TmdObject*    obj;
    Enemy*        spawned;
    void*         block;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    block      = memCalloc(sizeof(PairWalkWork), false);
    work       = block;
    task->work = block;
    if (block == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_451100_80132CAC;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = Gp_SpawnEnemyFromTable(D_actor_451100_8014E6E4, 1, 0, enemy);
    taskReparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_451100_8014E6FC, obj, work->rig.poses,
                         work->rig.slots);
    work->st.animId = 1;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = D_actor_451100_8014E6B4;
    pairWalkUpdate(task);
    task->state += 1;
}

#include "../../shared/pair_walk_update.inc.c"

/// Task handler of the actor whose work block lives only on its task, entry 0
/// of `D_actor_451100_8014E6E4`: runs the handler for the task's state from a
/// two-entry table built on the stack (0 spawns, 1 runs a frame), passing the
/// task's `Enemy` as well as the task.
void func_actor_451100_80132BD4(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_451100_801328A8,
        func_actor_451100_80132C28,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_451100_80132C28
#define walkerUpdate     pairWalkUpdate
#define walkerDrawShadow func_actor_451100_80132CD4
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback the `func_actor_451100_80132BD4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
static void func_actor_451100_80132CAC(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadowShaded func_actor_451100_80132CD4
#include "../../shared/walker_shadow_shaded.inc.c"
#undef walkerDrawShadowShaded

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x12 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_451100_80132E98(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PairWalkWork* work;

    work = task->work;
    if (args->animationId < 0x12) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        pairWalkUpdate(task);
        return 0;
    }
    return -1;
}

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

/// Message 0x7DB handler of `D_actor_451100_8014E6B4`: accepts the message and
/// does nothing.
s32 func_actor_451100_80132FE0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message 0x7DD handler of `D_actor_451100_8014E6B4`, the "walk to" opcode:
/// turns the actor's root coordinate to face `target`, caching the yaw in the
/// work block, and leaves the horizontal distance to it, in seventeenths, in
/// `travel` for the step routine to count down.
s32 func_actor_451100_80132FE8(Task* task, s32 arg1, VECTOR* target, s32 arg3)
{
    GfxCoord*     coord;
    PairWalkWork* work;
    s32           dx;
    s32           dz;
    u16           yaw;

    coord        = task->extra.tmd->coords;
    work         = task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

/// Per-frame handler of the sub-model task, entry 1 of
/// `D_actor_451100_8014E6E4`, reached with the sub-model's own `TmdObject` in
/// `Task::extra` and the actor holding it as `Task::parent`. On its first tick
/// it lights the sub-model with the parent's two leading work matrices and
/// hangs its root coordinate off coordinate 8 of the parent's model; after that
/// it only marks the coordinate dirty each frame so it follows that part.
void func_actor_451100_801330B0(Task* task)
{
    char       pad[0x10];
    Task*      parent = task->parent;
    TmdObject* obj    = task->extra.tmd;
    GfxCoord*  coord  = obj->coords;
    GfxCoord*  sub    = &parent->extra.tmd->coords[8];
    MATRIX*    work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = work;
            obj->colorMtx       = work + 1;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
