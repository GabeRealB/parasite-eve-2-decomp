#include "actors/task_tables.h"

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
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

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

/// `_Work::blinkStep`: the eye image the blink posts next.
///
/// A blink posts the closed, half-open and open eyes in turn, so the eyes are
/// seen opening. The request that starts one posts the half-open eyes itself,
/// which is the closing half of the blink.
enum {
    BLINK_NONE   = 0, // No blink in progress
    BLINK_CLOSED = 1, // The closed eyes are posted next
    BLINK_HALF   = 2, // The half-open eyes are posted next
    BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of the package's one actor, the hurt Rupert Broderick body a
/// room script places and plays clips on.
///
/// The task's spawn state allocates it zeroed at its full size and keeps it at
/// `Task::work` for the task's life. It opens with the rig of the twenty-part
/// body model and what that rig is playing, kept as words: a play request
/// rebinds the rig when its bank differs from `bank`, seeds the slots with its
/// clip and records it in `animId`. The spawn sets both ids to
/// `ACTOR_MOTION_ANIMATION_INDEX_NONE`, so the first request always binds its bank. The
/// model object borrows `light` and `color` for as long as the block lives.
///
/// What follows the matrices is the blink that swaps the eye band of the
/// face texture, and the delayed free of the model's buffers once the model
/// has been hidden.
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
///
/// `Task::state` selects 0 spawn, 1 frame update or 2 enemy teardown.
/// All three entries take the live task; there is no terminator or bounds check.
static const TaskFuncTable3 _gMistRupertRupertBroderickHurtStates = { {
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
///
/// 32 VRAM halfwords per row by 16 rows (256 packed 32-bit words).
/// The upload list borrows this word-aligned storage until GPU transfer completion.
static u_long _gMistRupertOpenEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_open.inc"
};

/// Writable COPY/END list for the open-eye texture band.
///
/// One copy covers 32 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gMistRupertOpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertOpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed half-open-eye pixels for the actor's face texture band.
///
/// 32 VRAM halfwords by 16 rows fill all 256 packed words.
/// This word-aligned package storage stays live through GPU transfer.
static u_long _gMistRupertHalfOpenEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_half_open.inc"
};

/// Writable COPY/END list for the half-open-eye texture band.
///
/// One copy covers 32 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gMistRupertHalfOpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertHalfOpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed closed-eye pixels for the actor's face texture band.
///
/// 32 VRAM halfwords by 16 rows fill all 256 packed words.
/// This word-aligned package storage stays live through GPU transfer.
static u_long _gMistRupertClosedEyesPixels[256] = {
#include "assets/characters/rupert/hurt_eyes_closed.inc"
};

/// Writable COPY/END list for the closed-eye texture band.
///
/// One copy covers 32 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gMistRupertClosedEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYES_WIDTH_WORDS, EYES_HEIGHT_ROWS }, _gMistRupertClosedEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Loaded animation sets for bank 0: clip IDs 1..8, with NULL at 0.
///
/// Playback borrows this table and the package's clip descriptors; requests
/// must select a loaded clip and keep the package live until teardown.
#include "assets/rooms/mist/r18/rupert_bank.inc"

/// Indexed animation banks; `AnimationPlayRequest.source.index` must be 0.
///
/// A bank change binds its borrowed clip table into the live animation rig.
/// The table and clips remain loaded for the duration of playback.
static AnimationSet** _gMistRupertAnimationBanks[1] = {
    _gMistRupertAnimationSets,
};

TaskDesc gMistRupertRupertBroderickHurtTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _mistRupertTask,
    .data     = { .model = &_gRupertHurtBody },
};

/// Eye-image selection or timed-blink message for the hurt Rupert body.
enum { MESSAGE_SET_EYES = 2016 };

