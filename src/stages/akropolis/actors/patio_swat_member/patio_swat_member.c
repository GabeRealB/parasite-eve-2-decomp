#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
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

#include "assets/rooms/akropolis/patio/swat_schema.h"

/// Whole-unit part of the last step `_actorContactApplyGridPushback` applied.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Values of `_Work::state`: the index of the handler the per-frame tick runs.
enum {
    STATE_HIDDEN  = 0, // not drawn and not lockable, with `body` out of the pair tests; the spawn starts here
    STATE_PLAYING = 1  // plays the animation the last room command selected
};

/// Values of `_Work::animRequest` and `_Work::blendRequest`.
enum {
    ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames the transition table gives
    ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// All three directional contributions of a model's room-light query.
enum { LIGHT_CONTRIBUTIONS = 3 };

/// The body parts the enemy follows (the waist) and the collision sphere rides (the chest).
enum {
    WAIST_PART = 2,
    CHEST_PART = 3,
};

/// Work block of the patio's SWAT member; a room command (its number one of the bank's clips) picks what he plays.
/// Its animation driver is laid out like `OddStrangerWork`'s, but only applies a request and advances `rig`: nothing
/// binds `blend` or requests it and no cue plays, so several members are written and never read.
typedef struct {
    s16                   state;           // `STATE_*`
    s16                   prevState;       // `state` the tick last ran; -1 from the spawn, so the first tick enters `state` afresh
    s16                   stateEntered;    // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    byte                  field_6[0x2];    // never accessed
    s16                   placedYaw;       // heading of the root after the last placement message, 4096 to a turn; never read
    byte                  field_A[0x6];    // never accessed
    ActorAnimRig19        rig;             // playback of the model's parts; the driver uses slots 1 to 18
    ActorAnimRig19        blend;           // second rig the driver restarts on `blendRequest`; never bound to the model
    byte                  field_888[0x4];  // never accessed
    s16                   animRequest;     // `ANIM_REQUEST_*` for `rig`
    byte                  field_88E[0x2];  // never accessed
    s16                   appliedAnim;     // animation `rig` was last started on
    s16                   animId;          // animation requested of `rig`: `SWAT_ANIMATION_DORMANT` from the spawn, then the room command's number
    u16                   animFrames;      // ticks since `animRequest` was last applied; never read
    s16                   animRate;        // playback rate of `rig`'s slots
    byte                  field_898[0x2];  // never accessed
    s16                   blendRequest;    // `ANIM_REQUEST_*` for `blend`; nothing requests it
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
    s16                   command;         // command word of the last actor command received; while it is `SWAT_ANIMATION_DORMANT`, view 0x10 plays the dormant sound
    byte                  field_8BA[0x2];  // never accessed
    WorldCollisionBody    body;            // sphere on the chest in collision list 2; the spawn enables its pair tests and entering `STATE_HIDDEN` disables them
    WorldCollisionContact contacts[3];     // contacts of `body`; a tick that finds the first occupied clears them
    byte                  field_924[0x20]; // never accessed
    MATRIX                light;           // storage for the model's `TmdObject::lightMtx`
    MATRIX                color;           // storage for the model's `TmdObject::colorMtx`
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x984);

static void _patioSwatMemberHide(Task* task);
static void _patioSwatMemberPlayAnimation(Task* task);

static TmdSource _gSwatMember1Body;
static s32       _patioSwatMemberSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32       _patioSwatMemberPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);
static s32       _patioSwatMemberApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _patioSwatMemberTask(Task* task);

#include "assets/characters/swat_member_1/body.inc"

#include "assets/rooms/akropolis/patio/swat_member_clips.inc"

/// Whole-frame blend durations indexed by previous and requested animation; every transition is immediate.
static s8 _gPatioSwatMemberAnimationBlendFrames[5][5] = { 0 };

/// Animation sets for the scripted SWAT member, indexed by animation ID.
#include "assets/rooms/akropolis/patio/swat_bank.inc"

/// Message map for the scripted SWAT member's drawing, commands and placement.
static const TaskMessageEntry _gPatioSwatMemberMessages[4] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _patioSwatMemberSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _patioSwatMemberApplyCommand },
    { ACTOR_MESSAGE_PLACE, _patioSwatMemberPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gPatioSwatMemberTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _patioSwatMemberTask, { .model = &_gSwatMember1Body } };

