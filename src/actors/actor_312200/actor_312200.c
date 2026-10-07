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

#include "main/areas.h"
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
#include "../../shared/actor_messages.h"

extern u8 D_actor_312200_80169F44[];

extern TaskMessageEntry D_actor_312200_80169F5C[4];

/// Whole-unit part of the last step `ActorContact_PushContact` applied.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
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

/// Animation-bank indices also used as commands in the Acropolis patio namespace.
///
/// Clip 1 is the initial animation and restarts on command; clips 2 to 4
/// blend from the current pose. Their pose identities are unproven.
enum {
    ACTOR_312200_ANIM_1 = 1,
    ACTOR_312200_ANIM_2 = 2,
    ACTOR_312200_ANIM_3 = 3,
    ACTOR_312200_ANIM_4 = 4,
};

/// All three directional contributions of a model's room-light query.
enum { ACTOR_312200_LIGHT_CONTRIBUTIONS = 3 };

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

/// Step table the seeding body `_actor312200DriveAnimation` walks: one 5-byte
/// row per animation in `_Actor312200Work::appliedAnim`, addressed by the
/// requested animation in `_Actor312200Work::animId`. The byte it reads is
/// handed to `animationSeekSlotWithBlend` as the request's fifth argument.
extern s8 D_actor_312200_80169F28[][5];

static void _actor312200Hide(Task* task);
static void _actor312200PlayAnimation(Task* task);

static TmdSource _gActor312200SwatMember1Body;
static s32       _actor312200SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32       _actor312200Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);
static s32       _actor312200ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _actor312200Task(Task* task);

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor312200SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor312200ApplyCommand },
    { ACTOR_MESSAGE_PLACE, _actor312200Place },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_312200_80169F7C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor312200Task, { .model = &_gActor312200SwatMember1Body } };

