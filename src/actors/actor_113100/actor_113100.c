#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/mist_parking.h"
#include "../../shared/actor/model_placement.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/actor/actor_motion_walk_helpers.h"

/// Body drawing and primitive-buffer policies; every mode disables sphere pairs.
enum {
    ACTOR_113100_DRAW_HIDE                  = 0,
    ACTOR_113100_DRAW_SHOW                  = 1,
    ACTOR_113100_DRAW_HIDE_AND_RELEASE      = 2,
    ACTOR_113100_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
    ACTOR_113100_BUFFER_RELEASE_DELAY_TICKS = 2,
};

/// Pierce's walk clips and transitions; velocity uses signed 16.16 units per tick.
enum {
    ACTOR_113100_BODY_ANIMATION_BANK     = 0,
    ACTOR_113100_ANIM_WALK               = 2,
    ACTOR_113100_WALK_STEP_QUEUE_HEADING = 0,
    ACTOR_113100_TURN_BLEND_FRAMES       = 4,
    ACTOR_113100_WALK_BLEND_FRAMES       = 5,
    ACTOR_113100_YAW_STEP                = 64,
};

static void _modelPlacementMirrorParentDrawFlags(Task* childTask);

/// Work block of Pierce Carradine's body, the package's scripted walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the twenty-part rig and the model
/// state, and the model object borrows `model.light` and `model.color` for as
/// long as the block lives.
///
/// The actor has a collision sphere of its own, which the spawn state links
/// and the exit callback unlinks, so `body` and the table it borrows sit
/// between the model state and the walk a room script sends the actor on.
/// What follows the walk is the package's own: the model that faces the
/// camera, the heading the walk opens on, the head turn toward the player and
/// the delayed free of the model's buffers once the model has been hidden.
typedef struct {
    ActorAnimRig20        rig;            // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState       model;          // Clip and bank the rig plays, and the matrices the model is lit with
    WorldCollisionBody    body;           // Sphere on part 1, linked for the task's life; the spawn state enables its pair pass and every visibility command disables it
    WorldCollisionContact contacts[1];    // One-entry table `body` borrows. The entry is marked LAST; an occupied contact is cleared each frame the model is drawn and never read
    ActorWalkState        walk;           // Destination, closing rotation, per-frame velocity and step of the walk in progress
    Task*                 billboardTask;  // Task of the camera-facing model carried on part 8, which an actor command shows and hides; started only in placement variant 2, NULL otherwise
    s16                   turnWeight;     // Weight of the per-frame head turn toward the player, 0 to 0x1000; stepped by 0x100 a frame while the model is drawn
    s16                   walkYaw;        // Heading from the root to `walk.target` as a walk starts, 4096 to a turn; the opening turn steers the root yaw to it
    u8                    turnUp;         // Direction `turnWeight` is ramped in (0 down to 0, 1 up to 0x1000); every play request sets it from the clip, and an actor command overrides it
    s8                    freeCountdown;  // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s8                    lastAppearFlag; // Game flag 0xED as the idle handler last read it; the actor appears on the tick the flag is positive and this is still 0
    byte                  pad_53F[1];
} _Actor113100PierceCarradineWork;
STATIC_ASSERT_SIZEOF(_Actor113100PierceCarradineWork, 0x540);

static void _actor113100SpawnPierceCarradine(Task* task);
static void _actor113100UpdatePierceCarradine(Task* task);
static void _actor113100TurnAndBeginWalk(Task* task);
static void _actor113100CheckWalkArrival(Task* task);
static s32  _actor113100SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void _actor113100AttachBillboard(Task* task);
static void _actor113100FaceBillboardToCamera(Task* task);
static void _actor113100Exit(Task* task);
static void _actor113100BindLighting(Task* task);
static void _actor113100CheckPierceAppearance(Task* task);
static void _actor113100StepWalk(Task* task);
static void _actor113100QueueWalkHeading(Task* task);
static void _actor113100TurnToArrivalYaw(Task* task);
static s32  _actor113100PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor113100StartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim);
static s32  _actor113100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void _actor113100BillboardTask(Task* task);
static void _actor113100AttachedModelTask(Task* task);

/// Task-state callbacks for Pierce Carradine's camera-facing billboard child.
///
/// `Task::state` selects 0 attach, 1 face the camera or 2 kill the child.
/// Attachment borrows a parent model part through the child's active lifetime.
static const TaskFuncTable3 _gActor113100BillboardStates = { {
    _actor113100AttachBillboard,
    _actor113100FaceBillboardToCamera,
    taskKill,
} };

