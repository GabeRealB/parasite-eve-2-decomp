#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_1f_heliport.h"
// The paced walk helpers this package carries run on the stride walker's block.
#define PACED_WALK_WORK_T StrideWalkWork
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"
#include "../../shared/stride_walk.h"

/// The clips the soldier's talk scenes add to the player's animation bank,
/// with the play requests stored after them.
///
/// Every event script the soldier's talk and remark handlers start sends a
/// copy request for this storage to the player before it plays any of the
/// clips. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the
/// start of the storage, which is more than the clip table holds: the six set
/// pointers occupy extended ids 47-52, and the first 26 words of the play
/// requests are written into the bank after them. The requests select ids
/// 47-52 only, so none of those request words is played as a clip.
typedef union {
    struct {
        AnimationSet*        sets[6];         // Player clips for extended ids 47-52
        AnimationPlayRequest playRequests[6]; // One request per clip, in id order; the talk scripts play them on the player
    } data;                                   // The records by name
    s32 words[36];                            // The same storage as the copy reads it; the last four words lie beyond the copied span
} _Actor161500TalkAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor161500TalkAnimationBankExtensionStorage, 144);

extern _Actor161500TalkAnimationBankExtensionStorage D_actor_161500_80133F90;

/// The clips the soldier's request scenes add to the player's animation bank,
/// with the words stored after them.
///
/// The request scenes are the ones the soldier plays while the companion is
/// present: the first exchange, the two that repeat while the request is
/// pending, and the one that hands the item over. Each event script that plays
/// one of these clips first sends the player a copy request for this storage.
/// The copy takes 10 words from the start of the storage, which is more than
/// the clip table holds: the six set pointers occupy extended ids 47-52, the
/// NULL that closes the table lands at id 53, and the first three words of the
/// play request are written into the bank after it. The scenes select ids
/// 47-52 only, so none of those following words is played as a clip.
///
/// The play request is part of this object only because the copied span
/// reaches into it. The six requests the scripts do play, one per clip, are
/// separate objects stored directly after this one.
typedef union {
    struct {
        AnimationSet*        sets[7];     // Player clips for extended ids 47-52, then NULL at id 53, which nothing requests
        AnimationPlayRequest playRequest; // Request for extended id 47; nothing references it, and it repeats the first of the requests after this object
    } data;                               // The records by name
    s32 words[12];                        // The same storage as the copy reads it; the last two words lie beyond the copied span
} _Actor161500RequestSceneAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor161500RequestSceneAnimationBankExtensionStorage, 48);

extern _Actor161500RequestSceneAnimationBankExtensionStorage D_actor_161500_80136D60;

extern TaskDesc      gStrideWalkTasks[];
extern AnimationSet* gStrideWalkAnimParams[12];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gStrideWalkMessages[6];

