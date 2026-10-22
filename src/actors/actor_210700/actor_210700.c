#include "actors/actor_210700.h"
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

/// `_Actor210700Work::blinkStep`: the eye image the blink posts next.
///
/// A blink request posts the half-open eyes itself, so the three steps that
/// follow close the eyes and open them again.
enum {
    ACTOR_210700_BLINK_NONE   = 0, // No blink in progress
    ACTOR_210700_BLINK_CLOSED = 1, // The closed eyes are posted next
    ACTOR_210700_BLINK_HALF   = 2, // The half-open eyes are posted next
    ACTOR_210700_BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Eye-texture command accepted by this actor's message table.
enum { ACTOR_210700_MESSAGE_SET_EYES = 0x7E0 };

/// Ground-shadow half extent in world-coordinate units.
enum { ACTOR_210700_GROUND_SHADOW_HALF_SIZE = 0x400 };

/// No primitive-buffer release is pending in the actor's signed tick counter.
enum { ACTOR_210700_BUFFER_FREE_NONE = -1 };

/// Work block of Rupert Broderick's body, the package's one actor.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the twenty-part rig and what the rig
/// plays, the same values an `ActorModelState` holds but kept as words; the
/// model object borrows `light` and `color` for as long as the block lives.
/// The tail is the blink that swaps the eye texture of the face and the
/// delayed free of the model's buffers once the model has been hidden.
///
/// Nothing in the package reads or writes `unknown_4C0`; the block is
/// allocated at its full size, so the bytes are its own, but nothing shows
/// what they hold.
typedef struct {
    ActorAnimRig20 rig;               // Playback storage of the body model; slots 1 to 19 are driven
    s32            ticking;           // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s32            animId;            // Clip the slots were last seeded with, within `bank` (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first)
    s32            bank;              // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first)
    MATRIX         light;             // Light-direction matrix lent to the model object
    MATRIX         color;             // Light-colour matrix lent to the model object
    byte           unknown_4C0[0x78]; // Never accessed
    s16            blinkFrameDelay;   // Value `blinkCountdown` restarts from after the closed and the half-open eyes: each is shown for this many ticks plus one
    s16            blinkCountdown;    // Ticks left before the blink posts its next eye image, which the tick taking it below 0 does; not reset as a blink starts or ends
    s16            blinkStep;         // Eye image the blink posts next (0 `ACTOR_210700_BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            freeCountdown;     // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor210700Work;
STATIC_ASSERT_SIZEOF(_Actor210700Work, 0x540);

static void _actor210700UpdateBlink(Task* task);
static void _actor210700Task(Task* task);
static void _actor210700Init(Task* task);
static void _actor210700Update(Task* task);
static void _actor210700Exit(Task* task);
static void _actor210700BindLighting(Task* task);
static s32  _actor210700PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32  _actor210700Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg);
static s32  _actor210700SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg);
static s32  _actor210700SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedSecondArg);

/// Task-state callbacks for Rupert Broderick's script-controlled body.
///
/// `Task::state` selects 0 initialize, 1 frame update or 2 enemy teardown.
/// All three entries take the live task; there is no terminator or bounds check.
static const TaskFuncTable3 _gActor210700RupertBroderickStates = { {
    _actor210700Init,
    _actor210700Update,
    _actor210700Exit,
} };

static AnimationSet _gAkropolisSanctuaryRupertAmbushedClip;
static AnimationSet _gAkropolisSanctuaryRupertArrival3Clip;
static AnimationSet _gAkropolisSanctuaryRupertHurtIdleClip;
static AnimationSet _gAkropolisSanctuaryRupertArrival11Clip;
static AnimationSet _gAkropolisSanctuaryRupertTalkClip;
static AnimationSet _gAkropolisSanctuaryRupertTalkEndClip;
static TmdSource    _gRupertBody;

#include "assets/rooms/akropolis/sanctuary/rupert_clips_1.inc"

#include "assets/characters/rupert/body.inc"

#include "assets/rooms/akropolis/sanctuary/rupert_clips_2.inc"

/// Packed open-eye texture pixels for the actor's face band.
///
/// 24 VRAM halfwords per row by 16 rows (192 packed 32-bit words).
/// The upload list borrows this word-aligned storage until GPU transfer completion.
static u_long _gActor210700OpenEyesPixels[192] = {
#include "assets/characters/rupert/eyes_open.inc"
};

/// Model-relative eye-texture location: Y/height count rows, width counts VRAM words.
enum {
    ACTOR_210700_EYE_TEXTURE_Y_ROWS      = 40,
    ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS = 24,
    ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS = 16,
};

