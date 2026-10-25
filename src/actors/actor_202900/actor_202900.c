#include "actors/actor_202900.h"
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
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"

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

/// Borrowed body task of this package's active scripted actor.
///
/// Published after successful spawn for animation messages, paired visibility
/// and model attachments. The body has 19 coordinates. Only the current live
/// instance may be used: a later spawn replaces the pointer, and teardown
/// leaves it dangling rather than clearing it. This cache owns no task.
static Task* _gActor202900BodyTask;

/// Borrowed helper-model task paired with this carrier's published body task.
///
/// Successful body setup publishes the handle for paired visibility and model
/// parenting. Teardown kills the helper before releasing the body; neither
/// published handle is cleared. Consumers require the current pair to be live.
static Task* _gActorMsgHelperTask;

static void _actor202900UpdateState(Enemy* unusedEnemy, Task* task);
static void _viewFigureExit(Task* task);
static void _actor202900StepAnim(Task* unusedTask);
static void _actor202900TickAnim(void);
static void _actor202900ResetAnim(void);
static void _actor202900BlendAnim(void);
static u8   _actor202900LatchSoundCue(void);

static TmdSource _gAnmcWoman2Body;
static TmdSource _gAnmcWoman2Glasses;
static void      _actor202900BodyTask(Task* task);
static void      _actor202900CarriedModelTask(Task* task);

static s32 _actor202900PlayAnimation(Task* unusedTask, s32 messageId,
                                     const AnimationPlayRequest* request, s32 unusedArgument);

#include "assets/rooms/akropolis/cafeteria/anmc_woman_clips_1.inc"

#include "assets/characters/anmc_woman_2/body.inc"

#include "assets/characters/anmc_woman_2/glasses.inc"

#include "assets/rooms/akropolis/cafeteria/anmc_woman_clips_2.inc"

/// Cafeteria woman's animation and paired-model visibility messages with an end sentinel.
///
/// The body task borrows this three-entry table. Animation requests require a
/// loaded normalized clip (1 or 4); visibility applies to the body and its helper.
static TaskMessageEntry _gActor202900WomanMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor202900PlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor202900WomanTaskDescriptors[2] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor202900BodyTask,
        .data     = { .model = &_gAnmcWoman2Body },
    },
    // 1: carried model.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor202900CarriedModelTask,
        .data     = { .model = &_gAnmcWoman2Glasses },
    },
};

/// Cafeteria woman's six-entry animation bank, with loaded sets 1 and 4.
///
/// Entries 0, 2, 3 and 5 are unplayable NULL holes. Setup starts 4; set 1 carries
/// the sound cue. Direct-slot initialization maps request 0 to 1 and negatives
/// to their magnitude, which must still select a loaded entry. The rig borrows
/// this table and its clips while the overlay is loaded.
#include "assets/rooms/akropolis/cafeteria/woman_bank.inc"

/// Borrowed singleton work of the cafeteria woman's body and animation messages.
///
/// Dispatch publishes Task::work and successful setup replaces it with the
/// allocated block. Animation, cue and message handlers require the current
/// initialized instance; enemy teardown frees it without clearing this pointer.
static _Actor202900Work* _gActor202900Work = NULL;

static void _actor202900SpawnBody(Enemy* enemy, Task* task);