/// Task-state callbacks shared by Pierce Carradine's two attached models.
///
/// `Task::state` selects 0 attach, 1 inherit the parent's drawing and buffer
/// policy or 2 kill the child. Parent coordinates and lighting remain borrowed.
static const TaskFuncTable3 _gActor113100AttachedModelStates = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Pierce Carradine body callbacks: 0 initialize, 1 update, 2 exit.
///
/// The task owns its body work and attached models; exit unlinks collision before
/// releasing the body and its descendants.
static const TaskFuncTable3 _gActor113100PierceCarradineTaskStates = {
    .funcs = {
        _actor113100SpawnPierceCarradine,
        _actor113100UpdatePierceCarradine,
        _actor113100Exit,
    },
};

/// Pierce Carradine's four scripted-walk steps, copied before dispatch.
///
/// `walk.motionStep` must be in 0..3; the closing turn returns motion to idle.
static const TaskFuncTable4 _gActor113100PierceCarradineWalkSteps = { {
    _actor113100QueueWalkHeading, // 0: queue destination heading and opening-turn clip
    _actor113100TurnAndBeginWalk, // 1: face the destination and begin forward motion
    _actor113100CheckWalkArrival, // 2: stop when the destination is reached or passed
    _actor113100TurnToArrivalYaw, // 3: face the destination yaw and return to idle
} };

static TmdSource _gPierceBody;
static void      _actor113100PierceCarradineTask(Task* task);

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/characters/pierce/id_badge.inc"

#include "assets/rooms/mist/parking/hand_billboard.inc"

#include "assets/rooms/mist/parking/pierce_clips.inc"

/// Pierce Carradine's bank-0 clip table for his twenty-part body rig.
///
/// Clip IDs 1..35 borrow loaded sets; entry 0 is NULL and cannot be played.
/// The outer bank selector supplies this table to the rig. The table and
/// referenced animation resources must stay loaded throughout playback.
#include "assets/rooms/mist/parking/pierce_bank.inc"

/// Body animation banks for Pierce Carradine; bank 0 is the sole 36-entry clip table.
///
/// An animation request's source index selects this table, independently of
/// its animationId. The context borrows the selected bank while the rig lives;
/// callers must select bank 0 and a non-NULL clip in that bank.
static AnimationSet** _gActor113100PierceAnimationBanks[1] = {
    _gActor113100PierceAnimationBank,
};

/// Directions of Pierce's Q12 head-aim weight ramp.
enum {
    ACTOR_113100_HEAD_TURN_RELEASE = 0,
    ACTOR_113100_HEAD_TURN_AIM     = 1,
};

/// Head-aim ramp direction for each of Pierce Carradine's 36 body-clip indices.
///
/// Values are 0 to lower the Q12 head-aim weight toward zero, or 1 to raise it
/// toward full player tracking. Every play request selects its byte even when
/// repeating a clip; later actor commands may override that direction.
static u8 _gActor113100HeadTurnByClip[36] = {
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_AIM,
    ACTOR_113100_HEAD_TURN_RELEASE,
};

static TmdSource _gMistParkingHandBillboard;
static TmdSource _gPierceGlasses;
static TmdSource _gPierceIdBadge;

enum { ACTOR_113100_PIERCE_TASK_PRIORITY = 192 };

TaskDesc gActor113100PierceCarradineTaskDescriptors[4] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_113100_PIERCE_TASK_PRIORITY } },
        .callback = _actor113100PierceCarradineTask,
        .data     = { .model = &_gPierceBody },
    },
    // 1: billboard.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_113100_PIERCE_TASK_PRIORITY } },
        .callback = _actor113100BillboardTask,
        .data     = { .model = &_gMistParkingHandBillboard },
    },
    // 2: part-4 attachment.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_113100_PIERCE_TASK_PRIORITY } },
        .callback = _actor113100AttachedModelTask,
        .data     = { .model = &_gPierceGlasses },
    },
    // 3: part-2 attachment.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_113100_PIERCE_TASK_PRIORITY } },
        .callback = _actor113100AttachedModelTask,
        .data     = { .model = &_gPierceIdBadge },
    },
};

