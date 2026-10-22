#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/captions.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/shelter_r48.h"

/// Work block of a drift-sprite emitter, a coordinate task that sheds Shelter
/// R48's drift sprites from one fixed point of the room.
///
/// Each sprite is spawned on the emitter's own coordinate, drifting narrowly
/// upward, with `spriteSize` and `cellPeriod` packed into its spawn argument.
/// While the room's scene state winds the emitter down the sprites shrink,
/// run through their cells faster and come less often, and the emitter ends
/// once `spawnInterval` passes sixteen frames.
///
/// The emitter's task allocates the block zeroed and keeps it in `Task::work`.
typedef struct {
    s32     spriteSize;    // Size the next sprite is spawned with, a perspective numerator; only bits 0..11 reach the sprite. Shrinks to a floor while winding down
    s32     cellPeriod;    // Frames the next sprite shows each animation cell, in 1/4096 of a frame; only the whole frames reach the sprite. Shrinks to one frame while winding down
    Fixed16 spawnInterval; // Running frames between sprites: one is spawned when the task's frame count passes the integer half. Grows while winding down
} _Actor503500DriftSpriteEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500DriftSpriteEmitterWork, 0xC);

static void _actor503500DriftSpriteEmitterTask(Task* task);
static void _actor503500FadeFromBlackTask(Task* task);
static void _actor503500StaffCardSceneTask(Task* task);

extern AnimationSet gAyaStandClip;
static AnimationSet _gBrahmanPlayerIntro1Clip;
static AnimationSet _gBrahmanPlayerIntro4Clip;
static AnimationSet _gBrahmanPlayerIntro6Clip;
static AnimationSet _gBrahmanPlayerIntro5Clip;
static AnimationSet _gBrahmanPlayerIntro2Clip;
static AnimationSet _gBrahmanPlayerDefeat2Clip;
static AnimationSet _gBrahmanPlayerDefeat3Clip;
static AnimationSet _gBrahmanPlayerDefeat4Clip;
static AnimationSet _gAyaBraceForFightClip;
static AnimationSet _gBrahmanPlayerIntro3Clip;

static PadScriptCmd              _gActor503500TentacleMoveVibrationCommands[2];
static PadScriptCmd              _gActor503500StaffCardVibrationCommands[3];
static PadScriptVibrationSegment _gActor503500TentacleMoveVibrationSegments[2];
static PadScriptVibrationSegment _gActor503500StaffCardVibrationSegments[3];
static void                      _actor503500StageSceneAudioStart(void);
static void                      _actor503500EnqueueScenePlayback(void);
static void                      _actor503500FinishScene(void);
static void                      _actor503500CancelScene(void);
static void                      _actor503500ReloadPodBottom(void);
static void                      _actor503500SpawnDriftSpriteEmitter(s32 positionIndex);
static void                      _actor503500SpawnFadeFromBlack(s32 holdFrames);
static void                      _actor503500CancelRoomEffects(void);
static void                      _actor503500ReleaseBossBattle(s8 endDelayFrames);
static void                      _actor503500AddSessionFlowFlags(s32 flowFlags);
static void                      _actor503500LockAttachmentsForEvent(void);
static void                      _actor503500RequestViewRespawn(void);
static void                      _actor503500SetStaffCardUseState(s32 cardUseState);
static void                      _actor503500SetRoomBackgroundSpritesVisible(s32 visible);
static void                      _actor503500ClearSavedPlayerTransform(void);
static void                      _actor503500SavePlayerTransform(void);
static void                      _actor503500RestorePlayerTransform(void);
static void                      _actor503500SetPlayerUpdateHold(u8 holdPlayerUpdate);
static void                      _actor503500ResetPlayerWeaponAttack(void);
static void                      _actor503500HaltPadScript(void);

/// R48 scene recipients and CAP commands used by this file's event records.
enum {
    ACTOR_503500_SCENE_ACTOR_BOSS            = 0,
    ACTOR_503500_SCENE_ACTOR_TENTACLE        = 1,
    ACTOR_503500_SCENE_CHILD_SLIDER_FIRST    = 32,
    ACTOR_503500_SCENE_CHILD_SLIDER_SECOND   = 33,
    ACTOR_503500_CAP_COMMAND_BOSS_INTRO      = 2,
    ACTOR_503500_CAP_COMMAND_BOSS_DEFEAT     = 3,
    ACTOR_503500_TENTACLE_ANIMATION_MOVE     = 1,
    ACTOR_503500_TENTACLE_ANIMATION_COLLAPSE = 2,
    ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER = 0,
    ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE    = 1,
    ACTOR_503500_SCENE_DRAW_HIDE_RELEASE     = 2,
    ACTOR_503500_TENTACLE_VIBRATION_RAMP     = 0,
    ACTOR_503500_TENTACLE_VIBRATION_HOLD     = 1,
    ACTOR_503500_STAFF_CARD_VIBRATION_HOLD   = 0,
    ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_1 = 1,
    ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_2 = 2,
};

/// Progress value installed by both normal and skipped Shelter R48 entry scripts.
enum { ACTOR_503500_STAFF_CARD_USE_READY = 1 };

/// Execution order and hit presentations of this translation unit's spawn recipes.
enum {
    ACTOR_503500_SCENE_EFFECT_TASK_PRIORITY      = 192,
    ACTOR_503500_ENEMY_TASK_PRIORITY             = 96,
    ACTOR_503500_ATTACK_TASK_PRIORITY            = 192,
    ACTOR_503500_ATTACK_REACTION_BODY_BLAST      = 6,
    ACTOR_503500_ATTACK_REACTION_SPARK_AND_PUFFS = 7,
};

static TmdSource _gBrahmanTorso;
static TmdSource _gBrahmanPinkFlashEmitter;
static TmdSource _gBrahmanLargeChainLeft;
static TmdSource _gBrahmanLargeChainRight;
static TmdSource _gBrahmanLungingChainRight2;
static TmdSource _gBrahmanLungingChainRight1;
static TmdSource _gBrahmanLungingChainLeft2;
static TmdSource _gBrahmanLungingChainLeft1;
static TmdSource _gBrahmanArmRight;
static TmdSource _gBrahmanArmLeft;

#include "assets/enemies/brahman/clips_1.inc"

TaskDesc gActor503500StaffCardSceneTaskDesc = { { { TASK_BODY_NONE, ACTOR_503500_SCENE_EFFECT_TASK_PRIORITY } }, _actor503500StaffCardSceneTask, { .value = 0 } };

