#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_actor_143000_80135C10;

s32 D_actor_143000_80135C14;

s32 D_actor_143000_80135C18;

s32 D_actor_143000_80135C1C;

#include "actor_143000_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/shelter_b2_laboratory.h"

/// The clips this actor's event scenes add to the player's animation bank,
/// with the play requests stored after them.
///
/// Each event script that plays one of these clips first sends the player a
/// copy request for this storage. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the five set pointers occupy
/// extended ids 47-51, and the first 27 words of the play requests are written
/// into the bank after them. The requests select ids 1 and 47-51 only, so none
/// of those request words is played as a clip.
///
/// The request for id 51 is a separate object stored directly after this one.
/// The requests are written as well as read: the caption callback stores the
/// player's current weapon bank in `source.index` before it dispatches one.
typedef union {
    struct {
        AnimationSet*        sets[5];         // Player clips for extended ids 47-51
        AnimationPlayRequest playRequests[6]; // Requests for base id 1, then extended ids 47, 47, 48, 49 and 50; nothing references the second
    } data;                                   // The records by name
    s32 words[35];                            // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor143000AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor143000AnimationBankExtensionStorage, 140);

extern _Actor143000AnimationBankExtensionStorage D_actor_143000_801350D4;

extern u8         D_actor_143000_801351AC;
extern EvsCommand D_actor_143000_801351B0[];
extern TaskDesc   D_actor_143000_801350C8;
extern EvsCommand D_actor_143000_80135870[];
extern EvsCommand D_actor_143000_80135A20[];
extern EvsCommand D_actor_143000_80135AE0[];

extern u8 D_actor_143000_80135C38[];

void func_actor_143000_801344A8(s32);
void func_actor_143000_801344D8(void);
void func_actor_143000_8013450C(void);
void func_actor_143000_8013452C(u8);
void func_actor_143000_80134538(void);

void func_actor_143000_80133EE4(Task*);

void func_actor_143000_801342F8(s32 x, s32 y, const u16* codes, s32 index, s32 active);

TaskDesc D_actor_143000_801350B0[2] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_143000_80133EE4, { .value = 0 } },
};

TaskDesc D_actor_143000_801350C8 = { { { TASK_BODY_NONE, 32 } }, func_actor_143000_80133CF0, { .value = 0 } };

