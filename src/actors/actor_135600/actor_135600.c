#include "actors/actor_135600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor135600InitializeKyleMadiganWalker(Task* task);
static s32  _actor135600DrawHeldItemQuad(GfxCoord* itemRoot, s32 lengthScale12);
static void _actor135600UpdateKyleMadiganWalker(Task* task);
static void _actor135600IdleKyleMadiganHand(Task* handTask);
static void _actor135600AttachKyleMadiganHeldItem(Task* itemTask);
static void _actor135600UpdateKyleMadiganHeldItem(Task* itemTask);
static void _actor135600ComposeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation);
static void _actor135600ExitKyleMadiganWalker(Task* task);
static void _actor135600BindKyleMadiganWalkerLighting(Task* task);
static void _actor135600IdleKyleMadiganWalk(Task* task);
static void _actor135600RunKyleMadiganWalkStep(Task* task);
static void _actor135600BeginKyleMadiganWalk(Task* task);
static s32  _actor135600SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32  _actor135600IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void _actor135600KyleMadiganHandTask(Task* handTask);
static void _actor135600KyleMadiganHeldItemTask(Task* itemTask);
static void _actor135600KyleMadiganWalkerTask(Task* task);

/// The held-item quad's Q12 length scale and signed screen-angle transition.
enum {
    ACTOR_135600_HELD_QUAD_FULL_LENGTH   = ONE,
    ACTOR_135600_HELD_QUAD_HALF_LENGTH   = ONE / 2,
    ACTOR_135600_HELD_QUAD_SHORTEN_ANGLE = 0x1F5, // 4096 units per turn
};

/// Task-state callbacks shared by Kyle Madigan's two hand models.
///
/// `Task::state` selects 0 attach to the body part, 1 idle under the body rig
/// or 2 kill the hand. Body coordinates and lighting remain borrowed.
static const TaskFuncTable3 _gActor135600KyleMadiganHandStates = { {
    _modelPlacementAttachPartTask,
    _actor135600IdleKyleMadiganHand,
    taskKill,
} };

/// Task-state callbacks for Kyle Madigan's held model and its textured quad.
///
/// `Task::state` selects 0 attach with an offset, 1 update and draw the quad
/// or 2 kill the child. The parent supplies borrowed coordinates and lighting.
static const TaskFuncTable3 _gActor135600KyleMadiganHeldItemStates = { {
    _actor135600AttachKyleMadiganHeldItem,
    _actor135600UpdateKyleMadiganHeldItem,
    taskKill,
} };

/// Kyle Madigan's body lifecycle: 0 initialize, 1 update, 2 release the actor.
///
/// Task state indexes a complete twelve-byte value copy, without a terminator.
/// The body owns its work and attachment tasks after initialization; keep this
/// package loaded through teardown. The update state handles actor freezing.
static const TaskFuncTable3 _gActor135600KyleMadiganWalkerStates = { {
    _actor135600InitializeKyleMadiganWalker,
    _actor135600UpdateKyleMadiganWalker,
    _actor135600ExitKyleMadiganWalker,
} };

/// Kyle Madigan's four scripted-walk steps, copied before dispatch.
///
/// `walk.motionStep` must be in 0..3; the closing turn returns motion to idle.
static const TaskFuncTable4 _gActor135600KyleMadiganWalkSteps = { {
    _actorMotionFaceTarget,           // 0: face the destination
    _actor135600BeginKyleMadiganWalk, // 1: begin forward motion
    _actorMotionArrive,               // 2: stop on arrival
    _actorMotionTurnToYaw,            // 3: face the destination yaw and return to idle
} };

enum { ACTOR_135600_KYLE_WALK_VELOCITY_ONE = 0x10000 };

/// Kyle Madigan's local +Z walk velocity: 32 coordinate units per update in signed 16.16.
///
/// The root's Q12 rotation converts XYZ to parent-space velocity without
/// translation. The fourth VECTOR word is zero and is not part of the motion.
static const VECTOR _gActor135600KyleMadiganForwardWalkVelocity = { 0, 0, 32 * ACTOR_135600_KYLE_WALK_VELOCITY_ONE, 0 };