/// Writable COPY/END list for the open-eye texture band.
///
/// One copy covers 24 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gActor210700OpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS, ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS }, _gActor210700OpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed half-open-eye texture pixels for the actor's face band.
///
/// 24 VRAM halfwords per row by 16 rows (192 packed 32-bit words).
/// The upload list borrows this word-aligned storage until GPU transfer completion.
static u_long _gActor210700HalfOpenEyesPixels[192] = {
#include "assets/characters/rupert/eyes_half_open.inc"
};

/// Writable COPY/END list for the half-open-eye texture band.
///
/// One copy covers 24 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gActor210700HalfOpenEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS, ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS }, _gActor210700HalfOpenEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed closed-eye texture pixels for the actor's face band.
///
/// 24 VRAM halfwords per row by 16 rows (192 packed 32-bit words).
/// The upload list borrows this word-aligned storage until GPU transfer completion.
static u_long _gActor210700ClosedEyesPixels[192] = {
#include "assets/characters/rupert/eyes_closed.inc"
};

/// Writable COPY/END list for the closed-eye texture band.
///
/// One copy covers 24 VRAM halfwords by 16 rows. Actor uploads rewrite
/// its destination relative to the model texture; pixel storage is borrowed
/// until the queued GPU transfer completes. The list lives with the package.
static GpuImageUpload _gActor210700ClosedEyesUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS, ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS }, _gActor210700ClosedEyesPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Loaded animation sets for bank 0: clip IDs 1..6, with NULL at 0.
///
/// Playback borrows this table and the package's clip descriptors; requests
/// must select a loaded clip and keep the package live until teardown.
#include "assets/rooms/akropolis/sanctuary/rupert_bank.inc"

/// Indexed animation banks; `AnimationPlayRequest.source.index` must be 0.
///
/// A bank change binds its borrowed clip table into the live animation rig.
/// The table and clips remain loaded for the duration of playback.
static AnimationSet** _gActor210700AnimationBanks[1] = {
    _gActor210700AnimationSets,
};

enum { ACTOR_210700_RUPERT_TASK_PRIORITY = 192 };

TaskDesc gActor210700RupertBroderickTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_210700_RUPERT_TASK_PRIORITY } },
    .callback = _actor210700Task,
    .data     = { .model = &_gRupertBody },
};

/// Rupert message dispatch: animation, placement, drawing and eye images.
///
/// The spawned actor borrows this terminated table and its callbacks until teardown.
/// Eye mode 0/2 opens the eyes, 1 closes them, and 3 starts a blink.
static TaskMessageEntry _gActor210700Messages[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor210700PlayAnimation },
    { ACTOR_MESSAGE_PLACE, _actor210700Place },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor210700SetModelDraw },
    { ACTOR_210700_MESSAGE_SET_EYES, _actor210700SetEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Fills the model-relative rectangle for Rupert's eye-texture replacement.
///
/// Requires a writable RECT lvalue without evaluation side effects, evaluated
/// four times. X uses two positions per VRAM word, width uses VRAM words and
/// Y/height use rows. The uploader applies the actor's texture-page displacement.
/// Captures no caller locals and retains no address.
/// Statement-only GNU expression; its value is not part of the interface.
#define ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect)         \
    ({                                                      \
        (eyeRect).x = 0;                                    \
        (eyeRect).y = ACTOR_210700_EYE_TEXTURE_Y_ROWS;      \
        (eyeRect).w = ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS; \
        (eyeRect).h = ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS; \
    })

/// Projects model part 1 onto the ground and draws its shadow when a floor is found.
///
/// Requires a side-effect-free live TMD task pointer with part 1's cached
/// transform, and a stable writable VECTOR3 lvalue. The task is evaluated once;
/// the point once on a miss and twice on a hit. Uses the current global room
/// shadow shade; neither argument's address is retained.
#define ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint)                                                                             \
    do {                                                                                                                               \
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&(task)->extra.tmd->coords[1].workm), &(groundPoint)) != 0) { \
            effectDrawGroundShadow(&(groundPoint), ACTOR_210700_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);         \
        }                                                                                                                              \
    } while (0)

/// Ticks the body's non-root animation tracks and applies their poses.
///
/// `rig->anim` must bind `rig->slots` and `rig->poses` to a live twenty-part
/// model, with slots 1..19 initialized for the same-numbered tracks and
/// coordinates. The rig storage, model and loaded clip data remain borrowed
/// throughout playback. Rates count sixteenths of a frame, subject to death
/// playback and boundary holds. Bounds, scratch-stack capacity and GTE
/// requirements follow `animationTickSlot`. Slot 0 retains the root placement.
static inline void _actor210700TickAnimationSlots(ActorAnimRig20* rig)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(rig->slots); slotIndex++) {
        animationTickSlot(&rig->anim, slotIndex);
    }
}