/// Pierce Carradine's synchronous animation, placement, drawing, walk and command messages.
///
/// The five ID-selected callbacks borrow message payloads for dispatch; walk setup
/// copies its destination. The sixth END/null entry terminates lookup.
/// Installed on the initialized body task for that task's lifetime.
static TaskMessageEntry _gActor113100PierceCarradineMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor113100PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor113100SetModelDraw },
    { ACTOR_MESSAGE_WALK_TO, _actor113100StartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor113100ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Creates Pierce Carradine's scripted body, attached models and collision sphere.
///
/// Requires a live twenty-part TMD and owning Enemy in `spawnArg2.pointer`.
/// Owns zeroed work until exit; allocation failure exits the enemy task.
/// Children borrow the parent task and model, and use its placement's textures.
/// Child failures are independent. Variant 2 also creates the camera-facing
/// model. Links an enemy-body sphere of radius 256 on part 1 with one contact
/// slot, then hides the body and disables its sphere pair pass.
/// Installs messages and teardown and advances from spawn state to update.
static void _actor113100SpawnPierceCarradine(Task* task)
{
    enum {
        ACTOR_113100_BILLBOARD_VARIANT     = 2,
        ACTOR_113100_BILLBOARD_CHILD       = 1,
        ACTOR_113100_ATTACHED_CHILD_2      = 2,
        ACTOR_113100_ATTACHED_CHILD_3      = 3,
        ACTOR_113100_BILLBOARD_PARENT_PART = 8,
        ACTOR_113100_CHILD_2_PARENT_PART   = 4,
        ACTOR_113100_CHILD_3_PARENT_PART   = 2,
        ACTOR_113100_BODY_RADIUS           = 256,
        ACTOR_113100_BUFFER_FREE_DISABLED  = -1,
    };
    _Actor113100PierceCarradineWork* work;
    Task*                            attachedTask2;
    Task*                            attachedTask3;
    WorldCollisionBody*              body;

    work = memCalloc(sizeof(_Actor113100PierceCarradineWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_113100_BUFFER_FREE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    if (gGameSession->location.variant == ACTOR_113100_BILLBOARD_VARIANT) {
        work->billboardTask = taskSpawnFromTable(gActor113100PierceCarradineTaskDescriptors, ACTOR_113100_BILLBOARD_CHILD, ACTOR_113100_BILLBOARD_PARENT_PART, task);
    }

    attachedTask2 = taskSpawnFromTable(gActor113100PierceCarradineTaskDescriptors, ACTOR_113100_ATTACHED_CHILD_2, ACTOR_113100_CHILD_2_PARENT_PART, task);
    if (attachedTask2 != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(attachedTask2, task->spawnArg2.pointer);
    }

    attachedTask3 = taskSpawnFromTable(gActor113100PierceCarradineTaskDescriptors, ACTOR_113100_ATTACHED_CHILD_3, ACTOR_113100_CHILD_3_PARENT_PART, task);
    if (attachedTask3 != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(attachedTask3, task->spawnArg2.pointer);
    }

    _actor113100BindLighting(task);

    // Keep the body linked for the work lifetime; visibility controls its pair pass.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[1];
    body->context.contacts = work->contacts;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    body->radius           = ACTOR_113100_BODY_RADIUS;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts), 0);

    task->msgTable = _gActor113100PierceCarradineMessages;
    mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_LOWERED);
    _actor113100SetModelDraw(task, 0, ACTOR_113100_DRAW_HIDE, 0);
    task->exitCallback = _actor113100Exit;
    task->state       += 1;
}

/// Applies one frame of Pierce's signed 16.16 walk velocity to the root.
///
/// Borrows live writable work/root in the same parent frame. Signed integer
/// carries move XYZ before composition is invalidated; unsigned fractional
/// halves remain for the next tick. Preserves the target's 32-bit additions.
static inline void _actor113100IntegrateWalkVelocity(_Actor113100PierceCarradineWork* work, GfxCoord* rootCoord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    rootCoord->coord.t[0]    += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]    += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]    += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Steps Pierce's head-aim weight toward zero or full aim.
///
/// Borrows work with a weight in 0..ONE. Selector 0 releases, 1 aims, and all
/// other signed-byte selectors hold. Each add/subtract narrows to a signed
/// halfword before saturation; the caller controls visibility and freeze gates.
static inline void _actor113100StepHeadAimWeight(_Actor113100PierceCarradineWork* work)
{
    enum { ACTOR_113100_HEAD_WEIGHT_STEP  = ONE / 16,
           ACTOR_113100_HEAD_WEIGHT_FULL  = ONE,
           ACTOR_113100_HEAD_RAMP_RELEASE = 0,
           ACTOR_113100_HEAD_RAMP_AIM     = 1 };
    s8  rampSelector;
    s16 steppedWeight;

    rampSelector = (s8)work->turnUp;
    switch (rampSelector) {
        case ACTOR_113100_HEAD_RAMP_RELEASE:
            steppedWeight    = work->turnWeight - ACTOR_113100_HEAD_WEIGHT_STEP;
            work->turnWeight = steppedWeight;
            if (steppedWeight < 0) {
                work->turnWeight = 0;
            }
            break;
        case ACTOR_113100_HEAD_RAMP_AIM:
            steppedWeight    = work->turnWeight + ACTOR_113100_HEAD_WEIGHT_STEP;
            work->turnWeight = steppedWeight;
            if (steppedWeight >= ACTOR_113100_HEAD_WEIGHT_FULL + 1) {
                work->turnWeight = ACTOR_113100_HEAD_WEIGHT_FULL;
            }
            break;
    }
}

