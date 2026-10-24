#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Eligible caption-cursor calls since overlay load; bit 2 gives a four-on/four-off blink.
static s32 _gActor143000CaptionCursorCalls;

/// Selected quiz question index (0..10), retained until its answer is scored.
static s32 _gActor143000TerminalQuestionIndex;

/// Answers scored in the current three-question attempt (0..3); reset when a quiz starts.
static s32 _gActor143000TerminalQuestionsAsked;

/// Correct answers in the current attempt (0..3), also passed as the CAP score variant.
static s32 _gActor143000TerminalCorrectAnswers;

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
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/shelter_b2_laboratory.h"

static void _actor143000StartCaptureBand(void* captureArgs);
static void _actor143000SelectSceneCaptions(void);
static void _actor143000ResetSceneCaptions(void);
static void _actor143000SetActorControl(u8 actorControl);
static void _actor143000RestorePlayerEquipment(void);

static void _actor143000TerminalSessionTask(Task* task);

static void _actor143000UpdateCaptionCursor(s32 cursorX, s32 cursorY, const u16* text, s32 revealIndex, s32 codeAdvanced);

enum {
    ACTOR_143000_TERMINAL_QUESTION_COUNT     = 11,
    ACTOR_143000_QUESTION_UNSEEN             = 0,
    ACTOR_143000_QUESTION_ASKED              = 1,
    ACTOR_143000_CAPTION_CUE_RELEASED        = 0,
    ACTOR_143000_CAPTION_CUE_ACTIVE          = 1,
    ACTOR_143000_SCENE_FADE_FRAMES           = 30,
    ACTOR_143000_FADE_USE_DEFAULT_RAMP       = 0, // Event-script default: seven frames
    ACTOR_143000_FADE_KEEP_CURRENT_RAMP      = 0, // Returning a secondary fade preserves its ramp duration
    ACTOR_143000_SKIP_FADE_WAIT_UPDATES      = 8,
    ACTOR_143000_SKIP_ANIMATION_WAIT_UPDATES = 3,
    ACTOR_143000_MUSIC_FADE_CONTROL          = 1,
    ACTOR_143000_PLAYER_BASE_CLIP            = 1,
    ACTOR_143000_PLAYER_TERMINAL_CLIP        = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_143000_PLAYER_CAPTION_GLYPH_CLIP   = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_143000_PLAYER_COMPLETION_CLIP_49   = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_143000_PLAYER_CAPTION_RELEASE_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_143000_PLAYER_COMPLETION_CLIP_51   = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_143000_PLAYER_CUE_BLEND_FRAMES     = 10,
    ACTOR_143000_PLAYER_SCENE_SET_COUNT      = ACTOR_143000_PLAYER_COMPLETION_CLIP_51 - ACTOR_143000_PLAYER_TERMINAL_CLIP + 1,
    ACTOR_143000_TERMINAL_VIEW               = 10,
    ACTOR_143000_EXIT_VIEW                   = 15,
};

TaskDesc gActor143000TerminalTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor143000TerminalSessionTask, { .value = 0 } },
};

/// Bodyless priority-32 task descriptor for asynchronous background-band capture.
static TaskDesc _gActor143000CaptureStripTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, actor143000CaptureStripTask, { .value = 0 } };

/// Player clips for the laboratory terminal and completion scenes, extended ids 47..51.
///
/// The bank copy reads 32 entries from here, past these five into the play
/// requests that follow; those words land in extension ids 52..78 and are never
/// played.
static AnimationSet* _gActor143000PlayerSceneSets[ACTOR_143000_PLAYER_SCENE_SET_COUNT] = { &gShelterB2LaboratoryPlayerAtTerminalClip, &gShelterB2LaboratoryPlayerTypeAtTerminalClip, &gShelterB2LaboratoryPlayerShockedAtTerminalClip, &gShelterB2LaboratoryPlayerStopTypingClip, &gShelterB2LaboratoryPlayerResultsCue16Clip };

