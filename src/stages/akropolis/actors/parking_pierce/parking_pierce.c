#include "stages/akropolis/actors/parking_pierce.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
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
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "stages/akropolis/rooms/mist_parking.h"

#include "assets/rooms/mist/parking/pierce_schema.h"

#include "../../rooms/mist_parking/config.h"
#include "../../../../shared/actor/model_placement.h"
#include "../../../../shared/actor/actor_messages.h"
#include "../../../../shared/actor/actor_motion_walk_helpers.h"

/// Ticks a hide without the automatic buffer waits before the body's buffers are freed.
enum { BUFFER_RELEASE_DELAY_TICKS = 2 };

/// The parts Pierce's sphere and lighting follow (the pelvis) and his attachments ride.
enum {
    PELVIS_PART     = 1,
    WAIST_PART      = 2, // The ID badge
    HEAD_PART       = 4, // The glasses
    RIGHT_HAND_PART = 8, // The hand billboard
};

/// Pierce's walk clips and transitions; velocity uses signed 16.16 units per tick.
enum {
    BODY_ANIMATION_BANK     = 0,
    WALK_STEP_QUEUE_HEADING = 0,
    TURN_BLEND_FRAMES       = 4,
    WALK_BLEND_FRAMES       = 5,
    YAW_STEP                = 64,
};

static void _modelPlacementMirrorParentDrawFlags(Task* childTask);

/// Work block of Pierce Carradine's body, the package's scripted walker.
typedef struct {
    ActorAnimRig20        rig;            // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState       model;          // Clip and bank the rig plays, and the matrices the model is lit with
    WorldCollisionBody    body;           // Sphere on the pelvis, linked for the task's life; the spawn state enables its pair pass and every visibility command disables it
    WorldCollisionContact contacts[1];    // One-entry table `body` borrows. The entry is marked LAST; an occupied contact is cleared each frame the model is drawn and never read
    ActorWalkState        walk;           // Destination, closing rotation, per-frame velocity and step of the walk in progress
    Task*                 billboardTask;  // Task of the camera-facing model carried on the right hand, which an actor command shows and hides; started only in placement variant 2, NULL otherwise
    s16                   turnWeight;     // Weight of the per-frame head turn toward the player, 0 to 0x1000; stepped by 0x100 a frame while the model is drawn
    s16                   walkYaw;        // Heading from the root to `walk.target` as a walk starts, 4096 to a turn; the opening turn steers the root yaw to it
    u8                    turnUp;         // Direction `turnWeight` is ramped in (0 down to 0, 1 up to 0x1000); every play request sets it from the clip, and an actor command overrides it
    s8                    freeCountdown;  // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s8                    lastAppearFlag; // `EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT` as the idle handler last read it; the actor appears on the tick the flag is positive and this is still 0
    byte                  pad_53F[1];
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x540);

static void _parkingPierceSpawn(Task* task);
static void _parkingPierceUpdate(Task* task);
static void _parkingPierceTurnAndBeginWalk(Task* task);
static void _parkingPierceCheckWalkArrival(Task* task);
static s32  _parkingPierceSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void _parkingPierceAttachBillboard(Task* task);
static void _parkingPierceFaceBillboardToCamera(Task* task);
static void _parkingPierceExit(Task* task);
static void _parkingPierceBindLighting(Task* task);
static void _parkingPierceCheckAppearance(Task* task);
static void _parkingPierceStepWalk(Task* task);
static void _parkingPierceQueueWalkHeading(Task* task);
static void _parkingPierceTurnToArrivalYaw(Task* task);
static s32  _parkingPiercePlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _parkingPierceStartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim);
static s32  _parkingPierceApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void _parkingPierceBillboardTask(Task* task);
static void _parkingPierceAttachedModelTask(Task* task);

/// Task-state callbacks for Pierce Carradine's camera-facing billboard child.
static const TaskFuncTable3 _gParkingPierceBillboardStates = { {
    _parkingPierceAttachBillboard,
    _parkingPierceFaceBillboardToCamera,
    taskKill,
} };

/// Task-state callbacks shared by Pierce Carradine's two attached models.
static const TaskFuncTable3 _gParkingPierceAttachedModelStates = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Pierce Carradine body callbacks: 0 initialize, 1 update, 2 exit.
static const TaskFuncTable3 _gParkingPierceStates = {
    .funcs = {
        _parkingPierceSpawn,
        _parkingPierceUpdate,
        _parkingPierceExit,
    },
};