/// Initializes the cafeteria woman's view-parented body and carried model.
///
/// State 0 owns a zeroed work block until the exit callback; allocation failure
/// destroys the enemy. The body borrows work-owned lighting and animation storage.
/// Requires a loaded placement and successful carried-model spawn: the returned
/// child is used without a NULL check. Starts clip 4 before advancing task state.
static void _actor202900SpawnBody(Enemy* enemy, Task* task)
{
    enum { ACTOR_202900_CARRIED_MODEL_TASK        = 1,
           ACTOR_202900_INITIAL_CLIP              = 4,
           ACTOR_202900_BODY_OT_OFFSET            = 1,
           ACTOR_202900_SPAWN_LIGHT_SAMPLE_HEIGHT = 800 };
    VECTOR     lightPosition;
    GfxCoord*  rootCoord;
    TmdObject* model;

/// Samples all three model lights from cached root XYZ with Y decreased by 800.
///
/// Captures live model/rootCoord, writable VECTOR lightPosition and the local
/// ACTOR_202900_SPAWN_LIGHT_SAMPLE_HEIGHT. Does not compose the root or refresh
/// workm; XYZ must already be suitable for the lighting query. The model borrows
/// work-owned writable light/color matrices until teardown. Only sample XYZ are
/// consumed, synchronously. Invoke within a braced statement body.
#define ACTOR_202900_RELIGHT_SPAWN_MODEL()                                                    \
    {                                                                                         \
        lightPosition.vx = rootCoord->workm.t[0];                                             \
        lightPosition.vy = rootCoord->workm.t[1] - ACTOR_202900_SPAWN_LIGHT_SAMPLE_HEIGHT;    \
        lightPosition.vz = rootCoord->workm.t[2];                                             \
        worldCoordSetModelLighting(model, &lightPosition, 0, ARRAY_SIZE(model->lightMtx->m)); \
    }

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (_gActor202900Work = memCalloc(sizeof(_Actor202900Work), false));
    if (_gActor202900Work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _viewFigureExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_202900_BODY_OT_OFFSET;
    model->flags                     = 0;
    _gActor202900BodyTask            = task;
    _gActorMsgHelperTask             = taskSpawnFromTable(gActor202900WomanTaskDescriptors, ACTOR_202900_CARRIED_MODEL_TASK, 0, 0);
    _actorRenderApplyTaskPlacementTextureOffsets(_gActorMsgHelperTask, enemy);
    model->lightMtx = &_gActor202900Work->light;
    model->colorMtx = &_gActor202900Work->color;
    // Bind work-owned lighting before sampling it and starting the body tracks.
    ACTOR_202900_RELIGHT_SPAWN_MODEL();
    animationBindModelContext(&_gActor202900Work->rig.anim, _gActor202900WomanAnimationSets, model,
                              _gActor202900Work->rig.poses);
    _gActor202900Work->st.animId    = ACTOR_202900_INITIAL_CLIP;
    _gActor202900Work->st.state     = ACTOR_ENEMY_ANIM_RESET;
    _gActor202900Work->st.cueRecord = 0;
    task->msgTable                  = _gActor202900WomanMessages;
    _actor202900StepAnim(task);
    task->state++;

#undef ACTOR_202900_RELIGHT_SPAWN_MODEL
}

/// Dispatches the cafeteria ANMC woman's initialization or body animation update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a nineteen-part
/// TMD body and state 0..1. Publishes the task-owned work for singleton message
/// and animation handlers before dispatch. State 0 allocates it and starts the
/// carried model; state 1 relights and animates the stationary body and sounds
/// its one-shot animation cue.
/// Initialization may destroy the enemy and task on allocation failure.
/// The handler array has no bounds check; keep this overlay and clips loaded.
static void _actor202900BodyTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor202900SpawnBody,
        _actor202900UpdateState,
    };

    _gActor202900Work = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
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
    bodyCoords              = _gActor202900BodyTask->extra.tmd->coords;
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
    if (_gActor202900Work->st.animId == ACTOR_202900_SOUND_ANIM && _actor202900LatchSoundCue()) {
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_CAFETERIA_WOMAN_CUE, 0, 0);
    }
}

#include "../../shared/actor/view_figure_exit.inc.c"

/// Applies a pending animation reseed or advances the running body tracks.
///
/// Uses the published live work block; `unusedTask` is ignored. A reseed enters
/// `ACTOR_ENEMY_ANIM_TICK` without a further tick, and other states do nothing.
static void _actor202900StepAnim(Task* unusedTask)
{
    if (_gActor202900Work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _actor202900BlendAnim();
        _gActor202900Work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gActor202900Work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _actor202900ResetAnim();
        _gActor202900Work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gActor202900Work->st.state == ACTOR_ENEMY_ANIM_TICK) {
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

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gActor202900Work->rig.slots); slotIndex++) {
        animationTickDirectSlot(&_gActor202900Work->rig.anim, &_gActor202900Work->rig.slots[slotIndex]);
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

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gActor202900Work->rig.slots); slotIndex++) {
        _gActor202900Work->rig.slots[slotIndex].rate = ACTOR_202900_PRE_RESET_RATE;
        animationInitDirectSlot(&_gActor202900Work->rig.anim, &_gActor202900Work->rig.slots[slotIndex], slotIndex,
                                _gActor202900Work->st.animId);
    }
    _gActor202900Work->st.appliedAnimId = _gActor202900Work->st.animId;
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

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gActor202900Work->rig.slots); slotIndex++) {
        animationStartDirectSlot(&_gActor202900Work->rig.anim, &_gActor202900Work->rig.slots[slotIndex], slotIndex,
                                 _gActor202900Work->st.animId, 0, ACTOR_202900_BLEND_FRAMES);
    }
    _gActor202900Work->st.appliedAnimId = _gActor202900Work->st.animId;
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

    recordIndex = _gActor202900Work->rig.slots[1].currentPose.indices.recordIndex;
    if ((recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_202900_SOUND_CUE_RECORD) {
        if (_gActor202900Work->st.cueRecord != (recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
            _gActor202900Work->st.cueRecord = recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            return 1;
        }
        _gActor202900Work->st.cueRecord = recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
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
        _gActor202900Work->st.animId  = request->animationId;
        actorTask                     = _gActor202900BodyTask;
        _gActor202900Work->st.state   = ACTOR_ENEMY_ANIM_RESET;
        _gActor202900Work->st.field_6 = 0;
        _actor202900StepAnim(actorTask);
        return 0;
    }
    return -1;
}

/// Selects the published cafeteria woman's body for paired visibility.
///
/// Expands to a `Task*` identifier with no evaluation side effects; this
/// fragment only reads it, independently of the message receiver. Requires
/// the current live TMD body and its matching helper in `_gActorMsgHelperTask`.
/// Successful spawn replaces the borrowed pointer; teardown does not clear it.
/// Bind only around the pair-visibility fragment and never dispatch after teardown.
#define ACTOR_MESSAGE_BODY_TASK _gActor202900BodyTask
#include "../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK
