#include "actors/actor_535700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/footstep_walk.h"
#include "../../shared/walk/pair_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);
static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

static s32 _gActor535700BlackoutFrames;

static FootstepWalkWork* _gFootstepWalkWork;

/// Borrowed task pointer of this carrier's singleton sound walker.
///
/// Spawn publishes it for synchronous animation and model-draw messages.
/// The pointer confers no ownership; teardown leaves it unchanged, so those
/// messages require the original task and model to remain live.
static Task* _gFootstepWalkTask;

static s16 _gFootstepWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gEveBody;
static void      _actor535700FootstepWalkerTask(Task* walkerTask);

static s32 _actor535700SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument);
static s32 _actor535700ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);

static TmdSource _gGolemPawnBody;
static TmdSource _gGolemBeamSword;
static s32       _actor535700IgnorePairWalkerCommand(Task* unusedTask, s32 messageId, s32 unusedArgument, s32 unusedSecondArgument);
static void      _actor535700PairWalkerTask(Task* task);

static void _actor535700SetBlackoutFrames(s32 frames);
static void _actor535700FinishScene(void);

static void _actor535700BlackoutTask(Task* task);

/// Scheduler order and whole-frame pose transition used by this scene's records.
enum {
    ACTOR_535700_SCENE_TASK_PRIORITY = 192,
    ACTOR_535700_SCENE_BLEND_FRAMES  = 8
};

/// Native scene clip keys without an established action name.
enum {
    ACTOR_535700_PLAYER_WEAPON_CLIP_1 = 1,
    ACTOR_535700_EVE_CLIP_1           = 1,
    ACTOR_535700_EVE_CLIP_5           = 5,
    ACTOR_535700_EVE_CLIP_6           = 6,
    ACTOR_535700_EVE_CLIP_7           = 7,
    ACTOR_535700_EVE_CLIP_8           = 8,
    ACTOR_535700_EVE_CLIP_10          = 10
};

/// The scene Eve command that schedules turning while her turn clip plays.
enum { ACTOR_535700_SCENE_EVE_COMMAND_TURN = 0 };

/// Singleton spawn recipe for the scene's bodyless opaque-black cover task.
///
/// Spawn at index zero with priority 192. The callback emits a full-screen
/// black tile; all live copies consume the same package-local signed countdown.
static TaskDesc _gActor535700BlackoutTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, ACTOR_535700_SCENE_TASK_PRIORITY } },
    .callback = _actor535700BlackoutTask,
    .data     = { .value = 0 }
};

/// Restarts the real player's equipped-weapon clip 1 while scene Eve is shown.
///
/// The event interpreter copies this request and replaces its bank selector.
/// Playback resets without blending and removes world-grid participation.
static AnimationPlayRequest _gActor535700PlayerWeaponClip1Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_535700_PLAYER_WEAPON_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Schedules twenty heading updates for scene Eve while her turn clip plays.
///
/// The stage/area tags identify Dryfield night room 8; this receiver ignores
/// those tags. Dispatch consumes the command synchronously without retaining it.
static ActorCommand _gActor535700EveTurnCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_R08 } },
    .command = ACTOR_535700_SCENE_EVE_COMMAND_TURN
};

enum { ACTOR_535700_EVE_CLIP_0 = 0 };