/// Hurt Rupert message dispatch: animation, Euler placement, drawing and eye images.
///
/// The spawned actor borrows this terminated table and its callbacks until teardown.
/// Eye mode 0/2 opens the eyes, 1 closes them, and 3 starts a blink.
static TaskMessageEntry _gMistRupertMessages[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _mistRupertPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _mistRupertSetModelDraw },
    { MESSAGE_SET_EYES, _mistRupertSetEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Queues a closed or half-open eye image, restarts its dwell and advances the blink.
///
/// Requires a live TMD `actorTask` and its writable `work`, with `blinkStep`
/// equal to `BLINK_CLOSED` or `BLINK_HALF`. Resets
/// `blinkCountdown` to `blinkFrameDelay` and selects the following step. A
/// nonnegative delay makes that step due after that many blink ticks plus one;
/// the caller performs the countdown, and this helper does not test it.
///
/// `uploadList` must be non-NULL, writable and terminated, with the eye image
/// in its first copy entry. That destination is replaced using `eyeRect` and
/// the model's texture page. Rectangle units and transfer bounds follow
/// `actorRenderUploadTexture`. The rectangle and records
/// are borrowed only for this call; pixel storage must remain valid and unchanged
/// until GPU transfer completes. Upload results are ignored; this does not wait.
static inline void _mistRupertAdvanceBlinkImage(Task* actorTask, _Work* work,
                                                GpuImageUpload* uploadList, const RECT* eyeRect)
{
    actorRenderUploadTexture(actorTask, uploadList, eyeRect);
    work->blinkCountdown = work->blinkFrameDelay;
    ++work->blinkStep;
}

/// Advances the actor's timed closed, half-open and open eye sequence by one tick.
///
/// Requires the live TMD task and its initialized work block. Each active step
/// decrements the signed-halfword countdown and posts its image when that value
/// becomes negative. Closed and half-open last `blinkFrameDelay + 1` ticks;
/// open ends the sequence without resetting the countdown. Inactive or unknown
/// steps do nothing. Eye pixel storage remains borrowed through the GPU transfer.
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
///
/// `task` must be live with state 0 (initialize), 1 (update) or 2 (exit), and
/// the descriptor-created twenty-part TMD body. The state is not bounds-checked.
/// `spawnArg2.pointer` must hold the task-owned live `Enemy` released on exit.
static void _mistRupertTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistRupertRupertBroderickHurtStates;
    states.funcs[task->state](task);
}

/// Initializes the hurt Rupert actor as a hidden, script-controlled model.
///
/// Requires the descriptor-created TMD body and its task-owned live `Enemy` in
/// `spawnArg2.pointer`. Owns a zeroed work block until
/// task teardown; the model borrows its lighting matrices for that lifetime.
/// Starts with no animation or pending buffer release, installs the message
/// handlers and advances to the update state. Allocation failure exits the task.
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
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, INITIAL_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    _mistRupertInitLighting(task);
    task->msgTable     = _gMistRupertMessages;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Updates the hurt Rupert actor's animation, shadow, lighting, eyes and pending buffer release.
///
/// Requires the live twenty-part TMD body and initialized work; animation slots
/// 1..19 must be seeded once playback starts. Lighting refreshes while the view
/// is ready, independently of visibility. A release countdown of 2 frees the
/// primitive buffer on the third update, then becomes inactive at -1.
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
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        partCoords                 = task->extra.tmd->coords;
        partCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&partCoords[1]);
        worldCoordSetModelLighting(model, partCoords[1].workm.t, 0, ARRAY_SIZE(work->light.m));
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

/// Binds the actor's lighting matrices and samples room lighting at model part 1.
///
/// Requires the initialized work and twenty-part model. The model borrows both
/// matrices until teardown. Refreshes part 1's composed transform before passing
/// its three translation words to the lighting query; no shadow is drawn here.
static void _mistRupertInitLighting(Task* task)
{
    _Work*     work;
    GfxCoord*  partCoords;
    TmdObject* model;

    work                       = task->work;
    model                      = task->extra.tmd;
    partCoords                 = model->coords;
    model->lightMtx            = &work->light;
    model->colorMtx            = &work->color;
    partCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&partCoords[1]);
    worldCoordSetModelLighting(model, partCoords[1].workm.t, 0, ARRAY_SIZE(work->light.m));
}

/// Starts a requested clip on the hurt Rupert actor's nineteen animated parts.
///
/// Requires initialized work and a request readable through this call, with
/// `source.index` 0 and `animationId` 1..8 for this package's loaded bank.
/// Zero blend resets slots; any nonzero blend uses six normal-rate frames,
/// ignoring `blendFrames` and collision settings. Blending requires previously
/// seeded slots. Every request advances each slot once immediately, enables
/// subsequent ticking and leaves root slot zero alone. The request is not
/// retained; bank data remains borrowed during playback. The message ID and
/// second payload are ignored. Returns 0; indices are not bounds-checked.
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
///
/// Requires its live TMD body and initialized work. Mode 0 hides and enables
/// automatic buffer recovery; 1 shows, attempts missing-buffer allocation and
/// enables recovery; 2 hides, disables recovery and schedules release on the
/// third update; 3 shows with recovery disabled. Other flags are preserved.
/// Showing does not cancel a pending release. The message ID and second payload
/// are ignored. Returns 0 for modes 0..3, even if allocation fails; otherwise 1.
static s32 _mistRupertSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
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
        case DRAW_SHOW_SKIP_AUTO_BUFFER:
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
///
/// Requires its live TMD body and initialized work. Modes 0 and 2 post open
/// eyes, 1 closed eyes, and 3 half-open eyes followed by closed, half-open and
/// open on subsequent updates. Closed and the following half-open each last two ticks.
/// Starting a blink retains the current countdown; setting a static image does
/// not cancel a blink. Pixel data stays borrowed until the GPU transfer finishes.
/// The message ID and second payload are ignored. Returns the upload result (0
/// for these present lists); unknown modes change nothing and also return 0.
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