static AnimationSet _gPierceRestClip;
static AnimationSet _gDryfieldNightSaloonGRKyleSitAtBarClip;
static AnimationSet _gDryfieldNightSaloonGRKyleOfferDrinkClip;
static AnimationSet _gDryfieldNightSaloonGRKyleFinallyProgressClip;
static AnimationSet _gDryfieldNightSaloonGRKyleGetDownToBusinessClip;
static AnimationSet _gDryfieldNightSaloonGRKyleProtestClip;
static AnimationSet _gDryfieldNightSaloonGRKyleTalkStartClip;
static AnimationSet _gDryfieldNightSaloonGRKyleTalkEndClip;
static AnimationSet _gDryfieldNightSaloonGRKyleMadiganAnim09Clip;
static AnimationSet _gDryfieldNightSaloonGRKyleRejectedClip;
static AnimationSet _gDryfieldNightSaloonGRKyleTalkClip;
static AnimationSet _gDryfieldNightSaloonGRKyleListenClip;
static AnimationSet _gDryfieldNightSaloonGRKyleHearAboutCarClip;
static AnimationSet _gDryfieldNightSaloonGRKyleCaughtOffGuardClip;
static AnimationSet _gDryfieldNightSaloonGRKyleGreetClip;
extern TmdSource    gKyleBody;
extern TmdSource    gKyleHandRight;
extern TmdSource    gKyleHandLeft;
static TmdSource    _gActor135600Model06AC4;

#include "assets/characters/kyle/body.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/characters/kyle/hand_left.inc"

static TmdBone _gActor135600Model06AC4Skeleton[1] = {
#include "assets/actor_135600_model_06AC4_skeleton.inc"
};

static u32 _gActor135600Model06AC4PartVerts[1] = {
#include "assets/actor_135600_model_06AC4_partVerts.inc"
};

static SVECTOR _gActor135600Model06AC4Verts[24] = {
#include "assets/actor_135600_model_06AC4_verts.inc"
};

static u32 _gActor135600Model06AC4Stream[129] = {
#include "assets/actor_135600_model_06AC4_stream.inc"
};

static TmdSource _gActor135600Model06AC4 = {
    0,
    928,
    0,
    1,
    _gActor135600Model06AC4PartVerts,
    _gActor135600Model06AC4Verts,
    &_gActor135600Model06AC4Verts[24],
    _gActor135600Model06AC4Skeleton,
    _gActor135600Model06AC4Stream,
};

#include "assets/rooms/dryfield/night_saloon_g_r/kyle_clips.inc"

/// Screen-pixel offsets of the held-item quad's near pair followed by its far pair.
///
/// The near -4/+4 X pair rotates with the projected item axis; the far -3/+3
/// pair stays horizontal. Y, Z and the vector's fourth halfword are zero.
static SVECTOR _gActor135600HeldItemQuadCorners[4] = {
    { -4, 0, 0, 0 },
    { 4, 0, 0, 0 },
    { -3, 0, 0, 0 },
    { 3, 0, 0, 0 },
};

/// Kyle Madigan's bank-0 clip table for his twenty-part body rig.
///
/// Clip IDs 1..15 borrow loaded sets; entry 0 is NULL and cannot be played.
/// The table and animation resources must remain loaded while the rig uses them.
#include "assets/rooms/dryfield/night_saloon_g_r/kyle_bank.inc"

/// Actor-motion bank selection for Kyle's twenty-part body (only source.index 0).
///
/// Requests select loaded clips 1..15. This table and its borrowed sets must
/// remain loaded for playback; attached models use the body's placed coordinates.
static AnimationSet** _gActorMotionAnimBanks[1] = {
    _gActor135600KyleMadiganAnimationBank,
};