SVECTOR ActorContact_ScratchPosition;

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Restarts the primary rig's part tracks and records the requested clip as applied.
///
/// Requires live work with `rig.anim` bound to `rig.slots` and a loaded
/// `animId` in 1..4 whose track table covers slots 1..18. The borrowed set
/// table and clip data must remain live throughout playback. Slot 0 is the
/// separately placed root. Each driven slot starts at its track's first record
/// with cleared boundary state and normal rate (`ANIMATION_RATE_ONE`).
/// Subsequent ticks produce the poses; the caller settles the request and
/// resets its frame counters.
static __inline__ void _actor312200RestartPrimaryAnimation(_Actor312200Work* work)
{
    enum { ACTOR_312200_FIRST_ANIMATED_SLOT = 1 };

    s32 slotIndex;

    // Slot reset replaces the requested rate; the driver reapplies it before ticking.
    for (slotIndex = ACTOR_312200_FIRST_ANIMATED_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
    work->appliedAnim = work->animId;
}

/// Applies animation requests and advances the actor's eighteen animated parts.
///
/// Requires initialized work, a bound model and the loaded animation bank.
/// Animation ids are 1..4; a blend also requires `appliedAnim` in 0..4 for
/// the transition table. Slot 0 is the root and is left alone. Rates are in
/// sixteenths of a frame per call; `animFrames` counts calls modulo 65536.
/// Applying a request clears that counter before this call increments it.
static void _actor312200DriveAnimation(Task* task)
{
    /// Retained second-rig pose weight, in Q12 units; this actor never mixes it.
    enum { ACTOR_312200_UNUSED_BLEND_WEIGHT = 0x500 };

    _Actor312200Work* work;
    _Actor312200Work* seekWork;
    _Actor312200Work* resetWork;
    _Actor312200Work* blendWork;
    _Actor312200Work* tickWork;
    s32               seekSlot;
    s32               blendSlot;
    s32               tickSlot;

    // Apply the pending clip before advancing its first frame.
    work = task->work;
    if (work->animRequest == ACTOR_312200_ANIM_REQUEST_BLEND) {
        seekWork = task->work;
        for (seekSlot = 1; seekSlot < ARRAY_SIZE(seekWork->rig.slots); seekSlot++) {
            seekWork->rig.slots[seekSlot].rate = seekWork->animRate;
            animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlot, seekWork->animId, 0,
                                       D_actor_312200_80169F28[seekWork->appliedAnim][seekWork->animId]);
        }
        seekWork->appliedAnim = seekWork->animId;
        work->animRequest     = ACTOR_312200_ANIM_REQUEST_PLAYING;
        work->animFrames      = 0;
        work->lastCueFrame    = 0;
    } else if (work->animRequest == ACTOR_312200_ANIM_REQUEST_RESET) {
        resetWork = task->work;
        _actor312200RestartPrimaryAnimation(resetWork);
        work->animRequest  = ACTOR_312200_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    // No caller here requests the unbound blend rig. Preserve its main-rig rate stores.
    if (work->blendRequest == ACTOR_312200_ANIM_REQUEST_RESET) {
        blendWork              = task->work;
        blendWork->blendRate   = 3 * ANIMATION_RATE_ONE;
        blendWork->blendWeight = ACTOR_312200_UNUSED_BLEND_WEIGHT;
        for (blendSlot = 1; blendSlot < ARRAY_SIZE(blendWork->blend.slots); blendSlot++) {
            blendWork->rig.slots[blendSlot].rate = blendWork->blendRate;
            animationResetSlot(&blendWork->blend.anim, blendSlot, blendWork->blendAnimId);
        }
        work->blendRequest = ACTOR_312200_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    tickWork = task->work;
    for (tickSlot = 1; tickSlot < ARRAY_SIZE(tickWork->rig.slots); tickSlot++) {
        tickWork->rig.slots[tickSlot].rate = tickWork->animRate;
        animationTickSlot(&tickWork->rig.anim, tickSlot);
    }
}

/// Initializes the actor's animation, target tracking, collision sphere and lighting.
///
/// Requires a live enemy and a nineteen-part TMD task with its package loaded.
/// The task owns the zeroed work; its model borrows the work's two lighting
/// matrices and pose storage. Allocation failure destroys the enemy and task.
/// Success starts clip 1, parents the root to the view coordinate and enters
/// the per-frame task handler in the hidden state. The first hidden tick
/// disables drawing, lock-on and the sphere's pair tests.
static void _actor312200Spawn(Enemy* enemy, Task* task)
{
    /// Enemy-body identity, radius in whole coordinate units, and the first-entry sentinel.
    enum {
        ACTOR_312200_BODY_ID         = 10,
        ACTOR_312200_BODY_RADIUS     = 0x180,
        ACTOR_312200_PREV_STATE_NONE = -1,
    };

    VECTOR              worldPosition;
    GfxCoord*           rootCoord;
    TmdObject*          model;
    TmdObject*          lightingModel;
    _Actor312200Work*   allocation;
    _Actor312200Work*   work;
    WorldCollisionBody* body;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    allocation = memCalloc(sizeof(_Actor312200Work), 0);
    work       = allocation;
    task->work = allocation;
    if (allocation == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &work->light;
    lightingModel->colorMtx = &work->color;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    enemy->bodyPos.vx       = 0;
    enemy->bodyPos.vy       = 0;
    enemy->bodyPos.vz       = 0;
    enemy->coord            = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_4D               = 0;
    enemy->reactionFlags          = 0;
    // The original spawn clears this byte twice.
    enemy->field_4D = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_312200_80169F44, model, work->rig.poses, work->rig.slots);
    work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
    work->animId      = ACTOR_312200_ANIM_1;
    work->animRate    = ANIMATION_RATE_ONE;
    _actor312200DriveAnimation(task);
    // The sphere and its three contact records live inside the task's work.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[3];
    body->context.contacts = work->contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_312200_BODY_ID;
    body->radius           = ACTOR_312200_BODY_RADIUS;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts), 0);
    task->msgTable       = D_actor_312200_80169F5C;
    work->field_8AC      = 0;
    work->relightPending = 1;
    // Compose the world position before ranking the three lighting contributions.
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, ACTOR_312200_LIGHT_CONTRIBUTIONS);
    work->state     = ACTOR_312200_STATE_HIDDEN;
    work->prevState = ACTOR_312200_PREV_STATE_NONE;
    task->state++;
}

/// Runs the hidden or animation state and refreshes contacts, lighting and sound.
///
/// Requires initialized work whose state is 0 or 1. Actor-control holds skip
/// the entire update. A state change marks only its first tick as entered.
/// Lighting uses the preceding tick's dirty-coordinate latch; view readiness
/// dirties the root again and queues the dormant sound in mapped view 16
/// while the last command word is 1. `unusedEnemy` is retained for the
/// lifecycle callback signature.
static void _actor312200Tick(Enemy* unusedEnemy, Task* task)
{
    enum { ACTOR_312200_DORMANT_SOUND_VIEW = 0x10 };

    TmdObject*        model;
    VECTOR            unusedVector;
    s32               pan;
    _Actor312200Work* work             = task->work;
    TaskFunc          stateHandlers[2] = {
        _actor312200Hide,
        _actor312200PlayAnimation,
    };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->prevState != work->state) {
            work->stateEntered = 1;
        } else {
            work->stateEntered = 0;
        }
        work->prevState = work->state;
        stateHandlers[work->state](task);
        if (work->contacts[0].key.value != 0) {
            worldCollisionClearContacts(work->contacts);
        }
        // Relight from the composed position before marking the next update dirty.
        if (work->relightPending != 0) {
            model = task->extra.tmd;
            worldCoordSetModelLighting(model, model->coords->workm.t, 0, ACTOR_312200_LIGHT_CONTRIBUTIONS);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((viewGetMappedIndex() == ACTOR_312200_DORMANT_SOUND_VIEW) && (work->command == ACTOR_312200_ANIM_1)) {
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
        // These unused stores are present in the original tick.
        unusedVector.vz = 0;
        unusedVector.vy = 0;
        unusedVector.vx = 0;
    }
}

