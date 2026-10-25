#include "actors/actor_450200.h"
#include "actors/actor_450200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_observatory.h"

/// Commands supplied by the companion scene and its skip script.
enum {
    ACTOR_450200_PUFF_COMMAND_STOP  = 0,
    ACTOR_450200_PUFF_COMMAND_START = 1,
    ACTOR_450200_PUFF_COMMAND_FADE  = 2,
};

/// Logical room selected by both completed paths of the backdrop scene.
enum { ACTOR_450200_SCENE_ROOM_VARIANT = 2 };

/// VRAM placement and Q7 weights of the scene's two 8-bit backdrop layers.
enum {
    ACTOR_450200_BACKDROP_COMMAND_START      = 1,
    ACTOR_450200_BACKDROP_CLUT_COLORS        = 256,
    ACTOR_450200_BACKDROP_SCALE_ONE          = 128,
    ACTOR_450200_BACKDROP_TEXTURE_Y          = 256,
    ACTOR_450200_BACKDROP_FROM_TEXTURE_X     = 320,
    ACTOR_450200_BACKDROP_TO_TEXTURE_X       = 448,
    ACTOR_450200_BACKDROP_FROM_SOURCE_CLUT_Y = 247,
    ACTOR_450200_BACKDROP_TO_SOURCE_CLUT_Y   = 248,
    ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y = 249,
    ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y   = 250,
    ACTOR_450200_BACKDROP_END_VIEW           = 8,
    ACTOR_450200_BEAM_FULL_INTENSITY         = 160,
};

/// Execution priorities of the scene's bodyless tasks.
enum {
    ACTOR_450200_HEAD_AIM_PRIORITY = 97,
};

/// Playable prefixes of the fixed-capacity animation-bank transfers.
enum {
    ACTOR_450200_COMPANION_SCENE_PLAYER_CLIP_COUNT = 8,
    ACTOR_450200_COMPANION_CLIP_COUNT              = 8,
    ACTOR_450200_SOLO_SCENE_PLAYER_CLIP_COUNT      = 5,
    ACTOR_450200_ROOM_VARIANT_PLAYER_CLIP_COUNT    = 4,
};

/// Native-bank clips used around the companion conversations and scene endings.
enum {
    ACTOR_450200_BASE_POSE_ANIMATION         = 1,
    ACTOR_450200_PLAYER_TALK_START_ANIMATION = 32,
    ACTOR_450200_PLAYER_TALK_END_ANIMATION   = 33,
};

/// CAP command slots in the observatory's default dialogue file.
enum {
    ACTOR_450200_COMPANION_RECOVERY_TALK = 8,
    ACTOR_450200_COMPANION_FEVER_TALK    = 9,
    ACTOR_450200_COMPANION_TEASING_TALK  = 10,
    ACTOR_450200_COMPANION_REPEAT_TALK   = 11,
};

/// Segment indices selected independently by the scene's two vibration lanes.
enum {
    ACTOR_450200_COMPANION_VIBRATION_RAMP = 0,
    ACTOR_450200_COMPANION_VIBRATION_HOLD = 1,
};

/// Equipment selector for the room-entry script's HIDE_WEAPONS command.
enum { ACTOR_450200_WEAPONS_COMPANION = 1 };

/// Default CAP command slots selected by the three exported scene scripts.
enum {
    ACTOR_450200_COMPANION_SCENE_CAP_COMMAND    = 7,
    ACTOR_450200_SOLO_SCENE_CAP_COMMAND         = 1,
    ACTOR_450200_ROOM_VARIANT_SCENE_CAP_COMMAND = 2,
};

/// Weapon selections and final views used by the exported scene scripts.
enum {
    ACTOR_450200_WEAPONS_PLAYER           = 0,
    ACTOR_450200_WEAPONS_BOTH             = 2,
    ACTOR_450200_COMPANION_SCENE_END_VIEW = 3,
    ACTOR_450200_SOLO_SCENE_END_VIEW      = 2,
};

/// Native player clips used by both approaches in the room-variant scene.
enum { ACTOR_450200_ROOM_VARIANT_WALK_ANIMATION = 19 };

// Preserve the first-declaration order of the overlay's BSS.
/// Borrowed handle of the observatory player's companion-facing head-aim task.
///
/// Scene entry replaces it with the spawn result, including NULL on failure;
/// talk scripts write its enable argument. The task's ordinary stop state clears
/// it, but work-allocation failure kills without clearing this handle.
static Task* _gActor450200HeadAimTask;
/// Borrowed handle of the companion scene's currently tracked puff emitter.
///
/// Start replaces it with the spawn result, including NULL on failure, without
/// stopping an earlier emitter. Stop kills and clears the tracked task; other
/// commands write its state. The scene must stop it before unloading its code
/// or the companion model used by its effects.
static Task* _gActor450200CompanionPuffTask;

/// Captured outgoing backdrop palette: 256 RGB555 halfwords from VRAM row 247.
///
/// Shared by the single active cross-fade task; the SDK transfers the complete
/// 512-byte row through a word-pointer view of this word-aligned storage.
u16 _gActor450200BackdropFromClut[ACTOR_450200_BACKDROP_CLUT_COLORS];

/// Captured incoming backdrop palette: 256 RGB555 halfwords from VRAM row 248.
///
/// Preserved as the source of each fade step; simultaneous tasks would overwrite it.
u16 _gActor450200BackdropToClut[ACTOR_450200_BACKDROP_CLUT_COLORS];

/// Outgoing RGB555 palette scaled by the cross-fade's descending Q7 weight.
///
/// All 256 colors have bit 15 set and are uploaded to VRAM row 249. Word-aligned
/// storage must remain live through the complete 512-byte GPU transfer.
u16 _gActor450200BackdropScaledFromClut[ACTOR_450200_BACKDROP_CLUT_COLORS];

/// Incoming RGB555 palette scaled by the complementary Q7 cross-fade weight.
///
/// All 256 colors have bit 15 set and are uploaded to VRAM row 250. Word-aligned
/// storage must remain live through the complete 512-byte GPU transfer.
u16 _gActor450200BackdropScaledToClut[ACTOR_450200_BACKDROP_CLUT_COLORS];

static void _actor450200StartBackdropCrossFade(s32 command);
static void _actor450200SetLightBeamIntensity(s32 intensity);
static void _actor450200SetRoomVariant(u8 roomVariant);

