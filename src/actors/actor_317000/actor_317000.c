#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Work block of the overlay's actor, allocated zeroed by its spawn state and
/// kept at `Task::work` for the task's life.
///
/// It opens with the head `_actorMotionPlayAnim19` runs on
/// (`ActorMotion19PlayWork`) and keeps a scripted walker's walk state
/// directly after it, so the room script plays the actor's clips and sends it
/// to a placement with the same messages as any scripted walker. The model
/// object borrows `model.light` and `model.color` for as long as the block
/// lives.
///
/// What a walk request starts here is a leap rather than a walk: the actor
/// turns to the placement's yaw, launches forward and up along its own axes,
/// falls under gravity while `airborne` until it is back near the floor, and
/// then turns to face the player. The placement's position is recorded in
/// `walk.target` and never read, and nothing measures an arrival.
///
/// What follows `walk` is the package's own: the leap's gravity switch, the
/// weight of the head turn toward the player, and the delayed free of the
/// model's buffers once the model has been hidden.
typedef struct {
    ActorAnimRig19  rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;             // Leap in progress: the yaw it launches along, the per-frame velocity and the step; `target` and `lastDistance` are not read
    s8              airborne;         // The leap is in flight, so each tick adds gravity to `walk.velocity.vy` (0 on the floor, 1 from launch to landing)
    s8              turnWeightRising; // Direction `turnWeight` ramps, set by an actor command (0 falls by 0x80 a tick, 1 rises by 0x40)
    s16             turnWeight;       // Weight handed to the per-frame head turn toward the player, 0 to `ONE`; the package's turn takes it and does not read it
    s16             freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor317000Work;
STATIC_ASSERT_SIZEOF(_Actor317000Work, 0x4CC);

/// Leap tuning and animation choices of this actor package.
enum {
    ACTOR_317000_YAW_STEP       = 0x40, // 4096 angle units per turn
    ACTOR_317000_LEAP_ANIMATION = 2,
    ACTOR_317000_BLEND_FRAMES   = 5,
};

/// Model draw modes accepted by this package's message handler.
enum {
    ACTOR_317000_DRAW_HIDE_AUTO     = 0,
    ACTOR_317000_DRAW_SHOW_ALLOCATE = 1,
    ACTOR_317000_DRAW_HIDE_RELEASE  = 2,
    ACTOR_317000_DRAW_SHOW_MANUAL   = 3,
};

/// Indexed by `_Actor317000Work::model.bank` for `animationInitContext`'s second
/// argument by `_actor317000BeginLeap` and `_actorMotionPlayAnim19`.
/// Every preset the actor builds has `field_0` 0, so only the first word is
/// ever read; the words after it (among them the address of
/// `_actor317000Task`) suggest a larger record, not a bank array.
extern AnimationSet*  D_actor_317000_8016CF1C[9];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `_actor317000Init`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_317000_8016CF50[];

static void _actor317000Update(Task* task);
static void _actor317000FacePlayerAfterLeap(Task* task);
static void _actor317000TurnHeadToTarget(Task* task, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight);
static void _actor317000Init(Task* task);
static void _actor317000Exit(Task* task);
static void _actor317000BindLighting(Task* task);
static void _actor317000Idle(Task* task);
static void _actor317000RunLeapStep(Task* task);
static void _actor317000TurnToLaunchYaw(Task* task);
static void _actor317000LaunchLeap(Task* task);
static void _actor317000LandLeap(Task* task);
static s32  _actor317000SetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);

/// The actor's three task states, which `_actor317000Task` runs by
/// `Task::state`: spawn, per-frame tick and exit.
static const TaskFuncTable3 D_actor_317000_80161E24 = { {
    _actor317000Init,
    _actor317000Update,
    _actor317000Exit,
} };

/// The four step handlers `_actor317000RunLeapStep` runs by
/// `_Actor317000Work::walk.motionStep`.
static const TaskFuncTable4 D_actor_317000_80161E30 = { {
    _actor317000TurnToLaunchYaw,
    _actor317000LaunchLeap,
    _actor317000LandLeap,
    _actor317000FacePlayerAfterLeap,
} };

/// The constant local-space offset `_actor317000LaunchLeap` rotates
/// through the root coordinate into `_Actor317000Work::walk.velocity`.
static const VECTOR D_actor_317000_80161E40 = { 0, 0xFF800000, 0x400000, 0 };