SVECTOR ActorContact_ScratchPosition;

#include "../../../../shared/actor/actor_contacts_find_push.inc.c"

#include "../../../../shared/actor/actor_contacts_steer.inc.c"

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

/// Restarts the primary rig's part tracks and records the requested clip as applied.
static inline void _patioSwatMemberRestartPrimaryAnimation(_Work* work)
{
    enum { FIRST_ANIMATED_SLOT = 1 };

    s32 slotIndex;

    // Slot reset replaces the requested rate; the driver reapplies it before ticking.
    for (slotIndex = FIRST_ANIMATED_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
    work->appliedAnim = work->animId;
}

/// Applies animation requests and advances the actor's eighteen animated parts.
static void _patioSwatMemberDriveAnimation(Task* task)
{
    /// Retained second-rig pose weight, in Q12 units; this actor never mixes it.
    enum { UNUSED_BLEND_WEIGHT = 0x500 };

    _Work* work;
    _Work* seekWork;
    _Work* resetWork;
    _Work* blendWork;
    _Work* tickWork;
    s32    seekSlot;
    s32    blendSlot;
    s32    tickSlot;

    // Apply the pending clip before advancing its first frame.
    work = task->work;
    if (work->animRequest == ANIM_REQUEST_BLEND) {
        seekWork = task->work;
        for (seekSlot = 1; seekSlot < ARRAY_SIZE(seekWork->rig.slots); seekSlot++) {
            seekWork->rig.slots[seekSlot].rate = seekWork->animRate;
            animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlot, seekWork->animId, 0,
                                       _gPatioSwatMemberAnimationBlendFrames[seekWork->appliedAnim][seekWork->animId]);
        }
        seekWork->appliedAnim = seekWork->animId;
        work->animRequest     = ANIM_REQUEST_PLAYING;
        work->animFrames      = 0;
        work->lastCueFrame    = 0;
    } else if (work->animRequest == ANIM_REQUEST_RESET) {
        resetWork = task->work;
        _patioSwatMemberRestartPrimaryAnimation(resetWork);
        work->animRequest  = ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    // No caller here requests the unbound blend rig. Preserve its main-rig rate stores.
    if (work->blendRequest == ANIM_REQUEST_RESET) {
        blendWork              = task->work;
        blendWork->blendRate   = 3 * ANIMATION_RATE_ONE;
        blendWork->blendWeight = UNUSED_BLEND_WEIGHT;
        for (blendSlot = 1; blendSlot < ARRAY_SIZE(blendWork->blend.slots); blendSlot++) {
            blendWork->rig.slots[blendSlot].rate = blendWork->blendRate;
            animationResetSlot(&blendWork->blend.anim, blendSlot, blendWork->blendAnimId);
        }
        work->blendRequest = ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    tickWork = task->work;
    for (tickSlot = 1; tickSlot < ARRAY_SIZE(tickWork->rig.slots); tickSlot++) {
        tickWork->rig.slots[tickSlot].rate = tickWork->animRate;
        animationTickSlot(&tickWork->rig.anim, tickSlot);
    }
}

/// Initializes the actor's animation, target tracking, collision sphere and lighting.
static void _patioSwatMemberSpawn(Enemy* enemy, Task* task)
{
    /// Enemy-body identity, radius in whole coordinate units, and the first-entry sentinel.
    enum {
        BODY_ID         = 10,
        BODY_RADIUS     = 0x180,
        PREV_STATE_NONE = -1,
    };

    VECTOR              worldPosition;
    GfxCoord*           rootCoord;
    TmdObject*          model;
    TmdObject*          lightingModel;
    _Work*              allocation;
    _Work*              work;
    WorldCollisionBody* body;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    allocation = memCalloc(sizeof(_Work), 0);
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
    enemy->coord            = &task->extra.tmd->coords[WAIST_PART];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_4D               = 0;
    enemy->reactionFlags          = 0;
    // The original spawn clears this byte twice.
    enemy->field_4D = 0;
    animationInitContext(&work->rig.anim, _gPatioSwatMemberAnimationSets, model, work->rig.poses, work->rig.slots);
    work->animRequest = ANIM_REQUEST_RESET;
    work->animId      = SWAT_ANIMATION_DORMANT;
    work->animRate    = ANIMATION_RATE_ONE;
    _patioSwatMemberDriveAnimation(task);
    // The sphere and its three contact records live inside the task's work.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[CHEST_PART];
    body->context.contacts = work->contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | BODY_ID;
    body->radius           = BODY_RADIUS;
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
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, LIGHT_CONTRIBUTIONS);
    work->state     = STATE_HIDDEN;
    work->prevState = PREV_STATE_NONE;
    task->state++;
}

/// Runs the hidden or animation state and refreshes contacts, lighting and sound.
static void _patioSwatMemberTick(Enemy* unusedEnemy, Task* task)
{
    enum { DORMANT_SOUND_VIEW = 0x10 };

    TmdObject* model;
    VECTOR     unusedVector;
    s32        pan;
    _Work*     work             = task->work;
    TaskFunc   stateHandlers[2] = {
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
            worldCoordSetModelLighting(model, model->coords->workm.t, 0, LIGHT_CONTRIBUTIONS);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((viewGetMappedIndex() == DORMANT_SOUND_VIEW) && (work->command == SWAT_ANIMATION_DORMANT)) {
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
static s32 _patioSwatMemberSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject* model;
    _Work*     work;

    model = task->extra.tmd;
    work  = task->work;
    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Places the model root with Rx * Ry * Rz and records the resulting heading.
static s32 _patioSwatMemberPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord* rootCoord;
    s32       rotationZX;
    s32       rotationZZ;
    _Work*    work;

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
static s32 _patioSwatMemberApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    /// Stage is the low byte and area the high byte of the command context.
    enum {
        PATIO_COMMAND_CONTEXT = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_PATIO << 8),
    };

    _Work* work;
    s32    animationId;

    work               = task->work;
    work->commandStage = command->context.loc.stage;
    work->commandArea  = command->context.loc.area;
    work->command      = command->command;

    if (command->context.key == PATIO_COMMAND_CONTEXT) {
        animationId = command->command;
        switch (animationId) {
            case SWAT_ANIMATION_DORMANT:
                work->animId      = animationId;
                work->animRequest = ANIM_REQUEST_RESET;
                break;

            case SWAT_ANIMATION_WARN:
                work->animId      = animationId;
                work->animRequest = ANIM_REQUEST_BLEND;
                break;

            case SWAT_ANIMATION_SLUMP:
                work->animId      = animationId;
                work->animRequest = ANIM_REQUEST_BLEND;
                break;

            case SWAT_ANIMATION_REST:
                work->animId      = animationId;
                work->animRequest = ANIM_REQUEST_BLEND;
                break;
        }
    }

    work->state = STATE_PLAYING;
    return 1;
}

/// Disables drawing, lock-on and sphere pair tests on entry to the hidden state.
static void _patioSwatMemberHide(Task* task)
{
    _Work*     work;
    Enemy*     enemy;
    TmdObject* model;

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
static void _patioSwatMemberPlayAnimation(Task* task)
{
    _Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        work->animRequest = ANIM_REQUEST_RESET;
        work->animRate    = ANIMATION_RATE_ONE;
        _patioSwatMemberDriveAnimation(task);
    }
    // Retained branch: no request here selects animation 0x10, outside this bank.
    if (work->animId == 0x10 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = SWAT_ANIMATION_REST;
        work->animRequest = ANIM_REQUEST_BLEND;
    }
    _patioSwatMemberDriveAnimation(task);
}

enum {
    TASK_SPAWN   = 0,
    TASK_UPDATE  = 1,
    TASK_DESTROY = 2,
};

/// The SWAT member's task states: spawn, hidden or playing update, destroy.
static const EnemyTaskFuncTable3 _gPatioSwatMemberStates = {
    .funcs = {
        [TASK_SPAWN]   = _patioSwatMemberSpawn,
        [TASK_UPDATE]  = _patioSwatMemberTick,
        [TASK_DESTROY] = enemyDestroy,
    },
};

/// Dispatches the actor task's spawn, per-frame update or enemy teardown callback.
static void _patioSwatMemberTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gPatioSwatMemberStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
