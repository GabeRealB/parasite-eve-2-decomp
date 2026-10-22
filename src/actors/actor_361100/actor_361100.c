#include "actors/actor_361100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actor_403600.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of the tentacle drawn with `_gBrahmanTentacle`, the first
/// of the package's two scripted models: a tube of ten segments about nine
/// times as long as Eve is tall, whose tip carries four two-jointed flaps.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotion19PlayWork` does, which the
/// play handler of the actor-motion library runs on, and the model object
/// borrows `model.light` and `model.color` for as long as the block lives.
///
/// What follows moves the model in a straight line: each tick adds `velocity`
/// to `carry` and moves the root coordinate by the whole units that makes. A
/// move command sets `velocity` to a displacement divided by a frame count and
/// `moveFrames` to that count; placing the actor stops the move.
///
/// The layout is `_Actor361100EveWork`'s with the velocity and the carry
/// in each other's place. No code reads or writes the word after `carry`, the
/// fourth word of `velocity`, or `pad_4A3`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    Fixed16         carry[3];      // X, Y and Z displacement not yet applied; only the fractions survive a tick
    byte            pad_48C[0x4];
    VECTOR          velocity;      // Displacement added each tick, in signed 16.16 units; zero while standing
    s16             moveFrames;    // Ticks the move still has to run; the tick finding 0 applies `velocity` once more, clears it and leaves -1 (0 from the spawn, -1 once no move is counting)
    s8              freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4A3[0x1];
} _Actor361100TentacleWork;
STATIC_ASSERT_SIZEOF(_Actor361100TentacleWork, 0x4A4);

/// Work block of Eve's body, the second of the package's two scripted
/// models.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the model's nineteen-part rig and model
/// state, and the model object borrows `model.light` and `model.color` for as
/// long as the block lives.
///
/// What follows moves the model in a straight line: each tick adds `velocity`
/// to `carry` and moves the root coordinate by the whole units that makes. A
/// move command sets `velocity` to a displacement divided by a frame count and
/// `moveFrames` to that count.
///
/// No code reads or writes the fourth word of `velocity`, `pad_49C` or
/// `pad_4A3`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    VECTOR          velocity;      // Displacement added each tick, in signed 16.16 units; zero while standing
    Fixed16         carry[3];      // X, Y and Z displacement not yet applied; only the fractions survive a tick
    byte            pad_49C[0x4];
    s16             moveFrames;    // Ticks the move still has to run; the tick finding 1 applies `velocity` a last time and clears it (0 no move)
    s8              freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4A3[0x1];
} _Actor361100EveWork;
STATIC_ASSERT_SIZEOF(_Actor361100EveWork, 0x4A4);

/// Retained head-aim task of the pod scene, or NULL before spawn and after teardown.
///
/// The task owns its aim work. Mode changes use this handle to update or kill it;
/// the room's reset only forgets it, so reset requires any prior task to be gone.
static Task* _gActor361100HeadAimTask;

static void _actor361100DrawStreamRefraction(Task* task);

/// Draw modes with deferred primitive-buffer release or manual buffer recovery.
///
/// Modes 0 and 1 use `ACTOR_MESSAGE_DRAW_HIDE` and `ACTOR_MESSAGE_DRAW_SHOW`.
/// Releasing stores mode 2 as the countdown: ticks seeing 2, 1, then 0 free it.
enum {
    ACTOR_361100_DRAW_HIDE_RELEASE         = 2,
    ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER = 3,
};

/// No primitive-buffer release pending in either model task.
enum { ACTOR_361100_BUFFER_FREE_IDLE = -1 };

/// One parent-relative coordinate unit in signed 16.16 translation.
enum { ACTOR_361100_TRANSLATION_ONE = 0x10000 };

/// Script-selected head-aim policy and local descriptor slots.
enum {
    ACTOR_361100_HEAD_AIM_RELAX      = 0,
    ACTOR_361100_HEAD_AIM_TRACK      = 1,
    ACTOR_361100_HEAD_AIM_KILL       = -1,
    ACTOR_361100_HEAD_AIM_TASK_INDEX = 0,
    ACTOR_361100_SHAKE_TASK_INDEX    = 1,
};

/// Execution priority of the package's water, control and scripted model tasks.
enum { ACTOR_361100_TASK_PRIORITY = 192 };

/// Player clip indices: scene pointers extend the equipped bank after its base sets.
///
/// The initial bank word is retained in the authored requests; event playback
/// overwrites it in a local copy with the equipped weapon's actual bank index.
enum {
    ACTOR_361100_PLAYER_INITIAL_BANK  = 1,
    ACTOR_361100_PLAYER_SKIP_CLIP     = 9,
    ACTOR_361100_PLAYER_SCENE_CLIP_48 = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_361100_PLAYER_SCENE_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_361100_PLAYER_SCENE_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_361100_PLAYER_SCENE_CLIP_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_361100_PLAYER_SCENE_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_361100_PLAYER_SCENE_CLIP_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_361100_PLAYER_SCENE_CLIP_54 = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_361100_PLAYER_SCENE_CLIP_55 = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_361100_PLAYER_SCENE_CLIP_56 = ANIMATION_BANK_BASE_SET_COUNT + 9,
};

/// Placed-actor indices used by the pod scene's message payloads.
enum {
    ACTOR_361100_SCENE_BOSS_INDEX     = 0,
    ACTOR_361100_SCENE_TENTACLE_INDEX = 1,
    ACTOR_361100_SCENE_EVE_INDEX      = 2,
};

/// Authored exit selector of both scripted models; each receiver exits on any unknown command.
enum { ACTOR_361100_COMMAND_EXIT = 0xFFFF };

/// View selectors and CAP command slot authored for the pod-bottom entry script.
enum {
    ACTOR_361100_SCENE_VIEW_REFRACTION = 11,
    ACTOR_361100_SCENE_VIEW_FIGHT      = 10,
    ACTOR_361100_SCENE_SKIP_VIEW_LOAD  = 13,
    ACTOR_361100_SCENE_CAP_COMMAND     = 2,
};

/// Indexed clips selected by the scene's two nineteen-part model requests.
enum {
    ACTOR_361100_SCENE_MODEL_BANK      = 0,
    ACTOR_361100_TENTACLE_SCENE_CLIP_1 = 1,
    ACTOR_361100_TENTACLE_SCENE_CLIP_2 = 2,
    ACTOR_361100_TENTACLE_SCENE_CLIP_3 = 3,
    ACTOR_361100_EVE_SCENE_CLIP_1      = 1,
    ACTOR_361100_EVE_SCENE_CLIP_2      = 2,
    ACTOR_361100_EVE_SCENE_CLIP_3      = 3,
    ACTOR_361100_EVE_SCENE_CLIP_4      = 4,
};

/// Receiver-specific selectors stored in the pod scene's actor-command records.
///
/// Boss hide also parks control and disables lock-on. Tentacle stop preserves
/// its carry/countdown; Eve stop clears her countdown while preserving carry.
enum {
    ACTOR_361100_SCENE_BOSS_HIDE           = 8,
    ACTOR_361100_SCENE_TENTACLE_STOP       = 0,
    ACTOR_361100_SCENE_TENTACLE_FIRST_LEG  = 1,
    ACTOR_361100_SCENE_TENTACLE_SECOND_LEG = 2,
    ACTOR_361100_SCENE_TENTACLE_THIRD_LEG  = 3,
    ACTOR_361100_SCENE_EVE_STOP            = 0,
    ACTOR_361100_SCENE_EVE_RISE            = 1,
};

/// Segment indices and full-scale byte intensity used by the scene's vibration scripts.
enum {
    ACTOR_361100_VIBRATION_RAMP_UP        = 0,
    ACTOR_361100_VIBRATION_RAMP_DOWN      = 1,
    ACTOR_361100_VIBRATION_BINARY_HOLD    = 2,
    ACTOR_361100_VIBRATION_FULL_INTENSITY = 255,
};

static void _actor361100TickTentacle(Task* task);
static void _actor361100InitTentacle(Task* task);
static void _actor361100ExitTentacle(Task* task);
static void _actor361100BindTentacleLighting(Task* task);
static void _actor361100TickEve(Task* task);
static void _actor361100InitEve(Task* task);
static void _actor361100ExitEve(Task* task);
static void _actor361100BindEveLighting(Task* task);

/// Lifecycle dispatch of the tentacle: state 0 initializes, 1 updates and 2 exits.
///
/// The dispatcher copies these three callbacks and requires a state in 0..2.
/// Scene-actor pause suspends dispatch; the package must stay loaded through exit.
static const TaskFuncTable3 _gActor361100TentacleStates = {
    {
        _actor361100InitTentacle,
        _actor361100TickTentacle,
        _actor361100ExitTentacle,
    },
};

/// Lifecycle dispatch of Eve's body: state 0 initializes, 1 updates and 2 exits.
///
/// The dispatcher copies these three callbacks and requires a state in 0..2.
/// Scene-actor pause suspends dispatch; the package must stay loaded through exit.
static const TaskFuncTable3 _gActor361100EveStates = {
    {
        _actor361100InitEve,
        _actor361100TickEve,
        _actor361100ExitEve,
    },
};