TaskDesc gActor503500SceneEffectTaskDescs[ACTOR_503500_SCENE_TASK_COUNT] = {
    [ACTOR_503500_SCENE_TASK_DRIFT_EMITTER]   = { { { TASK_BODY_COORD, ACTOR_503500_SCENE_EFFECT_TASK_PRIORITY } }, _actor503500DriftSpriteEmitterTask, { .value = 0 } },
    [ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK] = { { { TASK_BODY_NONE, ACTOR_503500_SCENE_EFFECT_TASK_PRIORITY } }, _actor503500FadeFromBlackTask, { .value = 0 } },
};

/// Fixed room-coordinate origins for Shelter R48 drift-sprite emitters.
///
/// Spawn argument 1 indexes 0..3 without a bounds check. The intro uses 0..2;
/// the defeat uses 0, 1 and 3. Y is 1000 game units at every origin.
static SVECTOR _gActor503500DriftSpriteEmitterPositions[4] = {
    { 5000, 1000, 5000, 0 },
    { 8000, 1000, 7000, 0 },
    { 9000, 1000, 2000, 0 },
    { 6000, 1000, 9000, 0 },
};

/// Player clips copied into the equipped animation bank for the R48 scenes.
///
/// The twelve pointers fill ids 47..58; id 47 is NULL and ids 48..58 are
/// playable sets. The descriptors and pose data are borrowed during playback
/// and require this overlay to remain loaded.
#include "assets/enemies/brahman/player_scene_bank.inc"

/// Installs the twelve R48 scene-set pointers in the equipped player bank.
///
/// Copies the entire pointer table, including its leading NULL, to ids 47..58.
/// The request is borrowed during dispatch; copied sets remain borrowed afterward.
static AnimationBankCopyRequest _gActor503500PlayerSceneAnimationBankCopy = { _gActor503500PlayerSceneAnimationSets, ARRAY_SIZE(_gActor503500PlayerSceneAnimationSets) };

/// Retained player bank-seed-1 clip-1 reset request with no script consumer.
///
/// World collision is disabled and reset ignores the zero blend duration.
static AnimationPlayRequest _gActor503500UnusedPlayerAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player playback request used when the defeat, card-use and phase scenes start.
///
/// Plays extension clip 48 with a twenty-frame blend and disables world collision.
/// The script copies this record and replaces its bank selector with the equipped
/// weapon bank before dispatch. Its visual motion is not established.
static AnimationPlayRequest _gActor503500PlayerSceneStartAnimation = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player playback requests for the R48 intro and defeat scenes.
///
/// Elements 0..9 play extension clips 49..58 with world collision disabled.
/// The script copies each record and replaces its bank selector with the equipped
/// weapon bank. The intro uses 0..3 and 8..9; the defeat uses 4..7. Reset requests
/// ignore their zero blend duration; other durations are whole animation frames.
static AnimationPlayRequest _gActor503500PlayerSceneAnimations[] = {
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 5, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT + 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Player playback request installed at the end of the normal or skipped intro.
///
/// Resets equipped-bank clip 9 with world collision disabled before battle control
/// resumes. The script copies the record and replaces its stored bank selector.
static AnimationPlayRequest _gActor503500PlayerBattleStartAnimation = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player placement at the start of the R48 boss intro.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500PlayerIntroTransform = { { 2500, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

/// Player placement shared by the normal and skipped intro endings.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500PlayerBattleStartTransform = { { 2000, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

/// Player placement at the start of the R48 boss defeat scene.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500PlayerDefeatStartTransform = { { 1990, -2000, 6580, 0 }, { 0, 910, 0, 0 } };

/// Player placement shared by the normal and skipped defeat endings.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500PlayerDefeatEndTransform = { { 2240, -2000, 6770, 0 }, { 0, 910, 0, 0 } };

/// Temporary player placement while the boss changes phase.
///
/// The script saves and later restores the live player placement. Position is in
/// whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500PlayerPhaseTransitionTransform = { { 2000, 0, 1000, 0 }, { 0, 0, 0, 0 } };

/// Boss playback requests for its intro and defeat scenes.
///
/// Entries 0/1 duplicate source-1 clip 1 and are not dispatched by these scripts;
/// 2 plays source-1 clip 2, 3 source-0 clip 14, and 4 source-1 clip 4 with a
/// forty-frame blend. The boss ignores the world-collision request word.
static AnimationPlayRequest _gActor503500BossSceneAnimations[] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 40, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Boss placements for the start of the intro and defeat scenes.
///
/// Elements 0/1 select intro/defeat. Position is in whole room-coordinate units;
/// Euler angles use 4096 units per turn.
static ActorTransform _gActor503500BossSceneTransforms[] = {
    { { 5200, 5200, 6500, 0 }, { 0, -1024, 0, 0 } },
    { { 8490, 1000, 6210, 0 }, { 0, -682, 0, 0 } },
};

/// Boss placement shared by the normal and skipped intro endings.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 units per turn.
static ActorTransform _gActor503500BossBattleStartTransform = { { 7000, 1000, 7000, 0 }, { 0, -1024, 0, 0 } };

/// Temporary boss placement for the phase-transition scene.
///
/// The phase command saves the live root before placement; the restore command
/// puts it back. Position is in whole room units, angles in 4096 units per turn.
static ActorTransform _gActor503500BossPhaseTransitionTransform = { { 7300, 500, 7600, 0 }, { 0, 1536, 0, 0 } };

/// Returns the boss to idle when the normal or skipped intro ends.
static ActorCommand _gActor503500BossResumeCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_RESUME };

/// Boss hold and collapse commands used by the intro and defeat scenes.
///
/// Element 0 holds the boss during the intro; element 1 starts its defeat collapse.
/// The R48 context tags identify the sender; this receiver reads only the command.
static ActorCommand _gActor503500BossSceneCommands[] = {
    { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_HOLD },
    { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_COLLAPSE },
};

/// Advances the boss task to teardown at either ending of the defeat scene.
static ActorCommand _gActor503500BossExitCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_EXIT };

/// Saves the boss root and starts its phase-transition sequence.
static ActorCommand _gActor503500BossPhaseTransitionCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_BEGIN_PHASE_TRANSITION };

/// Restores the boss root saved before the phase-transition scene.
///
/// Restores yaw and the complete coordinate, marks composition dirty and latches
/// completion so the boss can finish the transition. The R48 context is ignored.
static ActorCommand _gActor503500BossRestoreRootCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_BOSS_COMMAND_RESTORE_ROOT };

