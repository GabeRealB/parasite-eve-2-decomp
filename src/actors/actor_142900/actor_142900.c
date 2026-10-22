#include "actors/actor_142900.h"
#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/shelter_b2_elevator.h"

/// Shared shake phases; scripts request STOPPED, RUNNING or BEGIN_FADE.
enum {
    ACTOR_142900_SCREEN_SHAKE_STOPPED    = 0,
    ACTOR_142900_SCREEN_SHAKE_RUNNING    = 1,
    ACTOR_142900_SCREEN_SHAKE_BEGIN_FADE = 2,
    ACTOR_142900_SCREEN_SHAKE_FADING     = 3,
};

/// Duration of the shared amplitude ramp, in shake-task callbacks.
enum { ACTOR_142900_SCREEN_SHAKE_FADE_TICKS = 20 };

/// Scene clip indices in the current weapon or companion bank; both extensions start at 47.
enum {
    ACTOR_142900_IDLE_ANIMATION      = 1,
    ACTOR_142900_SCENE_ANIMATION_47  = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_142900_SCENE_ANIMATION_48  = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_142900_SCENE_ANIMATION_49  = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_142900_SCENE_ANIMATION_50  = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_142900_SCENE_ANIMATION_51  = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_142900_SCENE_ANIMATION_52  = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_142900_SCENE_ANIMATION_53  = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_142900_SCENE_ANIMATION_54  = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_142900_SCENE_ANIMATION_55  = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_142900_SCENE_ANIMATION_56  = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_142900_SCENE_ANIMATION_57  = ANIMATION_BANK_BASE_SET_COUNT + 10,
    ACTOR_142900_SCENE_ANIMATION_58  = ANIMATION_BANK_BASE_SET_COUNT + 11,
    ACTOR_142900_SCENE_ANIMATION_59  = ANIMATION_BANK_BASE_SET_COUNT + 12,
    ACTOR_142900_SCENE_BLEND_FRAMES  = 10,
    ACTOR_142900_SCENE_TASK_PRIORITY = 192,
};

static AnimationSet _gShelterB2ElevatorCompanionThisIsItClip;
static AnimationSet _gShelterB2ElevatorCompanionHandOverIdCardClip;
static AnimationSet _gShelterB2ElevatorCompanionAfterHandingCardClip;
static AnimationSet _gShelterB2ElevatorCompanionTalk1Clip;
static AnimationSet _gShelterB2ElevatorCompanionTalk2Clip;
static AnimationSet _gPierceAnim04Clip;
static AnimationSet _gPierceAnim35Clip;
static AnimationSet _gShelterB2ElevatorCompanionAnimationBankExtensionAnim05Clip;
static AnimationSet _gShelterB2ElevatorCompanionDontGetMeWrongClip;
static AnimationSet _gShelterB2ElevatorCompanionIAmAPiClip;
static AnimationSet _gDryfieldNightGarageCompanionShrugClip;
static AnimationSet _gKyleMadiganAnim04Clip;
static AnimationSet _gKyleListenClip;

/// Remaining callbacks in the elevator scene's shared screen-shake fade.
///
/// Initialized to 20 on BEGIN_FADE; active values are 1..20. Each fade callback
/// scales by the current count before decrementing it. Zero remains after the
/// fade ends; while inactive the retained value has no amplitude meaning.
/// The script must keep at most one shake task live.
static s32 _gActor142900ScreenShakeFadeCallbacksRemaining;

/// Shared vertical screen-shake phase (0 stopped, 1 running, 2 begin fade, 3 fading).
///
/// Scripts request 0..2; the next task callback changes 2 to 3 and initializes
/// the fade count. Its last fade callback selects 0 and clears the display.
/// Raw setter values are retained as s32; callers must use the established
/// phases and keep at most one shake task live.
static s32 _gActor142900ScreenShakePhase;

static void _actor142900ReloadElevatorHall(void);
static void _actor142900SetScreenShakePhase(s32 phase);

static AnimationSet _gShelterB2ElevatorPlayerTakeIdCardClip;
static AnimationSet _gShelterB2ElevatorPlayerLookAtIdCardClip;
static AnimationSet _gShelterB2ElevatorPlayerListenToPasswordClip;
static AnimationSet _gShelterB2ElevatorPlayerShockedClip;
extern AnimationSet gAyaIntroduceHerselfClip;
extern AnimationSet gAyaOnMyWayClip;
static AnimationSet _gAyaMayaWasMySisterClip;
static AnimationSet _gAyaSayKyleClip;
static AnimationSet _gAyaYouToo1Clip;
static AnimationSet _gAyaYouToo2Clip;