static AnimationSet _gShelterB2PodBottomTentacleEmergeClip;
static AnimationSet _gShelterB2PodBottomTentacleReachForEveClip;
static AnimationSet _gShelterB2PodBottomTentacleGrabEveClip;
static TmdSource    _gBrahmanTentacle;
static s32          _actor361100PlaceTentacle(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32          _actor361100SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32          _actor361100ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static s32          _actor361100PlayEveAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg);
static s32          _actor361100SetEveDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32          _actor361100ApplyEveCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static void         _actor361100TentacleTask(Task* task);
static void         _actor361100EveTask(Task* task);

static void _actor361100SetHeadAimMode(s32 mode);
static void _actor361100AddFlowFlags(s32 bits);

static PadScriptCmd              _gActor361100TentacleFirstLegVibrationCommands[3];
static PadScriptVibrationSegment _gActor361100TentacleFirstLegVibrationSegments[3];
static void                      _actor361100StageSceneAudioStart(void);
static void                      _actor361100StartScenePlayback(void);
static void                      _actor361100FinishScene(void);
static void                      _actor361100CancelScene(void);
static void                      _actor361100SpawnHeadAimTask(void);
static void                      _actor361100SpawnShakeTask(s32 durationTicks);

static void _actor361100StreamWaterTask(Task* task);
static void _actor361100HeadAimTask(Task* task);
static void _actor361100ShakeTask(Task* task);

TaskDesc gActor361100StreamWaterTaskDesc = { { { TASK_BODY_COORD, ACTOR_361100_TASK_PRIORITY } }, _actor361100StreamWaterTask, { .value = 0 } };

#include "assets/rooms/shelter/b2_pod_bottom/tentacle_scene_clips_1.inc"

/// Bodyless scene-task recipes: index 0 aims the player's head, index 1 shakes the display.
///
/// Both run at priority 192. The first spawn word selects relax/track (0/1) for
/// head aim, or a signed tick countdown for shake; the second word is unused.
/// Their callbacks and any borrowed vibration data must remain loaded until exit.
static TaskDesc _gActor361100SceneTasks[] = {
    { { { TASK_BODY_NONE, ACTOR_361100_TASK_PRIORITY } }, _actor361100HeadAimTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_361100_TASK_PRIORITY } }, _actor361100ShakeTask, { .value = 0 } },
};

/// Ten words installed at equipped player-bank indices 47..56 for the pod scene.
///
/// Index 47 becomes NULL; 48..56 borrow the nine package-owned clips in table
/// order. Clips 54 and 55 reverse their source declaration order. Copying the
/// pointers borrows the descriptors and pose data through playback.
#include "assets/rooms/shelter/b2_pod_bottom/player_scene_bank.inc"

/// Request to install all ten words of the pod scene's player animation extension.
///
/// Synchronous dispatch overwrites indices 47..56 of the equipped weapon bank,
/// including the leading NULL. The request is borrowed only through dispatch;
/// the installed clips must remain loaded while the player plays them.
static AnimationBankCopyRequest _gActor361100CopyPlayerSceneAnimations = {
    .sets  = _gActor361100PlayerSceneAnimationExtension,
    .count = ARRAY_SIZE(_gActor361100PlayerSceneAnimationExtension),
};

/// Player clip 9 requested when the pod scene is skipped.
///
/// Resets without blending and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSkipAnimation = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SKIP_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 48 requested at the pod scene opening.
///
/// Blends for 20 whole frames and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneOpeningAnimation = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 49 requested before the tentacle is shown.
///
/// Resets without blending and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation49 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 50 requested after the tentacle begins its first move.
///
/// Resets without blending and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation50 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 51 requested after Eve's body is shown.
///
/// Blends for 4 whole frames and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation51 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 52 requested after the first faded scene cut.
///
/// Resets without blending and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation52 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 53 requested after the run toward Eve begins.
///
/// Blends for 8 whole frames and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation53 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained player clip-54 request, unused by the pod scene's command streams.
///
/// Selects the installed scene-bank extension with four whole blend frames and
/// no grid participation. The approach instead selects clip 54 by its move-
/// animation record; no code or assembly takes this request's address.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation54 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 55 requested as the boss is told to ascend to the arena.
///
/// Blends for 10 whole frames and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation55 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 56 requested after the second faded scene cut.
///
/// Resets without blending and disables grid participation. Event playback
/// copies this borrowed request and resolves the equipped weapon bank in its copy.
static AnimationPlayRequest _gActor361100PlayerSceneAnimation56 = {
    .source               = { .index = ACTOR_361100_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_361100_PLAYER_SCENE_CLIP_56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player's opening pod-scene position and facing.
///
/// XYZ uses the model root parent's game units; Euler angles use 4096 units
/// per turn. The placement message consumes both vectors synchronously.
static ActorTransform _gActor361100PlayerSceneOpeningPlacement = { { 11540, -4010, 3190, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } };

/// Player's pod-scene run destination toward Eve.
///
/// The run message consumes only XYZ in the root parent's game units, copying
/// them into the player's work. Rotation and fourth vector words are unused;
/// the complete authored transform remains stored here.
static ActorTransform _gActor361100PlayerSceneRunDestination = { { 8310, -4010, 6490, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } };

/// Player's final pod-scene position and facing, shared by normal and skip paths.
///
/// XYZ uses the model root parent's game units; Euler angles use 4096 units
/// per turn. The placement message consumes both vectors synchronously.
static ActorTransform _gActor361100PlayerSceneExitPlacement = { { 7760, -4010, 7010, 0 }, { 0, -1480, 0, 0 } };

/// Eve's scripted body placement at the start of the player's approach.
///
/// The scene routes this borrowed transform to placed actor 2. XYZ uses root-
/// parent game units; all three Euler angles are zero. Dispatch consumes it
/// synchronously, before later messages show and animate the body.
static ActorTransform _gActor361100EveSceneOpeningPlacement = { { 3836, -8010, 10771, 0 }, { 0, 0, 0, 0 } };

/// Player clips for the pod-scene approach: 54 while running, 48 on arrival.
///
/// Both indices address the installed equipped-bank extension. The run message
/// consumes this borrowed record synchronously and stores its IDs as halfwords.
static GameActorMoveAnim _gActor361100PlayerSceneRunAnimations = {
    .approachAnimId = ACTOR_361100_PLAYER_SCENE_CLIP_54,
    .arrivalAnimId  = ACTOR_361100_PLAYER_SCENE_CLIP_48,
};

/// Tells the placed boss to ascend to its arena during the pod scene.
///
/// The receiver reads only the command word during synchronous dispatch; the
/// authored context tags identify the pod-bottom area but are ignored here.
static ActorCommand _gActor361100BossAscendCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_403600_COMMAND_ASCEND_TO_ARENA,
};

/// Starts the placed boss fight after normal playback or scene skip.
///
/// The receiver reads only the command word during synchronous dispatch; the
/// authored context tags identify the pod-bottom area but are ignored here.
static ActorCommand _gActor361100BossStartFightCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_403600_COMMAND_START_FIGHT,
};

/// Hides and parks the placed boss before pod-scene playback, disabling lock-on.
///
/// Context identifies the pod-bottom area; the boss reads only the command word
/// during synchronous dispatch and retains no pointer to this record.
static ActorCommand _gActor361100BossHideCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_BOSS_HIDE,
};

/// Unreferenced zero-filled storage; its animation-request type and role are unproven.
AnimationPlayRequest D_actor_361100_80165DDC = { 0 };

/// Resets tentacle bank-0 clip 1 before the first translation leg.
///
/// Dispatch consumes this request synchronously; the rig borrows the clip data
/// during playback. The tentacle ignores the world-collision choice.
static AnimationPlayRequest _gActor361100TentacleSceneAnimation1 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_TENTACLE_SCENE_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets tentacle bank-0 clip 2 after placement for the second translation leg.
///
/// Dispatch consumes this request synchronously; the rig borrows the clip data
/// during playback. The tentacle ignores the world-collision choice.
static AnimationPlayRequest _gActor361100TentacleSceneAnimation2 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_TENTACLE_SCENE_CLIP_2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends tentacle bank-0 clip 3 over four whole frames before the third leg.
///
/// Dispatch consumes this request synchronously; the ticking rig blends from
/// its current pose and borrows the clip data. World-collision choice is ignored.
static AnimationPlayRequest _gActor361100TentacleSceneAnimation3 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_TENTACLE_SCENE_CLIP_3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Tentacle root placement before its first translation leg.
///
/// XYZ uses root-parent game units and yaw is three eighths of a turn.
/// Synchronous placement applies all Euler angles and clears velocity/carry.
static ActorTransform _gActor361100TentacleFirstLegPlacement = {
    .pos = { 8200, 9270, 4060, 0 },
    .rot = { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Retained nominal first-leg destination, unused by the pod-scene scripts.
///
/// XYZ uses root-parent game units; yaw is three eighths of a turn. This is the
/// first-leg placement plus its authored displacement, not the actual endpoint:
/// the retained movement code applies one extra velocity step.
static ActorTransform _gActor361100TentacleFirstLegDestination = {
    .pos = { 7970, 1300, 4350, 0 },
    .rot = { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Tentacle root placement before its second translation leg.
///
/// XYZ uses root-parent game units; Euler angles use 4096 units per turn.
/// Synchronous placement applies Rz * Ry * Rx and clears velocity/carry.
static ActorTransform _gActor361100TentacleSecondLegPlacement = {
    .pos = { 4460, 470, 8650, 0 },
    .rot = { ACTOR_TRANSFORM_ANGLE_TURN / 4, -341, 0, 0 },
};

/// Retained tentacle stop command, unused by the pod-scene scripts.
///
/// Selector 0 clears velocity while preserving fractional carry and countdown.
/// Pod-bottom context tags are stored but ignored by the tentacle receiver.
static ActorCommand _gActor361100TentacleStopCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_TENTACLE_STOP,
};

/// Starts the tentacle's first leg with displacement (-230,-7970,290) over 25 ticks.
///
/// The receiver retains fractional carry and applies 26 truncated 16.16 velocity
/// steps, including the tick finding zero. Dispatch reads only the command word.
static ActorCommand _gActor361100TentacleFirstLegCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_TENTACLE_FIRST_LEG,
};

/// Starts the tentacle's second leg with displacement (-1350,-4430,740) over 26 ticks.
///
/// The receiver retains fractional carry and applies 27 truncated 16.16 velocity
/// steps, including the tick finding zero. Dispatch reads only the command word.
static ActorCommand _gActor361100TentacleSecondLegCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_TENTACLE_SECOND_LEG,
};

/// Starts the tentacle's third leg with displacement (1350,6340,-4140) over 14 ticks.
///
/// The receiver retains fractional carry and applies 15 truncated 16.16 velocity
/// steps, including the tick finding zero. Dispatch reads only the command word.
static ActorCommand _gActor361100TentacleThirdLegCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_TENTACLE_THIRD_LEG,
};

