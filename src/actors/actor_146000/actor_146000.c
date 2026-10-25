#include "actors/actor_146000.h"

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/dryfield_night_water_hole.h"

/// Scene clip indices in the current weapon or companion bank; both extensions start at 47.
enum {
    ACTOR_146000_IDLE_ANIMATION     = 1,
    ACTOR_146000_SCENE_ANIMATION_47 = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_146000_SCENE_ANIMATION_48 = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_146000_SCENE_ANIMATION_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_146000_SCENE_ANIMATION_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_146000_SCENE_ANIMATION_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_146000_SCENE_ANIMATION_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_146000_SCENE_ANIMATION_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_146000_SCENE_ANIMATION_54 = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_146000_SCENE_BLEND_FRAMES = 10,
};

/// Water-hole presentation selectors and the departure handshake consumed by the scene task.
enum {
    ACTOR_146000_CAP_AFTER_BURNER       = 3,
    ACTOR_146000_CAP_CONVERSATION       = 4,
    ACTOR_146000_SCRIPT_EVENT_FINISHED  = 2,
    ACTOR_146000_WEAPON_TARGETS_BOTH    = 2,
    ACTOR_146000_CAP_DIRECT_VIEW_IDS    = 1,
    ACTOR_146000_CONVERSATION_EXIT_VIEW = 5,
    ACTOR_146000_SKIP_EXIT_VIEW         = 3,
    ACTOR_146000_APPROACH_SOUND_SCRIPT  = 0x53200006,
};

static AnimationSet _gAyaSeeFlintClip;
static AnimationSet _gAyaMayaWasMySisterClip;
static AnimationSet _gAyaSayKyleClip;
static AnimationSet _gAyaExplainStartClip;
static AnimationSet _gAyaExplainClip;
static AnimationSet _gAyaYouToo1Clip;
static AnimationSet _gAyaYouToo2Clip;
static AnimationSet _gAyaConverseClip;
static AnimationSet _gKyleTalkStartClip;
static AnimationSet _gKyleTalkClip;
static AnimationSet _gKyleTalkEndClip;
static AnimationSet _gDryfieldNightWaterHoleCompanionPromiseToWaitClip;
static AnimationSet _gKyleMadiganAnim04Clip;
static AnimationSet _gKyleListenClip;
static AnimationSet _gPierceAnim04Clip;
static void         _actor146000WaterHoleSceneTask(Task* task);

#include "assets/rooms/dryfield/night_water_hole/shelter_departure_clips.inc"

TaskDesc gActor146000WaterHoleSceneTasks[2] = {
    // The unused entry retains the original two-argument enemy callback address.
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, (TaskFunc)enemyDestroy, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor146000WaterHoleSceneTask, { .value = 0 } },
};

/// Companion scene clips installed at extended bank indices 47..53.
///
/// The fixed 32-word copy reads 25 words past these seven pointers, through the
/// five complete requests from `_gActor146000CompanionPlayAnimation47` to
/// `_gActor146000CompanionPlayAnimation51`. Those copied words are not clips;
/// only indices 47..53 are played. Preserve that original overread and keep the
/// package loaded while either the scripts or installed clip pointers are live.
#include "assets/rooms/dryfield/night_water_hole/companion_scene_bank.inc"

// Script playback copies each request and replaces source.index with the current
// receiver's bank index. Stored selectors remain unchanged, including in raw copies.

/// Resets companion scene clip 47 with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation47 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets companion scene clip 48 with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation48 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends to companion scene clip 49 over ten frames with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionBlendAnimation49 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_49, ANIMATION_BLEND_INTERPOLATE, ACTOR_146000_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets companion scene clip 50 with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation50 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Retained reset request for companion scene clip 51; copied as raw words but never played.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation51 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets companion scene clip 52 with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation52 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets companion scene clip 53 with world collision enabled.
static AnimationPlayRequest _gActor146000CompanionPlayAnimation53 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player scene clips installed at extended bank indices 47..54.
///
/// The fixed 32-word copy reads 24 words past these eight pointers, through
/// `_gActor146000PlayerPlayAnimation47`, `_gActor146000PlayerPlayAnimation48`,
/// `_gActor146000PlayerPlayAnimation49` and the first nine words of
/// `_gActor146000PlayerPlayAnimations50To51`. Those copied words are not clips;
/// only indices 47..54 are played. Preserve that original overread and keep the
/// package loaded while either the scripts or installed clip pointers are live.
#include "assets/rooms/dryfield/night_water_hole/player_scene_bank.inc"

