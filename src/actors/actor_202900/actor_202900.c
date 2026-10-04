#include "actors/actor_202900.h"

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
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

/// Work block of the package's actor, the ANMC woman of the Acropolis
/// cafeteria: allocated zeroed at its full size by her task's setup state and
/// kept both at `Task::work` and in a global the rest of the package reaches
/// it through.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives. Slots 1 to 18 of the rig are driven; slot 0, the root's, is never
/// started. The package gives her no walk, so of `st` only the animation
/// request and the sound cue are used: `st.cueRecord` is cleared by setup
/// alone, so the cue sounds once in the actor's life.
///
/// The allocation is 0xB0 bytes longer than the members the package uses. No
/// access to `pad_4B4` has been observed, and its role is unproven.
typedef struct {
    MATRIX          light; // Light-direction matrix lent to the model object
    MATRIX          color; // Light-colour matrix lent to the model object
    ActorAnimRig19  rig;   // Playback storage of the nineteen-part body model
    ActorEnemyState st;    // Animation request and the record a sound was last cued for; heading and walk stay zero
    byte            pad_4B4[0xB0];
} _Actor202900Work;
STATIC_ASSERT_SIZEOF(_Actor202900Work, 0x564);

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_202900_80156E0C[3];
extern TaskDesc         D_actor_202900_80156E24[];
extern u8               D_actor_202900_80156E3C[];

/// The actor's work block, published so the overlay's functions can reach it
/// without the task in hand.
extern _Actor202900Work* D_actor_202900_80156E54;

/// The actor's task, published by the setup handler so the overlay's other
/// functions can reach the actor's model without the task in hand.
extern Task* gActorSelfTask;

/// The second task the setup handler starts. Its model is textured from the
/// area record the actor was placed from, shown and hidden together with the
/// actor's, and the task is killed when the actor's exit callback runs.
extern Task* gActorHelperTask;

static void func_actor_202900_8014A0B4(Enemy* enemy, Task* task);
static void func_actor_202900_8014A158(Task* arg0);
static void func_actor_202900_8014A194(Task* arg0);
static void func_actor_202900_8014A208(void);
static void func_actor_202900_8014A260(void);
static void func_actor_202900_8014A304(void);
static s32  func_actor_202900_8014A394(void);

static TmdSource _gActor202900AnmcWomanCafeteriaBody;
static TmdSource _gActor202900Model0BC44;
void             func_actor_202900_8014A02C(Task*);
void             func_actor_202900_8014A088(Task*);

s32 func_actor_202900_8014A3E0(Task*, s32, AnimationPlayRequest*, s32);

static AnimationPackedPose _gActor202900Animation06098Bank1[158] = {
#include "assets/actor_202900_animation_06098_bank1.inc"
};

static AnimationPackedRotation _gActor202900Animation06098Bank4[2161] = {
#include "assets/actor_202900_animation_06098_bank4.inc"
};

static AnimationRecord _gActor202900Animation06098Records[3118] = {
#include "assets/actor_202900_animation_06098_records.inc"
};

static u16 _gActor202900Animation06098Indices[20] = {
#include "assets/actor_202900_animation_06098_indices.inc"
};

