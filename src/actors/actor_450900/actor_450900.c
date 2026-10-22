#include "actors/actor_450900.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_growth_room.h"

#include "mapui/map_neo_ark.h"

/// Number of periodic player responses whose seventy-tick release point has passed.
///
/// Zeroed when this overlay loads; increments without saturation and is not
/// reset by task initialization. No consumer uses the count to change behavior.
static s32 _gActor450900PlayerResponseCount;

/// Borrowed handle of the growth-room task that blends the player's head aim.
///
/// The scene expects one distress task to spawn one head-aim task. Normal exit
/// clears this handle; initialization failure kills the task without clearing
/// it. Script callbacks require any non-NULL handle to still name a live task.
static Task* _gActor450900HeadAimTask;

static AnimationSet _gShelterB6NurseryCompanionDistressClip;
static AnimationSet _gShelterB6NurseryCompanionConversationReactionClip;
static AnimationSet _gShelterB6NurseryCompanionHurryUpClip;
static AnimationSet _gShelterB6GrowthRoomCompanionAnim03Clip;
static AnimationSet _gShelterB6GrowthRoomCompanionPostChoiceClip;
static AnimationSet _gShelterB6GrowthRoomCompanionConversationClip;
static AnimationSet _gAyaShooting1Clip;
static AnimationSet _gAyaShooting2Clip;
static AnimationSet _gAyaThisWayReplyClip;
static AnimationSet _gAyaHurryUpClip;
static AnimationSet _gShelterB6GrowthRoomPlayerResponseClip;
static AnimationSet _gShelterB6GrowthRoomPlayerEntryClip;
extern AnimationSet gShelterB6GrowthRoomPlayerChoiceStartClip;
extern AnimationSet gAyaTurnPaleClip;
extern AnimationSet gAyaItsNothingClip;
static AnimationSet _gAyaConverseClip;

// Request templates resolve their initial bank selector before playback. Extended
// clip ids index the tables installed at ANIMATION_BANK_BASE_SET_COUNT.
enum {
    ACTOR_450900_INITIAL_ANIMATION_BANK                    = 1,
    ACTOR_450900_IDLE_ANIMATION                            = 1,
    ACTOR_450900_COMPANION_DISTRESS_ANIMATION              = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_450900_COMPANION_CONVERSATION_REACTION_ANIMATION = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_450900_COMPANION_POST_CHOICE_ANIMATION           = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_450900_COMPANION_CONVERSATION_ANIMATION          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_450900_PLAYER_RESPONSE_ANIMATION                 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_450900_PLAYER_ENTRY_ANIMATION                    = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_450900_PLAYER_CHOICE_START_ANIMATION             = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_450900_PLAYER_CHOICE_MIDDLE_ANIMATION            = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_450900_PLAYER_CHOICE_END_ANIMATION               = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_450900_COMPANION_BLEND_FRAMES                    = 10,
    ACTOR_450900_PLAYER_RESPONSE_BLEND_FRAMES              = 12,
    ACTOR_450900_PLAYER_CHOICE_BLEND_FRAMES                = 8,
    ACTOR_450900_SCENE_TASK_PRIORITY                       = 192,
    ACTOR_450900_HEAD_AIM_TASK_PRIORITY                    = 97,
};

// CAP slots and the event-state handshake belong to these scripts. Durations
// count unfrozen interpreter updates, except the sound-system music fade.
enum {
    ACTOR_450900_CAP_ENTRY                         = 10,
    ACTOR_450900_CAP_BEFORE_DISTRESS_CONVERSATION  = 3,
    ACTOR_450900_CAP_DISTRESS_CONVERSATION         = 4,
    ACTOR_450900_CAP_POST_CHOICE_CONVERSATION      = 5,
    ACTOR_450900_ENTRY_VIEW                        = 7,
    ACTOR_450900_INTERACTION_VIEW                  = 2,
    ACTOR_450900_FADE_TICKS                        = 30,
    ACTOR_450900_SKIP_FADE_WAIT_TICKS              = 8,
    ACTOR_450900_ENTRY_POSITION_DELAY_TICKS        = 2,
    ACTOR_450900_CHOICE_START_HOLD_TICKS           = 23,
    ACTOR_450900_CHOICE_END_HOLD_TICKS             = 16,
    ACTOR_450900_DEPARTURE_MUSIC_FADE_TICKS        = 120,
    ACTOR_450900_DEPARTURE_FADE_READY              = 2,
    ACTOR_450900_DEPARTURE_AFTER_SIGNAL_WAIT_TICKS = 900,
    ACTOR_450900_DEPARTURE_SOUND                   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 1),
    ACTOR_450900_PLAYER_PRESENTATION_CLIP_32       = 0,
    ACTOR_450900_PLAYER_PRESENTATION_CLIP_33       = 1,
};