/// Advances the pending blink through closed, half-open and open eye textures.
///
/// Requires the actor's live work block and TMD body. Each active step decrements
/// the signed 16-bit countdown, uploading when its stored value becomes negative.
/// Closed and half-open eyes last `blinkFrameDelay + 1` ticks; the open step ends
/// the blink without resetting the countdown. Texture pixels remain borrowed by
/// the GPU queue after each upload.
static void _actor210700UpdateBlink(Task* task)
{
    _Actor210700Work* work;
    RECT              eyeRect;

    work = task->work;
    ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect);

    switch (work->blinkStep) {
        case ACTOR_210700_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &_gActor210700ClosedEyesUploads[0], &eyeRect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &_gActor210700HalfOpenEyesUploads[0], &eyeRect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &_gActor210700OpenEyesUploads[0], &eyeRect);
                work->blinkStep = ACTOR_210700_BLINK_NONE;
            }
            break;
    }
}

/// Dispatches Rupert Broderick's body task to initialization, update or teardown.
///
/// `task->state` must be 0 (initialize), 1 (update) or 2 (exit). The task owns a
/// twenty-part TMD body and carries its scene-owned `Enemy` in `spawnArg2`.
/// State zero allocates the work block; exit may release the task's resources.
static void _actor210700Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor210700RupertBroderickStates;
    stateHandlers.funcs[task->state](task);
}

/// Initializes the body at the origin with its default animation and message handlers.
///
/// Requires a live twenty-part TMD task with a scene-owned `Enemy` in `spawnArg2`.
/// Owns a zeroed primary-heap work block until task teardown; allocation failure
/// starts teardown immediately. Successful initialization advances to state 1.
static void _actor210700Init(Task* task)
{
    enum {
        ACTOR_210700_INITIAL_BANK      = 0,
        ACTOR_210700_INITIAL_ANIMATION = 1,
    };

    _Actor210700Work*    work;
    TmdObject*           model;
    ActorTransform       initialPlacement;
    AnimationPlayRequest initialAnimation;
    VECTOR3              groundPoint;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank          = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown = ACTOR_210700_BUFFER_FREE_NONE;
    model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;

    // Placement enables drawing; seed only the request components the handlers read.
    initialPlacement.pos.vx = 0;
    initialPlacement.pos.vy = 0;
    initialPlacement.pos.vz = 0;
    initialPlacement.rot.vx = 0;
    initialPlacement.rot.vy = 0;
    initialPlacement.rot.vz = 0;
    _actor210700Place(task, ACTOR_MESSAGE_PLACE, &initialPlacement, 0);
    initialAnimation.source.index = ACTOR_210700_INITIAL_BANK;
    initialAnimation.animationId  = ACTOR_210700_INITIAL_ANIMATION;
    initialAnimation.blend        = ANIMATION_BLEND_RESET;
    _actor210700PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &initialAnimation, 0);
    ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint);
    _actor210700BindLighting(task);
    task->msgTable     = _gActor210700Messages;
    task->exitCallback = _actor210700Exit;
    task->state++;
}

/// Updates body animation, ground shadow, room lighting, blinking and buffer release.
///
/// Requires successful initialization. Advances animation slots 1..19 once per
/// call when enabled. Lighting is refreshed only with a ready view. A pending
/// buffer release occurs on the call entering with countdown zero, after which
/// the counter becomes the inactive sentinel.
static void _actor210700Update(Task* task)
{
    _Actor210700Work* work;
    TmdObject*        model;
    VECTOR3           groundPoint;

    work  = task->work;
    model = task->extra.tmd;
    if (work->ticking != 0) {
        _actor210700TickAnimationSlots(&work->rig);
    }
    ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint);
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ARRAY_SIZE(work->light.m));
    }
    _actor210700UpdateBlink(task);

    // Release only when the countdown was already zero at entry.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Releases the actor's target-tracking object and starts task teardown.
///
/// Used by task state 2 and the exit callback. Requires the live `Enemy` in
/// `spawnArg2`; task teardown releases the work block and schedules body release.
static void _actor210700Exit(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the model to the actor's private light-direction and light-colour matrices.
///
/// Requires initialized work and a live TMD body. Both matrix pointers borrow
/// the work block and remain valid until task teardown releases it.
static void _actor210700BindLighting(Task* task)
{
    TmdObject*        model;
    _Actor210700Work* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

/// Selects a body animation, blending changed clips over six normal-rate frames.
///
/// Requires initialized work, the twenty-part model and a request readable during
/// dispatch. `source.index` must be 0 and `animationId` must select a loaded clip
/// 1..6. Drives slots 1..19, retaining the root placement. A changed bank rebinds
/// playback and forces clip selection; the same bank and clip do not restart.
/// Nonzero `blend` uses six frames, ignoring `blendFrames`; zero resets each slot.
/// Blending requires previously initialized slots; the first selection from
/// zeroed work must reset them.
/// The request is not retained, but animation data remain borrowed during playback.
/// The message ID, second argument and `enableWorldCollision` are ignored. Returns 0.
static s32 _actor210700PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { ACTOR_210700_ANIMATION_BLEND_FRAMES = 6 };

    _Actor210700Work* work;
    s32               slotIndex;
    TmdObject*        model;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor210700AnimationBanks[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    if (request->animationId != work->animId) {
        work->animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_210700_ANIMATION_BLEND_FRAMES);
            }
        } else {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->animId);
            }
        }
        // Apply the first pose immediately before enabling subsequent frame updates.
        _actor210700TickAnimationSlots(&work->rig);
        work->ticking = 1;
    }
    return 0;
}