_Actor143000AnimationBankExtensionStorage D_actor_143000_801350D4 = { .data = { { &gActor143000Animation02A20, &gActor143000Animation02CCC, &gActor143000Animation02EE8, &gActor143000Animation03090, &gActor143000Animation03248 }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_143000_80135160 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_143000_80135174 = { { .words = D_actor_143000_801350D4.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_143000_8013517C = { { 3270, 0, -1630, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_143000_80135194 = { { 3270, 0, -2630, 0 }, { 0, -2048, 0, 0 } };

u8 D_actor_143000_801351AC = 0;

EvsCommand D_actor_143000_801351B0[72] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_801344D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackSetText = capSetTextUpdateCallback }, { .captionText = func_actor_143000_801342F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143000_80135174 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_8013517C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_143000_8013452C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000F }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_80135090 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_801350A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_143000_8013452C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b2_laboratory_801804FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135160 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_80135194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_8013450C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143000_80135870[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_143000_8013452C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_80135194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b2_laboratory_801804FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_80134538 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_8013450C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143000_80135A20[8] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143000_80135174 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_8013517C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143000_80135AE0[12] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_80135194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_80134538 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_actor_143000_80135C00 = 0;

s32 D_actor_143000_80135C04 = 0;

ScreenFade D_actor_143000_80135C08 = { 0 };

u8 D_actor_143000_80135C0C[4] = {
    0,
    101,
    2,
    57,
};

char D_actor_143000_80135C20[24];

u8 D_actor_143000_80135C38[8];

void func_actor_143000_80133EE4(Task* arg0)
{
    ScreenFade* fade;
    s32         i;
    u8*         p;
    u8*         slot;

    fade = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            // The keypad left the screen faded out; fade back in on the scene set up below.
            fade->phase = SCREEN_FADE_RETURN;
            srand(gDisplayState.gameTick);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            Gp_CapFile = 0;
            capSelectLoadedFile(1);
            capSetTexturePage(0x340, 0);
            itemSetIdentified(0x121, 1);
            itemSetIdentified(0x122, 1);
            evsStartScript(D_actor_143000_80135A20, EVENT_SCRIPT_HUD_KEEP);
            arg0->state++;
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                gGameSession->eventState = 1;
                arg0->state              = 2;
            }
            return;
        case 2:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BOWMANS_CARD) != 0) {
                capRunCommand(1, CAP_PLAYBACK_IN_PLACE);
            } else {
                capRunCommand(2, CAP_PLAYBACK_IN_PLACE);
            }
            arg0->state++;
            return;
        case 3:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            return;
        case 4:
            switch (capGetVariantKey()) {
                case 0xA:
                    arg0->state = 0x14;
                    return;
                case 0x63:
                    capRunCommand(0x20, CAP_PLAYBACK_IN_PLACE);
                    arg0->state++;
                    return;
                default:
                    capRunCommand(3, CAP_PLAYBACK_IN_PLACE);
                    arg0->state = 6;
                    return;
            }
        case 5:
            if (capIsBusy() == 0) {
                arg0->state = 0xA;
            }
            return;
        case 10:
            evsStartScript(D_actor_143000_80135AE0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            arg0->state++;
            return;
        case 11:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            return;
        case 12:
            capReset();
            taskKill(arg0);
            return;
        case 20:
            i = 10;
            p = &D_actor_143000_80135C38[i];
            do {
                *p = 0;
                i--;
                p--;
            } while (i >= 0);
            D_actor_143000_80135C18 = 0;
            D_actor_143000_80135C1C = 0;
            capRunCommand(4, CAP_PLAYBACK_IN_PLACE);
            arg0->state++;
            return;
        case 21:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            return;
        case 22:
            while (1) {
                D_actor_143000_80135C14 = (rand() * 11) >> 15;
                slot                    = &D_actor_143000_80135C38[D_actor_143000_80135C14];
                if (*slot == 0) {
                    *slot = 1;
                    break;
                }
            }
            capRunCommand(D_actor_143000_80135C14 + 5, CAP_PLAYBACK_IN_PLACE);
            arg0->state++;
            return;
        case 23:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            return;
        case 24:
            if (capGetVariantKey() == 0xB) {
                D_actor_143000_80135C1C++;
            }
            D_actor_143000_80135C18++;
            if (D_actor_143000_80135C18 >= 3) {
                if (D_actor_143000_80135C1C >= 3) {
                    arg0->state = 0x28;
                } else {
                    arg0->state = 0x1E;
                }
            } else {
                arg0->state = 0x16;
            }
            return;
        case 30:
            capStartSequenceSlot(0x21, 0, (s16)D_actor_143000_80135C1C);
            arg0->state++;
            return;
        case 6:
        case 31:
            if (capIsBusy() == 0) {
                arg0->state = 2;
            }
            return;
        case 40:
            capRunCommand(0x22, CAP_PLAYBACK_IN_PLACE);
            arg0->state++;
            return;
        case 41:
            if (capIsBusy() == 0) {
                capReset();
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x27);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, 2);
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
                Gp_ApplyAreaRecs(D_shelter_b2_laboratory_80186488);
                if (gameFlagGetNibble(GAME_FLAG_083) == 0) {
                    Gp_ApplyAreaRecs(D_shelter_b2_laboratory_8018649C);
                }
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 3);
                evsStartScriptWithSkip(D_actor_143000_801351B0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_143000_80135870);
                taskKill(arg0);
            }
            return;
    }
}

void func_actor_143000_801342F8(s32 x, s32 y, const u16* codes, s32 index, s32 active)
{
    POLY_F4* prim;

    if (y < 0x59) {
        if (active != 0) {
            if ((codes[index] & 0xF000) == 0x3000) {
                playerActorWriteWeaponAnimationBankIndex(&D_actor_143000_801350D4.data.playRequests[3].source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_143000_801350D4.data.playRequests[3], 0);
                D_actor_143000_801351AC = 1;
            }
            if (codes[index] == 0xFFFE && D_actor_143000_801351AC == 1) {
                D_actor_143000_801351AC = 0;
                playerActorWriteWeaponAnimationBankIndex(&D_actor_143000_801350D4.data.playRequests[5].source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_143000_801350D4.data.playRequests[5], 0);
            }
        }
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyF4(prim);
        setRGB0(prim, 0, 0x7C, 0x2C);
        setXY4(prim, x + 4, y - 15, x + 19, y - 15, x + 4, y, x + 19, y);
        if (D_actor_143000_80135C10 & 4) {
            addPrim(&gGpuCurrentOt[2], prim);
        }
        D_actor_143000_80135C10++;
    }
}

void func_actor_143000_801344A8(s32 arg0)
{
    taskSpawnFromTable(&D_actor_143000_801350C8, 0, 0, arg0);
}

void func_actor_143000_801344D8(void)
{
    Gp_CapFile = 0;
    capSelectLoadedFile(3);
    capSetTexturePage(0x180, 0x100);
}

void func_actor_143000_8013450C(void)
{
    capReset();
}

void func_actor_143000_8013452C(u8 arg0)
{
    gSceneCombatState.actorControl = arg0;
}

void func_actor_143000_80134538(void)
{
    Gp_SpawnWeaponEff();
}
