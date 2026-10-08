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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_square.h"
#include "../../shared/actor_contacts.h"

/// Steps of `_Actor111800Work::sequenceStep`, in the order the sequence runs
/// them.
///
/// Each step advances to the next by incrementing the field; the last one
/// repeats until the task is killed.
enum {
    ACTOR_111800_STEP_START      = 0, // Cross-fades the body slots to animation set 0 over 15 frames
    ACTOR_111800_STEP_TURN       = 1, // Lowers `part5Yaw` and `part5Pitch` by 0x10 a tick each, until both are below -0x154
    ACTOR_111800_STEP_TURN_HOLD  = 2, // Holds for 31 ticks
    ACTOR_111800_STEP_SWING      = 3, // Raises `part5Yaw` by 0x80 a tick until it has reached 0x2AA
    ACTOR_111800_STEP_SWING_HOLD = 4, // Holds for 16 ticks, then cross-fades the body slots to animation set 2 over 10 frames
    ACTOR_111800_STEP_SET_WAIT   = 5, // Waits two ticks
    ACTOR_111800_STEP_DEPART     = 6  // Moves the model's root 0x96 along -Z every tick
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
    u16            sequenceStep;     // `ACTOR_111800_STEP_*` of the scripted sequence; reset to its first step as the player triggers it
    byte           field_486[0x2];   // Never accessed; role unproven
    u16            stepFrames;       // Ticks counted by the step in progress: its two holds and its two-tick wait count it up from 0
    byte           field_48A[0x2];   // Never accessed; role unproven
    s16            part5Yaw;         // Turn of model part 5 about the world's Y axis: 0 from setup, lowered to -0x160 and then raised to 0x320 by the sequence
    byte           field_48E[0x4];   // Never accessed; role unproven
    s16            slot1RecordIndex; // Keyframe record slot 1 was on after the last tick, cleared by each cross-fade; never read, purpose unproven
    s16            part5Pitch;       // Turn of model part 5 about its X axis: 0x155 from setup, lowered to -0x15B by the sequence
} _Actor111800Work;
STATIC_ASSERT_SIZEOF(_Actor111800Work, 0x498);

/// Animation bank `animationInitContext` builds the work block's clip context from;
/// the actor hands it over whole, so it is only ever a byte address here.
extern AnimationSet* D_actor_111800_8013A448[8];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Main-executable globals with no module header yet: `Gp_StateC08.mode` is 1 while the attachment wheel is open and
/// `gDisplayState.pendingMode` is a pending display mode. `acropolisSquareStartSirenSequence` is
/// the room overlay's handler the view-matrix test calls with `t[0]`.

static TmdSource _gActor111800GrinningStrangerBody;
void             func_actor_111800_8013251C(Task*);

static TmdBone _gActor111800GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor111800GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor111800GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor111800GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor111800GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor111800GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor111800GrinningStrangerBodyPartVerts,
    _gActor111800GrinningStrangerBodyVerts,
    _gActor111800GrinningStrangerBodyNormals,
    _gActor111800GrinningStrangerBodySkeleton,
    _gActor111800GrinningStrangerBodyStream,
};

static AnimationPackedPose _gActor111800Animation065FCBank1[4] = {
#include "assets/actor_111800_animation_065FC_bank1.inc"
};

static AnimationPackedRotation _gActor111800Animation065FCBank4[69] = {
#include "assets/actor_111800_animation_065FC_bank4.inc"
};

static AnimationRecord _gActor111800Animation065FCRecords[162] = {
#include "assets/actor_111800_animation_065FC_records.inc"
};

static u16 _gActor111800Animation065FCIndices[20] = {
#include "assets/actor_111800_animation_065FC_indices.inc"
};

