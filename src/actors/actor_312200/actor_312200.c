#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor_contacts.h"

extern u8 D_actor_312200_80169F44[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_312200_80169F5C[4];

/// Whole-unit part of the last step `ActorContact_PushContact` applied.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// Private work block of the actor 312200 task, hanging off `Task::work`,
/// `memCalloc(sizeof(Actor312200Work), 0)` in the spawn handler.
///
/// Only the fields the matched code touches are named so far: `yaw` is the
/// heading `func_actor_312200_801635CC` reads back from the root coordinate.
///
/// `field_0` is the state word the 0x7DB handler raises and the per-tick
/// handler dispatches on,
/// `field_4` the live-actor flag every state handler tests on entry, `field_88C`
/// the work state, and `field_892` / `field_896` the two timers the state
/// handlers arm.
///
/// `field_8B4` / `field_8B6` / `field_8B8` are the record
/// `func_actor_312200_801636CC` leaves of the last 0x7DB command it saw: the
/// sender id's two bytes - stored as the bytes they are read as, not as the
/// halfword the handler tests - and then the action halfword.
///
/// `field_8BC` is the display node the spawn handler
/// `func_actor_312200_80163178` builds in place and hands to `Gp_LinkObj` - the
/// `WorldCollisionBody` whose `context.contacts` is a three-entry `WorldCollisionContact` table at 0x8DC.
/// `func_actor_312200_80163778` clears bit 0x8000 of that node's `flags`.
typedef struct Actor312200Work {
    /* 0x000 */ s16 field_0;
    /// Second halfword of the state word above, set to -1 by the spawn handler.
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x2];
    /* 0x008 */ s16  yaw;
    /* 0x00A */ byte pad_A[0x6];
    /// Animation context the spawn body hands `animationInitContext` first, with its
    /// 19 slots directly behind it: the pose buffer that function is handed
    /// fourth starts directly after the 19 slots.
    /* 0x010 */ ActorAnimRig19 rig;
    /// Second animation context, seeded when the 0x89A request word is 2. It
    /// lives inside the pose buffer the first context was handed, and the slots
    /// it resets are the first context's, so the two share their slot array.
    /* 0x44C */ AnimationContext anim2;
    /* 0x460 */ byte             poses2[0x42C];
    /* 0x88C */ s16              field_88C;
    /* 0x88E */ byte             pad_88E[0x2];
    /* 0x890 */ s16              field_890;
    /* 0x892 */ u16              field_892;
    /* 0x894 */ u16              field_894;
    /* 0x896 */ u16              field_896;
    /* 0x898 */ byte             pad_898[0x2];
    /// Request state of the second animation context, laid out like the first:
    /// 2 seeds every slot and settles on 3.
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ u16  field_89E;
    /* 0x8A0 */ s16  field_8A0;
    /* 0x8A2 */ byte pad_8A2[0x6];
    /* 0x8A8 */ s32  field_8A8;
    /// The two bytes the spawn handler arms next to the display node; they sit
    /// immediately before the 0x7DB record, so they are the actor's own copy of
    /// that state rather than part of a message. `field_8AD` is read back with
    /// `lb` by the tick handler, so it is signed like the record halfwords.
    /* 0x8AC */ u8   field_8AC;
    /* 0x8AD */ s8   field_8AD;
    /* 0x8AE */ byte pad_8AE[0x6];
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s16  field_8B6;
    /* 0x8B8 */ s16  field_8B8;
    /* 0x8BA */ byte pad_8BA[0x2];
    /// Display node: `WorldCollisionBody` at 0x8BC, its `WorldCollisionContact` table at 0x8DC.
    /* 0x8BC */ WorldCollisionBody    field_8BC;
    /* 0x8DC */ WorldCollisionContact recs[3];
    /* 0x924 */ byte                  pad_924[0x20];
    /// The light / colour matrices the spawn handler stores into
    /// `TmdObject::lightMtx` / `colorMtx`, at the top of the block.
    /* 0x944 */ MATRIX light;
    /* 0x964 */ MATRIX color;
} Actor312200Work;
STATIC_ASSERT_SIZEOF(Actor312200Work, 0x984);