static AnimationSet _gNeoArkObservatoryKyleWalkForwardClip;
static AnimationSet _gNeoArkObservatoryKyleCollapseClip;
static AnimationSet _gNeoArkObservatoryKyleLieRaiseHeadClip;
static AnimationSet _gNeoArkObservatoryKyleLieHeadRaisedClip;
static AnimationSet _gNeoArkObservatoryKyleSitWinceClip;
static AnimationSet _gNeoArkObservatoryKyleSitClip;
static AnimationSet _gSoldierADontWorryEndClip;
static AnimationSet _gSoldierAHiyaFlintClip;
static void         _actor450200ControlCompanionPuffs(s32 command);
static void         _actor450200CancelRoomEffects(void);
static void         _actor450200SetHeadAimEnabled(s32 enabled);
static void         _actor450200UpdateTalkFacing(void);
static void         _actor450200BackdropCrossFadeTask(Task* task);

static void _actor450200CompanionPuffTask(Task* task);
static void _actor450200HeadAimTask(Task* task);

#include "assets/rooms/neo_ark/observatory/kyle_clips_1.inc"

TaskDesc gActor450200SceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor450200CompanionPuffTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_450200_HEAD_AIM_PRIORITY } }, _actor450200HeadAimTask, { .value = 0 } },
};

/// Player clips installed at bank ids 47-54 for the observatory companion scene.
///
/// The fixed 32-word transfer also reads the following copy request, the clip-47
/// blend request and 17 words of the unused clip-47 reset requests. Those words are
/// never selected for playback. Keep the adjacent records in their stored order
/// and the actor package loaded while the player's bank borrows these clips.
#include "assets/rooms/neo_ark/observatory/companion_scene_player_bank.inc"