/// Unplayed bank-0 clip-0 reset preset at the start of the scene-Eve request run.
///
/// The receiver is inferred from neighboring requests; Eve's bank entry 0 is
/// NULL. No dispatch establishes a use for this retained preset.
static AnimationPlayRequest _gActor535700UnusedEveClip0Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends scene Eve back to her native startup clip 1.
///
/// The transition takes eight normal-rate frames. The scene walker ignores
/// the source selector and collision word and consumes the request synchronously.
static AnimationPlayRequest _gActor535700EveClip1BlendRequest = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_535700_SCENE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's walk clip 2 for the backward approaches.
///
/// Travel mode is selected separately by the walk-target message.
/// The scene walker ignores the source selector and collision word.
static AnimationPlayRequest _gActor535700EveWalk2Request = {
    .source               = { .index = 0 },
    .animationId          = FOOTSTEP_WALK_ANIM_WALK_2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Blends scene Eve into the clip that permits her scripted turn.
///
/// The eight-frame transition selects the clip; the separate turn command
/// schedules the heading updates. Source and collision words are ignored.
static AnimationPlayRequest _gActor535700EveTurnBlendRequest = {
    .source               = { .index = 0 },
    .animationId          = FOOTSTEP_WALK_ANIM_TURN,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_535700_SCENE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

enum { ACTOR_535700_EVE_CLIP_4 = 4 };

/// Unplayed bank-0 clip-4 reset preset within the scene-Eve request run.
///
/// The receiver is inferred from neighboring requests. No dispatch establishes
/// the clip's action; source and collision words retain their stored zeros.
static AnimationPlayRequest _gActor535700UnusedEveClip4Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts scene Eve's native clip 5 after her final offset placement.
///
/// Consumed synchronously by the scene walker; source and collision words
/// are ignored. No visual action is established for this clip.
static AnimationPlayRequest _gActor535700EveClip5Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Blends scene Eve into native clip 6 before the closing fade.
///
/// The transition takes eight normal-rate frames; source and collision
/// words are ignored. No visual action is established for this clip.
static AnimationPlayRequest _gActor535700EveClip6BlendRequest = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_535700_SCENE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's native clip 7 at the final centre placement.
///
/// Consumed synchronously by the scene walker; source and collision words
/// are ignored. No visual action is established for this clip.
static AnimationPlayRequest _gActor535700EveClip7Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's native clip 8 after the scene's sixty-frame wait.
///
/// Consumed synchronously by the scene walker; source and collision words
/// are ignored. No visual action is established for this clip.
static AnimationPlayRequest _gActor535700EveClip8Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's native clip 10 at two caption cues during her approach.
///
/// Consumed synchronously by the scene walker; source and collision words
/// are ignored. No visual action is established for this clip.
static AnimationPlayRequest _gActor535700EveClip10Request = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_535700_EVE_CLIP_10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's travel-end idle clip at the approach caption cues.
///
/// The scene walker consumes the request synchronously and ignores
/// the source selector and collision word.
static AnimationPlayRequest _gActor535700EveIdleRequest = {
    .source               = { .index = 0 },
    .animationId          = FOOTSTEP_WALK_ANIM_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's walk clip 14 for her slow forward approach.
///
/// The following walk-target message independently selects slow-forward
/// travel. The source selector and collision word are ignored.
static AnimationPlayRequest _gActor535700EveWalk14Request = {
    .source               = { .index = 0 },
    .animationId          = FOOTSTEP_WALK_ANIM_WALK_14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts scene Eve's walk clip 15 for her forward approach.
///
/// The following walk-target message independently selects forward travel.
/// The source selector and collision word are ignored.
static AnimationPlayRequest _gActor535700EveWalk15Request = {
    .source               = { .index = 0 },
    .animationId          = FOOTSTEP_WALK_ANIM_WALK_15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Blends the first scene Pawn GOLEM into its travel clip.
///
/// The transition takes eight normal-rate frames; a separate walk target
/// supplies travel. The source selector and collision word are ignored.
static AnimationPlayRequest _gActor535700PawnGolemWalkBlendRequest = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_WALK,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_535700_SCENE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts the first scene Pawn GOLEM's idle clip at its entrance placement.
///
/// The pair walker consumes the request synchronously and ignores
/// the source selector and collision word.
static AnimationPlayRequest _gActor535700PawnGolemIdleRequest = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

enum {
    ACTOR_535700_UNUSED_REQUEST_CLIP_1 = 1,
    ACTOR_535700_UNUSED_REQUEST_CLIP_2 = 2,
    ACTOR_535700_UNUSED_REQUEST_CLIP_3 = 3,
};

/// Ten unplayed bank-0 presets retained between the Pawn GOLEM request records.
///
/// Entry 0 resets clip 2, entry 1 blends to clip 3 over eight frames, and entries
/// 2..9 reset clip 1. All disable world collision. No consumer establishes their
/// receivers, indexing or original grouping; keep the stored ten-record span.
static AnimationPlayRequest _gActor535700UnusedAnimationRequests[10] = {
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_3, ANIMATION_BLEND_INTERPOLATE, ACTOR_535700_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, ACTOR_535700_UNUSED_REQUEST_CLIP_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts the travel clip shared by all three scene Pawn GOLEMs.
///
/// Each receiver uses its own rig and separately supplied walk target.
/// The source selector and collision word are ignored.
static AnimationPlayRequest _gActor535700PawnGolemWalkRequest = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_WALK,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Offstage placement of the real player while the scene uses its own Eve model.
///
/// XYZ is in the player root parent frame; all Euler angles are zero.
/// The player placement message consumes the complete transform synchronously.
static ActorTransform _gActor535700PlayerOffstagePlacement = {
    .pos = { 30000, 0, 30000, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Opening placement of scene Eve before her first forward approach.
///
/// XYZ uses the model root parent frame, and yaw is zero.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700EveEntrancePlacement = {
    .pos = { 50, 0, -14280, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Slow-forward destination and subsequent snapped placement of scene Eve.
///
/// The walk message borrows the position member and ignores Y; the later
/// placement applies XYZ and zero yaw. Both use the model root parent frame.
static ActorTransform _gActor535700EveApproachTransform = {
    .pos = { 50, 0, -11460, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Scene Eve's approach placement with her heading turned for the next caption cue.
///
/// XYZ uses the model root parent frame; yaw is in 1/4096 turns.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700EveTurnedApproachPlacement = {
    .pos = { 50, 0, -11460, 0 },
    .rot = { 0, -398, 0, 0 }
};

/// Scene Eve's half-turn placement before her backward walk.
///
/// XYZ uses the model root parent frame; yaw is in 1/4096 turns.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700EveBackwardWalkPlacement = {
    .pos = { 0, 0, -11720, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 }
};

/// Destination shared by scene Eve's two backward walks.
///
/// The walk message borrows the position member in the model root parent
/// frame. Only X/Z is read; Y and the stored rotation are unused.
static ActorTransform _gActor535700EveBackwardWalkTarget = {
    .pos = { 0, 0, -10000, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Scene Eve's forward-walk destination after her scheduled turn.
///
/// The walk message borrows the position member in the model root parent
/// frame. Only X/Z is read; Y and the stored rotation are unused.
static ActorTransform _gActor535700EveForwardWalkTarget = {
    .pos = { -1300, 0, -10000, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Scene Eve's centre placement before native clips 7 and 8.
///
/// XYZ uses the model root parent frame; yaw is a negative half-turn.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700EveFinalCenterPlacement = {
    .pos = { 0, 0, -10000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 }
};

/// Scene Eve's offset placement before native clip 5 and the closing cues.
///
/// XYZ uses the model root parent frame; yaw is in 1/4096 turns.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700EveFinalOffsetPlacement = {
    .pos = { -300, 0, -10300, 0 },
    .rot = { 0, -1991, 0, 0 }
};

/// Opening placement of the first scene Pawn GOLEM.
///
/// XYZ uses the model root parent frame, and yaw is zero.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700PawnGolemEntrancePlacement = {
    .pos = { 0, 0, -18000, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// First walk destination of the first scene Pawn GOLEM.
///
/// X/Z uses the model root parent frame. The pair-walker target handler
/// ignores Y and rotation and retains no pointer to the record.
static ActorTransform _gActor535700PawnGolemEntranceWalkTarget = {
    .pos = { 0, 0, -15300, 0 },
    .rot = { 0, 0, 0, 0 }
};

/// Placement of the first scene Pawn GOLEM for its walk from the left.
///
/// XYZ uses the model root parent frame; yaw is a negative quarter-turn.
/// The placement handler consumes XYZ and yaw without retaining the record.
static ActorTransform _gActor535700PawnGolemLeftPlacement = {
    .pos = { -3670, 0, -10000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Destination of the first scene Pawn GOLEM's walk from the left.
///
/// X/Z uses the model root parent frame. The pair-walker target handler
/// ignores Y and rotation and retains no pointer to the record.
static ActorTransform _gActor535700PawnGolemLeftWalkTarget = {
    .pos = { -400, 0, -10000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Placement of the second scene Pawn GOLEM for its repeated walk from the right.
///
/// XYZ uses the root parent frame; the placement handler consumes only XYZ and yaw.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemRightPlacement = {
    .pos = { 3670, 0, -10000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Destination of the second scene Pawn GOLEM's repeated walk from the right.
///
/// X/Z uses the root parent frame. The target handler ignores Y and rotation.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemRightWalkTarget = {
    .pos = { 400, 0, -10000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Placement of the first scene Pawn GOLEM for its final walk down the centre.
///
/// XYZ uses the root parent frame; the placement handler consumes only XYZ and yaw.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemCenterPlacement = {
    .pos = { 0, 0, -500, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 2, 0, 0 }
};

/// Destination of the first scene Pawn GOLEM's final walk down the centre.
///
/// X/Z uses the root parent frame. The target handler ignores Y and rotation.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemCenterWalkTarget = {
    .pos = { 0, 0, -4500, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 2, 0, 0 }
};

/// Two unreferenced room-coordinate transforms beside the scene's Pawn GOLEM placements.
///
/// Positions lie at Y=0, Z=-2000 with X=3500 and X=0; both face a negative
/// quarter-turn in the 4096-unit angle scale. Receiver, placement-versus-walk
/// use and original grouping are unproven; retain both complete transforms.
static ActorTransform _gActor535700UnusedPlacementTransforms[2] = {
    { .pos = { 3500, 0, -2000, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { .pos = { 0, 0, -2000, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
};

/// Placement of the third scene Pawn GOLEM for its walk from the rear.
///
/// XYZ uses the root parent frame; the placement handler consumes only XYZ and yaw.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemRearPlacement = {
    .pos = { 0, 0, -14500, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Destination of the third scene Pawn GOLEM's walk from the rear.
///
/// X/Z uses the root parent frame. The target handler ignores Y and rotation.
/// The scene message consumes the borrowed transform synchronously.
static ActorTransform _gActor535700PawnGolemRearWalkTarget = {
    .pos = { 0, 0, -13090, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Scene/audio stream key for the Eve and three Pawn GOLEM sequence.
///
/// Group 3, stream 57, qualifier 11 select the loaded scene stream; this is
/// a stream key rather than a stage/area location. Playback borrows the record.
static EvsSceneKey _gActor535700SceneKey = { .group = 3, .streamId = 57, .subId = 11 };

enum {
    ACTOR_535700_ENCOUNTER_CAP_COMMAND      = 2,
    ACTOR_535700_ENCOUNTER_BLACKOUT_UPDATES = 300,
};

EvsCommand gActor535700PawnGolemEncounterScript[99] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor535700SceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_535700_ENCOUNTER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PlayerOffstagePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor535700PlayerWeaponClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveEntrancePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveWalk14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .pointer = &_gActor535700EveApproachTransform.pos } }, { .value = FOOTSTEP_WALK_MODE_SLOW_FORWARD } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveApproachTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveTurnedApproachPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip1BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemEntrancePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkBlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemEntranceWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveBackwardWalkPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveWalk2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .pointer = &_gActor535700EveBackwardWalkTarget.pos } }, { .value = FOOTSTEP_WALK_MODE_BACKWARD } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveTurnBlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor535700EveTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveWalk15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .pointer = &_gActor535700EveForwardWalkTarget.pos } }, { .value = FOOTSTEP_WALK_MODE_FORWARD } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveWalk2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .pointer = &_gActor535700EveBackwardWalkTarget.pos } }, { .value = FOOTSTEP_WALK_MODE_BACKWARD } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemLeftPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemLeftWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemRightPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemRightWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemRightPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemRightWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemCenterPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemCenterWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700PawnGolemRearPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700PawnGolemWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor535700PawnGolemRearWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveFinalCenterPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor535700EveFinalOffsetPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor535700EveClip6BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = ACTOR_535700_ENCOUNTER_BLACKOUT_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor535700FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor535700PawnGolemEncounterSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor535700SetBlackoutFrames }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor535700FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/eve/body.inc"

#include "assets/rooms/dryfield/night_r08/eve_and_golems_clips_1.inc"

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

/// Synchronous messages of this carrier's sound-enabled scene walker.
///
/// Playback and drawing use the published singleton; placement and approach
/// use the receiver. The command handler enables steps or schedules turning.
/// The final sentinel makes unknown IDs return zero. The task borrows this
/// table and its callbacks for its entire lifetime.
static TaskMessageEntry _gFootstepWalkMessageTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor535700SetWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor535700ApplyWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor535700EveWalkerTaskDesc = {
    .header   = { .fields = { TASK_BODY_TMD, ACTOR_535700_SCENE_TASK_PRIORITY } },
    .callback = _actor535700FootstepWalkerTask,
    .data     = { .model = &_gEveBody }
};

/// Native clip-index table borrowed by the sound walker's animation context.
///
/// Loaded keys are 1..8, 10 and 13..19; every other entry is NULL. The
/// play handler's exclusive bound of 35 does not validate loaded entries.
/// Each selected set must provide tracks 1..18, and both this table and its
/// clip data must remain loaded throughout playback.
#include "assets/rooms/dryfield/night_r08/eve_bank.inc"

#include "assets/enemies/golem/pawn/body.inc"

#include "assets/enemies/golem/beam_sword.inc"

#include "assets/rooms/dryfield/night_r08/eve_and_golems_clips_2.inc"

/// Message handlers borrowed by this carrier's pair-walker tasks.
///
/// Supports animation, paired draw flags, placement and XZ walk targets;
/// actor commands are ignored. The terminal entry makes other IDs return 0.
/// Keep this table and its handlers loaded while the walker can receive messages.
static TaskMessageEntry _gPairWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor535700IgnorePairWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _pairWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor535700PawnGolemTasks[PAIR_WALK_TASK_COUNT] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), PAIR_WALK_TASK_PRIORITY } }, _actor535700PairWalkerTask, { .model = &_gGolemPawnBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), PAIR_WALK_TASK_PRIORITY } }, _pairWalkSubModelTask, { .model = &_gGolemBeamSword } },
};

/// Six-entry clip bank borrowed by this carrier's pair-walker animation context.
///
/// Entry 0 is unplayable; entries 1..5 are loaded clips. Entry 1 is the
/// default pose and entry 4 drives travel. The table and clip data must remain
/// loaded for the walker's lifetime; the table holds pointers, not packed bytes.
#include "assets/rooms/dryfield/night_r08/golem_bank.inc"

/// Signed cover updates remaining, shared by every live scene blackout task.
///
/// Positive N draws for N updates with one task; zero stops drawing and the
/// terminating update decrements it to -1. Every nonzero setter call spawns
/// another task sharing this word. Negative values are not clamped.
static s32 _gActor535700BlackoutFrames;

/// Borrowed pointer to the sound walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkWork* _gFootstepWalkWork;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Queues a centered 320-by-256-pixel opaque black cover for the scene.
///
/// Borrows `sizeof(TILE)` writable, word-aligned bytes at `gGpuPrimCursor` and
/// advances the cursor by that extent without checking capacity. The rectangle
/// starts at (-160, -128) relative to the draw origin and links at tag index 10
/// of `gGpuCurrentOt`, which must be live and contain that entry. The packet
/// and table must survive GPU drawing; no task storage or countdown is changed.
static inline void _actor535700DrawBlackoutTile(void)
{
    enum {
        ACTOR_535700_BLACKOUT_WIDTH_PIXELS  = 320,
        ACTOR_535700_BLACKOUT_HEIGHT_PIXELS = 256,
        ACTOR_535700_BLACKOUT_OT_TAG        = 10
    };

    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -ACTOR_535700_BLACKOUT_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_535700_BLACKOUT_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_535700_BLACKOUT_WIDTH_PIXELS;
    tile->h  = ACTOR_535700_BLACKOUT_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + ACTOR_535700_BLACKOUT_OT_TAG, tile);
}

/// Covers the scene in opaque black while its shared countdown is nonzero.
///
/// Each live-task update with a nonzero count queues one 320-by-256-pixel tile.
/// The first zero-count update kills the task without drawing. Every update
/// decrements the shared count, including the terminating update, which leaves -1.
/// The package must remain loaded, and drawing requires the frame resources
/// described by `_actor535700DrawBlackoutTile`.
static void _actor535700BlackoutTask(Task* task)
{
    if (_gActor535700BlackoutFrames != 0) {
        _actor535700DrawBlackoutTile();
    } else {
        taskKill(task);
    }
    // The terminating update still consumes the shared countdown after teardown.
    _gActor535700BlackoutFrames--;
}

/// Sets the scene's opaque black countdown and spawns a cover task when nonzero.
///
/// `frames` is the complete signed update count; scripts use 300 to start and
/// zero to stop. Zero lets existing tasks exit on their next update. Every
/// nonzero call spawns another task sharing the same countdown, so a single
/// task covers exactly `frames` updates for a positive count. Negative counts
/// are accepted without clamping. Spawn failure is ignored after storing the count.
/// Keep this package and its task descriptor loaded while cover tasks are live.
static void _actor535700SetBlackoutFrames(s32 frames)
{
    _gActor535700BlackoutFrames = frames;
    if (frames != 0) {
        taskSpawnFromTable(&_gActor535700BlackoutTaskDesc, 0, 0, 0);
    }
}

/// Queues the scene's return to the Dryfield night motel balcony.
///
/// Normal completion and skipping select room 2, arrival 5, display resource
/// variant 1 and scene event 6, then finish scene streaming and restore its RNG.
/// Demo 9 leaves all state unchanged. Requires this scene to have been selected
/// successfully and the live save to belong to the Dryfield night stage.
static void _actor535700FinishScene(void)
{
    enum {
        ACTOR_535700_DEMO_SCENE              = 9,
        ACTOR_535700_BALCONY_ARRIVAL         = 5,
        ACTOR_535700_BALCONY_ROOM            = 2,
        ACTOR_535700_SPRITE_VARIANT          = 1,
        ACTOR_535700_POST_SCENE_EVENT        = 6,
        ACTOR_535700_SESSION_TASK_BANK       = GAME_FLOW_RELOAD_TASK_BANK,
        ACTOR_535700_SESSION_TRANSITION_TASK = GAME_FLOW_RELOAD_TASK_SLOT
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_535700_DEMO_SCENE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_535700_BALCONY_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_535700_BALCONY_ROOM;
        gDisplayState.spriteVariant                                 = ACTOR_535700_SPRITE_VARIANT;
        taskSpawn(ACTOR_535700_SESSION_TASK_BANK, ACTOR_535700_SESSION_TRANSITION_TASK, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_535700_POST_SCENE_EVENT;
        streamFinishScene();
    }
}

#include "../../shared/walk/footstep_walk_spawn.inc.c"

/// Initializes and updates the scene's footstep-enabled nineteen-part walker.
///
/// `walkerTask` must own a live TMD model and an `Enemy` in `spawnArg2.pointer`.
/// Its state is an unchecked index: 0 allocates and initializes the work block,
/// then enters state 1; 1 refreshes lighting, advances motion and animation, and
/// draws the ground shadow. State 0 requires no existing work; state 1 requires
/// the initialized task-owned `FootstepWalkWork` and borrowed animation data.
/// The package, model, clip data and frame resources must remain live.
///
/// Publishes the task's work before dispatch for the singleton animation and
/// message handlers. Initialization replaces that pointer with its allocation;
/// allocation failure destroys the enemy and starts task teardown. Published
/// pointers are not cleared at teardown and must not be used afterwards.
static void _actor535700FootstepWalkerTask(Task* walkerTask)
{
    const EnemyTaskFunc stateHandlers[] = {
        _footstepWalkSpawn,
        _actorRenderWalkerFrame,
    };

    // Singleton helpers must see this task's work before its state runs.
    _gFootstepWalkWork = walkerTask->work;
    stateHandlers[walkerTask->state](walkerTask->spawnArg2.pointer, walkerTask);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and begins teardown of its task and model.
///
/// `task` must be live with its owned `Enemy` in `spawnArg2.pointer`.
/// Enemy storage is invalid on return; task work and the model follow
/// `taskKill`'s immediate/deferred release rules. Do not use the task afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walk/footstep_walk_play_steps.inc.c"

#include "../../shared/walk/footstep_walk_tick_anim.inc.c"

#include "../../shared/walk/footstep_walk.inc.c"

/// Replaces the published walker's model flags for `ACTOR_MESSAGE_SET_MODEL_DRAW`.
///
/// Requires a live TMD model in `_gFootstepWalkTask`. Bit 0 permits active drawing;
/// without it the model is excluded. Bit 1 suppresses automatic buffer allocation.
/// All other model flags are cleared and other request bits are ignored. No buffer
/// is allocated or released. The receiver, message ID and second payload are
/// ignored. Returns 0.
static s32 _actor535700SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument)
{
    enum {
        ACTOR_535700_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_535700_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1
    };

    TmdObject* model;

    model = _gFootstepWalkTask->extra.tmd;
    if (flags & ACTOR_535700_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_535700_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/footstep_walk_place.inc.c"

/// Applies turn or footstep commands to the published walker.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with live `_gFootstepWalkWork` and a
/// command borrowed only for this call. Command 0 schedules twenty turning
/// updates while the turn clip plays; it does not select that clip. Command 1
/// enables footstep sounds until work teardown. Other commands do nothing.
/// Context tags, receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor535700ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        ACTOR_535700_WALKER_COMMAND_TURN             = 0,
        ACTOR_535700_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
        ACTOR_535700_WALKER_TURN_UPDATES             = 20
    };

    s32 commandId;

    commandId = command->command;
    switch (commandId) {
        case ACTOR_535700_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_535700_WALKER_TURN_UPDATES;
            break;
        case ACTOR_535700_WALKER_COMMAND_ENABLE_FOOTSTEPS:
            _gFootstepWalkWork->playFootsteps = commandId;
            break;
    }
    return 0;
}

/// Selects this sound walker's allocated work type for the walk-target handler.
///
/// The receiver owns a live `FootstepWalkWork`; the fragment writes its
/// signed-halfword `st.yaw` (4096 units per turn) and `st.travel` (whole moving
/// updates). This type-name alias evaluates no expressions and lasts one inclusion.
#define FOOTSTEP_WALK_WORK_T FootstepWalkWork
#include "../../shared/walk/footstep_walk_to.inc.c"
#undef FOOTSTEP_WALK_WORK_T

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Binds the shared pair-walker spawn to this package's public TaskDesc table.
///
/// The table uses PAIR_WALK_TASK_* indices. This identifier alias evaluates no
/// arguments, captures no locals and is scoped to the following fragment.
#define PAIR_WALK_TASK_TABLE gActor535700PawnGolemTasks
#include "../../shared/walk/pair_walk_spawn.inc.c"

/// Dispatches the scripted Pawn GOLEM's paired-model walker.
///
/// Task state must be 0 for spawn or 1 for motion, lighting and shadow drawing;
/// dispatch is unchecked. The second spawn argument borrows its live Enemy.
/// Spawn owns the PairWalkWork and carried model child; its exit callback
/// begins teardown, after which the borrowed model and rig must not be used.
static void _actor535700PairWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _pairWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _pairWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/pair_walk_exit.inc.c"

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/walk/pair_walk.inc.c"

#include "../../shared/walk/pair_walk_play.inc.c"

#include "../../shared/walk/pair_walk_visibility.inc.c"

/// Ignores `ACTOR_COMMAND_MESSAGE_APPLY` sent to the carried-model walker.
///
/// The receiver, message ID and both payload words are unused; no payload is
/// dereferenced or retained. Returns 0 without changing either model.
static s32 _actor535700IgnorePairWalkerCommand(Task* unusedTask, s32 messageId, s32 unusedArgument, s32 unusedSecondArgument)
{
    return 0;
}

#include "../../shared/walk/pair_walk_to.inc.c"

#include "../../shared/walk/pair_walk_sub_model.inc.c"
