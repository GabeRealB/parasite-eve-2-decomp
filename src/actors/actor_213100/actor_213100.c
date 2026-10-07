#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Work block of Jodie Bouquet as a room script poses her: what her body model
/// plays, the matrices it is lit with and the model she holds.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work` for the
/// task's life. It opens with the nineteen-part rig and the model state, the
/// head a play request views as `ActorMotion19PlayWork`; she does not walk, so
/// the package's own state follows directly and `model.nextAnimId` is unused.
/// The model object borrows `model.light` and `model.color` for as long as the
/// block lives.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    Task*           heldModelTask; // Task drawing the model hung on body part 8, a hand; it is shown and hidden with the body. Never `NULL` past the spawn state, which exits when the spawn fails
    s32             freeCountdown; // Ticks left before the body model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor213100JodieBouquetWork;
STATIC_ASSERT_SIZEOF(_Actor213100JodieBouquetWork, 0x488);

/// Animation bank table the 0x7D3 handler indexes with the preset's
/// `field_0`.
extern AnimationSet*  D_actor_213100_8015217C[10];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// Spawn table the spawn state takes its child from; entry 1 is the child,
/// whose body is `_actor213100HeldModelTask`.
extern TaskDesc D_actor_213100_801521A8[];

/// Message table the spawn state installs at `Task::msgTable`: 0x7D3 is the
/// animation handler `_actorMotionPlayAnim19`, 0x7D4 the placement
/// handler `actorMsgPlaceEuler` and 0x7D5 the display handler
/// `_actor213100SetModelDraw`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_213100_801521C0[4];

/// Per-view visibility table the tick indexes with the session's current
/// view: nonzero shows the actor and its child, zero hides both.
extern s8 D_actor_213100_801521E0[];

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor213100UpdateBody(Task* actorTask);
static void _actor213100HeldModelIdle(Task* unusedTask);
static void _actor213100InitBody(Task* actorTask);
static void _actor213100ExitBody(Task* actorTask);
static void _actor213100InitBodyLighting(Task* actorTask);

static TmdSource _gActor213100JodieBouquetBody1;
static TmdSource _gActor213100Actor213000Prop;
static s32       _actor213100SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg);
static void      _actor213100HeldModelTask(Task* heldModelTask);
static void      _actor213100BodyTask(Task* actorTask);

static TmdBone _gActor213100JodieBouquetBody1Skeleton[19] = {
#include "assets/jodie_bouquet_body_1_skeleton.inc"
};

static u32 _gActor213100JodieBouquetBody1PartVerts[19] = {
#include "assets/jodie_bouquet_body_1_partVerts.inc"
};

static SVECTOR _gActor213100JodieBouquetBody1Verts[394] = {
#include "assets/jodie_bouquet_body_1_verts.inc"
};

static SVECTOR _gActor213100JodieBouquetBody1Normals[394] = {
#include "assets/jodie_bouquet_body_1_normals.inc"
};

static u32 _gActor213100JodieBouquetBody1Stream[4179] = {
#include "assets/jodie_bouquet_body_1_stream.inc"
};

static TmdSource _gActor213100JodieBouquetBody1 = {
    0,
    22872,
    6804,
    19,
    _gActor213100JodieBouquetBody1PartVerts,
    _gActor213100JodieBouquetBody1Verts,
    _gActor213100JodieBouquetBody1Normals,
    _gActor213100JodieBouquetBody1Skeleton,
    _gActor213100JodieBouquetBody1Stream,
};

static TmdBone _gActor213100Actor213000PropSkeleton[1] = {
#include "assets/actor_213000_prop_skeleton.inc"
};

static u32 _gActor213100Actor213000PropPartVerts[1] = {
#include "assets/actor_213000_prop_partVerts.inc"
};

static SVECTOR _gActor213100Actor213000PropVerts[14] = {
#include "assets/actor_213000_prop_verts.inc"
};

static u32 _gActor213100Actor213000PropStream[79] = {
#include "assets/actor_213000_prop_stream.inc"
};