/// Pierce Carradine's four scripted-walk steps, copied before dispatch.
static const TaskFuncTable4 _gParkingPierceWalkSteps = { {
    _parkingPierceQueueWalkHeading, // 0: queue destination heading and opening-turn clip
    _parkingPierceTurnAndBeginWalk, // 1: face the destination and begin forward motion
    _parkingPierceCheckWalkArrival, // 2: stop when the destination is reached or passed
    _parkingPierceTurnToArrivalYaw, // 3: face the destination yaw and return to idle
} };

static TmdSource _gPierceBody;
static void      _parkingPierceTask(Task* task);

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/characters/pierce/id_badge.inc"

#include "assets/rooms/mist/parking/hand_billboard.inc"

#include "assets/rooms/mist/parking/pierce_clips.inc"

/// Pierce Carradine's bank-0 clip table for his twenty-part body rig.
#include "assets/rooms/mist/parking/pierce_bank.inc"

/// Body animation banks for Pierce Carradine; bank 0 is the sole 36-entry clip table.
static AnimationSet** _gParkingPierceAnimationBanks[1] = {
    _gParkingPierceAnimationSets,
};

/// Directions of Pierce's Q12 head-aim weight ramp.
enum {
    HEAD_TURN_RELEASE = 0,
    HEAD_TURN_AIM     = 1,
};

/// Head-aim ramp direction for each of Pierce's 36 clips: every play request takes its clip's, an actor command may override it.
static u8 _gParkingPierceHeadTurnByClip[36] = {
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
    HEAD_TURN_AIM,
    HEAD_TURN_AIM,
    HEAD_TURN_RELEASE,
};

static TmdSource _gMistParkingHandBillboard;
static TmdSource _gPierceGlasses;
static TmdSource _gPierceIdBadge;

TaskDesc gParkingPierceTaskDescs[4] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _parkingPierceTask,
        .data     = { .model = &_gPierceBody },
    },
    // 1: hand billboard.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _parkingPierceBillboardTask,
        .data     = { .model = &_gMistParkingHandBillboard },
    },
    // 2: glasses, on the head.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _parkingPierceAttachedModelTask,
        .data     = { .model = &_gPierceGlasses },
    },
    // 3: ID badge, on the waist.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _parkingPierceAttachedModelTask,
        .data     = { .model = &_gPierceIdBadge },
    },
};

/// Pierce Carradine's synchronous animation, placement, drawing, walk and command messages.
static TaskMessageEntry _gParkingPierceMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _parkingPiercePlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _parkingPierceSetModelDraw },
    { ACTOR_MESSAGE_WALK_TO, _parkingPierceStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _parkingPierceApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Creates Pierce Carradine's scripted body, attached models and collision sphere.
static void _parkingPierceSpawn(Task* task)
{
    enum {
        BILLBOARD_VARIANT    = 2,
        BILLBOARD_CHILD      = 1,
        GLASSES_CHILD        = 2,
        ID_BADGE_CHILD       = 3,
        BODY_RADIUS          = 256,
        BUFFER_FREE_DISABLED = -1,
    };
    _Work*              work;
    Task*               attachedTask2;
    Task*               attachedTask3;
    WorldCollisionBody* body;

    work = memCalloc(sizeof(_Work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = BUFFER_FREE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    if (gGameSession->location.variant == BILLBOARD_VARIANT) {
        work->billboardTask = taskSpawnFromTable(gParkingPierceTaskDescs, BILLBOARD_CHILD, RIGHT_HAND_PART, task);
    }

    attachedTask2 = taskSpawnFromTable(gParkingPierceTaskDescs, GLASSES_CHILD, HEAD_PART, task);
    if (attachedTask2 != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(attachedTask2, task->spawnArg2.pointer);
    }

    attachedTask3 = taskSpawnFromTable(gParkingPierceTaskDescs, ID_BADGE_CHILD, WAIST_PART, task);
    if (attachedTask3 != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(attachedTask3, task->spawnArg2.pointer);
    }

    _parkingPierceBindLighting(task);

    // Keep the body linked for the work lifetime; visibility controls its pair pass.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[PELVIS_PART];
    body->context.contacts = work->contacts;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    body->radius           = BODY_RADIUS;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts), 0);

    task->msgTable = _gParkingPierceMessages;
    mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_LOWERED);
    _parkingPierceSetModelDraw(task, 0, ACTOR_MESSAGE_DRAW_HIDE, 0);
    task->exitCallback = _parkingPierceExit;
    task->state       += 1;
}

