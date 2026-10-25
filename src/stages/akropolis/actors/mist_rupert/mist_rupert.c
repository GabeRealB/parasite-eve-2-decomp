#include "stages/akropolis/actors/mist_rupert.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../../../shared/actor/actor_messages.h"

/// Eye-band destination within the actor texture: width in VRAM words, Y/height in rows.
enum {
    EYES_Y_ROWS      = 40,
    EYES_WIDTH_WORDS = 32,
    EYES_HEIGHT_ROWS = 16,
};

/// The body part the shadow, the lighting and the placement follow.
enum { PELVIS_PART = 1 };

/// `_Work::blinkStep`: the eye image the blink posts next.
enum {
    BLINK_NONE   = 0, // No blink in progress
    BLINK_CLOSED = 1, // The closed eyes are posted next
    BLINK_HALF   = 2, // The half-open eyes are posted next
    BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of the package's one actor, the hurt Rupert Broderick body a room script places and plays clips on.
/// Both ids start at `ACTOR_MOTION_ANIMATION_INDEX_NONE`, so the first play request binds its bank; after the matrices
/// come the blink that swaps the face texture's eye band and the delayed free of the hidden model's buffers.
typedef struct {
    ActorAnimRig20 rig;             // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s32            ticking;         // Set once a clip has been applied, never cleared: the slots are ticked each frame from then on
    s32            animId;          // Clip the slots were last seeded with, within `bank`; recorded, and no request is skipped for repeating it
    s32            bank;            // Index, in the package's animation bank table, of the bank the rig is bound to
    MATRIX         light;           // Light-direction matrix lent to the model object
    MATRIX         color;           // Light-colour matrix lent to the model object
    s16            blinkFrameDelay; // Value `blinkCountdown` restarts from after the closed and the half-open eyes: each is shown for this many ticks plus one
    s16            blinkCountdown;  // Ticks left before the blink posts its next eye image, which the tick taking it below 0 does; not reset as a blink starts or ends
    s16            blinkStep;       // Eye image the blink posts next (0 `BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            field_4C6;       // Zeroed again by the spawn and never read; role unproven
    s16            freeCountdown;   // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x4CC);

static void _mistRupertSpawn(Task* task);
static void _mistRupertUpdate(Task* task);
static void _mistRupertInitLighting(Task* task);

/// Task-state callbacks for the hurt Rupert Broderick actor.
static const TaskFuncTable3 _gMistRupertStates = { {
    _mistRupertSpawn,
    _mistRupertUpdate,
    enemyTaskExit,
} };

static AnimationSet _gMistR18RupertRestClip;
static AnimationSet _gMistR18RupertBriefing1Clip;
static AnimationSet _gMistR18RupertBriefing2Clip;
static AnimationSet _gMistR18RupertCutTheCrapClip;
static AnimationSet _gMistR18RupertAskAboutAkropolisClip;
static AnimationSet _gMistR18RupertYesSirClip;
static AnimationSet _gMistR18RupertDismissedClip;
static AnimationSet _gMistR18RupertReminderClip;
static TmdSource    _gRupertHurtBody;
static s32          _mistRupertPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32          _mistRupertSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32          _mistRupertSetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg);
static void         _mistRupertTask(Task* task);

#include "assets/characters/rupert/hurt_body.inc"

#include "assets/rooms/mist/r18/rupert_clips.inc"

/// Packed open-eye texture pixels for the actor's face band.
static u_long _gMistRupertOpenEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_open.inc"
};

/// Writable COPY/END list for the open-eye texture band.
static GpuImageUpload _gMistRupertOpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertOpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed half-open-eye pixels for the actor's face texture band.
static u_long _gMistRupertHalfOpenEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_half_open.inc"
};

/// Writable COPY/END list for the half-open-eye texture band.
static GpuImageUpload _gMistRupertHalfOpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertHalfOpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed closed-eye pixels for the actor's face texture band.
static u_long _gMistRupertClosedEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_closed.inc"
};

/// Writable COPY/END list for the closed-eye texture band.
static GpuImageUpload _gMistRupertClosedEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertClosedEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Loaded animation sets for bank 0: clip IDs 1..8, with NULL at 0.
#include "assets/rooms/mist/r18/rupert_bank.inc"

/// Indexed animation banks; `AnimationPlayRequest.source.index` must be 0.
static AnimationSet** _gMistRupertAnimationBanks[1] = {
    _gMistRupertAnimationSets,
};

TaskDesc gMistRupertTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _mistRupertTask,
    .data     = { .model = &_gRupertHurtBody },
};

/// Hurt Rupert message dispatch: animation, Euler placement, drawing and eye images.
static TaskMessageEntry _gMistRupertMessages[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _mistRupertPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _mistRupertSetModelDraw },
    { ACTOR_MESSAGE_SET_TEXTURE_MODE, _mistRupertSetEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Queues a closed or half-open eye image, restarts its dwell and advances the blink.
static inline void _mistRupertAdvanceBlinkImage(Task* actorTask, _Work* work,
                                                GpuImageUpload* uploadList, const RECT* eyeRect)
{
    actorRenderUploadTexture(actorTask, uploadList, eyeRect);
    work->blinkCountdown = work->blinkFrameDelay;
    ++work->blinkStep;
}

/// Advances the actor's timed closed, half-open and open eye sequence by one tick.
static void _mistRupertTickBlink(Task* task)
{
    _Work* work;
    RECT   eyeRect;

    work      = task->work;
    eyeRect.x = 0;
    eyeRect.y = EYES_Y_ROWS;
    eyeRect.w = EYES_WIDTH_WORDS;
    eyeRect.h = EYES_HEIGHT_ROWS;

    switch (work->blinkStep) {
        case BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _mistRupertAdvanceBlinkImage(task, work, &_gMistRupertClosedEyesUploads[0], &eyeRect);
            }
            break;
        case BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _mistRupertAdvanceBlinkImage(task, work, &_gMistRupertHalfOpenEyesUploads[0], &eyeRect);
            }
            break;
        case BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &_gMistRupertOpenEyesUploads[0], &eyeRect);
                work->blinkStep = BLINK_NONE;
            }
            break;
    }
}