/// Resets player scene clip 47 with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation47 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets player scene clip 48 with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation48 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets player scene clip 49 with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation49 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Retained player reset requests for clips 50..51, with world collision enabled.
///
/// Neither is played. The bank copy includes all of clip 50's request and all
/// but the last word of clip 51's request as unused extension entries.
static AnimationPlayRequest _gActor146000PlayerPlayAnimations50To51[2] = {
    { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Resets player scene clip 52 with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation52 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets player scene clip 53 with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation53 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Retained reset request for player scene clip 54, with no playback consumer.
static AnimationPlayRequest _gActor146000PlayerPlayAnimation54 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends to player scene clip 54 over ten frames with world collision enabled.
static AnimationPlayRequest _gActor146000PlayerBlendAnimation54 = { { .index = 1 }, ACTOR_146000_SCENE_ANIMATION_54, ANIMATION_BLEND_INTERPOLATE, ACTOR_146000_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the current weapon bank's player idle clip, enabling world collision.
static AnimationPlayRequest _gActor146000PlayerIdleAnimation = { { .index = 1 }, ACTOR_146000_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the selected companion bank's idle clip, enabling world collision.
static AnimationPlayRequest _gActor146000CompanionIdleAnimation = { { .index = 6 }, ACTOR_146000_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Copies 32 words from the player scene table into the current weapon bank's extension.
///
/// Includes the adjacent request words described at the source table. Dispatch
/// borrows this request; copied clip pointers remain borrowed through playback.
static AnimationBankCopyRequest _gActor146000PlayerAnimationBankCopy = { _gActor146000PlayerAnimationBankExtension, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Copies 32 words from the companion scene table into its selected bank's extension.
///
/// Includes the adjacent request words described at the source table. Dispatch
/// borrows this request; copied clip pointers remain borrowed through playback.
static AnimationBankCopyRequest _gActor146000CompanionAnimationBankCopy = { _gActor146000CompanionAnimationBankExtension, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement before the post-burner walk, in world units and facing negative X.
static ActorTransform _gActor146000PlayerWalkStart = { { 4668, 0, -1337, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player walk destination in world units; the move handler reads XYZ only.
///
/// The unused orientation remains part of the original full placement record.
static ActorTransform _gActor146000PlayerWalkDestination = { { 5885, 0, -1300, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player conversation/skip placement in world units, facing the companion along negative X.
static ActorTransform _gActor146000PlayerConversationPlacement = { { 10270, 0, -1000, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Companion conversation/skip placement in world units, facing the player along positive X.
static ActorTransform _gActor146000CompanionConversationPlacement = { { 9020, 0, -1000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Common water-hole conversation and departure script, entered directly or after the lead-in.
///
/// Starts CAP command 4 and installs both clip extensions before playback.
/// Publishes event state 2 to request departure, then restores actor presentation.
/// Command and clip resources must remain loaded while their tasks use them.
static EvsCommand _gActor146000WaterHoleConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_146000_CAP_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146000PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146000CompanionAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = ACTOR_146000_CAP_DIRECT_VIEW_IDS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000PlayerConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000CompanionConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Cue-driven actor playback uses the installed scene clips.
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_146000_WEAPON_TARGETS_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerBlendAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionBlendAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerBlendAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionBlendAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerPlayAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerPlayAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionPlayAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerPlayAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerPlayAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Publish departure before the trailing presentation restoration.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_146000_CONVERSATION_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACTOR_146000_SCRIPT_EVENT_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_146000_WEAPON_TARGETS_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000CompanionConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Post-burner lead-in: approaches the conversation position, then jumps to the common script.
///
/// Starts CAP command 3 and installs the player clip extension. Retains two
/// zero-frame waits so message/motion updates occur before the next instruction.
/// Borrows all commands, payloads and clips until the jumped script finishes.
static EvsCommand _gActor146000WaterHoleAfterBurnerScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_146000_CAP_AFTER_BURNER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146000PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = ACTOR_146000_CAP_DIRECT_VIEW_IDS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Place the player, then yield around the asynchronous approach request.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000PlayerWalkStart } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_146000_WEAPON_TARGETS_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_146000_APPROACH_SOUND_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor146000PlayerWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000PlayerPlayAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Continue in the conversation without pushing a script return.
    { EVENT_SCRIPT_OPCODE_JUMP, { .commands = _gActor146000WaterHoleConversationScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Shared skip path: releases CAP control, requests departure and restores actor presentation.
///
/// Used by both entry paths; publishes event state 2 before its final waits and
/// restoration. Keep this stream and its borrowed placements live through END.
static EvsCommand _gActor146000WaterHoleSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Abort presentation and publish the same departure handshake as normal play.
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACTOR_146000_SCRIPT_EVENT_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_146000_SKIP_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_146000_WEAPON_TARGETS_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146000CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000CompanionConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146000PlayerConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the water-hole scene variant and reloads the night driveway on completion.
///
/// Starts the skippable script selected by the burner-defeated flag, preserving
/// the HUD for the pre-burner variant and selecting saved warp 2 or 4. Waits for
/// event state 2, stops nonambient sound, applies the room’s saved updates and
/// requests reload to night driveway room 2. Begin at task state 0 with both
/// overlays and scene resources live. This workless task kills itself at handoff.
static void _actor146000WaterHoleSceneTask(Task* task)
{
    enum {
        ACTOR_146000_SCENE_START                     = 0,
        ACTOR_146000_SCENE_WAIT                      = 1,
        ACTOR_146000_SCENE_RELOAD                    = 2,
        ACTOR_146000_EVENT_FINISHED                  = 2,
        ACTOR_146000_COMPANION_SCHEDULE_AFTER_BURNER = 7,
        ACTOR_146000_WARP_AFTER_BURNER               = 4,
        ACTOR_146000_WARP_BEFORE_BURNER              = 2,
    };

    s32 state;
    s8  eventState;

    state = task->state;
    switch (state) {
        case ACTOR_146000_SCENE_START:
            if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
                evsStartScriptWithSkip(_gActor146000WaterHoleAfterBurnerScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor146000WaterHoleSkipScript);
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, ACTOR_146000_COMPANION_SCHEDULE_AFTER_BURNER);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = ACTOR_146000_WARP_AFTER_BURNER;
            } else {
                evsStartScriptWithSkip(_gActor146000WaterHoleConversationScript, EVENT_SCRIPT_HUD_KEEP, _gActor146000WaterHoleSkipScript);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = ACTOR_146000_WARP_BEFORE_BURNER;
            }
            task->state++;
            return;
        case ACTOR_146000_SCENE_WAIT:
            eventState = gGameSession->eventState;
            if (eventState == ACTOR_146000_EVENT_FINISHED) {
                task->state = eventState;
            }
            return;
        case ACTOR_146000_SCENE_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
            areaApplySavedUpdates(gDryfieldNightWaterHoleSceneAreaUpdates);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = state;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}