/// Retained bank-0 reset matching the tentacle's move clip, with no consumer.
///
/// World collision is disabled and reset ignores the zero blend duration.
static AnimationPlayRequest _gActor503500UnusedTentacleAnimation1Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_503500_TENTACLE_ANIMATION_MOVE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Tentacle bank-0 clip played during its defeat-scene translation.
///
/// Resets clip 1 without blending. The nineteen-part receiver ignores the
/// collision word; the request is borrowed synchronously by message dispatch.
static AnimationPlayRequest _gActor503500TentacleMoveAnimation = { { .index = 0 }, ACTOR_503500_TENTACLE_ANIMATION_MOVE, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Tentacle bank-0 clip played during its translating collapse.
///
/// Selects clip 2 with a four-frame blend request. The nineteen-part receiver
/// ignores the collision word; the request is borrowed synchronously.
static AnimationPlayRequest _gActor503500TentacleCollapseAnimation = { { .index = 0 }, ACTOR_503500_TENTACLE_ANIMATION_COLLAPSE, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

/// Tentacle placement before its defeat-scene translation and collapse.
///
/// Position is in whole room-coordinate units; Euler angles use 4096 per turn.
/// The placement message borrows the record synchronously.
static ActorTransform _gActor503500TentacleDefeatStartTransform = { { 5190, 3920, 7820, 0 }, { 0, 1024, 0, 0 } };

/// Two unused placements retained beside the tentacle's defeat-scene start pose.
///
/// Positions use whole room-coordinate units; both yaws are a quarter turn.
/// No script places a task from these records, so their scene beats are unproven.
static ActorTransform _gActor503500UnusedTentaclePlacements[2] = {
    { { 11100, 560, 7970, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { 12850, 1610, 7970, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
};

/// Stops the scripted tentacle after either defeat-scene movement leg.
///
/// Clears all three velocity words while retaining fractional position carry
/// and collapse progress. The receiver ignores the R48 context.
static ActorCommand _gActor503500TentacleStopCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_TENTACLE_COMMAND_STOP };

/// Starts the tentacle translation that the defeat script stops after 23 updates.
///
/// Sets fixed-point velocity for a (5910, -3360, 150) room-unit displacement
/// over 23 ticks; it does not arm a timer. The receiver ignores the R48 context.
static ActorCommand _gActor503500TentacleMoveCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_TENTACLE_COMMAND_MOVE };

/// Starts the tentacle squash and its second defeat-scene translation.
///
/// Sets X/Y velocity and collapse motion without resetting collapse progress;
/// the script later stops it. The receiver ignores the R48 context.
static ActorCommand _gActor503500TentacleCollapseCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_TENTACLE_COMMAND_COLLAPSE };

/// Releases the scripted tentacle at either ending of the defeat scene.
///
/// This is scene child 1, whose command namespace is separate from the boss
/// at child 0; the tentacle receiver invokes its exit callback immediately.
static ActorCommand _gActor503500TentacleExitCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_TENTACLE_COMMAND_EXIT };

/// Initial placement of card-scene slab slider child 32.
///
/// Uses whole room-coordinate units and 4096 angle units per turn. The slab
/// starts here before path 1; placement borrows the record synchronously.
static ActorTransform _gActor503500SliderFirstCardSceneStartTransform = { { 3350, -3360, 11650, 0 }, { 0, -512, 0, 0 } };

/// Initial placement of card-scene slab slider child 33.
///
/// Uses whole room-coordinate units and 4096 angle units per turn. The slab
/// starts here before path 2; placement borrows the record synchronously.
static ActorTransform _gActor503500SliderSecondCardSceneStartTransform = { { 12570, -3360, 2430, 0 }, { 0, 1535, 0, 0 } };

/// Final placement of card-scene slab slider child 32 before motion stops.
///
/// Uses whole room-coordinate units and 4096 angle units per turn. Both slabs
/// share this position with different headings; dispatch borrows the record.
static ActorTransform _gActor503500SliderFirstCardSceneEndTransform = { { 7960, -3360, 7040, 0 }, { 0, -512, 0, 0 } };

/// Final placement of card-scene slab slider child 33 before motion stops.
///
/// Uses whole room-coordinate units and 4096 angle units per turn. Both slabs
/// share this position with different headings; dispatch borrows the record.
static ActorTransform _gActor503500SliderSecondCardSceneEndTransform = { { 7960, -3360, 7040, 0 }, { 0, 1535, 0, 0 } };

/// Stops both slab sliders at the end of the card-use scene.
///
/// Clears path, shake countdown and display shake. The R48 context is ignored.
static ActorCommand _gActor503500SliderStopCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_SLIDER_COMMAND_STOP };

/// Restarts slab child 32 on its first 360-update path during card use.
static ActorCommand _gActor503500SliderPathFirstCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_SLIDER_COMMAND_PATH_FIRST };

/// Restarts slab child 33 on its second 360-update path during card use.
static ActorCommand _gActor503500SliderPathSecondCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_SLIDER_COMMAND_PATH_SECOND };

/// Starts stationary slab shaking before the card scene begins sliding.
///
/// Arms a 10000-update countdown; the later stop command clears the shake.
static ActorCommand _gActor503500SliderShakeCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48 } }, ACTOR_503500_SLIDER_COMMAND_SHAKE };

/// Scene/audio stream key for the Shelter R48 boss intro (group 6, stream 10, qualifier 11).
///
/// The event and CAP controllers borrow this record until the scene ends.
static EvsSceneKey _gActor503500BossIntroSceneKey = { .group = 6, .streamId = 10, .subId = 11 };

/// Scene/audio stream key for the Shelter R48 boss defeat (group 6, stream 11, qualifier 11).
///
/// The event and CAP controllers borrow this record until the scene ends.
static EvsSceneKey _gActor503500BossDefeatSceneKey = { .group = 6, .streamId = 11, .subId = 11 };

/// Scene/audio stream key for accepted staff-card use (group 6, stream 80, qualifier 11).
///
/// The event and CAP controllers borrow this record until the scene ends.
static EvsSceneKey _gActor503500StaffCardSceneKey = { .group = 6, .streamId = 80, .subId = 11 };