/// Applies one frame of Pierce's signed 16.16 walk velocity to the root.
static inline void _parkingPierceIntegrateWalkVelocity(_Work* work, GfxCoord* rootCoord)
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
static inline void _parkingPierceStepHeadAimWeight(_Work* work)
{
    enum { WEIGHT_STEP  = ONE / 16,
           WEIGHT_FULL  = ONE,
           RAMP_RELEASE = 0,
           RAMP_AIM     = 1 };
    s8  rampSelector;
    s16 steppedWeight;

    rampSelector = (s8)work->turnUp;
    switch (rampSelector) {
        case RAMP_RELEASE:
            steppedWeight    = work->turnWeight - WEIGHT_STEP;
            work->turnWeight = steppedWeight;
            if (steppedWeight < 0) {
                work->turnWeight = 0;
            }
            break;
        case RAMP_AIM:
            steppedWeight    = work->turnWeight + WEIGHT_STEP;
            work->turnWeight = steppedWeight;
            if (steppedWeight >= WEIGHT_FULL + 1) {
                work->turnWeight = WEIGHT_FULL;
            }
            break;
    }
}

/// Updates Pierce's scripted walk, animation cues, head aim and model presentation.
static void _parkingPierceUpdate(Task* task)
{
    enum {
        SHADOW_HALF_SIZE    = 512,
        HEAD_MAX_YAW        = 512,
        HEAD_MAX_PITCH      = 256,
        ALTERNATE_STEP_VIEW = 16,
        STEP_FIRST          = MIST_PARKING_SOUND(MIST_PARKING_SOUND_PIERCE_STEP_1),
        STEP_ALTERNATE      = MIST_PARKING_SOUND(MIST_PARKING_SOUND_PIERCE_STEP_ALTERNATE),
        LIGHT_COUNT         = 3,
    };
    TmdObject*             bodyModel         = task->extra.tmd;
    _Work*                 work              = task->work;
    TaskFunc               motionHandlers[2] = { _parkingPierceCheckAppearance, _parkingPierceStepWalk };
    VECTOR3                groundPosition;
    GfxCoord*              rootCoord;
    const AnimationRecord* soundCue;
    s32                    slotIndex;
    s32                    soundId;

    // Project a visible body's shadow even when scene motion is frozen.
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        task->extra.tmd->coords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[PELVIS_PART]);
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[PELVIS_PART].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        motionHandlers[work->walk.motion](task);
        rootCoord = task->extra.tmd->coords;
        _parkingPierceIntegrateWalkVelocity(work, rootCoord);
        if (work->model.ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationTickSlot(&work->rig.anim, slotIndex);
            }
            soundCue = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (soundCue != NULL) {
                if (soundCue->flags & ANIMATION_RECORD_CUE_2) {
                    soundId = STEP_FIRST;
                    if (gGameSession->location.view == ALTERNATE_STEP_VIEW) {
                        soundId = STEP_ALTERNATE;
                    }
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                }
                if ((soundCue->flags & ANIMATION_RECORD_CUE_1) && (gGameSession->location.view != ALTERNATE_STEP_VIEW)) {
                    sndEvtRequestScriptStart(SOUND_MIST_PARKING_PIERCE_STEP_2, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                }
            }
        }
        if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
            worldCollisionClearContacts(work->contacts);
            _parkingPierceStepHeadAimWeight(work);
            animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), HEAD_MAX_YAW, HEAD_MAX_PITCH, work->turnWeight);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&task->extra.tmd->coords[PELVIS_PART]);
            worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[PELVIS_PART].workm.t, 0, LIGHT_COUNT);
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
static inline void _parkingPierceApplyAnimationRequest(_Work*     work,
                                                       TmdObject* model, const AnimationPlayRequest* request)
{
    s32 slotIndex;

    enum { FIRST_DRIVEN_SLOT = 1 };

    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gParkingPierceAnimationBanks[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    // Repeated clips keep their cursor; a bank change invalidates the previous clip.
    if (request->animationId != work->model.animId) {
        work->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
            }
        }
        // Seed the new pose before the actor's normal frame update can tick it.
        for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = 1;
    }
    work->turnUp = _gParkingPierceHeadTurnByClip[request->animationId];
}