/// Exits the tentacle task at the end of normal playback or scene skip.
///
/// The receiver reads only the command word during synchronous dispatch; the
/// authored context tags identify the pod-bottom area but are ignored here.
static ActorCommand _gActor361100TentacleExitCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_COMMAND_EXIT,
};

/// Unreferenced zero-filled storage; its animation-request type and role are unproven.
AnimationPlayRequest D_actor_361100_80165E88 = { 0 };

/// Resets Eve-body bank-0 clip 1 after the first cut and again before her rise.
///
/// Each synchronous request restarts the clip, even when it is already playing.
/// The rig borrows clip data during playback; world-collision choice is ignored.
static AnimationPlayRequest _gActor361100EveSceneAnimation1 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_EVE_SCENE_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Eve-body bank-0 clip 2 when the figure is first shown.
///
/// Dispatch consumes this request synchronously; the rig borrows the clip data
/// during playback. The body ignores the world-collision choice.
static AnimationPlayRequest _gActor361100EveSceneAnimation2 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_EVE_SCENE_CLIP_2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Eve-body bank-0 clip 3 after the reveal's forty-frame wait.
///
/// Dispatch consumes this request synchronously; the rig borrows the clip data
/// during playback. The body ignores the world-collision choice.
static AnimationPlayRequest _gActor361100EveSceneAnimation3 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_EVE_SCENE_CLIP_3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Eve-body bank-0 clip 4 on the next CAP cue before the tentacle is shown.
///
/// Dispatch consumes this request synchronously; the rig borrows the clip data
/// during playback. The body ignores the world-collision choice.
static AnimationPlayRequest _gActor361100EveSceneAnimation4 = {
    .source               = { .index = ACTOR_361100_SCENE_MODEL_BANK },
    .animationId          = ACTOR_361100_EVE_SCENE_CLIP_4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Eve-body root placement when the figure is first shown in the pod scene.
///
/// XYZ uses root-parent game units with zero Euler rotation. Synchronous
/// placement does not stop any existing scripted body translation.
static ActorTransform _gActor361100EveRevealPlacement = {
    .pos = { 4440, -12880, 6960, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Eve-body root placement at the first faded scene cut.
///
/// XYZ uses root-parent game units with zero Euler rotation. Synchronous
/// placement does not stop any existing scripted body translation.
static ActorTransform _gActor361100EveFirstCutPlacement = {
    .pos = { 6870, 1830, 5310, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Eve-body root placement immediately before the figure's rise command.
///
/// XYZ uses root-parent game units and Euler angles use 4096 units per turn.
/// Dispatch applies all Euler angles and retains existing scripted translation.
static ActorTransform _gActor361100EveRisePlacement = {
    .pos = { 7370, 2550, 6228, 0 },
    .rot = { 371, 885, -450, 0 },
};

/// Retained Eve-body stop command, unused by the pod-scene scripts.
///
/// Selector 0 clears velocity/countdown while preserving fractional carry.
/// Pod-bottom context tags are stored but ignored by the body receiver.
static ActorCommand _gActor361100EveStopCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_EVE_STOP,
};

/// Starts Eve's body moving 450 root-parent game units along +Y over 160 ticks.
///
/// The receiver applies 160 truncated 16.16 velocity steps, retaining carry
/// from earlier movement. Synchronous dispatch reads only the command word.
static ActorCommand _gActor361100EveRiseCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_SCENE_EVE_RISE,
};

/// Exits Eve's scripted body task at the end of normal playback or scene skip.
///
/// The receiver reads only the command word during synchronous dispatch; the
/// authored context tags identify the pod-bottom area but are ignored here.
static ActorCommand _gActor361100EveExitCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM } },
    .command = ACTOR_361100_COMMAND_EXIT,
};

/// Scene/audio stream key selected for the pod-bottom entry presentation.
///
/// Group 6 selects the current folder's stream table, ID 11 and first qualifier
/// 21, with second qualifier zero. The interpreter retains this key through
/// playback; the selected resources and this overlay must remain loaded.
static EvsSceneKey _gActor361100PodSceneKey = {
    .group    = 6,
    .streamId = 11,
    .subId    = 21,
};

EvsCommand gActor361100PodSceneScript[96] = {
    // Select playback, install player clips and park the boss for the presentation.
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor361100PodSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor361100CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneOpeningAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100PlayerSceneOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_BOSS_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100BossHideCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_361100_SCENE_VIEW_REFRACTION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Yield once before starting the staged scene playback.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100SpawnHeadAimTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_361100_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gActor361100PlayerSceneRunDestination } }, { .message = { .moveAnimation = &_gActor361100PlayerSceneRunAnimations } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100EveSceneOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100EveRevealPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100EveSceneAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_KILL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100EveSceneAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100EveSceneAnimation4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Play the tentacle's three translation legs against CAP cues.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100TentacleFirstLegPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100TentacleSceneAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100TentacleFirstLegCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gActor361100TentacleFirstLegVibrationCommands }, { .vibrationSegments = _gActor361100TentacleFirstLegVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100TentacleSecondLegPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100TentacleSceneAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100TentacleSecondLegCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100TentacleSceneAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100TentacleThirdLegCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade across a scene cut while restoring the next actor presentation.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100PlayerSceneExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_361100_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100EveFirstCutPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100EveSceneAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100EveRisePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor361100EveSceneAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100EveRiseCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SpawnShakeTask }, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade across a scene cut while restoring the next actor presentation.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSceneAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_BOSS_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100BossAscendCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore ordinary presentation and hand control to the boss fight.
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_361100_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_361100_SCENE_VIEW_FIGHT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_BOSS_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100BossStartFightCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100EveExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100TentacleExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor361100PodSceneSkipScript[26] = {
    // Restore the fight placement under a fade before releasing scene resources.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_361100_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_361100_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_BOSS_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100BossStartFightCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor361100PlayerSceneExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor361100PlayerSkipAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_KILL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = ACTOR_361100_SCENE_SKIP_VIEW_LOAD }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_361100_SCENE_VIEW_FIGHT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_EVE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100EveExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_361100_SCENE_TENTACLE_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor361100TentacleExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Two-lane controller vibration started with the tentacle's first translation leg.
///
/// The binary motor waits eleven script frames then holds for eight. The
/// variable motor plays the fifteen-frame rise and fall segments in order.
/// Both lanes reach the third STOP entry without jumps; PLAY indices are 0..2.
/// The spawned task borrows commands and segments until teardown.
static PadScriptCmd _gActor361100TentacleFirstLegVibrationCommands[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 11), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_361100_VIBRATION_RAMP_UP) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_361100_VIBRATION_BINARY_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_361100_VIBRATION_RAMP_DOWN) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Three timed segments for the tentacle's first-leg controller vibration.
///
/// Entries 0 and 1 ramp byte intensity 3 toward 255 and 255 toward 107 over
/// fifteen script frames each; the ramp stops short of the target. Entry 2
/// supplies an eight-frame binary hold, whose zero intensities are ignored.
/// The fourth byte's role is unproven and the interpreter never reads it.
/// This table stays borrowed by the vibration task until teardown.
static PadScriptVibrationSegment _gActor361100TentacleFirstLegVibrationSegments[3] = {
    { .startIntensity = 3, .endIntensity = ACTOR_361100_VIBRATION_FULL_INTENSITY, .durationFrames = 15, .field_3 = 1 },
    { .startIntensity = ACTOR_361100_VIBRATION_FULL_INTENSITY, .endIntensity = 107, .durationFrames = 15, .field_3 = 1 },
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 8, .field_3 = 0 },
};