/// Updates Pierce's scripted walk, animation cues, head aim and model presentation.
///
/// Requires initialized live body/work; walk.motion is 0 (idle) or 1 (walking).
/// A drawn body casts its shadow even while scene actors are paused. Motion,
/// slots 1..19, lighting and delayed buffer release advance only while actors
/// run. Velocity is signed 16.16 parent-space units per update; its signed
/// integer part moves the root and its unsigned low halfword stays as carry.
/// Drawn bodies clear their one contact and ramp head weight by 1/16 per tick
/// toward zero or full aim. View 16 selects the alternate first footstep cue
/// and suppresses the second. The tick finding freeCountdown == 0 frees the
/// primitive buffer and decrements the counter to its disabled value of -1.
static void _actor113100UpdatePierceCarradine(Task* task)
{
    enum {
        ACTOR_113100_SHADOW_HALF_SIZE    = 512,
        ACTOR_113100_HEAD_MAX_YAW        = 512,
        ACTOR_113100_HEAD_MAX_PITCH      = 256,
        ACTOR_113100_ALTERNATE_STEP_VIEW = 16,
        ACTOR_113100_STEP_FIRST          = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 0x0F),
        ACTOR_113100_STEP_ALTERNATE      = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 0x13),
        ACTOR_113100_LIGHT_COUNT         = 3,
    };
    TmdObject*                       bodyModel         = task->extra.tmd;
    _Actor113100PierceCarradineWork* work              = task->work;
    TaskFunc                         motionHandlers[2] = { _actor113100CheckPierceAppearance, _actor113100StepWalk };
    VECTOR3                          groundPosition;
    GfxCoord*                        rootCoord;
    const AnimationRecord*           soundCue;
    s32                              slotIndex;
    s32                              soundId;

    // Project a visible body's shadow even when scene motion is frozen.
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_113100_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        motionHandlers[work->walk.motion](task);
        rootCoord = task->extra.tmd->coords;
        _actor113100IntegrateWalkVelocity(work, rootCoord);
        if (work->model.ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationTickSlot(&work->rig.anim, slotIndex);
            }
            soundCue = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (soundCue != NULL) {
                if (soundCue->flags & ANIMATION_RECORD_CUE_2) {
                    soundId = ACTOR_113100_STEP_FIRST;
                    if (gGameSession->location.view == ACTOR_113100_ALTERNATE_STEP_VIEW) {
                        soundId = ACTOR_113100_STEP_ALTERNATE;
                    }
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                }
                if ((soundCue->flags & ANIMATION_RECORD_CUE_1) && (gGameSession->location.view != ACTOR_113100_ALTERNATE_STEP_VIEW)) {
                    sndEvtRequestScriptStart(SOUND_MIST_PARKING_PIERCE_STEP_2, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                }
            }
        }
        if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
            worldCollisionClearContacts(work->contacts);
            _actor113100StepHeadAimWeight(work);
            animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_113100_HEAD_MAX_YAW, ACTOR_113100_HEAD_MAX_PITCH, work->turnWeight);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, ACTOR_113100_LIGHT_COUNT);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                tmdFreePrimitiveBuffer(bodyModel);
            }
            work->freeCountdown--;
        }
    }
}

/// Applies Pierce's bank/clip transition and its clip-selected head-turn ramp.
///
/// Borrows initialized work, live model coordinates and a readable request with
/// `_actor113100PlayAnimation`'s bank, clip and lifetime contract. Repeated clips retain
/// their cursor. Slots 1..19 blend or reset and tick only for a changed clip.
static inline void _actor113100ApplyAnimationRequest(_Actor113100PierceCarradineWork* work,
                                                     TmdObject* model, const AnimationPlayRequest* request)
{
    s32 slotIndex;

    enum { ACTOR_113100_FIRST_DRIVEN_SLOT = 1 };

    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor113100PierceAnimationBanks[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    // Repeated clips keep their cursor; a bank change invalidates the previous clip.
    if (request->animationId != work->model.animId) {
        work->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
            }
        }
        // Seed the new pose before the actor's normal frame update can tick it.
        for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = 1;
    }
    work->turnUp = _gActor113100HeadTurnByClip[request->animationId];
}

