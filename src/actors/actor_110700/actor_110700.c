#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

/// Work block of the No. 9 golem as a room script poses it: what its body
/// model plays and the matrices it is lit with.
///
/// The setup state allocates it zeroed and keeps it at `Task::work` for the
/// task's life. The model object borrows `light` and `color`, and the
/// animation context borrows the rig's slots and pose buffers, for as long as
/// the block lives. Nothing is played until a script sends
/// `ACTOR_MESSAGE_PLAY_ANIMATION`; each request restarts every driven slot.
typedef struct {
    ActorAnimRig19 rig;    // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         color;  // Light-colour matrix lent to the model object
    MATRIX         light;  // Light-direction matrix lent to the model object
    s32            animId; // Entry of the package's animation-set table the last play request seeded the slots with (0 none yet: the slots are not ticked)
} _Actor110700No9GolemWork;
STATIC_ASSERT_SIZEOF(_Actor110700No9GolemWork, 0x480);

/// The actor's message table: handlers for 0x7D3 (start an animation), 0x7D4
/// (place the actor) and 0x7D5 (visibility), then the terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_110700_8013BFA0[];

/// Animation-set table bound to the work block's context by `animationInitContext`.
extern u8 D_actor_110700_8013BFC0[];

static void _actor110700No9GolemSetup(Enemy* enemy, Task* task);
static void _actor110700No9GolemUpdate(Enemy* enemy, Task* task);

static TmdSource _gActor110700No9GolemAkropolisBody;
static s32       _actor110700No9GolemPlayAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _actor110700No9GolemSetModelDraw(Task* task, s32 msgId, s32 drawFlags, s32 unusedArg);
static void      _actor110700No9GolemTask(Task* task);

/// Task states of the script-posed No. 9 golem.
enum {
    ACTOR_110700_STATE_SETUP  = 0,
    ACTOR_110700_STATE_UPDATE = 1,
};

/// No animation has been requested; the unseeded slots must not advance.
enum { ACTOR_110700_ANIMATION_NONE = 0 };

/// Model coordinate 0 is the placement root; body animation starts at 1.
enum { ACTOR_110700_FIRST_ANIMATED_PART = 1 };

static TmdBone _gActor110700No9GolemAkropolisBodySkeleton[19] = {
#include "assets/no9_golem_akropolis_body_skeleton.inc"
};

static u32 _gActor110700No9GolemAkropolisBodyPartVerts[19] = {
#include "assets/no9_golem_akropolis_body_partVerts.inc"
};

static SVECTOR _gActor110700No9GolemAkropolisBodyVerts[358] = {
#include "assets/no9_golem_akropolis_body_verts.inc"
};

static SVECTOR _gActor110700No9GolemAkropolisBodyNormals[356] = {
#include "assets/no9_golem_akropolis_body_normals.inc"
};

static u32 _gActor110700No9GolemAkropolisBodyStream[3928] = {
#include "assets/no9_golem_akropolis_body_stream.inc"
};

static TmdSource _gActor110700No9GolemAkropolisBody = {
    0,
    21588,
    5944,
    19,
    _gActor110700No9GolemAkropolisBodyPartVerts,
    _gActor110700No9GolemAkropolisBodyVerts,
    _gActor110700No9GolemAkropolisBodyNormals,
    _gActor110700No9GolemAkropolisBodySkeleton,
    _gActor110700No9GolemAkropolisBodyStream,
};

static AnimationPackedPose _gActor110700Animation05D64Bank1[6] = {
#include "assets/actor_110700_animation_05D64_bank1.inc"
};

static AnimationPackedRotation _gActor110700Animation05D64Bank4[80] = {
#include "assets/actor_110700_animation_05D64_bank4.inc"
};

static AnimationRecord _gActor110700Animation05D64Records[122] = {
#include "assets/actor_110700_animation_05D64_records.inc"
};

static u16 _gActor110700Animation05D64Indices[20] = {
#include "assets/actor_110700_animation_05D64_indices.inc"
};