// Shared timing and callback selectors for this package's growth-room scenes.
enum {
    ACTOR_450900_REACTION_PERIOD_TICKS          = 210,
    ACTOR_450900_COMPANION_DISTRESS_START_TICKS = 780,
    ACTOR_450900_HEAD_AIM_DISABLE               = 1,
    ACTOR_450900_HEAD_AIM_ENABLE                = 2,
    ACTOR_450900_COMPANION_VOICE_RANDOM         = 0,
    ACTOR_450900_COMPANION_VOICE_FIXED          = 1,
    ACTOR_450900_DEPARTURE_COMMAND              = 11,
};

static void _actor450900CompanionDistressTask(Task* task);
static void _actor450900PlayerReactionTask(Task* task);
static void _actor450900GrowthRoomActionTask(Task* task);
static void _actor450900DepartureTask(Task* task);
static void _actor450900ControlHeadAim(s32 command);
static void _actor450900HeadAimTask(Task* task);
static void _actor450900SetActorControl(u8 actorControl);
static void _actor450900PlayCompanionVoice(s32 voiceSelector);
static void _actor450900PreparePlayerFacingCompanion(void);

#include "assets/rooms/shelter/b6_growth_room/eve_distress_clips.inc"

/// Records whether this distress task has reached an eligible distress update.
///
/// Reset to 0 on task initialization and latched to 1 once timing, HP and the
/// conversation gate permit distress. No reader of this byte is established.
static s8 _gActor450900CompanionDistressStarted = false;

/// One-shot latch for the first conversation after the confirming growth-room choice.
///
/// Initially 0 when this overlay loads, set to 1 before starting the post-choice
/// script, and never reset while loaded. Also suppresses further distress.
static s32 _gActor450900PostChoiceConversationStarted = false;

TaskDesc gActor450900Tasks[ACTOR_450900_TASK_COUNT] = {
    { { { TASK_BODY_NONE, ACTOR_450900_SCENE_TASK_PRIORITY } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450900_SCENE_TASK_PRIORITY } }, _actor450900CompanionDistressTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450900_SCENE_TASK_PRIORITY } }, _actor450900PlayerReactionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450900_SCENE_TASK_PRIORITY } }, _actor450900GrowthRoomActionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450900_SCENE_TASK_PRIORITY } }, _actor450900DepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450900_HEAD_AIM_TASK_PRIORITY } }, _actor450900HeadAimTask, { .value = 0 } },
};

/// Ten playable player clips installed at bank indices 47..56.
///
/// The copy request reads 32 words: these ten pointers followed by 22 words
/// from the adjacent companion table, copy requests and play requests. Keep
/// those objects in data order. Only the first ten copied words are clips;
/// playback borrows their data until the installed animations are finished.
#include "assets/rooms/shelter/b6_growth_room/player_bank.inc"

/// Six playable companion clips installed at bank indices 47..52.
///
/// The copy request reads 32 words: these six pointers followed by 26 words
/// from the adjacent copy and play requests. Keep those objects in data order.
/// Only the first six copied words are clips; playback borrows their data until
/// the installed animations are finished.
#include "assets/rooms/shelter/b6_growth_room/companion_bank.inc"

