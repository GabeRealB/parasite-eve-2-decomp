#include "stages/akropolis/actors/square_grinning_stranger.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "stages/akropolis/rooms/square.h"

#include "assets/rooms/akropolis/square/grinning_stranger_schema.h"
#include "../../../../shared/actor/actor_contacts.h"

/// Steps of `_Work::sequenceStep`, in the order the sequence runs them.
enum {
    STEP_START      = 0, // Cross-fades the body to the react clip over 15 frames
    STEP_TURN       = 1, // Lowers `headYaw` and `headPitch` by `TURN_STEP` a tick each, until both are below -0x154
    STEP_TURN_HOLD  = 2, // Holds for 31 ticks
    STEP_SWING      = 3, // Raises `headYaw` by `SWING_STEP` a tick until it has reached 0x2AA
    STEP_SWING_HOLD = 4, // Holds for 16 ticks, then cross-fades the body to the run-off clip over 10 frames
    STEP_SET_WAIT   = 5, // Waits two ticks
    STEP_DEPART     = 6  // Moves the model's root 0x96 along -Z every tick
};

/// Work block of the Grinning Stranger the Akropolis square stages: a model with no collision that waits for the
/// player, runs one scripted sequence and leaves.
/// It idles until the player crosses the square's trigger line, then runs `sequenceStep`; on top of the animation the
/// head turns by `headPitch` about its X axis, then `headYaw` about the world's Y axis (4096ths of a turn).
typedef struct {
    ActorAnimRig19 rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         light;            // Light-direction matrix lent to the model object
    MATRIX         color;            // Light-colour matrix lent to the model object; rebuilt each tick from the lights at part 1 and scaled to a third
    Task*          playerTask;       // Player task registered when the block was set up; never read
    MATRIX*        playerMtx;        // Borrowed player root coordinate matrix (`gPlayerStatus.coordMtx`); its translation is tested against the trigger line
    u16            sequenceStep;     // `STEP_*` of the scripted sequence; reset to its first step as the player triggers it
    byte           field_486[0x2];   // Never accessed; role unproven
    u16            stepFrames;       // Ticks counted by the step in progress: its two holds and its two-tick wait count it up from 0
    byte           field_48A[0x2];   // Never accessed; role unproven
    s16            headYaw;          // Turn of the head (part 5) about the world's Y axis: 0 from setup, lowered to -0x160 and then raised to 0x320 by the sequence
    byte           field_48E[0x4];   // Never accessed; role unproven
    s16            slot1RecordIndex; // Keyframe record slot 1 was on after the last tick, cleared by each cross-fade; never read, purpose unproven
    s16            headPitch;        // Turn of the head (part 5) about its X axis: 0x155 from setup, lowered to -0x15B by the sequence
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x498);

static TmdSource _gStrangerGrinningBody;
static void      _squareGrinningStrangerTask(Task* task);

#include "assets/enemies/stranger/grinning/body.inc"

#include "assets/rooms/akropolis/square/grinning_stranger_clips.inc"

/// Grinning Stranger body clips for its staged turn and departure sequence.
#include "assets/rooms/akropolis/square/grinning_stranger_bank.inc"

TaskDesc gSquareGrinningStrangerTaskDesc = {
    .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
    .callback = _squareGrinningStrangerTask,
    .data     = { .model = &_gStrangerGrinningBody },
};

static inline void _squareGrinningStrangerTickAnim(Task* task);
static inline void _squareGrinningStrangerBlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames);
static void        _squareGrinningStrangerRunSequence(Task* task);
static void        _squareGrinningStrangerInitBody(Task* task);

#include "../../../../shared/actor/actor_contacts_turn_joint.inc.c"

/// Ticks the eighteen non-root body tracks and records slot 1's keyframe index.
static inline void _squareGrinningStrangerTickAnim(Task* task)
{
    _Work* work = task->work;
    u16    slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->slot1RecordIndex = work->rig.slots[1].currentPose.indices.recordIndex;
}

/// Captures the current body poses and blends tracks 1..18 to a selected set's start.
#define BLEND_BODY_SLOTS(work, animationId, blendFrames)                                               \
    do {                                                                                               \
        u16 slotIndex;                                                                                 \
        for (slotIndex = 1; slotIndex < ARRAY_SIZE((work)->rig.slots); slotIndex++) {                  \
            animationSeekSlotWithBlend(&(work)->rig.anim, slotIndex, (animationId), 0, (blendFrames)); \
        }                                                                                              \
    } while (0)

