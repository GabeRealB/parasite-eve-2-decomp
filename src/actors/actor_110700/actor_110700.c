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

static void func_actor_110700_80131E78(Enemy* enemy, Task* task);
static void func_actor_110700_80131F44(Enemy* enemy, Task* task);

static TmdSource _gActor110700No9GolemAkropolisBody;
s32              func_actor_110700_8013201C(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_110700_801320D8(Task*, s32, s32, s32);
void             func_actor_110700_80131E24(Task*);

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

TaskDesc D_actor_110700_8013BF94 = { { { TASK_BODY_TMD, 96 } }, func_actor_110700_80131E24, { .model = &_gActor110700No9GolemAkropolisBody } };

TaskMessageEntry D_actor_110700_8013BFA0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_110700_8013201C },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_110700_801320D8 },
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

/// The actor's task entry. Runs the handler for the task's current state,
/// passing the `Enemy` the task was spawned with: state 0 sets the actor up,
/// state 1 is its per-frame update. The two-entry handler table is built on
/// the stack on every call.
void func_actor_110700_80131E24(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_110700_80131E78,
        func_actor_110700_80131F44,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 0: allocates the work block, points the model at the block's light
/// and colour matrices, shows it, seeds the animation slots, installs the
/// message table and moves to state 1. If the allocation fails the enemy is
/// destroyed instead.
static void func_actor_110700_80131E78(Enemy* enemy, Task* task)
{
    GfxCoord*                 coord;
    TmdObject*                obj;
    _Actor110700No9GolemWork* work;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor110700No9GolemWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work    = work;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    obj->flags    = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_110700_8013BFC0, obj, work->rig.poses, work->rig.slots);
    work->animId        = 0;
    task->msgTable      = D_actor_110700_8013BFA0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state         = 1;
}

/// State 1, run every frame: ticks animation slots 1..0x12 once an animation
/// has been started, then pushes the world translation of the model's second
/// coordinate on the scratch stack and hands it to `worldCoordUpdateActorColor`.
static void func_actor_110700_80131F44(Enemy* enemy, Task* task)
{
    _Actor110700No9GolemWork* work;
    GfxCoord*                 coord;
    VECTOR*                   block;
    s32                       i;

    work  = task->work;
    coord = &task->extra.tmd->coords[1];
    SCRATCH_STACK_RESERVE_BYTES(0x10);
    block = SCRATCH_STACK_CURSOR(VECTOR);
    if (work->animId != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    worldCoordUpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Message 0x7D3 handler: starts the animation the payload names, storing its
/// id in the work block and reseeding slots 1..0x12 with it.
s32 func_actor_110700_8013201C(Task* task, s32 msgId, AnimationPlayRequest* args, s32 arg3)
{
    _Actor110700No9GolemWork* work;
    s32                       i;

    work         = task->work;
    work->animId = args->animationId;
    i            = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->animId);
        i++;
    } while (i < ARRAY_SIZE(work->rig.slots));
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Message 0x7D5 handler: sets the model's visibility from `arg2`. Bit 0 clear
/// replaces the object's flags with 0x80 (hidden), bit 0 set clears them
/// (shown); bit 1 then ORs in 0x4.
s32 func_actor_110700_801320D8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = task->extra.tmd;
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
