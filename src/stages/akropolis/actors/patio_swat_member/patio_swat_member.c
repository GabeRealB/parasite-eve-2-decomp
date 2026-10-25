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
#include "../../../../shared/actor/actor_contacts.h"
#include "../../../../shared/actor/actor_messages.h"

/// Whole-unit part of the last step `_actorContactApplyGridPushback` applied.
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

/// Values of `_PatioSwatMemberWork::state`: the index of the handler the per-frame
/// tick runs.
enum {
    PATIO_SWAT_MEMBER_STATE_HIDDEN  = 0, // not drawn and not lockable, with `body` out of the pair tests; the spawn starts here
    PATIO_SWAT_MEMBER_STATE_PLAYING = 1  // plays the animation the last room command selected
};

/// Values of `_PatioSwatMemberWork::animRequest` and `_PatioSwatMemberWork::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
enum {
    PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames the transition table gives
    PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    PATIO_SWAT_MEMBER_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Animation-bank indices also used as commands in the Acropolis patio namespace.
///
/// Clip 1 is the initial animation and restarts on command; clips 2 to 4
/// blend from the current pose. Their pose identities are unproven.
enum {
    PATIO_SWAT_MEMBER_ANIM_1 = 1,
    PATIO_SWAT_MEMBER_ANIM_2 = 2,
    PATIO_SWAT_MEMBER_ANIM_3 = 3,
    PATIO_SWAT_MEMBER_ANIM_4 = 4,
};

/// All three directional contributions of a model's room-light query.
enum { PATIO_SWAT_MEMBER_LIGHT_CONTRIBUTIONS = 3 };

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
    s16                   state;           // `PATIO_SWAT_MEMBER_STATE_*`
    s16                   prevState;       // `state` the tick last ran; -1 from the spawn, so the first tick enters `state` afresh
    s16                   stateEntered;    // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    byte                  field_6[0x2];    // never accessed
    s16                   placedYaw;       // heading of the root after the last placement message, 4096 to a turn; never read
    byte                  field_A[0x6];    // never accessed
    ActorAnimRig19        rig;             // playback of the model's parts; the driver uses slots 1 to 18
    ActorAnimRig19        blend;           // second rig the driver restarts on `blendRequest`; never bound to the model
    byte                  field_888[0x4];  // never accessed
    s16                   animRequest;     // `PATIO_SWAT_MEMBER_ANIM_REQUEST_*` for `rig`
    byte                  field_88E[0x2];  // never accessed
    s16                   appliedAnim;     // animation `rig` was last started on
    s16                   animId;          // animation requested of `rig`: 1 from the spawn, then the room command's number
    u16                   animFrames;      // ticks since `animRequest` was last applied; never read
    s16                   animRate;        // playback rate of `rig`'s slots
    byte                  field_898[0x2];  // never accessed
    s16                   blendRequest;    // `PATIO_SWAT_MEMBER_ANIM_REQUEST_*` for `blend`; nothing requests it
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
    WorldCollisionBody    body;            // sphere on model part 3 in collision list 2; the spawn enables its pair tests and entering `PATIO_SWAT_MEMBER_STATE_HIDDEN` disables them
    WorldCollisionContact contacts[3];     // contacts of `body`; a tick that finds the first occupied clears them
    byte                  field_924[0x20]; // never accessed
    MATRIX                light;           // storage for the model's `TmdObject::lightMtx`
    MATRIX                color;           // storage for the model's `TmdObject::colorMtx`
} _PatioSwatMemberWork;
STATIC_ASSERT_SIZEOF(_PatioSwatMemberWork, 0x984);

static void _patioSwatMemberHide(Task* task);
static void _patioSwatMemberPlayAnimation(Task* task);

static TmdSource _gSwatMember1Body;
static s32       _patioSwatMemberSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32       _patioSwatMemberPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);
static s32       _patioSwatMemberApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _patioSwatMemberTask(Task* task);

#include "assets/characters/swat_member_1/body.inc"