static TmdSource _gActor317000GrinningStrangerBody;
static s32       _actor317000BeginLeap(Task* task, s32 messageId, const ActorTransform* launchPlacement, const ActorMotionWalkAnim* animationRequest);
static s32       _actor317000ApplyCommand(Task* task, s32 messageId, const ActorCommand* commandRequest, s32 unusedArg);
static void      _actor317000Task(Task* task);

static TmdBone _gActor317000GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor317000GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor317000GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor317000GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor317000GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor317000GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor317000GrinningStrangerBodyPartVerts,
    _gActor317000GrinningStrangerBodyVerts,
    _gActor317000GrinningStrangerBodyNormals,
    _gActor317000GrinningStrangerBodySkeleton,
    _gActor317000GrinningStrangerBodyStream,
};

static AnimationPackedPose _gActor317000Animation0699CBank1[4] = {
#include "assets/actor_317000_animation_0699C_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0699CBank4[69] = {
#include "assets/actor_317000_animation_0699C_bank4.inc"
};

static AnimationRecord _gActor317000Animation0699CRecords[162] = {
#include "assets/actor_317000_animation_0699C_records.inc"
};

static u16 _gActor317000Animation0699CIndices[20] = {
#include "assets/actor_317000_animation_0699C_indices.inc"
};