TaskDesc gActor135600KyleMadiganTaskDescriptors[4] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor135600KyleMadiganWalkerTask,
        .data     = { .model = &gKyleBody },
    },
    // 1: left hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor135600KyleMadiganHandTask,
        .data     = { .model = &gKyleHandLeft },
    },
    // 2: right hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor135600KyleMadiganHandTask,
        .data     = { .model = &gKyleHandRight },
    },
    // 3: held model.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor135600KyleMadiganHeldItemTask,
        .data     = { .model = &_gActor135600Model06AC4 },
    },
};

/// Kyle Madigan's body messages for animation, placement, drawing and scripted walking.
///
/// Animation borrows a play request; placement and walking borrow a transform,
/// with optional start/closing clips in the walk's second word. Draw mode also
/// updates the hands and held item; actor commands are ignored. Five handlers
/// precede the end marker. The body borrows this table and its loaded callbacks.
static const TaskMessageEntry _gActor135600KyleMadiganWalkerMessages[6] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor135600SetKyleMadiganDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor135600IgnoreKyleMadiganCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Draws the orange quad ahead of the held model and returns its signed screen angle.
///
/// `itemRoot` must have an acyclic parent chain ending at `gGfxViewCoord`.
/// `lengthScale12` is a Q12 length multiplier (4096 full length, 2048 half),
/// placing the endpoints at local Z = 10 and 10 + 70 * lengthScale12 / 4096.
/// World positions narrow to signed halfwords. The returned angle has 4096
/// units per turn, measured from screen up toward screen right.
/// Requires room for a `POLY_F4` and `DR_TPAGE` in the current frame arena and
/// an ordering-table tag two entries before the far point's masked depth tag.
/// Emits packets only when the far projection's depth-cue output is nonnegative;
/// its GTE flags are ignored. Packets remain borrowed until the frame is drawn.
static s32 _actor135600DrawHeldItemQuad(GfxCoord* itemRoot, s32 lengthScale12)
{
    enum {
        ACTOR_135600_HELD_QUAD_ORIGIN_Z    = 10,
        ACTOR_135600_HELD_QUAD_FULL_SPAN   = 70,
        ACTOR_135600_HELD_QUAD_PACKET_CODE = 0x2A, // Semi-transparent flat quad
    };
    SVECTOR   nearWorldPoint;
    SVECTOR   farWorldPoint;
    SVECTOR   itemWorldPosition;
    SVECTOR   screenCorners[4];
    MATRIX    matrix;
    long      nearScreenXY;
    long      depthCue;
    long      projectionFlags;
    long      farScreenXY;
    s16       nearScreenY;
    s16       farScreenY;
    s32       screenAngle;
    u16       nearScreenX;
    u16       farScreenX;
    s32       orderingDepth;
    POLY_F4*  quadPacket;
    DR_TPAGE* drawModePacket;
    s32       pairIndex;

    // Project two points on the held model's forward axis into the current view.
    actorRenderComposeCoord(itemRoot);
    _actor135600ComposeWorldTransform(itemRoot, &matrix, &itemWorldPosition);

    nearWorldPoint.vx = 0;
    nearWorldPoint.vy = 0;
    nearWorldPoint.vz = ACTOR_135600_HELD_QUAD_ORIGIN_Z;
    ApplyMatrixSV(&matrix, &nearWorldPoint, &nearWorldPoint);

    farWorldPoint.vx = 0;
    farWorldPoint.vy = 0;
    farWorldPoint.vz = lengthScale12 * ACTOR_135600_HELD_QUAD_FULL_SPAN / ONE + ACTOR_135600_HELD_QUAD_ORIGIN_Z;
    ApplyMatrixSV(&matrix, &farWorldPoint, &farWorldPoint);

    nearWorldPoint.vx += itemWorldPosition.vx;
    nearWorldPoint.vy += itemWorldPosition.vy;
    nearWorldPoint.vz += itemWorldPosition.vz;
    farWorldPoint.vx  += itemWorldPosition.vx;
    farWorldPoint.vy  += itemWorldPosition.vy;
    farWorldPoint.vz  += itemWorldPosition.vz;

    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);

    RotTransPers(&nearWorldPoint, &nearScreenXY, &depthCue, &projectionFlags);
    orderingDepth = RotTransPers(&farWorldPoint, &farScreenXY, &depthCue, &projectionFlags);

    nearScreenX = (u16)nearScreenXY;
    farScreenX  = (u16)farScreenXY;
    nearScreenY = nearScreenXY >> 16;
    farScreenY  = farScreenXY >> 16;
    screenAngle = ratan2((s16)farScreenXY - (s16)nearScreenXY, nearScreenY - farScreenY);

    // Rotate the near vertex pair in screen space; the far pair stays horizontal.
    gfxSetRotIdentity(&matrix);
    RotMatrixZ(screenAngle, &matrix);

    for (pairIndex = 0; pairIndex < (s32)ARRAY_SIZE(_gActor135600HeldItemQuadCorners) / 2; pairIndex++) {
        ApplyMatrixSV(&matrix, &_gActor135600HeldItemQuadCorners[pairIndex], &screenCorners[pairIndex]);
        screenCorners[pairIndex].vx    += nearScreenX;
        screenCorners[pairIndex].vy    += nearScreenY;
        screenCorners[pairIndex + 2].vx = _gActor135600HeldItemQuadCorners[pairIndex + 2].vx + farScreenX;
        screenCorners[pairIndex + 2].vy = _gActor135600HeldItemQuadCorners[pairIndex + 2].vy + farScreenY;
    }

    if (depthCue >= 0) {
        quadPacket     = gGpuPrimCursor;
        gGpuPrimCursor = quadPacket + 1;
        setlen(quadPacket, sizeof(*quadPacket) / sizeof(u32) - 1);
        setcode(quadPacket, ACTOR_135600_HELD_QUAD_PACKET_CODE);
        setRGB0(quadPacket, 0xFF, 0x40, 0);
        quadPacket->x0 = screenCorners[0].vx;
        quadPacket->y0 = screenCorners[0].vy;
        quadPacket->x1 = screenCorners[1].vx;
        quadPacket->y1 = screenCorners[1].vy;
        quadPacket->x2 = screenCorners[2].vx;
        quadPacket->y2 = screenCorners[2].vy;
        quadPacket->x3 = screenCorners[3].vx;
        quadPacket->y3 = screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(orderingDepth) - 2, quadPacket);

        // addPrim prepends: the draw mode must reach the GPU before the quad.
        drawModePacket = gGpuPrimCursor;
        gGpuPrimCursor = drawModePacket + 1;
        setlen(drawModePacket, sizeof(*drawModePacket) / sizeof(u32) - 1);
        // Quarter-source additive blending, drawing in the display area, no dithering.
        drawModePacket->code[0] = _get_mode(1, 0, getTPage(0, 3, 320, 0));
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(orderingDepth) - 2, drawModePacket);
    }
    return screenAngle;
}