#include "assets/rooms/akropolis/patio/swat_member_clips.inc"

/// Whole-frame blend durations indexed by previous and requested animation.
///
/// Previous IDs are 0..4 (0 before any clip), requested IDs 1..4.
/// Every transition seeks with zero blend time; signed bytes promote
/// to the playback API's frame count. The five-by-five storage is retained.
static s8 _gPatioSwatMemberAnimationBlendFrames[5][5] = { 0 };

/// Animation sets for the scripted SWAT member, indexed by animation ID.
///
/// Requests select IDs 1..4; entries 0 and 5 are null slots, not terminators.
/// The rig borrows this word-aligned table and its sets until teardown.
#include "assets/rooms/akropolis/patio/swat_bank.inc"

/// Message map for the scripted SWAT member's drawing, commands and placement.
///
/// Drawing takes a scalar mode; commands borrow an `ActorCommand`, and placement
/// borrows an `ActorTransform`. The second payload is unused by all three.
/// The task borrows this terminated map and its callbacks until teardown.
static const TaskMessageEntry _gPatioSwatMemberMessages[4] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _patioSwatMemberSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _patioSwatMemberApplyCommand },
    { ACTOR_MESSAGE_PLACE, _patioSwatMemberPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gPatioSwatMemberSwatMember1TaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _patioSwatMemberTask, { .model = &_gSwatMember1Body } };

SVECTOR ActorContact_ScratchPosition;

#include "../../../../shared/actor/actor_contacts_find_push.inc.c"

#include "../../../../shared/actor/actor_contacts_steer.inc.c"

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