/// Places the model root, records its Euler angles and enables active drawing.
///
/// Requires a live TMD body and a placement readable during dispatch. Position
/// uses the root parent's coordinate frame; angles use 4096 units per turn in
/// SDK `RotMatrix` order. Only X/Y/Z components are read, and no payload pointer
/// is retained. Invalidates the composed transform without allocating a buffer.
/// The message ID and second argument are ignored. Returns 0.
static s32 _actor210700Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    GfxCoord*  rootCoord;
    TmdObject* model;

    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags           &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

/// Sets active visibility and automatic-buffer policy, optionally scheduling release.
///
/// Requires initialized work and a live TMD body. Modes: 0 hide with automatic
/// allocation enabled; 1 show, explicitly allocate if absent and enable automatic
/// allocation; 2 hide, disable automatic allocation and release after two
/// intervening update calls; 3 show with automatic allocation disabled. Other
/// model flags and any already pending release are retained in modes 0, 1 and 3.
/// Allocation failure is ignored. The message ID and second argument are ignored.
/// Returns 0 for modes 0..3, or 1 without changes for other values.
static s32 _actor210700SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_210700_MODEL_DRAW_HIDE_AUTO     = 0,
        ACTOR_210700_MODEL_DRAW_SHOW_ALLOCATE = 1,
        ACTOR_210700_MODEL_DRAW_HIDE_RELEASE  = 2,
        ACTOR_210700_MODEL_DRAW_SHOW_KEEP     = 3,
    };

    TmdObject*        model;
    _Actor210700Work* work;
    s32               invalidMode;

    model       = task->extra.tmd;
    work        = task->work;
    invalidMode = 0;

    switch (drawMode) {
        case ACTOR_210700_MODEL_DRAW_HIDE_AUTO:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_SHOW_ALLOCATE:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_HIDE_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            // This mode value also supplies the two-tick release countdown.
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_SHOW_KEEP:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            invalidMode = 1;
            break;
    }
    return invalidMode;
}

/// Selects an eye texture or starts the half-open/closed/half-open/open blink.
///
/// Requires initialized work and a live TMD body. Modes 0 and 2 upload open eyes,
/// 1 uploads closed eyes, and 3 uploads half-open eyes and starts a blink with
/// two ticks each for the subsequent closed and half-open images. The existing
/// countdown is retained; immediate image commands do not cancel a pending blink.
/// The message ID and second argument are ignored. Returns the texture uploader's
/// result for a handled mode, or 0 without an upload for other values. Static
/// texture pixels remain live while the GPU queue borrows them.
static s32 _actor210700SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_210700_EYES_OPEN           = 0,
        ACTOR_210700_EYES_CLOSED         = 1,
        ACTOR_210700_EYES_OPEN_ALTERNATE = 2,
        ACTOR_210700_EYES_BLINK          = 3,
        ACTOR_210700_BLINK_FRAME_DELAY   = 1,
    };

    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             uploadResult;

    uploadResult = 0;
    ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect);

    switch (eyeMode) {
        case ACTOR_210700_EYES_CLOSED:
            uploadList = &_gActor210700ClosedEyesUploads[0];
            break;
        case ACTOR_210700_EYES_OPEN:
        case ACTOR_210700_EYES_OPEN_ALTERNATE:
            uploadList = &_gActor210700OpenEyesUploads[0];
            break;
        case ACTOR_210700_EYES_BLINK:
            ((_Actor210700Work*)task->work)->blinkStep       = ACTOR_210700_BLINK_CLOSED;
            ((_Actor210700Work*)task->work)->blinkFrameDelay = ACTOR_210700_BLINK_FRAME_DELAY;
            uploadList                                       = &_gActor210700HalfOpenEyesUploads[0];
            break;
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        uploadResult = actorRenderUploadTexture(task, uploadList, &eyeRect);
    }
    return uploadResult;
}

#undef ACTOR_210700_DRAW_GROUND_SHADOW
#undef ACTOR_210700_INIT_EYE_TEXTURE_RECT