extern EvsCommand*    D_actor_161500_80134920[8];
extern EvsCommand*    D_actor_161500_80135288[8];
extern EvsCommand     D_actor_161500_801352A8[];
extern EvsCommand     D_actor_161500_801354B8[];
extern EvsCommand     D_actor_161500_80135668[];
extern EvsCommand     D_actor_161500_801357E8[];
extern EvsCommand     D_actor_161500_80135968[];
extern EvsCommand     D_actor_161500_80135AE8[];
extern EvsCommand     D_actor_161500_80135C68[];
extern EvsCommand     D_actor_161500_80136E88[];
extern EvsCommand     D_actor_161500_80137080[];
extern EvsCommand     D_actor_161500_80137650[];
extern ActorTransform D_actor_161500_801376E0;
extern EvsCommand     D_actor_161500_801376F8[];
extern EvsCommand     D_actor_161500_801378D8[];
extern EvsCommand     D_actor_161500_80137AB8[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern AnimationPlayRequest     D_actor_161500_80133F7C;
extern AnimationPlayRequest     D_actor_161500_80134020;
extern AnimationPlayRequest     D_actor_161500_80134034;
extern ActorCommand             D_actor_161500_80133F74;
extern ActorCommand             D_actor_161500_80133F78;
extern AnimationBankCopyRequest D_actor_161500_80134048;
void                            func_actor_161500_80131F50(s32);
void                            func_actor_161500_801320B4(void);
void                            func_actor_161500_801320F0(s32);
void                            func_actor_161500_80132150(void);

void func_actor_161500_801321B4(Task*);

static AnimationSet _gActor161500Animation04304;
static AnimationSet _gActor161500Animation04518;
static AnimationSet _gActor161500Animation047F8;
static AnimationSet _gActor161500Animation04A88;
static AnimationSet _gActor161500Animation04C60;
static AnimationSet _gActor161500Animation04E94;

extern ActorTransform D_actor_161500_80136CE8;
extern ActorTransform D_actor_161500_80136D00;
extern ActorTransform D_actor_161500_80136D18;
extern ActorTransform D_actor_161500_80136D30;
extern ActorTransform D_actor_161500_80136D48;

void func_actor_161500_80131F50(s32);

extern AnimationPlayRequest     D_actor_161500_80136D90;
extern AnimationPlayRequest     D_actor_161500_80136DA4;
extern AnimationPlayRequest     D_actor_161500_80136DB8;
extern AnimationPlayRequest     D_actor_161500_80136E38;
extern AnimationPlayRequest     D_actor_161500_80136E60;
extern AnimationPlayRequest     D_actor_161500_80136E74;
extern AnimationBankCopyRequest D_actor_161500_80136E08;
s32                             func_actor_161500_80132B88(Task* task, s32 msgId, ActorCommand* args, s32 arg3);
void                            func_actor_161500_80132210(void);
void                            func_actor_161500_80132294(u8);
void                            func_actor_161500_801326E8(Task*);

static AnimationPackedPose _gActor161500Animation010D0Bank1[3] = {
#include "assets/actor_161500_animation_010D0_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation010D0Bank4[36] = {
#include "assets/actor_161500_animation_010D0_bank4.inc"
};

static AnimationRecord _gActor161500Animation010D0Records[80] = {
#include "assets/actor_161500_animation_010D0_records.inc"
};

static u16 _gActor161500Animation010D0Indices[20] = {
#include "assets/actor_161500_animation_010D0_indices.inc"
};

static AnimationSet _gActor161500Animation010D0 = {
    _gActor161500Animation010D0Records,
    _gActor161500Animation010D0Indices,
    { NULL, _gActor161500Animation010D0Bank1, NULL, NULL, _gActor161500Animation010D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0129CBank1[3] = {
#include "assets/actor_161500_animation_0129C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0129CBank4[29] = {
#include "assets/actor_161500_animation_0129C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0129CRecords[57] = {
#include "assets/actor_161500_animation_0129C_records.inc"
};

static u16 _gActor161500Animation0129CIndices[20] = {
#include "assets/actor_161500_animation_0129C_indices.inc"
};

static AnimationSet _gActor161500Animation0129C = {
    _gActor161500Animation0129CRecords,
    _gActor161500Animation0129CIndices,
    { NULL, _gActor161500Animation0129CBank1, NULL, NULL, _gActor161500Animation0129CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation016C4Bank1[5] = {
#include "assets/actor_161500_animation_016C4_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation016C4Bank4[90] = {
#include "assets/actor_161500_animation_016C4_bank4.inc"
};

static AnimationRecord _gActor161500Animation016C4Records[141] = {
#include "assets/actor_161500_animation_016C4_records.inc"
};

static u16 _gActor161500Animation016C4Indices[20] = {
#include "assets/actor_161500_animation_016C4_indices.inc"
};

static AnimationSet _gActor161500Animation016C4 = {
    _gActor161500Animation016C4Records,
    _gActor161500Animation016C4Indices,
    { NULL, _gActor161500Animation016C4Bank1, NULL, NULL, _gActor161500Animation016C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation01A48Bank1[6] = {
#include "assets/actor_161500_animation_01A48_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation01A48Bank4[69] = {
#include "assets/actor_161500_animation_01A48_bank4.inc"
};

static AnimationRecord _gActor161500Animation01A48Records[118] = {
#include "assets/actor_161500_animation_01A48_records.inc"
};

static u16 _gActor161500Animation01A48Indices[20] = {
#include "assets/actor_161500_animation_01A48_indices.inc"
};

static AnimationSet _gActor161500Animation01A48 = {
    _gActor161500Animation01A48Records,
    _gActor161500Animation01A48Indices,
    { NULL, _gActor161500Animation01A48Bank1, NULL, NULL, _gActor161500Animation01A48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation01D74Bank1[7] = {
#include "assets/actor_161500_animation_01D74_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation01D74Bank4[56] = {
#include "assets/actor_161500_animation_01D74_bank4.inc"
};

static AnimationRecord _gActor161500Animation01D74Records[106] = {
#include "assets/actor_161500_animation_01D74_records.inc"
};

static u16 _gActor161500Animation01D74Indices[20] = {
#include "assets/actor_161500_animation_01D74_indices.inc"
};

static AnimationSet _gActor161500Animation01D74 = {
    _gActor161500Animation01D74Records,
    _gActor161500Animation01D74Indices,
    { NULL, _gActor161500Animation01D74Bank1, NULL, NULL, _gActor161500Animation01D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0212CBank1[3] = {
#include "assets/actor_161500_animation_0212C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0212CBank4[81] = {
#include "assets/actor_161500_animation_0212C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0212CRecords[128] = {
#include "assets/actor_161500_animation_0212C_records.inc"
};

static u16 _gActor161500Animation0212CIndices[20] = {
#include "assets/actor_161500_animation_0212C_indices.inc"
};

static AnimationSet _gActor161500Animation0212C = {
    _gActor161500Animation0212CRecords,
    _gActor161500Animation0212CIndices,
    { NULL, _gActor161500Animation0212CBank1, NULL, NULL, _gActor161500Animation0212CBank4, NULL, NULL, NULL },
};

ActorCommand D_actor_161500_80133F74 = { { .loc = { 5, 4 } }, 1 };

ActorCommand D_actor_161500_80133F78 = { { .loc = { 5, 4 } }, 0 };

AnimationPlayRequest D_actor_161500_80133F7C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

_Actor161500TalkAnimationBankExtensionStorage D_actor_161500_80133F90 = { .data = { { &_gActor161500Animation010D0, &_gActor161500Animation0129C, &_gActor161500Animation016C4, &_gActor161500Animation01A48, &_gActor161500Animation01D74, &_gActor161500Animation0212C }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE } } } };

AnimationPlayRequest D_actor_161500_80134020 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80134034 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_actor_161500_80134048 = { { .words = D_actor_161500_80133F90.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

EvsCommand D_actor_161500_80134050[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134188[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134290[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134398[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801344A0[15] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134608[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 35 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134710[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 36 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134818[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 37 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand* D_actor_161500_80134920[8] = {
    D_actor_161500_80134050,
    D_actor_161500_80134188,
    D_actor_161500_80134290,
    D_actor_161500_80134398,
    D_actor_161500_801344A0,
    D_actor_161500_80134608,
    D_actor_161500_80134710,
    D_actor_161500_80134818,
};

EvsCommand D_actor_161500_80134940[15] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_161500_80133F90.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134AA8[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134BC8[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134D00[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134E08[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134F28[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 39 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135048[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135180[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 41 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand* D_actor_161500_80135288[8] = {
    D_actor_161500_80134940,
    D_actor_161500_80134AA8,
    D_actor_161500_80134BC8,
    D_actor_161500_80134D00,
    D_actor_161500_80134E08,
    D_actor_161500_80134F28,
    D_actor_161500_80135048,
    D_actor_161500_80135180,
};

EvsCommand D_actor_161500_801352A8[22] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_801320B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 52 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801354B8[18] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_801320B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 52 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135668[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801357E8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 25 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135968[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 26 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135AE8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135C68[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor161500Animation04304Bank1[6] = {
#include "assets/actor_161500_animation_04304_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04304Bank4[75] = {
#include "assets/actor_161500_animation_04304_bank4.inc"
};

static AnimationRecord _gActor161500Animation04304Records[104] = {
#include "assets/actor_161500_animation_04304_records.inc"
};

static u16 _gActor161500Animation04304Indices[20] = {
#include "assets/actor_161500_animation_04304_indices.inc"
};

static AnimationSet _gActor161500Animation04304 = {
    _gActor161500Animation04304Records,
    _gActor161500Animation04304Indices,
    { NULL, _gActor161500Animation04304Bank1, NULL, NULL, _gActor161500Animation04304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04518Bank1[2] = {
#include "assets/actor_161500_animation_04518_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04518Bank4[24] = {
#include "assets/actor_161500_animation_04518_bank4.inc"
};

static AnimationRecord _gActor161500Animation04518Records[83] = {
#include "assets/actor_161500_animation_04518_records.inc"
};

static u16 _gActor161500Animation04518Indices[20] = {
#include "assets/actor_161500_animation_04518_indices.inc"
};

static AnimationSet _gActor161500Animation04518 = {
    _gActor161500Animation04518Records,
    _gActor161500Animation04518Indices,
    { NULL, _gActor161500Animation04518Bank1, NULL, NULL, _gActor161500Animation04518Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation047F8Bank1[5] = {
#include "assets/actor_161500_animation_047F8_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation047F8Bank4[60] = {
#include "assets/actor_161500_animation_047F8_bank4.inc"
};

static AnimationRecord _gActor161500Animation047F8Records[89] = {
#include "assets/actor_161500_animation_047F8_records.inc"
};

static u16 _gActor161500Animation047F8Indices[20] = {
#include "assets/actor_161500_animation_047F8_indices.inc"
};

static AnimationSet _gActor161500Animation047F8 = {
    _gActor161500Animation047F8Records,
    _gActor161500Animation047F8Indices,
    { NULL, _gActor161500Animation047F8Bank1, NULL, NULL, _gActor161500Animation047F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04A88Bank1[3] = {
#include "assets/actor_161500_animation_04A88_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04A88Bank4[36] = {
#include "assets/actor_161500_animation_04A88_bank4.inc"
};

static AnimationRecord _gActor161500Animation04A88Records[99] = {
#include "assets/actor_161500_animation_04A88_records.inc"
};

static u16 _gActor161500Animation04A88Indices[20] = {
#include "assets/actor_161500_animation_04A88_indices.inc"
};

static AnimationSet _gActor161500Animation04A88 = {
    _gActor161500Animation04A88Records,
    _gActor161500Animation04A88Indices,
    { NULL, _gActor161500Animation04A88Bank1, NULL, NULL, _gActor161500Animation04A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04C60Bank1[3] = {
#include "assets/actor_161500_animation_04C60_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04C60Bank4[32] = {
#include "assets/actor_161500_animation_04C60_bank4.inc"
};

static AnimationRecord _gActor161500Animation04C60Records[57] = {
#include "assets/actor_161500_animation_04C60_records.inc"
};

static u16 _gActor161500Animation04C60Indices[20] = {
#include "assets/actor_161500_animation_04C60_indices.inc"
};

static AnimationSet _gActor161500Animation04C60 = {
    _gActor161500Animation04C60Records,
    _gActor161500Animation04C60Indices,
    { NULL, _gActor161500Animation04C60Bank1, NULL, NULL, _gActor161500Animation04C60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04E94Bank1[3] = {
#include "assets/actor_161500_animation_04E94_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04E94Bank4[29] = {
#include "assets/actor_161500_animation_04E94_bank4.inc"
};

static AnimationRecord _gActor161500Animation04E94Records[83] = {
#include "assets/actor_161500_animation_04E94_records.inc"
};

static u16 _gActor161500Animation04E94Indices[20] = {
#include "assets/actor_161500_animation_04E94_indices.inc"
};

static AnimationSet _gActor161500Animation04E94 = {
    _gActor161500Animation04E94Records,
    _gActor161500Animation04E94Indices,
    { NULL, _gActor161500Animation04E94Bank1, NULL, NULL, _gActor161500Animation04E94Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_161500_80136CDC = { { { TASK_BODY_NONE, 32 } }, func_actor_161500_801321B4, { .value = 0 } };

ActorTransform D_actor_161500_80136CE8 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D00 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D18 = { { 2510, 0, 5950, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D30 = { { 1330, 0, 4780, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D48 = { { 4224, 0, 5209, 0 }, { 0, 2048, 0, 0 } };

_Actor161500RequestSceneAnimationBankExtensionStorage D_actor_161500_80136D60 = { .data = { { &_gActor161500Animation04304, &_gActor161500Animation04518, &_gActor161500Animation047F8, &_gActor161500Animation04A88, &_gActor161500Animation04C60, &_gActor161500Animation04E94, NULL }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } };

AnimationPlayRequest D_actor_161500_80136D90 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DA4 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DB8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DCC = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DE0 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DF4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_actor_161500_80136E08 = { { .words = D_actor_161500_80136D60.words }, 10 };

AnimationPlayRequest D_actor_161500_80136E10 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E24 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E38 = { { .index = 6 }, 38, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E4C = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E60 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E74 = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_161500_80136E88[21] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x55040003 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 23 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137080[62] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136CE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_161500_80136D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_80136D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137650[6] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

ActorTransform D_actor_161500_801376E0 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_161500_801376F8[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801378D8[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 29 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137AB8[29] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_161500_80132294 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor161500SoldierBRifleSkeleton[1] = {
#include "assets/soldier_b_rifle_skeleton.inc"
};

static u32 _gActor161500SoldierBRiflePartVerts[1] = {
#include "assets/soldier_b_rifle_partVerts.inc"
};

static SVECTOR _gActor161500SoldierBRifleVerts[58] = {
#include "assets/soldier_b_rifle_verts.inc"
};

static SVECTOR _gActor161500SoldierBRifleNormals[58] = {
#include "assets/soldier_b_rifle_normals.inc"
};

static u32 _gActor161500SoldierBRifleStream[406] = {
#include "assets/soldier_b_rifle_stream.inc"
};

static TmdSource _gActor161500SoldierBRifle = {
    0,
    2940,
    0,
    1,
    _gActor161500SoldierBRiflePartVerts,
    _gActor161500SoldierBRifleVerts,
    _gActor161500SoldierBRifleNormals,
    _gActor161500SoldierBRifleSkeleton,
    _gActor161500SoldierBRifleStream,
};

static TmdBone _gActor161500SoldierBBodySkeleton[20] = {
#include "assets/soldier_b_body_skeleton.inc"
};

static u32 _gActor161500SoldierBBodyPartVerts[20] = {
#include "assets/soldier_b_body_partVerts.inc"
};

static SVECTOR _gActor161500SoldierBBodyVerts[366] = {
#include "assets/soldier_b_body_verts.inc"
};

static SVECTOR _gActor161500SoldierBBodyNormals[363] = {
#include "assets/soldier_b_body_normals.inc"
};

static u32 _gActor161500SoldierBBodyStream[3913] = {
#include "assets/soldier_b_body_stream.inc"
};

static TmdSource _gActor161500SoldierBBody = {
    0,
    21132,
    6448,
    20,
    _gActor161500SoldierBBodyPartVerts,
    _gActor161500SoldierBBodyVerts,
    _gActor161500SoldierBBodyNormals,
    _gActor161500SoldierBBodySkeleton,
    _gActor161500SoldierBBodyStream,
};

static AnimationPackedPose _gActor161500Animation0C318Bank1[2] = {
#include "assets/actor_161500_animation_0C318_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0C318Bank4[32] = {
#include "assets/actor_161500_animation_0C318_bank4.inc"
};

static AnimationRecord _gActor161500Animation0C318Records[101] = {
#include "assets/actor_161500_animation_0C318_records.inc"
};

static u16 _gActor161500Animation0C318Indices[20] = {
#include "assets/actor_161500_animation_0C318_indices.inc"
};

static AnimationSet _gActor161500Animation0C318 = {
    _gActor161500Animation0C318Records,
    _gActor161500Animation0C318Indices,
    { NULL, _gActor161500Animation0C318Bank1, NULL, NULL, _gActor161500Animation0C318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0C88CBank1[2] = {
#include "assets/actor_161500_animation_0C88C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0C88CBank4[135] = {
#include "assets/actor_161500_animation_0C88C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0C88CRecords[188] = {
#include "assets/actor_161500_animation_0C88C_records.inc"
};

static u16 _gActor161500Animation0C88CIndices[20] = {
#include "assets/actor_161500_animation_0C88C_indices.inc"
};

static AnimationSet _gActor161500Animation0C88C = {
    _gActor161500Animation0C88CRecords,
    _gActor161500Animation0C88CIndices,
    { NULL, _gActor161500Animation0C88CBank1, NULL, NULL, _gActor161500Animation0C88CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0CB1CBank1[3] = {
#include "assets/actor_161500_animation_0CB1C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0CB1CBank4[27] = {
#include "assets/actor_161500_animation_0CB1C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0CB1CRecords[108] = {
#include "assets/actor_161500_animation_0CB1C_records.inc"
};

static u16 _gActor161500Animation0CB1CIndices[20] = {
#include "assets/actor_161500_animation_0CB1C_indices.inc"
};

static AnimationSet _gActor161500Animation0CB1C = {
    _gActor161500Animation0CB1CRecords,
    _gActor161500Animation0CB1CIndices,
    { NULL, _gActor161500Animation0CB1CBank1, NULL, NULL, _gActor161500Animation0CB1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D2B0Bank1[21] = {
#include "assets/actor_161500_animation_0D2B0_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D2B0Bank4[156] = {
#include "assets/actor_161500_animation_0D2B0_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D2B0Records[246] = {
#include "assets/actor_161500_animation_0D2B0_records.inc"
};

static u16 _gActor161500Animation0D2B0Indices[20] = {
#include "assets/actor_161500_animation_0D2B0_indices.inc"
};

static AnimationSet _gActor161500Animation0D2B0 = {
    _gActor161500Animation0D2B0Records,
    _gActor161500Animation0D2B0Indices,
    { NULL, _gActor161500Animation0D2B0Bank1, NULL, NULL, _gActor161500Animation0D2B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D544Bank1[5] = {
#include "assets/actor_161500_animation_0D544_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D544Bank4[36] = {
#include "assets/actor_161500_animation_0D544_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D544Records[94] = {
#include "assets/actor_161500_animation_0D544_records.inc"
};

static u16 _gActor161500Animation0D544Indices[20] = {
#include "assets/actor_161500_animation_0D544_indices.inc"
};

static AnimationSet _gActor161500Animation0D544 = {
    _gActor161500Animation0D544Records,
    _gActor161500Animation0D544Indices,
    { NULL, _gActor161500Animation0D544Bank1, NULL, NULL, _gActor161500Animation0D544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D834Bank1[2] = {
#include "assets/actor_161500_animation_0D834_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D834Bank4[55] = {
#include "assets/actor_161500_animation_0D834_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D834Records[107] = {
#include "assets/actor_161500_animation_0D834_records.inc"
};

static u16 _gActor161500Animation0D834Indices[20] = {
#include "assets/actor_161500_animation_0D834_indices.inc"
};

static AnimationSet _gActor161500Animation0D834 = {
    _gActor161500Animation0D834Records,
    _gActor161500Animation0D834Indices,
    { NULL, _gActor161500Animation0D834Bank1, NULL, NULL, _gActor161500Animation0D834Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D9E8Bank1[2] = {
#include "assets/actor_161500_animation_0D9E8_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D9E8Bank4[19] = {
#include "assets/actor_161500_animation_0D9E8_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D9E8Records[64] = {
#include "assets/actor_161500_animation_0D9E8_records.inc"
};

static u16 _gActor161500Animation0D9E8Indices[20] = {
#include "assets/actor_161500_animation_0D9E8_indices.inc"
};

static AnimationSet _gActor161500Animation0D9E8 = {
    _gActor161500Animation0D9E8Records,
    _gActor161500Animation0D9E8Indices,
    { NULL, _gActor161500Animation0D9E8Bank1, NULL, NULL, _gActor161500Animation0D9E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0DB9CBank1[2] = {
#include "assets/actor_161500_animation_0DB9C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0DB9CBank4[19] = {
#include "assets/actor_161500_animation_0DB9C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0DB9CRecords[64] = {
#include "assets/actor_161500_animation_0DB9C_records.inc"
};

static u16 _gActor161500Animation0DB9CIndices[20] = {
#include "assets/actor_161500_animation_0DB9C_indices.inc"
};

static AnimationSet _gActor161500Animation0DB9C = {
    _gActor161500Animation0DB9CRecords,
    _gActor161500Animation0DB9CIndices,
    { NULL, _gActor161500Animation0DB9CBank1, NULL, NULL, _gActor161500Animation0DB9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0DE54Bank1[3] = {
#include "assets/actor_161500_animation_0DE54_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0DE54Bank4[29] = {
#include "assets/actor_161500_animation_0DE54_bank4.inc"
};

static AnimationRecord _gActor161500Animation0DE54Records[116] = {
#include "assets/actor_161500_animation_0DE54_records.inc"
};

static u16 _gActor161500Animation0DE54Indices[20] = {
#include "assets/actor_161500_animation_0DE54_indices.inc"
};

static AnimationSet _gActor161500Animation0DE54 = {
    _gActor161500Animation0DE54Records,
    _gActor161500Animation0DE54Indices,
    { NULL, _gActor161500Animation0DE54Bank1, NULL, NULL, _gActor161500Animation0DE54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0E148Bank1[2] = {
#include "assets/actor_161500_animation_0E148_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0E148Bank4[43] = {
#include "assets/actor_161500_animation_0E148_bank4.inc"
};

static AnimationRecord _gActor161500Animation0E148Records[120] = {
#include "assets/actor_161500_animation_0E148_records.inc"
};

static u16 _gActor161500Animation0E148Indices[20] = {
#include "assets/actor_161500_animation_0E148_indices.inc"
};

static AnimationSet _gActor161500Animation0E148 = {
    _gActor161500Animation0E148Records,
    _gActor161500Animation0E148Indices,
    { NULL, _gActor161500Animation0E148Bank1, NULL, NULL, _gActor161500Animation0E148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0E338Bank1[2] = {
#include "assets/actor_161500_animation_0E338_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0E338Bank4[30] = {
#include "assets/actor_161500_animation_0E338_bank4.inc"
};

static AnimationRecord _gActor161500Animation0E338Records[68] = {
#include "assets/actor_161500_animation_0E338_records.inc"
};

static u16 _gActor161500Animation0E338Indices[20] = {
#include "assets/actor_161500_animation_0E338_indices.inc"
};

static AnimationSet _gActor161500Animation0E338 = {
    _gActor161500Animation0E338Records,
    _gActor161500Animation0E338Indices,
    { NULL, _gActor161500Animation0E338Bank1, NULL, NULL, _gActor161500Animation0E338Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gStrideWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, strideWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, strideWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_161500_80132B88 },
    { ACTOR_MESSAGE_WALK_TO, strideWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gStrideWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_161500_801326E8, { .model = &_gActor161500SoldierBBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, strideWalkSubModelTask, { .model = &_gActor161500SoldierBRifle } },
};

AnimationSet* gStrideWalkAnimParams[12] = {
    NULL,
    &_gActor161500Animation0C318,
    &_gActor161500Animation0CB1C,
    &_gActor161500Animation0C88C,
    &_gActor161500Animation0D2B0,
    &_gActor161500Animation0D834,
    &_gActor161500Animation0D9E8,
    &_gActor161500Animation0DB9C,
    &_gActor161500Animation0DE54,
    &_gActor161500Animation0E148,
    &_gActor161500Animation0E338,
    &_gActor161500Animation0D544,
};

void func_actor_161500_80131E38(void);
void func_actor_161500_80131FBC(void);
void func_actor_161500_80132038(void);
void func_actor_161500_80132110(void);
void func_actor_161500_801322A0(void);
void func_actor_161500_8013230C(void);

void func_actor_161500_80131E38(void)
{
    if ((gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != 1) && (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != 2) && (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 4)) {
        gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, 5);
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 4);
    }

    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE)) {
        case 0:
            func_800E8614(D_actor_161500_80135668, 0);
            break;
        case 1:
            func_800E8614(D_actor_161500_801357E8, 0);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 3);
            break;
        case 2:
            func_800E8614(D_actor_161500_80135968, 0);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 3);
            break;
        case 3:
            func_800E8614(D_actor_161500_80135AE8, 0);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 0);
            break;
        case 4:
            func_800E8614(D_actor_161500_80135C68, 0);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 0);
            break;
    }
}

void func_actor_161500_80131F50(s32 arg0)
{
    s8 capFile;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        if (arg0 <= 0) {
            capFile = 1;
            if (gGameSession->location.loc.variant == 1) {
                capFile = 2;
            }
            arg0 = capFile;
        }
        Gp_LoadCapFile(arg0);
        capSetTexturePage(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_161500_80131FBC(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->location.loc.variant == 1) * 4;
    temp_v0 = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A);
    func_800E8614(D_actor_161500_80134920[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A, temp_v0 + 1);
    }
}

void func_actor_161500_80132038(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->location.loc.variant == 1) * 4;
    temp_v0 = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B);
    func_800E8614(D_actor_161500_80135288[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B, temp_v0 + 1);
    }
}

void func_actor_161500_801320B4(void)
{
    GameSession* session;

    session = gGameSession;
    do {
        func_800D4D2C((session->location.loc.variant == 1) ? 0x31 : 0x30);
    } while (0);
}

void func_actor_161500_801320F0(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_actor_161500_80132110(void)
{
    if (gameFlagGetNibble(GAME_FLAG_105) == 0) {
        func_800E8614(D_actor_161500_801352A8, 0);
    } else {
        func_800E8614(D_actor_161500_801354B8, 0);
    }
}

void func_actor_161500_80132150(void)
{
    if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
        func_800D4D2C((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) != 2) ? 0x31 : 0x33);
    } else {
        func_800D4D2C((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == 2) ? 0x32 : 0x30);
    }
}

void func_actor_161500_801321B4(Task* arg0)
{
    D_80115768 = 1;
    Gp_SetItemSeenBit(0x124, 1);
    gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, 2);
    func_800E8614(D_actor_161500_80137AB8, 0);
    taskKill(arg0);
}

void func_actor_161500_80132210(void)
{
    GfxCoord* target;
    GfxCoord* player;

    target = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    player = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actorRenderComposeCoord(target);
    actorRenderComposeCoord(player);
    D_actor_161500_801376E0.rot.vy =
        ratan2(target->coord.t[0] - player->coord.t[0], target->coord.t[2] - player->coord.t[2]) & 0xFFF;
}

void func_actor_161500_80132294(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_161500_801322A0(void)
{
    s32 temp_v0;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        temp_v0 = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        if (temp_v0 == 1) {
            if (areaGetCurrentObjectState(3) == temp_v0) {
                func_800E8614(D_actor_161500_801378D8, 0);
            } else {
                func_800E8614(D_actor_161500_801376F8, 0);
            }
        }
    }
}

void func_actor_161500_8013230C(void)
{
    s32 temp_v0;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        temp_v0 = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        switch (temp_v0) {
            case 0:
                func_800E8634(D_actor_161500_80137080, 0, D_actor_161500_80136E88);
                gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, 1);
                break;
            case 1:
                func_800E8614(D_actor_161500_80137650, 1);
                break;
            case 2:
                break;
        }
    }
}

#include "../../shared/stride_walk_spawn.inc.c"

#include "../../shared/stride_walk_update.inc.c"

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `Enemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_161500_801326E8(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        strideWalkSpawn,
        strideWalkFrame,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/stride_walk_frame.inc.c"

/// The actor's `Task::exitCallback`: hands the task's `Enemy`, parked in
/// `Task::spawnArg2`, back to `enemyDestroy`.
void strideWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow.inc.c"

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

#include "../../shared/stride_walk_play.inc.c"

#include "../../shared/stride_walk_visibility.inc.c"

#include "../../shared/paced_walk_place.inc.c"

/// Turn command: sets the work block's `turnMode`, which selects whether the
/// per-frame body turns the walker's head toward the player
/// (`STRIDE_WALK_TURN_PLAYER`) or lets it settle back, to the command's value.
s32 func_actor_161500_80132B88(Task* task, s32 arg1, ActorCommand* args, s32 arg3)
{
    StrideWalkWork* work = task->work;

    work->turnMode = args->command;
    return 0;
}

#include "../../shared/stride_walk_to.inc.c"

#include "../../shared/stride_walk_sub_model.inc.c"
