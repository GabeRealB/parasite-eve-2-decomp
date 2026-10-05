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

/// Values of `_Actor312200Work::state`: the index of the handler the per-frame
/// tick runs.
enum {
    ACTOR_312200_STATE_HIDDEN  = 0, // not drawn and not lockable, with `body` out of the pair tests; the spawn starts here
    ACTOR_312200_STATE_PLAYING = 1  // plays the animation the last room command selected
};

/// Values of `_Actor312200Work::animRequest` and `_Actor312200Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
enum {
    ACTOR_312200_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames the transition table gives
    ACTOR_312200_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_312200_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Work block of the actor 312200 task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the two-state machine a room command and the draw-mode message
/// switch, the model's animation rig with its driver's state, the collision
/// body with its contact records, and storage for the model's matrices.
///
/// The driver state from `rig` to `lastCueFrame` is laid out, and driven, like
/// the same run of `OddStrangerWork`. This actor carries only the part of that
/// driver which applies a request and advances `rig`: nothing binds `blend` to
/// the model or requests it, and no animation cue is played, so several
/// members are written and never read.
///
/// Animation ids index the package's animation bank; rates are sixteenths of
/// a frame per tick, `ANIMATION_RATE_ONE` being normal speed.
typedef struct {
    s16                   state;           // `ACTOR_312200_STATE_*`
    s16                   prevState;       // `state` the tick last ran; -1 from the spawn, so the first tick enters `state` afresh
    s16                   stateEntered;    // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    byte                  field_6[0x2];    // never accessed
    s16                   placedYaw;       // heading of the root after the last placement message, 4096 to a turn; never read
    byte                  field_A[0x6];    // never accessed
    ActorAnimRig19        rig;             // playback of the model's parts; the driver uses slots 1 to 18
    ActorAnimRig19        blend;           // second rig the driver restarts on `blendRequest`; never bound to the model
    byte                  field_888[0x4];  // never accessed
    s16                   animRequest;     // `ACTOR_312200_ANIM_REQUEST_*` for `rig`
    byte                  field_88E[0x2];  // never accessed
    s16                   appliedAnim;     // animation `rig` was last started on
    s16                   animId;          // animation requested of `rig`: 1 from the spawn, then the room command's number
    u16                   animFrames;      // ticks since `animRequest` was last applied; never read
    s16                   animRate;        // playback rate of `rig`'s slots
    byte                  field_898[0x2];  // never accessed
    s16                   blendRequest;    // `ACTOR_312200_ANIM_REQUEST_*` for `blend`; nothing requests it
    s16                   blendAnimId;     // animation requested of `blend`; never written
    u16                   blendRate;       // rate stored at each restart of `blend`, three times normal speed
    s16                   blendWeight;     // 0x500 from each restart of `blend`; never read
    byte                  field_8A2[0x6];  // never accessed
    s32                   lastCueFrame;    // cleared whenever `animRequest` is applied; never read
    u8                    field_8AC;       // 0 from the spawn; never read, role unproven
    s8                    relightPending;  // 1 when the last tick left the root coordinate awaiting composition, so the next tick rebuilds the model's lighting at its position (0 otherwise)
    byte                  field_8AE[0x6];  // never accessed
    s16                   commandStage;    // stage tag of the last actor command received; never read
    s16                   commandArea;     // area tag of the last actor command received; never read
    s16                   command;         // command word of the last actor command received; while it is 1, view 0x10 plays the dormant sound
    byte                  field_8BA[0x2];  // never accessed
    WorldCollisionBody    body;            // sphere on model part 3 in collision list 2; the spawn enables its pair tests and entering `ACTOR_312200_STATE_HIDDEN` disables them
    WorldCollisionContact contacts[3];     // contacts of `body`; a tick that finds the first occupied clears them
    byte                  field_924[0x20]; // never accessed
    MATRIX                light;           // storage for the model's `TmdObject::lightMtx`
    MATRIX                color;           // storage for the model's `TmdObject::colorMtx`
} _Actor312200Work;
STATIC_ASSERT_SIZEOF(_Actor312200Work, 0x984);

/// Step table the seeding body `func_actor_312200_80162FB4` walks: one 5-byte
/// row per animation in `_Actor312200Work::appliedAnim`, addressed by the
/// requested animation in `_Actor312200Work::animId`. The byte it reads is
/// handed to `animationSeekSlotWithBlend` as the request's fifth argument.
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

/// Animation driver, run once per tick: an `animRequest` of
/// `ACTOR_312200_ANIM_REQUEST_BLEND` seeks slots 1 to 18 of `rig` to `animId`
/// through the step table, `ACTOR_312200_ANIM_REQUEST_RESET` restarts them on
/// it, and both settle on `ACTOR_312200_ANIM_REQUEST_PLAYING` and clear
/// `animFrames`. A reset requested of `blend` restarts its slots at three
/// times normal speed, then the tail counts a frame and advances every driven
/// slot of `rig` at `animRate`.
static void func_actor_312200_80162FB4(Task* task)
{
    _Actor312200Work* work;
    _Actor312200Work* start;
    _Actor312200Work* reset;
    _Actor312200Work* second;
    _Actor312200Work* tick;
    s32               i;
    s32               j;
    s32               k;
    s32               m;

    work = task->work;
    if (work->animRequest == ACTOR_312200_ANIM_REQUEST_BLEND) {
        start = task->work;
        for (i = 1; i < ARRAY_SIZE(start->rig.slots); i++) {
            start->rig.slots[i].rate = start->animRate;
            animationSeekSlotWithBlend(&start->rig.anim, i, start->animId, 0,
                                       D_actor_312200_80169F28[start->appliedAnim][start->animId]);
        }
        start->appliedAnim = start->animId;
        goto advance;
    }
    if (work->animRequest == ACTOR_312200_ANIM_REQUEST_RESET) {
        reset = task->work;
        for (j = 1; j < ARRAY_SIZE(reset->rig.slots); j++) {
            reset->rig.slots[j].rate = reset->animRate;
            animationResetSlot(&reset->rig.anim, j, reset->animId);
        }
        reset->appliedAnim = reset->animId;
    advance:
        work->animRequest  = ACTOR_312200_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_312200_ANIM_REQUEST_RESET) {
        second              = task->work;
        second->blendRate   = 3 * ANIMATION_RATE_ONE;
        second->blendWeight = 0x500;
        for (k = 1; k < ARRAY_SIZE(second->blend.slots); k++) {
            second->rig.slots[k].rate = second->blendRate;
            animationResetSlot(&second->blend.anim, k, second->blendAnimId);
        }
        work->blendRequest = ACTOR_312200_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    tick = task->work;
    for (m = 1; m < ARRAY_SIZE(tick->rig.slots); m++) {
        tick->rig.slots[m].rate = tick->animRate;
        animationTickSlot(&tick->rig.anim, m);
    }
}

/// Spawn handler, the first entry of the actor's state table: allocates the
/// zeroed `_Actor312200Work` into `Task::work` (tearing the enemy down if
/// that fails) and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The model's light
/// and colour matrices are pointed into the work block, the enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `coord`, and the collision sphere built in place at `body` gets the
/// block's `contacts` table and the model's fourth part coordinate.
/// The model coordinate is parented to `gGfxViewCoord` and rebuilt once before
/// `worldCoordSetModelLighting` rebuilds the model's three light contributions
/// at that coordinate's world position.
static void func_actor_312200_80163178(Enemy* enemy, Task* task)
{
    VECTOR              vec;
    GfxCoord*           coord;
    TmdObject*          obj;
    TmdObject*          tmd;
    _Actor312200Work*   mem;
    _Actor312200Work*   work;
    WorldCollisionBody* node;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(_Actor312200Work), 0);
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
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_4D               = 0;
    enemy->reactionFlags          = 0;
    enemy->field_4D               = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_312200_80169F44, obj, work->rig.poses, work->rig.slots);
    work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
    work->animId      = 1;
    work->animRate    = ANIMATION_RATE_ONE;
    func_actor_312200_80162FB4(task);
    node                   = &work->body;
    node->coord            = &task->extra.tmd->coords[3];
    node->context.contacts = work->contacts;
    node->pos.vx           = 0;
    node->pos.vy           = 0;
    node->pos.vz           = 0;
    node->key              = 0x3000A;
    node->radius           = 0x180;
    node->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, node);
    node->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(node->context.contacts, ARRAY_SIZE(work->contacts), 0);
    task->msgTable       = D_actor_312200_80169F5C;
    work->field_8AC      = 0;
    work->relightPending = 1;
    coord->parent        = &gGfxViewCoord;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &vec, 0, 3);
    work->state     = ACTOR_312200_STATE_HIDDEN;
    work->prevState = -1;
    task->state++;
}