static AnimationSet _gActor111800Animation065FC = {
    _gActor111800Animation065FCRecords,
    _gActor111800Animation065FCIndices,
    { NULL, _gActor111800Animation065FCBank1, NULL, NULL, _gActor111800Animation065FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor111800Animation07120Bank1[28] = {
#include "assets/actor_111800_animation_07120_bank1.inc"
};

static AnimationPackedRotation _gActor111800Animation07120Bank4[247] = {
#include "assets/actor_111800_animation_07120_bank4.inc"
};

static AnimationRecord _gActor111800Animation07120Records[362] = {
#include "assets/actor_111800_animation_07120_records.inc"
};

static u16 _gActor111800Animation07120Indices[20] = {
#include "assets/actor_111800_animation_07120_indices.inc"
};

static AnimationSet _gActor111800Animation07120 = {
    _gActor111800Animation07120Records,
    _gActor111800Animation07120Indices,
    { NULL, _gActor111800Animation07120Bank1, NULL, NULL, _gActor111800Animation07120Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor111800Animation07BA4Bank1[32] = {
#include "assets/actor_111800_animation_07BA4_bank1.inc"
};

static AnimationPackedRotation _gActor111800Animation07BA4Bank4[223] = {
#include "assets/actor_111800_animation_07BA4_bank4.inc"
};

static AnimationRecord _gActor111800Animation07BA4Records[334] = {
#include "assets/actor_111800_animation_07BA4_records.inc"
};

static u16 _gActor111800Animation07BA4Indices[20] = {
#include "assets/actor_111800_animation_07BA4_indices.inc"
};

static AnimationSet _gActor111800Animation07BA4 = {
    _gActor111800Animation07BA4Records,
    _gActor111800Animation07BA4Indices,
    { NULL, _gActor111800Animation07BA4Bank1, NULL, NULL, _gActor111800Animation07BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor111800Animation07FACBank1[6] = {
#include "assets/actor_111800_animation_07FAC_bank1.inc"
};

static AnimationPackedRotation _gActor111800Animation07FACBank4[70] = {
#include "assets/actor_111800_animation_07FAC_bank4.inc"
};

static AnimationRecord _gActor111800Animation07FACRecords[150] = {
#include "assets/actor_111800_animation_07FAC_records.inc"
};

static u16 _gActor111800Animation07FACIndices[20] = {
#include "assets/actor_111800_animation_07FAC_indices.inc"
};

static AnimationSet _gActor111800Animation07FAC = {
    _gActor111800Animation07FACRecords,
    _gActor111800Animation07FACIndices,
    { NULL, _gActor111800Animation07FACBank1, NULL, NULL, _gActor111800Animation07FACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor111800Animation08600Bank1[18] = {
#include "assets/actor_111800_animation_08600_bank1.inc"
};

static AnimationPackedRotation _gActor111800Animation08600Bank4[146] = {
#include "assets/actor_111800_animation_08600_bank4.inc"
};

static AnimationRecord _gActor111800Animation08600Records[185] = {
#include "assets/actor_111800_animation_08600_records.inc"
};

static u16 _gActor111800Animation08600Indices[20] = {
#include "assets/actor_111800_animation_08600_indices.inc"
};

static AnimationSet _gActor111800Animation08600 = {
    _gActor111800Animation08600Records,
    _gActor111800Animation08600Indices,
    { NULL, _gActor111800Animation08600Bank1, NULL, NULL, _gActor111800Animation08600Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_111800_8013A448[8] = {
    &_gActor111800Animation065FC,
    &_gActor111800Animation07120,
    &_gActor111800Animation07BA4,
    NULL,
    NULL,
    &_gActor111800Animation07FAC,
    &_gActor111800Animation08600,
    NULL,
};

TaskDesc D_actor_111800_8013A468 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_111800_8013251C, { .model = &_gActor111800GrinningStrangerBody } }; /// Turns joint `coord` by `yaw` about the world Y axis: builds its world

static inline void _actor111800TickAnim(Task* task);
static inline void _actor111800BlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames);
static void        _actor111800RunSequence(Task* task);
static void        _actor111800InitBody(Task* task);

#include "../../shared/actor_contacts_turn_joint.inc.c"

/// Ticks the eighteen non-root body tracks and records slot 1's keyframe index.
///
/// Requires initialized nineteen-part rig storage and live clip data. Each slot
/// advances at its existing rate and writes its pose; slot 0 is untouched. The
/// cached unsigned record index narrows into the signed halfword work field.
/// Borrows task/model storage and uses animation playback's scratch/GTE state.
static inline void _actor111800TickAnim(Task* task)
{
    _Actor111800Work* work = task->work;
    u16               slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->slot1RecordIndex = work->rig.slots[1].currentPose.indices.recordIndex;
}

/// Cross-fades body slots 1..0x12 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR111800_BLEND_SLOTS(work, id, frames)                                \
    do {                                                                          \
        u16 _i;                                                                   \
        for (_i = 1; _i < 0x13; _i++) {                                           \
            animationSeekSlotWithBlend(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                                         \
    } while (0)

/// Blends the eighteen non-root body tracks to the selected animation's start.
///
/// Requires an initialized rig and a loaded `animationId` with tracks 1..18.
/// Clears the cached slot-1 record before capturing/ticking each current pose.
/// Retains each slot's playback rate; slot 0 is untouched. `blendFrames` counts
/// whole normal-rate frames (0 immediate, 0..2047 without signed-time wrapping).
/// Pose buffers and clip data must remain live through the transition; uses
/// animation playback's scratch stack and GTE state.
static inline void _actor111800BlendBodyAnimation(Task* task, u16 animationId, u16 blendFrames)
{
    _Actor111800Work* work = task->work;

    work->slot1RecordIndex = 0;
    _ACTOR111800_BLEND_SLOTS(work, animationId, blendFrames);
}

/// Ticks the body animation and advances the staged Stranger sequence.
///
/// Requires initialized work and a live model. Steps blend clips, lower part 5
/// in pitch/yaw, hold, swing its yaw, then depart along parent-frame negative Z
/// by 150 units per update. Angles use 4096 units per turn; holds count updates.
static void _actor111800RunSequence(Task* task)
{
    enum {
        ACTOR_111800_SEQUENCE_START_CLIP = 0,
        ACTOR_111800_DEPART_CLIP         = 2,
        ACTOR_111800_START_BLEND_FRAMES  = 15,
        ACTOR_111800_DEPART_BLEND_FRAMES = 10,
        ACTOR_111800_LOWER_ANGLE_LIMIT   = -340,
        ACTOR_111800_SWING_ANGLE_LIMIT   = 682,
        ACTOR_111800_TURN_HOLD_UPDATES   = 31,
        ACTOR_111800_SWING_HOLD_UPDATES  = 16,
        ACTOR_111800_SET_WAIT_UPDATES    = 2,
        ACTOR_111800_DEPART_STEP_UNITS   = 150,
    };

    _Actor111800Work* work;
    GfxCoord*         rootCoord;
    s32               yawLowered;
    s32               pitchLowered;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _actor111800TickAnim(task);
    switch (work->sequenceStep) {
        case ACTOR_111800_STEP_START:
            _actor111800BlendBodyAnimation(task, ACTOR_111800_SEQUENCE_START_CLIP, ACTOR_111800_START_BLEND_FRAMES);
            work->stepFrames = 0;
            work->sequenceStep++;
            break;
        case ACTOR_111800_STEP_TURN:
            yawLowered   = 0;
            pitchLowered = 0;
            if (work->part5Yaw >= ACTOR_111800_LOWER_ANGLE_LIMIT) {
                work->part5Yaw -= 0x10;
            } else {
                yawLowered = 1;
            }
            if (work->part5Pitch >= ACTOR_111800_LOWER_ANGLE_LIMIT) {
                work->part5Pitch -= 0x10;
            } else {
                pitchLowered = 1;
            }
            if (pitchLowered & yawLowered) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_TURN_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= ACTOR_111800_TURN_HOLD_UPDATES) {
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SWING:
            if (work->part5Yaw < ACTOR_111800_SWING_ANGLE_LIMIT) {
                work->part5Yaw += 0x80;
            } else {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SWING_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= ACTOR_111800_SWING_HOLD_UPDATES) {
                _actor111800BlendBodyAnimation(task, ACTOR_111800_DEPART_CLIP, ACTOR_111800_DEPART_BLEND_FRAMES);
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SET_WAIT:
            work->stepFrames++;
            if (work->stepFrames >= ACTOR_111800_SET_WAIT_UPDATES) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_DEPART:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            rootCoord->coord.t[2]  -= ACTOR_111800_DEPART_STEP_UNITS;
            break;
    }
}

/// Restarts the eighteen body tracks at the idle clip's normal playback rate.
///
/// Requires an initialized nineteen-part rig. Slot zero is untouched and the
/// cached slot-1 record is cleared. Clip 5 and its model tracks remain loaded.
static inline void _actor111800StartIdleTracks(Task* task)
{
    enum {
        ACTOR_111800_IDLE_CLIP = 5,
    };

    _Actor111800Work* slotsWork;
    s32               slotIndex;

    slotIndex                   = 1;
    slotsWork                   = task->work;
    slotsWork->slot1RecordIndex = 0;
    do {
        slotsWork->rig.slots[slotIndex & 0xFFFF].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&slotsWork->rig.anim, slotIndex & 0xFFFF, ACTOR_111800_IDLE_CLIP);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(slotsWork->rig.slots));
}

/// Initializes the staged Stranger body and its eighteen non-root animation tracks.
///
/// Allocates zeroed task-owned work, killing the task on failure. The model borrows
/// its lighting matrices and the loaded clip bank. Parents the root to the view,
/// starts clip 5 and sets part-5 pitch to 341/4096 turns. Texture offsets come from
/// placement entry 19, or the end record when absent. Setup does not advance task state.
static void _actor111800InitBody(Task* task)
{
    enum {
        ACTOR_111800_INITIAL_PART5_PITCH = 341,
        ACTOR_111800_TEXTURE_ENTRY_ID    = 19,
    };

    _Actor111800Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    AreaPlacement*    place;

    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    task->work = memCalloc(sizeof(_Actor111800Work), false);
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
    animationInitContext(&work->rig.anim, D_actor_111800_8013A448, model, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerMtx  = gPlayerStatus.coordMtx;
    _actor111800StartIdleTracks(task);
    work->part5Pitch = ACTOR_111800_INITIAL_PART5_PITCH;
    place            = areaGetVariant(&gGameSession->location.loc)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != ACTOR_111800_TEXTURE_ENTRY_ID) {
        place++;
    }
    tmdSetTextureOffsets(model, place->texturePageOffset, place->clutRowOffset);
}

/// Per-frame state machine. State 0 waits until no cutscene is up, then runs
/// the spawn handler and advances. State 1 ticks slots 1..0x12, latches
/// `slot1RecordIndex`, and advances after `acropolisSquareStartSirenSequence` when the player is in
/// range. State 2 runs the sequence handler and kills the task once the
/// session is idle. Every path but the state-0 wait then pitches part 5 by
/// `part5Pitch`, writes it back, yaws it by `part5Yaw` through `_actorRenderYawJointInWorld`, and
/// rebuilds the colour matrix around part 1's translation.
void func_actor_111800_8013251C(Task* task)
{
    MATRIX            mtx;
    _Actor111800Work* work;
    _Actor111800Work* ctx;
    _Actor111800Work* work2;
    TmdObject*        extra;
    TmdObject*        obj;
    GfxCoord*         coords;
    GfxCoord*         part;
    MATRIX*           playerMtx;
    s32               state;
    s32               i;
    s32               x;
    u16               angle;

    state = task->state;
    work  = task->work;
    switch (state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                _actor111800InitBody(task);
                task->state += 1;
                break;
            }
            return;
        case 1:
            ctx = work;
            i   = 1;
            do {
                animationTickSlot(&ctx->rig.anim, i & 0xFFFF);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            ctx->slot1RecordIndex = ctx->rig.slots[1].currentPose.indices.recordIndex;
            playerMtx             = work->playerMtx;
            x                     = playerMtx->t[0];
            if ((x >= 0x5DD && playerMtx->t[2] >= -0x513) || (x >= 0xC81 && playerMtx->t[2] < -0x514)) {
                work->sequenceStep = ACTOR_111800_STEP_START;
                acropolisSquareStartSirenSequence(x);
                task->state += 1;
            }
            break;
        case 2:
            _actor111800RunSequence(task);
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
    extra  = task->extra.tmd;
    angle  = (u16)work->part5Pitch;
    coords = extra->coords;
    part   = coords + 5;
    _actorRenderAccumulateRotation(part, &mtx, &gGfxViewCoord);
    RotMatrixX((s32)(s16)angle, &mtx);
    _actorRenderLocalizeRotation(part, &mtx);
    memCopyBytes(mtx.m, part->coord.m, sizeof(mtx.m));
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(part);
    _actorRenderYawJointInWorld(task->extra.tmd->coords + 5, work->part5Yaw);
    obj                 = task->extra.tmd;
    work2               = task->work;
    ((VECTOR*)&mtx)->vx = obj->coords[1].workm.t[0];
    ((VECTOR*)&mtx)->vy = task->extra.tmd->coords[1].workm.t[1];
    ((VECTOR*)&mtx)->vz = task->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(obj, &mtx, 0, 3);
    ((VECTOR*)&mtx)->vz = 0x555;
    ((VECTOR*)&mtx)->vy = 0x555;
    ((VECTOR*)&mtx)->vx = 0x555;
    ScaleMatrix(&work2->color, (VECTOR*)&mtx);
}
