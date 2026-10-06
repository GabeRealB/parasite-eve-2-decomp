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
/// `gDisplayState.pendingMode` is a pending display mode. `func_acropolis_square_80182360` is
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

static inline void    _actor111800TickAnim(Task* task);
static inline void    _actor111800Reseed(Task* task, u16 id, u16 frames);
static void           func_actor_111800_8013214C(Task* task);
static void           func_actor_111800_80132390(Task* task);
static __inline__ s32 Actor111800_Accumulate(GfxCoord* arg0, MATRIX* arg1, MATRIX* src);

#include "../../shared/actor_contacts_turn_joint.inc.c"

/// Advances animation slots 1..0x12 by one frame and latches slot 1's current
/// record into `slot1RecordIndex`.
static inline void _actor111800TickAnim(Task* task)
{
    _Actor111800Work* work = task->work;
    u16               i;

    for (i = 1; i < 0x13; i++) {
        animationTickSlot(&work->rig.anim, i);
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

/// Clears the latched record and cross-fades every body slot to `id`.
static inline void _actor111800Reseed(Task* task, u16 id, u16 frames)
{
    _Actor111800Work* work = task->work;

    work->slot1RecordIndex = 0;
    _ACTOR111800_BLEND_SLOTS(work, id, frames);
}

/// Per-frame handler: ticks animation slots 1..0x12, latches `slots[1].currentPose.indices.recordIndex`
/// into `slot1RecordIndex`, then runs the seven-step sequence in `sequenceStep` (reseed,
/// ramp the two angles, wait, reverse the first angle, reseed again, wait,
/// then drop the model coordinate's Z and clear its flag).
static void func_actor_111800_8013214C(Task* task)
{
    _Actor111800Work* work;
    GfxCoord*         coord;
    s32               flag1;
    s32               flag2;

    work  = task->work;
    coord = task->extra.tmd->coords;
    _actor111800TickAnim(task);
    switch (work->sequenceStep) {
        case ACTOR_111800_STEP_START:
            _actor111800Reseed(task, 0, 0xF);
            work->stepFrames = 0;
            work->sequenceStep++;
            break;
        case ACTOR_111800_STEP_TURN:
            flag1 = 0;
            flag2 = 0;
            if (work->part5Yaw >= -0x154) {
                work->part5Yaw -= 0x10;
            } else {
                flag1 = 1;
            }
            if (work->part5Pitch >= -0x154) {
                work->part5Pitch -= 0x10;
            } else {
                flag2 = 1;
            }
            if (flag2 & flag1) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_TURN_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= 0x1F) {
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SWING:
            if (work->part5Yaw < 0x2AA) {
                work->part5Yaw += 0x80;
            } else {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SWING_HOLD:
            work->stepFrames++;
            if (work->stepFrames >= 0x10) {
                _actor111800Reseed(task, 2, 0xA);
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_SET_WAIT:
            work->stepFrames++;
            if (work->stepFrames >= 2) {
                work->stepFrames = 0;
                work->sequenceStep++;
            }
            break;
        case ACTOR_111800_STEP_DEPART:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[2]  -= 0x96;
            break;
    }
}

/// Spawn/setup handler for the actor's model: allocates the work block
/// into `Task::work`, hands the model object the view coordinate and the
/// block's two matrices, builds the animation context over the nineteen slots
/// and applies the nested area record matching id 0x13 through `tmdSetTextureOffsets`.
///
/// The allocation is parked in `Task::work` and read back before it is used, so
/// the first thing the block is named by is a reload: the `memCalloc` result is
/// stored straight from `$v0` and the failing branch tests that register, which
/// is what leaves the surviving copy of it to be emitted *after* the branch --
/// one declaration earlier and the copy lands before the test.
static void func_actor_111800_80132390(Task* task)
{
    _Actor111800Work* work;
    _Actor111800Work* work2;
    TmdObject*        obj;
    GfxCoord*         coord;
    AreaPlacement*    place;
    s32               i;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    task->work = memCalloc(sizeof(_Actor111800Work), false);
    if (task->work == NULL) {
        taskKill(task);
        return;
    }
    work = task->work;
    memFillBytes(work, 0U, sizeof(*work));
    coord->parent = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(obj);
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    obj->flags    = 0;
    animationInitContext(&work->rig.anim, D_actor_111800_8013A448, obj, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerMtx         = gPlayerStatus.coordMtx;
    i                       = 1;
    work2                   = task->work;
    work2->slot1RecordIndex = 0;
    do {
        work2->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work2->rig.anim, i & 0xFFFF, 5);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    work->part5Pitch = 0x155;
    place            = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != 0x13) {
        place++;
    }
    tmdSetTextureOffsets(obj, place->texturePageOffset, place->clutRowOffset);
}

/// Builds `arg0`'s absolute rotation in `arg1`, seeded from `src` rather than
/// from `arg0->coord`: each ancestor is pre-multiplied in turn (renormalised
/// after every step) up to but not including the view coordinate. Returns
/// whether the walk reached the view coordinate. The caller passes the part's
/// own rotation as `src`, addressed through the coordinate array.
static __inline__ s32 Actor111800_Accumulate(GfxCoord* arg0, MATRIX* arg1, MATRIX* src)
{
    MATRIX    matrix;
    GfxCoord* coord;
    GfxCoord* view;

    coord = arg0->parent;
    view  = &gGfxViewCoord;
    *arg1 = *src;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->parent;
    }
}

/// Per-frame state machine. State 0 waits until no cutscene is up, then runs
/// the spawn handler and advances. State 1 ticks slots 1..0x12, latches
/// `slot1RecordIndex`, and advances after `func_acropolis_square_80182360` when the player is in
/// range. State 2 runs the sequence handler and kills the task once the
/// session is idle. Every path but the state-0 wait then pitches part 5 by
/// `part5Pitch`, writes it back, yaws it by `part5Yaw` through `ActorContact_TurnJoint`, and
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
                func_actor_111800_80132390(task);
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
                func_acropolis_square_80182360(x);
                task->state += 1;
            }
            break;
        case 2:
            func_actor_111800_8013214C(task);
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
    extra  = task->extra.tmd;
    angle  = (u16)work->part5Pitch;
    coords = extra->coords;
    part   = coords + 5;
    Actor111800_Accumulate(part, &mtx, &coords[5].coord);
    RotMatrixX((s32)(s16)angle, &mtx);
    _actorRenderLocalizeRotation(part, &mtx);
    memCopyBytes(mtx.m, part->coord.m, sizeof(mtx.m));
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(part);
    ActorContact_TurnJoint(task->extra.tmd->coords + 5, work->part5Yaw);
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
