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
extern AnimationSet*    D_actor_202900_80156E3C[6];

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

static void _actor202900UpdateState(Enemy* unusedEnemy, Task* task);
static void _viewFigureExit(Task* task);
static void _actor202900StepAnim(Task* unusedTask);
static void _actor202900TickAnim(void);
static void _actor202900ResetAnim(void);
static void _actor202900BlendAnim(void);
static u8   _actor202900LatchSoundCue(void);

static TmdSource _gActor202900AnmcWomanCafeteriaBody;
static TmdSource _gActor202900Model0BC44;
void             func_actor_202900_8014A02C(Task*);
static void      _actor202900CarriedModelTask(Task* task);

static s32 _actor202900PlayAnimation(Task* unusedTask, s32 messageId,
                                     const AnimationPlayRequest* request, s32 unusedArgument);

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
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor202900PlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_202900_80156E24[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_202900_8014A02C, { .model = &_gActor202900AnmcWomanCafeteriaBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor202900CarriedModelTask, { .model = &_gActor202900Model0BC44 } },
};

AnimationSet* D_actor_202900_80156E3C[6] = {
    NULL,
    &_gActor202900Animation0CDB4,
    NULL,
    NULL,
    &_gActor202900Animation0CFC4,
    NULL,
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
    task->exitCallback               = _viewFigureExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    gActorSelfTask                   = task;
    gActorHelperTask                 = taskSpawnFromTable(D_actor_202900_80156E24, 1, 0, 0);
    actorTintTask(gActorHelperTask, enemy);
    obj->lightMtx = &D_actor_202900_80156E54->light;
    obj->colorMtx = &D_actor_202900_80156E54->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationBindModelContext(&D_actor_202900_80156E54->rig.anim, D_actor_202900_80156E3C, obj,
                              D_actor_202900_80156E54->rig.poses);
    D_actor_202900_80156E54->st.animId    = 4;
    D_actor_202900_80156E54->st.state     = ACTOR_ENEMY_ANIM_RESET;
    D_actor_202900_80156E54->st.cueRecord = 0;
    task->msgTable                        = D_actor_202900_80156E0C;
    _actor202900StepAnim(task);
    task->state++;
}

/// Update of the actor's task: publishes the task's work block in
/// `D_actor_202900_80156E54`, so the overlay's other functions can reach it
/// without the task in hand, then dispatches on the task's state to the setup
/// handler `func_actor_202900_80149E24` (state 0) or the per-frame handler
/// `_actor202900UpdateState` (state 1), passing the task's enemy record
/// along with the task.
void func_actor_202900_8014A02C(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_202900_80149E24,
        _actor202900UpdateState,
    };

    D_actor_202900_80156E54 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Keeps the carried model visible and attached to body part 4.
///
/// Both published tasks must own live TMD models. The carried model borrows
/// its parent's coordinate until helper teardown, which precedes body teardown.
static void _actor202900CarriedModelTask(Task* task)
{
    enum { ACTOR_202900_ATTACHMENT_PART = 4 };

    GfxCoord*  bodyCoords;
    GfxCoord*  rootCoord;
    TmdObject* model;

    model                   = task->extra.tmd;
    bodyCoords              = gActorSelfTask->extra.tmd->coords;
    rootCoord               = model->coords;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags            = 0;
    rootCoord->parent       = &bodyCoords[ACTOR_202900_ATTACHMENT_PART];
}

/// Samples the woman's full model lighting 800 world units above its origin.
///
/// Requires a composed root and live writable model lighting matrices.
static inline void _actor202900RelightModel(TmdObject* model)
{
    enum { ACTOR_202900_LIGHT_SAMPLE_HEIGHT = 800 };

    GfxCoord* rootCoord;
    VECTOR    lightPosition;

    rootCoord        = model->coords;
    lightPosition.vx = rootCoord->workm.t[0];
    lightPosition.vy = rootCoord->workm.t[1] - ACTOR_202900_LIGHT_SAMPLE_HEIGHT;
    lightPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ARRAY_SIZE(model->lightMtx->m));
}

/// Relights and animates the cafeteria woman, sounding her cue once per spawn.
///
/// State 1 requires the published work block and model bindings from setup.
/// `unusedEnemy` retains the setup/update dispatch signature. Clip 1 cues the
/// sound on slot 1's record 21; the latch is cleared only by setup, so later
/// playback requests cannot rearm it.
static void _actor202900UpdateState(Enemy* unusedEnemy, Task* task)
{
    enum { ACTOR_202900_SOUND_ANIM = 1 };

    TmdObject* model;

    model = task->extra.tmd;
    _actor202900RelightModel(model);
    _actor202900StepAnim(task);
    if (D_actor_202900_80156E54->st.animId == ACTOR_202900_SOUND_ANIM && _actor202900LatchSoundCue()) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_CAFETERIA_WOMAN_CUE, 0, 0);
    }
}

#include "../../shared/view_figure_exit.inc.c"