EvsCommand gActor503500BossIntroScript[56] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor503500BossIntroSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerIntroTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = _gActor503500BossSceneTransforms } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = _gActor503500BossSceneCommands } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnFadeFromBlack }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ClearSavedPlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = _gActor503500PlayerSceneAnimations }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_503500_CAP_COMMAND_BOSS_INTRO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[9] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[8] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor503500BossSceneAnimations[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor503500BossSceneAnimations[4] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerBattleStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerBattleStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500BossBattleStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossResumeCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor503500BossIntroSkipScript[29] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerBattleStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerBattleStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500BossBattleStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossResumeCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500HaltPadScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ClearSavedPlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor503500BossDefeatScript[61] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor503500BossDefeatSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_REEQUIP_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerDefeatStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500BossSceneTransforms[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor503500BossSceneAnimations[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossSceneCommands[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_503500_CAP_COMMAND_BOSS_DEFEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerDefeatEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500TentacleDefeatStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor503500TentacleMoveAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleMoveCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor503500TentacleMoveVibrationCommands }, { .vibrationSegments = _gActor503500TentacleMoveVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleStopCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor503500TentacleCollapseAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleCollapseCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneAnimations[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleStopCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor503500ReleaseBossBattle }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetStaffCardUseState }, { .value = ACTOR_503500_STAFF_CARD_USE_READY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACTOR_503500_DEFEAT_SKIP_END_DELAY_FRAMES = 11 };

EvsCommand gActor503500BossDefeatSkipScript[28] = {
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500HaltPadScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_REEQUIP_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerDefeatEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor503500ReleaseBossBattle }, { .value = ACTOR_503500_DEFEAT_SKIP_END_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_TENTACLE }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500TentacleExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetStaffCardUseState }, { .value = ACTOR_503500_STAFF_CARD_USE_READY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_HIDE_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACTOR_503500_CAP_COMMAND_STAFF_CARD = 4 };

/// Staff-card scene that shakes and slides both slabs before reloading pod bottom.
///
/// Thirty-two commands including END. Borrows R48 actors, scene/CAP resources,
/// player clip requests, both slider paths and vibration data. The bodyless
/// launcher ends after starting this script; the actor overlay stays loaded
/// through the scene and borrowed playback.
static EvsCommand _gActor503500StaffCardScript[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor503500StaffCardSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor503500SetPlayerUpdateHold }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500SliderFirstCardSceneStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500SliderSecondCardSceneStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500RequestViewRespawn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_503500_CAP_COMMAND_STAFF_CARD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500SliderShakeCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_503500_SCENE_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500SliderPathFirstCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500SliderPathSecondCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500SliderFirstCardSceneEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500SliderSecondCardSceneEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_FIRST }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500SliderStopCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = ACTOR_503500_SCENE_CHILD_SLIDER_SECOND }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500SliderStopCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor503500StaffCardVibrationCommands }, { .vibrationSegments = _gActor503500StaffCardVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ReloadPodBottom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Staff-card skip path restoring player playback and reloading pod bottom.
///
/// Eight commands including END. Borrows the same scene resources as the normal
/// stream; fades, cancels and cleans up the scene, then reloads pod bottom.
static EvsCommand _gActor503500StaffCardSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ReloadPodBottom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACTOR_503500_PHASE_TRANSITION_CAP_COMMAND = 5 };

EvsCommand gActor503500BossPhaseTransitionScript[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor503500PlayerSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor503500PlayerSceneStartAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ResetPlayerWeaponAttack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500SavePlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500PlayerPhaseTransitionTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossPhaseTransitionCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor503500BossPhaseTransitionTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_503500_PHASE_TRANSITION_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500RestorePlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_503500_SCENE_ACTOR_BOSS }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor503500BossRestoreRootCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Two-lane vibration script started with the tentacle defeat-scene move.
///
/// The binary lane plays the eight-frame segment; the variable lane plays the
/// twenty-frame ramp. Both then stop. Segment indices are 0..1 and step indices
/// 0..1; the spawned vibration task borrows both tables until it ends.
static PadScriptCmd _gActor503500TentacleMoveVibrationCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_TENTACLE_VIBRATION_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_TENTACLE_VIBRATION_RAMP) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Vibration segments for the tentacle move: a 255-to-107 ramp and an eight-frame binary hold.
///
/// Intensities are bytes; durations are eligible vibration-script frames.
/// The last byte is preserved but has no interpreter read or established role.
static PadScriptVibrationSegment _gActor503500TentacleMoveVibrationSegments[2] = {
    [ACTOR_503500_TENTACLE_VIBRATION_RAMP] = { .startIntensity = 255, .endIntensity = 107, .durationFrames = 20, .field_3 = 1 },
    [ACTOR_503500_TENTACLE_VIBRATION_HOLD] = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 8, .field_3 = 0 },
};

/// Two-lane vibration script played after the card-scene slabs meet.
///
/// The binary lane holds for eight frames then stops. The variable lane plays
/// an eight-frame ramp then an eleven-frame ramp and stops. Reachable command
/// and segment indices are 0..2; both tables are borrowed until task teardown.
static PadScriptCmd _gActor503500StaffCardVibrationCommands[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_STAFF_CARD_VIBRATION_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Vibration segments for card use: a binary hold and two variable-motor ramps.
///
/// Intensities are bytes and durations eligible script frames. The final stored
/// byte has no interpreter read or established role. All three entries are used.
static PadScriptVibrationSegment _gActor503500StaffCardVibrationSegments[3] = {
    [ACTOR_503500_STAFF_CARD_VIBRATION_HOLD]   = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 8, .field_3 = 0 },
    [ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_1] = { .startIntensity = 254, .endIntensity = 252, .durationFrames = 8, .field_3 = 1 },
    [ACTOR_503500_STAFF_CARD_VIBRATION_RAMP_2] = { .startIntensity = 255, .endIntensity = 22, .durationFrames = 11, .field_3 = 1 },
};

#include "assets/enemies/brahman/torso.inc"

#include "assets/enemies/brahman/pink_flash_emitter.inc"

#include "assets/enemies/brahman/large_chain_left.inc"

#include "assets/enemies/brahman/large_chain_right.inc"

#include "assets/enemies/brahman/lunging_chain_right_2.inc"

#include "assets/enemies/brahman/lunging_chain_right_1.inc"

#include "assets/enemies/brahman/lunging_chain_left_2.inc"

#include "assets/enemies/brahman/lunging_chain_left_1.inc"

#include "assets/enemies/brahman/arm_right.inc"

#include "assets/enemies/brahman/arm_left.inc"

#include "assets/enemies/brahman/clips_2.inc"

/// Damage rows selected by ballistic-shot kind: 0 chain shot, 1 small orb.
///
/// Both apply paralysis; powers are 20 and 25 before the player HP-band scale.
/// The shot packs the selected row into its attack sphere without a bounds check.
static DamageAttack _gActor503500BallisticShotAttacks[2] = {
    { .power = 20, .reaction = GAME_ACTOR_REACTION_PARALYSIS },
    { .power = 25, .reaction = GAME_ACTOR_REACTION_PARALYSIS },
};