AnimationSet gActor202900Animation06098 = {
    _gActor202900Animation06098Records,
    _gActor202900Animation06098Indices,
    { NULL, _gActor202900Animation06098Bank1, NULL, NULL, _gActor202900Animation06098Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor202900AnmcWomanCafeteriaBodySkeleton[19] = {
#include "assets/anmc_woman_cafeteria_body_skeleton.inc"
};

static u32 _gActor202900AnmcWomanCafeteriaBodyPartVerts[19] = {
#include "assets/anmc_woman_cafeteria_body_partVerts.inc"
};

static SVECTOR _gActor202900AnmcWomanCafeteriaBodyVerts[377] = {
#include "assets/anmc_woman_cafeteria_body_verts.inc"
};

static SVECTOR _gActor202900AnmcWomanCafeteriaBodyNormals[406] = {
#include "assets/anmc_woman_cafeteria_body_normals.inc"
};

static u32 _gActor202900AnmcWomanCafeteriaBodyStream[4054] = {
#include "assets/anmc_woman_cafeteria_body_stream.inc"
};

static TmdSource _gActor202900AnmcWomanCafeteriaBody = {
    0,
    22188,
    6568,
    19,
    _gActor202900AnmcWomanCafeteriaBodyPartVerts,
    _gActor202900AnmcWomanCafeteriaBodyVerts,
    _gActor202900AnmcWomanCafeteriaBodyNormals,
    _gActor202900AnmcWomanCafeteriaBodySkeleton,
    _gActor202900AnmcWomanCafeteriaBodyStream,
};

static TmdBone _gActor202900Model0BC44Skeleton[1] = {
#include "assets/actor_202900_model_0BC44_skeleton.inc"
};

static u32 _gActor202900Model0BC44PartVerts[1] = {
#include "assets/actor_202900_model_0BC44_partVerts.inc"
};

static SVECTOR _gActor202900Model0BC44Verts[6] = {
#include "assets/actor_202900_model_0BC44_verts.inc"
};

static SVECTOR _gActor202900Model0BC44Normals[8] = {
#include "assets/actor_202900_model_0BC44_normals.inc"
};

static u32 _gActor202900Model0BC44Stream[57] = {
#include "assets/actor_202900_model_0BC44_stream.inc"
};

static TmdSource _gActor202900Model0BC44 = {
    0,
    320,
    0,
    1,
    _gActor202900Model0BC44PartVerts,
    _gActor202900Model0BC44Verts,
    _gActor202900Model0BC44Normals,
    _gActor202900Model0BC44Skeleton,
    _gActor202900Model0BC44Stream,
};

static AnimationPackedPose _gActor202900Animation0CDB4Bank1[25] = {
#include "assets/actor_202900_animation_0CDB4_bank1.inc"
};

static AnimationPackedRotation _gActor202900Animation0CDB4Bank4[427] = {
#include "assets/actor_202900_animation_0CDB4_bank4.inc"
};

static AnimationRecord _gActor202900Animation0CDB4Records[538] = {
#include "assets/actor_202900_animation_0CDB4_records.inc"
};

static u16 _gActor202900Animation0CDB4Indices[20] = {
#include "assets/actor_202900_animation_0CDB4_indices.inc"
};

static AnimationSet _gActor202900Animation0CDB4 = {
    _gActor202900Animation0CDB4Records,
    _gActor202900Animation0CDB4Indices,
    { NULL, _gActor202900Animation0CDB4Bank1, NULL, NULL, _gActor202900Animation0CDB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor202900Animation0CFC4Bank1[2] = {
#include "assets/actor_202900_animation_0CFC4_bank1.inc"
};

static AnimationPackedRotation _gActor202900Animation0CFC4Bank4[33] = {
#include "assets/actor_202900_animation_0CFC4_bank4.inc"
};

static AnimationRecord _gActor202900Animation0CFC4Records[73] = {
#include "assets/actor_202900_animation_0CFC4_records.inc"
};

static u16 _gActor202900Animation0CFC4Indices[20] = {
#include "assets/actor_202900_animation_0CFC4_indices.inc"
};

static AnimationSet _gActor202900Animation0CFC4 = {
    _gActor202900Animation0CFC4Records,
    _gActor202900Animation0CFC4Indices,
    { NULL, _gActor202900Animation0CFC4Bank1, NULL, NULL, _gActor202900Animation0CFC4Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_202900_80156E0C[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_202900_8014A3E0 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_202900_80156E24[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_202900_8014A02C, { .model = &_gActor202900AnmcWomanCafeteriaBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_202900_8014A088, { .model = &_gActor202900Model0BC44 } },
};

u8 D_actor_202900_80156E3C[24] = {
    0,
    0,
    0,
    0,
    212,
    107,
    21,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    228,
    109,
    21,
    128,
    0,
    0,
    0,
    0,
};

_Actor202900Work* D_actor_202900_80156E54 = NULL;

Task* gActorSelfTask;

Task* gActorHelperTask;

static void func_actor_202900_80149E24(Enemy* enemy, Task* task);

/// Setup handler, state 0 of the actor's update: allocates and publishes the
/// work block, starts the second task and textures its model from the area
/// record the actor was placed from, then seeds the animation context and runs
/// the first step body.
static void func_actor_202900_80149E24(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (D_actor_202900_80156E54 = memCalloc(sizeof(_Actor202900Work), false));
    if (D_actor_202900_80156E54 == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_202900_8014A158;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    gActorSelfTask                   = task;
    gActorHelperTask                 = Task_SpawnFromTable(D_actor_202900_80156E24, 1, 0, 0);
    actorTintTask(gActorHelperTask, enemy);
    obj->lightMtx = &D_actor_202900_80156E54->light;
    obj->colorMtx = &D_actor_202900_80156E54->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_202900_80156E54->rig.anim, D_actor_202900_80156E3C, obj,
                   D_actor_202900_80156E54->rig.poses);
    D_actor_202900_80156E54->st.animId    = 4;
    D_actor_202900_80156E54->st.state     = ACTOR_ENEMY_ANIM_RESET;
    D_actor_202900_80156E54->st.cueRecord = 0;
    task->msgTable                        = D_actor_202900_80156E0C;
    func_actor_202900_8014A194(task);
    task->state++;
}

/// Update of the actor's task: publishes the task's work block in
/// `D_actor_202900_80156E54`, so the overlay's other functions can reach it
/// without the task in hand, then dispatches on the task's state to the setup
/// handler `func_actor_202900_80149E24` (state 0) or the per-frame handler
/// `func_actor_202900_8014A0B4` (state 1), passing the task's enemy record
/// along with the task.
void func_actor_202900_8014A02C(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_202900_80149E24,
        func_actor_202900_8014A0B4,
    };

    D_actor_202900_80156E54 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Update of the second task: parents its model to the fifth coordinate of the
/// actor's model, marks the coordinate for recomputation and shows the model.
void func_actor_202900_8014A088(Task* arg0)
{
    GfxCoord*  parent;
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = arg0->extra.tmd;
    parent              = gActorSelfTask->extra.tmd->coords;
    coord               = extra->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    coord->parent       = parent + 4;
}

/// Per-frame handler, state 1 of the actor's update: passes the model and a
/// point 0x320 above its origin to `func_800D7A9C`, runs the step dispatcher,
/// and while animation 1 plays enqueues a sound event each time the second
/// animation slot reaches frame 0x15.
static void func_actor_202900_8014A0B4(Enemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj    = task->extra.tmd;
    coord  = obj->coords;
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_202900_8014A194(task);
    if (D_actor_202900_80156E54->st.animId == 1 && (func_actor_202900_8014A394() & 0xFF)) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_CAFETERIA_WOMAN_CUE, 0, 0);
    }
}

/// Exit callback: kills the second task and destroys the enemy.
static void func_actor_202900_8014A158(Task* arg0)
{
    taskKill(gActorHelperTask);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like the overlay's other
/// functions.
static void func_actor_202900_8014A194(Task* arg0)
{
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        func_actor_202900_8014A304();
        D_actor_202900_80156E54->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_RESET) {
        func_actor_202900_8014A260();
        D_actor_202900_80156E54->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_TICK) {
        func_actor_202900_8014A208();
    }
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
static void func_actor_202900_8014A208(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i]);
        i++;
    } while (i < 0x13);
}

/// Reseeds animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `st.appliedAnimId` as the one now
/// playing.
///
/// The third argument is the loop counter itself, and the two scaled induction variables are the
/// compiler's own, not a pair of source level pointers.
static void func_actor_202900_8014A260(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_202900_80156E54->rig.slots[i].rate = 1;
        Gp_AnimInitSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i], i,
                        D_actor_202900_80156E54->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `st.appliedAnimId` as the one now playing.
///
/// The third argument is the loop counter itself. Giving the call its own
/// counter copy (as m2c does) makes the preheader's `a2` initialisation a
/// separate pseudo, and the scheduler then orders the prologue saves around it
/// instead of leaving each `sw` paired with the load that overwrites it.
static void func_actor_202900_8014A304(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i], i,
                      D_actor_202900_80156E54->st.animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Watches the second animation slot for the frame the overlay reacts to:
/// while it holds 0x15, records it in `st.cueRecord` and reports whether that is
/// a change.
///
/// The mask is written at each use rather than hoisted into a `u16` local.
/// Hoisting makes the local a copy of the masked word, and combine then folds
/// the compare's zero-extension into a `move`; masking where the value is read
/// keeps the `andi $a1,$a0,0xffff`.
static s32 func_actor_202900_8014A394(void)
{
    u16 frame;

    frame = D_actor_202900_80156E54->rig.slots[1].currentPose.indices.recordIndex;
    if ((frame & ANIMATION_POSE_CUE_INDEX_MASK) == 0x15) {
        if (D_actor_202900_80156E54->st.cueRecord != (frame & ANIMATION_POSE_CUE_INDEX_MASK)) {
            D_actor_202900_80156E54->st.cueRecord = frame & ANIMATION_POSE_CUE_INDEX_MASK;
            return 1;
        }
        D_actor_202900_80156E54->st.cueRecord = frame & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    return 0;
}

/// Message 0x7D3 handler, animation start: seeds the work block's `animId` with the requested
/// one, rejecting anything from 5 up, and leaves the actor in step 2 with
/// `st.field_6` cleared before running the step dispatcher.
///
/// The actor is read into a local between the first two stores on purpose: that
/// is where the original evaluates it, and it is what puts the global's
/// `lui`/`lw` ahead of the `li 2` and leaves the `st.field_6` clear for the
/// call's delay slot.
s32 func_actor_202900_8014A3E0(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    Task* actor;

    if (args->animationId < 5) {
        D_actor_202900_80156E54->st.animId  = args->animationId;
        actor                               = gActorSelfTask;
        D_actor_202900_80156E54->st.state   = ACTOR_ENEMY_ANIM_RESET;
        D_actor_202900_80156E54->st.field_6 = 0;
        func_actor_202900_8014A194(actor);
        return 0;
    }
    return -1;
}

#include "../../shared/actor_messages_pair_visibility.inc.c"