/// Clip 1 reset used on exit, logout and scene skip. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerBaseRequest = { { .index = 1 }, ACTOR_143000_PLAYER_BASE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 47 reset that no script plays. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerUnusedTerminalRequest = { { .index = 1 }, ACTOR_143000_PLAYER_TERMINAL_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 47 reset for login and completion CAP cues. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerTerminalRequest = { { .index = 1 }, ACTOR_143000_PLAYER_TERMINAL_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 48 reset for newly reached six-tick-delay glyphs; the bank index is rewritten before dispatch. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerCaptionGlyphRequest = { { .index = 1 }, ACTOR_143000_PLAYER_CAPTION_GLYPH_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 49 reset after background capture in the completion script. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerCompletion49Request = { { .index = 1 }, ACTOR_143000_PLAYER_COMPLETION_CLIP_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 50 ten-frame blend at a line break after a glyph cue; the bank index is rewritten before dispatch. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerCaptionReleaseRequest = { { .index = 1 }, ACTOR_143000_PLAYER_CAPTION_RELEASE_CLIP, ANIMATION_BLEND_INTERPOLATE, ACTOR_143000_PLAYER_CUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Clip 51 ten-frame blend near completion-scene end. Disables world collision.
static AnimationPlayRequest _gActor143000PlayerCompletion51Request = { { .index = 1 }, ACTOR_143000_PLAYER_COMPLETION_CLIP_51, ANIMATION_BLEND_INTERPOLATE, ACTOR_143000_PLAYER_CUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Installs the terminal clips as extended ids 47..51; reads 32 entries from a five-clip list.
static AnimationBankCopyRequest _gActor143000PlayerAnimationBankCopy = { _gActor143000PlayerSceneSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement at the laboratory terminal: world XYZ (3270,0,-1630), yaw zero.
static ActorTransform _gActor143000TerminalPlayerPlacement = { { 3270, 0, -1630, 0 }, { 0, 0, 0, 0 } };

/// Player placement after logout or completion: world XYZ (3270,0,-2630), yaw minus a half-turn.
static ActorTransform _gActor143000ExitPlayerPlacement = { { 3270, 0, -2630, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Caption-glyph animation latch (0 released, 1 awaiting a line break); initially released.
static u8 _gActor143000CaptionAnimationActive = ACTOR_143000_CAPTION_CUE_RELEASED;

/// Operands specific to the quiz-completion scene; sound names retain their bank entry IDs.
enum {
    ACTOR_143000_COMPLETION_CAP_SEQUENCE             = 3,
    ACTOR_143000_COMPLETION_PLAYER_WEAPON_SELECTION  = 0,
    ACTOR_143000_COMPLETION_AMBIENT_CHANNEL          = 100,
    ACTOR_143000_COMPLETION_LONG_FADE_FRAMES         = 60,
    ACTOR_143000_COMPLETION_CAPTURE_BAND_GAP_UPDATES = 70,
    ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES       = 0,
    ACTOR_143000_COMPLETION_SHORT_WAIT_UPDATES       = 1,
    ACTOR_143000_COMPLETION_SOUND_CUE_9              = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 9),
    ACTOR_143000_COMPLETION_SOUND_CUE_10             = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 10),
    ACTOR_143000_COMPLETION_SOUND_CUE_11             = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 11),
    ACTOR_143000_COMPLETION_SOUND_CUE_12             = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 12),
    ACTOR_143000_COMPLETION_SOUND_CUE_13             = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 13),
    ACTOR_143000_COMPLETION_SOUND_CUE_15             = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 15),
};

/// Quiz-completion scene, synchronized to CAP cues and paired with the completion skip path.
///
/// Selects the laboratory CAP file and sequence 3, installs player clips 47..51
/// and the caption-driven animation callback, then captures the upper and lower
/// background bands in view 14. Fades to exit view 15, restores placement,
/// equipment and lighting, clears the skip target and releases player control.
/// Seventy-two commands include the final END; wait counts preserve interpreter
/// yields. After ambience starts, skipping keeps selected sound scripts running.
/// The overlay, clips, capture records and CAP resources remain loaded throughout
/// playback; each capture record serves only one running capture task.
static EvsCommand _gActor143000CompletionScript[72] = {
    // Install CAP presentation and the player animation extension before its cues.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000SelectSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_143000_COMPLETION_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackSetText = capSetTextUpdateCallback }, { .captionText = _actor143000UpdateCaptionCursor }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor143000PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_143000_FADE_KEEP_CURRENT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor143000TerminalPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_143000_COMPLETION_PLAYER_WEAPON_SELECTION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerTerminalRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_143000_COMPLETION_AMBIENT_CHANNEL }, { .value = ACTOR_143000_COMPLETION_AMBIENT_CHANNEL }, { .value = ACTOR_143000_COMPLETION_AMBIENT_CHANNEL }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerTerminalRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerTerminalRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerTerminalRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_YIELD_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Hide the actors while asynchronous captures update the stored background.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_HIDDEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_SHORT_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_143000_COMPLETION_SOUND_CUE_15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackPointer = _actor143000StartCaptureBand }, { .storage = &gActor143000CompletionUpperCaptureBand }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_CAPTURE_BAND_GAP_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackPointer = _actor143000StartCaptureBand }, { .storage = &gActor143000CompletionLowerCaptureBand }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerCompletion49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_143000_COMPLETION_LONG_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_143000_FADE_KEEP_CURRENT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB2LaboratoryStartAmbience }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = ACTOR_143000_MUSIC_FADE_CONTROL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerCompletion51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Return to the exit presentation before releasing CAP and scripted control.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_143000_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor143000ExitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerBaseRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_143000_COMPLETION_PLAYER_WEAPON_SELECTION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_143000_FADE_KEEP_CURRENT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000ResetSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_COMPLETION_SHORT_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path for the quiz completion scene, restoring exit view, player equipment, lighting and CAP.
///
/// Borrows the exit placement and base-animation request; must remain loaded
/// until event playback has ended. The completion scene installs the clip bank.
static EvsCommand _gActor143000CompletionSkipScript[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_143000_FADE_USE_DEFAULT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = ACTOR_143000_MUSIC_FADE_CONTROL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SKIP_FADE_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_143000_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor143000SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor143000ExitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerBaseRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SKIP_ANIMATION_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB2LaboratoryStartAmbience }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SKIP_FADE_WAIT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000ResetSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Login scene: selects terminal view, installs clips, places the player and hides equipped models.
///
/// Leaves terminal clip 47 and ambient RGB (100,100,100) active for the menu.
static EvsCommand _gActor143000TerminalLoginScript[8] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_143000_TERMINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gActor143000PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor143000TerminalPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerTerminalRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Logout scene: fades to the exit view and restores player equipment, lighting and scripted control.
static EvsCommand _gActor143000TerminalLogoutScript[12] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_143000_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor143000ExitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143000PlayerBaseRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143000RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_143000_FADE_KEEP_CURRENT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_143000_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 gActor143000KeypadCaretDrawCalls = 0;

s32 gActor143000KeypadDogPhaseQ4 = 0;

ScreenFade gActor143000CodeAcceptedFade = { 0 };

u8 gActor143000KeypadPreviousView = 0;

/// Unreferenced initialized byte; its role is unproven.
static u8 D_actor_143000_80135C0D = 101;

/// Unreferenced initialized byte; its role is unproven.
static u8 D_actor_143000_80135C0E = 2;

/// Unreferenced initialized byte; its role is unproven.
static u8 D_actor_143000_80135C0F = 57;

char gActor143000KeypadCode[ACTOR_143000_KEYPAD_CODE_BUFFER_BYTES];

/// Eleven question selection latches (0 unseen, 1 already asked), reset for each quiz attempt.
///
/// Occupies the remainder of the overlay's stored zero-initialized data.
/// Selection uses indices 0..10, and reset clears all eleven bytes.
u8 _gActor143000TerminalVisitedQuestions[ACTOR_143000_TERMINAL_QUESTION_COUNT];

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
            gCapFile = 0;
            capSelectLoadedFile(ACTOR_143000_TERMINAL_CAP_DATA_ORDINAL);
            capSetTexturePage(ACTOR_143000_TERMINAL_CAP_VRAM_X, 0);
            itemSetIdentified(INVENTORY_COLLECTION_ID_BOWMANS_CARD, 1);
            itemSetIdentified(ACTOR_143000_TERMINAL_YOSHIDAS_CARD, 1);
            evsStartScript(_gActor143000TerminalLoginScript, EVENT_SCRIPT_HUD_KEEP);
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
            evsStartScript(_gActor143000TerminalLogoutScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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
            visitedQuestion = &_gActor143000TerminalVisitedQuestions[questionIndex];
            do {
                *visitedQuestion = ACTOR_143000_QUESTION_UNSEEN;
                questionIndex--;
                visitedQuestion--;
            } while (questionIndex >= 0);
            _gActor143000TerminalQuestionsAsked = 0;
            _gActor143000TerminalCorrectAnswers = 0;
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
                _gActor143000TerminalQuestionIndex = (rand() * ACTOR_143000_TERMINAL_QUESTION_COUNT) >> ACTOR_143000_TERMINAL_RAND_BITS;
                selectedQuestion                   = &_gActor143000TerminalVisitedQuestions[_gActor143000TerminalQuestionIndex];
                if (*selectedQuestion == ACTOR_143000_QUESTION_UNSEEN) {
                    *selectedQuestion = ACTOR_143000_QUESTION_ASKED;
                    break;
                }
            }
            capRunCommand(_gActor143000TerminalQuestionIndex + ACTOR_143000_TERMINAL_CAP_FIRST_QUESTION, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case ACTOR_143000_TERMINAL_STATE_WAIT_QUESTION:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SCORE_ANSWER:
            if (capGetVariantKey() == ACTOR_143000_TERMINAL_ANSWER_CORRECT) {
                _gActor143000TerminalCorrectAnswers++;
            }
            _gActor143000TerminalQuestionsAsked++;
            if (_gActor143000TerminalQuestionsAsked >= ACTOR_143000_TERMINAL_ANSWERS_REQUIRED) {
                if (_gActor143000TerminalCorrectAnswers >= ACTOR_143000_TERMINAL_ANSWERS_REQUIRED) {
                    task->state = ACTOR_143000_TERMINAL_STATE_SHOW_SUCCESS;
                } else {
                    task->state = ACTOR_143000_TERMINAL_STATE_SHOW_SCORE;
                }
            } else {
                task->state = ACTOR_143000_TERMINAL_STATE_SELECT_QUESTION;
            }
            return;
        case ACTOR_143000_TERMINAL_STATE_SHOW_SCORE:
            capStartSequenceSlot(ACTOR_143000_TERMINAL_CAP_QUIZ_SCORE, 0, (s16)_gActor143000TerminalCorrectAnswers);
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
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_143000_TERMINAL_COMPLETE_OBJECTIVE);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, ACTOR_143000_TERMINAL_PROGRESS_COMPLETE);
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
                areaApplySavedUpdates(gShelterB2LaboratoryQuizCompletionAreaUpdates);
                if (gameFlagGetNibble(GAME_FLAG_083) == 0) {
                    areaApplySavedUpdates(gShelterB2LaboratoryPodTunnelAreaUpdates);
                }
                gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
                gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ACTOR_143000_TERMINAL_COMPLETE_DIALOGUE);
                evsStartScriptWithSkip(_gActor143000CompletionScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor143000CompletionSkipScript);
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
                playerActorWriteWeaponAnimationBankIndex(&_gActor143000PlayerCaptionGlyphRequest.source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gActor143000PlayerCaptionGlyphRequest, 0);
                _gActor143000CaptionAnimationActive = ACTOR_143000_CAPTION_CUE_ACTIVE;
            }
            if (text[revealIndex] == ACTOR_143000_TEXT_LINE_BREAK && _gActor143000CaptionAnimationActive == ACTOR_143000_CAPTION_CUE_ACTIVE) {
                _gActor143000CaptionAnimationActive = ACTOR_143000_CAPTION_CUE_RELEASED;
                playerActorWriteWeaponAnimationBankIndex(&_gActor143000PlayerCaptionReleaseRequest.source.index);
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gActor143000PlayerCaptionReleaseRequest, 0);
            }
        }
        // Reserve the cursor packet even on the dark half of its eight-call blink.
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyF4(quad);
        setRGB0(quad, 0, ACTOR_143000_CURSOR_GREEN, ACTOR_143000_CURSOR_BLUE);
        setXY4(quad, cursorX + ACTOR_143000_CURSOR_X_OFFSET, cursorY - ACTOR_143000_CURSOR_SIZE, cursorX + ACTOR_143000_CURSOR_X_OFFSET + ACTOR_143000_CURSOR_SIZE, cursorY - ACTOR_143000_CURSOR_SIZE, cursorX + ACTOR_143000_CURSOR_X_OFFSET, cursorY, cursorX + ACTOR_143000_CURSOR_X_OFFSET + ACTOR_143000_CURSOR_SIZE, cursorY);
        if (_gActor143000CaptionCursorCalls & ACTOR_143000_CURSOR_BLINK_BIT) {
            addPrim(&gGpuCurrentOt[ACTOR_143000_CURSOR_OT_INDEX], quad);
        }
        _gActor143000CaptionCursorCalls++;
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
    taskSpawnFromTable(&_gActor143000CaptureStripTaskDesc, 0, 0, captureArgs);
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

    gCapFile = 0;
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