/// Initializes Kyle Madigan's walker, hands and held model at the scene's mark.
///
/// Allocates owned `KyleMadiganWalkerWork`; failure exits the body task. Left
/// and right hands attach at parts 8 and 12 and occupy hand slots 1 and 0 here.
/// The placement owner in spawnArg2 supplies both hands' texture offsets.
/// Starts bank 0, clip 2 at (2670, 0, 1520), yaw 1024 in 4096 units per turn.
/// The reset request's blend duration and collision choice are unread. Setup
/// checks each child spawn but its immediate show requires all three children;
/// that allocation assumption and existing failure behavior are retained.
static void _actor135600InitializeKyleMadiganWalker(Task* task)
{
    enum {
        ACTOR_135600_KYLE_FREE_DISABLED   = -1,
        ACTOR_135600_KYLE_LEFT_HAND_TASK  = 1,
        ACTOR_135600_KYLE_RIGHT_HAND_TASK = 2,
        ACTOR_135600_KYLE_HELD_TASK       = 3,
        ACTOR_135600_KYLE_LEFT_HAND_PART  = 8,
        ACTOR_135600_KYLE_RIGHT_HAND_PART = 12,
        ACTOR_135600_KYLE_INITIAL_CLIP    = 2,
    };
    KyleMadiganWalkerWork* work;
    Task*                  childTask;
    ActorTransform         placement;
    AnimationPlayRequest   initialAnimation;

    work = memCalloc(sizeof(KyleMadiganWalkerWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_135600_KYLE_FREE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    childTask = taskSpawnFromTable(gActor135600KyleMadiganTaskDescriptors, ACTOR_135600_KYLE_LEFT_HAND_TASK, ACTOR_135600_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        work->handTasks[1] = childTask;
        _actorRenderApplyPlacementTextureOffsets(childTask->extra.tmd, task->spawnArg2.pointer);
    }

    childTask = taskSpawnFromTable(gActor135600KyleMadiganTaskDescriptors, ACTOR_135600_KYLE_RIGHT_HAND_TASK, ACTOR_135600_KYLE_RIGHT_HAND_PART, task);
    if (childTask != NULL) {
        work->handTasks[0] = childTask;
        _actorRenderApplyPlacementTextureOffsets(childTask->extra.tmd, task->spawnArg2.pointer);
    }

    childTask = taskSpawnFromTable(gActor135600KyleMadiganTaskDescriptors, ACTOR_135600_KYLE_HELD_TASK, ACTOR_135600_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        work->heldItemTask = childTask;
    }

    _actor135600BindKyleMadiganWalkerLighting(task);

    /// Places Kyle and resets bank 0, clip 2 before any draw-mode propagation.
    ///
    /// Arguments are side-effect-free Task*, ActorTransform and play-request
    /// lvalues, evaluated repeatedly. Captures the local initial-clip constant.
    /// Writes only consumed fields; reset playback ignores duration/collision.
#define ACTOR_135600_PLACE_INITIAL_KYLE_POSE(task, placement, initialAnimation)             \
    {                                                                                       \
        (placement).pos.vx = 0xA6E;                                                         \
        (placement).pos.vz = 0x5F0;                                                         \
        (placement).pos.vy = 0;                                                             \
        (placement).rot.vx = 0;                                                             \
        (placement).rot.vy = ACTOR_TRANSFORM_ANGLE_TURN / 4;                                \
        (placement).rot.vz = 0;                                                             \
        actorMsgPlaceEuler((task), ACTOR_MESSAGE_PLACE, &(placement), 0);                   \
                                                                                            \
        (initialAnimation).source.index = 0;                                                \
        (initialAnimation).animationId  = ACTOR_135600_KYLE_INITIAL_CLIP;                   \
        (initialAnimation).blend        = ANIMATION_BLEND_RESET;                            \
        _actorMotionPlayAnim((task), ACTOR_MESSAGE_PLAY_ANIMATION, &(initialAnimation), 0); \
    }

    // Place the body and seed its first pose before showing all attachments.
    ACTOR_135600_PLACE_INITIAL_KYLE_POSE(task, placement, initialAnimation);

    _actor135600SetKyleMadiganDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);

    task->msgTable     = _gActor135600KyleMadiganWalkerMessages;
    task->exitCallback = _actor135600ExitKyleMadiganWalker;
    task->state       += 1;
}
#undef ACTOR_135600_PLACE_INITIAL_KYLE_POSE

/// Applies one frame's signed 16.16 walk velocity, retaining the fractional carry.
///
/// `work` and `rootCoord` must be live and writable. Invalidates the root's
/// cached composition even for zero velocity; integer additions retain the
/// target's 32-bit arithmetic and the remaining fractions are zero-extended.
static inline void _actor135600IntegrateWalkVelocity(KyleMadiganWalkerWork* work, GfxCoord* rootCoord)
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

/// Updates Kyle's scripted walk, twenty-slot animation rig, lighting and buffer release.
///
/// Requires a live TMD body and `KyleMadiganWalkerWork`; `walk.motion` is 0
/// (idle) or 1 (walking). A visible body's ground shadow is drawn even while
/// actor updates are frozen. Motion, slots 1..19, view-ready lighting and the
/// buffer-release counter advance only while scene actors are running.
/// The update finding `freeCountdown == 0` frees the body primitive buffer,
/// then decrements the counter to its disabled value of -1.
static void _actor135600UpdateKyleMadiganWalker(Task* task)
{
    enum { ACTOR_135600_KYLE_SHADOW_HALF_SIZE = 0x300 }; // World-coordinate units
    TmdObject*             bodyModel         = task->extra.tmd;
    KyleMadiganWalkerWork* work              = task->work;
    TaskFunc               motionHandlers[2] = { _actor135600IdleKyleMadiganWalk, _actor135600RunKyleMadiganWalkStep };
    VECTOR3                groundPosition;
    GfxCoord*              rootCoord;
    s32                    slotIndex;

    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_135600_KYLE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        motionHandlers[work->walk.motion](task);
        rootCoord = task->extra.tmd->coords;
        _actor135600IntegrateWalkVelocity(work, rootCoord);
        if (work->model.ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationTickSlot(&work->rig.anim, slotIndex);
            }
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                tmdFreePrimitiveBuffer(bodyModel);
            }
            work->freeCountdown--;
        }
    }
}