/// Damage rows selected by lingering-shot kind: 0 large orb, 1 chain-base projectile.
///
/// Both have base power 15; the large orb applies poison and the projectile
/// paralysis. The shot packs the selected row without a bounds check.
static DamageAttack _gActor503500LingeringShotAttacks[2] = {
    { .power = 15, .reaction = GAME_ACTOR_REACTION_POISON },
    { .power = 15, .reaction = GAME_ACTOR_REACTION_PARALYSIS },
};

/// Single damage row for the pink-flash capsule: power 40 and body-blast presentation.
static DamageAttack _gActor503500PinkFlashAttacks[1] = {
    { .power = 40, .reaction = ACTOR_503500_ATTACK_REACTION_BODY_BLAST },
};

/// Single damage row for the player-centred yellow flash: power 30 and darkness.
static DamageAttack _gActor503500YellowFlashAttacks[1] = {
    { .power = 30, .reaction = GAME_ACTOR_REACTION_DARKNESS },
};

/// Single damage row for the orange-flash capsule: power 120 and body-blast presentation.
static DamageAttack _gActor503500OrangeFlashAttacks[1] = {
    { .power = 120, .reaction = ACTOR_503500_ATTACK_REACTION_BODY_BLAST },
};

DamageAttack* gActor503500BallisticShotAttackTables[1] = {
    _gActor503500BallisticShotAttacks,
};

DamageAttack* gActor503500LingeringShotAttackTables[1] = {
    _gActor503500LingeringShotAttacks,
};

DamageAttack* gActor503500FlashAttacks[ACTOR_503500_FLASH_ATTACK_COUNT] = {
    [ACTOR_503500_FLASH_ATTACK_PINK]   = _gActor503500PinkFlashAttacks,
    [ACTOR_503500_FLASH_ATTACK_YELLOW] = _gActor503500YellowFlashAttacks,
};

DamageAttack* gActor503500OrangeFlashAttackTables[1] = {
    _gActor503500OrangeFlashAttacks,
};

/// Zero-power ordinary-hit row shared by the boss and its non-striking part records.
///
/// A stored zero row still packs as category 4; it is not the NULL/no-contact sentinel.
static DamageAttack _gActor503500ZeroPowerAttacks[1] = { { .power = 0, .reaction = GAME_ACTOR_REACTION_ORDINARY } };

/// Single lunging-chain contact row: power 25 and spark/puff hit presentation.
static DamageAttack _gActor503500LungingChainAttacks[1] = {
    { .power = 25, .reaction = ACTOR_503500_ATTACK_REACTION_SPARK_AND_PUFFS },
};

/// Single arm-strike damage row shared by both sides: power 45 and spark/puff presentation.
///
/// Forearm and hand contacts dispatch row 0 through the arm enemy parameters.
static DamageAttack _gActor503500ArmAttacks[1] = {
    { .power = 45, .reaction = ACTOR_503500_ATTACK_REACTION_SPARK_AND_PUFFS },
};

EnemyParams gActor503500EnemyParams[ACTOR_503500_SLOT_COUNT] = {
    [ACTOR_503500_SLOT_BODY]                 = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 3500, .exp = 300, .bp = 500, .mp = 200, .critChance = 250, .buildupSteps = 3, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_PINK_FLASH_EMITTER]   = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 1500, .exp = 100, .bp = 500, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_LARGE_CHAIN_0]        = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 500, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
    [ACTOR_503500_SLOT_LARGE_CHAIN_1]        = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 500, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_0]  = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 700, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_1]  = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 700, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_REAR_PART]            = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 1500, .exp = 700, .bp = 2000, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_CHAIN_BASE_0]         = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 700, .exp = 800, .bp = 3000, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_CHAIN_BASE_1]         = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 700, .exp = 800, .bp = 3000, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_SMALL_ORB_EMITTER]    = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 1500, .exp = 200, .bp = 500, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_ARM_0]                = { .attacks = _gActor503500ArmAttacks, .hpMax = 1000, .exp = 1000, .bp = 5000, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_ARM_1]                = { .attacks = _gActor503500ArmAttacks, .hpMax = 1000, .exp = 1000, .bp = 5000, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER] = { .attacks = _gActor503500ZeroPowerAttacks, .hpMax = 700, .exp = 100, .bp = 500, .mp = 0, .critChance = 100, .buildupSteps = 0, .damageOverTimeChance = 0, .damageOverTimeTicks = 0 },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_0]      = { .attacks = _gActor503500LungingChainAttacks, .hpMax = 100, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_1]      = { .attacks = _gActor503500LungingChainAttacks, .hpMax = 100, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_2]      = { .attacks = _gActor503500LungingChainAttacks, .hpMax = 100, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_3]      = { .attacks = _gActor503500LungingChainAttacks, .hpMax = 100, .exp = 0, .bp = 0, .mp = 0, .critChance = 100, .buildupSteps = 10, .damageOverTimeChance = 100, .damageOverTimeTicks = 10 },
};

s8 gActor503500BossAnimationTurnRateClasses[ACTOR_503500_BOSS_ANIMATION_PRESET_COUNT] = {
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_REDUCED,
    ACTOR_503500_TURN_RATE_REDUCED,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_NORMAL,
    ACTOR_503500_TURN_RATE_REDUCED,
    ACTOR_503500_TURN_RATE_REDUCED,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_REDUCED,
    ACTOR_503500_TURN_RATE_MINIMUM,
    ACTOR_503500_TURN_RATE_NORMAL,
};

u8 gActor503500SlotTargetEligibilityMasks[20] = {
    [ACTOR_503500_SLOT_BODY]                 = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_FORWARD,
    [ACTOR_503500_SLOT_PINK_FLASH_EMITTER]   = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_LARGE_CHAIN_0]        = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_LARGE_CHAIN_1]        = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_0]  = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_1]  = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_REAR_PART]            = ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_CHAIN_BASE_0]         = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_CHAIN_BASE_1]         = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_SMALL_ORB_EMITTER]    = ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_ARM_0]                = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_ARM_1]                = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER] = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_BEARING_FORWARD,
    [ACTOR_503500_SLOT_LUNGING_CHAIN_0]      = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_LUNGING_CHAIN_1]      = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE,
    [ACTOR_503500_SLOT_LUNGING_CHAIN_2]      = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    [ACTOR_503500_SLOT_LUNGING_CHAIN_3]      = ACTOR_503500_TARGET_HEIGHT_UPPER | ACTOR_503500_TARGET_HEIGHT_MIDDLE | ACTOR_503500_TARGET_HEIGHT_LOWER | ACTOR_503500_TARGET_BEARING_REAR | ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE,
    0,
    0,
    0,
};

