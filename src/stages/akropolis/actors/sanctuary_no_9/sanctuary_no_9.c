#include "stages/akropolis/actors/sanctuary_no_9.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/tmd_types.h"
#include "../../../../shared/actor/actor_messages.h"

/// Work block of the No. 9 golem as a room script poses it: what its body plays and the matrices it is lit with.
typedef struct {
    ActorAnimRig19 rig;    // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         color;  // Light-colour matrix lent to the model object
    MATRIX         light;  // Light-direction matrix lent to the model object
    s32            animId; // Entry of the package's animation-set table the last play request seeded the slots with (0 none yet: the slots are not ticked)
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x480);

static void _sanctuaryNo9Setup(Enemy* enemy, Task* task);
static void _sanctuaryNo9Update(Enemy* enemy, Task* task);

extern TmdSource gGolemNo9BodyAkropolis;
static s32       _sanctuaryNo9PlayAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _sanctuaryNo9SetModelDraw(Task* task, s32 msgId, s32 drawFlags, s32 unusedArg);
static void      _sanctuaryNo9Task(Task* task);

/// Task states of the script-posed No. 9 golem.
enum {
    STATE_SETUP  = 0,
    STATE_UPDATE = 1,
};

/// No animation has been requested; the unseeded slots must not advance.
enum { ANIMATION_NONE = 0 };

/// Model coordinate 0 is the placement root; body animation starts at 1.
enum { FIRST_ANIMATED_PART = 1 };

#include "assets/enemies/golem/no_9/body_akropolis.inc"

#include "assets/rooms/akropolis/sanctuary/no_9_clips.inc"

TaskDesc gSanctuaryNo9TaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_ACTOR } },
    .callback = _sanctuaryNo9Task,
    .data     = { .model = &gGolemNo9BodyAkropolis },
};

/// Message map for the script-posed No. 9 golem.
static const TaskMessageEntry _gSanctuaryNo9Messages[4] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _sanctuaryNo9PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _sanctuaryNo9SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Animation sets of the script-posed No. 9 golem, indexed by clip ID.
#include "assets/rooms/akropolis/sanctuary/no_9_bank.inc"

/// Runs the script-posed No. 9 golem's setup or per-frame update.
static void _sanctuaryNo9Task(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        [STATE_SETUP]  = _sanctuaryNo9Setup,
        [STATE_UPDATE] = _sanctuaryNo9Update,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Allocates the golem's animation and lighting storage and enables script messages.
static void _sanctuaryNo9Setup(Enemy* enemy, Task* task)
{
    GfxCoord*  rootCoord;
    TmdObject* model;
    _Work*     work;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The model and animation context borrow storage owned by this task.
    task->work      = work;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, _gSanctuaryNo9AnimationSets, model, work->rig.poses, work->rig.slots);
    work->animId            = ANIMATION_NONE;
    task->msgTable          = _gSanctuaryNo9Messages;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state             = STATE_UPDATE;
}

/// Advances the golem's animation and samples lighting at its first animated part.
static void _sanctuaryNo9Update(Enemy* enemy, Task* task)
{
    _Work*    work;
    GfxCoord* partCoord;
    VECTOR*   worldPosition;
    s32       slotIndex;

    work      = task->work;
    partCoord = &task->extra.tmd->coords[FIRST_ANIMATED_PART];
    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    worldPosition = SCRATCH_STACK_CURSOR(VECTOR);
    if (work->animId != ANIMATION_NONE) {
        for (slotIndex = FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
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
static inline void _sanctuaryNo9RestartTracks(_Work* work)
{
    s32 slotIndex;

    for (slotIndex = FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
}

/// Restarts the golem's body animation for `ACTOR_MESSAGE_PLAY_ANIMATION`.
static s32 _sanctuaryNo9PlayAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Work* work;

    work         = task->work;
    work->animId = request->animationId;
    _sanctuaryNo9RestartTracks(work);
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_rot_matrix.inc.c"

/// Shows or hides the golem for `ACTOR_MESSAGE_SET_MODEL_DRAW`; bit 1 also skips its automatic primitive buffer.
static s32 _sanctuaryNo9SetModelDraw(Task* task, s32 msgId, s32 drawFlags, s32 unusedArg)
{
    enum {
        SKIP_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* model;

    model = task->extra.tmd;
    if (!(drawFlags & ACTOR_MESSAGE_DRAW_SHOW)) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    if (drawFlags & SKIP_AUTO_BUFFER) {
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