/// Turns to the queued heading, then starts the walk at 32 parent-space units per tick.
///
/// Requires initialized body work and a live model root. Angles use 4096 units
/// per turn; the yaw difference narrows to a signed halfword without shortest-turn
/// wrapping. Steps by 64 until within 64, then snaps to `walkYaw`, rotates a
/// 16.16 forward velocity, seeds all arrival gaps and plays clip 2 with a
/// four-frame blend. Advances the walk step only on completion. Rebuilds a pure
/// Y rotation and invalidates composition, retaining translation and stored Euler
/// parameters. The caller integrates velocity after this step.
static void _actor113100TurnAndBeginWalk(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    VECTOR                           forwardVelocity;
    AnimationPlayRequest             walkRequest;
    s32                              currentYaw;
    s32                              currentYaw16;
    u16                              yaw;
    s16                              yawGap;

    enum { ACTOR_113100_WALK_SPEED_16_16 = 32 * 0x10000 };

    rootCoord  = task->extra.tmd->coords;
    work       = task->work;
    currentYaw = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]);
    yaw        = work->walkYaw;
    yawGap     = yaw - currentYaw;
    if (ABS(yawGap) >= ACTOR_113100_YAW_STEP + 1) {
        currentYaw16 = (s16)currentYaw;
        if (yawGap < 0) {
            yaw = currentYaw16 - ACTOR_113100_YAW_STEP;
        } else {
            yaw = currentYaw16 + ACTOR_113100_YAW_STEP;
        }
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        forwardVelocity.vx = 0;
        forwardVelocity.vy = 0;
        forwardVelocity.vz = ACTOR_113100_WALK_SPEED_16_16;
        ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
        work->walk.lastDistance.vx       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        work->walk.lastDistance.vy       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        work->walk.lastDistance.vz       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        walkRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        walkRequest.animationId          = ACTOR_113100_ANIM_WALK;
        walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        walkRequest.blendFrames          = ACTOR_113100_TURN_BLEND_FRAMES;
        walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &walkRequest, 0);
        work->walk.motionStep++;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Stops Pierce's walk when neither horizontal arrival gap gets smaller.