static AnimationSet _gActor110700Animation05D64 = {
    _gActor110700Animation05D64Records,
    _gActor110700Animation05D64Indices,
    { NULL, _gActor110700Animation05D64Bank1, NULL, NULL, _gActor110700Animation05D64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110700Animation0667CBank1[14] = {
#include "assets/actor_110700_animation_0667C_bank1.inc"
};

static AnimationPackedRotation _gActor110700Animation0667CBank4[238] = {
#include "assets/actor_110700_animation_0667C_bank4.inc"
};

static AnimationRecord _gActor110700Animation0667CRecords[282] = {
#include "assets/actor_110700_animation_0667C_records.inc"
};

static u16 _gActor110700Animation0667CIndices[20] = {
#include "assets/actor_110700_animation_0667C_indices.inc"
};

static AnimationSet _gActor110700Animation0667C = {
    _gActor110700Animation0667CRecords,
    _gActor110700Animation0667CIndices,
    { NULL, _gActor110700Animation0667CBank1, NULL, NULL, _gActor110700Animation0667CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110700Animation0820CBank1[53] = {
#include "assets/actor_110700_animation_0820C_bank1.inc"
};

static AnimationPackedRotation _gActor110700Animation0820CBank4[730] = {
#include "assets/actor_110700_animation_0820C_bank4.inc"
};

static AnimationRecord _gActor110700Animation0820CRecords[855] = {
#include "assets/actor_110700_animation_0820C_records.inc"
};

static u16 _gActor110700Animation0820CIndices[20] = {
#include "assets/actor_110700_animation_0820C_indices.inc"
};

static AnimationSet _gActor110700Animation0820C = {
    _gActor110700Animation0820CRecords,
    _gActor110700Animation0820CIndices,
    { NULL, _gActor110700Animation0820CBank1, NULL, NULL, _gActor110700Animation0820CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110700Animation08714Bank1[11] = {
#include "assets/actor_110700_animation_08714_bank1.inc"
};

static AnimationPackedRotation _gActor110700Animation08714Bank4[110] = {
#include "assets/actor_110700_animation_08714_bank4.inc"
};

static AnimationRecord _gActor110700Animation08714Records[159] = {
#include "assets/actor_110700_animation_08714_records.inc"
};

static u16 _gActor110700Animation08714Indices[20] = {
#include "assets/actor_110700_animation_08714_indices.inc"
};

static AnimationSet _gActor110700Animation08714 = {
    _gActor110700Animation08714Records,
    _gActor110700Animation08714Indices,
    { NULL, _gActor110700Animation08714Bank1, NULL, NULL, _gActor110700Animation08714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110700Animation0A14CBank1[51] = {
#include "assets/actor_110700_animation_0A14C_bank1.inc"
};

static AnimationPackedRotation _gActor110700Animation0A14CBank4[692] = {
#include "assets/actor_110700_animation_0A14C_bank4.inc"
};

static AnimationRecord _gActor110700Animation0A14CRecords[813] = {
#include "assets/actor_110700_animation_0A14C_records.inc"
};

static u16 _gActor110700Animation0A14CIndices[20] = {
#include "assets/actor_110700_animation_0A14C_indices.inc"
};

static AnimationSet _gActor110700Animation0A14C = {
    _gActor110700Animation0A14CRecords,
    _gActor110700Animation0A14CIndices,
    { NULL, _gActor110700Animation0A14CBank1, NULL, NULL, _gActor110700Animation0A14CBank4, NULL, NULL, NULL },
};

TaskDesc D_actor_110700_8013BF94 = { { { TASK_BODY_TMD, 96 } }, _actor110700No9GolemTask, { .model = &_gActor110700No9GolemAkropolisBody } };

TaskMessageEntry D_actor_110700_8013BFA0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor110700No9GolemPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor110700No9GolemSetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8 D_actor_110700_8013BFC0[24] = {
    0,
    0,
    0,
    0,
    132,
    123,
    19,
    128,
    156,
    132,
    19,
    128,
    44,
    160,
    19,
    128,
    52,
    165,
    19,
    128,
    108,
    191,
    19,
    128,
};

/// Runs the script-posed No. 9 golem's setup or per-frame update.
///
/// Requires a live TMD task with its owning `Enemy` in `spawnArg2.pointer`.
/// `state` must be `ACTOR_110700_STATE_SETUP` or `ACTOR_110700_STATE_UPDATE`;
/// there is no bounds check. Setup failure may destroy both arguments.
static void _actor110700No9GolemTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        [ACTOR_110700_STATE_SETUP]  = _actor110700No9GolemSetup,
        [ACTOR_110700_STATE_UPDATE] = _actor110700No9GolemUpdate,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Allocates the golem's animation and lighting storage and enables script messages.
///
/// Requires a newly spawned TMD task and its live owning `Enemy`. The task
/// owns the primary-heap work block; its model borrows the lighting matrices
/// until teardown. Playback remains inactive until a play request seeds the
/// slots. Allocation failure destroys the enemy and task; success selects
/// `ACTOR_110700_STATE_UPDATE`.
static void _actor110700No9GolemSetup(Enemy* enemy, Task* task)
{
    GfxCoord*                 rootCoord;
    TmdObject*                model;
    _Actor110700No9GolemWork* work;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor110700No9GolemWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The model and animation context borrow storage owned by this task.
    task->work      = work;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_110700_8013BFC0, model, work->rig.poses, work->rig.slots);
    work->animId            = ACTOR_110700_ANIMATION_NONE;
    task->msgTable          = D_actor_110700_8013BFA0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state             = ACTOR_110700_STATE_UPDATE;
}

/// Advances the golem's animation and samples lighting at its first animated part.
///
/// Requires completed setup and the live owning `Enemy`. Advances slots 1..18
/// once per call only after a play request; slot 0 is the undriven model root.
/// Samples coordinate 1's existing composed translation in world units.
/// Reserves one `VECTOR` on the initialized scratch stack through playback
/// and lighting; both callees require additional nested scratch space.
static void _actor110700No9GolemUpdate(Enemy* enemy, Task* task)
{
    _Actor110700No9GolemWork* work;
    GfxCoord*                 partCoord;
    VECTOR*                   worldPosition;
    s32                       slotIndex;

    work      = task->work;
    partCoord = &task->extra.tmd->coords[ACTOR_110700_FIRST_ANIMATED_PART];
    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    worldPosition = SCRATCH_STACK_CURSOR(VECTOR);
    if (work->animId != ACTOR_110700_ANIMATION_NONE) {
        for (slotIndex = ACTOR_110700_FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Lighting reads only XYZ; the VECTOR's fourth word stays untouched.
    worldPosition->vx = partCoord->workm.t[0];
    worldPosition->vy = partCoord->workm.t[1];
    worldPosition->vz = partCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Restarts all driven body tracks on the golem's requested animation set.
///
/// `work` must be live and its context bound to its rig and nineteen-part
/// model. `work->animId` selects a loaded package set in 1..5; zero is invalid
/// here, and no bounds are checked. The borrowed set table and clip data must
/// remain live during subsequent playback.
///
/// Slots 1..18 restart at their same-numbered track starts with
/// `ANIMATION_RATE_ONE` and cleared boundary state and result flags. Slot 0
/// is the placement root and stays untouched. No model coordinates or pose
/// buffer entries are written; the next tick establishes the new segment.
static inline void _actor110700No9GolemRestartTracks(_Actor110700No9GolemWork* work)
{
    s32 slotIndex;

    for (slotIndex = ACTOR_110700_FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
}

/// Restarts the golem's body animation for `ACTOR_MESSAGE_PLAY_ANIMATION`.
///
/// Requires completed setup and a readable, word-aligned request through
/// dispatch. `animationId` selects package set 1..5; zero has no set and is
/// invalid here. Copies only that word and retains no payload pointer. Bank,
/// blend and collision choices are ignored: every request restarts slots
/// 1..18 without a transition from the old pose. Ignores `msgId` and the
/// second argument. Returns 0.
static s32 _actor110700No9GolemPlayAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor110700No9GolemWork* work;

    work         = task->work;
    work->animId = request->animationId;
    _actor110700No9GolemRestartTracks(work);
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Replaces the golem's model flags for `ACTOR_MESSAGE_SET_MODEL_DRAW`.
///
/// Requires a live TMD task. Bit 0 permits active drawing; without it the
/// model is excluded. Bit 1 suppresses automatic missing-buffer allocation.
/// All other request bits are ignored and all unrelated model flags are
/// cleared. Allocates and frees no buffers. Ignores `msgId` and the second
/// argument. Returns 0.
static s32 _actor110700No9GolemSetModelDraw(Task* task, s32 msgId, s32 drawFlags, s32 unusedArg)
{
    enum {
        ACTOR_110700_MODEL_DRAW_SHOW             = 1 << 0,
        ACTOR_110700_MODEL_DRAW_SKIP_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* model;

    model = task->extra.tmd;
    if (!(drawFlags & ACTOR_110700_MODEL_DRAW_SHOW)) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    if (drawFlags & ACTOR_110700_MODEL_DRAW_SKIP_AUTO_BUFFER) {
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