TaskDesc gActor503500EnemyTaskDescs[ACTOR_503500_SLOT_COUNT] = {
    [ACTOR_503500_SLOT_BODY]                 = { { { TASK_BODY_TMD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500BossTask, { .model = &_gBrahmanTorso } },
    [ACTOR_503500_SLOT_PINK_FLASH_EMITTER]   = { { { TASK_BODY_TMD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500PinkFlashEmitterTask, { .model = &_gBrahmanPinkFlashEmitter } },
    [ACTOR_503500_SLOT_LARGE_CHAIN_0]        = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LargeChainTask, { .model = &_gBrahmanLargeChainRight } },
    [ACTOR_503500_SLOT_LARGE_CHAIN_1]        = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LargeChainTask, { .model = &_gBrahmanLargeChainLeft } },
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_0]  = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LargeOrbEmitterTask, { .value = 0 } },
    [ACTOR_503500_SLOT_LARGE_ORB_EMITTER_1]  = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LargeOrbEmitterTask, { .value = 0 } },
    [ACTOR_503500_SLOT_REAR_PART]            = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500RearPartTask, { .value = 0 } },
    [ACTOR_503500_SLOT_CHAIN_BASE_0]         = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500ChainBaseTask, { .value = 0 } },
    [ACTOR_503500_SLOT_CHAIN_BASE_1]         = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500ChainBaseTask, { .value = 0 } },
    [ACTOR_503500_SLOT_SMALL_ORB_EMITTER]    = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500SmallOrbEmitterTask, { .value = 0 } },
    [ACTOR_503500_SLOT_ARM_0]                = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500ArmTask, { .model = &_gBrahmanArmRight } },
    [ACTOR_503500_SLOT_ARM_1]                = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500ArmTask, { .model = &_gBrahmanArmLeft } },
    [ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER] = { { { TASK_BODY_COORD, ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500YellowFlashEmitterTask, { .value = 0 } },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_0]      = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LungingChainTask, { .model = &_gBrahmanLungingChainRight1 } },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_1]      = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LungingChainTask, { .model = &_gBrahmanLungingChainRight2 } },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_2]      = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LungingChainTask, { .model = &_gBrahmanLungingChainLeft1 } },
    [ACTOR_503500_SLOT_LUNGING_CHAIN_3]      = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_503500_ENEMY_TASK_PRIORITY } }, actor503500LungingChainTask, { .model = &_gBrahmanLungingChainLeft2 } },
};

TaskDesc gActor503500AttackTaskDescs[ACTOR_503500_ATTACK_TASK_COUNT] = {
    [ACTOR_503500_ATTACK_TASK_BALLISTIC_SHOT] = { { { TASK_BODY_COORD, ACTOR_503500_ATTACK_TASK_PRIORITY } }, actor503500BallisticShotTask, { .value = 0 } },
    [ACTOR_503500_ATTACK_TASK_LINGERING_SHOT] = { { { TASK_BODY_COORD, ACTOR_503500_ATTACK_TASK_PRIORITY } }, actor503500LingeringShotTask, { .value = 0 } },
    [ACTOR_503500_ATTACK_TASK_PINK_FLASH]     = { { { TASK_BODY_COORD, ACTOR_503500_ATTACK_TASK_PRIORITY } }, actor503500PinkFlashAttackTask, { .value = 0 } },
    [ACTOR_503500_ATTACK_TASK_YELLOW_FLASH]   = { { { TASK_BODY_COORD, ACTOR_503500_ATTACK_TASK_PRIORITY } }, actor503500YellowFlashAttackTask, { .value = 0 } },
    [ACTOR_503500_ATTACK_TASK_ORANGE_FLASH]   = { { { TASK_BODY_COORD, ACTOR_503500_ATTACK_TASK_PRIORITY } }, actor503500OrangeFlashAttackTask, { .value = 0 } },
};

/// Emits drift sprites from a fixed Shelter R48 position until its scene winds down.
///
/// Requires a coordinate body and spawn argument 1 in 0..3. Task teardown owns
/// the allocated work; spawned sprites have independent lifetimes. Only actor-running
/// ticks advance the spawn counter; scene-state wind-down runs on every call.
/// Scene values 0/1 require a running, unskipped event; 2/3 wait for the event
/// to end before shrinking sprites and increasing the interval; 4 only increases
/// the interval. Other values end the task. Initialization also runs the emitter.
static void _actor503500DriftSpriteEmitterTask(Task* task)
{
    enum {
        ACTOR_503500_DRIFT_SPRITE_EMITTER_INITIALIZE          = 0,
        ACTOR_503500_DRIFT_SPRITE_EMITTER_EMITTING            = 1,
        ACTOR_503500_DRIFT_SCENE_INACTIVE                     = 0,
        ACTOR_503500_DRIFT_SCENE_ENTRY                        = 1,
        ACTOR_503500_DRIFT_SCENE_PHASE_TRANSITION             = 2,
        ACTOR_503500_DRIFT_SCENE_DEFEAT                       = 3,
        ACTOR_503500_DRIFT_SCENE_STAFF_CARD                   = 4,
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE         = 0xC00,      // Sprite size until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP    = 0x10,       // Size lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN     = 0x100,      // Floor of the shrinking size
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD         = 0x4000,     // Four frames per cell until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP    = 0x20,       // 1/128 of a frame lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN     = 0x1000,     // Floor of the shrinking period, one frame per cell
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL      = 0x60000,    // Six frames between sprites until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP = 0x1000,     // 1/16 of a frame gained per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX  = 0x100000,   // Sixteen frames; the emitter ends once its interval is longer
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK     = 0xFFF,      // Size bits of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK   = 0xF000,     // Frames-per-cell nibble of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD = 0x03800000, // Movement kind 3 (random, narrowly upward) at speed 0x80
    };
    GfxCoord*                           coord;
    _Actor503500DriftSpriteEmitterWork* work;
    SVECTOR*                            spawnPosition;
    u8                                  finished;

    coord = task->extra.coordBody->coord;
    if (task->state == ACTOR_503500_DRIFT_SPRITE_EMITTER_INITIALIZE) {
        spawnPosition     = &_gActor503500DriftSpriteEmitterPositions[task->spawnArg1.value];
        coord->coord.t[0] = spawnPosition->vx;
        coord->coord.t[1] = spawnPosition->vy;
        coord->coord.t[2] = spawnPosition->vz;
        gfxSetRotIdentity(&coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work                = memCalloc(sizeof(*work), false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work               = work;
        work->spriteSize         = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE;
        work->cellPeriod         = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD;
        work->spawnInterval.word = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL;
        task->state++; // Enters ACTOR_503500_DRIFT_SPRITE_EMITTER_EMITTING; emission runs this tick too.
    }
    work = task->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->spawnInterval.halves.integer < ++task->killCountdown) {
            task->killCountdown = 0;
            effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord,
                        (work->cellPeriod & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK) | ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD | (work->spriteSize & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK), NULL);
        }
    }
    switch (gameFlagGetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE)) {
        case ACTOR_503500_DRIFT_SCENE_INACTIVE:
        case ACTOR_503500_DRIFT_SCENE_ENTRY:
            if (gGameSession->eventState == 0) {
                taskKill(task);
                return;
            }
            finished = gGameSession->evtSkipped;
            break;
        case ACTOR_503500_DRIFT_SCENE_PHASE_TRANSITION:
        case ACTOR_503500_DRIFT_SCENE_DEFEAT:
            if (gGameSession->eventState != 0) {
                return;
            }
            // Winding down: later sprites are smaller and shorter-lived.
            work->cellPeriod -= ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP;
            if (work->cellPeriod < ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN) {
                work->cellPeriod = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN;
            }
            work->spriteSize -= ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP;
            if (work->spriteSize < ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN) {
                work->spriteSize = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN;
            }
            // Fall through: both wind-down modes also stretch the spawn interval.
        case ACTOR_503500_DRIFT_SCENE_STAFF_CARD:
            work->spawnInterval.word += ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP;
            finished                  = work->spawnInterval.word > ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX;
            break;
        default:
            taskKill(task);
            return;
    }
    if (finished) {
        taskKill(task);
    }
}