/// Step table the seeding body `func_actor_312200_80162FB4` walks: one 5-byte
/// row per clip the previous request latched in `Actor312200Work::field_890`,
/// addressed by the requested clip in `field_892`. The byte it reads is handed
/// to `animationSeekSlotWithBlend` as the request's fifth argument.
extern s8 D_actor_312200_80169F28[][5];

static void func_actor_312200_80163778(Task* task);
static void func_actor_312200_801637CC(Task* task);

static TmdSource _gActor312200SwatMember1Body;
s32              func_actor_312200_80163510(Task*, s32, s32, s32);
s32              func_actor_312200_801635CC(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32              func_actor_312200_801636CC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_312200_80163854(Task*);

static TmdBone _gActor312200SwatMember1BodySkeleton[19] = {
#include "assets/swat_member_1_body_skeleton.inc"
};

static u32 _gActor312200SwatMember1BodyPartVerts[19] = {
#include "assets/swat_member_1_body_partVerts.inc"
};

static SVECTOR _gActor312200SwatMember1BodyVerts[296] = {
#include "assets/swat_member_1_body_verts.inc"
};

static SVECTOR _gActor312200SwatMember1BodyNormals[294] = {
#include "assets/swat_member_1_body_normals.inc"
};

static u32 _gActor312200SwatMember1BodyStream[3276] = {
#include "assets/swat_member_1_body_stream.inc"
};

static TmdSource _gActor312200SwatMember1Body = {
    0,
    17428,
    5584,
    19,
    _gActor312200SwatMember1BodyPartVerts,
    _gActor312200SwatMember1BodyVerts,
    _gActor312200SwatMember1BodyNormals,
    _gActor312200SwatMember1BodySkeleton,
    _gActor312200SwatMember1BodyStream,
};

static AnimationPackedPose _gActor312200Animation07368Bank1[21] = {
#include "assets/actor_312200_animation_07368_bank1.inc"
};

static AnimationPackedRotation _gActor312200Animation07368Bank4[408] = {
#include "assets/actor_312200_animation_07368_bank4.inc"
};

static AnimationRecord _gActor312200Animation07368Records[550] = {
#include "assets/actor_312200_animation_07368_records.inc"
};

static u16 _gActor312200Animation07368Indices[20] = {
#include "assets/actor_312200_animation_07368_indices.inc"
};

static AnimationSet _gActor312200Animation07368 = {
    _gActor312200Animation07368Records,
    _gActor312200Animation07368Indices,
    { NULL, _gActor312200Animation07368Bank1, NULL, NULL, _gActor312200Animation07368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor312200Animation0773CBank1[3] = {
#include "assets/actor_312200_animation_0773C_bank1.inc"
};

static AnimationPackedRotation _gActor312200Animation0773CBank4[70] = {
#include "assets/actor_312200_animation_0773C_bank4.inc"
};

static AnimationRecord _gActor312200Animation0773CRecords[146] = {
#include "assets/actor_312200_animation_0773C_records.inc"
};

static u16 _gActor312200Animation0773CIndices[20] = {
#include "assets/actor_312200_animation_0773C_indices.inc"
};

static AnimationSet _gActor312200Animation0773C = {
    _gActor312200Animation0773CRecords,
    _gActor312200Animation0773CIndices,
    { NULL, _gActor312200Animation0773CBank1, NULL, NULL, _gActor312200Animation0773CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor312200Animation07D78Bank1[4] = {
#include "assets/actor_312200_animation_07D78_bank1.inc"
};

static AnimationPackedRotation _gActor312200Animation07D78Bank4[157] = {
#include "assets/actor_312200_animation_07D78_bank4.inc"
};

static AnimationRecord _gActor312200Animation07D78Records[210] = {
#include "assets/actor_312200_animation_07D78_records.inc"
};

static u16 _gActor312200Animation07D78Indices[20] = {
#include "assets/actor_312200_animation_07D78_indices.inc"
};

static AnimationSet _gActor312200Animation07D78 = {
    _gActor312200Animation07D78Records,
    _gActor312200Animation07D78Indices,
    { NULL, _gActor312200Animation07D78Bank1, NULL, NULL, _gActor312200Animation07D78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor312200Animation080E0Bank1[3] = {
#include "assets/actor_312200_animation_080E0_bank1.inc"
};

static AnimationPackedRotation _gActor312200Animation080E0Bank4[57] = {
#include "assets/actor_312200_animation_080E0_bank4.inc"
};

static AnimationRecord _gActor312200Animation080E0Records[132] = {
#include "assets/actor_312200_animation_080E0_records.inc"
};

static u16 _gActor312200Animation080E0Indices[20] = {
#include "assets/actor_312200_animation_080E0_indices.inc"
};

static AnimationSet _gActor312200Animation080E0 = {
    _gActor312200Animation080E0Records,
    _gActor312200Animation080E0Indices,
    { NULL, _gActor312200Animation080E0Bank1, NULL, NULL, _gActor312200Animation080E0Bank4, NULL, NULL, NULL },
};

s8 D_actor_312200_80169F28[5][5] = { 0 };

u8 D_actor_312200_80169F44[24] = {
    0,
    0,
    0,
    0,
    136,
    145,
    22,
    128,
    92,
    149,
    22,
    128,
    152,
    155,
    22,
    128,
    0,
    159,
    22,
    128,
    0,
    0,
    0,
    0,
};

TaskMessageEntry D_actor_312200_80169F5C[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_312200_80163510 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_312200_801636CC },
    { ACTOR_MESSAGE_PLACE, func_actor_312200_801635CC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_312200_80169F7C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_312200_80163854, { .model = &_gActor312200SwatMember1Body } };

SVECTOR ActorContact_ScratchPosition;

static void func_actor_312200_80162FB4(Task* task);
static void func_actor_312200_80163178(Enemy* enemy, Task* task);
static void func_actor_312200_80163370(Enemy* enemy, Task* task);

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Animation seeding body, run once per tick: request state 1 seeks every slot
/// of the first context to `field_892` through the step table, state 2 resets
/// them, and both settle on 3 and clear the frame counter at `field_894`.
/// Request state 2 on the second context resets its slots at rate 0x30, then
/// the tail counts a frame and ticks every slot of the first context.
static void func_actor_312200_80162FB4(Task* task)
{
    Actor312200Work* work;
    Actor312200Work* start;
    Actor312200Work* reset;
    Actor312200Work* second;
    Actor312200Work* tick;
    s32              i;
    s32              j;
    s32              k;
    s32              m;

    work = (Actor312200Work*)task->work;
    if (work->field_88C == 1) {
        start = (Actor312200Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->rig.slots[i].rate = start->field_896;
            animationSeekSlotWithBlend(&start->rig.anim, i, (s16)start->field_892, 0,
                                       D_actor_312200_80169F28[start->field_890][(s16)start->field_892]);
        }
        start->field_890 = start->field_892;
        goto advance;
    }
    if (work->field_88C == 2) {
        reset = (Actor312200Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->rig.slots[j].rate = reset->field_896;
            animationResetSlot(&reset->rig.anim, j, (s16)reset->field_892);
        }
        reset->field_890 = reset->field_892;
    advance:
        work->field_88C = 3;
        work->field_894 = 0;
        work->field_8A8 = 0;
    }
    if (work->field_89A == 2) {
        second            = (Actor312200Work*)task->work;
        second->field_89E = 3 * ANIMATION_RATE_ONE;
        second->field_8A0 = 0x500;
        for (k = 1; k < 0x13; k++) {
            second->rig.slots[k].rate = second->field_89E;
            animationResetSlot(&second->anim2, k, (s16)second->field_89C);
        }
        work->field_89A = 3;
    }
    work->field_894++;
    tick = (Actor312200Work*)task->work;
    for (m = 1; m < 0x13; m++) {
        tick->rig.slots[m].rate = tick->field_896;
        animationTickSlot(&tick->rig.anim, m);
    }
}

/// Spawn handler, the first entry of the actor's state table: allocates the
/// 0x984-byte `Actor312200Work` into `Task::work` (tearing the enemy down if
/// that fails) and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The model's light
/// and colour matrices are pointed into the work block, the enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `coord`, and the display node built in place at `field_8BC` gets the
/// block's three-entry `WorldCollisionContact` table and the model's fourth part coordinate.
/// The model coordinate is parented to `gGfxViewCoord` and rebuilt once before
/// its translation is propagated over the three part coordinates
/// (`func_800D7A9C`, start 0, count 3).
static void func_actor_312200_80163178(Enemy* enemy, Task* task)
{
    VECTOR              vec;
    GfxCoord*           coord;
    TmdObject*          obj;
    TmdObject*          tmd;
    Actor312200Work*    mem;
    Actor312200Work*    work;
    WorldCollisionBody* node;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(Actor312200Work), 0);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    tmd               = task->extra.tmd;
    tmd->lightMtx     = &work->light;
    tmd->colorMtx     = &work->color;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_4D               = 0;
    enemy->reactionFlags          = 0;
    enemy->field_4D               = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_312200_80169F44, obj, work->rig.poses, work->rig.slots);
    work->field_88C = 2;
    work->field_892 = 1;
    work->field_896 = ANIMATION_RATE_ONE;
    func_actor_312200_80162FB4(task);
    node                   = &work->field_8BC;
    node->coord            = &task->extra.tmd->coords[3];
    node->context.contacts = work->recs;
    node->pos.vx           = 0;
    node->pos.vy           = 0;
    node->pos.vz           = 0;
    node->key              = 0x3000A;
    node->radius           = 0x180;
    node->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, node);
    node->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(node->context.contacts, 3, 0);
    task->msgTable      = D_actor_312200_80169F5C;
    work->field_8AC     = 0;
    work->field_8AD     = 1;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(task->extra.tmd, &vec, 0, 3);
    work->field_0 = 0;
    work->field_2 = -1;
    task->state++;
}

/// Per-tick handler and state dispatcher, called with the task second. The
/// handler table is built in place - `func_actor_312200_80163778` at index 0,
/// the tick handler at index 1 - and `field_0` selects from it, unless the global
/// `gSceneCombatState.actorControl` holds the actor. `field_4` records whether the state moved
/// before it is re-latched into `field_2`. The tail clears the display node's
/// `WorldCollisionContact` record while occupied, re-propagates the root coordinate's
/// translation over the model's three part coordinates while `field_8AD` is
/// set, and then refreshes `field_8AD` from that coordinate's `composeStamp` - so the
/// propagation runs on the frame after the coordinate is dirtied. That same
/// coordinate is rebuilt (`composeStamp` dropped) and the 0x51030008 loop queued while
/// the room is live, from view 0x10 with the 0x7DB action `field_8B8` at 1.
///
/// The trailing `vec` is the original's own - three dead stores, but the frame
/// and the rest of the schedule are built around them.
static void func_actor_312200_80163370(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    VECTOR           vec;
    s32              pan;
    Actor312200Work* work                = (Actor312200Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_312200_80163778,
        func_actor_312200_801637CC,
    };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->field_2 != work->field_0) {
            work->field_4 = 1;
        } else {
            work->field_4 = 0;
        }
        work->field_2 = work->field_0;
        states[work->field_0](task);
        if (work->recs[0].key.value != 0) {
            Gp_ClearRec18Occupied(work->recs);
        }
        if (work->field_8AD != 0) {
            obj = task->extra.tmd;
            func_800D7A9C(obj, (VECTOR*)obj->coords->workm.t, 0, 3);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((Gp_GetViewIndex() == 0x10) && (work->field_8B8 == 1)) {
                pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                SndEvt_EnqueueType6(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, pan,
                                    (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            }
        }
        if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
            work->field_8AD = 1;
        } else {
            work->field_8AD = 0;
        }
        vec.vz = 0;
        vec.vy = 0;
        vec.vx = 0;
    }
}

/// Id 0x7D5 command handler, listed first in `D_actor_312200_80169F5C`. `arg2`
/// is the mode: 0 sets the model's `TmdObject::flags` to exactly 0x80, 1 clears
/// them, 2 raises `TMD_OBJECT_SKIP_AUTO_BUFFER`, and 3 clears them and then raises `TMD_OBJECT_SKIP_AUTO_BUFFER`. Modes 0
/// and 1 re-run `Tmd_AllocBuffers` on the model, and every mode except 1 resets
/// the work block's `field_0` state word. `arg1` is unused.
s32 func_actor_312200_80163510(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    Actor312200Work* work;

    obj  = task->extra.tmd;
    work = (Actor312200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Id 0x7D4 placement opcode: the three longs of `placement->pos` are copied
/// onto the actor's root coordinate, the Euler angles are applied X / Y / Z,
/// and the resulting heading is read back out of the matrix Z-axis with
/// `ratan2` and cached in `Actor312200Work::yaw`.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*        coord;
    s32              mx;
    s32              mz;
    Actor312200Work* work;

    work                                = (Actor312200Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    mx                                    = coord->coord.m[2][0];
    mz                                    = coord->coord.m[2][2];
    work->yaw                             = ratan2(-mx, mz);
    return 1;
}

/// Id 0x7DB command handler. The payload is always recorded in the work block,
/// and a message from sender 0x301 additionally selects the work state: action
/// 1 takes state 2, actions 2, 3 and 4 take state 1, and the action itself is
/// latched in the 0x892 timer. Either way the actor's `field_0` state word is
/// raised to 1.
s32 func_actor_312200_801636CC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    Actor312200Work* work;
    s32              action;

    work            = (Actor312200Work*)task->work;
    work->field_8B4 = msg->context.loc.stage;
    work->field_8B6 = msg->context.loc.area;
    work->field_8B8 = msg->command;

    if (msg->context.key == 0x301) {
        action = msg->command;
        switch (action) {
            case 1:
                work->field_892 = action;
                work->field_88C = 2;
                break;

            case 2:
                work->field_892 = action;
                work->field_88C = 1;
                break;

            case 3:
                work->field_892 = action;
                work->field_88C = 1;
                break;

            case 4:
                work->field_892 = action;
                work->field_88C = 1;
                break;
        }
    }

    work->field_0 = 1;
    return 1;
}

/// On a live actor, marks the enemy's target node not lockable, raises the model's
/// 0x80 bit (which takes it out of `Tmd_DrawActiveNodes`), clears
/// `Enemy::field_4D` and drops bit 0x8000 of the `field_8BC` node's flags.
static void func_actor_312200_80163778(Task* task)
{
    Actor312200Work* work;
    Enemy*           enemy;
    TmdObject*       obj;

    work = (Actor312200Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy                         = (Enemy*)task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        enemy->field_4D               = 0;
        work->field_8BC.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

/// Per-tick state callback. A live actor (`field_4`) re-enters work state 2
/// with the 0x896 timer armed at 0x10; once the 0x892 timer has counted those
/// 0x10 ticks and the task's flag bit 0 is set, the state drops to 1 and the
/// timer to 4. Either way the tick ends in the actor's anim/particle update.
static void func_actor_312200_801637CC(Task* task)
{
    Actor312200Work* work;

    work = (Actor312200Work*)task->work;
    if (work->field_4 != 0) {
        work->field_88C = 2;
        work->field_896 = ANIMATION_RATE_ONE;
        func_actor_312200_80162FB4(task);
    }
    if ((s16)work->field_892 == 0x10 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_892 = 4;
        work->field_88C = 1;
    }
    func_actor_312200_80162FB4(task);
}

/// The actor's three state handlers, dispatched by
/// `func_actor_312200_80163854`: spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_312200_80161E24 = {
    func_actor_312200_80163178,
    func_actor_312200_80163370,
    enemyDestroy,
};

/// Runs the handler `Task::state` selects from `D_actor_312200_80161E24`,
/// passing the spawn argument and the task. The table is copied onto the stack
/// before the call.
void func_actor_312200_80163854(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_312200_80161E24;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