/// Blends the eighteen non-root body tracks to the selected animation's start.
static inline void _squareGrinningStrangerBlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames)
{
    _Work* work = task->work;

    work->slot1RecordIndex = 0;
    BLEND_BODY_SLOTS(work, animationId, blendFrames);
}
#undef BLEND_BODY_SLOTS

/// Ticks the body animation and advances the staged Stranger sequence.
static void _squareGrinningStrangerRunSequence(Task* task)
{
    enum {
        START_BLEND_FRAMES  = 15,
        DEPART_BLEND_FRAMES = 10,
        LOWER_ANGLE_LIMIT   = -340,
        SWING_ANGLE_LIMIT   = 682,
        TURN_STEP           = 0x10,
        SWING_STEP          = 0x80,
        TURN_HOLD_UPDATES   = 31,
        SWING_HOLD_UPDATES  = 16,
        SET_WAIT_UPDATES    = 2,
        DEPART_STEP_UNITS   = 150,
    };

    _Work*    work;
    GfxCoord* rootCoord;
    s32       yawLowered;
    s32       pitchLowered;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _squareGrinningStrangerTickAnim(task);
    switch (work->sequenceStep) {
        case STEP_START:
            _squareGrinningStrangerBlendBodyAnimation(task, GRINNING_STRANGER_ANIMATION_REACT, START_BLEND_FRAMES);
            work->stepFrames = 0;
            work->sequenceStep++;
            break;
        case STEP_TURN:
            yawLowered   = 0;
            pitchLowered = 0;
            if (work->headYaw >= LOWER_ANGLE_LIMIT) {
                work->headYaw -= TURN_STEP;
            } else {
                yawLowered = 1;
            }
            if (work->headPitch >= LOWER_ANGLE_LIMIT) {
                work->headPitch -= TURN_STEP;
            } else {
                pitchLowered = 1;
            }
            if (pitchLowered & yawLowered) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case STEP_TURN_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= TURN_HOLD_UPDATES) {
                work->sequenceStep++;
            }
            break;
        case STEP_SWING:
            if (work->headYaw < SWING_ANGLE_LIMIT) {
                work->headYaw += SWING_STEP;
            } else {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case STEP_SWING_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= SWING_HOLD_UPDATES) {
                _squareGrinningStrangerBlendBodyAnimation(task, GRINNING_STRANGER_ANIMATION_RUN_OFF, DEPART_BLEND_FRAMES);
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case STEP_SET_WAIT:
            work->stepFrames++;
            if (work->stepFrames >= SET_WAIT_UPDATES) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case STEP_DEPART:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            rootCoord->coord.t[2]  -= DEPART_STEP_UNITS;
            break;
    }
}

/// Restarts the eighteen body tracks at the idle clip's normal playback rate.
static inline void _squareGrinningStrangerStartIdleTracks(Task* task)
{
    _Work* work;
    s32    slotIndex;

    slotIndex              = 1;
    work                   = task->work;
    work->slot1RecordIndex = 0;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, GRINNING_STRANGER_ANIMATION_IDLE);
        slotIndex += 1;
    } while ((u32)((u16)slotIndex) < ARRAY_SIZE(work->rig.slots));
}

/// Initializes the staged Stranger body and its eighteen non-root animation tracks.
static void _squareGrinningStrangerInitBody(Task* task)
{
    enum {
        INITIAL_HEAD_PITCH = 341,
        PLACEMENT_ENTRY_ID = 19, // The square's placement of this actor, whose texture offsets it takes
    };

    _Work*         work;
    TmdObject*     model;
    GfxCoord*      rootCoord;
    AreaPlacement* place;

    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    task->work = memCalloc(sizeof(_Work), false);
    if (task->work == NULL) {
        taskKill(task);
        return;
    }
    work = task->work;
    memFillBytes(work, 0U, sizeof(*work));
    rootCoord->parent = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, _gSquareGrinningStrangerAnimationSets, model, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerMtx  = gPlayerStatus.coordMtx;
    _squareGrinningStrangerStartIdleTracks(task);
    work->headPitch = INITIAL_HEAD_PITCH;
    place           = areaGetVariant(&gGameSession->location)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != PLACEMENT_ENTRY_ID) {
        place++;
    }
    tmdSetTextureOffsets(model, place->texturePageOffset, place->clutRowOffset);
}