/// Turns to the queued heading, then starts the walk at 32 parent-space units per tick.
static void _parkingPierceTurnAndBeginWalk(Task* task)
{
    _Work*               work;
    GfxCoord*            rootCoord;
    VECTOR               forwardVelocity;
    AnimationPlayRequest walkRequest;
    s32                  currentYaw;
    s32                  currentYaw16;
    u16                  yaw;
    s16                  yawGap;

    enum { WALK_SPEED_16_16 = 32 * 0x10000 };

    rootCoord  = task->extra.tmd->coords;
    work       = task->work;
    currentYaw = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]);
    yaw        = work->walkYaw;
    yawGap     = yaw - currentYaw;
    if (ABS(yawGap) >= YAW_STEP + 1) {
        currentYaw16 = (s16)currentYaw;
        if (yawGap < 0) {
            yaw = currentYaw16 - YAW_STEP;
        } else {
            yaw = currentYaw16 + YAW_STEP;
        }
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        forwardVelocity.vx = 0;
        forwardVelocity.vy = 0;
        forwardVelocity.vz = WALK_SPEED_16_16;
        ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
        work->walk.lastDistance.vx       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        work->walk.lastDistance.vy       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        work->walk.lastDistance.vz       = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
        walkRequest.source.index         = BODY_ANIMATION_BANK;
        walkRequest.animationId          = PIERCE_ANIMATION_WALK;
        walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        walkRequest.blendFrames          = TURN_BLEND_FRAMES;
        walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _parkingPiercePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &walkRequest, 0);
        work->walk.motionStep++;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Stops Pierce's walk when neither horizontal arrival gap gets smaller.
static void _parkingPierceCheckWalkArrival(Task* task)
{
    _Work*               work;
    GfxCoord*            rootCoord;
    SVECTOR              distance;
    s32                  deltaX;
    s32                  deltaZ;
    AnimationPlayRequest arrivalRequest;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // The full-word sign selects a low-halfword gap; signed narrowing is intentional.
    deltaX      = _actorMotionWalkAxisGap(&work->walk.target.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actorMotionWalkAxisGap(&work->walk.target.vz, &rootCoord->coord.t[2]);
    distance.vz = deltaZ;
    if (distance.vx >= work->walk.lastDistance.vx && distance.vz >= work->walk.lastDistance.vz) {
        arrivalRequest.source.index         = BODY_ANIMATION_BANK;
        arrivalRequest.animationId          = work->model.nextAnimId;
        arrivalRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        arrivalRequest.blendFrames          = WALK_BLEND_FRAMES;
        arrivalRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _parkingPiercePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
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
static s32 _parkingPierceSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    _Work*              work;
    _Work*              drawWork;
    WorldCollisionBody* body;
    WorldCollisionBody* bodyCursor;
    TmdObject*          model;
    s32                 bodyIndex;
    s32                 result;

    enum { COLLISION_BODY_COUNT = 1 };

    work     = task->work;
    model    = task->extra.tmd;
    drawWork = task->work;
    body     = &drawWork->body;
    result   = 0;

    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            drawWork->freeCountdown = BUFFER_RELEASE_DELAY_TICKS;
            model->flags           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &work->body;
            for (bodyIndex = 0; bodyIndex < COLLISION_BODY_COUNT; bodyIndex++) {
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
static s32 _parkingPierceStartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim)
{
    _Work*               playWork;
    _Work*               walkWork;
    AnimationPlayRequest walkRequest;
    TmdObject*           model;

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = WALK_STEP_QUEUE_HEADING;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    walkRequest.source.index    = BODY_ANIMATION_BANK;
    if (walkAnim != NULL) {
        walkRequest.animationId    = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        walkRequest.animationId    = PIERCE_ANIMATION_WALK;
        walkWork->model.nextAnimId = PIERCE_ANIMATION_REST;
    }
    walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    walkRequest.blendFrames          = WALK_BLEND_FRAMES;
    walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    playWork = task->work;
    model    = task->extra.tmd;
    _parkingPierceApplyAnimationRequest(playWork, model, &walkRequest);
    return 0;
}

/// Dispatches Pierce's billboard attachment, camera-facing tick or teardown.
static void _parkingPierceBillboardTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gParkingPierceBillboardStates;
    states.funcs[task->state](task);
}

/// Attaches the billboard 100 units above the selected parent model part.
static void _parkingPierceAttachBillboard(Task* task)
{
    TmdObject* model;
    Task*      parentTask;
    s32        parentPartIndex;
    GfxCoord*  rootCoord;
    GfxCoord*  parentCoords;

    enum { BILLBOARD_Y_OFFSET = 100 };

    model           = task->extra.tmd;
    parentTask      = task->spawnArg2.pointer;
    parentPartIndex = task->spawnArg1.value;
    rootCoord       = model->coords;
    parentCoords    = parentTask->extra.tmd->coords;

    rootCoord->coord.t[0]   = 0;
    rootCoord->coord.t[1]   = BILLBOARD_Y_OFFSET;
    rootCoord->coord.t[2]   = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    rootCoord->parent       = &parentCoords[parentPartIndex];

    taskReparent(parentTask, task);
    if (gameFlagGetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE) == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    task->state += 1;
}

/// Cancels the parent and camera rotations to face Pierce's billboard toward the camera.
static void _parkingPierceFaceBillboardToCamera(Task* task)
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
static void _parkingPierceAttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gParkingPierceAttachedModelStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/actor/model_placement_attach.inc.c"

#include "../../../../shared/actor/model_placement_mirror_parent.inc.c"

/// Dispatches Pierce Carradine's body setup, frame update or teardown.
static void _parkingPierceTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gParkingPierceStates;
    states.funcs[task->state](task);
}

/// Unlinks Pierce's collision sphere before releasing his body task and work.
static void _parkingPierceExit(Task* task)
{
    _Work* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}

/// Makes Pierce's model borrow the light and color matrices in its work block.
static void _parkingPierceBindLighting(Task* task)
{
    TmdObject* model;
    _Work*     work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Reveals Pierce when his appearance flag first becomes positive while he is idle.
static void _parkingPierceCheckAppearance(Task* task)
{
    _Work* work;
    s32    appearFlag;

    work       = task->work;
    appearFlag = gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT);
    if (appearFlag > 0 && work->lastAppearFlag == 0) {
        _parkingPierceSetModelDraw(task, 0, ACTOR_MESSAGE_DRAW_SHOW, 0);
        mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_RESTORED);
    }
    work->lastAppearFlag = appearFlag;
}