static AnimationSet _gActor317000Animation0699C = {
    _gActor317000Animation0699CRecords,
    _gActor317000Animation0699CIndices,
    { NULL, _gActor317000Animation0699CBank1, NULL, NULL, _gActor317000Animation0699CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation074C0Bank1[28] = {
#include "assets/actor_317000_animation_074C0_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation074C0Bank4[247] = {
#include "assets/actor_317000_animation_074C0_bank4.inc"
};

static AnimationRecord _gActor317000Animation074C0Records[362] = {
#include "assets/actor_317000_animation_074C0_records.inc"
};

static u16 _gActor317000Animation074C0Indices[20] = {
#include "assets/actor_317000_animation_074C0_indices.inc"
};

static AnimationSet _gActor317000Animation074C0 = {
    _gActor317000Animation074C0Records,
    _gActor317000Animation074C0Indices,
    { NULL, _gActor317000Animation074C0Bank1, NULL, NULL, _gActor317000Animation074C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation07F44Bank1[32] = {
#include "assets/actor_317000_animation_07F44_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation07F44Bank4[223] = {
#include "assets/actor_317000_animation_07F44_bank4.inc"
};

static AnimationRecord _gActor317000Animation07F44Records[334] = {
#include "assets/actor_317000_animation_07F44_records.inc"
};

static u16 _gActor317000Animation07F44Indices[20] = {
#include "assets/actor_317000_animation_07F44_indices.inc"
};

static AnimationSet _gActor317000Animation07F44 = {
    _gActor317000Animation07F44Records,
    _gActor317000Animation07F44Indices,
    { NULL, _gActor317000Animation07F44Bank1, NULL, NULL, _gActor317000Animation07F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation08F08Bank1[29] = {
#include "assets/actor_317000_animation_08F08_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation08F08Bank4[374] = {
#include "assets/actor_317000_animation_08F08_bank4.inc"
};

static AnimationRecord _gActor317000Animation08F08Records[528] = {
#include "assets/actor_317000_animation_08F08_records.inc"
};

static u16 _gActor317000Animation08F08Indices[20] = {
#include "assets/actor_317000_animation_08F08_indices.inc"
};

static AnimationSet _gActor317000Animation08F08 = {
    _gActor317000Animation08F08Records,
    _gActor317000Animation08F08Indices,
    { NULL, _gActor317000Animation08F08Bank1, NULL, NULL, _gActor317000Animation08F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation09F30Bank1[27] = {
#include "assets/actor_317000_animation_09F30_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation09F30Bank4[406] = {
#include "assets/actor_317000_animation_09F30_bank4.inc"
};

static AnimationRecord _gActor317000Animation09F30Records[527] = {
#include "assets/actor_317000_animation_09F30_records.inc"
};

static u16 _gActor317000Animation09F30Indices[20] = {
#include "assets/actor_317000_animation_09F30_indices.inc"
};

static AnimationSet _gActor317000Animation09F30 = {
    _gActor317000Animation09F30Records,
    _gActor317000Animation09F30Indices,
    { NULL, _gActor317000Animation09F30Bank1, NULL, NULL, _gActor317000Animation09F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0A338Bank1[6] = {
#include "assets/actor_317000_animation_0A338_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0A338Bank4[70] = {
#include "assets/actor_317000_animation_0A338_bank4.inc"
};

static AnimationRecord _gActor317000Animation0A338Records[150] = {
#include "assets/actor_317000_animation_0A338_records.inc"
};

static u16 _gActor317000Animation0A338Indices[20] = {
#include "assets/actor_317000_animation_0A338_indices.inc"
};

static AnimationSet _gActor317000Animation0A338 = {
    _gActor317000Animation0A338Records,
    _gActor317000Animation0A338Indices,
    { NULL, _gActor317000Animation0A338Bank1, NULL, NULL, _gActor317000Animation0A338Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0A98CBank1[18] = {
#include "assets/actor_317000_animation_0A98C_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0A98CBank4[146] = {
#include "assets/actor_317000_animation_0A98C_bank4.inc"
};

static AnimationRecord _gActor317000Animation0A98CRecords[185] = {
#include "assets/actor_317000_animation_0A98C_records.inc"
};

static u16 _gActor317000Animation0A98CIndices[20] = {
#include "assets/actor_317000_animation_0A98C_indices.inc"
};

static AnimationSet _gActor317000Animation0A98C = {
    _gActor317000Animation0A98CRecords,
    _gActor317000Animation0A98CIndices,
    { NULL, _gActor317000Animation0A98CBank1, NULL, NULL, _gActor317000Animation0A98CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0B0D4Bank1[13] = {
#include "assets/actor_317000_animation_0B0D4_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0B0D4Bank4[180] = {
#include "assets/actor_317000_animation_0B0D4_bank4.inc"
};

static AnimationRecord _gActor317000Animation0B0D4Records[227] = {
#include "assets/actor_317000_animation_0B0D4_records.inc"
};

static u16 _gActor317000Animation0B0D4Indices[20] = {
#include "assets/actor_317000_animation_0B0D4_indices.inc"
};

static AnimationSet _gActor317000Animation0B0D4 = {
    _gActor317000Animation0B0D4Records,
    _gActor317000Animation0B0D4Indices,
    { NULL, _gActor317000Animation0B0D4Bank1, NULL, NULL, _gActor317000Animation0B0D4Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_317000_8016CF1C[9] = {
    NULL,
    &_gActor317000Animation0699C,
    &_gActor317000Animation074C0,
    &_gActor317000Animation07F44,
    &_gActor317000Animation08F08,
    &_gActor317000Animation09F30,
    &_gActor317000Animation0A338,
    &_gActor317000Animation0A98C,
    &_gActor317000Animation0B0D4,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_317000_8016CF1C,
};

TaskDesc D_actor_317000_8016CF44 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor317000Task, { .model = &_gActor317000GrinningStrangerBody } };

TaskMessageEntry D_actor_317000_8016CF50[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor317000SetDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actor317000BeginLeap },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor317000ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Applies one frame of leap displacement and advances the airborne velocity.
///
/// Requires initialized writable work and a live root coordinate in separate
/// storage. Velocity uses signed 16.16 units per frame in the root's parent
/// coordinate frame; carry holds the previous frame's unsigned fractions.
/// While airborne, positive Y gravity adds 18 whole units per frame to the
/// next frame's velocity. Signed integer halves move the root, and carry words
/// finish in 0..0xFFFF. Composition is invalidated even when stationary.
/// Performs no collision or floor correction and retains no pointers.
static inline void _actor317000IntegrateLeapVelocity(_Actor317000Work* work, GfxCoord* rootCoord)
{
    enum { ACTOR_317000_LEAP_GRAVITY = 18 << 16 }; // Signed 16.16 units per tick squared

    // Accumulate this frame's displacement before gravity changes the velocity.
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    if (work->airborne != 0) {
        work->walk.velocity.vy += ACTOR_317000_LEAP_GRAVITY;
    }
    rootCoord->coord.t[0]  += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]  += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]  += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Retain the unsigned fractions after consuming the signed integer steps.
    work->walk.carry[0].word = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word = work->walk.carry[2].halves.fraction;
}

/// Updates the leap, animation, head turn, lighting and delayed buffer release.
///
/// Requires a live nineteen-part TMD model and initialized `_Actor317000Work`.
/// `walk.motion` must be 0 (idle) or 1 (leaping). Displacement uses signed
/// 16.16 root-frame units; gravity changes the next tick's vertical velocity.
/// Slots 1..18 tick while playback is active. Lighting refresh requires a ready
/// view, but motion and head turning run every tick. The update finding a zero
/// release counter frees the primitive buffer, then disables the counter.
static void _actor317000Update(Task* task)
{
    enum { ACTOR_317000_HEAD_WEIGHT_RISE = 0x40,
           ACTOR_317000_HEAD_WEIGHT_FALL = 0x80,
           ACTOR_317000_LIGHT_COUNT      = 3 };
    TmdObject*        bodyModel         = task->extra.tmd;
    _Actor317000Work* work              = task->work;
    TaskFunc          motionHandlers[2] = { _actor317000Idle, _actor317000RunLeapStep };
    GfxCoord*         rootCoord;
    s32               slotIndex;

    motionHandlers[work->walk.motion](task);

    // Integrate this tick before gravity changes the next tick's velocity.
    rootCoord = task->extra.tmd->coords;
    _actor317000IntegrateLeapVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, ACTOR_317000_LIGHT_COUNT);
    }
    if (work->turnWeightRising != 0) {
        work->turnWeight += ACTOR_317000_HEAD_WEIGHT_RISE;
        if (work->turnWeight > ONE) {
            work->turnWeight = ONE;
        }
    } else {
        work->turnWeight -= ACTOR_317000_HEAD_WEIGHT_FALL;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    _actor317000TurnHeadToTarget(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_TRANSFORM_ANGLE_TURN / 4, ACTOR_TRANSFORM_ANGLE_TURN / 8, work->turnWeight);
    // Delay release until queued model primitives have left the frame buffers.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

/// Ends the leap by turning the root toward the player's position.
///
/// Requires a live player TMD task in `GAME_TASK_SLOT_PLAYER`, with both roots
/// in the same coordinate frame. Turns by 64 of 4096 angle units per tick.
/// A yaw gap of at most one step returns motion to idle without snapping the
/// matrix to the target yaw. The desired yaw is also stored in `param.rot`.
/// The signed-halfword yaw difference is retained without half-turn wrapping.
static void _actor317000FacePlayerAfterLeap(Task* task)
{
    _Actor317000Work* work;
    GfxCoord*         rootCoord;
    GfxCoord*         playerCoord;
    VECTOR            toPlayer;
    SVECTOR           direction;
    SVECTOR           targetAngles;
    SVECTOR           rootAngles;
    s16               yawDelta;
    s32               yawMagnitude;
    s32               currentYaw;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    work        = task->work;

    toPlayer.vx = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    toPlayer.vy = playerCoord->coord.t[1] - rootCoord->coord.t[1];
    toPlayer.vz = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    VectorNormalS(&toPlayer, &direction);

    targetAngles.vx = 0;
    targetAngles.vy = ratan2(direction.vx, direction.vz);
    targetAngles.vz = 0;

    rootCoord->param.rot.vx = targetAngles.vx;
    rootCoord->param.rot.vy = targetAngles.vy;
    rootCoord->param.rot.vz = targetAngles.vz;

    gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
    yawDelta     = ratan2(direction.vx, direction.vz) - rootAngles.vy;
    yawMagnitude = abs(yawDelta);
    if (yawMagnitude >= ACTOR_317000_YAW_STEP + 1) {
        currentYaw = rootAngles.vy;
        if (yawDelta < 0) {
            rootAngles.vy = currentYaw - ACTOR_317000_YAW_STEP;
        } else {
            rootAngles.vy = currentYaw + ACTOR_317000_YAW_STEP;
        }
    } else {
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }
    RotMatrix(&rootAngles, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turns the head toward a target model and shares some rotation with part 3.
///
/// Requires live subject coordinates 0..5 and target coordinates 0..4, with
/// current composed matrices in the same frame. Uses the head at part 5 and
/// target part 4, expressing their separation through part 2's transposed
/// rotation. Angles use 4096 units per turn; additions narrow to halfwords.
/// Part 3 receives one eighth of the resulting head Euler angles, and the head
/// keeps five eighths. `maxYaw`, `maxPitch` and the 1/4096 `blendWeight` are
/// accepted but ignored. Leaves composition stamps unchanged; retains no pointers.
static void _actor317000TurnHeadToTarget(Task* task, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight)
{
    enum {
        ACTOR_317000_HEAD_PART           = 5,
        ACTOR_317000_TARGET_HEAD_PART    = 4,
        ACTOR_317000_HEAD_REFERENCE_PART = 2,
        ACTOR_317000_HEAD_SUPPORT_PART   = 3,
    };
    GfxCoord* subjectCoords;
    GfxCoord* targetCoords;
    GfxCoord* headCoord;
    GfxCoord* targetHeadCoord;
    MATRIX*   part3Rotation;
    VECTOR    toTarget;
    VECTOR    direction;
    SVECTOR   headAngles;
    SVECTOR   jointAngles;
    SVECTOR   turnAngles;

    subjectCoords   = task->extra.tmd->coords;
    targetCoords    = targetTask->extra.tmd->coords;
    headCoord       = &subjectCoords[ACTOR_317000_HEAD_PART];
    targetHeadCoord = &targetCoords[ACTOR_317000_TARGET_HEAD_PART];

    // Express the world separation in the subject's part-2 frame.
    toTarget.vx = targetHeadCoord->workm.t[0] - headCoord->workm.t[0];
    toTarget.vy = targetHeadCoord->workm.t[1] - headCoord->workm.t[1];
    toTarget.vz = targetHeadCoord->workm.t[2] - headCoord->workm.t[2];
    VectorNormal(&toTarget, &toTarget);
    ApplyTransposeMatrixLV(&task->extra.tmd->coords[ACTOR_317000_HEAD_REFERENCE_PART].workm, &toTarget, &toTarget);
    VectorNormal(&toTarget, &direction);

    turnAngles.vx = ratan2(direction.vz, direction.vy);
    turnAngles.vy = ratan2(direction.vx, -ABS(direction.vz));
    turnAngles.vz = ratan2(direction.vx, direction.vy);

    gfxExtractSmallestEuler(&headAngles, &subjectCoords[ACTOR_317000_HEAD_PART].coord);
    headAngles.vx = (u16)headAngles.vx + (u16)turnAngles.vx;
    headAngles.vy = (u16)headAngles.vy + (u16)turnAngles.vy;
    headAngles.vz = (u16)headAngles.vz + (u16)turnAngles.vz;
    RotMatrix(&headAngles, &subjectCoords[ACTOR_317000_HEAD_PART].coord);

    // Share the resulting head rotation with part 3 before reducing the head pose.
    gfxExtractSmallestEuler(&turnAngles, &subjectCoords[ACTOR_317000_HEAD_PART].coord);
    part3Rotation = &task->extra.tmd->coords[ACTOR_317000_HEAD_SUPPORT_PART].coord;
    gfxExtractSmallestEuler(&jointAngles, part3Rotation);
    jointAngles.vx = (u16)jointAngles.vx + turnAngles.vx / 8;
    jointAngles.vy = (u16)jointAngles.vy + turnAngles.vy / 8;
    jointAngles.vz = (u16)jointAngles.vz + turnAngles.vz / 8;
    RotMatrix(&jointAngles, part3Rotation);

    gfxExtractSmallestEuler(&jointAngles, &subjectCoords[ACTOR_317000_HEAD_PART].coord);
    jointAngles.vx = jointAngles.vx * 5 / 8;
    jointAngles.vy = jointAngles.vy * 5 / 8;
    jointAngles.vz = jointAngles.vz * 5 / 8;

    gfxSetRotIdentity(&subjectCoords[ACTOR_317000_HEAD_PART].coord);
    RotMatrix(&jointAngles, &subjectCoords[ACTOR_317000_HEAD_PART].coord);
}

/// Applies the leap's requested start clip without restarting repeated clips.
///
/// Requires a live nineteen-part TMD task and initialized `_Actor317000Work`,
/// with `model.bank` initially `ACTOR_MODEL_STATE_NONE`. The readable request
/// must not overlap playback storage: `source.index` selects bank 0 and
/// `animationId` selects a loaded clip 1..8; stored IDs narrow to signed bytes.
/// A bank change binds the rig and invalidates the old clip. An unchanged clip
/// in the same bank leaves playback alone, including its blend and cursor.
/// Slots 1..18 blend when requested and already ticking, otherwise reset.
/// Blending captures an advanced pose before a final tick of the new clip;
/// `blendFrames` counts whole normal-rate frames (0..2047). Slot 0 and the
/// collision choice are unused. The request may expire on return; work-owned
/// slots/poses, model coordinates and bank/clip data stay live during playback.
/// Scratch-stack and GTE requirements follow `animationTickSlotPose`.
static inline void _actor317000ApplyLeapStartAnimation(Task* task, const AnimationPlayRequest* request)
{
    enum { ACTOR_317000_FIRST_DRIVEN_SLOT = 1 };
    _Actor317000Work* playbackWork;
    TmdObject*        bodyModel;
    s32               slotIndex;

    playbackWork = task->work;
    bodyModel    = task->extra.tmd;
    // Changing banks invalidates the previous clip even when its ID agrees.
    if (request->source.index != playbackWork->model.bank) {
        playbackWork->model.bank   = request->source.index;
        playbackWork->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&playbackWork->rig.anim, gActorMotionAnimBanks19[playbackWork->model.bank], bodyModel, playbackWork->rig.poses,
                             playbackWork->rig.slots);
    }
    if (request->animationId != playbackWork->model.animId) {
        playbackWork->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && playbackWork->model.ticking != 0) {
            for (slotIndex = ACTOR_317000_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&playbackWork->rig.anim, slotIndex, playbackWork->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = ACTOR_317000_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
                animationResetSlot(&playbackWork->rig.anim, slotIndex, playbackWork->model.animId);
            }
        }
        // Apply the selected pose before the normal frame update resumes ticking.
        for (slotIndex = ACTOR_317000_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
            animationTickSlot(&playbackWork->rig.anim, slotIndex);
        }
        playbackWork->model.ticking = true;
    }
}

/// Handles a walk-to message by starting this actor's scripted leap.
///
/// Requires initialized work and a live nineteen-part model. Borrows a readable
/// placement and optional clip pair through dispatch; records the position
/// without using it as an arrival target. The placement yaw selects the launch
/// heading. An absent clip pair selects clips 2 and 1; supplied IDs must index
/// bank 0's loaded clips. The start clip blends for five frames when already
/// ticking, otherwise its slots reset. Returns 0; ignores the message ID.
/// Keeps the current step, velocity and fractional carry, so a fresh sequence
/// requires the idle step left by initialization or completion.
static s32 _actor317000BeginLeap(Task* task, s32 messageId, const ActorTransform* launchPlacement, const ActorMotionWalkAnim* animationRequest)
{
    enum { ACTOR_317000_DEFAULT_LANDING_ANIMATION = 1 };
    _Actor317000Work*    leapWork;
    AnimationPlayRequest startRequest;

    leapWork                    = task->work;
    leapWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    leapWork->walk.target.vx    = launchPlacement->pos.vx;
    leapWork->walk.target.vy    = launchPlacement->pos.vy;
    leapWork->walk.target.vz    = launchPlacement->pos.vz;
    leapWork->walk.targetRot.vx = launchPlacement->rot.vx;
    leapWork->walk.targetRot.vy = launchPlacement->rot.vy;
    leapWork->walk.targetRot.vz = launchPlacement->rot.vz;
    startRequest.source.index   = 0;
    if (animationRequest != NULL) {
        startRequest.animationId   = animationRequest->animationId;
        leapWork->model.nextAnimId = animationRequest->nextAnimId;
    } else {
        startRequest.animationId   = ACTOR_317000_LEAP_ANIMATION;
        leapWork->model.nextAnimId = ACTOR_317000_DEFAULT_LANDING_ANIMATION;
    }
    startRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    startRequest.blendFrames          = ACTOR_317000_BLEND_FRAMES;
    startRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    // Apply a changed clip immediately; an unchanged clip keeps its current cursor.
    _actor317000ApplyLeapStartAnimation(task, &startRequest);
    return 0;
}

/// Runs this actor's initialization, update or exit state.
///
/// Requires a live TMD task with `state` 0, 1 or 2 respectively; the stack-copied
/// dispatch table has no bounds check. The actor package and its descriptor's
/// model data must remain loaded for the task's lifetime.
static void _actor317000Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_317000_80161E24;
    stateHandlers.funcs[task->state](task);
}

/// Allocates the actor's playback and leap work and installs its callbacks.
///
/// The zeroed block belongs to the task and remains live until enemy teardown.
/// Seeds unbound bank/clip IDs and a disabled buffer-release counter, lends the
/// work's light and colour matrices to the TMD object, and starts hidden with
/// automatic buffer handling. Allocation failure tears down the task.
static void _actor317000Init(Task* task)
{
    enum { ACTOR_317000_BUFFER_FREE_NONE = -1 };
    _Actor317000Work* work;

    work = memCalloc(sizeof(_Actor317000Work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = ACTOR_317000_BUFFER_FREE_NONE;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _actor317000BindLighting(task);
    _actor317000SetDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_317000_DRAW_HIDE_AUTO, 0);

    task->msgTable     = D_actor_317000_8016CF50;
    task->exitCallback = _actor317000Exit;
    task->state++;
}

/// Tears down the actor through the enemy task lifecycle.
///
/// Used both as state 2 and as the exit callback; the task and its owned work
/// must not be used after this call.
static void _actor317000Exit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the work-owned light and colour matrices to the actor's TMD object.
///
/// Requires initialized live work and model storage. Both matrices must remain
/// live while the model borrows them; no matrix contents are changed here.
static void _actor317000BindLighting(Task* task)
{
    TmdObject*        bodyModel;
    _Actor317000Work* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves an idle actor's motion unchanged while its animation and head turn continue.
static void _actor317000Idle(Task* task)
{
}

/// Dispatches the current leap phase from a stack copy of the package's table.
///
/// Requires initialized work with `walk.motionStep` 0..3: turn to launch yaw,
/// launch, land, face the player. Initialization and the final phase establish
/// step 0; each intervening phase advances once. Dispatch has no bounds check.
static void _actor317000RunLeapStep(Task* task)
{
    TaskFuncTable4    handlers;
    _Actor317000Work* work;

    work     = task->work;
    handlers = D_actor_317000_80161E30;
    handlers.funcs[work->walk.motionStep](task);
}

/// Turns to the requested launch yaw and selects the leap animation.
///
/// Requires initialized work and root coordinate. Angles use 4096 units per
/// turn; moves at most 64 per tick, then snaps within that gap, selects clip 2
/// with a five-frame blend, disables gravity and advances to launch.
/// The signed-halfword yaw subtraction has no half-turn wrap correction.
static void _actor317000TurnToLaunchYaw(Task* task)
{
    _Actor317000Work*    work;
    GfxCoord*            rootCoord;
    SVECTOR              rootAngles;
    AnimationPlayRequest launchRequest;
    s32                  currentYaw;
    s16                  yawDelta;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
    yawDelta = (u16)work->walk.targetRot.vy - (u16)rootAngles.vy;
    if (ABS(yawDelta) >= ACTOR_317000_YAW_STEP + 1) {
        currentYaw = rootAngles.vy;
        if (yawDelta < 0) {
            rootAngles.vy = currentYaw - ACTOR_317000_YAW_STEP;
        } else {
            rootAngles.vy = currentYaw + ACTOR_317000_YAW_STEP;
        }
    } else {
        rootAngles.vy                      = work->walk.targetRot.vy;
        launchRequest.source.index         = 0;
        launchRequest.animationId          = ACTOR_317000_LEAP_ANIMATION;
        launchRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        launchRequest.blendFrames          = ACTOR_317000_BLEND_FRAMES;
        launchRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &launchRequest, 0);
        work->airborne = 0;
        work->walk.motionStep++;
    }

    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rootAngles, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Launches the actor by rotating its signed 16.16 local velocity into the root frame.
///
/// Requires an initialized root rotation and leap work. The package vector
/// provides -128 Y and +64 Z coordinate units per tick before rotation.
/// Enables gravity and advances to the landing check; retains fractional carry.
static void _actor317000LaunchLeap(Task* task)
{
    _Actor317000Work* work;
    GfxCoord*         rootCoord;
    VECTOR            launchVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    launchVelocity = D_actor_317000_80161E40;
    ApplyMatrixLV(&rootCoord->coord, &launchVelocity, &work->walk.velocity);
    work->airborne = 1;
    work->walk.motionStep++;
}

/// Stops the leap near floor height, plays the queued clip and sounds the landing.
///
/// Requires initialized work and root coordinate. Waits while root Y is below
/// -48 coordinate units, then blends to `model.nextAnimId` for five frames,
/// plays the positional actor sound, clears velocity and gravity, and advances
/// to face the player. It neither snaps root Y to the floor nor clears carry.
static void _actor317000LandLeap(Task* task)
{
    enum {
        ACTOR_317000_LANDING_Y     = -0x30, // Root-coordinate units; positive Y points down
        ACTOR_317000_LANDING_SOUND = SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x0B),
    };
    GfxCoord*            rootCoord;
    _Actor317000Work*    work;
    AnimationPlayRequest landingRequest;
    s32                  audioPan;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    if (rootCoord->coord.t[1] < ACTOR_317000_LANDING_Y) {
        return;
    }
    landingRequest.source.index         = 0;
    landingRequest.animationId          = work->model.nextAnimId;
    landingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    landingRequest.blendFrames          = ACTOR_317000_BLEND_FRAMES;
    landingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &landingRequest, 0);
    audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
    sndEvtRequestScriptStart(ACTOR_317000_LANDING_SOUND, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));

    work->walk.velocity.vx = 0;
    work->walk.velocity.vy = 0;
    work->walk.velocity.vz = 0;
    work->airborne         = 0;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Handles the actor's draw mode and primitive-buffer ownership choice.
///
/// Requires initialized work and a live TMD object. Modes: 0 hides with automatic
/// buffers; 1 shows and allocates a buffer with automatic handling; 2 hides,
/// schedules release on the third following update and disables automatic handling;
/// 3 shows with manual buffers. Mode 3 requires a usable buffer already present.
/// Other values return 1 unchanged; supported modes return 0. Message ID and
/// fourth argument are ignored. Changing modes does not cancel a pending release.
static s32 _actor317000SetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject*        bodyModel;
    _Actor317000Work* work;
    s32               result;

    bodyModel = task->extra.tmd;
    work      = task->work;
    result    = 0;
    switch (mode) {
        case ACTOR_317000_DRAW_HIDE_AUTO:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_317000_DRAW_SHOW_ALLOCATE:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_317000_DRAW_HIDE_RELEASE:
            bodyModel->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_317000_DRAW_SHOW_MANUAL:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Selects the head-turn weight ramp or prints the actor's root transform.
///
/// Borrows a readable command through dispatch and ignores its context tags.
/// Commands 0 and 1 lower or raise the 1/4096 weight; all other values print root
/// position in coordinate units and Euler rotation in 4096 units per turn.
/// The head-turn routine currently ignores that weight. Requires initialized
/// work and a live root; ignores message ID and fourth argument. Returns 0.
static s32 _actor317000ApplyCommand(Task* task, s32 messageId, const ActorCommand* commandRequest, s32 unusedArg)
{
    enum { ACTOR_317000_COMMAND_LOWER_HEAD_WEIGHT = 0,
           ACTOR_317000_COMMAND_RAISE_HEAD_WEIGHT = 1 };
    _Actor317000Work* work;
    GfxCoord*         rootCoord;
    SVECTOR           rootAngles;
    s32               command;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    command   = commandRequest->command;
    switch (command) {
        case ACTOR_317000_COMMAND_LOWER_HEAD_WEIGHT:
            work->turnWeightRising = 0;
            break;
        case ACTOR_317000_COMMAND_RAISE_HEAD_WEIGHT:
            work->turnWeightRising = command;
            break;
        default:
            GPU_printf("%s=(%d,%d,%d)\n", "pos", rootCoord->coord.t[0], rootCoord->coord.t[1], rootCoord->coord.t[2]);
            gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
            GPU_printf("%s=(%d,%d,%d)\n", "rot", rootAngles.vx, rootAngles.vy, rootAngles.vz);
            break;
    }
    return 0;
}