/// Per-tick handler and state dispatcher, called with the task second. The
/// handler table is built in place - `func_actor_312200_80163778` at index 0,
/// the tick handler at index 1 - and `state` selects from it, unless the global
/// `gSceneCombatState.actorControl` holds the actor. `stateEntered` records whether the state moved
/// before it is re-latched into `prevState`. The tail clears the `contacts` of
/// `body` while the first is occupied, rebuilds the model's lighting at the
/// root coordinate's position while `relightPending` is
/// set, and then refreshes `relightPending` from that coordinate's `composeStamp` - so the
/// lighting is rebuilt on the frame after the coordinate is dirtied. That same
/// coordinate is dirtied and the dormant sound queued while
/// the view is ready, the sound only in view 0x10 with the last `command` at 1.
///
/// The trailing `vec` is the original's own - three dead stores, but the frame
/// and the rest of the schedule are built around them.
static void func_actor_312200_80163370(Enemy* enemy, Task* task)
{
    TmdObject*        obj;
    VECTOR            vec;
    s32               pan;
    _Actor312200Work* work                = task->work;
    void              (*states[2])(Task*) = {
        func_actor_312200_80163778,
        func_actor_312200_801637CC,
    };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->prevState != work->state) {
            work->stateEntered = 1;
        } else {
            work->stateEntered = 0;
        }
        work->prevState = work->state;
        states[work->state](task);
        if (work->contacts[0].key.value != 0) {
            worldCollisionClearContacts(work->contacts);
        }
        if (work->relightPending != 0) {
            obj = task->extra.tmd;
            worldCoordSetModelLighting(obj, obj->coords->workm.t, 0, 3);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((viewGetMappedIndex() == 0x10) && (work->command == 1)) {
                pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, pan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            }
        }
        if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
            work->relightPending = 1;
        } else {
            work->relightPending = 0;
        }
        vec.vz = 0;
        vec.vy = 0;
        vec.vx = 0;
    }
}