/// Sets the model's draw flags and buffer policy for an actor visibility message.
///
/// Requires a live TMD task and initialized work. Modes 0 and 1 replace all
/// flags with hidden or visible drawing and allocate a buffer if missing.
/// Modes 2 and 3 set automatic-buffer exclusion, retaining or clearing the
/// other flags respectively. All except mode 1 select the hidden state;
/// showing retains the current state. Other values change nothing.
/// `messageId` and `unusedArg` are ignored. Returns 0.
static s32 _actor312200SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject*        model;
    _Actor312200Work* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_312200_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_312200_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_312200_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Places the model root with Rx * Ry * Rz and records the resulting heading.
///
/// Requires a live TMD task, initialized work and a readable, word-aligned
/// placement through dispatch. XYZ positions use the existing root parent's
/// frame; angles are 4096ths of a turn. Reads only XYZ components and retains
/// no payload pointer. Invalidates composition without changing stored Euler
/// angles. `messageId` and `unusedArg` are ignored. Returns 1.
static s32 _actor312200Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*         rootCoord;
    s32               rotationZX;
    s32               rotationZZ;
    _Actor312200Work* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    rootCoord                             = task->extra.tmd->coords;
    rotationZX                            = rootCoord->coord.m[2][0];
    rotationZZ                            = rootCoord->coord.m[2][2];
    work->placedYaw                       = ratan2(-rotationZX, rotationZZ);
    return 1;
}

/// Records a room command and selects animation playback in the patio namespace.
///
/// Requires initialized work and a readable four-byte command through dispatch.
/// Copies both context bytes and the full command word without retaining the
/// payload pointer. Patio commands 1..4 select the matching animation-bank id;
/// 1 restarts and 2..4 blend. Other contexts or commands retain that request.
/// Every command enters the playing state, including ignored selectors.
/// `messageId` and `unusedArg` are ignored. Returns 1.
static s32 _actor312200ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    /// Stage is the low byte and area the high byte of the command context.
    enum {
        ACTOR_312200_PATIO_COMMAND_CONTEXT = GAME_STAGE_ACROPOLIS | (GAME_AREA_ACROPOLIS_PATIO << 8),
    };

    _Actor312200Work* work;
    s32               animationId;

    work               = task->work;
    work->commandStage = command->context.loc.stage;
    work->commandArea  = command->context.loc.area;
    work->command      = command->command;

    if (command->context.key == ACTOR_312200_PATIO_COMMAND_CONTEXT) {
        animationId = command->command;
        switch (animationId) {
            case ACTOR_312200_ANIM_1:
                work->animId      = animationId;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
                break;

            case ACTOR_312200_ANIM_2:
                work->animId      = animationId;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;

            case ACTOR_312200_ANIM_3:
                work->animId      = animationId;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;

            case ACTOR_312200_ANIM_4:
                work->animId      = animationId;
                work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
                break;
        }
    }

    work->state = ACTOR_312200_STATE_PLAYING;
    return 1;
}

/// Disables drawing, lock-on and sphere pair tests on entry to the hidden state.
///
/// Requires initialized work, a live model and the live enemy borrowed by
/// `spawnArg2.pointer`. Later hidden ticks leave those objects unchanged.
static void _actor312200Hide(Task* task)
{
    _Actor312200Work* work;
    Enemy*            enemy;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        enemy->field_4D               = 0;
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

/// Restarts playback on state entry and advances the requested animation.
///
/// Requires initialized animation work. Entry forces a reset at normal speed
/// and advances once before the regular driver call, so that first tick
/// advances twice. Later ticks advance once, preserving the current rate.
static void _actor312200PlayAnimation(Task* task)
{
    _Actor312200Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        work->animRequest = ACTOR_312200_ANIM_REQUEST_RESET;
        work->animRate    = ANIMATION_RATE_ONE;
        _actor312200DriveAnimation(task);
    }
    // Retained branch: no request here selects animation 0x10, outside this bank.
    if (work->animId == 0x10 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_312200_ANIM_4;
        work->animRequest = ACTOR_312200_ANIM_REQUEST_BLEND;
    }
    _actor312200DriveAnimation(task);
}

/// The actor's three state handlers, dispatched by
/// `_actor312200Task`: spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_312200_80161E24 = {
    _actor312200Spawn,
    _actor312200Tick,
    enemyDestroy,
};

/// Dispatches the actor task's spawn, per-frame update or enemy teardown callback.
///
/// Requires a live task borrowing its live enemy in `spawnArg2.pointer`.
/// `Task::state` must be 0 (spawn), 1 (update) or 2 (destroy); the table has no
/// bounds check. Teardown may release both objects before this call returns.
static void _actor312200Task(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = D_actor_312200_80161E24;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