#include "../../shared/actor/actor_motion_arrive.inc.c"

#include "../../shared/actor/actor_motion_start.inc.c"

/// Runs a Kyle hand model's attachment, idle or teardown state.
///
/// Requires a live TMD task with state 0..2. Setup borrows the parent task
/// from `spawnArg2.pointer` and its part index from `spawnArg1.value` (8 for
/// the left hand, 12 for the right); both parent model and lighting must outlive
/// the attached hand. The body rig drives the hand's inherited transform.
static void _actor135600KyleMadiganHandTask(Task* handTask)
{
    TaskFuncTable3 states;

    states = _gActor135600KyleMadiganHandStates;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Leaves an attached Kyle hand idle while its parent rig drives the transform.
static void _actor135600IdleKyleMadiganHand(Task* handTask)
{
}

/// Runs Kyle's held model through attachment, quad drawing and teardown.
///
/// Requires a live TMD task with state 0..2; setup's parent task and part-index
/// spawn arguments must remain valid. The attached model and its quad share
/// the parent's transform and lighting lifetime.
static void _actor135600KyleMadiganHeldItemTask(Task* itemTask)
{
    TaskFuncTable3 states;

    states = _gActor135600KyleMadiganHeldItemStates;
    states.funcs[itemTask->state](itemTask);
}

/// Attaches Kyle's held model to a body part and initializes its quad length.
///
/// Requires live child and parent TMD tasks: `spawnArg2.pointer` borrows the
/// parent, and `spawnArg1.value` is a valid coordinate index (setup supplies 8).
/// Sets the root's local translation to (-150, 80, 0), appends a quarter turn
/// about Y, and borrows the parent's part and lighting matrices until teardown.
/// Joins the parent's child-task ring and advances to state 1. While this
/// callback is active, `killCountdown` stores a Q12 length scale, not a timer.
static void _actor135600AttachKyleMadiganHeldItem(Task* itemTask)
{
    enum { ACTOR_135600_HELD_ITEM_ATTACH_YAW = 0x400 }; // Quarter turn in 4096-angle units
    MATRIX     attachmentRotation;
    MATRIX*    rotationMatrix;
    Task*      parentTask;
    s32        parentPartIndex;
    TmdObject* itemModel;
    TmdObject* parentModel;
    GfxCoord*  itemRoot;
    GfxCoord*  parentPart;

    parentTask      = itemTask->spawnArg2.pointer;
    itemModel       = itemTask->extra.tmd;
    parentPartIndex = itemTask->spawnArg1.value;
    parentModel     = parentTask->extra.tmd;
    itemRoot        = itemModel->coords;
    parentPart      = &parentModel->coords[parentPartIndex];

    itemRoot->coord.t[0] = -0x96;
    itemRoot->coord.t[1] = 0x50;
    itemRoot->coord.t[2] = 0;

    rotationMatrix = &attachmentRotation;
    gfxSetRotIdentity(rotationMatrix);
    RotMatrixY(ACTOR_135600_HELD_ITEM_ATTACH_YAW, rotationMatrix);
    MulMatrix0(&itemRoot->coord, rotationMatrix, &itemRoot->coord);

    // Borrow the body's resources and join its teardown tree.
    itemRoot->parent       = parentPart;
    itemRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    itemModel->lightMtx    = parentModel->lightMtx;
    itemModel->colorMtx    = parentModel->colorMtx;
    itemModel->otOffset    = 0;
    taskReparent(parentTask, itemTask);
    itemTask->killCountdown = ACTOR_135600_HELD_QUAD_FULL_LENGTH;
    itemTask->state        += 1;
}

/// Draws the held-item quad during script events, shortening it at a screen-angle threshold.
///
/// Requires an attached live TMD task. `killCountdown` holds the positive Q12
/// length multiplier: setup supplies 4096, and a signed screen angle at least
/// 501 replaces it with 2048. Neither this callback nor the task scheduler
/// decrements that storage while the held-item callbacks are installed.
/// Outside a script event, or for a nonpositive scale, no quad is drawn.
static void _actor135600UpdateKyleMadiganHeldItem(Task* itemTask)
{
    s16 lengthScale12;

    if (gGameSession->eventState != 0) {
        lengthScale12 = itemTask->killCountdown;
        if (lengthScale12 > 0 && _actor135600DrawHeldItemQuad(itemTask->extra.tmd->coords, lengthScale12) >= ACTOR_135600_HELD_QUAD_SHORTEN_ANGLE) {
            itemTask->killCountdown = ACTOR_135600_HELD_QUAD_HALF_LENGTH;
        }
    }
}

/// Composes a model node's world rotation and halfword translation, excluding the view.
///
/// `node` must be a non-world node in an acyclic chain ending at `gGfxViewCoord`.
/// Outputs must be writable and separate from the input nodes, with the matrix
/// word-aligned. Writes only the
/// Q12 rotation coefficients and the translation vector's xyz halfwords; other
/// output bytes are untouched. Each local translation narrows to a signed
/// halfword before rotation, and each addition narrows again. No cache changes.
static void _actor135600ComposeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation)
{
    SVECTOR localTranslation;

    if (node->parent != &gGfxViewCoord) {
        _actor135600ComposeWorldTransform(node->parent, worldRotation, worldTranslation);
    } else {
        gfxSetRotIdentity(worldRotation);
        worldTranslation->vx = 0;
        worldTranslation->vy = 0;
        worldTranslation->vz = 0;
    }

    // Retain the signed-halfword translation at every level of the hierarchy.
    localTranslation.vx = (u16)node->coord.t[0];
    localTranslation.vy = (u16)node->coord.t[1];
    localTranslation.vz = (u16)node->coord.t[2];
    ApplyMatrixSV(worldRotation, &localTranslation, &localTranslation);
    worldTranslation->vx += localTranslation.vx;
    worldTranslation->vy += localTranslation.vy;
    worldTranslation->vz += localTranslation.vz;
    MulMatrix0(worldRotation, (MATRIX*)&node->coord, worldRotation);
}

