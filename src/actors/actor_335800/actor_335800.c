#include "actors/actor_335800.h"

#include "actors/actor_335800.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/dryfield_night_motel_balcony.h"

/// Selects Flint's repeated-clip playback semantics for nineteen-part arrival.
///
/// Bind to the function identifier with signature
/// `s32(Task*, s32, const AnimationPlayRequest*, s32)`. The arrival fragment
/// calls it once with `ACTOR_MESSAGE_PLAY_ANIMATION`, a stack-owned request
/// and zero fourth argument; the return value is ignored. Define before
/// `actor_motion.h` and keep through `actor_motion_arrive19.inc.c`, then undefine.
#define ACTOR_MOTION_PLAY19_HANDLER _actor335800FlintPlayAnimation
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of Flint, the dog whose model this package carries beside
/// Gary Douglas's.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the head the nineteen-part actor motion
/// handlers run on (`ActorMotion19WalkWork`), so the room script places Flint,
/// plays his clips and walks him from point to point with the same messages
/// it sends Douglas. The model object borrows `model.light` and `model.color`
/// for as long as the block lives.
///
/// No code reads or writes the bytes of `pad_4C6`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;          // Scripted walk: destination, per-frame velocity and the step in progress
    s16             freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4C6[0x2];
} _Actor335800FlintWork;
STATIC_ASSERT_SIZEOF(_Actor335800FlintWork, 0x4C8);

/// Bank and initial motion step selected by Flint's scripted-walk handlers.
enum {
    ACTOR_335800_FLINT_ANIMATION_BANK   = 0,
    ACTOR_335800_FLINT_FACE_TARGET_STEP = 0
};

/// One game-coordinate unit in the approach vectors' signed 16.16 velocity.
enum { ACTOR_335800_WALK_VELOCITY_ONE = 0x10000 };

/// Balcony Z ranges, placement-table entries and ordinary views used by the scenes.
enum {
    ACTOR_335800_BALCONY_LOW_SCENE_Z         = 1001,
    ACTOR_335800_BALCONY_SPLIT_Z             = 3155,
    ACTOR_335800_BALCONY_HIGH_SCENE_Z        = 5570,
    ACTOR_335800_BALCONY_LOW_PLACEMENT       = 1,
    ACTOR_335800_BALCONY_HIGH_PLACEMENT      = 2,
    ACTOR_335800_BALCONY_LOW_EXIT_PLACEMENT  = 3,
    ACTOR_335800_BALCONY_HIGH_EXIT_PLACEMENT = 4,
    ACTOR_335800_BALCONY_LOW_VIEW            = 6,
    ACTOR_335800_BALCONY_HIGH_VIEW           = 5
};

/// Values of `_Actor335800GaryDouglasWork::lightState`.
enum {
    ACTOR_335800_GARY_DOUGLAS_LIGHT_DIMMED        = -1, // The matrices are at half strength and no ground shadow is drawn
    ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED = 0,  // The next tick that draws the model halves the matrices and moves to `_DIMMED`
    ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL          = 1   // The matrices are as the room's lights built them, and the ground shadow is drawn
};

/// Work block of Gary Douglas's body, the package's twenty-part scripted
/// walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotionWalkWork` does - the
/// twenty-part rig, the model state and the walk a room script sends the
/// actor on - and the model object borrows `model.light` and `model.color`
/// for as long as the block lives. The two models hung off the body share
/// those matrices.
///
/// What follows `walk` is the package's own: the tasks of the two attached
/// models, which a draw-mode request keeps in step with the body's; the
/// dimming an actor command asks for, which lasts until the next view
/// relights the model; the delayed free of the model's buffers once the
/// model has been hidden; and the animation cues of the previous tick, whose
/// end fires the gun's muzzle flash.
typedef struct {
    ActorAnimRig20  rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;          // Scripted walk: destination, closing rotation, per-frame velocity and the step in progress
    Task*           headTask;      // Child task drawing the head-and-hat model, which hangs from body part 4; NULL if its spawn failed, which the draw-mode handler does not test
    Task*           gunTask;       // Child task drawing the long gun, which hangs from body part 8, where its muzzle flash appears; NULL if its spawn failed, likewise untested
    s16             lightState;    // `ACTOR_335800_GARY_DOUGLAS_LIGHT_*`: `_FULL` from spawn and from every view that comes up, since each relights the model
    s16             freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s32             prevCueFlags;  // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous ticked frame, so cue 2 fires the muzzle flash once, as it ends
} _Actor335800GaryDouglasWork;
STATIC_ASSERT_SIZEOF(_Actor335800GaryDouglasWork, 0x50C);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor335800GaryDouglasInit(Task* task);
static void _actor335800GaryDouglasUpdate(Task* task);
static void _actor335800GaryDouglasPartIdle(Task* task);
static void _actor335800GaryDouglasExit(Task* task);
static void _actor335800GaryDouglasBindLighting(Task* task);
static void _actor335800GaryDouglasIdle(Task* task);
static void _actor335800GaryDouglasRunWalkStep(Task* task);
static void _actor335800GaryDouglasBeginApproach(Task* task);
static void _actor335800FlintUpdate(Task* task);
static void _actor335800FlintInit(Task* task);
static void _actor335800FlintExit(Task* task);
static void _actor335800FlintBindLighting(Task* task);
static void _actor335800FlintIdle(Task* task);
static void _actor335800FlintRunWalkStep(Task* task);
static void _actor335800FlintFaceTarget(Task* task);
static void _actor335800FlintBeginApproach(Task* task);
static void _actor335800FlintTurnToYaw(Task* task);

/// States of Gary Douglas's attached head-and-hat and gun model tasks.
///
/// `Task::state` must be 0 attach, 1 retain the parent-driven pose, or 2 kill.
/// Attachment borrows the body's selected part and lighting until teardown.
static const TaskFuncTable3 _gActor335800GaryDouglasPartStates = { {
    _modelPlacementAttachPartTask,
    _actor335800GaryDouglasPartIdle,
    taskKill,
} };

/// Gary Douglas's body-task initialization, update and enemy teardown states.
///
/// Indexed without a bounds check by `Task::state` in 0..2 while scene actor
/// control is running. Teardown releases the body and its attached head/gun.
static const TaskFuncTable3 _gActor335800GaryDouglasStates = { {
    _actor335800GaryDouglasInit,
    _actor335800GaryDouglasUpdate,
    _actor335800GaryDouglasExit,
} };

