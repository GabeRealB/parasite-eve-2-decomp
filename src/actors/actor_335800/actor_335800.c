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
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

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

extern ActorTransform D_actor_335800_80164F80;

extern TaskDesc             D_actor_335800_80164DE0[];
extern AnimationPlayRequest D_actor_335800_80164E7C;

/// The warp-payload table the two dispatchers reach by entry:
/// `_actor335800PlacePlayerAfterScene` selects an entry of it by index.
extern ActorTransform D_actor_335800_80164EA4[5];
extern EvsCommand     D_actor_335800_80165FC0[];
extern EvsCommand     D_actor_335800_80166098[];

/// Animation bank tables of the parent and the child block.
extern AnimationSet*  D_actor_335800_8016EAC4[5];
extern AnimationSet** gActorMotionAnimBanks[1];

/// The two part tasks the parent block spawns, and its message table; both
/// live in this overlay's trailing data.
extern TaskDesc D_actor_335800_8016EADC[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_335800_8016EB00[];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `_actor335800FlintInit`; terminator id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_actor_335800_80172EA8[];

static void _modelPlacementAttachPartTask(Task* childTask);
static void func_actor_335800_80162640(Task* arg0);
static void func_actor_335800_80162844(Task* task);
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

/// Spawn, tick and teardown handlers of the two part tasks the parent block
/// spawns, dispatched by `_actor335800GaryDouglasPartTask`.
static const TaskFuncTable3 D_actor_335800_80161E24 = { {
    _modelPlacementAttachPartTask,
    _actor335800GaryDouglasPartIdle,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent block, dispatched by
/// `func_actor_335800_80162F10`.
static const TaskFuncTable3 D_actor_335800_80161E30 = { {
    func_actor_335800_80162640,
    func_actor_335800_80162844,
    _actor335800GaryDouglasExit,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_335800_80161E3C = { {
    _actorMotionFaceTarget,
    _actor335800GaryDouglasBeginApproach,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

/// The constant local-space offset `_actor335800GaryDouglasBeginApproach` rotates for
/// the parent block: straight ahead along the part's own +Z.
static const VECTOR D_actor_335800_80161E4C = { 0, 0, 0x200000, 0 };

/// Spawn, tick and teardown handlers of the child block, dispatched by
/// `_actor335800FlintTask`.
static const TaskFuncTable3 D_actor_335800_80161E5C = { {
    _actor335800FlintInit,
    _actor335800FlintUpdate,
    _actor335800FlintExit,
} };

/// Step handlers of the child block's motion sequence, indexed by
/// `ActorWalkState::motionStep`, in the same order as the parent's.
static const TaskFuncTable4 D_actor_335800_80161E68 = { {
    _actor335800FlintFaceTarget,
    _actor335800FlintBeginApproach,
    _actorMotionArrive19,
    _actor335800FlintTurnToYaw,
} };

/// The child block's copy of the forward offset, rotated by
/// `_actor335800FlintBeginApproach`.
static const VECTOR D_actor_335800_80161E78 = { 0, 0, 0x200000, 0 };

static void _actor335800SceneGroundShadowTask(Task* task);
static void _actor335800StartSceneVariantTask(Task* task);
static void _actor335800RestorePlayerAnimationOnBattleResetTask(Task* task);
static void _actor335800SceneScreenShakeTask(Task* task);

extern ActorTransform           D_actor_335800_80164F80;
extern AnimationPlayRequest     D_actor_335800_80164E54;
extern AnimationPlayRequest     D_actor_335800_80164E90;
extern AnimationPlayRequest     D_actor_335800_80164F44;
extern AnimationPlayRequest     D_actor_335800_80164F58;
extern AnimationPlayRequest     D_actor_335800_80164F6C;
extern ActorCommand             D_actor_335800_80164FC8;
extern AnimationBankCopyRequest D_actor_335800_80164E24;
extern ActorTransform           D_actor_335800_80164EA4[5];
extern ActorTransform           D_actor_335800_80164F98;
extern ActorTransform           D_actor_335800_80164FB0;
static void                     _actor335800StageSceneAudioStart(void);
static void                     _actor335800StartScenePlayback(void);
static void                     _actor335800FinishStreamedScene(void);
static void                     _actor335800SpawnSceneGroundShadow(void);
void                            func_actor_335800_801623D8(void);
static void                     _actor335800EnableDisplay(void);
static void                     _actor335800SetStageAmbientMuted(s32 muted);
static void                     _actor335800SpawnSceneScreenShake(void);

extern AnimationPlayRequest     D_actor_335800_80164E40;
extern AnimationPlayRequest     D_actor_335800_80164E7C;
extern AnimationPlayRequest     D_actor_335800_80164E90;
extern ActorCommand             D_actor_335800_8016502C;
extern ActorCommand             D_actor_335800_80165038;
extern ActorCommand             D_actor_335800_8016503C;
extern ActorCommand             D_actor_335800_80165040;
extern ActorCommand             D_actor_335800_80165044;
extern AnimationBankCopyRequest D_actor_335800_80164E24;
extern EvsSceneKey              D_actor_335800_80165050;
extern EvsSceneKey              D_actor_335800_80165058;
extern ActorTransform           D_actor_335800_80164EA4[5];
static s32                      _actor335800GaryDouglasSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32                      _actor335800GaryDouglasRequestDim(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg);
static s32                      _actor335800FlintStartWalk(Task* task, s32 messageId, const ActorTransform* destination, const ActorMotionWalkAnim* clips);
static s32                      _actor335800FlintPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32                      _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32                      _actor335800FlintSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32                      _actor335800FlintIgnoreCommand(Task* task, s32 messageId, s32 unusedCommand, s32 unusedArg);
static void                     _actor335800CancelStreamedScene(void);
static void                     _actor335800SetSceneRoom(u8 room);
static void                     _actor335800PlacePlayerForScene(void);
static void                     _actor335800PlacePlayerAfterScene(s32 faceQuarterTurn);
static void                     _actor335800RestorePlayerView(void);
static void                     _actor335800SetSceneSpriteBatchesHidden(s32 hidden);
static void                     _actor335800SetSceneEvent(s8 sceneEvent);
static void                     _actor335800StopStageMusic(s32 fadeTicks);
static void                     _actor335800SetPostSceneObjective(void);
static void                     _actor335800LockAttachmentsAndCancelEffects(void);
static void                     _actor335800GaryDouglasPartTask(Task* task);
void                            func_actor_335800_80162F10(Task*);
static void                     _actor335800FlintTask(Task* task);

static AnimationPackedPose _gActor335800Animation0255CBank1[6] = {
#include "assets/actor_335800_animation_0255C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0255CBank4[46] = {
#include "assets/actor_335800_animation_0255C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0255CRecords[109] = {
#include "assets/actor_335800_animation_0255C_records.inc"
};

static u16 _gActor335800Animation0255CIndices[20] = {
#include "assets/actor_335800_animation_0255C_indices.inc"
};

static AnimationSet _gActor335800Animation0255C = {
    _gActor335800Animation0255CRecords,
    _gActor335800Animation0255CIndices,
    { NULL, _gActor335800Animation0255CBank1, NULL, NULL, _gActor335800Animation0255CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02B78Bank1[12] = {
#include "assets/actor_335800_animation_02B78_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02B78Bank4[146] = {
#include "assets/actor_335800_animation_02B78_bank4.inc"
};

static AnimationRecord _gActor335800Animation02B78Records[189] = {
#include "assets/actor_335800_animation_02B78_records.inc"
};

static u16 _gActor335800Animation02B78Indices[20] = {
#include "assets/actor_335800_animation_02B78_indices.inc"
};

static AnimationSet _gActor335800Animation02B78 = {
    _gActor335800Animation02B78Records,
    _gActor335800Animation02B78Indices,
    { NULL, _gActor335800Animation02B78Bank1, NULL, NULL, _gActor335800Animation02B78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02D8CBank1[2] = {
#include "assets/actor_335800_animation_02D8C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02D8CBank4[24] = {
#include "assets/actor_335800_animation_02D8C_bank4.inc"
};

static AnimationRecord _gActor335800Animation02D8CRecords[83] = {
#include "assets/actor_335800_animation_02D8C_records.inc"
};

static u16 _gActor335800Animation02D8CIndices[20] = {
#include "assets/actor_335800_animation_02D8C_indices.inc"
};

static AnimationSet _gActor335800Animation02D8C = {
    _gActor335800Animation02D8CRecords,
    _gActor335800Animation02D8CIndices,
    { NULL, _gActor335800Animation02D8CBank1, NULL, NULL, _gActor335800Animation02D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02F98Bank1[2] = {
#include "assets/actor_335800_animation_02F98_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02F98Bank4[23] = {
#include "assets/actor_335800_animation_02F98_bank4.inc"
};

static AnimationRecord _gActor335800Animation02F98Records[82] = {
#include "assets/actor_335800_animation_02F98_records.inc"
};

static u16 _gActor335800Animation02F98Indices[20] = {
#include "assets/actor_335800_animation_02F98_indices.inc"
};

static AnimationSet _gActor335800Animation02F98 = {
    _gActor335800Animation02F98Records,
    _gActor335800Animation02F98Indices,
    { NULL, _gActor335800Animation02F98Bank1, NULL, NULL, _gActor335800Animation02F98Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_335800_80164DE0[4] = {
    { { { TASK_BODY_COORD, 192 } }, _actor335800SceneGroundShadowTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor335800RestorePlayerAnimationOnBattleResetTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor335800StartSceneVariantTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor335800SceneScreenShakeTask, { .value = 0 } },
};

AnimationSet* D_actor_335800_80164E10[5] = {
    NULL,
    &_gActor335800Animation0255C,
    &_gActor335800Animation02B78,
    &_gActor335800Animation02D8C,
    &_gActor335800Animation02F98,
};

AnimationBankCopyRequest D_actor_335800_80164E24 = { { .sets = D_actor_335800_80164E10 }, ARRAY_SIZE(D_actor_335800_80164E10) };

AnimationPlayRequest D_actor_335800_80164E2C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E40 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E54 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E68 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E7C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E90 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_335800_80164EA4[5] = {
    { { -7180, -3200, 4310, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 2047, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 2047, 0, 0 } },
};

AnimationPlayRequest D_actor_335800_80164F1C[2] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_335800_80164F44 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F58 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F6C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80164F80 = { { -4000, 0, 1970, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_335800_80164F98 = { { -7730, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_335800_80164FB0 = { { -6260, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorCommand D_actor_335800_80164FC8 = { { .loc = { 3, 29 } }, 0 };

AnimationPlayRequest D_actor_335800_80164FCC = { 0 };

AnimationPlayRequest D_actor_335800_80164FE0 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164FF4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80165008 = { { -1287, 0, -2750, 0 }, { 0, -1650, 0, 0 } };

ActorCommand D_actor_335800_80165020 = { { .loc = { 3, 29 } }, 1 };

ActorCommand D_actor_335800_80165024 = { { .loc = { 3, 29 } }, 2 };

ActorCommand D_actor_335800_80165028 = { { .loc = { 3, 29 } }, 3 };

ActorCommand D_actor_335800_8016502C = { { .loc = { 3, 29 } }, 0xFFFF };

ActorCommand D_actor_335800_80165030 = { { .loc = { 3, 29 } }, 4 };

ActorCommand D_actor_335800_80165034 = { { .loc = { 3, 29 } }, 5 };

ActorCommand D_actor_335800_80165038 = { { .loc = { 3, 29 } }, 6 };

ActorCommand D_actor_335800_8016503C = { { .loc = { 3, 29 } }, 7 };

ActorCommand D_actor_335800_80165040 = { { .loc = { 3, 29 } }, 8 };

ActorCommand D_actor_335800_80165044 = { { .loc = { 3, 29 } }, 10 };

EvsSceneKey D_actor_335800_80165048 = { 3, 58, 11 };

EvsSceneKey D_actor_335800_80165050 = { 3, 59, 11 };

EvsSceneKey D_actor_335800_80165058 = { 3, 60, 11 };

EvsCommand D_actor_335800_80165060[72] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_801623D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800EnableDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneScreenShake }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = D_actor_335800_80164EA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 49 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165024 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80165008 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164F80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneGroundShadow }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165030 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80164FC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164F98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165720[5] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165798[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800CancelStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800LockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = D_actor_335800_80164EA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800EnableDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165960[21] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800LockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800PlacePlayerForScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SpawnSceneScreenShake }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetSceneSpriteBatchesHidden }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetSceneSpriteBatchesHidden }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor335800SetSceneRoom }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800SetPostSceneObjective }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165B58[26] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016503C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 270 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165040 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800RestorePlayerView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800PlacePlayerAfterScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor335800SetSceneEvent }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165DC8[21] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165044 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800PlacePlayerAfterScene }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x401F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800FinishStreamedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800SetStageAmbientMuted }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800RestorePlayerView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165FC0[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165050 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165B58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80166098[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor335800StopStageMusic }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165058 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor335800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165DC8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor335800GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor335800GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor335800GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor335800GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor335800GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor335800GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor335800GaryDouglasBodyPartVerts,
    _gActor335800GaryDouglasBodyVerts,
    _gActor335800GaryDouglasBodyNormals,
    _gActor335800GaryDouglasBodySkeleton,
    _gActor335800GaryDouglasBodyStream,
};

static TmdBone _gActor335800GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor335800GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor335800GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor335800GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor335800GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor335800GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor335800GaryDouglasHeadHatPartVerts,
    _gActor335800GaryDouglasHeadHatVerts,
    _gActor335800GaryDouglasHeadHatNormals,
    _gActor335800GaryDouglasHeadHatSkeleton,
    _gActor335800GaryDouglasHeadHatStream,
};

static TmdBone _gActor335800Actor120300Model082F8Skeleton[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

static u32 _gActor335800Actor120300Model082F8PartVerts[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

static SVECTOR _gActor335800Actor120300Model082F8Verts[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

static SVECTOR _gActor335800Actor120300Model082F8Normals[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

static u32 _gActor335800Actor120300Model082F8Stream[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

static TmdSource _gActor335800Actor120300Model082F8 = {
    0,
    1692,
    0,
    1,
    _gActor335800Actor120300Model082F8PartVerts,
    _gActor335800Actor120300Model082F8Verts,
    _gActor335800Actor120300Model082F8Normals,
    _gActor335800Actor120300Model082F8Skeleton,
    _gActor335800Actor120300Model082F8Stream,
};

static AnimationPackedPose _gActor335800Animation0AB18Bank1[2] = {
#include "assets/actor_335800_animation_0AB18_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0AB18Bank4[32] = {
#include "assets/actor_335800_animation_0AB18_bank4.inc"
};

static AnimationRecord _gActor335800Animation0AB18Records[101] = {
#include "assets/actor_335800_animation_0AB18_records.inc"
};

static u16 _gActor335800Animation0AB18Indices[20] = {
#include "assets/actor_335800_animation_0AB18_indices.inc"
};

static AnimationSet _gActor335800Animation0AB18 = {
    _gActor335800Animation0AB18Records,
    _gActor335800Animation0AB18Indices,
    { NULL, _gActor335800Animation0AB18Bank1, NULL, NULL, _gActor335800Animation0AB18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0B59CBank1[11] = {
#include "assets/actor_335800_animation_0B59C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0B59CBank4[255] = {
#include "assets/actor_335800_animation_0B59C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0B59CRecords[365] = {
#include "assets/actor_335800_animation_0B59C_records.inc"
};

static u16 _gActor335800Animation0B59CIndices[20] = {
#include "assets/actor_335800_animation_0B59C_indices.inc"
};

static AnimationSet _gActor335800Animation0B59C = {
    _gActor335800Animation0B59CRecords,
    _gActor335800Animation0B59CIndices,
    { NULL, _gActor335800Animation0B59CBank1, NULL, NULL, _gActor335800Animation0B59CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0CA48Bank1[34] = {
#include "assets/actor_335800_animation_0CA48_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0CA48Bank4[546] = {
#include "assets/actor_335800_animation_0CA48_bank4.inc"
};

static AnimationRecord _gActor335800Animation0CA48Records[655] = {
#include "assets/actor_335800_animation_0CA48_records.inc"
};

static u16 _gActor335800Animation0CA48Indices[20] = {
#include "assets/actor_335800_animation_0CA48_indices.inc"
};

static AnimationSet _gActor335800Animation0CA48 = {
    _gActor335800Animation0CA48Records,
    _gActor335800Animation0CA48Indices,
    { NULL, _gActor335800Animation0CA48Bank1, NULL, NULL, _gActor335800Animation0CA48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0CC7CBank1[4] = {
#include "assets/actor_335800_animation_0CC7C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0CC7CBank4[35] = {
#include "assets/actor_335800_animation_0CC7C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0CC7CRecords[74] = {
#include "assets/actor_335800_animation_0CC7C_records.inc"
};

static u16 _gActor335800Animation0CC7CIndices[20] = {
#include "assets/actor_335800_animation_0CC7C_indices.inc"
};

static AnimationSet _gActor335800Animation0CC7C = {
    _gActor335800Animation0CC7CRecords,
    _gActor335800Animation0CC7CIndices,
    { NULL, _gActor335800Animation0CC7CBank1, NULL, NULL, _gActor335800Animation0CC7CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_335800_8016EAC4[5] = {
    NULL,
    &_gActor335800Animation0AB18,
    &_gActor335800Animation0B59C,
    &_gActor335800Animation0CA48,
    &_gActor335800Animation0CC7C,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_335800_8016EAC4,
};

TaskDesc D_actor_335800_8016EADC[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_335800_80162F10, { .model = &_gActor335800GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor335800GaryDouglasPartTask, { .model = &_gActor335800GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, _actor335800GaryDouglasPartTask, { .model = &_gActor335800Actor120300Model082F8 } },
};

TaskMessageEntry D_actor_335800_8016EB00[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor335800GaryDouglasSetDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor335800GaryDouglasRequestDim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor335800FlintBodySkeleton[19] = {
#include "assets/flint_body_skeleton.inc"
};

static u32 _gActor335800FlintBodyPartVerts[19] = {
#include "assets/flint_body_partVerts.inc"
};

static SVECTOR _gActor335800FlintBodyVerts[238] = {
#include "assets/flint_body_verts.inc"
};

static SVECTOR _gActor335800FlintBodyNormals[238] = {
#include "assets/flint_body_normals.inc"
};

static u32 _gActor335800FlintBodyStream[2784] = {
#include "assets/flint_body_stream.inc"
};

static TmdSource _gActor335800FlintBody = {
    0,
    13636,
    6016,
    19,
    _gActor335800FlintBodyPartVerts,
    _gActor335800FlintBodyVerts,
    _gActor335800FlintBodyNormals,
    _gActor335800FlintBodySkeleton,
    _gActor335800FlintBodyStream,
};

static AnimationPackedPose _gActor335800Animation11048Bank1[7] = {
#include "assets/actor_335800_animation_11048_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation11048Bank4[128] = {
#include "assets/actor_335800_animation_11048_bank4.inc"
};

static AnimationRecord _gActor335800Animation11048Records[208] = {
#include "assets/actor_335800_animation_11048_records.inc"
};

static u16 _gActor335800Animation11048Indices[20] = {
#include "assets/actor_335800_animation_11048_indices.inc"
};

static AnimationSet _gActor335800Animation11048 = {
    _gActor335800Animation11048Records,
    _gActor335800Animation11048Indices,
    { NULL, _gActor335800Animation11048Bank1, NULL, NULL, _gActor335800Animation11048Bank4, NULL, NULL, NULL },
};

/// Flint's embedded clip table, with clip 0 absent and clip 1 loaded.
///
/// Playback borrows this table and its clip data while the actor package is
/// loaded. Animation IDs index its two entries directly; the NULL entry is
/// not playable. There is no embedded clip-table extension.
static AnimationSet* _gActor335800FlintAnimationBank[2] = {
    NULL,
    &_gActor335800Animation11048,
};

/// Indexed banks for Flint's nineteen-part animation rig; bank 0 is the only bank.
///
/// `AnimationPlayRequest.source.index` selects the bank, whose pointer table
/// the rig borrows until playback ends or the actor package is unloaded.
static AnimationSet** _gActor335800FlintAnimationBanks[1] = {
    _gActor335800FlintAnimationBank,
};

TaskDesc D_actor_335800_80172E9C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor335800FlintTask, { .model = &_gActor335800FlintBody } };

TaskMessageEntry D_actor_335800_80172EA8[6] = {
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
            _actor335800PlaceSceneShadow(coord, &D_actor_335800_80164F80);
            task->killCountdown = ACTOR_335800_SHADOW_INITIAL_SPEED;
            task->state++;
            // Initialization also performs the first movement tick.
        case ACTOR_335800_SHADOW_MOVING:
            if (D_actor_335800_80164F80.pos.vz < coord->coord.t[2]) {
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
    if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.coordBody->coord->workm), &groundPoint) != 0) {
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
    taskSpawnFromTable(D_actor_335800_80164DE0, ACTOR_335800_SCENE_GROUND_SHADOW_TASK, 0, 0);
}

/// Selects the saved and live scene room and requests deferred object relinking.
///
/// `room` is a valid byte-sized room key in the current area. This updates the
/// live save slot without loading a room or requesting a view image.
static void _actor335800SetSceneRoom(u8 room)
{
    gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = room;
    gGameSession->roomObjsDirty                                                                  = true;
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
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_335800_80164EA4[ACTOR_335800_BALCONY_HIGH_PLACEMENT], 0);
        }
        if ((u32)(coord->coord.t[2] - ACTOR_335800_BALCONY_LOW_SCENE_Z) < ACTOR_335800_BALCONY_SPLIT_Z - ACTOR_335800_BALCONY_LOW_SCENE_Z) {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_335800_80164EA4[ACTOR_335800_BALCONY_LOW_PLACEMENT], 0);
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
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_335800_80164EA4[highPlacementIndex], 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_335800_80164EA4[lowPlacementIndex], 0);
        }
    }
}

/// Selects the saved and live room view and requests its image and objects reload.
///
/// `view` is a valid 1-based view slot in the current room, stored as a byte.
/// This only schedules the reload; it does not load or apply the view itself.
static inline void _actor335800SetView(s32 view)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = view;
    gGameSession->location.loc.view                            = view;
    gGameSession->viewDirty                                    = 1;
    gGameSession->roomObjsDirty                                = 1;
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
    location = &session->location.loc;
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
            evsStartScript(D_actor_335800_80166098, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        } else {
            evsStartScript(D_actor_335800_80165FC0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        task->state += 1;
        return;
    }
    taskKill(task);
}

void func_actor_335800_801623D8(void)
{
    taskSpawnFromTable(D_dryfield_night_motel_balcony_80182834, 0, 0, 0);
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
    sndEvtRequestMidiStop(gStageSceneMusicEntry, fadeTicks & ACTOR_335800_MUSIC_FADE_TICKS_MASK);
}

/// Selects map objective code 24 after the balcony scene changes rooms.
static void _actor335800SetPostSceneObjective(void)
{
    enum { ACTOR_335800_POST_SCENE_OBJECTIVE = 24 };
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACTOR_335800_POST_SCENE_OBJECTIVE);
}

/// Locks attachment commands for the scene and requests cancellation of room effects.
///
/// Preserves other attachment flags. Effect cancellation is deferred to room
/// effect processing. Normal attachment/HUD updates later clear the lock.
static void _actor335800LockAttachmentsAndCancelEffects(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
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
/// then synchronously plays bank 1 clip 51. A missing player is not tested;
/// the reset flag remains set. The task waits indefinitely while it is clear.
static void _actor335800RestorePlayerAnimationOnBattleResetTask(Task* task)
{
    Task* playerTask;

    if (gGameSession->battleResetPending != 0) {
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        playerActorWriteWeaponAnimationBankIndex(&D_actor_335800_80164E7C.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_PLAY, &D_actor_335800_80164E7C, 0);
        taskKill(task);
    }
}

/// Spawns vibration-driven scene screen shake; allocation failure is ignored.
static void _actor335800SpawnSceneScreenShake(void)
{
    enum { ACTOR_335800_SCENE_SCREEN_SHAKE_TASK = 3 };
    taskSpawnFromTable(D_actor_335800_80164DE0, ACTOR_335800_SCENE_SCREEN_SHAKE_TASK, 0, 0);
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

static void func_actor_335800_80162640(Task* arg0)
{
    _Actor335800GaryDouglasWork* work;
    GameLocationKey              key;
    GameLocationKey*             sessionKey;
    GameLocationKey*             keyAddr;
    Task*                        spawned;

    work = memCalloc(sizeof(_Actor335800GaryDouglasWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    spawned                  = taskSpawnFromTable(D_actor_335800_8016EADC, 1, 4, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->headTask = spawned;
        model          = spawned->extra.tmd;
        idx            = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey     = &gGameSession->location.loc;
        key.stage      = sessionKey->stage;
        key.area       = sessionKey->area;
        key.room       = sessionKey->room;
        key.view       = sessionKey->view;
        areaSyncLocationVariant(&key);
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    spawned = taskSpawnFromTable(D_actor_335800_8016EADC, 2, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->gunTask = spawned;
        model         = spawned->extra.tmd;
        idx           = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey    = (keyAddr = &gGameSession->location.loc);
        key.stage     = sessionKey->stage;
        key.area      = sessionKey->area;
        key.room      = keyAddr->room;
        key.view      = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    _actor335800GaryDouglasBindLighting(arg0);
    arg0->msgTable     = D_actor_335800_8016EB00;
    arg0->exitCallback = _actor335800GaryDouglasExit;
    arg0->state       += 1;
}

static void func_actor_335800_80162844(Task* task)
{
    TmdObject*                   ext      = task->extra.tmd;
    _Actor335800GaryDouglasWork* work     = (_Actor335800GaryDouglasWork*)task->work;
    TaskFunc                     funcs[2] = { _actor335800GaryDouglasIdle, _actor335800GaryDouglasRunWalkStep };
    VECTOR3                      pos;
    GfxCoord*                    coord;
    const AnimationRecord*       rec;
    s32                          i;
    s32                          j;

    funcs[work->walk.motion](task);
    coord                     = task->extra.tmd->coords;
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
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                animationTickSlot(&work->rig.anim, i);
            }
            rec = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                    effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, &task->extra.tmd->coords[8], 0xD, NULL);
                }
                work->prevCueFlags = rec->flags & ANIMATION_RECORD_CUE_MASK;
            }
        }
        if (work->lightState == ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->model.color.m[i][j] >>= 1;
                    work->model.light.m[i][j] >>= 1;
                }
                work->model.color.t[i] >>= 1;
                work->model.light.t[i] >>= 1;
            }
            work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_DIMMED;
        }
        if (work->lightState >= ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL) {
            if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
                effectDrawGroundShadow(&pos, 0x300, gRoomEffectState->groundShadowShade);
            }
        }
    }
    if (gGameSession->viewReady != 0) {
        work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(ext, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// Dispatches Gary Douglas's attached head-and-hat and gun model tasks.
///
/// State must be 0..2: attach to the spawning body's selected part, retain that
/// attachment, or kill the task. Setup borrows the parent's coordinates and
/// lighting and joins its teardown tree; the parent must outlive the model.
static void _actor335800GaryDouglasPartTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_335800_80161E24;
    handlers.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Retains an attached part's parent-driven pose without a per-frame update.
static void _actor335800GaryDouglasPartIdle(Task* task)
{
}

/// State dispatcher of the parent block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80162F10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
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
    handlers = D_actor_335800_80161E3C;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Starts Douglas's forward approach and primes the next arrival test.
///
/// Requires initialized work and a live root already facing the destination.
/// Rotates a signed 16.16 forward velocity into the root parent's frame: 32
/// coordinate units per update for a unit-scale root. Seeds the previous gaps
/// with `ACTOR_WALK_DISTANCE_NONE` and advances to arrival checking, without
/// changing translation or fractional carry.
static void _actor335800GaryDouglasBeginApproach(Task* task)
{
    _Actor335800GaryDouglasWork* work;
    GfxCoord*                    rootCoord;
    VECTOR                       forwardVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    // Rotate velocity without adding the root's translation.
    forwardVelocity = D_actor_335800_80161E4C;
    ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

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
        ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE                  = 0,
        ACTOR_335800_GARY_DOUGLAS_DRAW_SHOW_ALLOCATE         = 1,
        ACTOR_335800_GARY_DOUGLAS_DRAW_HIDE_RELEASE          = 2,
        ACTOR_335800_GARY_DOUGLAS_DRAW_SHOW_SKIP_AUTO_BUFFER = 3
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
        case ACTOR_335800_GARY_DOUGLAS_DRAW_SHOW_SKIP_AUTO_BUFFER:
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
    enum { ACTOR_335800_GARY_DOUGLAS_COMMAND_DIM = 0 };
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
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
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

#include "../../shared/actor_motion_arrive19.inc.c"
#undef ACTOR_MOTION_PLAY19_HANDLER

/// Binds Flint's playback storage and starts the requested clip, including repeats.
///
/// Requires initialized work and a live nineteen-part model. Set `work->model.bank`
/// to `ACTOR_MODEL_STATE_NONE` before the first request so it binds the rig;
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

    handlers = D_actor_335800_80161E5C;
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
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = ACTOR_335800_FLINT_NO_BUFFER_RELEASE;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _actor335800FlintBindLighting(task);

    task->msgTable     = D_actor_335800_80172EA8;
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
    handlers = D_actor_335800_80161E68;
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

    localVelocity = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
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
#include "../../shared/actor_messages_place_euler.inc.c"
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
        ACTOR_335800_FLINT_DRAW_HIDE                  = 0,
        ACTOR_335800_FLINT_DRAW_SHOW_ALLOCATE         = 1,
        ACTOR_335800_FLINT_DRAW_HIDE_RELEASE          = 2,
        ACTOR_335800_FLINT_DRAW_SHOW_SKIP_AUTO_BUFFER = 3
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
        case ACTOR_335800_FLINT_DRAW_SHOW_SKIP_AUTO_BUFFER:
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
