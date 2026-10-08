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

extern u8         D_actor_143000_801351AC;
extern EvsCommand D_actor_143000_801351B0[];
extern TaskDesc   D_actor_143000_801350C8;
extern EvsCommand D_actor_143000_80135870[];
extern EvsCommand D_actor_143000_80135A20[];
extern EvsCommand D_actor_143000_80135AE0[];

extern u8 D_actor_143000_80135C38[];

void        func_actor_143000_801344A8(s32);
static void _actor143000SelectSceneCaptions(void);
static void _actor143000ResetSceneCaptions(void);
static void _actor143000SetActorControl(u8 actorControl);
void        func_actor_143000_80134538(void);

void func_actor_143000_80133EE4(Task*);

static void _actor143000UpdateCaptionCursor(s32 cursorX, s32 cursorY, const u16* text, s32 revealIndex, s32 codeAdvanced);

TaskDesc D_actor_143000_801350B0[2] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_143000_80133EE4, { .value = 0 } },
};

TaskDesc D_actor_143000_801350C8 = { { { TASK_BODY_NONE, 32 } }, actor143000CaptureStripTask, { .value = 0 } };

/// Player clips for extended ids 47-51.
///
/// Each event script that plays one of these clips first sends the player its
/// copy request. `D_actor_143000_80135174` copies
/// `ANIMATION_BANK_EXTENSION_CAPACITY` (32) words starting here into the
/// player's bank, which is 27 words past the end of this array: the read runs
/// on through `D_actor_143000_801350E8`, `D_actor_143000_801350FC`,
/// `D_actor_143000_80135110`, `D_actor_143000_80135124`,
/// `D_actor_143000_80135138` and the first two words of
/// `D_actor_143000_8013514C`. That overrun is the original's and is kept as it
/// is: the request carries the bank's fixed capacity, while the table was
/// stored with only its own entries. The requests select ids 1 and 47-51 only,
/// so none of the words installed after the table is played as a clip.
AnimationSet* D_actor_143000_801350D4[5] = { &gActor143000Animation02A20, &gActor143000Animation02CCC, &gActor143000Animation02EE8, &gActor143000Animation03090, &gActor143000Animation03248 };

// The player's requests start here: base id 1, then the extended ids in order.
AnimationPlayRequest D_actor_143000_801350E8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Not referenced.
AnimationPlayRequest D_actor_143000_801350FC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143000_80135110 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// The caption callback stores the player's current weapon bank in `source.index` before it dispatches this one.
AnimationPlayRequest D_actor_143000_80135124 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143000_80135138 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// The caption callback stores the player's current weapon bank in `source.index` before it dispatches this one.
AnimationPlayRequest D_actor_143000_8013514C = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143000_80135160 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_143000_80135174 = { { .sets = D_actor_143000_801350D4 }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_143000_8013517C = { { 3270, 0, -1630, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_143000_80135194 = { { 3270, 0, -2630, 0 }, { 0, -2048, 0, 0 } };

u8 D_actor_143000_801351AC = 0;

EvsCommand D_actor_143000_801351B0[72] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000SelectSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackSetText = capSetTextUpdateCallback }, { .captionText = _actor143000UpdateCaptionCursor }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143000_80135174 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_8013517C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135110 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135110 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135110 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135110 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_HIDDEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541F000F }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_80135090 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_801350A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135138 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000ResetSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_80135194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b2_laboratory_801804FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143000_80134538 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000ResetSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143000_80135A20[8] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143000_80135174 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_8013517C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135110 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143000_80135AE0[12] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143000_80135194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350E8 }, { .value = 0 } },
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

u8 D_actor_143000_80135C0C = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_actor_143000_80135C0D = 101;

u8 D_actor_143000_80135C0E = 2;

u8 D_actor_143000_80135C0F = 57;

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
                areaApplySavedUpdates(D_shelter_b2_laboratory_80186488);
                if (gameFlagGetNibble(GAME_FLAG_083) == 0) {
                    areaApplySavedUpdates(D_shelter_b2_laboratory_8018649C);
                }
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 3);
                evsStartScriptWithSkip(D_actor_143000_801351B0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_143000_80135870);
                taskKill(arg0);
            }
            return;
    }
}