///
/// Requires a live root and initialized walk work. Gaps use root-parent units:
/// the full-word sign chooses a low-halfword subtraction, then each gap narrows
/// to a signed halfword. Previous gaps hold their absolute values. Arrival plays
/// the queued bank-0 clip with a five-frame blend, clears XYZ velocity and advances
/// the walk step. The queued clip must be loaded; position and fractional carry
/// are retained. Otherwise updates only the X/Z previous gaps.
static void _actor113100CheckWalkArrival(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    SVECTOR                          distance;
    s32                              deltaX;
    s32                              deltaZ;
    AnimationPlayRequest             arrivalRequest;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // The full-word sign selects a low-halfword gap; signed narrowing is intentional.
    deltaX      = _actorMotionWalkAxisGap(&work->walk.target.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actorMotionWalkAxisGap(&work->walk.target.vz, &rootCoord->coord.t[2]);
    distance.vz = deltaZ;
    if (distance.vx >= work->walk.lastDistance.vx && distance.vz >= work->walk.lastDistance.vz) {
        arrivalRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        arrivalRequest.animationId          = work->model.nextAnimId;
        arrivalRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        arrivalRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
        arrivalRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = distance.vx < 0 ? -distance.vx : distance.vx;
    work->walk.lastDistance.vz = distance.vz < 0 ? -distance.vz : distance.vz;
}

/// Sets Pierce's body drawing, sphere-pair participation and buffer policy.
///
/// Requires initialized body work and a live TMD task. Modes 0/2 hide and 1/3 show;
/// every accepted mode disables the body's sphere-pair pass. Modes 0/1 permit
/// automatic buffers, and 1 requests a missing buffer immediately. Mode 2 disables
/// automatic buffers and seeds a countdown of 2; two eligible ticks consume it,
/// then the tick reading zero frees the buffers. Mode 3 disables automatic buffers without allocating.
/// Other modes leave any pending release countdown intact. Ignores message ID and
/// second payload. Returns 0 for modes 0..3, or 1 without changes for another mode.
static s32 _actor113100SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    _Actor113100PierceCarradineWork* drawWork;
    WorldCollisionBody*              body;
    WorldCollisionBody*              bodyCursor;
    TmdObject*                       model;
    s32                              bodyIndex;
    s32                              result;

    enum { ACTOR_113100_COLLISION_BODY_COUNT = 1 };

    work     = task->work;
    model    = task->extra.tmd;
    drawWork = task->work;
    body     = &drawWork->body;
    result   = 0;

    switch (drawMode) {
        case ACTOR_113100_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_HIDE_AND_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            drawWork->freeCountdown = ACTOR_113100_BUFFER_RELEASE_DELAY_TICKS;
            model->flags           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &work->body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Copies a walk destination and starts Pierce's four-stage walking sequence.
///
/// Requires initialized body work and a live root. Borrows a readable placement
/// through the call; XYZ position is in the root parent's frame, and Euler angles
/// use 4096 units per turn. Only the final yaw is used by the closing turn.
/// An optional readable clip pair selects the initial and queued arrival clips
/// in bank 0; NULL selects clips 2 and 1. Clip IDs must name loaded entries 1..35.
/// Starts at heading selection, applies the initial clip with a five-frame blend
/// when playback is ticking, and selects its head-turn ramp. Repeated clips keep
/// their cursor. Velocity and fractional carry are retained until later walk steps
/// replace them. Ignores message ID, retains no payload pointer, and returns 0.
static s32 _actor113100StartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim)
{
    _Actor113100PierceCarradineWork* playWork;
    _Actor113100PierceCarradineWork* walkWork;
    AnimationPlayRequest             walkRequest;
    TmdObject*                       model;

    enum { ACTOR_113100_ANIM_DEFAULT_ARRIVAL = 1 };

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = ACTOR_113100_WALK_STEP_QUEUE_HEADING;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    walkRequest.source.index    = ACTOR_113100_BODY_ANIMATION_BANK;
    if (walkAnim != NULL) {
        walkRequest.animationId    = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        walkRequest.animationId    = ACTOR_113100_ANIM_WALK;
        walkWork->model.nextAnimId = ACTOR_113100_ANIM_DEFAULT_ARRIVAL;
    }
    walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    walkRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
    walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    playWork = task->work;
    model    = task->extra.tmd;
    _actor113100ApplyAnimationRequest(playWork, model, &walkRequest);
    return 0;
}

/// Dispatches Pierce's billboard attachment, camera-facing tick or teardown.
///
/// Requires a live TMD task and state 0..2. Setup borrows the parent TMD task from
/// `spawnArg2.pointer` and its coordinate index from `spawnArg1.value`; those
/// resources must outlive the child. The selected callback may destroy the task.
/// There is no bounds check or return value.
static void _actor113100BillboardTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor113100BillboardStates;
    states.funcs[task->state](task);
}

/// Attaches the billboard 100 units above the selected parent model part.
///
/// Setup requires live child and parent TMD tasks, child coordinate 0 and a valid
/// parent coordinate index in `spawnArg1.value`. Borrows the parent from
/// `spawnArg2.pointer` and joins its teardown tree; ancestry must remain acyclic.
/// The coordinate parent remains borrowed until teardown. Initially shows the
/// model only while game flag 0xF1 is zero, then advances to the camera-facing tick.
static void _actor113100AttachBillboard(Task* task)
{
    TmdObject* model;
    Task*      parentTask;
    s32        parentPartIndex;
    GfxCoord*  rootCoord;
    GfxCoord*  parentCoords;

    enum { ACTOR_113100_BILLBOARD_Y_OFFSET = 100 };

    model           = task->extra.tmd;
    parentTask      = task->spawnArg2.pointer;
    parentPartIndex = task->spawnArg1.value;
    rootCoord       = model->coords;
    parentCoords    = parentTask->extra.tmd->coords;

    rootCoord->coord.t[0]   = 0;
    rootCoord->coord.t[1]   = ACTOR_113100_BILLBOARD_Y_OFFSET;
    rootCoord->coord.t[2]   = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    rootCoord->parent       = &parentCoords[parentPartIndex];

    taskReparent(parentTask, task);
    if (gameFlagGetNibble(GAME_FLAG_0F1) == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    task->state += 1;
}

/// Cancels the parent and camera rotations to face Pierce's billboard toward the camera.
///
/// Requires an attached child root, a live parent part selected by `spawnArg1.value`
/// and the active mapped camera. Uses transpose(parent world rotation) times
/// transpose(camera rotation), then applies the parent part's extracted local yaw
/// as an X rotation. Retains local translation and marks composition dirty.
/// The parent world matrix must already be current; no coordinate composition is
/// performed here.
static void _actor113100FaceBillboardToCamera(Task* task)
{
    Task*     parentTask;
    MATRIX    inverseView;
    SVECTOR   parentRotation;
    MATRIX*   viewMatrix;
    MATRIX*   billboardMatrix;
    GfxCoord* parentPart;
    GfxCoord* rootCoord;
    s32       parentPartIndex;

    parentPartIndex = task->spawnArg1.value;
    rootCoord       = task->extra.tmd->coords;
    parentTask      = task->spawnArg2.pointer;
    parentPart      = &parentTask->extra.tmd->coords[parentPartIndex];
    viewMatrix      = &viewGetMappedCamera(&gGameSession->location)->transform;
    billboardMatrix = &rootCoord->coord;
    TransposeMatrix(&parentPart->workm, billboardMatrix);
    TransposeMatrix(viewMatrix, &inverseView);
    MulMatrix0(billboardMatrix, &inverseView, billboardMatrix);
    gfxExtractSmallestEuler(&parentRotation, &parentPart->coord);
    RotMatrixX(parentRotation.vy, billboardMatrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Dispatches an attached model's setup, inherited draw-policy tick or teardown.
///
/// Requires a live TMD task and state 0..2. Setup borrows the parent TMD task from
/// `spawnArg2.pointer` and a valid coordinate index from `spawnArg1.value`; the
/// parent coordinates and lighting must outlive the child. Setup joins the parent
/// teardown tree. Each tick mirrors drawing and buffer policy, allocating a missing
/// buffer when permitted. The selected callback may destroy the task. No bounds
/// check or return value.
static void _actor113100AttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor113100AttachedModelStates;
    states.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach.inc.c"

#include "../../shared/actor/model_placement_mirror_parent.inc.c"

/// Dispatches Pierce Carradine's body setup, frame update or teardown.
///
/// Requires the twenty-part TMD task created by this package's body descriptor
/// and `state` in 0..2. Setup allocates owned work and collision storage; update
/// moves and animates the body; teardown unlinks collision before releasing it.
/// Dispatch continues independently of the scene's actor-control gate. A state
/// callback may destroy the task. The state index is unchecked.
static void _actor113100PierceCarradineTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor113100PierceCarradineTaskStates;
    states.funcs[task->state](task);
}

/// Unlinks Pierce's collision sphere before releasing his body task and work.
///
/// Requires a live initialized work block whose body is linked. The task exit
/// releases its work and descendants; no borrowed lighting or coordinates may be
/// used afterwards.
static void _actor113100Exit(Task* task)
{
    _Actor113100PierceCarradineWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}

/// Makes Pierce's model borrow the light and color matrices in its work block.
///
/// Requires a live TMD task with allocated body work. The matrices remain borrowed
/// until the model is destroyed, so the work block must outlive rendering.
static void _actor113100BindLighting(Task* task)
{
    TmdObject*                       model;
    _Actor113100PierceCarradineWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Reveals Pierce when his appearance flag first becomes positive while he is idle.
///
/// Requires live body work. A positive flag after a recorded zero enables model
/// drawing and restores the parking collision patch. Every idle update records
/// the flag in its signed-byte latch, including updates that do not reveal him.
static void _actor113100CheckPierceAppearance(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    s32                              appearFlag;

    work       = task->work;
    appearFlag = gameFlagGetNibble(GAME_FLAG_0ED);
    if (appearFlag > 0 && work->lastAppearFlag == 0) {
        _actor113100SetModelDraw(task, 0, ACTOR_113100_DRAW_SHOW, 0);
        mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_RESTORED);
    }
    work->lastAppearFlag = appearFlag;
}

/// Dispatches Pierce's heading, opening turn, arrival or closing-turn step.
///
/// Requires live initialized body work and a walk step in 0..3, in that order.
/// The caller integrates the step's velocity after dispatch. The stack copy of
/// the four callbacks is indexed without a bounds check; this is a motion-state
/// index, independent of animation slot indices.
static void _actor113100StepWalk(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    TaskFuncTable4                   walkSteps;

    work      = task->work;
    walkSteps = _gActor113100PierceCarradineWalkSteps;
    walkSteps.funcs[work->walk.motionStep](task);
}

/// Queues the destination heading and starts Pierce's opening-turn clip.
///
/// Requires initialized walk work and a live root coordinate. Normalizes the
/// root-parent offset to `walk.target`, stores its yaw in `walkYaw` (4096 units per
/// turn), and plays bank-0 clip 22 with a four-frame blend. Advances to the opening
/// turn without changing the root rotation or velocity; that next step applies
/// the queued heading.
static void _actor113100QueueWalkHeading(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    AnimationPlayRequest             turnRequest;
    VECTOR                           targetOffset;
    SVECTOR                          direction;

    enum { ACTOR_113100_ANIM_OPENING_TURN = 22 };

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &direction);
    work->walkYaw = ratan2(direction.vx, direction.vz);

    turnRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
    turnRequest.animationId          = ACTOR_113100_ANIM_OPENING_TURN;
    turnRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    turnRequest.blendFrames          = ACTOR_113100_TURN_BLEND_FRAMES;
    turnRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &turnRequest, 0);
    work->walk.motionStep++;
}

/// Turns Pierce's root to the destination yaw, then returns the walk to idle.
///
/// Requires initialized walk work and a live root. Angles use 4096 units per turn;
/// the target-minus-current yaw narrows to a signed halfword without shortest-turn
/// wrapping. Steps by 64 until within 64, then snaps, plays the queued bank-0 clip
/// with a five-frame blend and resets both motion selectors. The queued clip must
/// be loaded. Rebuilds rotation using the extracted pitch and roll, retaining
/// translation and stored Euler parameters, and invalidates composition.
static void _actor113100TurnToArrivalYaw(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    SVECTOR                          rotation;
    AnimationPlayRequest             closingRequest;
    s32                              currentYaw;
    s16                              yawGap;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rotation, &rootCoord->coord);
    yawGap = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawGap) >= ACTOR_113100_YAW_STEP + 1) {
        currentYaw = rotation.vy;
        if (yawGap < 0) {
            rotation.vy = currentYaw - ACTOR_113100_YAW_STEP;
        } else {
            rotation.vy = currentYaw + ACTOR_113100_YAW_STEP;
        }
    } else {
        rotation.vy                         = work->walk.targetRot.vy;
        closingRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        closingRequest.animationId          = work->model.nextAnimId;
        closingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        closingRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
        closingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &closingRequest, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = ACTOR_113100_WALK_STEP_QUEUE_HEADING;
    }

    // Rebuild only rotation; parent-space translation remains intact.
    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies a changed clip to Pierce's twenty-part rig and selects its head-turn ramp.