/// Four steps of Gary Douglas's scripted walk, indexed by `walk.motionStep`.
///
/// Indices 0..3 face the destination, begin forward movement, detect arrival,
/// then turn to the destination yaw. The dispatcher does not check bounds.
static const TaskFuncTable4 _gActor335800GaryDouglasWalkSteps = { {
    _actorMotionFaceTarget,
    _actor335800GaryDouglasBeginApproach,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

/// Gary Douglas's local +Z approach velocity in signed 16.16 game units per update.
///
/// A unit-scale root advances 32 units; its rotation converts this velocity
/// into the root parent's frame without applying translation.
static const VECTOR _gActor335800GaryDouglasForwardVelocity = { 0, 0, 32 * ACTOR_335800_WALK_VELOCITY_ONE, 0 };

/// Flint's three task states, dispatched while scene actors are running.
///
/// `Task::state` must be 0 initialize, 1 update, or 2 release the enemy task.
/// Flint's task is independently spawned by the room.
static const TaskFuncTable3 _gActor335800FlintStates = { {
    _actor335800FlintInit,
    _actor335800FlintUpdate,
    _actor335800FlintExit,
} };

/// Four steps of Flint's scripted walk, indexed by `walk.motionStep`.
///
/// Indices 0..3 face the destination, begin forward movement, detect arrival,
/// then turn to the closing yaw and idle. Indexing is unchecked.
static const TaskFuncTable4 _gActor335800FlintWalkSteps = { {
    _actor335800FlintFaceTarget,
    _actor335800FlintBeginApproach,
    _actorMotionArrive19,
    _actor335800FlintTurnToYaw,
} };

/// Flint's local +Z approach velocity in signed 16.16 game units per update.
///
/// A unit-scale root advances 32 units; the approach step rotates the velocity
/// into the root parent's frame before integration.
static const VECTOR _gActor335800FlintForwardVelocity = { 0, 0, 32 * ACTOR_335800_WALK_VELOCITY_ONE, 0 };

static void _actor335800SceneGroundShadowTask(Task* task);
static void _actor335800StartSceneVariantTask(Task* task);
static void _actor335800RestorePlayerAnimationOnBattleResetTask(Task* task);
static void _actor335800SceneScreenShakeTask(Task* task);

static void _actor335800StageSceneAudioStart(void);
static void _actor335800StartScenePlayback(void);
static void _actor335800FinishStreamedScene(void);
static void _actor335800SpawnSceneGroundShadow(void);
static void _actor335800StartBalconyMovies(void);
static void _actor335800EnableDisplay(void);
static void _actor335800SetStageAmbientMuted(s32 muted);
static void _actor335800SpawnSceneScreenShake(void);

static s32  _actor335800GaryDouglasSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32  _actor335800GaryDouglasRequestDim(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg);
static s32  _actor335800FlintStartWalk(Task* task, s32 messageId, const ActorTransform* destination, const ActorMotionWalkAnim* clips);
static s32  _actor335800FlintPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32  _actor335800FlintSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32  _actor335800FlintIgnoreCommand(Task* task, s32 messageId, s32 unusedCommand, s32 unusedArg);
static void _actor335800CancelStreamedScene(void);
static void _actor335800SetSceneRoom(u8 room);
static void _actor335800PlacePlayerForScene(void);
static void _actor335800PlacePlayerAfterScene(s32 faceQuarterTurn);
static void _actor335800RestorePlayerView(void);
static void _actor335800SetSceneSpriteBatchesHidden(s32 hidden);
static void _actor335800SetSceneEvent(s8 sceneEvent);
static void _actor335800StopStageMusic(s32 fadeTicks);
static void _actor335800SetPostSceneObjective(void);
static void _actor335800LockAttachmentsAndCancelEffects(void);
static void _actor335800GaryDouglasPartTask(Task* task);
static void _actor335800GaryDouglasTask(Task* task);
static void _actor335800FlintTask(Task* task);

#include "assets/rooms/dryfield/night_motel_balcony/gary_and_flint_clips_1.inc"

/// Spawn recipes for the balcony scene's shadow and three control tasks.
///
/// Indices 0..3 create a moving ground shadow, wait for battle reset to replay
/// the player's scene animation, start stream 59/60 from the first spawn word,
/// or shake the display. Only index 0 has a coordinate body; all run at priority 192.
static TaskDesc _gActor335800SceneTasks[] = {
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, _actor335800SceneGroundShadowTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor335800RestorePlayerAnimationOnBattleResetTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor335800StartSceneVariantTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor335800SceneScreenShakeTask, { .value = 0 } },
};

/// Five words installed at player weapon-bank indices 47..51 for balcony scenes.
///
/// Index 47 becomes NULL; 48..51 borrow the four package-owned clips. Copying
/// the pointers does not copy clip data, which must stay loaded through playback.
#include "assets/rooms/dryfield/night_motel_balcony/player_scene_bank.inc"

/// Request to overwrite five words of the player's equipped-weapon bank extension.
///
/// Copies the full pointer table, including its leading NULL, starting at set
/// index 47. The request is borrowed synchronously; the copied clips remain
/// borrowed from this loaded package while playback uses them.
static AnimationBankCopyRequest _gActor335800CopyPlayerSceneAnimations = {
    .sets  = _gActor335800PlayerSceneAnimationExtension,
    .count = ARRAY_SIZE(_gActor335800PlayerSceneAnimationExtension)
};

/// Player clips installed by the extension and Douglas clips in bank 0.
enum {
    ACTOR_335800_PLAYER_INITIAL_BANK       = 1,
    ACTOR_335800_PLAYER_SCENE_OPENING_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_335800_PLAYER_BALCONY_ENTRY_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_335800_PLAYER_SCENE_CLIP_51      = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_335800_PLAYER_RESTORE_CLIP       = 9,
    ACTOR_335800_GARY_DOUGLAS_GUNSHOT_CLIP = 2,
    ACTOR_335800_GARY_DOUGLAS_SCENE_CLIP_3 = 3,
    ACTOR_335800_GARY_DOUGLAS_SCENE_CLIP_4 = 4
};

/// Unreferenced five-word record with bank-1/clip-1 reset values; its role is unproven.
static AnimationPlayRequest D_actor_335800_80164E2C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 48 at the common opening of the Burner retreat and defeat scenes.
///
/// Blends for twenty whole frames and disables grid participation. Event
/// playback copies the request and resolves the equipped weapon's bank in that copy.
static AnimationPlayRequest _gActor335800PlayerSceneOpeningAnimation = {
    .source               = { .index = ACTOR_335800_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_335800_PLAYER_SCENE_OPENING_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Resets player clip 49 in the balcony entry scene with grid participation disabled.
///
/// Event playback copies the request before resolving the equipped weapon's bank.
static AnimationPlayRequest _gActor335800PlayerBalconyEntryAnimation = {
    .source               = { .index = ACTOR_335800_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_335800_PLAYER_BALCONY_ENTRY_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Unreferenced five-word record with bank-1/clip-50 reset values; its role is unproven.
static AnimationPlayRequest D_actor_335800_80164E68 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player clip 51 at Burner scene boundaries and after a battle reset.
///
/// Disables grid participation. Event playback resolves the weapon bank in a
/// copy; the battle-reset task writes `source.index` in this writable record.
static AnimationPlayRequest _gActor335800PlayerSceneAnimation51 = {
    .source               = { .index = ACTOR_335800_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_335800_PLAYER_SCENE_CLIP_51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restores player weapon-bank clip 9 and grid participation after entry or skip.
///
/// Resets without blending; event playback resolves the weapon bank in its copy.
static AnimationPlayRequest _gActor335800PlayerRestoreAnimation = {
    .source               = { .index = ACTOR_335800_PLAYER_INITIAL_BANK },
    .animationId          = ACTOR_335800_PLAYER_RESTORE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Player poses for balcony entry and the low/high ends of later scene playback.
///
/// Positions use the root parent's game units, angles 4096 units per turn.
/// Entry 0 is the entry/skip pose; 1/2 face +X at the low/high end, and 3/4
/// use yaw 2047 at those same ends. Placement messages borrow individual entries.
static ActorTransform _gActor335800PlayerScenePlacements[] = {
    { { -7180, -3200, 4310, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } }, // Entry and skip recovery
    { { -6280, -3200, 1000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } }, // Low-end scene pose
    { { -6280, -3200, 5570, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } }, // High-end scene pose
    { { -6280, -3200, 1000, 0 }, { 0, 2047, 0, 0 } },                           // Low-end exit pose
    { { -6280, -3200, 5570, 0 }, { 0, 2047, 0, 0 } },                           // High-end exit pose
};

/// Unreferenced bank-0/clip-1 reset and twenty-frame blend records; their role is unproven.
static AnimationPlayRequest D_actor_335800_80164F1C[2] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Resets Douglas's bank-0 gunshot clip in the balcony entry scene.
///
/// Clip 2 drives slots 1..19; slot 1's cue-2 falling edge fires the gun flash.
/// The actor ignores the request's world-collision choice and borrows it only
/// through dispatch; loaded clip data remains borrowed during playback.
static AnimationPlayRequest _gActor335800GaryDouglasGunshotAnimation = {
    .source               = { .index = ACTOR_MOTION_WALK_ANIMATION_BANK },
    .animationId          = ACTOR_335800_GARY_DOUGLAS_GUNSHOT_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Resets Douglas's bank-0 clip 3 after the entry scene's second placement.
///
/// No blend; the actor ignores the world-collision choice. The request is
/// borrowed through dispatch and the package's clip data through playback.
static AnimationPlayRequest _gActor335800GaryDouglasSceneAnimation3 = {
    .source               = { .index = ACTOR_MOTION_WALK_ANIMATION_BANK },
    .animationId          = ACTOR_335800_GARY_DOUGLAS_SCENE_CLIP_3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Resets Douglas's bank-0 clip 4 in view 19 near the end of the entry scene.
///
/// No blend; the actor ignores the world-collision choice. The request is
/// borrowed through dispatch and the package's clip data through playback.
static AnimationPlayRequest _gActor335800GaryDouglasSceneAnimation4 = {
    .source               = { .index = ACTOR_MOTION_WALK_ANIMATION_BANK },
    .animationId          = ACTOR_335800_GARY_DOUGLAS_SCENE_CLIP_4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Douglas's initial entry-scene pose and reference for the moving ground shadow.
///
/// Position uses root-parent game units and yaw 2047 in a 4096-unit turn. The
/// shadow begins 1000 units below this Z and decelerates after passing it.
static ActorTransform _gActor335800GaryDouglasSceneArrivalPlacement = { { -4000, 0, 1970, 0 }, { 0, 2047, 0, 0 } };

/// Douglas's second entry-scene placement, before his bank-0 clip 3.
///
/// XYZ uses the root parent's game units and yaw is minus a quarter turn.
/// The placement is borrowed only through its synchronous message dispatch.
static ActorTransform _gActor335800GaryDouglasSceneSecondPlacement = {
    .pos = { -7730, 0, 1280, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Douglas's third entry-scene placement, retained through view 19 and clip 4.
///
/// XYZ uses the root parent's game units and yaw is minus a quarter turn.
/// The placement is borrowed only through its synchronous message dispatch.
static ActorTransform _gActor335800GaryDouglasSceneThirdPlacement = {
    .pos = { -6260, 0, 1280, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 }
};

/// Command requesting Douglas's half-strength lighting and ground-shadow suppression.
enum { ACTOR_335800_GARY_DOUGLAS_COMMAND_DIM = 0 };

/// Douglas dim request between the moving-shadow cue and his second placement.
///
/// Context is stage 3/area 29 but Douglas reads only the command. The next
/// visible update halves both shared lighting matrices until a view relights
/// the model; another request can halve them again. Dispatch borrows the record.
static ActorCommand _gActor335800GaryDouglasDimCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_GARY_DOUGLAS_COMMAND_DIM
};

/// Unreferenced twenty zero bytes; the animation-request interpretation is unproven.
static AnimationPlayRequest D_actor_335800_80164FCC = { 0 };

/// Flint's only embedded clip, shared by entry-scene reset and blended restarts.
enum { ACTOR_335800_FLINT_SCENE_CLIP = 1 };

/// Resets Flint's bank-0 clip 1 after placing him in the balcony entry scene.
///
/// Drives slots 1..18 and ticks once immediately. The receiver ignores the
/// collision choice and borrows this request only through dispatch.
static AnimationPlayRequest _gActor335800FlintSceneAnimationReset = {
    .source               = { .index = ACTOR_335800_FLINT_ANIMATION_BANK },
    .animationId          = ACTOR_335800_FLINT_SCENE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Restarts Flint's bank-0 clip 1 with a ten-frame blend at four entry-scene cues.
///
/// Captures the advancing pose of slots 1..18 before seeking each track's
/// start, then ticks once. Collision choice is ignored; dispatch borrows the
/// request, and playback keeps borrowing the loaded clip data.
static AnimationPlayRequest _gActor335800FlintSceneAnimationBlend = {
    .source               = { .index = ACTOR_335800_FLINT_ANIMATION_BANK },
    .animationId          = ACTOR_335800_FLINT_SCENE_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Flint's entry-scene placement while view 18 shows his repeated clip.
///
/// XYZ uses the root parent's game units; yaw -1650 uses 4096 units per turn.
/// The receiver copies XYZ and Euler rotation through synchronous dispatch.
static ActorTransform _gActor335800FlintScenePlacement = {
    .pos = { -1287, 0, -2750, 0 }, .rot = { 0, -1650, 0, 0 }
};

/// Commands in the Burner's stage-3/area-29 namespace.
enum {
    ACTOR_335800_BURNER_COMMAND_BEGIN_FIGHT    = 0xFFFF,
    ACTOR_335800_BURNER_COMMAND_ENTRANCE       = 1,
    ACTOR_335800_BURNER_COMMAND_AIM_AND_IMPACT = 2,
    ACTOR_335800_BURNER_COMMAND_ADVANCE        = 3,
    ACTOR_335800_BURNER_COMMAND_FLAME_BREATH   = 4,
    ACTOR_335800_BURNER_COMMAND_BALCONY_IMPACT = 5,
    ACTOR_335800_BURNER_COMMAND_TURN           = 6,
    ACTOR_335800_BURNER_COMMAND_FLAME_RETREAT  = 7,
    ACTOR_335800_BURNER_COMMAND_DEPARTURE      = 8,
    ACTOR_335800_BURNER_COMMAND_DEFEAT         = 10
};

/// Starts the Burner's scripted entrance across the balcony.
///
/// Stage 3/area 29, command 1 selects scene state 1 and resets its substate;
/// the scene handler places the root and begins the entrance clip/movement.
/// The record is borrowed through synchronous dispatch.
static ActorCommand _gActor335800BurnerEntranceCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_ENTRANCE
};

/// Starts the Burner's placed head-aiming and impact sequence in view 18.
///
/// Stage 3/area 29, command 2 selects scene state 2 and resets its substate.
/// The record is borrowed through synchronous dispatch.
static ActorCommand _gActor335800BurnerAimAndImpactCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_AIM_AND_IMPACT
};

/// Starts the Burner's balcony advance and timed impact cues in view 19.
///
/// Stage 3/area 29, command 3 selects scene state 3 and resets its substate;
/// the scene handler advances along +Z before its sound and sprite cues.
/// The record is borrowed through synchronous dispatch.
static ActorCommand _gActor335800BurnerAdvanceCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_ADVANCE
};

/// Command returning the Burner from the entry scene to combat initialization.
///
/// Context is stage 3/area 29. Value 65535 selects task state 2, whose callback
/// runs combat, and clears both behaviour and substate. Dispatch borrows this record.
static ActorCommand _gActor335800BurnerBeginFightCommand = { .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_BEGIN_FIGHT };

/// Starts the Burner's fixed-target flame breath after Douglas's gunshot cue.
///
/// Stage 3/area 29, command 4 selects scene state 4 and resets its substate.
/// The scene breath emits collision-disabled flames. Dispatch borrows the record.
static ActorCommand _gActor335800BurnerFlameBreathCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_FLAME_BREATH
};

/// Starts the Burner's timed balcony sound, shake and sprite-impact cues.
///
/// Stage 3/area 29, command 5 selects scene state 5 and resets its substate.
/// The entry scene sends it before stream playback starts; dispatch borrows it.
static ActorCommand _gActor335800BurnerBalconyImpactCommand = {
    .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_BALCONY_IMPACT
};

/// Command selecting the Burner's turn-to-walk scene sequence.
///
/// Context is stage 3/area 29. Value 6 enters scene task state 1, selects scene
/// sequence 6 and resets its substate. Both later balcony scenes borrow this record.
static ActorCommand _gActor335800BurnerTurnCommand = { .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_TURN };

/// Command selecting the Burner's flame bursts and walking retreat.
///
/// Context is stage 3/area 29. Value 7 enters scene task state 1, selects scene
/// sequence 7 and resets its substate. Stream 59's ending borrows this record.
static ActorCommand _gActor335800BurnerFlameRetreatCommand = { .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_FLAME_RETREAT };

/// Command selecting the Burner's final departure and reward release.
///
/// Context is stage 3/area 29. Value 8 enters scene task state 1, selects scene
/// sequence 8 and resets its substate. Stream 59's ending borrows this record.
static ActorCommand _gActor335800BurnerDepartureCommand = { .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_DEPARTURE };

/// Command selecting the Burner's defeat task state during stream 60's ending.
///
/// Context is stage 3/area 29. Value 10 selects task state 3 and clears behaviour
/// and substate. Dispatch borrows this complete four-byte record.
static ActorCommand _gActor335800BurnerDefeatCommand = { .context = { .loc = { 3, 29 } }, .command = ACTOR_335800_BURNER_COMMAND_DEFEAT };

/// Stream key for the Burner, Douglas and Flint balcony entry scene.
///
/// Group 3, stream 58, first qualifier 11; playback supplies second qualifier
/// zero. Caption/audio selection borrows the complete six-byte key until END.
static EvsSceneKey _gActor335800BalconyEntrySceneKey = {
    .group = 3, .streamId = 58, .subId = 11
};

/// Stream key for the balcony scene that ends with the Burner's retreat.
///
/// Group 3, stream 59, first qualifier 11; the second qualifier is implicitly
/// zero. Caption/audio selection borrows the key until the event script ends.
static EvsSceneKey _gActor335800BurnerRetreatSceneKey = { .group = 3, .streamId = 59, .subId = 11 };

/// Stream key for the balcony scene that ends with the Burner's defeat.
///
/// Group 3, stream 60, first qualifier 11; the second qualifier is implicitly
/// zero. Caption/audio selection borrows the key until the event script ends.
static EvsSceneKey _gActor335800BurnerDefeatSceneKey = { .group = 3, .streamId = 60, .subId = 11 };

/// Scene-script selectors and cues used by the two Burner endings.
enum {
    ACTOR_335800_BURNER_PLACEMENT    = 0,
    ACTOR_335800_SCENE_ROOM          = 3,
    ACTOR_335800_RETREAT_SCENE_EVENT = 7,
    ACTOR_335800_DEFEAT_SOUND        = 0x401F000A,
    ACTOR_335800_SCENE_FADE_WHITE    = 1
};

/// Placed actor indices and presentation choices in the balcony entry/skip scripts.
///
/// Actor show mode 1 permits drawing and automatic buffers (Douglas/Flint also
/// allocate immediately); hide mode 2 schedules buffer release and disables
/// automatic recovery. These values apply to all three loaded scene actors.
enum {
    ACTOR_335800_GARY_DOUGLAS_PLACEMENT   = 1,
    ACTOR_335800_FLINT_PLACEMENT          = 2,
    ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE = 2,
    ACTOR_335800_BALCONY_ENTRY_VIEW       = 22,
    ACTOR_335800_BURNER_ENTRANCE_VIEW     = 17,
    ACTOR_335800_FLINT_SCENE_VIEW         = 18,
    ACTOR_335800_DOUGLAS_GUNSHOT_VIEW     = 21,
    ACTOR_335800_BALCONY_ENTRY_END_VIEW   = 19
};

EvsCommand gActor335800BalconyEntryScript[72] = {
    // Hand presentation to the room movies, then stage the entry stream and actors.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StartBalconyMovies }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800EnableDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor335800BalconyEntrySceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneScreenShake }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = _gActor335800PlayerScenePlacements } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_BALCONY_ENTRY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Run the Burner's entrance while Flint's single clip restarts on timed cues.
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerBalconyEntryAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerBalconyImpactCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 49 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerEntranceCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_BURNER_ENTRANCE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_FLINT_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerAimAndImpactCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor335800FlintScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800FlintSceneAnimationReset } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800FlintSceneAnimationBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800FlintSceneAnimationBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800FlintSceneAnimationBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800FlintSceneAnimationBlend } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    // Douglas's gunshot, moving shadow and dim request precede his later placements.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor335800GaryDouglasSceneArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800GaryDouglasGunshotAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_DOUGLAS_GUNSHOT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneGroundShadow }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerFlameBreathCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800GaryDouglasDimCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor335800GaryDouglasSceneSecondPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800GaryDouglasSceneAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerAdvanceCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor335800GaryDouglasSceneThirdPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_BALCONY_ENTRY_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor335800GaryDouglasSceneAnimation4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade out streamed presentation before restoring the player and Burner combat.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerRestoreAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerBeginFightCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_335800_BALCONY_HIGH_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor335800BalconyBattleStartScript[5] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor335800BalconyEntrySkipScript[19] = {
    // Cancel the stream under a fade before restoring the actors' combat presentation.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800CancelStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800LockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerRestoreAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = _gActor335800PlayerScenePlacements } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_GARY_DOUGLAS_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerBeginFightCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_FLINT_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_335800_SCENE_ACTOR_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = ACTOR_335800_BALCONY_HIGH_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800EnableDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Common opening subscript for the Burner's retreat and defeat streams.
///
/// Requires a selected stream with staged audio, the night-balcony resources,
/// the player and its writable weapon-animation bank. Locks attachments, hides
/// the player, starts playback and times the lamp sprites before selecting
/// room 3 and objective 24. All 21 commands remain loaded until RETURN pops
/// the caller; waits count interpreter updates and a zero wait still yields.
static EvsCommand _gActor335800BalconySceneOpeningScript[] = {
    // Hold presentation and install the player's package-owned clips before playback.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800LockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerSceneOpeningAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800PlacePlayerForScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneScreenShake }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Time the balcony lamp burst against the selected stream.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetSceneSpriteBatchesHidden }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetSceneSpriteBatchesHidden }, { .value = true }, { .value = false }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Keep the stream active as control returns to its retreat or defeat ending.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor335800SetSceneRoom }, { .value = ACTOR_335800_SCENE_ROOM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SetPostSceneObjective }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Stream 59's ending: Burner flame retreat, departure and return to play.
///
/// Called after the common opening with the Burner at placement index 0 and
/// the player and balcony resources loaded. Runs scene sequences 7 and 8,
/// finishes streaming, restores the player/view and selects scene event 7.
/// The 26 commands and borrowed payloads remain loaded through RETURN;
/// this subscript requires a live caller on the interpreter's return stack.
static EvsCommand _gActor335800BurnerRetreatSceneEndingScript[] = {
    // Hide the player while the Burner emits flames and retreats.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerSceneAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerFlameRetreatCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 270 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // The departure sequence also releases the encounter reward.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerDepartureCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore the ordinary balcony view and model before releasing scripted control.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800RestorePlayerView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800PlacePlayerAfterScene }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor335800SetSceneEvent }, { .value = ACTOR_335800_RETREAT_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Stream 60's ending: Burner defeat, sound cue and return to player control.
///
/// Called after the common opening with the Burner at placement index 0 and
/// its sound bank and balcony resources loaded. Enters Burner defeat, places
/// the player with quarter-turn yaw, fades to white, ends streaming, restores
/// the model/view and releases scripted control. All 21 commands and payloads
/// remain loaded through RETURN, which requires a live script caller.
static EvsCommand _gActor335800BurnerDefeatSceneEndingScript[] = {
    // Keep the player hidden through the Burner's defeat playback.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerSceneAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerDefeatCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800PlacePlayerAfterScene }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Use the Burner's loaded sound bank during the white-fade transition.
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_335800_DEFEAT_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = ACTOR_335800_SCENE_FADE_WHITE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800RestorePlayerView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand gActor335800BurnerRetreatSceneScript[9] = {
    // Select stream 59 before entering the balanced opening/retreat subscript calls.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerSceneAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor335800BurnerRetreatSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor335800BalconySceneOpeningScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor335800BurnerRetreatSceneEndingScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor335800BurnerDefeatSceneScript[10] = {
    // Stop stage music and select stream 60 before the opening/defeat subscript calls.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor335800CopyPlayerSceneAnimations } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor335800PlayerSceneAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800StopStageMusic }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor335800BurnerDefeatSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_335800_BURNER_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor335800BurnerTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor335800BalconySceneOpeningScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = _gActor335800BurnerDefeatSceneEndingScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/gary/body.inc"

#include "assets/characters/gary/head_hat.inc"

#include "assets/characters/gary/shotgun.inc"

#include "assets/rooms/dryfield/night_motel_balcony/gary_and_flint_clips_2.inc"

/// Gary Douglas's bank-0 clip table for his twenty-part rig.
///
/// Clip IDs 1..4 borrow the four loaded animation sets; entry 0 is NULL and
/// cannot be played. `_gActorMotionAnimBanks[0]` supplies this table to the rig.
/// The table and all referenced data must stay loaded while playback uses them.
#include "assets/rooms/dryfield/night_motel_balcony/gary_bank.inc"

/// Actor-motion bank selection for Douglas's twenty-part body (only source.index 0).
///
/// Requests select loaded clips 1..4. This table and its borrowed sets must
/// remain loaded for playback; Flint's nineteen-part rig has a separate bank.
static AnimationSet** _gActorMotionAnimBanks[1] = {
    _gActor335800GaryDouglasAnimationBank,
};

TaskDesc gActor335800GaryDouglasTasks[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor335800GaryDouglasTask, { .model = &_gGaryBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _actor335800GaryDouglasPartTask, { .model = &_gGaryHeadHat } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _actor335800GaryDouglasPartTask, { .model = &_gGaryShotgun } },
};

/// Gary Douglas's body-task interface for animation, placement, draw, walk and dimming.
///
/// Installed at initialization and borrowed until body teardown. Payloads are
/// respectively AnimationPlayRequest*, ActorTransform*, draw mode 0..3,
/// ActorTransform* plus optional ActorMotionWalkAnim*, and ActorCommand*.
/// Unsupported IDs return zero at the end marker; never dispatch that marker.
static TaskMessageEntry _gActor335800GaryDouglasMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor335800GaryDouglasSetDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor335800GaryDouglasRequestDim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/characters/flint/body.inc"

#include "assets/rooms/dryfield/night_motel_balcony/gary_and_flint_clips_3.inc"

/// Flint's embedded clip table, with clip 0 absent and clip 1 loaded.
///
/// Playback borrows this table and its clip data while the actor package is
/// loaded. Animation IDs index its two entries directly; the NULL entry is
/// not playable. There is no embedded clip-table extension.
#include "assets/rooms/dryfield/night_motel_balcony/flint_bank.inc"

/// Indexed banks for Flint's nineteen-part animation rig; bank 0 is the only bank.
///
/// `AnimationPlayRequest.source.index` selects the bank, whose pointer table
/// the rig borrows until playback ends or the actor package is unloaded.
static AnimationSet** _gActor335800FlintAnimationBanks[1] = {
    _gActor335800FlintAnimationBank,
};

TaskDesc gActor335800FlintTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor335800FlintTask, { .model = &gFlintBody } };

/// Flint's synchronous placed-actor message table.
///
/// Plays clips, applies full Euler placement, changes drawing, starts a walk,
/// and ignores actor commands. The final end-ID/NULL pair stops unsupported
/// lookups; the table and callbacks must stay loaded while his task can receive
/// messages. Walks require explicit valid clips: the retained NULL default
/// selects clip 13 beyond Flint's two-entry bank.
static TaskMessageEntry _gActor335800FlintMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor335800FlintPlayAnimation },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor335800FlintSetDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actor335800FlintStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor335800FlintIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void _actor335800SetView(s32 view);

/// Initializes the scene shadow's root 1000 units before the pose along parent Z.
///
/// Borrows a live writable coordinate and a placement in its parent's frame.
/// Copies XYZ and Euler angles (4096 units per turn), rebuilds the rotation
/// and marks composition dirty. The offset follows parent Z regardless of yaw.
static inline void _actor335800PlaceSceneShadow(GfxCoord* coord, const ActorTransform* placement)
{
    enum { ACTOR_335800_SHADOW_START_Z_OFFSET = 1000 };
    const SVECTOR* rotation;

    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz - ACTOR_335800_SHADOW_START_Z_OFFSET;
    rotation            = &placement->rot;
    coord->param.rot.vx = rotation->vx;
    coord->param.rot.vy = rotation->vy;
    coord->param.rot.vz = rotation->vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves and draws the scene's temporary ground shadow, then ends its task.
///
/// Requires a coordinate body and initial state 0. The shadow starts 1000
/// parent-coordinate units behind the scene pose, moving +Z at 100 units per
/// tick. After passing the pose it loses 6 units of speed each tick, stops
/// below -60 and lingers for four ticks. A ready view ends either active phase.
/// `killCountdown` holds signed speed, then elapsed linger ticks. The final
/// ground projection remains after teardown, as in the binary.
static void _actor335800SceneGroundShadowTask(Task* task)
{
    enum {
        ACTOR_335800_SHADOW_INIT          = 0,
        ACTOR_335800_SHADOW_MOVING        = 1,
        ACTOR_335800_SHADOW_LINGERING     = 2,
        ACTOR_335800_SHADOW_INITIAL_SPEED = 100,
        ACTOR_335800_SHADOW_DECELERATION  = 6,
        ACTOR_335800_SHADOW_STOP_SPEED    = -60,
        ACTOR_335800_SHADOW_LINGER_TICKS  = 4,
        ACTOR_335800_SHADOW_HALF_SIZE     = 2048
    };
    GfxCoord* coord;
    VECTOR3   groundPoint;

    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_335800_SHADOW_INIT:
            _actor335800PlaceSceneShadow(coord, &_gActor335800GaryDouglasSceneArrivalPlacement);
            task->killCountdown = ACTOR_335800_SHADOW_INITIAL_SPEED;
            task->state++;
            // Initialization also performs the first movement tick.
        case ACTOR_335800_SHADOW_MOVING:
            if (_gActor335800GaryDouglasSceneArrivalPlacement.pos.vz < coord->coord.t[2]) {
                task->killCountdown -= ACTOR_335800_SHADOW_DECELERATION;
                if (task->killCountdown < ACTOR_335800_SHADOW_STOP_SPEED) {
                    task->killCountdown = 0;
                    task->state++;
                }
            }
            coord->coord.t[2] += task->killCountdown;
            if (gGameSession->viewReady != 0) {
                taskKill(task);
            }
            break;
        case ACTOR_335800_SHADOW_LINGERING:
            if (++task->killCountdown < ACTOR_335800_SHADOW_LINGER_TICKS) {
                if (gGameSession->viewReady != 0) {
                    taskKill(task);
                }
            } else {
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
    // Sample the coordinate refresh list's composed position, including the final tick.
    if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.coordBody->coord->workm), &groundPoint) != 0) {
        effectDrawGroundShadow(&groundPoint, ACTOR_335800_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
    }
}

/// Stages audio start for the selected streamed scene for a later CD-queue commit.
///
/// Requires a selected scene and playback storage that survives consumption;
/// without a selected slot the previous deferred request is retained.
static void _actor335800StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues playback of the selected streamed scene and marks its audio as starting.
///
/// Selection and playback buffers must survive consumption, with CD-queue
/// capacity available. No selected slot enters scene-playing mode immediately.
static void _actor335800StartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes streamed scene playback and restores the pre-scene random generators.
static void _actor335800FinishStreamedScene(void)
{
    streamFinishScene();
}

/// Cancels queued scene work and finishes streamed playback during skip recovery.
///
/// Discards the deferred CD request, requests queue cancellation and finishes
/// the scene, restoring the pre-scene random generators. Requires a prior scene
/// selection whose saved random values remain live.
static void _actor335800CancelStreamedScene(void)
{
    cdCmdCancelScene();
}

/// Spawns the balcony scene's moving ground shadow; allocation failure is ignored.
static void _actor335800SpawnSceneGroundShadow(void)
{
    enum { ACTOR_335800_SCENE_GROUND_SHADOW_TASK = 0 };
    taskSpawnFromTable(_gActor335800SceneTasks, ACTOR_335800_SCENE_GROUND_SHADOW_TASK, 0, 0);
}

/// Selects the saved and live scene room and requests deferred object relinking.
///
/// `roomState` is a valid byte-sized room key in the current area. This updates the
/// live save slot without loading a room or requesting a view image.
static void _actor335800SetSceneRoom(u8 room)
{
    gGameSession->location.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = room;
    gGameSession->roomObjsDirty                                                                    = true;
}

/// Places the live player at the corresponding end of the balcony for the scene.
///
/// Requires the player's live TMD root when its slot exists. Z in 1001..3154
/// selects the low end, 3155..5569 the high end, with yaw 1024 in a 4096-unit
/// turn. Other positions and an absent player are unchanged. Payloads are
/// borrowed by synchronous placement messages; the second test reads Z again.
static void _actor335800PlacePlayerForScene(void)
{
    Task*     playerTask;
    GfxCoord* coord;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask != NULL) {
        coord = playerTask->extra.tmd->coords;
        if ((u32)(coord->coord.t[2] - ACTOR_335800_BALCONY_SPLIT_Z) < ACTOR_335800_BALCONY_HIGH_SCENE_Z - ACTOR_335800_BALCONY_SPLIT_Z) {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor335800PlayerScenePlacements[ACTOR_335800_BALCONY_HIGH_PLACEMENT], 0);
        }
        if ((u32)(coord->coord.t[2] - ACTOR_335800_BALCONY_LOW_SCENE_Z) < ACTOR_335800_BALCONY_SPLIT_Z - ACTOR_335800_BALCONY_LOW_SCENE_Z) {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor335800PlayerScenePlacements[ACTOR_335800_BALCONY_LOW_PLACEMENT], 0);
        }
    }
}

/// Places the player at the balcony end selected by the live root's Z position.
///
/// Z at least 3155 selects the high end; otherwise the low end. Nonzero
/// `faceQuarterTurn` selects yaw 1024, zero yaw 2047 (4096 units per turn).
/// Uses table entries 1..4 and borrowed synchronous placement payloads. An
/// absent player is unchanged; a present player requires its live TMD root.
static void _actor335800PlacePlayerAfterScene(s32 faceQuarterTurn)
{
    Task*     playerTask;
    GfxCoord* coord;
    s32       lowPlacementIndex;
    s32       highPlacementIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask != NULL) {
        coord             = playerTask->extra.tmd->coords;
        lowPlacementIndex = ACTOR_335800_BALCONY_LOW_PLACEMENT;
        if (faceQuarterTurn != 0) {
            highPlacementIndex = ACTOR_335800_BALCONY_HIGH_PLACEMENT;
        } else {
            lowPlacementIndex  = ACTOR_335800_BALCONY_LOW_EXIT_PLACEMENT;
            highPlacementIndex = ACTOR_335800_BALCONY_HIGH_EXIT_PLACEMENT;
        }
        if (coord->coord.t[2] >= ACTOR_335800_BALCONY_SPLIT_Z) {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor335800PlayerScenePlacements[highPlacementIndex], 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor335800PlayerScenePlacements[lowPlacementIndex], 0);
        }
    }
}

/// Selects the saved and live room view and requests its image and objects reload.
///
/// `view` is a valid 1-based view slot in the current room, stored as a byte.
/// This only schedules the reload; it does not load or apply the view itself.
static inline void _actor335800SetView(s32 view)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = view;
    gGameSession->location.view                            = view;
    gGameSession->viewDirty                                = 1;
    gGameSession->roomObjsDirty                            = 1;
}

/// Restores balcony view 5 or 6 from the player's position after scene playback.
///
/// Z at least 3155 selects view 5; otherwise view 6. Updates the live save and
/// session, requesting image reload and object relinking. An absent player
/// leaves both unchanged; a present player requires its live TMD root.
static void _actor335800RestorePlayerView(void)
{
    Task* playerTask;
    s16   view;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask != NULL) {
        view = ACTOR_335800_BALCONY_LOW_VIEW;
        if (playerTask->extra.tmd->coords->coord.t[2] >= ACTOR_335800_BALCONY_SPLIT_Z) {
            view = ACTOR_335800_BALCONY_HIGH_VIEW;
        }
        _actor335800SetView(view);
    }
}

/// Shows or hides the balcony scene's two sprite ranges and arms the lamp burst.
///
/// Requires loaded night-balcony sprites (stage 3, area 29, sprite variant 1).
/// View 39 owns five batch records, including its terminator;
/// this changes only batches 2 and 3. `hidden` 0 shows, 1 hides and requests
/// the room's lamp burst through flag 0x7F, other values do nothing. Showing
/// does not reset the flag. The sprite arrays remain owned by the room overlay.
static void _actor335800SetSceneSpriteBatchesHidden(s32 hidden)
{
    enum {
        ACTOR_335800_SCENE_SPRITES_VISIBLE     = 0,
        ACTOR_335800_SCENE_SPRITES_HIDDEN      = 1,
        ACTOR_335800_SCENE_SPRITE_VIEW_INDEX   = 38,
        ACTOR_335800_SCENE_FIRST_SPRITE_BATCH  = 2,
        ACTOR_335800_SCENE_SECOND_SPRITE_BATCH = 3,
        ACTOR_335800_LAMP_BURST_REQUESTED      = 1
    };
    GameSession*     session;
    GameLocationKey* location;
    SpriteView*      views;
    SpriteBatch*     batches;

    session  = gGameSession;
    location = &session->location;
    views    = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];
    switch (hidden) {
        case ACTOR_335800_SCENE_SPRITES_VISIBLE:
            batches                                                = views[ACTOR_335800_SCENE_SPRITE_VIEW_INDEX].batches;
            batches[ACTOR_335800_SCENE_FIRST_SPRITE_BATCH].hidden  = ACTOR_335800_SCENE_SPRITES_VISIBLE;
            batches[ACTOR_335800_SCENE_SECOND_SPRITE_BATCH].hidden = ACTOR_335800_SCENE_SPRITES_VISIBLE;
            break;
        case ACTOR_335800_SCENE_SPRITES_HIDDEN:
            batches                                                = views[ACTOR_335800_SCENE_SPRITE_VIEW_INDEX].batches;
            batches[ACTOR_335800_SCENE_FIRST_SPRITE_BATCH].hidden  = hidden;
            batches[ACTOR_335800_SCENE_SECOND_SPRITE_BATCH].hidden = hidden;
            gameFlagSetNibble(GAME_FLAG_07F, ACTOR_335800_LAMP_BURST_REQUESTED);
            break;
    }
}

/// Starts one of the two balcony scene variants, then ends on its next dispatch.
///
/// Initial state is zero. A zero first spawn argument selects stream 59, nonzero
/// stream 60; both scripts borrow this loaded package's data and hide/restore
/// the HUD. The task does not wait for playback; any nonzero state kills it.
static void _actor335800StartSceneVariantTask(Task* task)
{
    enum { ACTOR_335800_SCENE_VARIANT_START = 0 };
    if (task->state == ACTOR_335800_SCENE_VARIANT_START) {
        if (task->spawnArg1.value != 0) {
            evsStartScript(gActor335800BurnerDefeatSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        } else {
            evsStartScript(gActor335800BurnerRetreatSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        task->state += 1;
        return;
    }
    taskKill(task);
}

/// Queues the night-balcony room's two-movie sequence from the scene script.
///
/// Requires that room overlay and its movie/display resources to remain loaded.
/// Spawns the bodyless startup task without waiting or saving its handle; it
/// hands presentation to the room's movie controller. Allocation failure is ignored.
static void _actor335800StartBalconyMovies(void)
{
    enum { ACTOR_335800_BALCONY_MOVIES_START_TASK = 0 };
    taskSpawnFromTable(gDryfieldNightMotelBalconyMovieTaskDescs, ACTOR_335800_BALCONY_MOVIES_START_TASK, 0, NULL);
}

/// Enables display output at the scene's presentation or skip-recovery boundary.
static void _actor335800EnableDisplay(void)
{
    SetDispMask(true);
}

/// Selects the signed-byte scene event in the live save for stage music selection.
///
/// The scene passes event 7. Stage music reads this value when choosing its
/// music-table column; this callback does not issue a sound request itself.
static void _actor335800SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Requests that the selected stage sequence stop over the low 16 bits of `fadeTicks`.
///
/// Duration counts audio updates and is rounded down to a multiple of four by
/// sound dispatch. Queue admission and playback matching follow `sndEvtRequestMidiStop`;
/// its result is ignored. The scene passes 1, producing an immediate stop.
static void _actor335800StopStageMusic(s32 fadeTicks)
{
    enum { ACTOR_335800_MUSIC_FADE_TICKS_MASK = 0xFFFF };
    sndEvtRequestMidiStop(gStageCountdownMusicEntry, fadeTicks & ACTOR_335800_MUSIC_FADE_TICKS_MASK);
}

/// Selects map objective code 24 after the balcony scene changes rooms.
static void _actor335800SetPostSceneObjective(void)
{
    enum { ACTOR_335800_POST_SCENE_OBJECTIVE = 24 };
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_335800_POST_SCENE_OBJECTIVE);
}

/// Locks attachment commands for the scene and requests cancellation of room effects.
///
/// Preserves other attachment flags. Effect cancellation is deferred to room
/// effect processing. Normal attachment/HUD updates later clear the lock.
static void _actor335800LockAttachmentsAndCancelEffects(void)
{
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

/// Sets whether the stage ambient sound is suppressed (1 muted, 0 enabled).
///
/// The event scripts pass only 0 or 1. The flag nibble is polled by stage music;
/// this callback does not issue a sound request itself.
static void _actor335800SetStageAmbientMuted(s32 muted)
{
    gameFlagSetNibble(GAME_FLAG_STAGE_AMBIENT_MUTED, muted);
}

/// Waits for battle reset, applies the player's package-selected clip and ends.
///
/// Requires a live player and the package's animation bank and request.
/// Retrieves the player slot before writing the weapon-animation bank index,
/// then synchronously plays clip 51 in that bank. A missing player is not tested;
/// the reset flag remains set. The task waits indefinitely while it is clear.
static void _actor335800RestorePlayerAnimationOnBattleResetTask(Task* task)
{
    Task* playerTask;

    if (gGameSession->battleResetPending != 0) {
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        playerActorWriteWeaponAnimationBankIndex(&_gActor335800PlayerSceneAnimation51.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_PLAY, &_gActor335800PlayerSceneAnimation51, 0);
        taskKill(task);
    }
}

/// Spawns vibration-driven scene screen shake; allocation failure is ignored.
static void _actor335800SpawnSceneScreenShake(void)
{
    enum { ACTOR_335800_SCENE_SCREEN_SHAKE_TASK = 3 };
    taskSpawnFromTable(_gActor335800SceneTasks, ACTOR_335800_SCENE_SCREEN_SHAKE_TASK, 0, 0);
}

/// Samples a one-pixel shake from vibration task activity and the tick's parity.
///
/// Borrows the task's signed halfword tick counter without changing it. The
/// binary motor's activity gives +1 on odd ticks; the variable motor's activity
/// gives -1 on even ticks, overriding zero there. Other samples are zero.
static inline s8 _actor335800SampleSceneShake(const Task* task, u8 motorActivity)
{
    s8  offsetY;
    s32 tick;

    offsetY = 0;
    if (motorActivity & GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE) {
        // Widen the unsigned halfword before narrowing its parity to a byte.
        tick    = (u16)task->killCountdown;
        offsetY = tick & 1;
    }
    if ((motorActivity & GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE) && !(task->killCountdown & 1)) {
        offsetY = -1;
    }
    return offsetY;
}

/// Shakes the screen during an unskipped event, then clears the offset and ends.
///
/// Requires a bodyless task; `killCountdown` is a halfword tick counter here,
/// initially zero on spawn and incremented modulo 65536 per active dispatch. Offsets
/// use pixels: binary-motor activity produces 0/+1, variable-motor activity
/// -1/0, both -1/+1. These are task activity bits, not delivered motor strength.
/// An idle or skipped event clears the persistent display offset before teardown.
static void _actor335800SceneScreenShakeTask(Task* task)
{
    s8 offsetY;
    u8 motorActivity;

    if ((gGameSession->eventState != 0) && (gGameSession->evtSkipped == 0)) {
        motorActivity = gGameSession->padScriptFlags;
        offsetY       = _actor335800SampleSceneShake(task, motorActivity);
        displaySetShakeY(offsetY);
        task->killCountdown += 1;
        return;
    }
    displaySetShakeY(0);
    taskKill(task);
}

/// Applies a placement's texture-page and CLUT offsets to an attached part model.
///
/// Borrows the live model and placement for the call. Signed page offsets count
/// 64-word VRAM columns; CLUT offsets count rows. Stores them even without a
/// primitive buffer. With a buffer, rebuilds both writable halves and preserves
/// nextBufferHalf; scratch space and GPU lifetime must satisfy `tmdBuildBufferHalf`.
/// Allocates nothing and retains no placement pointer.
static inline void _actorRenderApplyPartPlacementTextures(TmdObject* partModel, const AreaPlacement* placement)
{
    partModel->texturePageOffset = placement->texturePageOffset;
    partModel->clutRowOffset     = placement->clutRowOffset;
    if (partModel->buffer != NULL) {
        tmdBuildBufferHalf(partModel);
        tmdBuildBufferHalf(partModel);
    }
}

/// Initializes Gary Douglas's body work, attached head/gun and script interface.
///
/// Requires a twenty-part TMD task in state 0 with its live Enemy in spawnArg2.
/// Owns zeroed work for the task lifetime; allocation failure exits the enemy.
/// Successful children attach at body parts 4 and 8 and receive texture-page
/// and CLUT-row offsets from the enemy's current area placement; both existing
/// primitive-buffer halves are rebuilt. Failed child spawns leave NULL handles,
/// which the draw-mode handler does not check. Binds work-owned lighting, installs
/// messages/exit and advances state. The model and children borrow the matrices.
static void _actor335800GaryDouglasInit(Task* task)
{
    enum { ACTOR_335800_DOUGLAS_HEAD_TASK         = 1,
           ACTOR_335800_DOUGLAS_HEAD_PART         = 4,
           ACTOR_335800_DOUGLAS_GUN_TASK          = 2,
           ACTOR_335800_DOUGLAS_GUN_PART          = 8,
           ACTOR_335800_DOUGLAS_NO_BUFFER_RELEASE = -1 };

    _Actor335800GaryDouglasWork* work;
    GameLocationKey              location;
    GameLocationKey*             sessionLocation;
    GameLocationKey*             secondSessionLocation;
    Task*                        partTask;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_335800_DOUGLAS_NO_BUFFER_RELEASE;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    // Attached head and gun inherit this enemy placement's texture offsets.
    // Resolve each attached model's texture offsets from this enemy's area placement.
    partTask = taskSpawnFromTable(gActor335800GaryDouglasTasks, ACTOR_335800_DOUGLAS_HEAD_TASK, ACTOR_335800_DOUGLAS_HEAD_PART, task);
    if (partTask != NULL) {
        TmdObject*     partModel;
        AreaVariant*   areaVariant;
        AreaPlacement* placement;
        s32            placementIndex;

        work->headTask     = partTask;
        partModel          = partTask->extra.tmd;
        placementIndex     = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionLocation    = &gGameSession->location;
        location.stage     = sessionLocation->stage;
        location.area      = sessionLocation->area;
        location.roomState = sessionLocation->roomState;
        location.view      = sessionLocation->view;
        areaSyncLocationVariant(&location);
        areaVariant = areaGetVariant(&location);
        placement   = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
        _actorRenderApplyPartPlacementTextures(partModel, placement);
    }
    partTask = taskSpawnFromTable(gActor335800GaryDouglasTasks, ACTOR_335800_DOUGLAS_GUN_TASK, ACTOR_335800_DOUGLAS_GUN_PART, task);
    if (partTask != NULL) {
        TmdObject*     partModel;
        AreaVariant*   areaVariant;
        AreaPlacement* placement;
        s32            placementIndex;

        work->gunTask      = partTask;
        partModel          = partTask->extra.tmd;
        placementIndex     = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionLocation    = (secondSessionLocation = &gGameSession->location);
        location.stage     = sessionLocation->stage;
        location.area      = sessionLocation->area;
        location.roomState = secondSessionLocation->roomState;
        location.view      = gGameSession->location.view;
        areaSyncLocationVariant(&location);
        areaVariant = areaGetVariant(&location);
        placement   = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
        _actorRenderApplyPartPlacementTextures(partModel, placement);
    }
    _actor335800GaryDouglasBindLighting(task);
    task->msgTable     = _gActor335800GaryDouglasMessages;
    task->exitCallback = _actor335800GaryDouglasExit;
    task->state       += 1;
}

/// Applies signed 16.16 root-parent velocity and retains unsigned XYZ fractions.
///
/// Requires live writable Douglas work and root coordinate. Applies signed high
/// halves in X/Y/Z order, zero-extends the retained low halves and dirties
/// composition even at rest. Velocity already uses the root parent's frame.
static inline void _actor335800IntegrateWalkVelocity(_Actor335800GaryDouglasWork* work, GfxCoord* rootCoord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    rootCoord->coord.t[0]    += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]    += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]    += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Halves both Douglas lighting matrices and records the completed dim request.
///
/// Invoke as a statement inside a braced block in dim-requested state. dimWork
/// is a side-effect-free live work pointer; matrixRow/matrixColumn are writable
/// integer locals used repeatedly and left at 3. Signed coefficients and
/// translations use arithmetic shifts. Captures the package's dimmed-state constant.
#define ACTOR_335800_DOUGLAS_DIM_LIGHTING(dimWork, matrixRow, matrixColumn)                                         \
    for ((matrixRow) = 0; (matrixRow) < (s32)ARRAY_SIZE((dimWork)->model.color.m); (matrixRow)++) {                 \
        for ((matrixColumn) = 0; (matrixColumn) < (s32)ARRAY_SIZE((dimWork)->model.color.m[0]); (matrixColumn)++) { \
            (dimWork)->model.color.m[(matrixRow)][(matrixColumn)] >>= 1;                                            \
            (dimWork)->model.light.m[(matrixRow)][(matrixColumn)] >>= 1;                                            \
        }                                                                                                           \
        (dimWork)->model.color.t[(matrixRow)] >>= 1;                                                                \
        (dimWork)->model.light.t[(matrixRow)] >>= 1;                                                                \
    }                                                                                                               \
    (dimWork)->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_DIMMED;

/// Updates Douglas's scripted walk, visible animation, lighting and buffer release.
///
/// Requires initialized body work, walk.motion 0/1 and a live twenty-part model.
/// XYZ velocity uses signed 16.16 root-parent units with unsigned fractions kept.
/// Visibility gates slots 1..19, cue history, dim requests and ground shadows;
/// the muzzle flash fires once on cue 2's falling edge at gun part 8 with buckshot
/// argument 13. A visible dim request halves both 3x3 light/color matrices and
/// translations once and suppresses the shadow. viewReady restores full lighting
/// after that phase. A nonnegative countdown frees buffers on the tick reading
/// zero and then settles at -1. Hidden motion and delayed frees continue.
static void _actor335800GaryDouglasUpdate(Task* task)
{
    enum { ACTOR_335800_DOUGLAS_GUN_PART         = 8,
           ACTOR_335800_DOUGLAS_BUCKSHOT_ARG     = 13,
           ACTOR_335800_DOUGLAS_SHADOW_HALF_SIZE = 768,
           ACTOR_335800_DOUGLAS_LIGHT_COUNT      = 3 };
    TmdObject*                   model             = task->extra.tmd;
    _Actor335800GaryDouglasWork* work              = task->work;
    TaskFunc                     motionHandlers[2] = { _actor335800GaryDouglasIdle, _actor335800GaryDouglasRunWalkStep };
    VECTOR3                      groundPoint;
    GfxCoord*                    rootCoord;
    const AnimationRecord*       record;
    s32                          loopIndex;
    s32                          columnIndex;

    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    _actor335800IntegrateWalkVelocity(work, rootCoord);
    // Hidden bodies still move; visible bodies tick poses and detect cue-2's falling edge.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (work->model.ticking != 0) {
            for (loopIndex = 1; loopIndex < (s32)ARRAY_SIZE(work->rig.slots); loopIndex++) {
                animationTickSlot(&work->rig.anim, loopIndex);
            }
            record = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (record != NULL) {
                if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                    effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, &task->extra.tmd->coords[ACTOR_335800_DOUGLAS_GUN_PART], ACTOR_335800_DOUGLAS_BUCKSHOT_ARG, NULL);
                }
                work->prevCueFlags = record->flags & ANIMATION_RECORD_CUE_MASK;
            }
        }
        if (work->lightState == ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED) {
            ACTOR_335800_DOUGLAS_DIM_LIGHTING(work, loopIndex, columnIndex);
        }
        if (work->lightState >= ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL) {
            if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
                effectDrawGroundShadow(&groundPoint, ACTOR_335800_DOUGLAS_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
        }
    }
    // New view lighting restores full strength after any visible-frame dim request.
    if (gGameSession->viewReady != 0) {
        work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ACTOR_335800_DOUGLAS_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

#undef ACTOR_335800_DOUGLAS_DIM_LIGHTING

#include "../../shared/actor/actor_motion_arrive.inc.c"

#include "../../shared/actor/actor_motion_start.inc.c"

/// Dispatches Gary Douglas's attached head-and-hat and gun model tasks.
///
/// State must be 0..2: attach to the spawning body's selected part, retain that
/// attachment, or kill the task. Setup borrows the parent's coordinates and
/// lighting and joins its teardown tree; the parent must outlive the model.
static void _actor335800GaryDouglasPartTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor335800GaryDouglasPartStates;
    handlers.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Retains an attached part's parent-driven pose without a per-frame update.
static void _actor335800GaryDouglasPartIdle(Task* task)
{
}

/// Dispatches Gary Douglas's body initialization, update or enemy teardown.
///
/// Requires a live TMD task and state 0..2. Only the running actor-control value
/// dispatches; other values hold all three states. The copied table is indexed
/// without checking bounds, and teardown may release the body and its children.
static void _actor335800GaryDouglasTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor335800GaryDouglasStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Releases Douglas's enemy record and tears down his body and attached models.
///
/// Requires his live enemy in the second spawn argument. Work and child tasks
/// are released by teardown; model storage may remain until deferred collection.
static void _actor335800GaryDouglasExit(Task* task)
{
    enemyTaskExit(task);
}

/// Binds Douglas's work-owned lighting matrices and samples the room's lights.
///
/// Requires initialized work and a live twenty-part model whose part 1 has a
/// composed world position. Samples up to three selected lights there and records
/// full-lighting mode. The model and its attached parts borrow the matrices until
/// task teardown; this does not compose coordinates or allocate light storage.
static void _actor335800GaryDouglasBindLighting(Task* task)
{
    enum { ACTOR_335800_GARY_DOUGLAS_LIGHT_COUNT = 3 };
    TmdObject*                   model;
    _Actor335800GaryDouglasWork* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
    worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ACTOR_335800_GARY_DOUGLAS_LIGHT_COUNT);
    work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL;
}

/// Retains Douglas's idle motion state without changing velocity or animation.
static void _actor335800GaryDouglasIdle(Task* task)
{
}

/// Runs the current step of Douglas's scripted walk.
///
/// Requires initialized work with `walk.motionStep` in 0..3: face the target,
/// begin approach, detect arrival, or turn to the destination yaw. The selected
/// step uses a live model root; the table is indexed without a bounds check.
static void _actor335800GaryDouglasRunWalkStep(Task* task)
{
    _Actor335800GaryDouglasWork* work;
    TaskFuncTable4               handlers;

    work     = task->work;
    handlers = _gActor335800GaryDouglasWalkSteps;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor/actor_motion_face.inc.c"

/// Starts Douglas's forward approach and primes the next arrival test.
///
/// Requires initialized work and a live root already facing the destination.
/// Rotates a signed 16.16 forward velocity into the root parent's frame: 32
/// coordinate units per update for a unit-scale root. Seeds the previous gaps
/// with `ACTOR_MOTION_INITIAL_ARRIVAL_GAP` and advances to arrival checking, without
/// changing translation or fractional carry.
static void _actor335800GaryDouglasBeginApproach(Task* task)
{
    _Actor335800GaryDouglasWork* work;
    GfxCoord*                    rootCoord;
    VECTOR                       forwardVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    // Rotate velocity without adding the root's translation.
    forwardVelocity = _gActor335800GaryDouglasForwardVelocity;
    ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

#include "../../shared/actor/actor_motion_turn.inc.c"

#include "../../shared/actor/actor_motion_play.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets Douglas's body visibility and buffer policy, copying all flags to his parts.
///
/// Requires initialized work, a live body and both live head/gun tasks; missing
/// parts are dereferenced even for an invalid mode. Modes 0/1 hide/show with
/// automatic buffer recovery; 1 also allocates the body's buffer if absent.
/// Modes 2/3 hide/show without automatic recovery; 2 schedules body-buffer
/// release on the third update after the request. Other modes do not cancel a
/// pending release. Allocation failure is ignored and parts retain their buffers.
/// Ignores message ID and fourth argument; returns 0 for modes 0..3, else 1.
static s32 _actor335800GaryDouglasSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    enum {
        ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE          = 0,
        ACTOR_335800_GARY_DOUGLAS_DRAW_SHOW_ALLOCATE = 1,
        ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE_RELEASE  = 2,
    };
    _Actor335800GaryDouglasWork* work;
    TmdObject*                   bodyModel;
    TmdObject*                   headModel;
    TmdObject*                   gunModel;
    s32                          result;

    work      = task->work;
    bodyModel = task->extra.tmd;
    headModel = work->headTask->extra.tmd;
    gunModel  = work->gunTask->extra.tmd;
    result    = 0;
    switch (mode) {
        case ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_335800_GARY_DOUGLAS_DRAW_SHOW_ALLOCATE:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE_RELEASE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            // The mode value also supplies the two-tick release countdown.
            work->freeCountdown = mode;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    // Parts inherit the complete body flags even when the mode is unrecognized.
    headModel->flags = bodyModel->flags;
    gunModel->flags  = bodyModel->flags;
    return result;
}

/// Requests half-strength lighting for Douglas when actor command 0 is received.
///
/// Borrows a readable request and initialized work. The next visible update
/// halves the shared lighting matrices and suppresses the ground shadow until
/// a view relights the model. Repeating command 0 requests another halving.
/// Ignores the command's context, other command values, message ID and fourth
/// argument; acknowledges every request with 0.
static s32 _actor335800GaryDouglasRequestDim(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg)
{
    _Actor335800GaryDouglasWork* work;

    work = task->work;
    if (request->command == ACTOR_335800_GARY_DOUGLAS_COMMAND_DIM) {
        work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED;
    }
    return 0;
}

/// Applies Flint's signed 16.16 XYZ velocity and retains the unsigned fractions.
///
/// Velocity uses the root parent's coordinate units. Borrows initialized work
/// and a live writable root. Composition is marked dirty even while stationary;
/// integer halves are signed, fractions zero-extended.
static inline void _actor335800FlintIntegrateVelocity(_Actor335800FlintWork* work, GfxCoord* coord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    coord->coord.t[0]        += work->walk.carry[0].halves.integer;
    coord->coord.t[1]        += work->walk.carry[1].halves.integer;
    coord->coord.t[2]        += work->walk.carry[2].halves.integer;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Advances the nineteen-part rig's driven slots once when playback is enabled.
///
/// Work-owned rig bindings and their borrowed model coordinates and animation
/// data must remain live. Slot 0 is not driven; slots 1..18 advance in order.
static inline void _actor335800FlintTickAnimation(_Actor335800FlintWork* work)
{
    enum { ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    if (work->model.ticking != 0) {
        for (slotIndex = ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Updates Flint's scripted walk, animation, visible lighting and delayed buffer release.
///
/// Requires initialized work and a live nineteen-part TMD body. Motion is 0
/// (idle) or 1 (walking), indexing two handlers without a bounds check. Movement
/// and slots 1..18 continue while hidden. The shadow samples part 1's previous
/// composed position before lighting recomposes it. A nonnegative release
/// countdown frees buffers on the tick that finds zero, then becomes inactive at -1.
static void _actor335800FlintUpdate(Task* task)
{
    enum {
        ACTOR_335800_FLINT_SHADOW_HALF_SIZE = 512,
        ACTOR_335800_FLINT_LIGHT_COUNT      = 3
    };
    TmdObject*             model             = task->extra.tmd;
    _Actor335800FlintWork* work              = task->work;
    TaskFunc               motionHandlers[2] = { _actor335800FlintIdle, _actor335800FlintRunWalkStep };
    VECTOR3                groundPoint;
    GfxCoord*              coord;

    motionHandlers[work->walk.motion](task);
    coord = task->extra.tmd->coords;
    _actor335800FlintIntegrateVelocity(work, coord);
    _actor335800FlintTickAnimation(work);
    // Hidden Flint still moves and animates; only shadow and lighting are gated.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_335800_FLINT_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ACTOR_335800_FLINT_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor/actor_motion_arrive19.inc.c"
#undef ACTOR_MOTION_PLAY19_HANDLER

/// Binds Flint's playback storage and starts the requested clip, including repeats.
///
/// Requires initialized work and a live nineteen-part model. Set `work->model.bank`
/// to `ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request so it binds the rig;
/// later requests retain the bindings while the bank agrees. Work-owned slots
/// and word-aligned poses, model coordinates, and the loaded clip table/data
/// must remain live during playback. The request is read only through the
/// call and must not overlap playback state.
///
/// Bank 0 and clip 1 select the embedded table's only loaded clip; bank and
/// clip are stored as signed bytes before indexing. Drives slots 1..18, leaving
/// slot 0 untouched. Nonzero blend on an already ticking rig captures each
/// advancing slot's pose and seeks its track start for `blendFrames` whole
/// normal-rate frames (0..2047 keeps remaining time nonnegative). Otherwise
/// resets each track at normal rate. Both paths tick the slots once afterward
/// and enable subsequent frame ticking. Ignores `enableWorldCollision`.
static inline void _actor335800FlintApplyAnimationRequest(_Actor335800FlintWork* work, TmdObject* model, const AnimationPlayRequest* request)
{
    enum { ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    // Rebind the borrowed playback storage when the selected bank changes.
    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, _gActor335800FlintAnimationBanks[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = request->animationId;
    // A blend captures an advancing pose; a reset replaces the track state.
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    // Apply the new pose before the ordinary frame tick takes over.
    for (slotIndex = ACTOR_335800_FLINT_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = true;
}

/// Starts Flint's scripted walk toward a borrowed destination and closing yaw.
///
/// Requires initialized work and a live TMD model. Copies XYZ position in the
/// root parent's coordinate frame and rotation in 4096 units per turn, then
/// starts the approach clip in bank 0 with a five-frame blend. Optional clips
/// supply approach and arrival IDs; NULL retains the binary's clip-13/clip-1
/// defaults, whose approach ID is outside the embedded bank's proven extent.
/// Neither payload is retained. Message ID is ignored; returns 0.
static s32 _actor335800FlintStartWalk(Task* task, s32 messageId, const ActorTransform* destination, const ActorMotionWalkAnim* clips)
{
    enum {
        ACTOR_335800_FLINT_DEFAULT_APPROACH_CLIP = 13,
        ACTOR_335800_FLINT_DEFAULT_ARRIVAL_CLIP  = 1,
        ACTOR_335800_FLINT_WALK_BLEND_FRAMES     = 5
    };
    _Actor335800FlintWork* work;
    AnimationPlayRequest   preset;

    work                    = task->work;
    work->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    work->walk.motionStep   = ACTOR_335800_FLINT_FACE_TARGET_STEP;
    work->walk.target.vx    = destination->pos.vx;
    work->walk.target.vy    = destination->pos.vy;
    work->walk.target.vz    = destination->pos.vz;
    work->walk.targetRot.vx = destination->rot.vx;
    work->walk.targetRot.vy = destination->rot.vy;
    work->walk.targetRot.vz = destination->rot.vz;
    preset.source.index     = ACTOR_335800_FLINT_ANIMATION_BANK;
    if (clips != NULL) {
        preset.animationId     = clips->animationId;
        work->model.nextAnimId = clips->nextAnimId;
    } else {
        preset.animationId     = ACTOR_335800_FLINT_DEFAULT_APPROACH_CLIP;
        work->model.nextAnimId = ACTOR_335800_FLINT_DEFAULT_ARRIVAL_CLIP;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = ACTOR_335800_FLINT_WALK_BLEND_FRAMES;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    _actor335800FlintApplyAnimationRequest(task->work, task->extra.tmd, &preset);
    return 0;
}

/// Dispatches Flint's spawn, frame-update or teardown state while actors are running.
///
/// Requires a live TMD task with state 0 (initialize), 1 (update) or 2 (exit).
/// States 1 and 2 require initialized task-owned work. Frozen actor control
/// skips every state, including initialization and teardown; indexing is unchecked.
static void _actor335800FlintTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor335800FlintStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

/// Allocates and initializes Flint's work, lighting bindings and message handlers.
///
/// Called in spawn state 0 with a live TMD model. The zeroed work is owned by
/// the task until teardown; an unbound bank forces the first play request to
/// bind its storage. Allocation failure ends the task. Success binds the
/// work-owned matrices, installs teardown and advances to ticking state 1.
static void _actor335800FlintInit(Task* task)
{
    enum { ACTOR_335800_FLINT_NO_BUFFER_RELEASE = -1 };
    _Actor335800FlintWork* work;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_335800_FLINT_NO_BUFFER_RELEASE;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _actor335800FlintBindLighting(task);

    task->msgTable     = _gActor335800FlintMessages;
    task->exitCallback = _actor335800FlintExit;
    task->state       += 1;
}

/// Releases Flint's task-owned work and model resources, then ends the task.
///
/// Installed as both teardown state 2 and the exit callback. Owned resources
/// must be live at entry and must not be used after this call.
static void _actor335800FlintExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends Flint's work-owned light and colour matrices to the live TMD model.
///
/// Does not compute lighting. The work must remain allocated while the model
/// uses these pointers; the frame tick fills the matrices from room lighting.
static void _actor335800FlintBindLighting(Task* task)
{
    TmdObject*             model;
    _Actor335800FlintWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves idle Flint motion unchanged while the frame tick maintains his model.
static void _actor335800FlintIdle(Task* task)
{
}

/// Runs the current step of Flint's scripted walk.
///
/// Requires initialized task-owned work and a live TMD root. `walk.motionStep`
/// must be 0 (face target), 1 (begin approach), 2 (check arrival), or 3 (turn to
/// closing yaw); indexing is unchecked. The frame update dispatches this only
/// while `walk.motion` is `ACTOR_WALK_MOTION_WALKING`.
static void _actor335800FlintRunWalkStep(Task* task)
{
    TaskFuncTable4         handlers;
    _Actor335800FlintWork* work;

    work     = task->work;
    handlers = _gActor335800FlintWalkSteps;
    handlers.funcs[work->walk.motionStep](task);
}

/// Faces Flint's root toward the latched walk destination and advances the step.
///
/// Requires initialized work and a live model root. Destination and root
/// translation share the parent's coordinate frame. The normalized offset's
/// yaw uses 4096 units per turn; pitch and roll become zero and prior scale is
/// replaced by the yaw rotation. Translation is retained, composition invalidated.
static void _actor335800FlintFaceTarget(Task* task)
{
    _Actor335800FlintWork* work;
    GfxCoord*              coord;
    VECTOR                 delta;
    SVECTOR                direction;
    SVECTOR                rotation;

    work  = task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &direction);

    rotation.vx = 0;
    rotation.vy = ratan2(direction.vx, direction.vz);
    rotation.vz = 0;

    coord->param.rot.vx = rotation.vx;
    coord->param.rot.vy = rotation.vy;
    coord->param.rot.vz = rotation.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}

/// Starts Flint's forward approach and seeds the first arrival measurement.
///
/// Requires initialized work and a root already facing the target. Rotates
/// local +Z speed 32 units per tick into the root parent's frame, retaining it
/// in signed 16.16 velocity. Seeds all distance components with the no-distance
/// sentinel and advances to the arrival step; existing fractional carry remains.
static void _actor335800FlintBeginApproach(Task* task)
{
    _Actor335800FlintWork* work;
    GfxCoord*              coord;
    VECTOR                 localVelocity;

    coord = task->extra.tmd->coords;
    work  = task->work;

    localVelocity = _gActor335800FlintForwardVelocity;
    ApplyMatrixLV(&coord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

/// Turns Flint to the walk's closing yaw, then restarts the arrival clip and idles.
///
/// Requires initialized work and a live root. Turns at most 64 angle units
/// per tick (4096 per turn), using the signed low-halfword yaw difference.
/// Within one step it snaps to the target and blends bank 0's queued arrival
/// clip for five whole frames. Retains extracted pitch/roll and translation,
/// replaces scale and invalidates composition; stored Euler parameters stay intact.
static void _actor335800FlintTurnToYaw(Task* task)
{
    _Actor335800FlintWork* work;
    GfxCoord*              coord;
    SVECTOR                rotation;
    AnimationPlayRequest   preset;
    s32                    currentYaw;
    s16                    yawDifference;
    enum {
        ACTOR_335800_FLINT_TURN_STEP            = 64,
        ACTOR_335800_FLINT_ARRIVAL_BLEND_FRAMES = 5
    };

    coord = task->extra.tmd->coords;
    work  = task->work;

    gfxExtractSmallestEuler(&rotation, &coord->coord);
    yawDifference = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawDifference) >= ACTOR_335800_FLINT_TURN_STEP + 1) {
        currentYaw = rotation.vy;
        if (yawDifference < 0) {
            rotation.vy = currentYaw - ACTOR_335800_FLINT_TURN_STEP;
        } else {
            rotation.vy = currentYaw + ACTOR_335800_FLINT_TURN_STEP;
        }
    } else {
        rotation.vy                 = work->walk.targetRot.vy;
        preset.source.index         = ACTOR_335800_FLINT_ANIMATION_BANK;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = ACTOR_335800_FLINT_ARRIVAL_BLEND_FRAMES;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor335800FlintPlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = ACTOR_335800_FLINT_FACE_TARGET_STEP;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrix(&rotation, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Restarts Flint's requested clip, even when it is already playing.
///
/// Requires initialized work, a live nineteen-part TMD model and a readable
/// request through the call. Bank 0/clip 1 select the embedded table's only
/// loaded clip. Drives slots 1..18 and enables later ticking, blending for
/// `blendFrames` whole frames when requested and already ticking, resetting
/// otherwise. Clip and bank IDs are narrowed to signed bytes before use.
/// Message ID, collision choice and fourth argument are ignored; returns 0.
static s32 _actor335800FlintPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor335800FlintWork* work;
    TmdObject*             model;

    work  = task->work;
    model = task->extra.tmd;
    _actor335800FlintApplyAnimationRequest(work, model, request);
    return 0;
}

/// Selects the private placement handler for the Flint model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Sets Flint's visibility and automatic primitive-buffer recovery mode.
///
/// Modes 0/1 hide/show with automatic recovery enabled; 1 also allocates the
/// buffer. Modes 2/3 hide/show with recovery disabled; 2 schedules release on
/// the third frame tick after the request. Requires a live model and, for
/// mode 2, initialized work. Other modes leave pending release unchanged.
/// Ignores message ID and fourth argument; returns 0 for modes 0..3, else 1.
static s32 _actor335800FlintSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    enum {
        ACTOR_335800_FLINT_DRAW_HIDE          = 0,
        ACTOR_335800_FLINT_DRAW_SHOW_ALLOCATE = 1,
        ACTOR_335800_FLINT_DRAW_HIDE_RELEASE  = 2,
    };
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_335800_FLINT_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_335800_FLINT_DRAW_SHOW_ALLOCATE:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_335800_FLINT_DRAW_HIDE_RELEASE: {
            _Actor335800FlintWork* work;

            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        }
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Acknowledges any actor command to Flint without changing the task.
///
/// All arguments, including the command payload word, are ignored; returns 0.
static s32 _actor335800FlintIgnoreCommand(Task* task, s32 messageId, s32 unusedCommand, s32 unusedArg)
{
    return 0;
}