/// Id 0x7D5 command handler, listed first in `D_actor_312200_80169F5C`. `arg2`
/// is the mode: 0 sets the model's `TmdObject::flags` to exactly 0x80, 1 clears
/// them, 2 raises `TMD_OBJECT_SKIP_AUTO_BUFFER`, and 3 clears them and then raises `TMD_OBJECT_SKIP_AUTO_BUFFER`. Modes 0
/// and 1 re-run `tmdAllocPrimitiveBuffer` on the model, and every mode except 1 resets
/// the work block's `state` to `ACTOR_312200_STATE_HIDDEN`. `arg1` is unused.
s32 func_actor_312200_80163510(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj;
    _Actor312200Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_312200_STATE_HIDDEN;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_312200_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_312200_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Id 0x7D4 placement opcode: the three longs of `placement->pos` are copied
/// onto the actor's root coordinate, the Euler angles are applied X / Y / Z,
/// and the resulting heading is read back out of the matrix Z-axis with
/// `ratan2` and kept in `_Actor312200Work::placedYaw`.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*         coord;
    s32               mx;
    s32               mz;
    _Actor312200Work* work;

    work                                = task->work;
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
    work->placedYaw                       = ratan2(-mx, mz);
    return 1;
}

/// Id 0x7DB command handler. The command is always recorded in the work block
/// (`commandStage`, `commandArea`, `command`), and one whose context key is
/// 0x301 additionally requests an animation of the driver: the command's
/// number becomes `animId`, command 1 restarting the rig on it
/// (`ACTOR_312200_ANIM_REQUEST_RESET`) and commands 2, 3 and 4 blending to it
/// (`ACTOR_312200_ANIM_REQUEST_BLEND`). Either way the actor's `state` becomes
/// `ACTOR_312200_STATE_PLAYING`.
s32 func_actor_312200_801636CC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor312200Work* work;
    s32               action;

    work               = task->work;
    work->commandStage = msg->context.loc.stage;
    work->commandArea  = msg->context.loc.area;
    work->command      = msg->command;

    if (msg->context.key == 0x301) {
        action = msg->command;
        switch (action) {
            case 1:
                work->animId      = action;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
                break;

            case 2:
                work->animId      = action;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;

            case 3:
                work->animId      = action;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;

            case 4:
                work->animId      = action;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;
        }
    }

    work->state = ACTOR_312200_STATE_PLAYING;
    return 1;
}

/// Handler of `ACTOR_312200_STATE_HIDDEN`. On the state's first tick
/// (`stateEntered`) it marks the enemy's target node not lockable, raises the
/// model's 0x80 bit (which takes it out of `Tmd_DrawActiveNodes`), clears
/// `Enemy::field_4D` and takes `body` out of the pair tests.
static void func_actor_312200_80163778(Task* task)
{
    _Actor312200Work* work;
    Enemy*            enemy;
    TmdObject*        obj;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy                         = (Enemy*)task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        enemy->field_4D               = 0;
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

/// Handler of `ACTOR_312200_STATE_PLAYING`. The state's first tick
/// (`stateEntered`) restarts the rig on `animId` at normal speed
/// (`ACTOR_312200_ANIM_REQUEST_RESET`, `animRate` at `ANIMATION_RATE_ONE`)
/// and runs the driver once for it. When animation 0x10 - which nothing in
/// this package requests - reaches a boundary on slot 1, a blend to animation
/// 4 is requested. Every tick then ends in the animation driver.
static void func_actor_312200_801637CC(Task* task)
{
    _Actor312200Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
        work->animRate    = ANIMATION_RATE_ONE;
        func_actor_312200_80162FB4(task);
    }
    if (work->animId == 0x10 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = 4;
        work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
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