/// Draws the scene's blinking caption cursor and applies its player animation cues.
///
/// Follows `CapTextUpdateCallback`: coordinates are screen-centred pixels, text is
/// borrowed CAP storage, and revealIndex is the post-update u16 element index.
/// For Y < 89, each newly reached 0x3xxx glyph (reveal delay 6) restarts clip 48;
/// the next newly reached line break blends to clip 50 if that cue is latched.
/// Draws a 15-pixel green square four pixels to the cursor's right for four of
/// every eight eligible calls. Requires the installed player bank extension,
/// live player task, GPU primitive arena and ordering table. The overlay and
/// caption storage must remain loaded until CAP clears the callback.
static void _actor143000UpdateCaptionCursor(s32 cursorX, s32 cursorY, const u16* text, s32 revealIndex, s32 codeAdvanced)
{
    enum {
        ACTOR_143000_CAPTION_BOTTOM_Y       = 89,
        ACTOR_143000_ANIMATION_CODE_MASK    = 0xF000,
        ACTOR_143000_ANIMATION_CODE_DELAY_6 = 0x3000,
        ACTOR_143000_TEXT_LINE_BREAK        = 0xFFFE,
        ACTOR_143000_CURSOR_SIZE            = 15,
        ACTOR_143000_CURSOR_X_OFFSET        = 4,
        ACTOR_143000_CURSOR_GREEN           = 124,
        ACTOR_143000_CURSOR_BLUE            = 44,
        ACTOR_143000_CURSOR_BLINK_BIT       = 4,
        ACTOR_143000_CURSOR_OT_INDEX        = 2,
    };

    POLY_F4* quad;

    if (cursorY < ACTOR_143000_CAPTION_BOTTOM_Y) {
        // Newly reached delayed glyphs restart clip 48; a line break releases it.
        if (codeAdvanced != 0) {
            if ((text[revealIndex] & ACTOR_143000_ANIMATION_CODE_MASK) == ACTOR_143000_ANIMATION_CODE_DELAY_6) {
                playerActorWriteWeaponAnimationBankIndex(&D_actor_143000_80135124.source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_143000_80135124, 0);
                D_actor_143000_801351AC = 1;
            }
            if (text[revealIndex] == ACTOR_143000_TEXT_LINE_BREAK && D_actor_143000_801351AC == 1) {
                D_actor_143000_801351AC = 0;
                playerActorWriteWeaponAnimationBankIndex(&D_actor_143000_8013514C.source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_143000_8013514C, 0);
            }
        }
        // Reserve the cursor packet even on the dark half of its eight-call blink.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyF4(quad);
        setRGB0(quad, 0, ACTOR_143000_CURSOR_GREEN, ACTOR_143000_CURSOR_BLUE);
        setXY4(quad, cursorX + ACTOR_143000_CURSOR_X_OFFSET, cursorY - ACTOR_143000_CURSOR_SIZE, cursorX + ACTOR_143000_CURSOR_X_OFFSET + ACTOR_143000_CURSOR_SIZE, cursorY - ACTOR_143000_CURSOR_SIZE, cursorX + ACTOR_143000_CURSOR_X_OFFSET, cursorY, cursorX + ACTOR_143000_CURSOR_X_OFFSET + ACTOR_143000_CURSOR_SIZE, cursorY);
        if (D_actor_143000_80135C10 & ACTOR_143000_CURSOR_BLINK_BIT) {
            addPrim(&gGpuCurrentOt[ACTOR_143000_CURSOR_OT_INDEX], quad);
        }
        D_actor_143000_80135C10++;
    }
}

void func_actor_143000_801344A8(s32 arg0)
{
    taskSpawnFromTable(&D_actor_143000_801350C8, 0, 0, arg0);
}

/// Selects the laboratory scene's CAP file and title/text texture origin.
///
/// Requires loaded CAP data resource ordinal 3 and its texture at VRAM (384,256).
/// Clears the current file before selecting the fourth data resource;
/// the selected CAP data and texture must remain live throughout playback.
static void _actor143000SelectSceneCaptions(void)
{
    enum { ACTOR_143000_SCENE_CAP_DATA_ORDINAL = 3,
           ACTOR_143000_CAP_VRAM_X             = 384,
           ACTOR_143000_CAP_VRAM_Y             = 256 };

    Gp_CapFile = 0;
    capSelectLoadedFile(ACTOR_143000_SCENE_CAP_DATA_ORDINAL);
    capSetTexturePage(ACTOR_143000_CAP_VRAM_X, ACTOR_143000_CAP_VRAM_Y);
}

/// Restores default CAP resources after the scene has stopped playback.
///
/// The current CDF bundle's default CAP resource must remain loaded and writable.
/// Used by both normal completion and the skip script; no task is killed here.
static void _actor143000ResetSceneCaptions(void)
{
    capReset();
}

/// Selects scene actor updating and presentation for the event script.
///
/// Pass `SCENE_COMBAT_ACTORS_RUNNING`, `SCENE_COMBAT_ACTORS_PAUSED` or
/// `SCENE_COMBAT_ACTORS_HIDDEN`. Stores the byte unchanged; effect control
/// and event-script execution are independent of this setting.
static void _actor143000SetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

void func_actor_143000_80134538(void)
{
    playerActorRestoreEquipment();
}