/// Restarts the primary rig's part tracks and records the requested clip as applied.
///
/// Requires live work with `rig.anim` bound to `rig.slots` and a loaded
/// `animId` in 1..4 whose track table covers slots 1..18. The borrowed set
/// table and clip data must remain live throughout playback. Slot 0 is the
/// separately placed root. Each driven slot starts at its track's first record
/// with cleared boundary state and normal rate (`ANIMATION_RATE_ONE`).
/// Subsequent ticks produce the poses; the caller settles the request and
/// resets its frame counters.
static inline void _patioSwatMemberRestartPrimaryAnimation(_PatioSwatMemberWork* work)
{
    enum { PATIO_SWAT_MEMBER_FIRST_ANIMATED_SLOT = 1 };

    s32 slotIndex;

    // Slot reset replaces the requested rate; the driver reapplies it before ticking.
    for (slotIndex = PATIO_SWAT_MEMBER_FIRST_ANIMATED_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
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
static void _patioSwatMemberDriveAnimation(Task* task)
{
    /// Retained second-rig pose weight, in Q12 units; this actor never mixes it.
    enum { PATIO_SWAT_MEMBER_UNUSED_BLEND_WEIGHT = 0x500 };

    _PatioSwatMemberWork* work;
    _PatioSwatMemberWork* seekWork;
    _PatioSwatMemberWork* resetWork;
    _PatioSwatMemberWork* blendWork;
    _PatioSwatMemberWork* tickWork;
    s32                   seekSlot;
    s32                   blendSlot;
    s32                   tickSlot;

    // Apply the pending clip before advancing its first frame.
    work = task->work;
    if (work->animRequest == PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND) {
        seekWork = task->work;
        for (seekSlot = 1; seekSlot < ARRAY_SIZE(seekWork->rig.slots); seekSlot++) {
            seekWork->rig.slots[seekSlot].rate = seekWork->animRate;
            animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlot, seekWork->animId, 0,
                                       _gPatioSwatMemberAnimationBlendFrames[seekWork->appliedAnim][seekWork->animId]);
        }
        seekWork->appliedAnim = seekWork->animId;
        work->animRequest     = PATIO_SWAT_MEMBER_ANIM_REQUEST_PLAYING;
        work->animFrames      = 0;
        work->lastCueFrame    = 0;
    } else if (work->animRequest == PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET) {
        resetWork = task->work;
        _patioSwatMemberRestartPrimaryAnimation(resetWork);
        work->animRequest  = PATIO_SWAT_MEMBER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    // No caller here requests the unbound blend rig. Preserve its main-rig rate stores.
    if (work->blendRequest == PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET) {
        blendWork              = task->work;
        blendWork->blendRate   = 3 * ANIMATION_RATE_ONE;
        blendWork->blendWeight = PATIO_SWAT_MEMBER_UNUSED_BLEND_WEIGHT;
        for (blendSlot = 1; blendSlot < ARRAY_SIZE(blendWork->blend.slots); blendSlot++) {
            blendWork->rig.slots[blendSlot].rate = blendWork->blendRate;
            animationResetSlot(&blendWork->blend.anim, blendSlot, blendWork->blendAnimId);
        }
        work->blendRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_PLAYING;
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
static void _patioSwatMemberSpawn(Enemy* enemy, Task* task)
{
    /// Enemy-body identity, radius in whole coordinate units, and the first-entry sentinel.
    enum {
        PATIO_SWAT_MEMBER_BODY_ID         = 10,
        PATIO_SWAT_MEMBER_BODY_RADIUS     = 0x180,
        PATIO_SWAT_MEMBER_PREV_STATE_NONE = -1,
    };

    VECTOR                worldPosition;
    GfxCoord*             rootCoord;
    TmdObject*            model;
    TmdObject*            lightingModel;
    _PatioSwatMemberWork* allocation;
    _PatioSwatMemberWork* work;
    WorldCollisionBody*   body;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    allocation = memCalloc(sizeof(_PatioSwatMemberWork), 0);
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
    animationInitContext(&work->rig.anim, _gPatioSwatMemberAnimationSets, model, work->rig.poses, work->rig.slots);
    work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET;
    work->animId      = PATIO_SWAT_MEMBER_ANIM_1;
    work->animRate    = ANIMATION_RATE_ONE;
    _patioSwatMemberDriveAnimation(task);
    // The sphere and its three contact records live inside the task's work.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[3];
    body->context.contacts = work->contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | PATIO_SWAT_MEMBER_BODY_ID;
    body->radius           = PATIO_SWAT_MEMBER_BODY_RADIUS;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts), 0);
    task->msgTable       = _gPatioSwatMemberMessages;
    work->field_8AC      = 0;
    work->relightPending = 1;
    // Compose the world position before ranking the three lighting contributions.
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, PATIO_SWAT_MEMBER_LIGHT_CONTRIBUTIONS);
    work->state     = PATIO_SWAT_MEMBER_STATE_HIDDEN;
    work->prevState = PATIO_SWAT_MEMBER_PREV_STATE_NONE;
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
static void _patioSwatMemberTick(Enemy* unusedEnemy, Task* task)
{
    enum { PATIO_SWAT_MEMBER_DORMANT_SOUND_VIEW = 0x10 };

    TmdObject*            model;
    VECTOR                unusedVector;
    s32                   pan;
    _PatioSwatMemberWork* work             = task->work;
    TaskFunc              stateHandlers[2] = {
        _patioSwatMemberHide,
        _patioSwatMemberPlayAnimation,
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
            worldCoordSetModelLighting(model, model->coords->workm.t, 0, PATIO_SWAT_MEMBER_LIGHT_CONTRIBUTIONS);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((viewGetMappedIndex() == PATIO_SWAT_MEMBER_DORMANT_SOUND_VIEW) && (work->command == PATIO_SWAT_MEMBER_ANIM_1)) {
                pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(SOUND_AKROPOLIS_PATIO_STRANGER_DORMANT, pan,
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
static s32 _patioSwatMemberSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject*            model;
    _PatioSwatMemberWork* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = PATIO_SWAT_MEMBER_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = PATIO_SWAT_MEMBER_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = PATIO_SWAT_MEMBER_STATE_HIDDEN;
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
static s32 _patioSwatMemberPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*             rootCoord;
    s32                   rotationZX;
    s32                   rotationZZ;
    _PatioSwatMemberWork* work;

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
static s32 _patioSwatMemberApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    /// Stage is the low byte and area the high byte of the command context.
    enum {
        PATIO_SWAT_MEMBER_PATIO_COMMAND_CONTEXT = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_PATIO << 8),
    };

    _PatioSwatMemberWork* work;
    s32                   animationId;

    work               = task->work;
    work->commandStage = command->context.loc.stage;
    work->commandArea  = command->context.loc.area;
    work->command      = command->command;

    if (command->context.key == PATIO_SWAT_MEMBER_PATIO_COMMAND_CONTEXT) {
        animationId = command->command;
        switch (animationId) {
            case PATIO_SWAT_MEMBER_ANIM_1:
                work->animId      = animationId;
                work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET;
                break;

            case PATIO_SWAT_MEMBER_ANIM_2:
                work->animId      = animationId;
                work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND;
                break;

            case PATIO_SWAT_MEMBER_ANIM_3:
                work->animId      = animationId;
                work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND;
                break;

            case PATIO_SWAT_MEMBER_ANIM_4:
                work->animId      = animationId;
                work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND;
                break;
        }
    }

    work->state = PATIO_SWAT_MEMBER_STATE_PLAYING;
    return 1;
}

/// Disables drawing, lock-on and sphere pair tests on entry to the hidden state.
///
/// Requires initialized work, a live model and the live enemy borrowed by
/// `spawnArg2.pointer`. Later hidden ticks leave those objects unchanged.
static void _patioSwatMemberHide(Task* task)
{
    _PatioSwatMemberWork* work;
    Enemy*                enemy;
    TmdObject*            model;

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
static void _patioSwatMemberPlayAnimation(Task* task)
{
    _PatioSwatMemberWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_RESET;
        work->animRate    = ANIMATION_RATE_ONE;
        _patioSwatMemberDriveAnimation(task);
    }
    // Retained branch: no request here selects animation 0x10, outside this bank.
    if (work->animId == 0x10 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = PATIO_SWAT_MEMBER_ANIM_4;
        work->animRequest = PATIO_SWAT_MEMBER_ANIM_REQUEST_BLEND;
    }
    _patioSwatMemberDriveAnimation(task);
}

enum {
    PATIO_SWAT_MEMBER_SWAT_TASK_SPAWN   = 0,
    PATIO_SWAT_MEMBER_SWAT_TASK_UPDATE  = 1,
    PATIO_SWAT_MEMBER_SWAT_TASK_DESTROY = 2,
};

/// SWAT figure lifecycle handlers: 0 spawn, 1 hidden/playback update, 2 destroy.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Callbacks require a live task; update requires setup
/// work, and teardown may release both the task and its owned `Enemy`.
/// Each callback receives the owned `Enemy` from `Task::spawnArg2` and the task.
/// Destroy leaves the work-owned collision sphere linked. After work release,
/// collision traversal must remain suspended until the room lists are reset.
static const EnemyTaskFuncTable3 _gPatioSwatMemberSwatTaskStates = {
    .funcs = {
        [PATIO_SWAT_MEMBER_SWAT_TASK_SPAWN]   = _patioSwatMemberSpawn,
        [PATIO_SWAT_MEMBER_SWAT_TASK_UPDATE]  = _patioSwatMemberTick,
        [PATIO_SWAT_MEMBER_SWAT_TASK_DESTROY] = enemyDestroy,
    },
};

/// Dispatches the actor task's spawn, per-frame update or enemy teardown callback.
///
/// Requires a live task borrowing its live enemy in `spawnArg2.pointer`.
/// `Task::state` must be 0 (spawn), 1 (update) or 2 (destroy); the table has no
/// bounds check. Teardown may release both objects before this call returns.
static void _patioSwatMemberTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gPatioSwatMemberSwatTaskStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
