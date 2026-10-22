#include "actors/task_tables.h"

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
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"

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

static void _actor110700No9GolemSetup(Enemy* enemy, Task* task);
static void _actor110700No9GolemUpdate(Enemy* enemy, Task* task);

extern TmdSource gGolemNo9BodyAkropolis;
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

#include "assets/enemies/golem/no_9/body_akropolis.inc"

#include "assets/rooms/akropolis/sanctuary/no_9_clips.inc"

enum { ACTOR_110700_GOLEM_TASK_PRIORITY = 96 };

TaskDesc gActor110700No9GolemTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_110700_GOLEM_TASK_PRIORITY } },
    .callback = _actor110700No9GolemTask,
    .data     = { .model = &gGolemNo9BodyAkropolis },
};

/// Message map for the script-posed No. 9 golem.
///
/// Animation messages borrow an `AnimationPlayRequest` with clip ID 1..5;
/// placement borrows an `ActorTransform`, and model drawing takes a flag word.
/// The task borrows this terminated map and its callbacks until teardown.
static const TaskMessageEntry _gActor110700No9GolemMessages[4] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor110700No9GolemPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor110700No9GolemSetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Animation sets of the script-posed No. 9 golem, indexed by clip ID.
///
/// Entry 0 denotes no requested clip and must not be played; IDs 1..5 are loaded
/// sets. The rig borrows this word-aligned table and its sets until teardown.
#include "assets/rooms/akropolis/sanctuary/no_9_bank.inc"

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
    animationInitContext(&work->rig.anim, _gActor110700No9GolemAnimationSets, model, work->rig.poses, work->rig.slots);
    work->animId            = ACTOR_110700_ANIMATION_NONE;
    task->msgTable          = _gActor110700No9GolemMessages;
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

#include "../../shared/actor/actor_messages_place_rot_matrix.inc.c"

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
