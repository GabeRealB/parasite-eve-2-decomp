#include "actors/task_tables.h"

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
#include "../../../../shared/actor/actor_contacts.h"

/// Steps of `_Work::sequenceStep`, in the order the sequence runs
/// them.
///
/// Each step advances to the next by incrementing the field; the last one
/// repeats until the task is killed.
enum {
    STEP_START      = 0, // Cross-fades the body slots to animation set 0 over 15 frames
    STEP_TURN       = 1, // Lowers `part5Yaw` and `part5Pitch` by 0x10 a tick each, until both are below -0x154
    STEP_TURN_HOLD  = 2, // Holds for 31 ticks
    STEP_SWING      = 3, // Raises `part5Yaw` by 0x80 a tick until it has reached 0x2AA
    STEP_SWING_HOLD = 4, // Holds for 16 ticks, then cross-fades the body slots to animation set 2 over 10 frames
    STEP_SET_WAIT   = 5, // Waits two ticks
    STEP_DEPART     = 6  // Moves the model's root 0x96 along -Z every tick
};

/// Work block of the Grinning Stranger that Acropolis Square stages: a model
/// with no collision that waits for the player, runs one scripted sequence
/// and leaves.
///
/// The task's setup allocates it zeroed and keeps it at `Task::work` for the
/// task's life. The model object borrows `light` and `color` for as long as
/// the block lives. Slots 1 to 18 of the rig are driven; slot 0, the root's,
/// is never started.
///
/// The actor idles on animation set 5 until the player's position crosses the
/// room's trigger line, then runs `sequenceStep` from its first step. Model
/// part 5 is turned on top of the animation every tick, by `part5Pitch` about
/// its X axis and then `part5Yaw` about the world's Y axis. Angles are 4096ths
/// of a turn.
typedef struct {
    ActorAnimRig19 rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         light;            // Light-direction matrix lent to the model object
    MATRIX         color;            // Light-colour matrix lent to the model object; rebuilt each tick from the lights at part 1 and scaled to a third
    Task*          playerTask;       // Player task registered when the block was set up; never read
    MATRIX*        playerMtx;        // Borrowed player root coordinate matrix (`gPlayerStatus.coordMtx`); its translation is tested against the trigger line
    u16            sequenceStep;     // `SQUARE_GRINNING_STRANGER_STEP_*` of the scripted sequence; reset to its first step as the player triggers it
    byte           field_486[0x2];   // Never accessed; role unproven
    u16            stepFrames;       // Ticks counted by the step in progress: its two holds and its two-tick wait count it up from 0
    byte           field_48A[0x2];   // Never accessed; role unproven
    s16            part5Yaw;         // Turn of model part 5 about the world's Y axis: 0 from setup, lowered to -0x160 and then raised to 0x320 by the sequence
    byte           field_48E[0x4];   // Never accessed; role unproven
    s16            slot1RecordIndex; // Keyframe record slot 1 was on after the last tick, cleared by each cross-fade; never read, purpose unproven
    s16            part5Pitch;       // Turn of model part 5 about its X axis: 0x155 from setup, lowered to -0x15B by the sequence
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x498);

static TmdSource _gStrangerGrinningBody;
static void      _squareGrinningStrangerGrinningStrangerTask(Task* task);

#include "assets/enemies/stranger/grinning/body.inc"

static AnimationPackedPose _gSquareGrinningStrangerAnimation065FCBank1[4] = {
#include "assets/square_grinning_stranger_animation_065FC_bank1.inc"
};

static AnimationPackedRotation _gSquareGrinningStrangerAnimation065FCBank4[69] = {
#include "assets/square_grinning_stranger_animation_065FC_bank4.inc"
};

static AnimationRecord _gSquareGrinningStrangerAnimation065FCRecords[162] = {
#include "assets/square_grinning_stranger_animation_065FC_records.inc"
};

static u16 _gSquareGrinningStrangerAnimation065FCIndices[20] = {
#include "assets/square_grinning_stranger_animation_065FC_indices.inc"
};