/// Dispatches Pierce's heading, opening turn, arrival or closing-turn step.
static void _parkingPierceStepWalk(Task* task)
{
    _Work*         work;
    TaskFuncTable4 walkSteps;

    work      = task->work;
    walkSteps = _gParkingPierceWalkSteps;
    walkSteps.funcs[work->walk.motionStep](task);
}

/// Queues the destination heading and starts Pierce's opening-turn clip.
static void _parkingPierceQueueWalkHeading(Task* task)
{
    _Work*               work;
    GfxCoord*            rootCoord;
    AnimationPlayRequest turnRequest;
    VECTOR               targetOffset;
    SVECTOR              direction;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &direction);
    work->walkYaw = ratan2(direction.vx, direction.vz);

    turnRequest.source.index         = BODY_ANIMATION_BANK;
    turnRequest.animationId          = PIERCE_ANIMATION_TURN;
    turnRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    turnRequest.blendFrames          = TURN_BLEND_FRAMES;
    turnRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _parkingPiercePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &turnRequest, 0);
    work->walk.motionStep++;
}

/// Turns Pierce's root to the destination yaw, then returns the walk to idle.
static void _parkingPierceTurnToArrivalYaw(Task* task)
{
    _Work*               work;
    GfxCoord*            rootCoord;
    SVECTOR              rotation;
    AnimationPlayRequest closingRequest;
    s32                  currentYaw;
    s16                  yawGap;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rotation, &rootCoord->coord);
    yawGap = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawGap) >= YAW_STEP + 1) {
        currentYaw = rotation.vy;
        if (yawGap < 0) {
            rotation.vy = currentYaw - YAW_STEP;
        } else {
            rotation.vy = currentYaw + YAW_STEP;
        }
    } else {
        rotation.vy                         = work->walk.targetRot.vy;
        closingRequest.source.index         = BODY_ANIMATION_BANK;
        closingRequest.animationId          = work->model.nextAnimId;
        closingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        closingRequest.blendFrames          = WALK_BLEND_FRAMES;
        closingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _parkingPiercePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &closingRequest, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = WALK_STEP_QUEUE_HEADING;
    }

    // Rebuild only rotation; parent-space translation remains intact.
    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies a changed clip to Pierce's twenty-part rig and selects its head-turn ramp.
static s32 _parkingPiercePlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Work*     work;
    TmdObject* model;

    work  = task->work;
    model = task->extra.tmd;
    _parkingPierceApplyAnimationRequest(work, model, request);
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_euler.inc.c"

/// Shows or hides Pierce's billboard, or selects his head-turn ramp.
static s32 _parkingPierceApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Work*     work;
    TmdObject* billboardModel;

    enum {
        SHOW_BILLBOARD = 0,
        HIDE_BILLBOARD = 1,
        AIM_HEAD       = 2,
        RELEASE_HEAD   = 3,
    };

    work = task->work;

    switch (command->command) {
        case SHOW_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case HIDE_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case AIM_HEAD:
            work->turnUp = HEAD_TURN_AIM;
            break;

        case RELEASE_HEAD:
            work->turnUp = HEAD_TURN_RELEASE;
            break;

        default:
            return 0;
    }
    return 0;
}