/// Applies a pending animation reseed or advances the running body tracks.
///
/// Uses the published live work block; `unusedTask` is ignored. A reseed enters
/// `ACTOR_ENEMY_ANIM_TICK` without a further tick, and other states do nothing.
static void _actor202900StepAnim(Task* unusedTask)
{
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _actor202900BlendAnim();
        D_actor_202900_80156E54->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _actor202900ResetAnim();
        D_actor_202900_80156E54->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (D_actor_202900_80156E54->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _actor202900TickAnim();
    }
}

/// Advances body parts 1 through 18 and writes their model coordinates.
///
/// Requires the published bound rig, initialized slots and loaded clip data.
/// Slot indices must equal their track indices for `animationTickDirectSlot`
/// to recover the array base. Root part 0 is left alone. Storage, scratch-stack
/// and GTE requirements follow `animationTickSlotPose`.
static void _actor202900TickAnim(void)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(D_actor_202900_80156E54->rig.slots); slotIndex++) {
        animationTickDirectSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[slotIndex]);
    }
}

/// Restarts body parts 1 through 18 on the requested clip without a blend.
///
/// Requires a bound rig and loaded tracks/coordinates for every driven part.
/// `animationInitDirectSlot` maps zero to set 1 and negative IDs to their
/// magnitude. Each slot finishes at `ANIMATION_RATE_ONE` without advancing or
/// writing a pose. Records the request in `st.appliedAnimId`; root 0 is untouched.
static void _actor202900ResetAnim(void)
{
    enum { ACTOR_202900_PRE_RESET_RATE = 1 }; // Sixteenths of a frame; initialization replaces it

    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(D_actor_202900_80156E54->rig.slots); slotIndex++) {
        D_actor_202900_80156E54->rig.slots[slotIndex].rate = ACTOR_202900_PRE_RESET_RATE;
        animationInitDirectSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[slotIndex], slotIndex,
                                D_actor_202900_80156E54->st.animId);
    }
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Reseeds body parts 1 through 18 with an eight-frame blend in demo scene 1.
///
/// Requires the published bound rig and loaded tracks. In demo scene 1, slots
/// must already be initialized with track indices matching their array indices;
/// each captures its ticked pose before blending to record offset zero.
/// Outside that scene, `animationStartDirectSlot` restarts the normalized set
/// without a blend. Root 0 is untouched; the requested clip is recorded in
/// `st.appliedAnimId`. Borrowed pose storage must remain live through playback.
static void _actor202900BlendAnim(void)
{
    enum { ACTOR_202900_BLEND_FRAMES = 8 };

    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(D_actor_202900_80156E54->rig.slots); slotIndex++) {
        animationStartDirectSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[slotIndex], slotIndex,
                                 D_actor_202900_80156E54->st.animId, 0, ACTOR_202900_BLEND_FRAMES);
    }
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Reports slot 1's first visit to sound-cue record 21 since setup.
///
/// Reads the current endpoint's low ten record-index bits, not elapsed frames.
/// Requires the published initialized rig. Latches 21 in `st.cueRecord` and
/// returns 1 on its first visit, otherwise 0; reseeds never clear this latch.
static u8 _actor202900LatchSoundCue(void)
{
    enum { ACTOR_202900_SOUND_CUE_RECORD = 21 };

    u16 recordIndex;

    recordIndex = D_actor_202900_80156E54->rig.slots[1].currentPose.indices.recordIndex;
    if ((recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_202900_SOUND_CUE_RECORD) {
        if (D_actor_202900_80156E54->st.cueRecord != (recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
            D_actor_202900_80156E54->st.cueRecord = recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            return 1;
        }
        D_actor_202900_80156E54->st.cueRecord = recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    return 0;
}

/// Restarts the published woman's requested animation synchronously.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, message ID and second
/// payload are ignored. Reads only `request->animationId`, borrowing the
/// request through this call; bank, blend and collision choices are ignored.
/// Returns -1 unchanged for IDs 5 and above, otherwise narrows the ID to s16,
/// clears `st.field_6` (role unproven) and resets all driven slots. Returns 0.
/// The upper-bound check does not validate negative IDs or unloaded table entries;
/// callers must select a normalized loaded set (1 or 4) after narrowing.
/// The bound rig and clip storage must remain live throughout playback.
static s32 _actor202900PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_202900_ANIM_ID_LIMIT = 5 };

    Task* actorTask;

    if (request->animationId < ACTOR_202900_ANIM_ID_LIMIT) {
        D_actor_202900_80156E54->st.animId  = request->animationId;
        actorTask                           = gActorSelfTask;
        D_actor_202900_80156E54->st.state   = ACTOR_ENEMY_ANIM_RESET;
        D_actor_202900_80156E54->st.field_6 = 0;
        _actor202900StepAnim(actorTask);
        return 0;
    }
    return -1;
}

#include "../../shared/actor_messages_pair_visibility.inc.c"