/// Installs the companion's six clips and adjacent words into its 32-word bank extension.
static AnimationBankCopyRequest _gActor450900CompanionAnimationBankCopy = { _gActor450900CompanionAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Installs the player's ten clips and adjacent words into its 32-word bank extension.
static AnimationBankCopyRequest _gActor450900PlayerAnimationBankCopy = { _gActor450900PlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Unplayed clip-47 reset preset at the start of the companion request run.
///
/// The neighboring dispatched requests suggest the companion as receiver.
/// Both 32-word bank-extension copies read this complete record as opaque
/// words, so retain its data position and disabled world-collision word.
static AnimationPlayRequest _gActor450900UnusedCompanionClip47Request = {
    .source               = { .index = ACTOR_450900_INITIAL_ANIMATION_BANK },
    .animationId          = ACTOR_450900_COMPANION_DISTRESS_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Writable companion reaction request played at each eligible distress period.
///
/// The live companion bank selector is written in place before dispatch. This
/// resets clip 47, disables world collision and precedes the damage message.
static AnimationPlayRequest _gActor450900CompanionDistressAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_DISTRESS_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion clip 48 played during the distressed conversation before its fixed voice cue.
///
/// The script resolves the bank in a copy, blends for ten frames and disables
/// world collision; the following cue restores the companion's idle clip.
static AnimationPlayRequest _gActor450900CompanionConversationReactionAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_CONVERSATION_REACTION_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_COMPANION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    ACTOR_450900_COMPANION_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_450900_COMPANION_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
};

/// Unplayed clip-49/50 reset presets within the companion request run.
///
/// The receiver is inferred from neighboring dispatched requests. The
/// companion's 32-word bank copy reads all of entry 0 and the source and clip
/// words of entry 1 as opaque data; retain this two-record span in data order.
static AnimationPlayRequest _gActor450900UnusedCompanionClip49To50Requests[2] = {
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_CLIP_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_CLIP_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Companion clip 51 played at the first cue of the post-choice conversation.
///
/// The script resolves the bank in a copy, blends for ten frames and disables
/// world collision before the next cue requests a turn.
static AnimationPlayRequest _gActor450900CompanionPostChoiceAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_POST_CHOICE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_COMPANION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    ACTOR_450900_PLAYER_CLIP_47 = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_450900_PLAYER_CLIP_48 = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_450900_PLAYER_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_450900_PLAYER_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_450900_PLAYER_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
};

/// Five unplayed reset presets preceding the dispatched player clip-51 request.
///
/// Stored ids are 47, 47, 48, 49 and 50; only entry 0 disables world collision.
/// The player receiver is inferred from position. No consumer establishes
/// indexing or the original grouping of this retained five-record span.
static AnimationPlayRequest _gActor450900UnusedPlayerClip47To50Requests[5] = {
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CLIP_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CLIP_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CLIP_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CLIP_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CLIP_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Writable player clip 51 request used by the periodic growth-room response.
///
/// Its weapon-bank selector is filled in place before dispatch. Blends for
/// twelve frames and restores world collision; control is released later.
static AnimationPlayRequest _gActor450900PlayerPeriodicResponseAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_RESPONSE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_PLAYER_RESPONSE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Unplayed clip-52 preset with a ten-frame blend and world collision disabled.
///
/// Its position between the dispatched player clips 51 and 53 suggests the
/// player receiver; no dispatch establishes that interpretation.
static AnimationPlayRequest _gActor450900UnusedPlayerClip52Request = {
    .source               = { .index = ACTOR_450900_INITIAL_ANIMATION_BANK },
    .animationId          = ACTOR_450900_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 53 used by the growth-room entry scene.
///
/// The script resolves the weapon bank in a copy, resets playback and restores
/// world collision while the entry CAP sequence runs.
static AnimationPlayRequest _gActor450900PlayerEntryAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_ENTRY_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player clip 54 starting the confirmed choice's animation sequence.
///
/// The script resolves the weapon bank in a copy, blends for eight frames and
/// disables world collision. Clip 55 replaces it after 23 interpreter ticks.
static AnimationPlayRequest _gActor450900PlayerChoiceStartAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CHOICE_START_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_PLAYER_CHOICE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 55 between the confirmed choice's start and final CAP cue.
///
/// The script resolves the weapon bank in a copy and resets playback while
/// leaving world collision disabled.
static AnimationPlayRequest _gActor450900PlayerChoiceMiddleAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CHOICE_MIDDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 56 played at the final cue of the confirmed choice's animation sequence.
///
/// The script resolves the weapon bank in a copy and resets playback while
/// leaving world collision disabled; the script then waits sixteen ticks.
static AnimationPlayRequest _gActor450900PlayerChoiceEndAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_PLAYER_CHOICE_END_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion clip 52 shared by the early and distressed conversations.
///
/// The script resolves the bank in a copy, blends for ten frames and disables
/// world collision before awaiting the next CAP cue.
static AnimationPlayRequest _gActor450900CompanionConversationAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_COMPANION_CONVERSATION_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_COMPANION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player idle request used to settle the entry scene and its skip target.
///
/// The script resolves the weapon bank in a copy, resets base clip 1 and
/// restores world collision.
static AnimationPlayRequest _gActor450900PlayerIdleAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Unplayed clip-1 preset with a ten-frame blend and world collision enabled.
///
/// It follows the dispatched player idle reset and precedes the companion's
/// idle requests. The player receiver is inferred from that position.
static AnimationPlayRequest _gActor450900UnusedPlayerIdleBlendRequest = {
    .source               = { .index = ACTOR_450900_INITIAL_ANIMATION_BANK },
    .animationId          = ACTOR_450900_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion idle request used by entry setup and the early conversation's closing cue.
///
/// The script resolves the bank in a copy, resets base clip 1 and disables
/// world collision.
static AnimationPlayRequest _gActor450900CompanionIdleAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion idle request used after a distress reaction or distressed conversation.
///
/// Blends to base clip 1 for ten frames with world collision disabled. The
/// periodic task updates its bank selector in place; the script resolves a copy.
static AnimationPlayRequest _gActor450900CompanionBlendedIdleAnimation = { { .index = ACTOR_450900_INITIAL_ANIMATION_BANK }, ACTOR_450900_IDLE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_450900_COMPANION_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion placement for entry, its skip target and the early conversation.
///
/// Position uses room coordinate units; yaw is one quarter turn in the
/// 4096-unit angle scale. Placement reads both position and rotation.
static ActorTransform _gActor450900CompanionEntryTransform = { { 3450, 0, 6378, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Companion turn request sent at the second post-choice conversation cue.
///
/// Only yaw is used: one half turn in the 4096-unit angle scale. The turn
/// message ignores the position even though the payload is a full transform.
static ActorTransform _gActor450900CompanionPostChoiceFacing = { { 3300, 0, 6378, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player placement shared by growth-room entry and its skip target.
///
/// Position uses room coordinate units; yaw is negative one quarter turn in
/// the 4096-unit angle scale. Placement reads both position and rotation.
static ActorTransform _gActor450900PlayerEntryTransform = { { 750, 0, 3710, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

EvsCommand gActor450900EntryScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900CompanionAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor450900PlayerEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450900_CAP_ENTRY }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6GrowthRoomResetCollisionBox }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450900_ENTRY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerEntryAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_ENTRY_POSITION_DELAY_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor450900CompanionEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450900_INTERACTION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450900EntrySkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_SKIP_FADE_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor450900CompanionEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor450900PlayerEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450900_INTERACTION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_SKIP_FADE_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Writable yaw request that makes the player face the companion during conversation.
///
/// Updated from the live roots immediately before each turn message. Yaw is
/// wrapped to 0..4095; the turn message ignores the zero position fields.
static ActorTransform _gActor450900PlayerFaceCompanionTransform = { { 0, 0, 0, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Growth-room conversation before distress timing reaches its start threshold.
///
/// Places the companion, installs both clip banks and starts CAP command 3.
/// Faces the player toward the companion, enables head aiming, plays the
/// companion's cue-driven clips, then resumes actors and releases the player.
static EvsCommand _gActor450900BeforeDistressConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor450900CompanionEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450900_CAP_BEFORE_DISTRESS_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900CompanionAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .pointer = &_gActor450900PlayerFaceCompanionTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_32 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionConversationAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_33 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Growth-room conversation once distress timing reaches its start threshold.
///
/// Starts CAP command 4, faces the player toward the companion and pauses
/// actors with head aiming enabled. Companion clip and voice cues run before
/// idle restoration, head-aim disable, actor resumption and player release.
static EvsCommand _gActor450900DistressConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450900_CAP_DISTRESS_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .pointer = &_gActor450900PlayerFaceCompanionTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_32 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionConversationAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionConversationReactionAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900PlayCompanionVoice }, { .value = ACTOR_450900_COMPANION_VOICE_FIXED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionBlendedIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_33 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// One-shot growth-room conversation after the confirming choice.
///
/// Restores the collision box, starts CAP command 5 and installs both clip
/// banks. Faces the player toward the companion, pauses actors and enables
/// head aiming, then coordinates the companion clip and half-turn with CAP
/// cues before resuming actors and releasing player control.
static EvsCommand _gActor450900PostChoiceConversationScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6GrowthRoomResetCollisionBox }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450900_CAP_POST_CHOICE_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900CompanionAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .pointer = &_gActor450900PlayerFaceCompanionTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_32 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900CompanionPostChoiceAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .pointer = &_gActor450900CompanionPostChoiceFacing } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION }, { .value = ACTOR_450900_PLAYER_PRESENTATION_CLIP_33 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Player animation follow-up for the confirming growth-room choice.
///
/// Installs the player's clips and plays clips 54, 55 and 56 around CAP cues
/// and frame waits. The owning choice task resumes play after the script ends.
static EvsCommand _gActor450900ChoiceActionScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor450900PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerChoiceStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_CHOICE_START_HOLD_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerChoiceMiddleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450900PlayerChoiceEndAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_CHOICE_END_HOLD_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Departure fade that signals when the companion-removal task can reload the area.
///
/// Fades music, starts the room sound and secondary fade, then sends event
/// state 2 after two thirty-tick waits. Holds for 900 further interpreter ticks
/// if the session reload has not already discarded the scene.
static EvsCommand _gActor450900DepartureFadeScript[] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = ACTOR_450900_DEPARTURE_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_450900_DEPARTURE_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACTOR_450900_DEPARTURE_FADE_READY }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_450900_DEPARTURE_AFTER_SIGNAL_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Advances growth-room distress timing and periodically animates and hurts the companion.
///
/// State 0 spawns the tracked head-aim task. State 1 advances the persistent
/// counter only outside CAP/event playback and actor pauses, except in demo 11.
/// From tick 780, while the companion has HP and the conversation latch is zero,
/// every 210 ticks plays a random spatial voice and a power-16 damage reaction;
/// 60 ticks later restores its idle pose. Requires live companion/model and
/// gameplay sound, animation and damage resources; messages borrow the requests
/// synchronously. Other states do nothing.
static void _actor450900CompanionDistressTask(Task* task)
{
    enum {
        ACTOR_450900_DISTRESS_INIT             = 0,
        ACTOR_450900_DISTRESS_RUN              = 1,
        ACTOR_450900_DISTRESS_IDLE_DELAY_TICKS = 60,
        ACTOR_450900_DISTRESS_DEMO_SCENE       = 11,
        ACTOR_450900_DISTRESS_ATTACK_KEY       = DAMAGE_ATTACK_CATEGORY | 16,
    };
    GfxCoord* companionCoord;
    s32       state;
    s32       distressTicks;
    s8        pan;
    s8        depth;
    Task*     companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    state         = task->state;
    switch (state) {
        case ACTOR_450900_DISTRESS_INIT:
            _gActor450900CompanionDistressStarted = false;
            _gActor450900HeadAimTask              = taskSpawnFromTable(gActor450900Tasks, ACTOR_450900_TASK_HEAD_AIM, 0, 0);
            task->state                           = task->state + 1;
            break;
        case ACTOR_450900_DISTRESS_RUN:
            if (capIsBusy() != 0) {
                break;
            }
            if (gGameSession->eventState != 0) {
                break;
            }
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                break;
            }
            // Count eligible updates across visits; demo playback retains the counter.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_450900_DISTRESS_DEMO_SCENE) {
                gMapNeoArkCompanionDistressTicks = gMapNeoArkCompanionDistressTicks + 1;
            }
            distressTicks = gMapNeoArkCompanionDistressTicks - ACTOR_450900_COMPANION_DISTRESS_START_TICKS;
            if (_gActor450900PostChoiceConversationStarted == false && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0 && distressTicks >= 0) {
                _gActor450900CompanionDistressStarted = state;
                if (distressTicks % ACTOR_450900_REACTION_PERIOD_TICKS == 0) {
                    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
                    pan            = (s8)worldCoordGetOriginAudioPan(companionCoord);
                    depth          = (s8)worldCoordGetOriginAudioDepth(companionCoord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
                    }
                    companionWriteAnimationBankIndex(&_gActor450900CompanionDistressAnimation.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &_gActor450900CompanionAnimationBankCopy, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &_gActor450900CompanionDistressAnimation, 0);
                    taskMessageDispatch(companionTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, ACTOR_450900_DISTRESS_ATTACK_KEY, 0);
                } else if (distressTicks % ACTOR_450900_REACTION_PERIOD_TICKS == ACTOR_450900_DISTRESS_IDLE_DELAY_TICKS) {
                    companionWriteAnimationBankIndex(&_gActor450900CompanionBlendedIdleAnimation.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &_gActor450900CompanionBlendedIdleAnimation, 0);
                }
            }
            break;
    }
}

/// Plays the player's periodic growth-room response after the distress timer reaches 1110.
///
/// Outside CAP/event playback and actor pauses, starts a spatial voice and the
/// extended response animation every 210 counter ticks. Seventy ticks later
/// increments the completion counter and releases scripted player control.
/// Requires a live player TMD and this package's animation and sound resources.
/// State 0 clears the task countdown; state 1 runs; other states do nothing.
static void _actor450900PlayerReactionTask(Task* task)
{
    enum {
        ACTOR_450900_PLAYER_REACTION_INIT        = 0,
        ACTOR_450900_PLAYER_REACTION_RUN         = 1,
        ACTOR_450900_PLAYER_REACTION_START_TICKS = 1110,
        ACTOR_450900_PLAYER_RESPONSE_TICKS       = 70,
    };
    GfxCoord* playerCoord;
    Task*     playerTask;
    s32       stateOrReactionCount;
    s8        pan;
    s8        depth;

    playerTask           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    stateOrReactionCount = task->state;
    switch (stateOrReactionCount) {
        case ACTOR_450900_PLAYER_REACTION_INIT:
            task->killCountdown = 0;
            task->state         = task->state + 1;
            return;
        case ACTOR_450900_PLAYER_REACTION_RUN:
            if ((capIsBusy() == 0) && (gGameSession->eventState == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) && ((gMapNeoArkCompanionDistressTicks - ACTOR_450900_PLAYER_REACTION_START_TICKS) >= 0)) {
                if ((gMapNeoArkCompanionDistressTicks - ACTOR_450900_PLAYER_REACTION_START_TICKS) % ACTOR_450900_REACTION_PERIOD_TICKS == 0) {
                    playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                    pan         = (s8)worldCoordGetOriginAudioPan(playerCoord);
                    depth       = (s8)worldCoordGetOriginAudioDepth(playerCoord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_2, pan, depth);
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &_gActor450900PlayerAnimationBankCopy, 0);
                    playerActorWriteWeaponAnimationBankIndex(&_gActor450900PlayerPeriodicResponseAnimation.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_PLAY, &_gActor450900PlayerPeriodicResponseAnimation, 0);
                } else if ((gMapNeoArkCompanionDistressTicks - ACTOR_450900_PLAYER_REACTION_START_TICKS) % ACTOR_450900_REACTION_PERIOD_TICKS == ACTOR_450900_PLAYER_RESPONSE_TICKS) {
                    // The shared temporary and equal arms retain the matching shape;
                    // the original counter condition is unproven.
                    stateOrReactionCount = _gActor450900PlayerResponseCount;
                    stateOrReactionCount++;
                    _gActor450900PlayerResponseCount = stateOrReactionCount;
                    if (stateOrReactionCount != 0) {
                        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    } else {
                        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                }
            }
            return;
    }
}

/// Runs the growth-room action choice and its optional player-animation follow-up.
///
/// CAP command 1 pauses actors. Variant 11 sets the persistent action flag,
/// starts command 2 and the player animation script; other variants resume play.
/// The follow-up resumes play when event state returns to zero. Requires this
/// package, the room's CAP commands, and the player model through completion.
static void _actor450900GrowthRoomActionTask(Task* task)
{
    enum {
        ACTOR_450900_ACTION_START             = 0,
        ACTOR_450900_ACTION_WAIT_CHOICE       = 1,
        ACTOR_450900_ACTION_HANDLE_CHOICE     = 2,
        ACTOR_450900_ACTION_WAIT_SCRIPT       = 3,
        ACTOR_450900_ACTION_COMMAND           = 1,
        ACTOR_450900_ACTION_FOLLOW_UP_COMMAND = 2,
        ACTOR_450900_ACTION_FOLLOW_UP_VARIANT = 11,
    };
    switch (task->state) {
        case ACTOR_450900_ACTION_START:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(ACTOR_450900_ACTION_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_ACTION_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_ACTION_HANDLE_CHOICE:
            if (capGetVariantKey() != ACTOR_450900_ACTION_FOLLOW_UP_VARIANT) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                break;
            }
            gameFlagSetNibble(GAME_FLAG_0D8, 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(ACTOR_450900_ACTION_FOLLOW_UP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            evsStartScript(_gActor450900ChoiceActionScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_ACTION_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

/// Offers growth-room departure, then removes the companion and reloads Neo Ark Garden.
///
/// CAP command 11 uses variant 1. A returned key other than 11 resumes the
/// player and kills this task. Key 11 runs the fade script; its event-state 2
/// handshake commits story/route flags and destination room 1, warp 3, before
/// spawning the resident session-load task. Requires this package and the room's
/// CAP and event resources through the handshake. Other task states do nothing.
static void _actor450900DepartureTask(Task* task)
{
    enum {
        ACTOR_450900_DEPARTURE_START            = 0,
        ACTOR_450900_DEPARTURE_WAIT_CHOICE      = 1,
        ACTOR_450900_DEPARTURE_HANDLE_CHOICE    = 2,
        ACTOR_450900_DEPARTURE_WAIT_FADE        = 3,
        ACTOR_450900_DEPARTURE_COMMIT           = 4,
        ACTOR_450900_DEPARTURE_OFFER_VARIANT    = 1,
        ACTOR_450900_DEPARTURE_CONFIRMED_KEY    = 11,
        ACTOR_450900_DEPARTURE_STORY_DIALOGUE   = 8,
        ACTOR_450900_DEPARTURE_DESTINATION_WARP = 3,
        ACTOR_450900_DEPARTURE_DESTINATION_ROOM = 1,
        ACTOR_450900_COMPANION_NONE             = 0,
        ACTOR_450900_DEFAULT_DISPLAY_VARIANT    = 1,
        ACTOR_450900_SESSION_LOAD_TASK_INDEX    = GAME_FLOW_RELOAD_TASK_SLOT,
    };
    switch (task->state) {
        case ACTOR_450900_DEPARTURE_START:
            capStartSequenceSlot(ACTOR_450900_DEPARTURE_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, ACTOR_450900_DEPARTURE_OFFER_VARIANT);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_DEPARTURE_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_HANDLE_CHOICE:
            if (capGetVariantKey() != ACTOR_450900_DEPARTURE_CONFIRMED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            } else {
                evsStartScript(_gActor450900DepartureFadeScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_WAIT_FADE:
            if (gGameSession->eventState == ACTOR_450900_DEPARTURE_FADE_READY) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_COMMIT:
            // Commit the post-companion route before the session-load task reads the save.
            gameFlagSetNibble(GAME_FLAG_COMPANION_3_SCHEDULE, 0);
            gameFlagSetNibble(GAME_FLAG_0FC, 1);
            gameFlagSetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 0);
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1C7, 0);
            gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, 0);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ACTOR_450900_DEPARTURE_STORY_DIALOGUE);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_NEO_ARK_GARDEN;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_450900_DEPARTURE_DESTINATION_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType      = ACTOR_450900_COMPANION_NONE;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_450900_DEPARTURE_DESTINATION_ROOM;
            gDisplayState.spriteVariant                                 = ACTOR_450900_DEFAULT_DISPLAY_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, ACTOR_450900_SESSION_LOAD_TASK_INDEX, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            streamFinishScene();
            taskKill(task);
            break;
    }
}

/// Sets whether the tracked task fades the player's head turn toward the companion.
///
/// Command 1 disables aiming; every other value enables it (scripts use 2).
/// Does nothing without a tracked task. Requires any retained handle to remain
/// live; this changes spawnArg1, leaving task state and lifetime untouched.
static void _actor450900ControlHeadAim(s32 command)
{
    if (_gActor450900HeadAimTask != NULL) {
        if (command == ACTOR_450900_HEAD_AIM_DISABLE) {
            _gActor450900HeadAimTask->spawnArg1.value = 0;
            return;
        }
        _gActor450900HeadAimTask->spawnArg1.value = 1;
    }
}

/// Fades the scene's Q12 head-aim blend weight toward full aiming or zero.
///
/// Requires writable `aim` with `rate` normally in 0..ONE; only that field
/// changes. Nonzero `enabled` adds ONE/8 and caps at ONE; zero subtracts ONE/8
/// and floors at zero. Holding either setting traverses the full range in
/// eight calls. The result narrows to a signed halfword before the selected
/// endpoint check, preserving wraparound for out-of-range inputs.
static inline void _actor450900RampHeadAimWeight(AnimationHeadAim* aim, s32 enabled)
{
    enum { ACTOR_450900_HEAD_AIM_FADE_TICKS = 8 };
    s16 nextWeight;

    if (enabled != 0) {
        nextWeight = aim->rate + ONE / ACTOR_450900_HEAD_AIM_FADE_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextWeight = aim->rate - ONE / ACTOR_450900_HEAD_AIM_FADE_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight < 0) {
            aim->rate = 0;
        }
    }
}

/// Fades the player's head turn toward the companion during growth-room scenes.
///
/// State 0 owns a zeroed `AnimationHeadAim` in `Task::work`, limits yaw to 1/16
/// turn and pitch to 1/8 turn, and immediately runs state 1. Nonzero spawnArg1
/// enables aiming. Both tasks require live TMD skeletons with five coordinates.
/// Other states kill this task, freeing its work and clearing the tracked
/// handle. Allocation failure kills without clearing that handle.
static void _actor450900HeadAimTask(Task* task)
{
    enum {
        ACTOR_450900_HEAD_AIM_INIT        = 0,
        ACTOR_450900_HEAD_AIM_RUN         = 1,
        ACTOR_450900_HEAD_AIM_YAW_LIMIT   = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_450900_HEAD_AIM_PITCH_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8,
    };
    AnimationHeadAim* aim;
    Task*             playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case ACTOR_450900_HEAD_AIM_INIT:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = ACTOR_450900_HEAD_AIM_YAW_LIMIT;
            aim->pitchLimit = ACTOR_450900_HEAD_AIM_PITCH_LIMIT;
            task->state++;
            /* fallthrough */
        case ACTOR_450900_HEAD_AIM_RUN:
            aim = task->work;
            _actor450900RampHeadAimWeight(aim, task->spawnArg1.value);
            animationAimHeadAt(playerTask, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            _gActor450900HeadAimTask = NULL;
            return;
    }
}

/// Supplies a script argument's low byte to the scene's actor-control gate.
///
/// Scripts use SCENE_COMBAT_ACTORS_RUNNING (0) and SCENE_COMBAT_ACTORS_PAUSED
/// (1); other byte values are stored unchanged. This does not change player
/// scripted-control state or resume tasks on its own.
static void _actor450900SetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// Plays a spatial growth-room companion voice from its cached root coordinate.
///
/// Selector 0 randomly chooses voice 1 or 2; every nonzero selector uses voice 3.
/// Requires a live companion TMD, a composed local-to-view matrix, projection
/// scratch space and the room's loaded sound bank. Pan/depth retain signed-byte
/// narrowing before submission; the coordinate is borrowed only during the call.
static void _actor450900PlayCompanionVoice(s32 voiceSelector)
{
    GfxCoord* companionCoord;
    s8        pan;
    s8        depth;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    pan            = (s8)worldCoordGetOriginAudioPan(companionCoord);
    depth          = (s8)worldCoordGetOriginAudioDepth(companionCoord);
    if (voiceSelector != ACTOR_450900_COMPANION_VOICE_RANDOM) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_3, pan, depth);
    } else if (rand() & 1) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
    } else {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
    }
}

/// Updates the player's scripted transform request to face the companion.
///
/// Requires both live TMD roots. Composes their coordinates and stores only
/// the player's yaw in 4096-unit turns, wrapped to 0..4095. The event script
/// then sends the package-owned request with GAME_ACTOR_MESSAGE_TURN_TO_YAW.
static void _actor450900PreparePlayerFacingCompanion(void)
{
    GfxCoord* companionCoord;
    GfxCoord* playerCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    playerCoord    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(companionCoord);
    actorRenderComposeCoord(playerCoord);
    _gActor450900PlayerFaceCompanionTransform.rot.vy =
        ratan2(companionCoord->coord.t[0] - playerCoord->coord.t[0], companionCoord->coord.t[2] - playerCoord->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

void actor450900StartCompanionConversation(void)
{
    enum {
        ACTOR_450900_REPEAT_CONVERSATION_CAP = 12,
    };
    if (gameFlagGetNibble(GAME_FLAG_0D8) != 0) {
        if (_gActor450900PostChoiceConversationStarted == false) {
            _gActor450900PostChoiceConversationStarted = true;
            evsStartScript(_gActor450900PostChoiceConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        } else {
            capSpawnEventIfIdle(ACTOR_450900_REPEAT_CONVERSATION_CAP, CAP_EVENT_PAUSE_ACTORS);
        }
    } else if (gMapNeoArkCompanionDistressTicks < ACTOR_450900_COMPANION_DISTRESS_START_TICKS) {
        evsStartScript(_gActor450900BeforeDistressConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    } else {
        evsStartScript(_gActor450900DistressConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
}

void actor450900HandleDepartureTrigger(void)
{
    enum {
        ACTOR_450900_DEPARTURE_TRIGGER_Z    = -1900,
        ACTOR_450900_DEPARTURE_PASS_VARIANT = 0,
    };
    GfxCoord* companionCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    if (companionCoord->coord.t[2] < ACTOR_450900_DEPARTURE_TRIGGER_Z) {
        taskSpawnFromTable(gActor450900Tasks, ACTOR_450900_TASK_DEPARTURE, 0, 0);
    } else {
        capStartSequenceSlot(ACTOR_450900_DEPARTURE_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, ACTOR_450900_DEPARTURE_PASS_VARIANT);
    }
}