static void _actor142900ScreenShakeTask(Task* task);

#include "assets/rooms/shelter/b2_elevator/elevator_scene_clips.inc"

// Descriptor 0's handler has the two-argument enemy shape, not a `TaskFunc`'s.
TaskDesc gActor142900TaskTable[2] = {
    { { { TASK_BODY_NONE, ACTOR_142900_SCENE_TASK_PRIORITY } }, (TaskFunc)enemyDestroy, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_142900_SCENE_TASK_PRIORITY } }, _actor142900ScreenShakeTask, { .value = 0 } },
};

/// Companion scene clips installed at extended bank indices 47..59.
///
/// Installation copies 32 raw words: these 13 pointers and 19 words from
/// the immediately following request data: three complete play requests and
/// four words of the next.
/// The extra words are never selected for playback. Preserve their original
/// adjacency and keep this package loaded while its installed clips are live.
/// Script playback copies requests before overwriting source.index with the
/// receiver's current bank; the stored request words remain unchanged.
#include "assets/rooms/shelter/b2_elevator/companion_scene_bank.inc"

/// Spare companion scene-clip 47 reset record included in the raw bank copy.
///
/// No playback instruction selects this request. All five words are read after
/// the thirteen clip pointers by the retained 32-word extension transfer.
/// Preserve its position; bank seed 1 and enabled collision remain stored data.
static AnimationPlayRequest _gActor142900UnusedCompanionPlayAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 47 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 48 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 49 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 58 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation58 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 59 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation59 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 50 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unplayed companion scene-clip 51/52 resets outside the bank-copy span.
///
/// Both retain bank seed 1, enabled world collision and an ignored zero blend
/// duration. No script or other recovered consumer reads either record.
static AnimationPlayRequest _gActor142900UnusedCompanionPlayAnimations51To52[2] = {
    { { .index = 1 }, ACTOR_142900_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ACTOR_142900_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Resets companion scene clip 53 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation53 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 54 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation54 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 55 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation55 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 56 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation56 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets companion scene clip 57 with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionPlayAnimation57 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player scene clips installed at extended bank indices 47..56.
///
/// Installation copies 32 raw words: these 10 pointers and 22 words from
/// the immediately following request data: four complete play requests and
/// two words of the next.
/// The extra words are never selected for playback. Preserve their original
/// adjacency and keep this package loaded while its installed clips are live.
/// Script playback copies requests before overwriting source.index with the
/// receiver's current bank; the stored request words remain unchanged.
#include "assets/rooms/shelter/b2_elevator/player_scene_bank.inc"

/// Spare player scene-clip 47 reset record included in the raw bank copy.
///
/// No playback instruction selects this request. All five words follow the ten
/// clip pointers in the retained 32-word extension transfer. Preserve its
/// position, bank seed 1 and enabled world-collision word.
static AnimationPlayRequest _gActor142900UnusedPlayerPlayAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 47 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 48 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 49 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 50 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets the selected player bank's idle clip with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerIdleAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets the selected companion bank's idle clip with world collision enabled.
static AnimationPlayRequest _gActor142900CompanionIdleAnimation = {
    .source               = { .index = 6 },
    .animationId          = ACTOR_142900_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends to player scene clip 51 over ten frames with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerBlendAnimation51 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_142900_SCENE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 52 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation52 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 53 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation53 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 54 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation54 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 55 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation55 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player scene clip 56 with world collision enabled.
static AnimationPlayRequest _gActor142900PlayerPlayAnimation56 = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_142900_SCENE_ANIMATION_56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Copies 32 raw words from the player scene table into its current weapon bank extension.
///
/// The source span includes the adjacent request words described at the table.
/// Dispatch borrows this request and source; copied clips remain borrowed for
/// playback. The destination bank must be loaded and writable.
static AnimationBankCopyRequest _gActor142900PlayerAnimationBankCopyRequest = {
    .sets  = _gActor142900PlayerAnimationBankExtension,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Copies 32 raw words from the companion scene table into its selected companion bank extension.
///
/// The source span includes the adjacent request words described at the table.
/// Dispatch borrows this request and source; copied clips remain borrowed for
/// playback. The destination bank must be loaded and writable.
static AnimationBankCopyRequest _gActor142900CompanionAnimationBankCopyRequest = {
    .sets  = _gActor142900CompanionAnimationBankExtension,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Player placement for elevator scene setup and skipped-scene restoration.
///
/// Position is in whole room-coordinate units; zero Euler angles face +Z.
/// Placement dispatch borrows the complete record without retaining it.
static ActorTransform _gActor142900PlayerElevatorPlacement = {
    .pos = { 11360, 0, -1050, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Companion placement facing the player at elevator scene setup.
///
/// Position is in whole room-coordinate units; yaw is a half turn toward -Z.
/// Placement dispatch borrows the complete record without retaining it.
static ActorTransform _gActor142900CompanionElevatorPlacement = {
    .pos = { 11360, 0, 190, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 2, 0, 0 },
};

/// CAP slots, performer selection and room-view target of the first-visit scripts.
enum {
    ACTOR_142900_CAP_FIRST_VISIT          = 1,
    ACTOR_142900_CAP_SKIP_CONTINUATION    = 2,
    ACTOR_142900_WEAPONS_BOTH_ACTORS      = 2,
    ACTOR_142900_FIRST_VISIT_EXIT_VIEW    = 2,
    ACTOR_142900_ELEVATOR_SOUND_3         = 0x541A0003,
    ACTOR_142900_ELEVATOR_DOOR_OPEN_SOUND = 0x541A0001,
};

EvsCommand gActor142900ElevatorFirstVisitScript[87] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_142900_CAP_FIRST_VISIT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142900PlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142900CompanionAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_142900_WEAPONS_BOTH_ACTORS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142900PlayerElevatorPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142900CompanionElevatorPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor142900SetScreenShakePhase }, { .value = ACTOR_142900_SCREEN_SHAKE_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor142900SetScreenShakePhase }, { .value = ACTOR_142900_SCREEN_SHAKE_BEGIN_FADE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_142900_ELEVATOR_SOUND_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = SHELTER_B2_ELEVATOR_MESSAGE_OPEN_DOOR }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_142900_ELEVATOR_DOOR_OPEN_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerBlendAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation59 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation59 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation59 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionPlayAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerPlayAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor142900ReloadElevatorHall }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142900_FIRST_VISIT_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142900_WEAPONS_BOTH_ACTORS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor142900ElevatorFirstVisitSkipScript[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor142900SetScreenShakePhase }, { .value = ACTOR_142900_SCREEN_SHAKE_STOPPED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_142900_CAP_SKIP_CONTINUATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor142900ReloadElevatorHall }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142900_FIRST_VISIT_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142900_WEAPONS_BOTH_ACTORS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142900PlayerElevatorPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Advances the elevator scene's shake fade and returns its scaled vertical offset.
///
/// Call once per shake-task callback while FADING, with one live shake task and
/// `unfadedOffsetY` in [-4, 4] pixels. The shared remaining-callback count must
/// be in [1, ACTOR_142900_SCREEN_SHAKE_FADE_TICKS]. Scale by the count before
/// decrementing it, rounding toward zero, then advance the task's sine phase
/// by five counts, retaining the signed 16-bit wrap in `killCountdown`.
/// Reaching zero selects STOPPED; the caller clears the display and ends the
/// task without applying the final result.
static inline s32 _actor142900FadeScreenShake(Task* task, s32 unfadedOffsetY)
{
    enum { ACTOR_142900_SCREEN_SHAKE_FADE_PHASE_STEP = 5 };

    s32 fadedOffsetY;

    fadedOffsetY = unfadedOffsetY * _gActor142900ScreenShakeFadeCallbacksRemaining / ACTOR_142900_SCREEN_SHAKE_FADE_TICKS;
    _gActor142900ScreenShakeFadeCallbacksRemaining--;
    task->killCountdown += ACTOR_142900_SCREEN_SHAKE_FADE_PHASE_STEP;
    if (_gActor142900ScreenShakeFadeCallbacksRemaining == 0) {
        _gActor142900ScreenShakePhase = ACTOR_142900_SCREEN_SHAKE_STOPPED;
    }
    return fadedOffsetY;
}

/// Applies the elevator scene's vertical shake and ends its task when stopped.
///
/// Descriptor 1 spawns a zeroed, bodyless task. Its signed 16-bit
/// `killCountdown` counts the sine phase, advancing once per callback while
/// running and five times per callback during the 20-callback fade. The
/// 60-count cycle produces offsets in [-4, 4] pixels before fading; each
/// counter update retains the task field's 16-bit wrap. Stopping clears the
/// persistent display offset before task teardown.
static void _actor142900ScreenShakeTask(Task* task)
{
    /// Sine cadence in callbacks and its Q12 amplitude conversion to pixels.
    enum {
        ACTOR_142900_SCREEN_SHAKE_CYCLE_TICKS      = 60,
        ACTOR_142900_SCREEN_SHAKE_AMPLITUDE_PIXELS = 4,
        ACTOR_142900_SCREEN_SHAKE_SINE_DIVISOR     = ONE / ACTOR_142900_SCREEN_SHAKE_AMPLITUDE_PIXELS,
    };

    // The display callee narrows to s8; its prototype adds a conversion absent here.
    extern void displaySetShakeY();
    s32         offsetY;

    if (_gActor142900ScreenShakePhase == ACTOR_142900_SCREEN_SHAKE_BEGIN_FADE) {
        _gActor142900ScreenShakePhase                  = ACTOR_142900_SCREEN_SHAKE_FADING;
        _gActor142900ScreenShakeFadeCallbacksRemaining = ACTOR_142900_SCREEN_SHAKE_FADE_TICKS;
    }
    // Convert the phase to a Q12 turn; signed division truncates toward zero.
    offsetY = rsin((task->killCountdown * ONE) / ACTOR_142900_SCREEN_SHAKE_CYCLE_TICKS) / ACTOR_142900_SCREEN_SHAKE_SINE_DIVISOR;
    if (_gActor142900ScreenShakePhase == ACTOR_142900_SCREEN_SHAKE_RUNNING) {
        task->killCountdown = task->killCountdown + 1;
    }
    if (_gActor142900ScreenShakePhase == ACTOR_142900_SCREEN_SHAKE_FADING) {
        // Fade the amplitude as the phase speeds up; the last callback clears it.
        offsetY = _actor142900FadeScreenShake(task, offsetY);
    }
    if (_gActor142900ScreenShakePhase == ACTOR_142900_SCREEN_SHAKE_STOPPED) {
        displaySetShakeY(0);
        taskKill(task);
    } else {
        displaySetShakeY(offsetY);
    }
}

/// Completes the elevator scene and queues arrival in the B2 elevator hall.
///
/// Both the full and skipped scripts call this with the Mine/Shelter stage
/// active. Demo scene 9 leaves saved state unchanged. Otherwise applies the
/// elevator's saved-area update, selects companion schedule 0 and arrival warp
/// 2 in room 1, then queues a captured-frame reload with sprite variant 1.
/// Requires the room update list and gameplay reload bank to remain loaded;
/// the reload runs later and its spawn result is ignored.
static void _actor142900ReloadElevatorHall(void)
{
    enum {
        ACTOR_142900_DEMO_SCENE                   = 9,
        ACTOR_142900_COMPANION_SCHEDULE           = 0,
        ACTOR_142900_ELEVATOR_HALL_ARRIVAL_WARP   = 2,
        ACTOR_142900_ELEVATOR_HALL_ROOM           = 1,
        ACTOR_142900_ELEVATOR_HALL_SPRITE_VARIANT = 1,
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_142900_DEMO_SCENE) {
        areaApplySavedUpdates(gShelterB2ElevatorSceneSavedAreaUpdates);
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, ACTOR_142900_COMPANION_SCHEDULE);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B2_ELEVATOR_HALL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_142900_ELEVATOR_HALL_ARRIVAL_WARP;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_142900_ELEVATOR_HALL_ROOM;
        gDisplayState.spriteVariant                                 = ACTOR_142900_ELEVATOR_HALL_SPRITE_VARIANT;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    }
}

/// Requests the elevator scene's shared vertical screen-shake phase.
///
/// Scripts pass STOPPED (0), RUNNING (1) or BEGIN_FADE (2). Every RUNNING
/// request attempts to spawn descriptor 1, even if already running; callers
/// must keep at most one shake task live. Spawn failure still updates the
/// phase. STOPPED and BEGIN_FADE take effect on the next shake-task callback;
/// this function does not clear the display offset itself.
static void _actor142900SetScreenShakePhase(s32 phase)
{
    enum { ACTOR_142900_SCREEN_SHAKE_TASK_INDEX = 1 };

    if (phase == ACTOR_142900_SCREEN_SHAKE_RUNNING) {
        taskSpawnFromTable(gActor142900TaskTable, ACTOR_142900_SCREEN_SHAKE_TASK_INDEX, 0, 0);
    }
    _gActor142900ScreenShakePhase = phase;
}