/// Stages the Grinning Stranger's trigger, scripted departure and joint lighting.
static void _squareGrinningStrangerTask(Task* task)
{
    enum {
        TASK_INITIALIZE       = 0,
        TASK_IDLE             = 1,
        TASK_SEQUENCE         = 2,
        HEAD_PART             = 5,
        PELVIS_PART           = 1,
        LIGHT_COUNT           = 3,
        COLOR_SCALE_ONE_THIRD = ONE / 3,
        TRIGGER_NEAR_X        = 1501,
        TRIGGER_FAR_X         = 3201,
        TRIGGER_NEAR_Z        = -1299,
        TRIGGER_FAR_Z         = -1300,
    };

    // Reuse one 32-byte frame slot for successive rotation, position and scale phases.
    union {
        MATRIX rotation;     // Joint rotation composed above the view coordinate
        VECTOR viewPosition; // Part 1 cached view-space position for lighting
        VECTOR colorScale;   // Three Q12 components applied to the colour matrix
    } frameScratch;
    _Work*     work;
    _Work*     lightingWork;
    TmdObject* bodyModel;
    TmdObject* lightingModel;
    GfxCoord*  bodyCoords;
    GfxCoord*  headPart;
    MATRIX*    playerMtx;
    s32        state;
    s32        playerX;
    s16        partPitch;

    state = task->state;
    work  = task->work;
    switch (state) {
        case TASK_INITIALIZE:
            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                _squareGrinningStrangerInitBody(task);
                task->state += 1;
                break;
            }
            return;
        case TASK_IDLE:
            _squareGrinningStrangerTickAnim(task);
            playerMtx = work->playerMtx;
            playerX   = playerMtx->t[0];
            if ((playerX >= TRIGGER_NEAR_X && playerMtx->t[2] >= TRIGGER_NEAR_Z) || (playerX >= TRIGGER_FAR_X && playerMtx->t[2] < TRIGGER_FAR_Z)) {
                work->sequenceStep = STEP_START;
                akropolisSquareStartTelephoneScene(playerX);
                task->state += 1;
            }
            break;
        case TASK_SEQUENCE:
            _squareGrinningStrangerRunSequence(task);
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
    // Apply the sequence's pitch and world yaw after the animation pose.
    bodyModel  = task->extra.tmd;
    partPitch  = work->headPitch;
    bodyCoords = bodyModel->coords;
    headPart   = bodyCoords + HEAD_PART;
    _actorRenderAccumulateRotation(headPart, &frameScratch.rotation, &gGfxViewCoord);
    RotMatrixX(partPitch, &frameScratch.rotation);
    _actorRenderLocalizeRotation(headPart, &frameScratch.rotation);
    memCopyBytes(frameScratch.rotation.m, headPart->coord.m, sizeof(frameScratch.rotation.m));
    headPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(headPart);
    _actorRenderYawJointInWorld(task->extra.tmd->coords + HEAD_PART, work->headYaw);
    // Relight the pose, then apply the truncated one-third colour scale.
    lightingModel                = task->extra.tmd;
    lightingWork                 = task->work;
    frameScratch.viewPosition.vx = lightingModel->coords[PELVIS_PART].workm.t[0];
    frameScratch.viewPosition.vy = task->extra.tmd->coords[PELVIS_PART].workm.t[1];
    frameScratch.viewPosition.vz = task->extra.tmd->coords[PELVIS_PART].workm.t[2];
    worldCoordSetModelLighting(lightingModel, &frameScratch.viewPosition, 0, LIGHT_COUNT);
    frameScratch.colorScale.vz = COLOR_SCALE_ONE_THIRD;
    frameScratch.colorScale.vy = COLOR_SCALE_ONE_THIRD;
    frameScratch.colorScale.vx = COLOR_SCALE_ONE_THIRD;
    ScaleMatrix(&lightingWork->color, &frameScratch.colorScale);
}
