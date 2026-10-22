#include "actors/actor_142600.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"

#include "main/session.h"

// This package contains no code: only the retained scene data below. Its id
// word, before the data, is written by the link (tools/splat_ext/pkgid.py).

enum {
    ACTOR_142600_RETAINED_TASK_PRIORITY = 192,
    ACTOR_142600_ARRIVAL_CAP_COMMAND    = 2,
    ACTOR_142600_EXIT_VIEW              = 5,
    ACTOR_142600_WEAPON_SELECTION_BOTH  = 2,
    ACTOR_142600_RESUME_NORMAL_CONTROL  = 0,
    ACTOR_142600_FADE_DEFAULT_RAMP      = 0,
    ACTOR_142600_FADE_KEEP_RAMP         = 0,
    ACTOR_142600_IDLE_ANIMATION         = 1,
    ACTOR_142600_SCENE_ANIMATION_47     = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_142600_SCENE_ANIMATION_48,
    ACTOR_142600_SCENE_ANIMATION_49,
    ACTOR_142600_SCENE_ANIMATION_50,
    ACTOR_142600_SCENE_ANIMATION_51,
    ACTOR_142600_SCENE_ANIMATION_52,
    ACTOR_142600_SCENE_ANIMATION_53,
    ACTOR_142600_SCENE_ANIMATION_54,
    ACTOR_142600_SCENE_ANIMATION_55,
    ACTOR_142600_SCENE_ANIMATION_56,
    ACTOR_142600_SCENE_ANIMATION_57,
    ACTOR_142600_SCENE_ANIMATION_58,
};

#include "assets/rooms/shelter/b3_incinerator_control_room/arrival_scene_clips.inc"

TaskDesc gActor142600TaskDescriptors[1] = {
    // Retain the unspawned descriptor's original, incompatible callback address.
    { { { TASK_BODY_NONE, ACTOR_142600_RETAINED_TASK_PRIORITY } }, (TaskFunc)enemyDestroy, { .value = 0 } },
};

/// Companion scene clips installed at extended bank indices 47..53.
///
/// The 32-word copy reads 25 words past these seven pointers, through the five
/// requests from `_gActor142600UnusedCompanionAnimation47` to
/// `_gActor142600CompanionAnimation50`. Those words are not playable clips.
/// Preserve the original overread and the consecutive declarations; keep the
/// package loaded while its installed clips remain in playback.
#include "assets/rooms/shelter/b3_incinerator_control_room/companion_bank.inc"

/// Unplayed duplicate companion clip-47 request retained in the bank-copy span.
static AnimationPlayRequest _gActor142600UnusedCompanionAnimation47 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Script playback copies requests and resolves the live receiver bank before
// dispatch. Stored selectors stay unchanged, including in the raw bank copies.

/// Resets companion scene clip 47 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation47 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 48 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation48 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 49 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation49 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 50 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation50 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 51 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation51 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 52 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation52 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 53 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation53 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player scene clips installed at extended bank indices 47..59.
///
/// The 32-word copy reads 19 words past these thirteen pointers: both unused
/// clip-47 requests, clip 48's request and the first four words of clip 49's.
/// Those words are not playable clips. Preserve the original overread and the
/// consecutive declarations; keep the package loaded during scene playback.
/// This scene plays only extension indices 48..50 and 54..58.
#include "assets/rooms/shelter/b3_incinerator_control_room/player_bank.inc"

/// Two unplayed clip-47 reset requests retained in the player's bank-copy span.
static AnimationPlayRequest _gActor142600UnusedPlayerAnimation47Requests[] = {
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Resets player scene clip 48 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation48 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 49 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation49 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 50 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation50 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the player's equipped-bank idle clip with world collision enabled.
static AnimationPlayRequest _gActor142600PlayerIdleAnimation = { { .index = 1 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends to the player's equipped-bank idle over 20 frames, disabling world collision.
static AnimationPlayRequest _gActor142600PlayerIdleBlendAnimation = { { .index = 1 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion idle with world collision enabled, including the skip path.
///
/// The stored selector 6 is retained, but script dispatch replaces it with the
/// current companion's bank index in a local copy, as for the other requests.
static AnimationPlayRequest _gActor142600CompanionIdleAnimation = { { .index = 6 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Unplayed player clip-51..53 reset requests with world collision disabled.
static AnimationPlayRequest _gActor142600UnusedPlayerAnimations51To53[] = {
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Blends over 10 frames to player scene clip 54 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation54 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends over 10 frames to player scene clip 55 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation55 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends over 10 frames to player scene clip 56 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation56 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 57 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation57 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 58 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation58 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Retained player scene-clip 59 reset request, with no playback consumer.
///
/// Bank selector 1 is the player seed; collision is disabled and the reset
/// ignores blend duration. This record is outside the scene bank-copy span.
static AnimationPlayRequest _gActor142600UnusedPlayerAnimation59 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs 32 words from the player's scene table, including its documented overread.
///
/// Sent before scene playback; the loaded character/weapon bank must be writable.
/// The copied scene-set pointers borrow this package for their playback lifetime.
static AnimationBankCopyRequest _gActor142600PlayerAnimationBankExtensionCopy = { _gActor142600PlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Installs 32 words from the companion's scene table, including its documented overread.
///
/// Sent before scene playback; the loaded companion bank must be writable.
/// The copied scene-set pointers borrow this package for their playback lifetime.
static AnimationBankCopyRequest _gActor142600CompanionAnimationBankExtensionCopy = { _gActor142600CompanionAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement for the captioned scene, in room units with yaw 2275/4096 turn.
static ActorTransform _gActor142600PlayerScenePlacement = { { -5860, 0, 1540, 0 }, { 0, 2275, 0, 0 } };

/// Companion placement for the captioned scene, in room units facing a quarter turn.
static ActorTransform _gActor142600CompanionScenePlacement = { { -6440, 0, 110, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's view-5 exit placement shared by normal completion and skipping.
///
/// Position is in room-coordinate units; yaw is half a turn. Placement messages
/// consume this borrowed record synchronously while the package is loaded.
static ActorTransform _gActor142600PlayerExitPlacement = { { -6000, 0, 948, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Companion's view-5 exit placement shared by normal completion and skipping.
///
/// Position is in room-coordinate units; yaw is half a turn. Placement messages
/// consume this borrowed record synchronously while the package is loaded.
static ActorTransform _gActor142600CompanionExitPlacement = { { -6442, 0, 1540, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

EvsCommand gActor142600IncineratorArrivalScript[76] = {
    // Install both scene clip tables before the first CAP cue releases playback.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_142600_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142600PlayerAnimationBankExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142600CompanionAnimationBankExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleBlendAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142600_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_142600_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor142600IncineratorArrivalSkipScript[15] = {
    // Cover scene cleanup and exit placement with a black fade.
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_142600_FADE_DEFAULT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142600_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};