/// Queues a centred 320x240 subtractive tile and its blend command at OT slot 3.
///
/// Channels are byte subtraction intensities. Requires centred screen coordinates,
/// a writable ordering-table slot 3 and arena space for a TILE and DR_TPAGE.
/// Both packets borrow the current arena until GPU drawing completes. The draw
/// command enables drawing into the displayed area, disables dithering and
/// leaves subtractive blending active afterward.
static inline void _actor503500DrawBlackOverlay(u8 red, u8 green, u8 blue)
{
    enum { BLACK_OVERLAY_WIDTH          = 320,
           BLACK_OVERLAY_HEIGHT         = 240,
           BLACK_OVERLAY_OT_SLOT        = 3,
           BLACK_OVERLAY_TEXTURE_PAGE_X = 320 };
    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -BLACK_OVERLAY_WIDTH / 2;
    tile->y0 = -BLACK_OVERLAY_HEIGHT / 2;
    tile->w  = BLACK_OVERLAY_WIDTH;
    tile->h  = BLACK_OVERLAY_HEIGHT;
    setRGB0(tile, red, green, blue);
    addPrim(gGpuCurrentOt + BLACK_OVERLAY_OT_SLOT, tile);
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_SUBTRACT, BLACK_OVERLAY_TEXTURE_PAGE_X, 0));
    // OT insertion prepends the blend command so it executes before the tile.
    addPrim(gGpuCurrentOt + BLACK_OVERLAY_OT_SLOT, drawMode);
}

/// Holds the screen black for the spawn delay, then reveals it eight shade levels per tick.
///
/// `spawnArg1.value` is a mutable frame countdown; the hold ends once it is
/// negative or the event is skipped. Initialization, hold and reveal advance
/// only while scene actors run, but the overlay draws on every callback.
/// `killCountdown` holds a signed halfword shade, initially 255. Each tick
/// draws its preceding low byte, including initialization and teardown ticks.
/// Requires the current frame arena and OT slot 3; task teardown owns the task.
static void _actor503500FadeFromBlackTask(Task* task)
{
    enum { FADE_INITIALIZE,
           FADE_HOLD,
           FADE_REVEAL,
           FADE_BLACK_LEVEL = 255,
           FADE_SHADE_STEP  = 8 };
    u8 red, green, blue;

    // Sample before updating so the final decrement still draws the preceding shade.
    red = green = blue = task->killCountdown;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (task->state) {
            case FADE_INITIALIZE:
                task->killCountdown = FADE_BLACK_LEVEL;
                task->state++;
                break;
            case FADE_HOLD:
                if (--task->spawnArg1.value < 0 || gGameSession->evtSkipped != 0) {
                    task->state++;
                }
                break;
            case FADE_REVEAL:
                task->killCountdown -= FADE_SHADE_STEP;
                if (task->killCountdown < 0) {
                    taskKill(task);
                }
                break;
            default:
                taskKill(task);
                break;
        }
    }
    _actor503500DrawBlackOverlay(red, green, blue);
}

/// Stages the selected scene's audio start for later CD queue admission.
///
/// Entry scripts first select the scene. Its descriptor and buffers must stay
/// live through deferred request consumption; a missing slot leaves the request alone.
static void _actor503500StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Starts playback of the entry script's selected scene/audio session.
///
/// Prepared descriptor and buffers must stay live through playback. A selected
/// slot needs room in the CD request ring; without a slot, playback mode starts directly.
static void _actor503500EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the selected scene's streaming state and restores its saved random values.
///
/// Normal entry/card-use scripts call this after their final CAP cue. Requires
/// a successful scene selection; buffer and task teardown remain with their owners.
static void _actor503500FinishScene(void)
{
    streamFinishScene();
}

/// Discards deferred scene playback, requests CD cancellation and finishes the scene.
///
/// Entry and skip scripts use this after selecting a scene. Streaming/RNG state
/// finishes immediately; CD cancellation completes through subsequent dispatches.
static void _actor503500CancelScene(void)
{
    cdCmdCancelScene();
}