static TmdSource _gActor213100Actor213000Prop = {
    0,
    528,
    0,
    1,
    _gActor213100Actor213000PropPartVerts,
    _gActor213100Actor213000PropVerts,
    &_gActor213100Actor213000PropVerts[14],
    _gActor213100Actor213000PropSkeleton,
    _gActor213100Actor213000PropStream,
};

static AnimationPackedPose _gActor213100Animation068BCBank1[6] = {
#include "assets/actor_213100_animation_068BC_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation068BCBank4[46] = {
#include "assets/actor_213100_animation_068BC_bank4.inc"
};

static AnimationRecord _gActor213100Animation068BCRecords[109] = {
#include "assets/actor_213100_animation_068BC_records.inc"
};

static u16 _gActor213100Animation068BCIndices[20] = {
#include "assets/actor_213100_animation_068BC_indices.inc"
};

static AnimationSet _gActor213100Animation068BC = {
    _gActor213100Animation068BCRecords,
    _gActor213100Animation068BCIndices,
    { NULL, _gActor213100Animation068BCBank1, NULL, NULL, _gActor213100Animation068BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation06C90Bank1[8] = {
#include "assets/actor_213100_animation_06C90_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation06C90Bank4[84] = {
#include "assets/actor_213100_animation_06C90_bank4.inc"
};

static AnimationRecord _gActor213100Animation06C90Records[117] = {
#include "assets/actor_213100_animation_06C90_records.inc"
};

static u16 _gActor213100Animation06C90Indices[20] = {
#include "assets/actor_213100_animation_06C90_indices.inc"
};

static AnimationSet _gActor213100Animation06C90 = {
    _gActor213100Animation06C90Records,
    _gActor213100Animation06C90Indices,
    { NULL, _gActor213100Animation06C90Bank1, NULL, NULL, _gActor213100Animation06C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation07080Bank1[7] = {
#include "assets/actor_213100_animation_07080_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation07080Bank4[62] = {
#include "assets/actor_213100_animation_07080_bank4.inc"
};

static AnimationRecord _gActor213100Animation07080Records[149] = {
#include "assets/actor_213100_animation_07080_records.inc"
};

static u16 _gActor213100Animation07080Indices[20] = {
#include "assets/actor_213100_animation_07080_indices.inc"
};

static AnimationSet _gActor213100Animation07080 = {
    _gActor213100Animation07080Records,
    _gActor213100Animation07080Indices,
    { NULL, _gActor213100Animation07080Bank1, NULL, NULL, _gActor213100Animation07080Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation073ECBank1[7] = {
#include "assets/actor_213100_animation_073EC_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation073ECBank4[74] = {
#include "assets/actor_213100_animation_073EC_bank4.inc"
};

static AnimationRecord _gActor213100Animation073ECRecords[104] = {
#include "assets/actor_213100_animation_073EC_records.inc"
};

static u16 _gActor213100Animation073ECIndices[20] = {
#include "assets/actor_213100_animation_073EC_indices.inc"
};

static AnimationSet _gActor213100Animation073EC = {
    _gActor213100Animation073ECRecords,
    _gActor213100Animation073ECIndices,
    { NULL, _gActor213100Animation073ECBank1, NULL, NULL, _gActor213100Animation073ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation0793CBank1[9] = {
#include "assets/actor_213100_animation_0793C_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation0793CBank4[100] = {
#include "assets/actor_213100_animation_0793C_bank4.inc"
};

static AnimationRecord _gActor213100Animation0793CRecords[193] = {
#include "assets/actor_213100_animation_0793C_records.inc"
};

static u16 _gActor213100Animation0793CIndices[20] = {
#include "assets/actor_213100_animation_0793C_indices.inc"
};

static AnimationSet _gActor213100Animation0793C = {
    _gActor213100Animation0793CRecords,
    _gActor213100Animation0793CIndices,
    { NULL, _gActor213100Animation0793CBank1, NULL, NULL, _gActor213100Animation0793CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation07CECBank1[7] = {
#include "assets/actor_213100_animation_07CEC_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation07CECBank4[81] = {
#include "assets/actor_213100_animation_07CEC_bank4.inc"
};

static AnimationRecord _gActor213100Animation07CECRecords[114] = {
#include "assets/actor_213100_animation_07CEC_records.inc"
};

static u16 _gActor213100Animation07CECIndices[20] = {
#include "assets/actor_213100_animation_07CEC_indices.inc"
};

static AnimationSet _gActor213100Animation07CEC = {
    _gActor213100Animation07CECRecords,
    _gActor213100Animation07CECIndices,
    { NULL, _gActor213100Animation07CECBank1, NULL, NULL, _gActor213100Animation07CECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation07EB0Bank1[3] = {
#include "assets/actor_213100_animation_07EB0_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation07EB0Bank4[27] = {
#include "assets/actor_213100_animation_07EB0_bank4.inc"
};

static AnimationRecord _gActor213100Animation07EB0Records[57] = {
#include "assets/actor_213100_animation_07EB0_records.inc"
};

static u16 _gActor213100Animation07EB0Indices[20] = {
#include "assets/actor_213100_animation_07EB0_indices.inc"
};

static AnimationSet _gActor213100Animation07EB0 = {
    _gActor213100Animation07EB0Records,
    _gActor213100Animation07EB0Indices,
    { NULL, _gActor213100Animation07EB0Bank1, NULL, NULL, _gActor213100Animation07EB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation08170Bank1[4] = {
#include "assets/actor_213100_animation_08170_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation08170Bank4[40] = {
#include "assets/actor_213100_animation_08170_bank4.inc"
};

static AnimationRecord _gActor213100Animation08170Records[104] = {
#include "assets/actor_213100_animation_08170_records.inc"
};

static u16 _gActor213100Animation08170Indices[20] = {
#include "assets/actor_213100_animation_08170_indices.inc"
};

static AnimationSet _gActor213100Animation08170 = {
    _gActor213100Animation08170Records,
    _gActor213100Animation08170Indices,
    { NULL, _gActor213100Animation08170Bank1, NULL, NULL, _gActor213100Animation08170Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213100Animation08334Bank1[3] = {
#include "assets/actor_213100_animation_08334_bank1.inc"
};

static AnimationPackedRotation _gActor213100Animation08334Bank4[27] = {
#include "assets/actor_213100_animation_08334_bank4.inc"
};

static AnimationRecord _gActor213100Animation08334Records[57] = {
#include "assets/actor_213100_animation_08334_records.inc"
};

static u16 _gActor213100Animation08334Indices[20] = {
#include "assets/actor_213100_animation_08334_indices.inc"
};

static AnimationSet _gActor213100Animation08334 = {
    _gActor213100Animation08334Records,
    _gActor213100Animation08334Indices,
    { NULL, _gActor213100Animation08334Bank1, NULL, NULL, _gActor213100Animation08334Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_213100_8015217C[10] = {
    NULL,
    &_gActor213100Animation068BC,
    &_gActor213100Animation06C90,
    &_gActor213100Animation07080,
    &_gActor213100Animation073EC,
    &_gActor213100Animation0793C,
    &_gActor213100Animation07CEC,
    &_gActor213100Animation07EB0,
    &_gActor213100Animation08170,
    &_gActor213100Animation08334,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_213100_8015217C,
};

TaskDesc D_actor_213100_801521A8[2] = {
    { { { TASK_BODY_TMD, 192 } }, _actor213100BodyTask, { .model = &_gActor213100JodieBouquetBody1 } },
    { { { TASK_BODY_TMD, 192 } }, _actor213100HeldModelTask, { .model = &_gActor213100Actor213000Prop } },
};

TaskMessageEntry D_actor_213100_801521C0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor213100SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s8 D_actor_213100_801521E0[24] = {
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Advances the body's eighteen child animation slots, leaving the root unchanged.
///
/// Requires an initialized nineteen-part rig and its borrowed animation bank.
static inline void _actor213100TickBodyAnimation(ActorAnimRig19* rig)
{
    enum { ACTOR_213100_FIRST_ANIMATED_PART = 1 };
    s32 slotIndex;

    for (slotIndex = ACTOR_213100_FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(rig->slots); slotIndex++) {
        animationTickSlot(&rig->anim, slotIndex);
    }
}

/// Updates Jodie's animation, ground shadow, lighting and paired model visibility.
///
/// Requires initialized body work, live body and held-model tasks, and a valid
/// room view index into `D_actor_213100_801521E0` (1..20 in M.I.S.T. parking).
/// The shadow uses body part 1's cached world position before the ready-view
/// lighting/visibility update. Its half-side is 768 world-coordinate units.
/// A pending buffer release advances even when hidden or the view is not ready;
/// the update observing zero frees only the body's primitive buffers.
static void _actor213100UpdateBody(Task* actorTask)
{
    enum {
        ACTOR_213100_LIGHTING_PART           = 1,
        ACTOR_213100_GROUND_SHADOW_HALF_SIZE = 0x300,
        ACTOR_213100_LIGHT_COUNT             = 3,
    };

    _Actor213100JodieBouquetWork* work;
    TmdObject*                    model;
    TmdObject*                    heldModel;
    VECTOR3                       groundPosition;

    work  = actorTask->work;
    model = actorTask->extra.tmd;
    if (work->model.ticking != 0) {
        _actor213100TickBodyAnimation(&work->rig);
    }
    // Sample the cached body transform before refreshing it for this view.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_213100_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART]);
        worldCoordSetModelLighting(model, actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].workm.t, 0, ACTOR_213100_LIGHT_COUNT);
        heldModel = work->heldModelTask->extra.tmd;
        if (D_actor_213100_801521E0[gGameSession->location.loc.view] != 0) {
            model->flags     &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            heldModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            heldModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
    // Zero is acted on before decrementing, then -1 disables further release.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// State table of the child the spawn state creates: attach to the parent,
/// idle, kill.
static const TaskFuncTable3 D_actor_213100_80149E24 = {
    {
        _modelPlacementAttachPartTask,
        _actor213100HeldModelIdle,
        taskKill,
    },
};

/// Dispatches the held model's attachment, idle and teardown states.
///
/// Requires a live TMD task with state 0..2. State 0 borrows the parent TMD
/// task from `spawnArg2.pointer` and its part index from `spawnArg1.value`;
/// the coordinate and lighting matrices remain borrowed through teardown.
static void _actor213100HeldModelTask(Task* heldModelTask)
{
    TaskFuncTable3 stateCallbacks;

    stateCallbacks = D_actor_213100_80149E24;
    stateCallbacks.funcs[heldModelTask->state](heldModelTask);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Leaves the attached held model under its parent's transform and lighting.
///
/// The task argument is ignored; attachment and resource lifetime are unchanged.
static void _actor213100HeldModelIdle(Task* unusedTask)
{
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213100_80149E30 = {
    {
        _actor213100InitBody,
        _actor213100UpdateBody,
        _actor213100ExitBody,
    },
};

/// Dispatches Jodie's body initialization, frame update and teardown states.
///
/// Requires a live TMD task with state 0..2 and a live owned `Enemy` in
/// `spawnArg2.pointer`. Initialization failure tears down that enemy and task;
/// frame updates require successfully initialized body work and held model.
static void _actor213100BodyTask(Task* actorTask)
{
    TaskFuncTable3 stateCallbacks;

    stateCallbacks = D_actor_213100_80149E30;
    stateCallbacks.funcs[actorTask->state](actorTask);
}

/// Initializes Jodie's body playback and spawns the model held on part 8.
///
/// Requires the fresh state-0 nineteen-part TMD task and its owned `Enemy` in
/// `spawnArg2.pointer`. Allocates primary-heap work owned by the task, lends
/// its lighting matrices to the body and starts bank 0, clip 5 without blending.
/// Both models begin hidden; the held task attaches on its first update.
/// Allocation failure exits the actor. Success installs its message/exit
/// callbacks and advances to the frame-update state.
static void _actor213100InitBody(Task* actorTask)
{
    enum {
        ACTOR_213100_BUFFER_FREE_NONE       = -1,
        ACTOR_213100_HELD_MODEL_DESCRIPTOR  = 1,
        ACTOR_213100_HELD_MODEL_PART        = 8,
        ACTOR_213100_INITIAL_ANIMATION_BANK = 0,
        ACTOR_213100_INITIAL_ANIMATION_ID   = 5,
    };

    _Actor213100JodieBouquetWork* work;
    AnimationPlayRequest          initialAnimation;
    TmdObject*                    model;
    TmdObject*                    heldModel;
    Task*                         heldModelTask;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(actorTask);
        return;
    }
    actorTask->work     = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = ACTOR_213100_BUFFER_FREE_NONE;
    heldModelTask       = taskSpawnFromTable(D_actor_213100_801521A8, ACTOR_213100_HELD_MODEL_DESCRIPTOR, ACTOR_213100_HELD_MODEL_PART, actorTask);
    work->heldModelTask = heldModelTask;
    if (heldModelTask == NULL) {
        enemyTaskExit(actorTask);
        return;
    }
    // The held task will borrow these matrices when it attaches to the body.
    _actor213100InitBodyLighting(actorTask);
    model                                 = actorTask->extra.tmd;
    model->flags                         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    heldModel                             = work->heldModelTask->extra.tmd;
    heldModel->flags                     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    initialAnimation.source.index         = ACTOR_213100_INITIAL_ANIMATION_BANK;
    initialAnimation.animationId          = ACTOR_213100_INITIAL_ANIMATION_ID;
    initialAnimation.blend                = ANIMATION_BLEND_RESET;
    initialAnimation.blendFrames          = 0;
    initialAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actorMotionPlayAnim19(actorTask, 0, &initialAnimation, 0);
    actorTask->msgTable     = D_actor_213100_801521C0;
    actorTask->exitCallback = _actor213100ExitBody;
    actorTask->state++;
}

/// Releases the actor's enemy object and tears down its body and attached model.
///
/// Used for state 2 and as the exit callback. Requires the live owned `Enemy`
/// in `spawnArg2.pointer`; task teardown releases body work and dispatches
/// attached children before releasing model resources. Do not use the work
/// or enemy after this call.
static void _actor213100ExitBody(Task* actorTask)
{
    enemyTaskExit(actorTask);
}

/// Lends the body's work-owned light-direction and light-colour matrices to its model.
///
/// Requires live body work and a TMD model. Replaces both matrix pointers
/// without initializing their values; the work must outlive every model use.
static void _actor213100InitBodyLighting(Task* actorTask)
{
    TmdObject*                    model;
    _Actor213100JodieBouquetWork* work;

    model           = actorTask->extra.tmd;
    work            = actorTask->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sets body drawing and buffer policy, then mirrors every flag to the held model.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` after successful initialization.
/// Modes: 0 hides and permits automatic buffer recovery; 1 shows, attempts
/// body-buffer allocation and permits recovery; 2 hides, suppresses recovery
/// and schedules body-buffer release; 3 shows and suppresses recovery without
/// allocation. Mode 2 frees on the third body update, after two decrements.
/// Other modes retain body flags but still copy them to the held model.
///
/// Modes 0/1/3 retain a pending release. A ready-view body update may replace
/// either model's active-draw flag with the room's visibility policy. Returns
/// 0 for modes 0..3, including allocation failure, and 1 otherwise.
/// The message ID and second payload are ignored; no payload is retained.
static s32 _actor213100SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg)
{
    enum {
        ACTOR_213100_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
        ACTOR_213100_BUFFER_FREE_DELAY_TICKS    = 2,
    };

    TmdObject*                    model;
    TmdObject*                    heldModel;
    _Actor213100JodieBouquetWork* work;
    s32                           result;

    model     = actorTask->extra.tmd;
    work      = actorTask->work;
    heldModel = work->heldModelTask->extra.tmd;
    result    = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = ACTOR_213100_BUFFER_FREE_DELAY_TICKS;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_213100_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    heldModel->flags = model->flags;
    return result;
}