/// Single variable-motor pulse requested on every positive countdown tick of scene shake.
///
/// The binary lane stops immediately; the variable lane plays segment 0 for
/// one script frame and then stops at command 1. No command or segment index
/// can escape the two-entry stream and its one segment. Each spawned task
/// borrows this pair until teardown, including after the shake task ends.
static PadScriptCmd _gActor361100ShakeVibrationCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Full-intensity one-script-frame variable-motor pulse used by scene shake.
///
/// Both byte intensities are 255; duration 1 posts one eligible motor refresh.
/// The fourth byte's role is unproven and the interpreter never reads it.
/// Each spawned vibration task borrows this record until teardown.
static PadScriptVibrationSegment _gActor361100ShakeVibrationSegment = {
    .startIntensity = ACTOR_361100_VIBRATION_FULL_INTENSITY,
    .endIntensity   = ACTOR_361100_VIBRATION_FULL_INTENSITY,
    .durationFrames = 1,
    .field_3        = 1,
};

#include "assets/enemies/brahman/tentacle.inc"

#include "assets/rooms/shelter/b2_pod_bottom/tentacle_scene_clips_2.inc"

/// Bank 0 of the tentacle: slot 0 is NULL, playable clips are 1..3.
///
/// The nineteen-part motion handler drives slots 1..18 and borrows the table,
/// descriptors and pose data through playback. Requests must select bank 0
/// and a non-NULL clip; no range checks occur in the handler.
#include "assets/rooms/shelter/b2_pod_bottom/tentacle_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..3.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor361100TentacleAnimationBank,
};

TaskDesc gActor361100TentacleTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_361100_TASK_PRIORITY } }, _actor361100TentacleTask, { .model = &_gBrahmanTentacle } };

/// Sentinel-terminated message handlers of the initialized tentacle task.
///
/// Play borrows an `AnimationPlayRequest` (bank 0, clip 1..3); place borrows an
/// `ActorTransform` and stops translation. Draw takes mode 0..3; command borrows
/// an `ActorCommand` (0 stop, 1..3 move, other values exit). Payloads are consumed
/// synchronously; the task borrows this table until teardown.
static TaskMessageEntry _gActor361100TentacleMessageTable[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, _actor361100PlaceTentacle },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor361100SetTentacleDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor361100ApplyTentacleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/characters/eve/body.inc"

#include "assets/rooms/shelter/b2_pod_bottom/tentacle_scene_clips_3.inc"

/// Bank 0 of Eve's scripted body: slot 0 is NULL, playable clips are 1..4.
///
/// Playback drives model slots 1..18. The table, descriptors and pose data remain
/// borrowed through playback; the handler requires a non-NULL clip in this bank.
#include "assets/rooms/shelter/b2_pod_bottom/eve_bank.inc"

/// Single animation bank of Eve's scripted body, selected by `request.source.index` 0.
///
/// The play handler indexes this table without a bounds check and stores the
/// bank selector in a signed byte. The bound rig borrows bank-0 data through playback.
static AnimationSet** _gActor361100EveAnimationBanks[] = {
    _gActor361100EveAnimationBank,
};

TaskDesc gActor361100EveTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_361100_TASK_PRIORITY } }, _actor361100EveTask, { .model = &_gEveBody } };

/// Sentinel-terminated message handlers of Eve's initialized scripted body.
///
/// Play borrows an `AnimationPlayRequest` (bank 0, clip 1..4), restarting repeats;
/// place borrows an `ActorTransform` without cancelling scripted translation.
/// Draw takes mode 0..3; command borrows an `ActorCommand` (0 stop, 1 rise, other
/// values exit). Payloads are consumed synchronously; the task borrows this table
/// until teardown.
static TaskMessageEntry _gActor361100EveMessageTable[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor361100PlayEveAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor361100SetEveDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor361100ApplyEveCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Draws the pod-bottom scene's water while actor data streams into slot 2.
///
/// Requires this overlay and actor_403600 loaded, a live coordinate-body task,
/// initialized graphics/scratch state and a 0x18000-byte stream window. Waits for
/// a written-byte count, then allocates a task-owned shallow ripple and warms it
/// for 30 ticks at (7330,1810,6300) game units under the view coordinate.
/// View 11 draws refracted strips; view 12 draws the ripple using the current
/// display buffer's half of the aligned free tail; view 10 releases the task.
/// The ripple draw needs 192 complete 40-byte packet records per half (at least
/// 15360 free bytes), aligned packet storage and no streaming overwrite until GPU
/// consumption. This branch has no capacity check. Written count must be in
/// 0..0x18000; allocation must succeed because the retained failure path exits
/// and then continues using the allocation. Other view IDs leave the task live.
static void _actor361100StreamWaterTask(Task* task)
{
    enum { ACTOR_361100_STREAM_WINDOW_BYTES          = 0x18000,
           ACTOR_361100_STREAM_NOT_STARTED           = -1,
           ACTOR_361100_STREAM_PACKET_ALIGNMENT_MASK = 7,
           ACTOR_361100_STREAM_RIPPLE_WARMUP_TICKS   = 30,
           ACTOR_361100_STREAM_VIEW_EXIT             = 10,
           ACTOR_361100_STREAM_VIEW_REFRACTION       = 11,
           ACTOR_361100_STREAM_VIEW_RIPPLE           = 12,
           ACTOR_361100_STREAM_DISC_X                = 7330,
           ACTOR_361100_STREAM_DISC_Y                = 1810,
           ACTOR_361100_STREAM_DISC_Z                = 6300 };
    Actor403600Ripple* ripple;
    GfxCoord*          discCoord;
    s32                warmupTick;
    u8*                streamWrite;
    u32                freeBytes;
    u8*                viewIdPtr;
    u8                 viewId;

    ripple    = task->work;
    viewIdPtr = &gGameSession->location.view;
    discCoord = task->extra.coordBody->coord;
    if (gFsActorBufferWrittenBytes[2] != ACTOR_361100_STREAM_NOT_STARTED) {
        freeBytes   = ACTOR_361100_STREAM_WINDOW_BYTES - gFsActorBufferWrittenBytes[2];
        freeBytes  &= ~ACTOR_361100_STREAM_PACKET_ALIGNMENT_MASK;
        streamWrite = gFsActorBuffer2Base + gFsActorBufferWrittenBytes[2];
        // Seed a shallow disc before either water presentation can draw.
        if (task->state == 0) {
            ripple = memCalloc(sizeof(Actor403600Ripple), false);
            if (ripple == NULL) {
                taskCallExit(task);
            }
            task->work       = ripple;
            ripple->shallow  = 1;
            ripple->emitting = 1;
            warmupTick       = 0;
            do {
                actor403600TickRipple(ripple);
                warmupTick += 1;
            } while (warmupTick < ACTOR_361100_STREAM_RIPPLE_WARMUP_TICKS);
            discCoord->parent = &gGfxViewCoord;
            gfxSetRotIdentity(&discCoord->coord);
            discCoord->coord.t[0]   = ACTOR_361100_STREAM_DISC_X;
            discCoord->coord.t[1]   = ACTOR_361100_STREAM_DISC_Y;
            discCoord->coord.t[2]   = ACTOR_361100_STREAM_DISC_Z;
            discCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state            += 1;
        }
        viewId = *viewIdPtr;
        if (viewId == ACTOR_361100_STREAM_VIEW_REFRACTION) {
            _actor361100DrawStreamRefraction(task);
            return;
        } else if (viewId == ACTOR_361100_STREAM_VIEW_RIPPLE) {
            // Borrow the current buffer's half of the free streaming tail.
            gActor403600EffectPrimitiveCursor = streamWrite + (gDisplayState.otBuffer * ((s32)(freeBytes + (freeBytes >> 0x1F)) >> 1));
            actor403600TickRipple(ripple);
            actor403600DrawRipple(ripple, discCoord);
            return;
        } else if (viewId == ACTOR_361100_STREAM_VIEW_EXIT) {
            taskCallExit(task);
        }
    }
}

/// Prepares screen rays and the depth dividend for the scene's horizontal plane.
///
/// Requires a caller-reserved `WaterRefractionScratch`, current composed view
/// matrix and live display projection. Transposes the view's Q12 basis and
/// rotates its translation after signed-halfword narrowing/saturation. Sets
/// depth to (rotated Y + 1810 world units) * projection distance. Screen rays
/// use X = 0 and Z = screen distance in pixels; the caller supplies each Y row.
/// Projection distance must fit a signed halfword; the depth product must fit
/// a signed word.
/// Leaves rotated-row storage and screen-row Y intact. Retains no pointer and
/// changes GTE rotation/arithmetic state; the caller owns scratch lifetime.
static inline void _actor361100PrepareRefractionProjection(WaterRefractionScratch* scratch, const DisplayState* display)
{
    enum { ACTOR_361100_REFRACTION_PLANE_Y = 1810 };

    TransposeMatrix(&gGfxViewCoord.workm, &scratch->transposedView);
    scratch->viewTranslation.vx = gGfxViewCoord.workm.t[0];
    scratch->viewTranslation.vy = gGfxViewCoord.workm.t[1];
    scratch->viewTranslation.vz = gGfxViewCoord.workm.t[2];
    _gfxRotateSv(&scratch->transposedView, &scratch->viewTranslation);
    scratch->depth        = scratch->viewTranslation.vy + ACTOR_361100_REFRACTION_PLANE_Y;
    scratch->depth       *= display->screenDistance;
    scratch->screenRow.vx = 0;
    scratch->screenRow.vz = display->screenDistance;
    gte_SetRotMatrix(&scratch->transposedView);
}

/// Redraws the scene's lower screen rows as vertically refracted framebuffer strips.
///
/// Called for view 11 during the chunked actor load. Requires a writable 0x18000-
/// byte streaming window at `gFsActorBuffer2Base`, a written-byte count in 0..0x18000,
/// and scratch-stack space. The count is rounded up to an eight-byte boundary;
/// only an aligned free tail of at least 0x6680 bytes draws. Each display buffer
/// uses half that tail, with 160 rows split into 320 POLY_FT4 packets (12800 bytes).
/// Packets and the sampled framebuffer must survive GPU consumption and further
/// streaming must not overwrite the selected half while it is still in use.
///
/// Screen positions are pixels centred on (160,120); the horizontal plane offset
/// is 1810 world units. `killCountdown` is a wrapping 16-bit wave phase (4096 units
/// per turn), advanced by 32 while actor control runs. Rotated rays supply depth
/// buckets masked to 0..1023. The clipping configuration is fixed to full width.
/// Borrows task state and streaming storage; restores the scratch cursor and
/// overwrites GTE rotation/arithmetic state.
static void _actor361100DrawStreamRefraction(Task* task)
{
    enum {
        ACTOR_361100_REFRACTION_STREAM_BYTES       = 0x18000,
        ACTOR_361100_REFRACTION_MIN_FREE_BYTES     = 0x6680,
        ACTOR_361100_REFRACTION_TOP_ROW            = 80,
        ACTOR_361100_REFRACTION_BOTTOM_ROW         = 240,
        ACTOR_361100_REFRACTION_HALF_WIDTH         = 160,
        ACTOR_361100_REFRACTION_HALF_HEIGHT        = 120,
        ACTOR_361100_REFRACTION_PHASE_STEP         = 32,
        ACTOR_361100_REFRACTION_SINE_ROW_STEP      = 31,
        ACTOR_361100_REFRACTION_COSINE_ROW_STEP    = 197,
        ACTOR_361100_REFRACTION_COSINE_OFFSET      = 308,
        ACTOR_361100_REFRACTION_WAVE_BIAS          = 2 * ONE,
        ACTOR_361100_REFRACTION_WAVE_PIXEL_SHIFT   = 9,
        ACTOR_361100_REFRACTION_DEPTH_PHASE_START  = 768,
        ACTOR_361100_REFRACTION_FADE_ROWS          = 8,
        ACTOR_361100_REFRACTION_TRIG_SHIFT         = 12,
        ACTOR_361100_REFRACTION_QUAD_WORDS         = 9,
        ACTOR_361100_REFRACTION_RAW_QUAD_CODE      = 0x2D,
        ACTOR_361100_REFRACTION_MAX_DEPTH          = 0x3FFF,
        ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS  = 32,
        ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS   = 96,
        ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW   = 239,
        ACTOR_361100_REFRACTION_TEXTURE_REFLECTION = 476,
    };
    DisplayState*           display;
    WaterRefractionScratch* scratch;
    POLY_FT4*               quad;
    s32                     freeBytes;
    s32                     negativeWrittenBytes;
    u8*                     packetBytes;
    s32                     displayBuffer;
    s32                     clipMode;
    s32                     splitX;
    s32                     sinePhase;
    s32                     cosinePhase;
    s32                     screenY;
    s32                     centeredY;
    s32                     spanLeft;
    s32                     spanRight;
    s32                     spanCount;
    s32                     splitY;
    s32                     bucketOffset;
    s32                     fullWave;
    s32                     waveScale;
    s32                     modeLeftEdge; // Uninitialized only in dormant clip modes; fixed mode 0 never reads it
    s32                     waveDisplacement;
    s32                     fadedDisplacement;
    s32                     fadeStartY;
    s32                     fadeShift;
    s32                     fadeDistance;
    s32                     rowDepth;
    s32                     bucket;
    s32                     spanIndex;
    s32                     splitFadeStartY;
    s32                     fadeRows;
    s32                     baseLeft;
    s32                     baseRight;
    s32                     splitLeft;
    s32                     splitRight;
    s32                     rightPageLeft;
    s32                     leftPageRight;
    s32                     textureY;
    s32                     secondSpanWidth;
    s32                     sineSample;
    s32                     cosineSample;
    u16                     unusedFrameSlot; // Its shifted read emits no load; retained for the matched spill layout

    freeBytes            = ACTOR_361100_REFRACTION_STREAM_BYTES - gFsActorBufferWrittenBytes[2];
    freeBytes           &= -8;
    negativeWrittenBytes = freeBytes - ACTOR_361100_REFRACTION_STREAM_BYTES;
    packetBytes          = gFsActorBuffer2Base - negativeWrittenBytes;
    display              = &gDisplayState;
    displayBuffer        = display->otBuffer;
    clipMode             = 0;
    splitX               = clipMode;
    splitY               = 0;
    waveScale            = ONE;
    fullWave             = ONE;
    fadeStartY           = ACTOR_361100_REFRACTION_TOP_ROW;
    fadeShift            = 1;
    bucketOffset         = 0;
    splitFadeStartY      = 0;
    fadeRows             = ACTOR_361100_REFRACTION_FADE_ROWS;
    baseLeft             = -ACTOR_361100_REFRACTION_HALF_WIDTH;
    baseRight            = ACTOR_361100_REFRACTION_HALF_WIDTH;
    splitLeft            = -ACTOR_361100_REFRACTION_HALF_WIDTH;
    splitRight           = ACTOR_361100_REFRACTION_HALF_WIDTH;
    // The remaining streamed-load tail supplies two independent packet halves.
    if ((u32)freeBytes >= (u32)ACTOR_361100_REFRACTION_MIN_FREE_BYTES) {
        if (displayBuffer != 0) {
            packetBytes += freeBytes >> 1;
        }
        quad = (POLY_FT4*)packetBytes;
        quad--;
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            task->killCountdown = (u16)task->killCountdown + ACTOR_361100_REFRACTION_PHASE_STEP;
        }
        sinePhase   = task->killCountdown * 2;
        cosinePhase = task->killCountdown;
        scratch     = SCRATCH_STACK_RESERVE_BLOCK(WaterRefractionScratch);
        _actor361100PrepareRefractionProjection(scratch, display);
        // Rotate each screen ray, then displace the sampled framebuffer row.
        screenY = ACTOR_361100_REFRACTION_TOP_ROW;
        do {
            centeredY             = screenY - ACTOR_361100_REFRACTION_HALF_HEIGHT;
            scratch->screenRow.vy = centeredY;
            gte_ldv0(&scratch->screenRow);
            gte_rtv0();
            spanLeft  = baseLeft;
            spanRight = baseRight;
            spanCount = 1;
            if (splitY > 0) {
                if (screenY < ACTOR_361100_REFRACTION_FADE_ROWS) {
                    spanRight = spanLeft + splitX;
                    if (splitX <= 0) {
                        spanRight = baseRight;
                        spanLeft  = spanRight + splitX;
                    }
                    if (splitY < screenY) {
                        spanCount = 2;
                    }
                }
            } else {
                if ((splitY < 0) && (-splitY < screenY)) {
                    spanLeft = splitLeft;
                    if (splitX > 0) {
                        spanRight = spanLeft + splitX;
                    } else {
                        spanRight = splitRight;
                        spanLeft  = spanRight + splitX;
                    }
                }
            }
            // Combine Q12 samples, then fade the pixel displacement over the first eight rows.
            sineSample          = rsin(sinePhase);
            cosineSample        = rcos(cosinePhase + ACTOR_361100_REFRACTION_COSINE_OFFSET);
            sineSample         += ACTOR_361100_REFRACTION_WAVE_BIAS;
            fadedDisplacement   = cosineSample + sineSample;
            fadedDisplacement >>= ACTOR_361100_REFRACTION_WAVE_PIXEL_SHIFT;
            // Retain the allocator-visible one-pass wave latches required by this image.
            if (fullWave == 0) {
                fadedDisplacement = (fadedDisplacement * waveScale) >> ACTOR_361100_REFRACTION_TRIG_SHIFT;
                do {
                    waveDisplacement  = fadedDisplacement;
                    fadedDisplacement = waveDisplacement + 1;
                } while (0);
            } else {
                do {
                    waveDisplacement  = fadedDisplacement;
                    fadedDisplacement = waveDisplacement + 1;
                } while (0);
            }
            if (fadeStartY != 1) {
                fadeDistance = screenY - fadeStartY;
                if (fadeDistance < fadeRows) {
                    fadedDisplacement  = waveDisplacement >> ((fadeRows - fadeDistance) >> fadeShift);
                    fadedDisplacement += fadeShift;
                }
            }
            gte_stsv(&scratch->rotatedRow);
            if (scratch->rotatedRow.vy > 0) {
                bucket   = scratch->depth / scratch->rotatedRow.vy;
                bucket >>= 2;
            } else {
                bucket = ACTOR_361100_REFRACTION_MAX_DEPTH;
            }
            textureY = centeredY + ACTOR_361100_REFRACTION_HALF_HEIGHT + fadedDisplacement;
            rowDepth = bucket;
            bucket   = ((rowDepth << gDisplayState.otDepthShift) & ACTOR_361100_REFRACTION_MAX_DEPTH) >> 4;
            bucket  += bucketOffset;
            if (textureY >= ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW) {
                textureY = ACTOR_361100_REFRACTION_TEXTURE_REFLECTION - textureY;
            }
            if (clipMode == 1) {
                modeLeftEdge = -ACTOR_361100_REFRACTION_HALF_WIDTH;
                if (screenY < 0x7D) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    spanLeft = modeLeftEdge;
                    if (screenY < 0xB3) {
                        spanCount = 2;
                    }
                    spanRight = -0x59;
                }
            } else if (clipMode == 2) {
                if (screenY < 0x83) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    if (screenY < 0xB7) {
                        spanCount = 2;
                        spanLeft  = 0x57;
                    } else {
                        spanLeft = 0x57;
                    }
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                }
            } else if (clipMode == 3) {
                spanCount = 1;
                if (screenY < 0x43) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    spanCount = 2;
                }
            }
            spanIndex = 0;
            if (spanCount != 0) {
                do {
                    if (clipMode == 1) {
                        if (spanIndex != 0) {
                            spanLeft  = 0x3C;
                            spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                        }
                    } else if (clipMode == 2) {
                        if (spanIndex == 1) {
                            spanLeft  = modeLeftEdge;
                            spanRight = -0x69;
                        }
                    } else if (clipMode == 3) {
                        if (spanIndex == 0) {
                            if (screenY < 0x43) {
                                spanLeft  = modeLeftEdge;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            } else {
                                spanLeft  = modeLeftEdge;
                                spanRight = -0x57;
                            }
                        } else {
                            if (screenY < 0xC1) {
                                spanLeft  = 0x5D;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            } else {
                                spanLeft  = 0x2A;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            }
                        }
                    } else if (spanIndex == 1) {
                        if (screenY - splitFadeStartY < fadeRows) {
                            fadedDisplacement = waveDisplacement >> ((fadeRows - (screenY - splitFadeStartY)) >> fadeShift);
                            textureY          = centeredY + 0x79 + fadedDisplacement;
                            if (textureY >= ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW) {
                                textureY = ACTOR_361100_REFRACTION_TEXTURE_REFLECTION - textureY;
                            }
                        }
                        secondSpanWidth = splitX - 2 * ACTOR_361100_REFRACTION_HALF_WIDTH;
                        if (splitX <= 0) {
                            secondSpanWidth = splitX + 2 * ACTOR_361100_REFRACTION_HALF_WIDTH;
                        }
                        spanLeft = baseLeft;
                        if (secondSpanWidth > 0) {
                            spanRight = secondSpanWidth + spanLeft;
                        } else {
                            spanRight = baseRight;
                            spanLeft  = secondSpanWidth + spanRight;
                        }
                    }
                    // Split at X=0 because each texture page covers one screen half.
                    if (spanRight > 0) {
                        quad++;
                        quad->y1      = centeredY;
                        quad->y0      = centeredY;
                        quad->y3      = centeredY + 1;
                        quad->y2      = centeredY + 1;
                        quad->tpage   = getTPage(2, 0, 0x80, displayBuffer << 8);
                        rightPageLeft = spanLeft;
                        if (spanLeft < 0) {
                            rightPageLeft = 0;
                        }
                        quad->x2 = rightPageLeft;
                        quad->x0 = rightPageLeft;
                        quad->u2 = rightPageLeft + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->u0 = rightPageLeft + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->x3 = spanRight;
                        quad->x1 = spanRight;
                        quad->u3 = spanRight + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->u1 = spanRight + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->v1 = textureY + (displayBuffer << 4);
                        quad->v0 = textureY + (displayBuffer << 4);
                        quad->v3 = textureY + (displayBuffer << 4) + 1;
                        quad->v2 = textureY + (displayBuffer << 4) + 1;
                        setlen(quad, ACTOR_361100_REFRACTION_QUAD_WORDS);
                        setcode(quad, ACTOR_361100_REFRACTION_RAW_QUAD_CODE);
                        addPrim(&gGpuCurrentOt[bucket], quad);
                    }
                    if (spanLeft <= 0) {
                        quad++;
                        quad->y1      = centeredY;
                        quad->y0      = centeredY;
                        quad->y2      = (quad->y3 = centeredY + 1);
                        quad->tpage   = getTPage(2, 0, 0, displayBuffer << 8);
                        leftPageRight = spanRight;
                        // Split at X=0 because each texture page covers one screen half.
                        if (spanRight > 0) {
                            leftPageRight = 0;
                        }
                        quad->u2 = spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS;
                        quad->u0 = spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS;
                        quad->u3 = (spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS) + (leftPageRight - spanLeft);
                        quad->u1 = (spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS) + (leftPageRight - spanLeft);
                        quad->x2 = spanLeft;
                        quad->x0 = spanLeft;
                        quad->x3 = leftPageRight;
                        quad->x1 = leftPageRight;
                        quad->v1 = textureY + (displayBuffer << 4);
                        quad->v0 = textureY + (displayBuffer << 4);
                        quad->v3 = textureY + (displayBuffer << 4) + 1;
                        quad->v2 = textureY + (displayBuffer << 4) + 1;
                        setlen(quad, ACTOR_361100_REFRACTION_QUAD_WORDS);
                        setcode(quad, ACTOR_361100_REFRACTION_RAW_QUAD_CODE);
                        addPrim(&gGpuCurrentOt[bucket], quad);
                    }
                    spanIndex += 1;
                } while (spanIndex < spanCount);
            }
            sinePhase += ACTOR_361100_REFRACTION_SINE_ROW_STEP + (unusedFrameSlot >> 16);
            if (rowDepth >= ACTOR_361100_REFRACTION_DEPTH_PHASE_START + 1) {
                cosinePhase += ACTOR_361100_REFRACTION_COSINE_ROW_STEP + (rowDepth - ACTOR_361100_REFRACTION_DEPTH_PHASE_START) / 4;
            } else {
                cosinePhase += ACTOR_361100_REFRACTION_COSINE_ROW_STEP;
            }
            screenY += 1;
        } while (screenY < ACTOR_361100_REFRACTION_BOTTOM_ROW);
        SCRATCH_STACK_RELEASE_BLOCK(WaterRefractionScratch);
    }
}

/// Ramps the retained head-aim rate by ONE/16, with halfword truncation before clamping.
///
/// `aim` is live writable state; nonzero `tracking` ramps toward ONE, zero
/// toward 0. The Q12 increment is 256 per dispatch. Each halfword result is
/// stored before the signed endpoint check, including halfword wrap.
static inline void _actor361100RampHeadAimRate(AnimationHeadAim* aim, s32 tracking)
{
    enum { ACTOR_361100_HEAD_AIM_RATE_STEP = ONE / 16 };
    s16 nextRate;

    if (tracking != 0) {
        nextRate  = aim->rate + ACTOR_361100_HEAD_AIM_RATE_STEP;
        aim->rate = nextRate;
        if (nextRate > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextRate  = aim->rate - ACTOR_361100_HEAD_AIM_RATE_STEP;
        aim->rate = nextRate;
        if (nextRate < 0) {
            aim->rate = 0;
        }
    }
}

/// Turns the player's head toward placed scene actor 2 with a ramped Q12 rate.
///
/// Both tasks must own live TMD models with parts 0..4 in root-to-head order.
/// The script freeze gate suspends all state changes. State 0 allocates zeroed
/// AnimationHeadAim work, setting yaw/pitch limits to 768/512 units (4096 per
/// turn), then updates immediately. State 1 changes rate by ONE/16 each tick:
/// nonzero spawnArg1 ramps to ONE, zero ramps to 0. The rate is narrowed to a
/// halfword before a signed clamp. Missing tasks, failed allocation and other
/// states kill the task and clear its handle. taskKill releases owned work.
static void _actor361100HeadAimTask(Task* task)
{
    enum {
        ACTOR_361100_HEAD_AIM_INIT        = 0,
        ACTOR_361100_HEAD_AIM_UPDATE      = 1,
        ACTOR_361100_HEAD_AIM_TARGET      = 2,
        ACTOR_361100_HEAD_AIM_YAW_LIMIT   = 768,
        ACTOR_361100_HEAD_AIM_PITCH_LIMIT = 512,
    };
    Task*             subject;
    Task*             target;
    AnimationHeadAim* aim;

    subject = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    target  = sceneFindPlacedActor(ACTOR_361100_HEAD_AIM_TARGET);
    if (gEvsPlaybackPaused == 0) {
        if ((subject == NULL) || (target == NULL)) {
            task->state = ACTOR_361100_HEAD_AIM_KILL;
        }
        switch (task->state) {
            case ACTOR_361100_HEAD_AIM_INIT:
                aim = memCalloc(sizeof(AnimationHeadAim), false);
                if (aim != NULL) {
                    task->work      = aim;
                    aim->yawLimit   = ACTOR_361100_HEAD_AIM_YAW_LIMIT;
                    aim->pitchLimit = ACTOR_361100_HEAD_AIM_PITCH_LIMIT;
                    task->state++;
                        /* fallthrough */
                    case ACTOR_361100_HEAD_AIM_UPDATE:
                        aim = task->work;
                        _actor361100RampHeadAimRate(aim, task->spawnArg1.value);
                        animationAimHeadAt(subject, target, aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(task);
                _gActor361100HeadAimTask = NULL;
                break;
        }
    }
}

/// Stages the selected scene's deferred audio start for this package's script.
///
/// Requires a selected scene and prepared buffers through CD consumption. A
/// missing selection leaves the previous deferred request intact.
static void _actor361100StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues playback of the selected scene for this package's event script.
///
/// Requires prepared playback storage and resident CD queue capacity. The scene
/// descriptor and buffers must remain live until playback completes.
static void _actor361100StartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Ends the selected scene stream and restores its saved random state.
///
/// Requires a successful scene selection. Buffer and CD-request teardown remain
/// with their owners; the script cancels the deferred CD request separately.
static void _actor361100FinishScene(void)
{
    streamFinishScene();
}

/// Cancels this scene's deferred CD request and finishes its streaming session.
///
/// Used by both normal and skip scripts after successful scene selection. The
/// deferred replacement is discarded and scene flags/random state are restored
/// immediately; CD cancellation completes through later resident dispatches.
/// Buffer and task teardown remain with their owners.
static void _actor361100CancelScene(void)
{
    cdCmdCancelScene();
}

/// Spawns and retains this scene's initially relaxed player-head aim task.
///
/// The package and gameplay must remain loaded while it runs. Call once after
/// clearing the handle; an existing task is neither checked nor killed. Failed
/// spawning stores NULL. Task teardown owns any subsequently allocated work.
static void _actor361100SpawnHeadAimTask(void)
{
    _gActor361100HeadAimTask = taskSpawnFromTable(_gActor361100SceneTasks, ACTOR_361100_HEAD_AIM_TASK_INDEX, ACTOR_361100_HEAD_AIM_RELAX, 0);
}

/// Sets the retained head-aim task's ramp policy, or kills it for any other value.
///
/// Mode 0 relaxes, 1 tracks, and every other signed word kills and clears the
/// handle; the scripts use -1 for teardown. A NULL handle does nothing. The
/// change is applied on the task's next unfrozen update, with work owned by it.
static void _actor361100SetHeadAimMode(s32 mode)
{
    if (_gActor361100HeadAimTask != NULL) {
        if (mode < ACTOR_361100_HEAD_AIM_TRACK + 1) {
            if (mode >= ACTOR_361100_HEAD_AIM_RELAX) {
                _gActor361100HeadAimTask->spawnArg1.value = mode;
                return;
            }
        }
        taskKill(_gActor361100HeadAimTask);
        _gActor361100HeadAimTask = NULL;
    }
}

/// Spawns the scene's one-pixel vertical shake and controller-vibration task.
///
/// durationTicks is the signed update countdown (the script uses 120); the child
/// decrements before shaking, giving durationTicks-1 active updates for a positive
/// value, and stops when the event is skipped. Spawning failure is ignored. The
/// descriptor and pad/vibration script storage must survive the child task.
static void _actor361100SpawnShakeTask(s32 durationTicks)
{
    taskSpawnFromTable(_gActor361100SceneTasks, ACTOR_361100_SHAKE_TASK_INDEX, durationTicks, 0);
}

/// Alternates the scene's vertical display offset and emits controller vibration.
///
/// spawnArg1.value is a signed update countdown, greater than INT_MIN. It is
/// decremented first: a positive remainder produces one vibration request and
/// an offset of zero or minus one pixel. Expiry or event skip then restores zero
/// and kills the task; a skipped positive tick still emits its vibration first.
/// The pad script and its descriptor must remain loaded through each request.
static void _actor361100ShakeTask(Task* task)
{
    enum { ACTOR_361100_SHAKE_Y_PIXELS = -1 };
    s32 countdown;

    countdown             = task->spawnArg1.value - 1;
    task->spawnArg1.value = countdown;
    if (countdown > 0) {
        displaySetShakeY((countdown & 1) ? 0 : ACTOR_361100_SHAKE_Y_PIXELS);
        padScriptSpawn(_gActor361100ShakeVibrationCommands, &_gActor361100ShakeVibrationSegment);
    }
    // Check skip after producing this tick's shake and vibration.
    if ((task->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
        displaySetShakeY(0);
        taskKill(task);
    }
}

/// Adds music-loading and weapon-restoration flags requested by the event script.
///
/// bits is a raw signed callback word; OR assignment retains only its low byte.
/// The scripts set GAME_SESSION_FLOW_SKIP_ENDING_MUSIC and
/// GAME_SESSION_FLOW_SKIP_AREA_MUSIC independently, preserving earlier flags.
static void _actor361100AddFlowFlags(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void actor361100ClearHeadAimTaskHandle(s32 unused)
{
    _gActor361100HeadAimTask = NULL;
}

/// Applies one signed 16.16 root translation step and retains fractional carry.
///
/// rootCoord and work are side-effect-free pointers to live, separate writable
/// storage. Either model's work provides three Fixed16 carries and a VECTOR velocity
/// in its root parent's frame; velocity's fourth word is unused. Arguments are
/// evaluated repeatedly. Adds the signed high half to translation, preserves the
/// unsigned low half for the next update and invalidates composition, including
/// on zero velocity. No caller locals are captured.
/// Statement-only GNU expression; its value is not part of the interface.
#define ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work)      \
    ({                                                                \
        (work)->carry[0].word    += (work)->velocity.vx;              \
        (work)->carry[1].word    += (work)->velocity.vy;              \
        (work)->carry[2].word    += (work)->velocity.vz;              \
        (rootCoord)->coord.t[0]  += (work)->carry[0].halves.integer;  \
        (rootCoord)->coord.t[1]  += (work)->carry[1].halves.integer;  \
        (rootCoord)->coord.t[2]  += (work)->carry[2].halves.integer;  \
        (rootCoord)->composeStamp = GRAPHICS_COORD_DIRTY;             \
        (work)->carry[0].word     = (work)->carry[0].halves.fraction; \
        (work)->carry[1].word     = (work)->carry[1].halves.fraction; \
        (work)->carry[2].word     = (work)->carry[2].halves.fraction; \
    })

/// Advances the tentacle's scripted translation, animation and model lighting.
///
/// Requires the initialized, live model/work and its borrowed Enemy. Translation
/// adds signed 16.16 velocity to the root's parent-relative position, retaining
/// fractional carry. An uninterrupted move armed with N runs for N+1 updating
/// ticks: the tick that finds zero still moves before stopping. Hidden models
/// still move and animate; only lighting is skipped. A pending buffer release
/// runs after movement and drawing preparation, on the tick finding zero.
static void _actor361100TickTentacle(Task* task)
{
    TmdObject*                model = task->extra.tmd;
    _Actor361100TentacleWork* work  = task->work;
    GfxCoord*                 rootCoord;
    VECTOR                    worldPosition;
    s32                       slotIndex;

    rootCoord = model->coords;
    ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work);
    if (work->moveFrames >= 0) {
        if (work->moveFrames == 0) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        worldPosition.vx = rootCoord->workm.t[0];
        worldPosition.vy = rootCoord->workm.t[1];
        worldPosition.vz = rootCoord->workm.t[2];
        worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Dispatches the tentacle's lifecycle while scene actors are running.
///
/// Requires a live TMD task with a borrowed Enemy in spawnArg2 and state 0
/// (initialize), 1 (update) or 2 (exit). Other states are outside the table.
/// Pausing scene actors also pauses initialization and both countdowns.
static void _actor361100TentacleTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor361100TentacleStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Allocates the tentacle's owned work and enables its update/message handlers.
///
/// Requires the model's live root coordinate and a borrowed Enemy in spawnArg2.
/// Publishes its local matrix on the Enemy, clears its contact table, and binds
/// model lighting to the work's matrices. Allocation failure exits the enemy
/// task. Success advances state and retains the work until task teardown.
static void _actor361100InitTentacle(Task* task)
{
    _Actor361100TentacleWork* work;
    GfxCoord*                 rootCoord;
    Enemy*                    enemy;

    rootCoord = task->extra.tmd->coords;
    enemy     = task->spawnArg2.pointer;

    work = memCalloc(sizeof(_Actor361100TentacleWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->model.animId  = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown = ACTOR_361100_BUFFER_FREE_IDLE;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;

    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    _actor361100BindTentacleLighting(task);

    task->msgTable     = _gActor361100TentacleMessageTable;
    task->exitCallback = _actor361100ExitTentacle;
    task->state       += 1;
}

/// Releases the tentacle's Enemy and starts task/model teardown.
///
/// The live task must borrow an Enemy in spawnArg2. Default teardown owns the
/// work allocation; its lighting matrices remain borrowed by the model.
static void _actor361100ExitTentacle(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the tentacle model's lighting matrices to its owned work.
///
/// Requires initialized work and a live model. The model borrows both matrices
/// until task teardown; later lighting updates populate them.
static void _actor361100BindTentacleLighting(Task* task)
{
    TmdObject*                model;
    _Actor361100TentacleWork* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

#include "../../shared/actor/actor_motion_play19.inc.c"

/// Places the tentacle's root and stops its translation without resetting its timer.
///
/// Requires initialized work, a live root and a readable, word-aligned placement
/// through dispatch. XYZ uses whole units in the root parent's frame; Euler
/// angles use 4096 units per turn. Builds Rz * Ry * Rx and records all three
/// angles. Clears velocity and fractional carry but leaves moveFrames alone.
/// The payload is not retained; msgId and unusedArg are ignored. Returns 0.
static s32 _actor361100PlaceTentacle(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*                 rootCoord;
    _Actor361100TentacleWork* work;

    work                    = task->work;
    rootCoord               = task->extra.tmd->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->carry[0].word     = 0;
    work->carry[1].word     = 0;
    work->carry[2].word     = 0;
    work->velocity.vx       = 0;
    work->velocity.vy       = 0;
    work->velocity.vz       = 0;
    return 0;
}

/// Changes tentacle visibility and primitive-buffer ownership policy.
///
/// Requires a live model and initialized work. Mode 0 hides and enables automatic
/// buffer recovery; 1 shows, requests a buffer and enables recovery; 2 hides,
/// disables recovery and schedules release on the third subsequent updating
/// tick; 3 shows while keeping recovery disabled. Showing does not cancel an
/// earlier release. msgId and unusedArg are ignored. Returns 0 for modes 0..3,
/// or 1 without changing anything for other values.
static s32 _actor361100SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    _Actor361100TentacleWork* work;
    TmdObject*                model;
    s32                       result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_HIDE_RELEASE:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Stops the tentacle, starts one of its three translation legs, or exits it.
///
/// Requires initialized work and a readable ActorCommand through dispatch. Only
/// its command word is read; context, msgId and unusedArg are ignored. Command 0
/// zeros velocity but keeps fractional carry and the countdown. Commands 1..3
/// set signed 16.16 velocity and arm 25, 26 or 14 ticks respectively, retaining
/// carry. Uninterrupted movement applies 26, 27 or 15 steps, including the tick
/// that finds zero. Every other command calls the task's exit handler. The
/// payload is not retained. Returns 0.
static s32 _actor361100ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_361100_TENTACLE_COMMAND_STOP        = 0,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_FIRST  = 1,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_SECOND = 2,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_THIRD  = 3,
        ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES    = 25,
        ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES   = 26,
        ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES    = 14,
    };
    _Actor361100TentacleWork* work;

    work = task->work;
    // Commands replace velocity/countdown but preserve the fractional carry.
    switch (command->command) {
        case ACTOR_361100_TENTACLE_COMMAND_STOP:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_FIRST:
            work->velocity.vx = -230 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->velocity.vy = -7970 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->velocity.vz = 290 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_SECOND:
            work->velocity.vx = -1350 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->velocity.vy = -4430 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->velocity.vz = 740 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_THIRD:
            work->velocity.vx = 1350 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->velocity.vy = 6340 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->velocity.vz = -4140 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// Advances Eve's scripted body translation, animation, ground shadow and lighting.
///
/// Requires initialized work, the live nineteen-part model and room effect
/// state. Signed 16.16 velocity moves the root in its parent's frame, retaining
/// fractional carry. An uninterrupted move armed with N runs for N updating
/// ticks, stopping after the tick that finds one. Hidden models still move and
/// animate. Visible models project the second part's cached world position for
/// a shadow before recomposing that part and updating lighting. Pending buffer
/// release runs last, on the tick finding zero.
static void _actor361100TickEve(Task* task)
{
    // Ground-shadow half-side, in whole coordinate units before view rotation.
    enum { ACTOR_361100_EVE_SHADOW_HALF_SIZE = 512 };
    TmdObject*           model = task->extra.tmd;
    _Actor361100EveWork* work  = task->work;
    GfxCoord*            rootCoord;
    VECTOR3              groundPosition;
    s32                  slotIndex;

    // Integrate whole root displacement and retain only the fractional carry.
    rootCoord = model->coords;
    ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work);
    if (work->moveFrames > 0) {
        if (work->moveFrames == 1) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        // Sample the cached second-part position before recomposing it.
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_361100_EVE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

#undef ACTOR_361100_INTEGRATE_ROOT_TRANSLATION

/// Dispatches Eve's body lifecycle while scene actors are running.
///
/// Requires a live TMD task with a borrowed Enemy in spawnArg2 and state 0
/// (initialize), 1 (update) or 2 (exit). Other states are outside the table.
/// Pausing scene actors also pauses initialization and both countdowns.
static void _actor361100EveTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor361100EveStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Allocates Eve's owned body work and enables its update/message handlers.
///
/// Requires a live model and a borrowed Enemy in spawnArg2 for teardown.
/// Marks the rig unbound so its first play request installs bank 0, and binds
/// the model's lighting to the work's matrices. Allocation failure exits the
/// enemy task. Success advances state and retains work until task teardown.
static void _actor361100InitEve(Task* task)
{
    _Actor361100EveWork* work;

    work = memCalloc(sizeof(_Actor361100EveWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->model.animId  = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown = ACTOR_361100_BUFFER_FREE_IDLE;
    _actor361100BindEveLighting(task);
    task->msgTable     = _gActor361100EveMessageTable;
    task->exitCallback = _actor361100ExitEve;
    task->state       += 1;
}

/// Releases Eve's body Enemy and starts task/model teardown.
///
/// The live task must borrow an Enemy in spawnArg2. Default teardown owns the
/// work allocation; its lighting matrices remain borrowed by the model.
static void _actor361100ExitEve(Task* task)
{
    enemyTaskExit(task);
}

/// Binds Eve's body model lighting matrices to its owned work.
///
/// Requires initialized work and a live model. The model borrows both matrices
/// until task teardown; later lighting updates populate them.
static void _actor361100BindEveLighting(Task* task)
{
    TmdObject*           model;
    _Actor361100EveWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Starts or blends the requested clip on Eve's bound non-root animation slots.
///
/// work must own a bound nineteen-slot rig with loaded set and track data for
/// request.animationId (1..4). All rig, pose-buffer, model and clip storage must
/// remain live during playback. request is borrowed only through this call.
/// Its clip id narrows to the model's signed byte; blendFrames counts whole
/// frames (0..2047). Nonzero blend captures a ticking pose before seeking the
/// new clip; otherwise each driven slot resets. Ticks slots 1..18 immediately
/// and enables subsequent playback. Slot 0 retains separately scripted motion.
static inline void _actor361100StartEveClip(_Actor361100EveWork* work, const AnimationPlayRequest* request)
{
    s32 slotIndex;

    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
}

/// Applies an animation request to Eve's body, restarting even the current clip.
///
/// Requires initialized work and a readable request through synchronous dispatch.
/// source.index must be 0 and animationId 1..4; the package's bank data must stay
/// loaded during playback. Stored bank/clip ids narrow to signed bytes. Drives
/// slots 1..18, leaving root motion to the scripted translation. Nonzero blend
/// interpolates an already ticking rig for blendFrames whole frames (0..2047);
/// otherwise slots reset. Ticks the new clip once immediately, then enables
/// future ticks. Collision choice, msgId and unusedArg are ignored. Returns 0.
static s32 _actor361100PlayEveAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor361100EveWork* work;
    TmdObject*           model;

    work  = task->work;
    model = task->extra.tmd;
    // A bank switch rebinds the rig; every request restarts or blends its clip.
    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor361100EveAnimationBanks[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    _actor361100StartEveClip(work, request);
    return 0;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Changes Eve's body visibility and primitive-buffer ownership policy.
///
/// Requires a live model and initialized work. Mode 0 hides and enables automatic
/// buffer recovery; 1 shows, requests a buffer and enables recovery; 2 hides,
/// disables recovery and schedules release on the third subsequent updating
/// tick; 3 shows while keeping recovery disabled. Showing does not cancel an
/// earlier release. msgId and unusedArg are ignored. Returns 0 for modes 0..3,
/// or 1 without changing anything for other values.
static s32 _actor361100SetEveDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    _Actor361100EveWork* work;
    TmdObject*           model;
    s32                  result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_HIDE_RELEASE:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Stops Eve's body, starts its upward translation, or exits it.
///
/// Requires initialized work and a readable ActorCommand through dispatch. Only
/// its command word is read; context, msgId and unusedArg are ignored. Command 0
/// zeros velocity and moveFrames while retaining fractional carry. Command 1
/// adds 450/160 units along the root parent's +Y on each of 160 updating ticks,
/// with velocity truncated to signed 16.16. Every other command calls the
/// task's exit handler. The payload is not retained. Returns 0.
static s32 _actor361100ApplyEveCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_361100_EVE_COMMAND_STOP = 0,
        ACTOR_361100_EVE_COMMAND_RISE = 1,
        ACTOR_361100_EVE_RISE_FRAMES  = 160,
    };
    _Actor361100EveWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_361100_EVE_COMMAND_STOP:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            work->moveFrames  = 0;
            break;
        case ACTOR_361100_EVE_COMMAND_RISE:
            work->velocity.vy = 450 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_EVE_RISE_FRAMES;
            work->velocity.vx = 0;
            work->velocity.vz = 0;
            work->moveFrames  = ACTOR_361100_EVE_RISE_FRAMES;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}