///
/// Requires live model coordinates and allocated body work with `model.bank`
/// initially `ACTOR_MOTION_ANIMATION_INDEX_NONE`. Borrows a nonoverlapping readable request
/// through the call. The loaded bank is 0; clip IDs 1..35 index both its sets and
/// the head-turn table, and stored IDs narrow to signed bytes. A bank change binds
/// the rig and invalidates the previous clip. A changed clip blends for whole
/// `blendFrames` (normally 0..2047) when blend is nonzero and the rig is ticking, or resets otherwise;
/// slots 1..19 tick once before subsequent frame ticks are enabled. A repeated
/// clip in the same bank keeps its cursor, but still selects the head-turn ramp.
/// Coordinates, slots, poses and loaded clip data remain borrowed for playback's
/// lifetime. Ignores message ID, collision choice and second payload. Returns 0.
static s32 _actor113100PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       model;

    work  = task->work;
    model = task->extra.tmd;
    _actor113100ApplyAnimationRequest(work, model, request);
    return 0;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Shows or hides Pierce's billboard, or selects his head-turn ramp.
///
/// Requires initialized body work and a readable borrowed command through the call.
/// Actions 0/1 show/hide the optional billboard; 2 ramps the head toward the player
/// and 3 releases that turn. A later animation request can replace this ramp choice.
/// Ignores the command context, message ID and second payload. Unknown actions and
/// an absent billboard have no effect. Retains no payload pointer and returns 0.
static s32 _actor113100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       billboardModel;

    enum {
        ACTOR_113100_COMMAND_SHOW_BILLBOARD = 0,
        ACTOR_113100_COMMAND_HIDE_BILLBOARD = 1,
        ACTOR_113100_COMMAND_AIM_HEAD       = 2,
        ACTOR_113100_COMMAND_RELEASE_HEAD   = 3,
    };

    work = task->work;

    switch (command->command) {
        case ACTOR_113100_COMMAND_SHOW_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case ACTOR_113100_COMMAND_HIDE_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case ACTOR_113100_COMMAND_AIM_HEAD:
            work->turnUp = ACTOR_113100_HEAD_TURN_AIM;
            break;

        case ACTOR_113100_COMMAND_RELEASE_HEAD:
            work->turnUp = ACTOR_113100_HEAD_TURN_RELEASE;
            break;

        default:
            return 0;
    }
    return 0;
}