/// Returns to Shelter B2's pod bottom after normal or skipped staff-card playback.
///
/// Replaces the live save's area, warp and room selectors, retaining stage and view.
/// The resident reload task captures the frame and applies the saved location later.
static void _actor503500ReloadPodBottom(void)
{
    enum {
        ACTOR_503500_POD_BOTTOM_WARP = 1,
        ACTOR_503500_POD_BOTTOM_ROOM = 1,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B2_POD_BOTTOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_503500_POD_BOTTOM_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_503500_POD_BOTTOM_ROOM;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Starts a drift-sprite emitter at room position `positionIndex` (0..3).
///
/// Requires this overlay and Shelter R48 resources loaded. Spawn failure is ignored;
/// successful tasks own their work and follow the room's scene-state lifetime.
static void _actor503500SpawnDriftSpriteEmitter(s32 positionIndex)
{
    taskSpawnFromTable(gActor503500SceneEffectTaskDescs, ACTOR_503500_SCENE_TASK_DRIFT_EMITTER, positionIndex, 0);
}

/// Starts the entry scene's fade from black and publishes its borrowed task handle.
///
/// `holdFrames` counts actor-running ticks; the hold ends on the first decrement
/// below zero or on event skip. A negative value reveals on the first hold tick.
/// Failure publishes NULL; task teardown owns the fade, whose handle may become stale.
static void _actor503500SpawnFadeFromBlack(s32 holdFrames)
{
    gActor503500FadeFromBlackTask = taskSpawnFromTable(gActor503500SceneEffectTaskDescs, ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK, holdFrames, 0);
}

/// Requests cancellation of all room effects at an entry-script transition.
///
/// Requires the live room-effect state; cancellation is handled by its later updates.
static void _actor503500CancelRoomEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Releases the placed boss's battle hold with rewards and sets the battle-end delay.
///
/// Placement index 0 must remain live while a battle hold exists. The delay is
/// in actor-running frames, copied as a low byte (scripts pass 5 or 11).
/// The task and its enemy work remain owned by the scene.
static void _actor503500ReleaseBossBattle(s8 endDelayFrames)
{
    enum { BOSS_PLACEMENT_INDEX = 0 };

    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(BOSS_PLACEMENT_INDEX), 0x23);
    gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
}

/// Adds session music-loading or weapon-restoration options from an entry script.
///
/// `flowFlags` uses `GAME_SESSION_FLOW_*`; only its low byte is stored.
/// Existing options accumulate until the session's flow handling clears them.
static void _actor503500AddSessionFlowFlags(s32 flowFlags)
{
    gGameSession->flowFlags |= flowFlags;
}

/// Queues normal and skip scripts for accepted staff-card use, then releases this launcher.
///
/// Requires the room and actor overlays loaded and event control already held.
/// The event runner retains the static script tables; it outlives this bodyless task.
static void _actor503500StaffCardSceneTask(Task* task)
{
    evsStartScriptWithSkip(_gActor503500StaffCardScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor503500StaffCardSkipScript);
    taskKill(task);
}

/// Locks attachment activation and cancels an active attachment for the event.
///
/// Attachment updates consume the lock; ordinary activation is blocked while set.
static void _actor503500LockAttachmentsForEvent(void)
{
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Requests deferred respawn of the saved view after the card-use scene begins.
static void _actor503500RequestViewRespawn(void)
{
    gGameSession->viewDirty = true;
}

/// Sets Shelter R48's staff-card progress from a normal or skipped entry script.
///
/// Only the low nibble is stored: 0 inactive, 1 ready for a staff card, 2 card-use
/// started. Both callers install 1; the room advances to 2 on card acceptance.
static void _actor503500SetStaffCardUseState(s32 cardUseState)
{
    gameFlagSetNibble(GAME_FLAG_100, cardUseState);
}

/// Adapts an event-script operand to Shelter R48's background-sprite visibility.
///
/// Only the low byte is passed: 0 hides, 1 shows, other byte values leave the
/// sprites unchanged. Requires the current room's mutable sprite resources.
static void _actor503500SetRoomBackgroundSpritesVisible(s32 visible)
{
    enum {
        ACTOR_503500_EVENT_OPERAND_BYTE_MASK = 0xFF,
    };
    shelterR48SetBackgroundSpritesVisible(visible & ACTOR_503500_EVENT_OPERAND_BYTE_MASK);
}

/// Invalidates the saved player placement by clearing its position sentinel.
///
/// All-zero XYZ suppresses restoration; the saved rotation remains available.
static void _actor503500ClearSavedPlayerTransform(void)
{
    gActor503500SavedPlayerTransform.pos.vx = 0;
    gActor503500SavedPlayerTransform.pos.vy = 0;
    gActor503500SavedPlayerTransform.pos.vz = 0;
}

/// Saves the live player's root translation and Euler angles for a temporary scene placement.
///
/// Requires the player task, model root and `GameActor` work live. Positions use
/// the root's parent-coordinate units; angles use 4096 units per turn. The
/// package retains the copy until cleared or replaced; an origin position
/// cannot be restored because it is also the empty sentinel.
static void _actor503500SavePlayerTransform(void)
{
    Task*     playerTask;
    GfxCoord* rootCoord;
    SVECTOR*  savedRotation;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    rootCoord  = playerTask->extra.tmd->coords;

    gActor503500SavedPlayerTransform.pos.vx = rootCoord->coord.t[0];
    gActor503500SavedPlayerTransform.pos.vy = rootCoord->coord.t[1];
    gActor503500SavedPlayerTransform.pos.vz = rootCoord->coord.t[2];

    savedRotation     = &gActor503500SavedPlayerTransform.rot;
    savedRotation->vx = ((GameActor*)playerTask->work)->rotation.vx;
    savedRotation->vy = ((GameActor*)playerTask->work)->rotation.vy;
    savedRotation->vz = ((GameActor*)playerTask->work)->rotation.vz;
}

/// Restores the saved player placement after the temporary scene, when its position is nonzero.
///
/// Requires a live player with its placement-message handler. Dispatch borrows
/// the package's saved `ActorTransform` synchronously and copies XYZ/angles;
/// the snapshot remains valid for later restoration until cleared or replaced.
static void _actor503500RestorePlayerTransform(void)
{
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((gActor503500SavedPlayerTransform.pos.vx != 0) || (gActor503500SavedPlayerTransform.pos.vy != 0) ||
        (gActor503500SavedPlayerTransform.pos.vz != 0)) {
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &gActor503500SavedPlayerTransform, 0);
    }
}

/// Holds or releases the player's ordinary state tick during the card-use scene.
///
/// Zero permits the tick; any nonzero byte suppresses it without stopping the
/// rest of player movement/collision upkeep. The script passes 1 to hold it.
static void _actor503500SetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    gPlayerActorStateTickHeld = holdPlayerUpdate;
}

/// Resets the player's weapon attack before the temporary card-use scene.
///
/// Requires the live player task and equipped weapon. Resets weapon effects,
/// eases aiming back and disables weapon pair/grid collision before the script
/// holds and temporarily places the player.
static void _actor503500ResetPlayerWeaponAttack(void)
{
    playerActorResetWeaponAttack(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), gPlayerStatus.weapon, 0);
}

/// Stops scripted controller vibration when an entry scene is skipped.
///
/// Raises the script/hold/ramp halt requests and clears script activity and
/// vibration output. Pending script tasks consume the halt requests on update.
static void _actor503500HaltPadScript(void)
{
    padScriptHalt();
    // The binary repeats this clear after the halt helper has already cleared it.
    gGameSession->padScriptFlags = 0;
}

void actor503500ClearFadeFromBlackHandle(s32 unused)
{
    gActor503500FadeFromBlackTask = NULL;
}
