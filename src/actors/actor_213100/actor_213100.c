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
#include "../../shared/model_placement.h"

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
/// whose body is `func_actor_213100_80149FE4`.
extern TaskDesc D_actor_213100_801521A8[];

/// Message table the spawn state installs at `Task::msgTable`: 0x7D3 is the
/// animation handler `actorMotionPlayAnim19`, 0x7D4 the placement
/// handler `actorMsgPlaceEuler` and 0x7D5 the display handler
/// `func_actor_213100_8014A40C`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_213100_801521C0[4];

/// Per-view visibility table the tick indexes with the session's current
/// view: nonzero shows the actor and its child, zero hides both.
extern s8 D_actor_213100_801521E0[];

static void func_actor_213100_8014A0B8(Task* task);
static void func_actor_213100_8014A118(Task* arg0);
static void func_actor_213100_8014A21C(Task* arg0);
static void func_actor_213100_8014A23C(Task* arg0);

static TmdSource _gActor213100JodieBouquetBody1;
static TmdSource _gActor213100Actor213000Prop;
s32              func_actor_213100_8014A40C(Task*, s32, s32, s32);
void             func_actor_213100_80149FE4(Task*);
void             func_actor_213100_8014A0C0(Task*);

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
    { { { TASK_BODY_TMD, 192 } }, func_actor_213100_8014A0C0, { .model = &_gActor213100JodieBouquetBody1 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213100_80149FE4, { .model = &_gActor213100Actor213000Prop } },
};

TaskMessageEntry D_actor_213100_801521C0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_213100_8014A40C },
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

static void func_actor_213100_80149E3C(Task* task);

/// Per-frame tick: ticks the work block's animation slots once they have been
/// started, and while the model is shown samples the child part's
/// translation through `func_800EA1A8` and draws the ground shadow where it
/// hits. Once the view is ready, rebuilds that part's world matrix, hands its
/// translation to `worldCoordSetModelLighting`, and shows or hides this model and the
/// child's together from the per-view table. `freeCountdown` then frees the
/// model's buffers as it reaches zero.
static void func_actor_213100_80149E3C(Task* task)
{
    _Actor213100JodieBouquetWork* work;
    TmdObject*                    extra;
    TmdObject*                    child;
    VECTOR3                       pos;
    s32                           i;

    work  = task->work;
    extra = task->extra.tmd;
    if (work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(extra, task->extra.tmd->coords[1].workm.t, 0, 3);
        child = work->heldModelTask->extra.tmd;
        if (D_actor_213100_801521E0[gGameSession->location.loc.view] != 0) {
            extra->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            child->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            child->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(extra);
        }
        work->freeCountdown--;
    }
}

/// State table of the child the spawn state creates: attach to the parent,
/// idle, kill.
static const TaskFuncTable3 D_actor_213100_80149E24 = {
    {
        modelPlacementAttachPart,
        func_actor_213100_8014A0B8,
        taskKill,
    },
};

/// Body of the child task: dispatches on its state through
/// `D_actor_213100_80149E24`.
void func_actor_213100_80149FE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// The child's idle state: does nothing.
static void func_actor_213100_8014A0B8(Task* task)
{
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213100_80149E30 = {
    {
        func_actor_213100_8014A118,
        func_actor_213100_80149E3C,
        func_actor_213100_8014A21C,
    },
};

/// Body of the actor's task: dispatches on its state through
/// `D_actor_213100_80149E30`.
void func_actor_213100_8014A0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E30;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the work block into `Task::work`, seeds its clip
/// and bank to `ACTOR_MODEL_STATE_NONE` and `freeCountdown` to -1, then spawns
/// the child from entry 1 of the spawn table, attached to part 8 of this
/// actor's skeleton and kept in `heldModelTask`. Either allocation failing
/// exits the task instead. Both models start hidden and
/// take the work block's matrices; the actor starts its animation by calling
/// the 0x7D3 handler directly with the preset `{ 0, 5, 0, 0, 0 }`, then
/// installs its message table and exit callback and advances to the tick.
static void func_actor_213100_8014A118(Task* arg0)
{
    _Actor213100JodieBouquetWork* work;
    AnimationPlayRequest          preset;
    TmdObject*                    ext;
    Task*                         child;

    work = memCalloc(sizeof(_Actor213100JodieBouquetWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = -1;
    child               = taskSpawnFromTable(D_actor_213100_801521A8, 1, 8, arg0);
    work->heldModelTask = child;
    if (child == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    func_actor_213100_8014A23C(arg0);
    ext                         = arg0->extra.tmd;
    ext->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    ext                         = work->heldModelTask->extra.tmd;
    ext->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    preset.source.index         = 0;
    preset.animationId          = 5;
    preset.blend                = ANIMATION_BLEND_RESET;
    preset.blendFrames          = 0;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    actorMotionPlayAnim19(arg0, 0, &preset, 0);
    arg0->msgTable     = D_actor_213100_801521C0;
    arg0->exitCallback = func_actor_213100_8014A21C;
    arg0->state++;
}

/// The actor's teardown state and its `Task::exitCallback`: hands the task to
/// `enemyTaskExit`.
static void func_actor_213100_8014A21C(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Points the model's light and colour matrices at the work block's own pair.
static void func_actor_213100_8014A23C(Task* arg0)
{
    TmdObject*                    ext;
    _Actor213100JodieBouquetWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 display handler: switches on the message's mode word, then
/// copies the model's flags onto the child's model. Mode 0 hides the model
/// and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 1 shows it, reallocates its buffers through
/// `tmdAllocPrimitiveBuffer` and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 2 hides it, sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` and starts
/// `freeCountdown` at two ticks, after which the tick frees the buffers; 3
/// shows it and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. The handled modes return 0; any other mode changes
/// nothing on this model and returns 1.
s32 func_actor_213100_8014A40C(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*                    obj;
    TmdObject*                    other;
    _Actor213100JodieBouquetWork* work;
    s32                           ret;

    obj   = task->extra.tmd;
    work  = task->work;
    other = work->heldModelTask->extra.tmd;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}