/// Installs the companion scene's eight player clips and their adjacent raw words.
///
/// The count is the bank's 32-word capacity, including 24 words after the clip
/// table. Dispatch copies synchronously; playback continues to borrow the clips.
static AnimationBankCopyRequest _gActor450200CompanionScenePlayerBankCopyRequest = { _gActor450200CompanionScenePlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Blends the player into companion-scene clip 47 over five frames with collision enabled.
///
/// Weapon-animation event commands replace `source.index` with the equipped
/// weapon's bank before dispatch; the stored selector is not used by this scene.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation47Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE };

/// Four stored player clip-47 resets in the companion scene's bank-transfer tail.
///
/// No script plays these requests. The bank transfer reads their first 17 words:
/// three complete requests and the first two words of the fourth. Preserve all
/// four complete records; the final three words are outside the transfer.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation47ResetRequests[4] = {
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Two further stored companion-scene player clip-47 resets with collision enabled.
///
/// Neither request is played or included in the preceding bank transfer. Their
/// original purpose and grouping with the preceding four resets are unproven.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAdditionalAnimation47ResetRequests[2] = {
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Blends the player into companion-scene clip 48 over eight frames with collision enabled.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation48Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the player to companion-scene clip 49 with collision enabled.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation49Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the player to companion-scene clip 50 with collision enabled.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation50Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the player to companion-scene clip 51 with collision enabled.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation51Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Stored companion-scene player clip-52 reset with no playback or bank-copy use.
///
/// Duplicates the later clip-52 reset's values; its original purpose is unproven.
static AnimationPlayRequest _gActor450200CompanionScenePlayerUnusedAnimation52Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the player to companion-scene clip 53 after the clip-52 pause.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation53Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player into companion-scene clip 54 over eight frames before restoring the base pose.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation54Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

/// Resets the player to companion-scene clip 52 at the start of the repeated clip-52/53 sequence.
static AnimationPlayRequest _gActor450200CompanionScenePlayerAnimation52Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Companion clips installed at bank ids 47-54 for the scene, room entry and talk.
///
/// The fixed 32-word transfer also reads the following copy request, the four
/// complete play requests for ids 47-50 and two words of the id-51 request.
/// Those extra words are never selected for playback. Keep the records in their
/// stored order and this package loaded while the companion borrows the clips.
#include "assets/rooms/neo_ark/observatory/companion_bank.inc"

/// Installs eight observatory companion clips and the 24 raw words following their table.
///
/// Scene entry and three talk scripts install the bank; the scene's skip script
/// and third talk script rely on the installed extension. Dispatch copies the
/// full 32 words synchronously, and playback continues to borrow the clips.
static AnimationBankCopyRequest _gActor450200CompanionBankCopyRequest = { _gActor450200CompanionAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Resets the companion to scene clip 47 before the player's first approach.
///
/// Disables world collision. Event playback supplies the live companion bank;
/// the scene extension and its borrowed clip data must remain loaded.
static AnimationPlayRequest _gActor450200CompanionAnimation47Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the companion into scene clip 48 over eight frames as the puffs start.
///
/// Disables world collision; requires the installed companion extension.
static AnimationPlayRequest _gActor450200CompanionAnimation48Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the companion to scene clip 49 before the player's dialogue placement.
///
/// Disables world collision; requires the installed companion extension.
static AnimationPlayRequest _gActor450200CompanionAnimation49Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts companion scene clip 50 at each of four dialogue cues.
///
/// Disables world collision; requires the installed companion extension.
static AnimationPlayRequest _gActor450200CompanionAnimation50Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the companion to scene clip 51 before the closing fade.
///
/// Disables world collision; requires the installed companion extension.
static AnimationPlayRequest _gActor450200CompanionAnimation51Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the companion to scene clip 52 before repositioning for the player's departure.
///
/// Disables world collision; requires the installed companion extension.
static AnimationPlayRequest _gActor450200CompanionAnimation52Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the companion to clip 53 at scene completion, room entry and talk returns.
///
/// Disables world collision. Event playback supplies the current companion bank;
/// this package's extension must already be installed and remain loaded.
static AnimationPlayRequest _gActor450200CompanionAnimation53Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the companion to clip 54 during conversations with world collision disabled.
///
/// Talk scripts return to clip 53 at the following CAP cue. Event playback
/// supplies the current companion bank, which must retain this extension.
static AnimationPlayRequest _gActor450200CompanionAnimation54Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restores native-bank pose 1 on either actor with world collision enabled.
///
/// Event playback resolves the bank separately for the player and companion;
/// this request does not depend on the observatory's extension clips.
static AnimationPlayRequest _gActor450200RestoreBaseAnimationRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player into native clip 32 over four frames at conversation start.
///
/// Uses the equipped weapon's bank and enables world collision.
static AnimationPlayRequest _gActor450200PlayerTalkStartAnimationRequest = { { .index = 1 }, ACTOR_450200_PLAYER_TALK_START_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends the player into native clip 33 over four frames at conversation end.
///
/// Uses the equipped weapon's bank and enables world collision.
static AnimationPlayRequest _gActor450200PlayerTalkEndAnimationRequest = { { .index = 1 }, ACTOR_450200_PLAYER_TALK_END_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

/// Holds the binary motor for 13 frames while the variable motor ramps for 12.
///
/// Each lane plays its own segment, then stops at the second command. Both
/// arrays must remain loaded until the borrowed vibration script finishes.
static PadScriptCmd _gActor450200CompanionSceneVibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_450200_COMPANION_VIBRATION_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_450200_COMPANION_VIBRATION_RAMP) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Variable-motor ramp and binary-motor hold used at the companion scene's heat cue.
///
/// Segment 0 ramps intensity 253 toward 67 over 12 script frames; segment 1
/// holds the binary motor for 13. The trailing bytes are retained but unread
/// by the interpreter. Storage is borrowed until both lanes finish.
static PadScriptVibrationSegment _gActor450200CompanionSceneVibrationSegments[] = {
    [ACTOR_450200_COMPANION_VIBRATION_RAMP] = { .startIntensity = 253, .endIntensity = 67, .durationFrames = 12, .field_3 = 1 },
    [ACTOR_450200_COMPANION_VIBRATION_HOLD] = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 13, .field_3 = 0 },
};

/// Player's initial room placement in the companion scene, facing a negative quarter turn.
///
/// Placement consumes XYZ and all Euler angles; angles use 4096 units per turn.
static ActorTransform _gActor450200CompanionScenePlayerStartPlacement = { { 14000, 0, 11600, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's first scripted walk destination in the companion scene.
///
/// Room-coordinate XYZ is copied during dispatch; the stored angles are ignored.
static ActorTransform _gActor450200CompanionScenePlayerFirstWalkDestination = { { 8000, 0, 11600, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement for the companion scene's repeated dialogue exchanges.
///
/// Room-coordinate XYZ and a negative quarter-turn yaw; placement reads both vectors.
static ActorTransform _gActor450200CompanionScenePlayerDialoguePlacement = { { 10500, 0, 12200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's first departure walk destination after the companion-scene dialogue.
///
/// Room-coordinate XYZ is copied during dispatch; the stored angles are ignored.
static ActorTransform _gActor450200CompanionScenePlayerSecondWalkDestination = { { 8000, 0, 12200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's final departure walk destination before the companion scene closes.
///
/// Room-coordinate XYZ is copied during dispatch; the stored angles are ignored.
static ActorTransform _gActor450200CompanionScenePlayerFinalWalkDestination = { { 8000, 0, 9000, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Companion's initial room placement in the scene, facing a negative quarter turn.
///
/// Placement consumes XYZ and all Euler angles; angles use 4096 units per turn.
static ActorTransform _gActor450200CompanionSceneCompanionStartPlacement = { { 11000, 0, 12400, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Companion placement during the player's departure, before the closing fade.
///
/// Room-coordinate XYZ and yaw -113 in 4096 units per turn; placement reads both vectors.
static ActorTransform _gActor450200CompanionSceneCompanionExitPlacement = { { 11000, 0, 11300, 0 }, { 0, -113, 0, 0 } };

/// Companion placement restored by scene completion, skipping and later room entry.
///
/// Room-coordinate XYZ and Euler angles in 4096 units per turn; all angles are
/// zero. Sent with `GAME_ACTOR_MESSAGE_PLACE`, which reads both vectors.
static ActorTransform _gActor450200CompanionPostScenePlacement = { { 9750, 0, 0x2C24, 0 }, { 0, 0, 0, 0 } };

/// Player placement shared by normal and skipped companion-scene endings.
///
/// Room-coordinate XYZ with a negative half-turn yaw; placement reads both vectors.
static ActorTransform _gActor450200CompanionScenePlayerEndPlacement = { { 8000, 0, 9700, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player yaw target for the four observatory companion conversations.
///
/// Updated from the live actors before each turn command. Yaw uses 4096 units
/// per turn; `GAME_ACTOR_MESSAGE_TURN_TO_YAW` ignores position, pitch and roll.
static ActorTransform _gActor450200CompanionTalkFacing = { { 0, 0, 0, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Repeated companion-scene exchange: reset player clip 52, wait 24 frames, then reset 53.
///
/// Called three times by the scene. Requires its player extension; RETURN resumes
/// the caller, so this stream must not be started as a top-level event script.
static EvsCommand _gActor450200CompanionScenePlayerAnimation52To53Script[] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Repeated companion-scene exchange using player clips 48, 49 and 50.
///
/// Waits 13 frames after 48 and 50 after 49, then sets the animation rate to
/// 30 sixteenths of a frame per tick before clip 50 resets it to the normal rate.
/// Requires the player extension; RETURN resumes the caller, so this stream is
/// only a script-call target.
static EvsCommand _gActor450200CompanionScenePlayerAnimation48To50Script[] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_SET_RATE }, { .value = 30 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand gActor450200CompanionSceneScript[82] = {
    // Install the borrowed scene clips before the first CAP cue can yield.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_COMPANION_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RestoreBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RestoreBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionScenePlayerStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionSceneCompanionStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450200_WEAPONS_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor450200CompanionScenePlayerFirstWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Couple the companion's puff phases and player animation sequences to CAP cues.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 8) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(0x66, 0x0A) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor450200CompanionSceneVibrationCommands }, { .vibrationSegments = _gActor450200CompanionSceneVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 7) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionScenePlayerDialoguePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_FADE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation52To53Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation48To50Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation52To53Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation48To50Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation52To53Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor450200CompanionScenePlayerAnimation48To50Script }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionScenePlayerAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RestoreBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionSceneCompanionExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor450200CompanionScenePlayerSecondWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor450200CompanionScenePlayerFinalWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Stop effects before restoring the actors and the room's companion obstacle.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_COMPANION_SCENE_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionPostScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RestoreBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = neoArkObservatoryUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450200CompanionSceneSkipScript[19] = {
    // Restore the final actors and obstacle without replaying the puff sequence.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_COMPANION_SCENE_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionPostScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450200_WEAPONS_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RestoreBaseAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = neoArkObservatoryUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Companion model coordinate indices eligible as random puff anchors.
///
/// The emitter selects only the first eleven bytes for rand values 0..32767.
/// The greatest selected part is 19, requiring twenty live coordinates.
/// The final zero byte is retained storage outside the selection domain; its
/// original purpose is unproven.
static u8 _gActor450200CompanionPuffParts[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

/// Local spawn offset for the companion scene's shrinking puffs and smoke tail.
///
/// Signed game-coordinate XYZ in the selected model part's frame. Effect spawn
/// copies (0, -128, 0) for placement and retains the borrowed vector pointer, so
/// the actor package must remain loaded while those effects can use it.
static SVECTOR _gActor450200CompanionPuffOffset = { 0, -128, 0, 0 };

/// First observatory companion conversation: Aya asks whether Kyle is okay.
///
/// Starts CAP command 8, installs the companion clips and turns the player toward
/// him. Brackets the conversation with head aiming and player talk animations,
/// then releases scripted player control. The package and CAP file stay loaded.
static EvsCommand _gActor450200CompanionRecoveryTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_COMPANION_RECOVERY_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200CompanionTalkFacing } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkStartAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second observatory companion conversation: Aya asks about Kyle's fever.
///
/// Starts CAP command 9 and installs the companion clips. Turns the player toward
/// him, enables head aiming through the talk poses and releases player control
/// before disabling head aiming. The package and CAP file stay loaded.
static EvsCommand _gActor450200CompanionFeverTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_COMPANION_FEVER_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200CompanionTalkFacing } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkStartAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third observatory companion conversation: Kyle teases Aya about lingering.
///
/// Starts CAP command 10 and plays companion clip 54 at two dialogue cues.
/// Relies on an earlier scene or talk having installed the companion extension.
/// Turns the player, brackets head aiming and talk poses, then releases control.
static EvsCommand _gActor450200CompanionTeasingTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_COMPANION_TEASING_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200CompanionTalkFacing } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkStartAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeatable observatory conversation: Kyle leaves the next move to Aya.
///
/// Starts CAP command 11 and reinstalls companion clips. Turns the player toward
/// Kyle and brackets head aiming with the talk poses and control release. Selected
/// for talk count 3 without advancing that count; package and CAP data stay loaded.
static EvsCommand _gActor450200CompanionRepeatTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_COMPANION_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200CompanionTalkFacing } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkStartAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200PlayerTalkEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores the companion's post-scene placement, clip bank, pose and room obstacle.
///
/// Room entry starts this stream with HUD visibility retained. Hides the
/// companion's weapon, yields before playback and waits one frame for clip 53
/// before rebuilding the room obstacle. Requires a live companion and loaded clips.
static EvsCommand _gActor450200CompanionRoomEntryScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200CompanionPostScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200CompanionBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450200_WEAPONS_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200CompanionAnimation53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = neoArkObservatoryUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/rooms/neo_ark/observatory/kyle_clips_2.inc"

/// Player clips installed at bank ids 47-51 for the observatory's solo scene.
///
/// The fixed 32-word transfer includes the next five complete play requests
/// and two words of the sixth request. Those 27 non-clip words are never
/// selected for playback. Keep the adjacent records in stored order and the
/// actor package loaded while the player borrows these clips.
#include "assets/rooms/neo_ark/observatory/solo_scene_player_bank.inc"

/// Resets the player to native pose 1 at both solo-scene endings, disabling collision.
///
/// Event playback resolves the equipped weapon's bank. The scene's final
/// scripted-control release restores ordinary playback and collision later.
static AnimationPlayRequest _gActor450200SoloScenePlayerEndAnimationRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into native pose 1 over 15 frames as the solo scene starts.
///
/// Event playback selects the equipped weapon's bank and disables world collision.
static AnimationPlayRequest _gActor450200SoloScenePlayerStartAnimationRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the player to solo-scene clip 47 at its first animation cue.
///
/// Disables world collision; requires the installed solo-scene player extension.
static AnimationPlayRequest _gActor450200SoloScenePlayerAnimation47Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the player to solo-scene clip 48 before the 15-frame transition pause.
///
/// Disables world collision; requires the installed solo-scene player extension.
static AnimationPlayRequest _gActor450200SoloScenePlayerAnimation48Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into solo-scene clip 49 over ten frames at either of two cues.
///
/// Disables world collision; requires the installed solo-scene player extension.
static AnimationPlayRequest _gActor450200SoloScenePlayerAnimation49Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into solo-scene clip 50 over ten frames at its CAP cue.
///
/// Disables world collision; requires the installed solo-scene player extension.
static AnimationPlayRequest _gActor450200SoloScenePlayerAnimation50Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into solo-scene clip 51 over ten frames before the 50-frame hold.
///
/// Disables world collision; requires the installed solo-scene player extension.
static AnimationPlayRequest _gActor450200SoloScenePlayerAnimation51Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Installs five solo-scene player clips and the 27 raw words following their table.
///
/// Dispatch copies all 32 words synchronously into the equipped weapon bank's
/// extension; only ids 47-51 are clips. Keep the adjacent records in stored order
/// and this package loaded while playback borrows the installed clip data.
static AnimationBankCopyRequest _gActor450200SoloScenePlayerBankCopyRequest = { _gActor450200SoloScenePlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement at solo-scene setup and both normal and skipped endings.
///
/// Room-coordinate XYZ with a negative quarter-turn yaw; placement reads both vectors.
static ActorTransform _gActor450200SoloScenePlayerEndPlacement = { { 0x2710, 0, 0x2EE0, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

EvsCommand gActor450200SoloSceneScript[40] = {
    // Install the player-only scene bank, then reveal its setup placement.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_SOLO_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200SoloScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerStartAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200SoloScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 1) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP cues advance the player clips while the model's draw state changes.
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 2) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore the native pose and weapons under the closing fade.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_SOLO_SCENE_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200SoloScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450200SoloSceneSkipScript[12] = {
    // Apply the same final placement and pose without installing the scene clips.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_SOLO_SCENE_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200SoloScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200SoloScenePlayerEndAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/rooms/neo_ark/observatory/kyle_clips_3.inc"

/// Bodyless priority-32 task that cross-fades the observatory backdrop palettes.
///
/// Spawned directly at index zero. The scene must avoid simultaneous instances,
/// which would overwrite the same four palette buffers; view 8 terminates it.
static TaskDesc _gActor450200BackdropCrossFadeTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor450200BackdropCrossFadeTask, { .value = 0 } };

/// Player clips installed at bank ids 47-50 for the observatory room-variant scene.
///
/// The fixed 32-word transfer includes the next five complete play requests
/// and three words of the sixth request. Those 28 non-clip words are never
/// selected for playback. Keep the adjacent records in stored order and the
/// package loaded while playback borrows these clips. Both scene endings select
/// room variant 2 after the backdrop changes.
#include "assets/rooms/neo_ark/observatory/room_variant_scene_player_bank.inc"

/// Resets the player to native pose 1 when the room-variant scene is skipped.
///
/// Disables world collision; the skip script releases scripted control afterwards.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerSkipAnimationRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into native pose 1 over 15 frames at setup and between approaches.
///
/// Event playback selects the equipped weapon's bank and disables world collision.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerBasePoseBlendRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

/// Stored 30-frame native-pose-1 blend in the room-variant scene's bank-transfer tail.
///
/// All five words are copied by the bank transfer; no script plays the request.
/// Its original playback purpose is unproven. Preserve the collision-disabled
/// request as a complete record rather than discarding its unread-for-playback bytes.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerBasePoseSlowBlendRequest = { { .index = 1 }, ACTOR_450200_BASE_POSE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into room-variant scene clip 47 over 15 frames at two approach cues.
///
/// Disables world collision; requires the installed room-variant player extension.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerAnimation47Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the player to room-variant scene clip 48 after the backdrop fade starts.
///
/// Disables world collision; requires the installed player extension. The bank
/// transfer also copies all five words. Event playback supplies the weapon bank.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerAnimation48Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into room-variant scene clip 49 over eight frames before the long hold.
///
/// Disables world collision; requires the installed player extension. The bank
/// transfer copies only the first three words of this complete request.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerAnimation49Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into room-variant scene clip 50 over eight frames after the long hold.
///
/// Disables world collision; requires the installed player extension. Event
/// playback selects the equipped weapon bank in a copy of this request.
static AnimationPlayRequest _gActor450200RoomVariantScenePlayerAnimation50Request = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

/// Installs the room-variant scene's four player clips and 28 following raw words.
///
/// Copies 32 words synchronously: the four pointers, five complete play requests
/// and three words of the clip-49 request. Only extension ids 47-50 are clips.
/// Preserve the records' stored order and keep the package loaded while the
/// equipped weapon's bank borrows the installed clip data.
static AnimationBankCopyRequest _gActor450200RoomVariantScenePlayerBankCopyRequest = { _gActor450200RoomVariantScenePlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player approach and arrival clips shared by both room-variant scene walks.
///
/// Uses native active-bank clip 19 while approaching and pose 1 on arrival.
/// The move handler copies both ids into halfwords; the record is borrowed only
/// through synchronous dispatch, with no animation-bank or collision selection.
static GameActorMoveAnim _gActor450200RoomVariantScenePlayerMoveAnimation = { ACTOR_450200_ROOM_VARIANT_WALK_ANIMATION, ACTOR_450200_BASE_POSE_ANIMATION };

/// Full player placement before the room-variant scene's first approach.
///
/// XYZ uses room coordinates; Euler angles use 4096 units per turn. Placement
/// reads all three angles, including zero pitch and roll, through dispatch.
static ActorTransform _gActor450200RoomVariantScenePlayerStartPlacement = { { 6271, 0, 3306, 0 }, { 0, -682, 0, 0 } };

/// Room-coordinate player destination for the room-variant scene's first walk.
///
/// The move handler copies XYZ only. The stored rotation, including yaw -1080
/// in 4096 units per turn, has no consumer in this scene.
static ActorTransform _gActor450200RoomVariantScenePlayerFirstWalkDestination = { { 3096, 0, 4774, 0 }, { 0, -1080, 0, 0 } };

/// Room-coordinate destination and following yaw target for the scene's second walk.
///
/// The move handler copies XYZ, then the turn handler copies yaw 3015 in 4096
/// units per turn. Stored pitch and roll are unread by those commands. Payload
/// storage is borrowed only through each synchronous dispatch.
static ActorTransform _gActor450200RoomVariantScenePlayerSecondWalkDestination = { { 880, 0, 6890, 0 }, { 0, 3015, 0, 0 } };

/// Player destination pose after the room-variant scene's backdrop transition.
///
/// The normal ending turns to yaw 967; skipping places both room-coordinate XYZ
/// and Euler angles. Angles use 4096 units per turn.
static ActorTransform _gActor450200RoomVariantScenePlayerEndPlacement = { { 880, 0, 6890, 0 }, { 0, 967, 0, 0 } };

EvsCommand gActor450200RoomVariantSceneScript[44] = {
    // Install four playable clips; the transfer also preserves adjacent request words.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_450200_ROOM_VARIANT_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor450200RoomVariantScenePlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerBasePoseBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    // Walk to both scene positions, then turn to the second destination's yaw.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerFirstWalkDestination } }, { .message = { .moveAnimation = &_gActor450200RoomVariantScenePlayerMoveAnimation } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 4) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerBasePoseBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerSecondWalkDestination } }, { .message = { .moveAnimation = &_gActor450200RoomVariantScenePlayerMoveAnimation } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerSecondWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 3) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Cross-fade the backdrop and ramp the beams before selecting the final room.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200StartBackdropCrossFade }, { .value = ACTOR_450200_BACKDROP_COMMAND_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerAnimation48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_BACKDROP_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450200SetRoomVariant }, { .value = ACTOR_450200_SCENE_ROOM_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor450200RoomVariantSceneSkipScript[15] = {
    // Set the final room, view and beam intensity directly, then restore player control.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450200SetRoomVariant }, { .value = ACTOR_450200_SCENE_ROOM_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetLightBeamIntensity }, { .value = ACTOR_450200_BEAM_FULL_INTENSITY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_450200_BACKDROP_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor450200RoomVariantScenePlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor450200RoomVariantScenePlayerSkipAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_450200_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static void        _actor450200DrawBackdropLayer(s32 screenX, s32 textureX, s32 paletteY, s32 semiTransparent, s32 brightness, s32 rawTexture);
static inline void _actor450200LoadScaledClut(const u16* sourceColors, u16* scaledColors, s32 scale, s32 vramY);

/// Emits additive puffs and smoke from randomly chosen companion model parts.
///
/// States 0/1 initialize/run steady emission; 2/3 initialize/run the shrinking
/// puffs and smoke tail. The scene owns teardown: the signed halfword counter
/// wraps and never kills this task. Requires a live companion TMD with at least
/// twenty coordinates throughout emission and while spawned effects borrow
/// their anchors and the shared offset vector.
static void _actor450200CompanionPuffTask(Task* task)
{
    enum {
        ACTOR_450200_PUFF_INIT            = 0,
        ACTOR_450200_PUFF_STEADY          = 1,
        ACTOR_450200_PUFF_FADE_INIT       = 2,
        ACTOR_450200_PUFF_FADE            = 3,
        ACTOR_450200_PUFF_ANCHOR_COUNT    = 11,
        ACTOR_450200_PUFF_STEADY_SEED     = 100,
        ACTOR_450200_PUFF_FADE_TICKS      = 128,
        ACTOR_450200_PUFF_SMOKE_TAIL_MIN  = -31,
        ACTOR_450200_PUFF_STEADY_SPAWN    = 0x80000300,
        ACTOR_450200_PUFF_FADE_SPAWN_BASE = 0x80000080,
        ACTOR_450200_PUFF_SMOKE_SPAWN     = 0xF0010100,
    };
    Task*     companion;
    GfxCoord* anchor;
    s16       countdown;

    // The twelfth table byte is excluded; every tick advances rand before dispatch.
    companion = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    anchor    = &companion->extra.tmd->coords[_gActor450200CompanionPuffParts[(rand() * ACTOR_450200_PUFF_ANCHOR_COUNT) >> 15]];
    switch (task->state) {
        case ACTOR_450200_PUFF_INIT:
            task->killCountdown = ACTOR_450200_PUFF_STEADY_SEED;
            task->state++;
            return;
        case ACTOR_450200_PUFF_STEADY:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if ((countdown & 1) == 0) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, anchor, ACTOR_450200_PUFF_STEADY_SPAWN, NULL);
            }
            return;
        case ACTOR_450200_PUFF_FADE_INIT:
            task->killCountdown = ACTOR_450200_PUFF_FADE_TICKS;
            task->state++;
            return;
        case ACTOR_450200_PUFF_FADE:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if (countdown & 1) {
                if (countdown > 0) {
                    effectSpawn(EFFECT_ADDITIVE_PUFF, anchor, countdown * 2 + ACTOR_450200_PUFF_FADE_SPAWN_BASE,
                                &_gActor450200CompanionPuffOffset);
                }
            } else if (countdown >= ACTOR_450200_PUFF_SMOKE_TAIL_MIN && (countdown & 7) == 0) {
                effectSpawn(EFFECT_SMOKE_PUFF, anchor, ACTOR_450200_PUFF_SMOKE_SPAWN, &_gActor450200CompanionPuffOffset);
            }
            return;
    }
}

/// Fades the scene's head-turn blend weight toward full aiming or the current pose.
///
/// A full-range fade takes eight calls with `enabled` nonzero or sixteen with
/// it zero. Requires writable `aim`; only its Q12 `rate` changes, where zero
/// holds the current pose and `ONE` applies the aim. Updates wrap to a signed
/// halfword before endpoint checks, including outside the normal 0..ONE range.
static inline void _actor450200RampHeadAimWeight(AnimationHeadAim* aim, s32 enabled)
{
    enum {
        ACTOR_450200_HEAD_AIM_FADE_IN_TICKS  = 8,
        ACTOR_450200_HEAD_AIM_FADE_OUT_TICKS = 16,
    };
    s16 nextWeight;

    if (enabled != 0) {
        nextWeight = aim->rate + ONE / ACTOR_450200_HEAD_AIM_FADE_IN_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextWeight = aim->rate - ONE / ACTOR_450200_HEAD_AIM_FADE_OUT_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight < 0) {
            aim->rate = 0;
        }
    }
}

/// Fades the player's head turn toward the companion during talk scenes.
///
/// State 0 owns a zeroed `AnimationHeadAim` in `Task::work` and immediately runs
/// state 1. `spawnArg1` enables aiming when nonzero. Both actors must retain live
/// TMD models with at least five coordinates. Other states kill this task and
/// clear its tracked handle; allocation failure kills without clearing it.
static void _actor450200HeadAimTask(Task* task)
{
    enum {
        ACTOR_450200_HEAD_AIM_INIT  = 0,
        ACTOR_450200_HEAD_AIM_RUN   = 1,
        ACTOR_450200_HEAD_AIM_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    };
    Task*             looker;
    AnimationHeadAim* aim;

    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case ACTOR_450200_HEAD_AIM_INIT:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = ACTOR_450200_HEAD_AIM_LIMIT;
            aim->pitchLimit = ACTOR_450200_HEAD_AIM_LIMIT;
            task->state++;
            /* fallthrough */
        case ACTOR_450200_HEAD_AIM_RUN:
            aim = task->work;
            _actor450200RampHeadAimWeight(aim, task->spawnArg1.value);
            animationAimHeadAt(looker, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            _gActor450200HeadAimTask = NULL;
            return;
    }
}

/// Controls the companion scene's tracked puff emitter.
///
/// Commands are 0 stop, 1 start and 2 begin fading. Start always creates a new
/// task and replaces the tracked handle, even when one is already live; failed
/// spawning stores NULL. Other values write the live task's state unchanged.
static void _actor450200ControlCompanionPuffs(s32 command)
{
    enum { ACTOR_450200_PUFF_TASK_TABLE_INDEX = 1 };

    if (command == ACTOR_450200_PUFF_COMMAND_STOP) {
        if (_gActor450200CompanionPuffTask != NULL) {
            taskKill(_gActor450200CompanionPuffTask);
            _gActor450200CompanionPuffTask = NULL;
        }
    } else if (command == ACTOR_450200_PUFF_COMMAND_START) {
        _gActor450200CompanionPuffTask = taskSpawnFromTable(gActor450200SceneTaskDescs, ACTOR_450200_PUFF_TASK_TABLE_INDEX, 0, 0);
    } else if (_gActor450200CompanionPuffTask != NULL) {
        _gActor450200CompanionPuffTask->state = command;
    }
}

/// Requests cancellation of all room effects at the scene's cleanup points.
static void _actor450200CancelRoomEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Sets the live player head-aim task's enable value (zero off, nonzero on).
///
/// Stores the full event argument; a missing task makes this a no-op.
static void _actor450200SetHeadAimEnabled(s32 enabled)
{
    if (_gActor450200HeadAimTask != NULL) {
        _gActor450200HeadAimTask->spawnArg1.value = enabled;
    }
}

/// Updates the player's talk yaw target toward the companion.
///
/// Both tasks must own live root coordinates in the same parent frame. Stores
/// a heading in 4096 units per turn; only yaw changes. The talk script applies
/// this payload to the player after this callback returns.
static void _actor450200UpdateTalkFacing(void)
{
    GfxCoord* target;
    GfxCoord* looker;

    target = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(target);
    actorRenderComposeCoord(looker);
    _gActor450200CompanionTalkFacing.rot.vy =
        ratan2(target->coord.t[0] - looker->coord.t[0], target->coord.t[2] - looker->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

void actor450200StartCompanionTalk(void)
{
    enum {
        ACTOR_450200_TALK_FIRST  = 0,
        ACTOR_450200_TALK_SECOND = 1,
        ACTOR_450200_TALK_THIRD  = 2,
        ACTOR_450200_TALK_REPEAT = 3,
    };
    switch (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT)) {
        case ACTOR_450200_TALK_FIRST:
            evsStartScript(_gActor450200CompanionRecoveryTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_SECOND);
            break;
        case ACTOR_450200_TALK_SECOND:
            evsStartScript(_gActor450200CompanionFeverTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_THIRD);
            break;
        case ACTOR_450200_TALK_THIRD:
            evsStartScript(_gActor450200CompanionTeasingTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_REPEAT);
            break;
        case ACTOR_450200_TALK_REPEAT:
            evsStartScript(_gActor450200CompanionRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

void actor450200InitializeObservatoryCompanion(void)
{
    enum {
        ACTOR_450200_HEAD_AIM_DESCRIPTOR = 2,
    };
    if (gameFlagGetNibble(GAME_FLAG_0D7) != 0) {
        evsStartScript(_gActor450200CompanionRoomEntryScript, EVENT_SCRIPT_HUD_KEEP);
    } else {
        neoArkObservatoryUpdateCompanionObstacle(0);
    }
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        _gActor450200HeadAimTask = taskSpawnFromTable(gActor450200SceneTaskDescs, ACTOR_450200_HEAD_AIM_DESCRIPTOR, 0, 0);
    }
}

/// Draws one backdrop layer as two adjacent 256-by-240 8-bit sprites.
///
/// `screenX` is the left edge in pixels before subtracting the 160-pixel screen
/// centre. `textureX` is a VRAM word X and `paletteY` selects the CLUT row; the
/// texture starts at row 256 and the 256-colour CLUT at X=0. Nonzero `rawTexture` ignores RGB
/// modulation; otherwise `brightness` supplies its low byte to all channels.
/// Nonzero `semiTransparent` enables additive blending of bit-15 texels.
/// Requires space for two SPRT/DR_MODE pairs in the current frame's packet arena
/// and at least 1023 ordering-table entries. Packets remain live through drawing.
static void _actor450200DrawBackdropLayer(s32 screenX, s32 textureX, s32 paletteY, s32 semiTransparent, s32 brightness, s32 rawTexture)
{
    enum {
        ACTOR_450200_BACKDROP_SPRITES       = 2,
        ACTOR_450200_BACKDROP_SPRITE_WIDTH  = 256,
        ACTOR_450200_BACKDROP_SCREEN_HALF_X = 160,
        ACTOR_450200_BACKDROP_SCREEN_HALF_Y = 120,
        ACTOR_450200_BACKDROP_PAGE_WORDS    = 128,
        ACTOR_450200_BACKDROP_TEXTURE_8_BIT = 1,
        ACTOR_450200_BACKDROP_SORTING_TAG   = 1022,
    };
    SPRT*    sprite;
    DR_MODE* drawMode;
    s32      tileIndex;

    for (tileIndex = 0; tileIndex < ACTOR_450200_BACKDROP_SPRITES; tileIndex++) {
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setSprt(sprite);
        setShadeTex(sprite, rawTexture);
        setSemiTrans(sprite, semiTransparent);
        sprite->x0   = screenX - ACTOR_450200_BACKDROP_SCREEN_HALF_X;
        sprite->y0   = -ACTOR_450200_BACKDROP_SCREEN_HALF_Y;
        sprite->w    = ACTOR_450200_BACKDROP_SPRITE_WIDTH;
        sprite->u0   = 0;
        sprite->v0   = 0;
        sprite->h    = DISPLAY_SCREEN_HEIGHT;
        sprite->r0   = brightness;
        sprite->g0   = brightness;
        sprite->b0   = brightness;
        sprite->clut = GetClut(0, paletteY);
        addPrim(&gGpuCurrentOt[ACTOR_450200_BACKDROP_SORTING_TAG], sprite);

        // Prepending the draw mode makes it execute before its sprite.
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 0, 1, getTPage(ACTOR_450200_BACKDROP_TEXTURE_8_BIT, GPU_BLEND_ADD, textureX, ACTOR_450200_BACKDROP_TEXTURE_Y));
        addPrim(&gGpuCurrentOt[ACTOR_450200_BACKDROP_SORTING_TAG], drawMode);

        textureX += ACTOR_450200_BACKDROP_PAGE_WORDS;
        screenX  += ACTOR_450200_BACKDROP_SPRITE_WIDTH;
    }
}

/// Scales and uploads a 256-entry RGB555 backdrop palette with bit 15 set.
///
/// `scale` is a Q7 weight, normally 0..128. Both arrays contain 256 halfwords;
/// they may be identical, but otherwise must not overlap. `scaledColors` must
/// be word-aligned and remain live until the GPU transfer completes. Uploads
/// one row at VRAM word coordinates (0, `vramY`); retains no source pointer.
static inline void _actor450200LoadScaledClut(const u16* sourceColors, u16* scaledColors, s32 scale, s32 vramY)
{
    enum {
        ACTOR_450200_CLUT_CHANNEL_MASK     = 31,
        ACTOR_450200_CLUT_COLOR_MASK       = 0x7FFF,
        ACTOR_450200_CLUT_SCALE_SHIFT      = 7,
        ACTOR_450200_CLUT_SCALED_BYTE_MASK = 255,
    };
    RECT rect;
    s32  colorIndex;
    u32  channelValue;
    u32  green;
    u32  red;
    u32  packedColor;

    for (colorIndex = 0; colorIndex < ACTOR_450200_BACKDROP_CLUT_COLORS; colorIndex++) {
        channelValue             = ((sourceColors[colorIndex] >> 10) & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        green                    = ((sourceColors[colorIndex] >> 5) & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        red                      = ((u8)sourceColors[colorIndex] & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        packedColor              = channelValue >> ACTOR_450200_CLUT_SCALE_SHIFT;
        green                  >>= ACTOR_450200_CLUT_SCALE_SHIFT;
        packedColor             &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        packedColor            <<= 10;
        packedColor             |= ~ACTOR_450200_CLUT_COLOR_MASK;
        green                   &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        green                  <<= 5;
        packedColor             |= green;
        channelValue             = red >> ACTOR_450200_CLUT_SCALE_SHIFT;
        channelValue            &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        channelValue            |= packedColor;
        scaledColors[colorIndex] = channelValue;
    }
    setRECT(&rect, 0, vramY, ACTOR_450200_BACKDROP_CLUT_COLORS, 1);
    LoadImage(&rect, (u_long*)scaledColors);
}

/// Cross-fades the scene's two backdrop layers while raising beam intensity.
///
/// State 0 captures two 256-colour source palettes; state 1 lowers the outgoing
/// Q7 weight by four per tick and draws the destination opaquely after the fade.
/// View 8 kills the task without restoring palettes. Requires the observatory
/// overlay, backdrop textures and frame drawing resources. Palette buffers are
/// shared, so the scene must run only one of these tasks at a time.
static void _actor450200BackdropCrossFadeTask(Task* task)
{
    enum {
        ACTOR_450200_BACKDROP_CAPTURE            = 0,
        ACTOR_450200_BACKDROP_DRAW               = 1,
        ACTOR_450200_BACKDROP_TO_SCREEN_X        = -64,
        ACTOR_450200_BACKDROP_WEIGHT_STEP        = 4,
        ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS = 128,
    };
    RECT rect;
    s32  beamIntensity;

    if (gGameSession->location.view == ACTOR_450200_BACKDROP_END_VIEW) {
        taskKill(task);
        return;
    }

    switch (task->state) {
        case ACTOR_450200_BACKDROP_CAPTURE:
            task->killCountdown = ACTOR_450200_BACKDROP_SCALE_ONE;
            task->state        += 1;
            setRECT(&rect, 0, ACTOR_450200_BACKDROP_FROM_SOURCE_CLUT_Y, ACTOR_450200_BACKDROP_CLUT_COLORS, 1);
            StoreImage(&rect, (u_long*)_gActor450200BackdropFromClut);
            rect.y = ACTOR_450200_BACKDROP_TO_SOURCE_CLUT_Y;
            StoreImage(&rect, (u_long*)_gActor450200BackdropToClut);
            break;

        case ACTOR_450200_BACKDROP_DRAW:
            // Add the incoming palette over the outgoing one using complementary weights.
            if (task->killCountdown >= 0) {
                _actor450200DrawBackdropLayer(ACTOR_450200_BACKDROP_TO_SCREEN_X, ACTOR_450200_BACKDROP_TO_TEXTURE_X, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y, true, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
                _actor450200DrawBackdropLayer(0, ACTOR_450200_BACKDROP_FROM_TEXTURE_X, ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y, false, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
                _actor450200LoadScaledClut(_gActor450200BackdropFromClut, _gActor450200BackdropScaledFromClut, task->killCountdown, ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y);
                _actor450200LoadScaledClut(_gActor450200BackdropToClut, _gActor450200BackdropScaledToClut, ACTOR_450200_BACKDROP_SCALE_ONE - task->killCountdown, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y);
            } else {
                _actor450200DrawBackdropLayer(ACTOR_450200_BACKDROP_TO_SCREEN_X, ACTOR_450200_BACKDROP_TO_TEXTURE_X, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y, false, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
            }

            // Clamp after low-halfword wrapping, before passing the beam level to the room.
            beamIntensity = ACTOR_450200_BEAM_FULL_INTENSITY - (u16)task->killCountdown;
            if ((u32)(beamIntensity & 0xFFFF) >= (u32)ACTOR_450200_BEAM_FULL_INTENSITY) {
                beamIntensity = ACTOR_450200_BEAM_FULL_INTENSITY;
            }
            neoArkObservatorySetLightBeamIntensity(beamIntensity & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - ACTOR_450200_BACKDROP_WEIGHT_STEP;
            break;
    }
}

/// Starts a backdrop cross-fade for command 1; other commands do nothing.
///
/// The scene must avoid overlapping starts; the task uses shared palette buffers.
/// Spawn failure is ignored. View 8 terminates the task after the scene finishes.
static void _actor450200StartBackdropCrossFade(s32 command)
{
    if (command == ACTOR_450200_BACKDROP_COMMAND_START) {
        taskSpawnFromTable(&_gActor450200BackdropCrossFadeTaskDesc, 0, 0, 0);
    }
}

/// Supplies an event argument's low halfword to the observatory beam setter.
///
/// The room overlay must be loaded. This adapter zero-extends the argument;
/// the setter stores it as a signed halfword without clamping. Scenes use 160.
static void _actor450200SetLightBeamIntensity(s32 intensity)
{
    neoArkObservatorySetLightBeamIntensity(intensity & 0xFFFF);
}

/// Updates the current area's room selector in the session and live save state.
///
/// The selector must be valid for the current area; both scene paths supply 2.
/// Performs no room loading or view change and leaves the other save slots alone.
static void _actor450200SetRoomVariant(u8 roomVariant)
{
    gGameSession->location.roomState                            = roomVariant;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomVariant;
}