/// Runs Kyle's walker through allocation, per-frame update and teardown.
///
/// Requires a live TMD task with state 0..2. Dispatch itself remains active
/// while scene actors are frozen; the update state applies the freeze gate.
static void _actor135600KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor135600KyleMadiganWalkerStates;
    states.funcs[task->state](task);
}

/// Releases Kyle's enemy record, child tasks and work, then requests body teardown.
///
/// Requires a live primary-heap `Enemy` in `spawnArg2.pointer`. Both the exit
/// state and exit callback use `enemyTaskExit`'s ownership contract: normal TMD
/// teardown is deferred, or immediate when the display requests it. Neither
/// the task nor the enemy may be accessed again after this call.
static void _actor135600ExitKyleMadiganWalker(Task* task)
{
    enemyTaskExit(task);
}

/// Lends Kyle's work-owned light and colour matrices to his body model.
///
/// Requires a live TMD body and `KyleMadiganWalkerWork`. The work block must
/// outlive all model and attached-child uses of these borrowed pointers.
static void _actor135600BindKyleMadiganWalkerLighting(Task* task)
{
    TmdObject*             bodyModel;
    KyleMadiganWalkerWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves Kyle's motion idle between scripted walks.
static void _actor135600IdleKyleMadiganWalk(Task* task)
{
}

/// Runs the current face, begin-walk, arrival or closing-turn step of Kyle's walk.
///
/// Requires live `KyleMadiganWalkerWork` with `walk.motionStep` in 0..3.
/// The arrival step stops velocity; the closing turn returns motion to idle.
static void _actor135600RunKyleMadiganWalkStep(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = _gActor135600KyleMadiganWalkSteps;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor/actor_motion_face.inc.c"

/// Starts Kyle moving along his local +Z axis and advances to the arrival step.
///
/// Requires a live TMD root and `KyleMadiganWalkerWork` in motion step 1.
/// The Q12 root rotation transforms a 32-unit-per-frame 16.16 velocity into
/// the parent's frame. Seeds the previous-distance sentinels so the first
/// arrival check records its X/Z distances, then advances to step 2.
static void _actor135600BeginKyleMadiganWalk(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              rootCoord;
    VECTOR                 localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = _gActor135600KyleMadiganForwardWalkVelocity;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

#include "../../shared/actor/actor_motion_turn.inc.c"

#include "../../shared/actor/actor_motion_play.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets Kyle's body draw mode and propagates the complete flags to both hands and held model.
///
/// Requires live body and all three child TMD tasks in `KyleMadiganWalkerWork`.
/// Modes 0/1 hide/show and enable automatic buffers; showing allocates a body
/// primitive buffer if needed. Mode 2 hides, disables automatic buffers and
/// arms body-buffer release with counter 2. Mode 3 shows with automatic buffers
/// disabled. Modes 0, 1 and 3 leave any pending release unchanged. Other modes retain the
/// body flags; all modes copy those flags to the children. Returns 0 for modes
/// 0..3, otherwise 1. The message ID and second payload are ignored.
/// These flags govern the TMD models; the held-item quad has its own event gate.
static s32 _actor135600SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum {
        ACTOR_135600_KYLE_BUFFER_RELEASE_DELAY = 2,
    };
    KyleMadiganWalkerWork* work;
    TmdObject*             bodyModel;
    TmdObject*             rightHandModel;
    TmdObject*             leftHandModel;
    TmdObject*             heldItemModel;
    s32                    result;

    work           = task->work;
    bodyModel      = task->extra.tmd;
    leftHandModel  = work->handTasks[1]->extra.tmd;
    rightHandModel = work->handTasks[0]->extra.tmd;
    heldItemModel  = work->heldItemTask->extra.tmd;
    result         = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            bodyModel->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = ACTOR_135600_KYLE_BUFFER_RELEASE_DELAY;
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
    leftHandModel->flags  = bodyModel->flags;
    rightHandModel->flags = bodyModel->flags;
    heldItemModel->flags  = bodyModel->flags;
    return result;
}

/// Accepts an actor command without changing Kyle's state and returns zero.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; ignores the receiver, message
/// ID and both payload words, retaining no borrowed command data.
static s32 _actor135600IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