/// Dispatches the hurt Rupert actor's initialization, update or teardown state.
static void _mistRupertTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistRupertStates;
    states.funcs[task->state](task);
}

/// Initializes the hurt Rupert actor as a hidden, script-controlled model.
static void _mistRupertSpawn(Task* task)
{
    enum {
        BUFFER_FREE_NONE         = -1,
        INITIAL_SHADOW_HALF_SIZE = 512,
    };
    _Work*     work;
    TmdObject* model;
    VECTOR3    groundPoint;
    u16        modelFlags;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank          = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->field_4C6     = 0;
    work->freeCountdown = BUFFER_FREE_NONE;
    modelFlags          = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags        = modelFlags;
    // Retain the shadow branch even though initialization has just hidden the model.
    if (!(modelFlags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[PELVIS_PART].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, INITIAL_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    _mistRupertInitLighting(task);
    task->msgTable     = _gMistRupertMessages;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Updates the hurt Rupert actor's animation, shadow, lighting, eyes and pending buffer release.
static void _mistRupertUpdate(Task* task)
{
    enum { SHADOW_HALF_SIZE = 768 };
    _Work*     work;
    TmdObject* model;
    GfxCoord*  partCoords;
    VECTOR3    groundPoint;
    s32        slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (work->ticking != 0) {
        // Slot zero is the script-placed root, outside clip playback.
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[PELVIS_PART].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        partCoords                           = task->extra.tmd->coords;
        partCoords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&partCoords[PELVIS_PART]);
        worldCoordSetModelLighting(model, partCoords[PELVIS_PART].workm.t, 0, ARRAY_SIZE(work->light.m));
    }
    _mistRupertTickBlink(task);
    // Leave the primitive buffer alive through the intervening frame updates.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Binds the actor's lighting matrices and samples room lighting at the pelvis.
static void _mistRupertInitLighting(Task* task)
{
    _Work*     work;
    GfxCoord*  partCoords;
    TmdObject* model;

    work                                 = task->work;
    model                                = task->extra.tmd;
    partCoords                           = model->coords;
    model->lightMtx                      = &work->light;
    model->colorMtx                      = &work->color;
    partCoords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&partCoords[PELVIS_PART]);
    worldCoordSetModelLighting(model, partCoords[PELVIS_PART].workm.t, 0, ARRAY_SIZE(work->light.m));
}

/// Starts a requested clip on the hurt Rupert actor's nineteen animated parts.
static s32 _mistRupertPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum { ANIMATION_BLEND_FRAMES = 6 };
    _Work*     work;
    TmdObject* model;
    s32        slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->bank) {
        // Bind the bank before selecting a clip, including the first request.
        work->bank   = request->source.index;
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gMistRupertAnimationBanks[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ANIMATION_BLEND_FRAMES);
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->animId);
        }
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->ticking = true;
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_euler.inc.c"

/// Changes the hurt Rupert actor's drawing and primitive-buffer policy.
static s32 _mistRupertSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject* model;
    _Work*     work;
    s32        result;

    model  = task->extra.tmd;
    work   = task->work;
    result = 0;
    switch (drawMode) {
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
            // This mode's value also supplies the two-tick release countdown.
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Sets the hurt Rupert actor's eye image or starts a timed blink for message 0x7E0.
static s32 _mistRupertSetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg)
{
    enum {
        EYES_OPEN           = 0,
        EYES_CLOSED         = 1,
        EYES_OPEN_ALTERNATE = 2,
        EYES_BLINK          = 3,
        BLINK_FRAME_DELAY   = 1,
    };
    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             result;

    result    = 0;
    eyeRect.x = 0;
    eyeRect.y = EYES_Y_ROWS;
    eyeRect.w = EYES_WIDTH_WORDS;
    eyeRect.h = EYES_HEIGHT_ROWS;

    switch (eyeMode) {
        case EYES_CLOSED:
            uploadList = &_gMistRupertClosedEyesUploads[0];
            break;
        case EYES_OPEN:
        case EYES_OPEN_ALTERNATE:
            uploadList = &_gMistRupertOpenEyesUploads[0];
            break;
        case EYES_BLINK: {
            _Work* stepWork;
            _Work* delayWork;

            stepWork                   = task->work;
            stepWork->blinkStep        = BLINK_CLOSED;
            delayWork                  = task->work;
            delayWork->blinkFrameDelay = BLINK_FRAME_DELAY;
            uploadList                 = &_gMistRupertHalfOpenEyesUploads[0];
            break;
        }
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        result = actorRenderUploadTexture(task, uploadList, &eyeRect);
    }
    return result;
}
