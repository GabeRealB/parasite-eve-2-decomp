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

static void _actor143000StartCaptureBand(void* captureArgs);
static void _actor143000SelectSceneCaptions(void);
static void _actor143000ResetSceneCaptions(void);
static void _actor143000SetActorControl(u8 actorControl);
static void _actor143000RestorePlayerEquipment(void);

static void _actor143000TerminalSessionTask(Task* task);

static void _actor143000UpdateCaptionCursor(s32 cursorX, s32 cursorY, const u16* text, s32 revealIndex, s32 codeAdvanced);

TaskDesc D_actor_143000_801350B0[2] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _actor143000TerminalSessionTask, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackPointer = _actor143000StartCaptureBand }, { .storage = &D_actor_143000_80135090 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackPointer = _actor143000StartCaptureBand }, { .storage = &D_actor_143000_801350A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

/// Runs the laboratory terminal menu and three-question challenge after keypad login.
///
/// spawnArg2 borrows the persistent keypad ScreenFade; initialization returns
/// that fade, selects CAP resource 1 at VRAM (832,0), identifies Bowman's and
/// Yoshida's cards and starts the login script. The menu starts a challenge,
/// logs out or displays a message. The challenge asks three distinct questions
/// from eleven. Three correct answers apply the laboratory's saved
/// updates and start the completion scene with its skip script. Logout waits for
/// its script, resets CAP and ends the task. CAP and overlay data stay loaded
/// throughout; visited-question storage must provide eleven writable bytes.
static void _actor143000TerminalSessionTask(Task* task)
{
    enum {
        ACTOR_143000_TERMINAL_STATE_INITIALIZE         = 0,
        ACTOR_143000_TERMINAL_STATE_WAIT_LOGIN_SCENE   = 1,
        ACTOR_143000_TERMINAL_STATE_SHOW_MENU          = 2,
        ACTOR_143000_TERMINAL_STATE_WAIT_MENU          = 3,
        ACTOR_143000_TERMINAL_STATE_HANDLE_MENU        = 4,
        ACTOR_143000_TERMINAL_STATE_WAIT_LOGOUT_PROMPT = 5,
        ACTOR_143000_TERMINAL_STATE_WAIT_MENU_MESSAGE  = 6,
        ACTOR_143000_TERMINAL_STATE_START_LOGOUT       = 10,
        ACTOR_143000_TERMINAL_STATE_WAIT_LOGOUT        = 11,
        ACTOR_143000_TERMINAL_STATE_FINISH             = 12,
        ACTOR_143000_TERMINAL_STATE_RESET_QUIZ         = 20,
        ACTOR_143000_TERMINAL_STATE_WAIT_QUIZ_INTRO    = 21,
        ACTOR_143000_TERMINAL_STATE_SELECT_QUESTION    = 22,
        ACTOR_143000_TERMINAL_STATE_WAIT_QUESTION      = 23,
        ACTOR_143000_TERMINAL_STATE_SCORE_ANSWER       = 24,
        ACTOR_143000_TERMINAL_STATE_SHOW_SCORE         = 30,
        ACTOR_143000_TERMINAL_STATE_WAIT_SCORE         = 31,
        ACTOR_143000_TERMINAL_STATE_SHOW_SUCCESS       = 40,
        ACTOR_143000_TERMINAL_STATE_WAIT_SUCCESS       = 41,
        ACTOR_143000_TERMINAL_CAP_MENU_WITH_CARD       = 1,
        ACTOR_143000_TERMINAL_CAP_MENU_WITHOUT_CARD    = 2,
        ACTOR_143000_TERMINAL_CAP_MENU_MESSAGE         = 3,
        ACTOR_143000_TERMINAL_CAP_QUIZ_INTRO           = 4,
        ACTOR_143000_TERMINAL_CAP_LOGOUT_PROMPT        = 32,
        ACTOR_143000_TERMINAL_CAP_QUIZ_SCORE           = 33,
        ACTOR_143000_TERMINAL_CAP_QUIZ_SUCCESS         = 34,
        ACTOR_143000_TERMINAL_CHOICE_QUIZ              = 10,
        ACTOR_143000_TERMINAL_CHOICE_LOGOUT            = 99,
        ACTOR_143000_TERMINAL_ANSWER_CORRECT           = 11,
        ACTOR_143000_TERMINAL_QUESTION_COUNT           = 11,
        ACTOR_143000_TERMINAL_RAND_BITS                = 15,
        ACTOR_143000_TERMINAL_CAP_FIRST_QUESTION       = 5,
        ACTOR_143000_TERMINAL_ANSWERS_REQUIRED         = 3,
        ACTOR_143000_TERMINAL_CAP_DATA_ORDINAL         = 1,
        ACTOR_143000_TERMINAL_CAP_VRAM_X               = 832,
        ACTOR_143000_TERMINAL_YOSHIDAS_CARD            = 0x122,
        ACTOR_143000_TERMINAL_COMPLETE_OBJECTIVE       = 39,
        ACTOR_143000_TERMINAL_PROGRESS_COMPLETE        = 2,
        ACTOR_143000_TERMINAL_COMPLETE_DIALOGUE        = 3,
    };

    ScreenFade* fade;
    s32         questionIndex;
    u8*         visitedQuestion;
    u8*         selectedQuestion;

    fade = task->spawnArg2.pointer;
    switch (task->state) {
        case ACTOR_143000_TERMINAL_STATE_INITIALIZE:
            // The keypad left the screen faded out; fade back in on the scene set up below.
            fade->phase = SCREEN_FADE_RETURN;
            srand(gDisplayState.gameTick);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            Gp_CapFile = 0;
            capSelectLoadedFile(ACTOR_143000_TERMINAL_CAP_DATA_ORDINAL);
            capSetTexturePage(ACTOR_143000_TERMINAL_CAP_VRAM_X, 0);
            itemSetIdentified(INVENTORY_COLLECTION_ID_BOWMANS_CARD, 1);
            itemSetIdentified(ACTOR_143000_TERMINAL_YOSHIDAS_CARD, 1);
            evsStartScript(D_actor_143000_80135A20, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_LOGIN_SCENE:
            if (gGameSession->eventState == 0) {
                gGameSession->eventState = 1;
                task->state              = ACTOR_143000_TERMINAL_STATE_SHOW_MENU;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SHOW_MENU:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BOWMANS_CARD) != 0) {
                capRunCommand(ACTOR_143000_TERMINAL_CAP_MENU_WITH_CARD, CAP_PLAYBACK_IN_PLACE);
            } else {
                capRunCommand(ACTOR_143000_TERMINAL_CAP_MENU_WITHOUT_CARD, CAP_PLAYBACK_IN_PLACE);
            }
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_MENU:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_HANDLE_MENU:
            switch (capGetVariantKey()) {
                case ACTOR_143000_TERMINAL_CHOICE_QUIZ:
                    task->state = ACTOR_143000_TERMINAL_STATE_RESET_QUIZ;
                    return;
                case ACTOR_143000_TERMINAL_CHOICE_LOGOUT:
                    capRunCommand(ACTOR_143000_TERMINAL_CAP_LOGOUT_PROMPT, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                    return;
                default:
                    capRunCommand(ACTOR_143000_TERMINAL_CAP_MENU_MESSAGE, CAP_PLAYBACK_IN_PLACE);
                    task->state = ACTOR_143000_TERMINAL_STATE_WAIT_MENU_MESSAGE;
                    return;
            }
        case ACTOR_143000_TERMINAL_STATE_WAIT_LOGOUT_PROMPT:
            if (capIsBusy() == 0) {
                task->state = ACTOR_143000_TERMINAL_STATE_START_LOGOUT;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_START_LOGOUT:
            evsStartScript(D_actor_143000_80135AE0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_LOGOUT:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_FINISH:
            capReset();
            taskKill(task);
            return;
        // Select three distinct questions; visited storage needs eleven entries.
        case ACTOR_143000_TERMINAL_STATE_RESET_QUIZ:
            questionIndex   = ACTOR_143000_TERMINAL_QUESTION_COUNT - 1;
            visitedQuestion = &D_actor_143000_80135C38[questionIndex];
            do {
                *visitedQuestion = 0;
                questionIndex--;
                visitedQuestion--;
            } while (questionIndex >= 0);
            D_actor_143000_80135C18 = 0;
            D_actor_143000_80135C1C = 0;
            capRunCommand(ACTOR_143000_TERMINAL_CAP_QUIZ_INTRO, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_QUIZ_INTRO:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SELECT_QUESTION:
            while (1) {
                D_actor_143000_80135C14 = (rand() * ACTOR_143000_TERMINAL_QUESTION_COUNT) >> ACTOR_143000_TERMINAL_RAND_BITS;
                selectedQuestion        = &D_actor_143000_80135C38[D_actor_143000_80135C14];
                if (*selectedQuestion == 0) {
                    *selectedQuestion = 1;
                    break;
                }
            }
            capRunCommand(D_actor_143000_80135C14 + ACTOR_143000_TERMINAL_CAP_FIRST_QUESTION, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_QUESTION:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SCORE_ANSWER:
            if (capGetVariantKey() == ACTOR_143000_TERMINAL_ANSWER_CORRECT) {
                D_actor_143000_80135C1C++;
            }
            D_actor_143000_80135C18++;
            if (D_actor_143000_80135C18 >= ACTOR_143000_TERMINAL_ANSWERS_REQUIRED) {
                if (D_actor_143000_80135C1C >= ACTOR_143000_TERMINAL_ANSWERS_REQUIRED) {
                    task->state = ACTOR_143000_TERMINAL_STATE_SHOW_SUCCESS;
                } else {
                    task->state = ACTOR_143000_TERMINAL_STATE_SHOW_SCORE;
                }
            } else {
                task->state = ACTOR_143000_TERMINAL_STATE_SELECT_QUESTION;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SHOW_SCORE:
            capStartSequenceSlot(ACTOR_143000_TERMINAL_CAP_QUIZ_SCORE, 0, (s16)D_actor_143000_80135C1C);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_MENU_MESSAGE:
        case ACTOR_143000_TERMINAL_STATE_WAIT_SCORE:
            if (capIsBusy() == 0) {
                task->state = ACTOR_143000_TERMINAL_STATE_SHOW_MENU;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SHOW_SUCCESS:
            capRunCommand(ACTOR_143000_TERMINAL_CAP_QUIZ_SUCCESS, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_SUCCESS:
            if (capIsBusy() == 0) {
                capReset();
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACTOR_143000_TERMINAL_COMPLETE_OBJECTIVE);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, ACTOR_143000_TERMINAL_PROGRESS_COMPLETE);
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
                areaApplySavedUpdates(D_shelter_b2_laboratory_80186488);
                if (gameFlagGetNibble(GAME_FLAG_083) == 0) {
                    areaApplySavedUpdates(D_shelter_b2_laboratory_8018649C);
                }
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACTOR_143000_TERMINAL_COMPLETE_DIALOGUE);
                evsStartScriptWithSkip(D_actor_143000_801351B0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_143000_80135870);
                taskKill(task);
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

/// Starts asynchronous strip capture for the event script's borrowed band record.
///
/// captureArgs must point to a writable Actor143000CaptureArgs meeting
/// actor143000CaptureStripTask's row/count contract. The task retains it and
/// writes progress until capture ends; one record serves one live capture.
/// The event callback's pointer is transported in one PS1 argument word.
static void _actor143000StartCaptureBand(void* captureArgs)
{
    taskSpawnFromTable(&D_actor_143000_801350C8, 0, 0, captureArgs);
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

/// Restores the player's equipped weapon models after the laboratory scene.
///
/// Both the completion script and its skip path call this after scripted
/// presentation; the live player and equipped weapon resources are required.
static void _actor143000RestorePlayerEquipment(void)
{
    playerActorRestoreEquipment();
}