static AnimationSet _gSquareGrinningStrangerAnimation065FC = {
    _gSquareGrinningStrangerAnimation065FCRecords,
    _gSquareGrinningStrangerAnimation065FCIndices,
    { NULL, _gSquareGrinningStrangerAnimation065FCBank1, NULL, NULL, _gSquareGrinningStrangerAnimation065FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSquareGrinningStrangerAnimation07120Bank1[28] = {
#include "assets/square_grinning_stranger_animation_07120_bank1.inc"
};

static AnimationPackedRotation _gSquareGrinningStrangerAnimation07120Bank4[247] = {
#include "assets/square_grinning_stranger_animation_07120_bank4.inc"
};

static AnimationRecord _gSquareGrinningStrangerAnimation07120Records[362] = {
#include "assets/square_grinning_stranger_animation_07120_records.inc"
};

static u16 _gSquareGrinningStrangerAnimation07120Indices[20] = {
#include "assets/square_grinning_stranger_animation_07120_indices.inc"
};

static AnimationSet _gSquareGrinningStrangerAnimation07120 = {
    _gSquareGrinningStrangerAnimation07120Records,
    _gSquareGrinningStrangerAnimation07120Indices,
    { NULL, _gSquareGrinningStrangerAnimation07120Bank1, NULL, NULL, _gSquareGrinningStrangerAnimation07120Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSquareGrinningStrangerAnimation07BA4Bank1[32] = {
#include "assets/square_grinning_stranger_animation_07BA4_bank1.inc"
};

static AnimationPackedRotation _gSquareGrinningStrangerAnimation07BA4Bank4[223] = {
#include "assets/square_grinning_stranger_animation_07BA4_bank4.inc"
};

static AnimationRecord _gSquareGrinningStrangerAnimation07BA4Records[334] = {
#include "assets/square_grinning_stranger_animation_07BA4_records.inc"
};

static u16 _gSquareGrinningStrangerAnimation07BA4Indices[20] = {
#include "assets/square_grinning_stranger_animation_07BA4_indices.inc"
};

static AnimationSet _gSquareGrinningStrangerAnimation07BA4 = {
    _gSquareGrinningStrangerAnimation07BA4Records,
    _gSquareGrinningStrangerAnimation07BA4Indices,
    { NULL, _gSquareGrinningStrangerAnimation07BA4Bank1, NULL, NULL, _gSquareGrinningStrangerAnimation07BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSquareGrinningStrangerAnimation07FACBank1[6] = {
#include "assets/square_grinning_stranger_animation_07FAC_bank1.inc"
};

static AnimationPackedRotation _gSquareGrinningStrangerAnimation07FACBank4[70] = {
#include "assets/square_grinning_stranger_animation_07FAC_bank4.inc"
};

static AnimationRecord _gSquareGrinningStrangerAnimation07FACRecords[150] = {
#include "assets/square_grinning_stranger_animation_07FAC_records.inc"
};

static u16 _gSquareGrinningStrangerAnimation07FACIndices[20] = {
#include "assets/square_grinning_stranger_animation_07FAC_indices.inc"
};

static AnimationSet _gSquareGrinningStrangerAnimation07FAC = {
    _gSquareGrinningStrangerAnimation07FACRecords,
    _gSquareGrinningStrangerAnimation07FACIndices,
    { NULL, _gSquareGrinningStrangerAnimation07FACBank1, NULL, NULL, _gSquareGrinningStrangerAnimation07FACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSquareGrinningStrangerAnimation08600Bank1[18] = {
#include "assets/square_grinning_stranger_animation_08600_bank1.inc"
};

static AnimationPackedRotation _gSquareGrinningStrangerAnimation08600Bank4[146] = {
#include "assets/square_grinning_stranger_animation_08600_bank4.inc"
};

static AnimationRecord _gSquareGrinningStrangerAnimation08600Records[185] = {
#include "assets/square_grinning_stranger_animation_08600_records.inc"
};

static u16 _gSquareGrinningStrangerAnimation08600Indices[20] = {
#include "assets/square_grinning_stranger_animation_08600_indices.inc"
};

static AnimationSet _gSquareGrinningStrangerAnimation08600 = {
    _gSquareGrinningStrangerAnimation08600Records,
    _gSquareGrinningStrangerAnimation08600Indices,
    { NULL, _gSquareGrinningStrangerAnimation08600Bank1, NULL, NULL, _gSquareGrinningStrangerAnimation08600Bank4, NULL, NULL, NULL },
};

/// Grinning Stranger body clips for its staged turn and departure sequence.
///
/// Loaded entries are 0, 1, 2, 5 and 6; 3, 4 and 7 are unplayable NULL holes.
/// Setup starts 5, the sequence blends to 0 and then 2. Sets and tracks remain
/// borrowed by the body rig while the overlay is loaded.
static AnimationSet* _gSquareGrinningStrangerGrinningStrangerAnimationSets[8] = {
    &_gSquareGrinningStrangerAnimation065FC,
    &_gSquareGrinningStrangerAnimation07120,
    &_gSquareGrinningStrangerAnimation07BA4,
    NULL,
    NULL,
    &_gSquareGrinningStrangerAnimation07FAC,
    &_gSquareGrinningStrangerAnimation08600,
    NULL,
};

TaskDesc gSquareGrinningStrangerGrinningStrangerTaskDesc = {
    .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
    .callback = _squareGrinningStrangerGrinningStrangerTask,
    .data     = { .model = &_gStrangerGrinningBody },
};

static inline void _squareGrinningStrangerTickAnim(Task* task);
static inline void _squareGrinningStrangerBlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames);
static void        _squareGrinningStrangerRunSequence(Task* task);
static void        _squareGrinningStrangerInitBody(Task* task);

#include "../../../../shared/actor/actor_contacts_turn_joint.inc.c"

/// Ticks the eighteen non-root body tracks and records slot 1's keyframe index.
///
/// Requires initialized nineteen-part rig storage and live clip data. Each slot
/// advances at its existing rate and writes its pose; slot 0 is untouched. The
/// cached unsigned record index narrows into the signed halfword work field.
/// Borrows task/model storage and uses animation playback's scratch/GTE state.
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
///
/// work, animationId and blendFrames must have no side effects and are evaluated
/// on every iteration; sizeof inspects work unevaluated for the bound. Requires live slots,
/// loaded clip data and pose buffers through the transition. Retains playback
/// rates and slot 0; capture ticks consume the existing rate and retain flags.
/// blendFrames counts normal-rate frames (0 immediate, 0..2047 without signed-time
/// wrapping). Captures no caller identifiers and forms one statement with its
/// own u16 slotIndex; arguments must not refer to that local.
#define BLEND_BODY_SLOTS(work, animationId, blendFrames)                                               \
    do {                                                                                               \
        u16 slotIndex;                                                                                 \
        for (slotIndex = 1; slotIndex < ARRAY_SIZE((work)->rig.slots); slotIndex++) {                  \
            animationSeekSlotWithBlend(&(work)->rig.anim, slotIndex, (animationId), 0, (blendFrames)); \
        }                                                                                              \
    } while (0)

/// Blends the eighteen non-root body tracks to the selected animation's start.
///
/// Requires an initialized rig and a loaded `animationId` with tracks 1..18.
/// Clears the cached slot-1 record before capturing/ticking each current pose.
/// Retains each slot's playback rate; slot 0 is untouched. `blendFrames` counts
/// whole normal-rate frames (0 immediate, 0..2047 without signed-time wrapping).
/// Pose buffers and clip data must remain live through the transition; uses
/// animation playback's scratch stack and GTE state.
static inline void _squareGrinningStrangerBlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames)
{
    _Work* work = task->work;

    work->slot1RecordIndex = 0;
    BLEND_BODY_SLOTS(work, animationId, blendFrames);
}
#undef BLEND_BODY_SLOTS

/// Ticks the body animation and advances the staged Stranger sequence.
///
/// Requires initialized work and a live model. Steps blend clips, lower part 5
/// in pitch/yaw, hold, swing its yaw, then depart along parent-frame negative Z
/// by 150 units per update. Angles use 4096 units per turn; holds count updates.
static void _squareGrinningStrangerRunSequence(Task* task)
{
    enum {
        SEQUENCE_START_CLIP = 0,
        DEPART_CLIP         = 2,
        START_BLEND_FRAMES  = 15,
        DEPART_BLEND_FRAMES = 10,
        LOWER_ANGLE_LIMIT   = -340,
        SWING_ANGLE_LIMIT   = 682,
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
            _squareGrinningStrangerBlendBodyAnimation(task, SEQUENCE_START_CLIP, START_BLEND_FRAMES);
            work->stepFrames = 0;
            work->sequenceStep++;
            break;
        case STEP_TURN:
            yawLowered   = 0;
            pitchLowered = 0;
            if (work->part5Yaw >= LOWER_ANGLE_LIMIT) {
                work->part5Yaw -= 0x10;
            } else {
                yawLowered = 1;
            }
            if (work->part5Pitch >= LOWER_ANGLE_LIMIT) {
                work->part5Pitch -= 0x10;
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
            if (work->part5Yaw < SWING_ANGLE_LIMIT) {
                work->part5Yaw += 0x80;
            } else {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case STEP_SWING_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= SWING_HOLD_UPDATES) {
                _squareGrinningStrangerBlendBodyAnimation(task, DEPART_CLIP, DEPART_BLEND_FRAMES);
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
///
/// Requires an initialized nineteen-part rig. Slot zero is untouched and the
/// cached slot-1 record is cleared. Clip 5 and its model tracks remain loaded.
static inline void _squareGrinningStrangerStartIdleTracks(Task* task)
{
    enum {
        IDLE_CLIP = 5,
    };

    _Work* work;
    s32    slotIndex;

    slotIndex              = 1;
    work                   = task->work;
    work->slot1RecordIndex = 0;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, IDLE_CLIP);
        slotIndex += 1;
    } while ((u32)((u16)slotIndex) < ARRAY_SIZE(work->rig.slots));
}

/// Initializes the staged Stranger body and its eighteen non-root animation tracks.
///
/// Allocates zeroed task-owned work, killing the task on failure. The model borrows
/// its lighting matrices and the loaded clip bank. Parents the root to the view,
/// starts clip 5 and sets part-5 pitch to 341/4096 turns. Texture offsets come from
/// placement entry 19, or the end record when absent. Setup does not advance task state.
static void _squareGrinningStrangerInitBody(Task* task)
{
    enum {
        INITIAL_PART5_PITCH = 341,
        TEXTURE_ENTRY_ID    = 19,
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
    animationInitContext(&work->rig.anim, _gSquareGrinningStrangerGrinningStrangerAnimationSets, model, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerMtx  = gPlayerStatus.coordMtx;
    _squareGrinningStrangerStartIdleTracks(task);
    work->part5Pitch = INITIAL_PART5_PITCH;
    place            = areaGetVariant(&gGameSession->location)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != TEXTURE_ENTRY_ID) {
        place++;
    }
    tmdSetTextureOffsets(model, place->texturePageOffset, place->clutRowOffset);
}

/// Stages the Grinning Stranger's trigger, scripted departure and joint lighting.
///
/// State 0 waits for the attachment wheel and pending display change to clear,
/// then initializes the body; state 1 ticks tracks 1..18 until the player crosses
/// the room trigger; state 2 runs the sequence until session event state is 0.
/// The common tail applies part 5's pitch and world yaw in 4096-turn units,
/// samples three lights at part 1, and scales colour by truncated Q12 one-third.
/// The tail retains entry-time work after initialization and continues after
/// a kill request; no lifetime guard is present.
static void _squareGrinningStrangerGrinningStrangerTask(Task* task)
{
    enum {
        TASK_INITIALIZE       = 0,
        TASK_IDLE             = 1,
        TASK_SEQUENCE         = 2,
        SEQUENCE_PART         = 5,
        LIGHTING_PART         = 1,
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
    GfxCoord*  sequencePart;
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
    bodyModel    = task->extra.tmd;
    partPitch    = work->part5Pitch;
    bodyCoords   = bodyModel->coords;
    sequencePart = bodyCoords + SEQUENCE_PART;
    _actorRenderAccumulateRotation(sequencePart, &frameScratch.rotation, &gGfxViewCoord);
    RotMatrixX(partPitch, &frameScratch.rotation);
    _actorRenderLocalizeRotation(sequencePart, &frameScratch.rotation);
    memCopyBytes(frameScratch.rotation.m, sequencePart->coord.m, sizeof(frameScratch.rotation.m));
    sequencePart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(sequencePart);
    _actorRenderYawJointInWorld(task->extra.tmd->coords + SEQUENCE_PART, work->part5Yaw);
    // Relight the pose, then apply the truncated one-third colour scale.
    lightingModel                = task->extra.tmd;
    lightingWork                 = task->work;
    frameScratch.viewPosition.vx = lightingModel->coords[LIGHTING_PART].workm.t[0];
    frameScratch.viewPosition.vy = task->extra.tmd->coords[LIGHTING_PART].workm.t[1];
    frameScratch.viewPosition.vz = task->extra.tmd->coords[LIGHTING_PART].workm.t[2];
    worldCoordSetModelLighting(lightingModel, &frameScratch.viewPosition, 0, LIGHT_COUNT);
    frameScratch.colorScale.vz = COLOR_SCALE_ONE_THIRD;
    frameScratch.colorScale.vy = COLOR_SCALE_ONE_THIRD;
    frameScratch.colorScale.vx = COLOR_SCALE_ONE_THIRD;
    ScaleMatrix(&lightingWork->color, &frameScratch.colorScale);
}
