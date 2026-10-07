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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

extern GpuImageUpload D_actor_113000_8013AB6C[2];

/// Animation source table `_actor113000PlayAnimation` indexes by the preset's
/// bank index and hands `animationInitContext` as its data argument.
extern AnimationSet*  D_actor_113000_8013AB8C[9];
extern AnimationSet** D_actor_113000_8013ABB0[1];

/// Eye-band destination within the actor texture: width in VRAM words, Y/height in rows.
enum {
    ACTOR_113000_EYES_Y_ROWS      = 40,
    ACTOR_113000_EYES_WIDTH_WORDS = 32,
    ACTOR_113000_EYES_HEIGHT_ROWS = 16,
};

/// `_Actor113000Work::blinkStep`: the eye image the blink posts next.
///
/// A blink posts the closed, half-open and open eyes in turn, so the eyes are
/// seen opening. The request that starts one posts the half-open eyes itself,
/// which is the closing half of the blink.
enum {
    ACTOR_113000_BLINK_NONE   = 0, // No blink in progress
    ACTOR_113000_BLINK_CLOSED = 1, // The closed eyes are posted next
    ACTOR_113000_BLINK_HALF   = 2, // The half-open eyes are posted next
    ACTOR_113000_BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of the package's one actor, the hurt Rupert Broderick body a
/// room script places and plays clips on.
///
/// The task's spawn state allocates it zeroed at its full size and keeps it at
/// `Task::work` for the task's life. It opens with the rig of the twenty-part
/// body model and what that rig is playing, kept as words: a play request
/// rebinds the rig when its bank differs from `bank`, seeds the slots with its
/// clip and records it in `animId`. The spawn sets both ids to
/// `ACTOR_MODEL_STATE_NONE`, so the first request always binds its bank. The
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
    s16            blinkStep;       // Eye image the blink posts next (0 `ACTOR_113000_BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            field_4C6;       // Zeroed again by the spawn and never read; role unproven
    s16            freeCountdown;   // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor113000Work;
STATIC_ASSERT_SIZEOF(_Actor113000Work, 0x4CC);

/// Message dispatch table the spawn handler parks in `Task::msgTable`:
/// message id / handler pairs, terminated by `TASK_MESSAGE_TABLE_END` and a null word.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_113000_8013ABC0[5];

static void _actor113000Spawn(Task* task);
static void _actor113000Update(Task* task);
static void _actor113000InitLighting(Task* task);

/// The actor's three task states, which `_actor113000Task` runs by
/// `Task::state`: spawn, per-frame tick and exit.
static const TaskFuncTable3 D_actor_113000_80131E24 = { {
    _actor113000Spawn,
    _actor113000Update,
    enemyTaskExit,
} };

static AnimationSet _gActor113000Animation05D94;
static AnimationSet _gActor113000Animation061F4;
static AnimationSet _gActor113000Animation0657C;
static AnimationSet _gActor113000Animation06D10;
static AnimationSet _gActor113000Animation073A8;
static AnimationSet _gActor113000Animation078B8;
static AnimationSet _gActor113000Animation07E6C;
static AnimationSet _gActor113000Animation080E4;
static TmdSource    _gActor113000RupertBroderickHurtBody;
static s32          _actor113000PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32          _actor113000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32          _actor113000SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg);
static void         _actor113000Task(Task* task);

static TmdBone _gActor113000RupertBroderickHurtBodySkeleton[20] = {
#include "assets/rupert_broderick_hurt_body_skeleton.inc"
};

static u32 _gActor113000RupertBroderickHurtBodyPartVerts[20] = {
#include "assets/rupert_broderick_hurt_body_partVerts.inc"
};

static SVECTOR _gActor113000RupertBroderickHurtBodyVerts[343] = {
#include "assets/rupert_broderick_hurt_body_verts.inc"
};

static SVECTOR _gActor113000RupertBroderickHurtBodyNormals[334] = {
#include "assets/rupert_broderick_hurt_body_normals.inc"
};

static u32 _gActor113000RupertBroderickHurtBodyStream[3801] = {
#include "assets/rupert_broderick_hurt_body_stream.inc"
};

static TmdSource _gActor113000RupertBroderickHurtBody = {
    0,
    20988,
    5348,
    20,
    _gActor113000RupertBroderickHurtBodyPartVerts,
    _gActor113000RupertBroderickHurtBodyVerts,
    _gActor113000RupertBroderickHurtBodyNormals,
    _gActor113000RupertBroderickHurtBodySkeleton,
    _gActor113000RupertBroderickHurtBodyStream,
};

static AnimationPackedPose _gActor113000Animation05D94Bank1[3] = {
#include "assets/actor_113000_animation_05D94_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation05D94Bank4[27] = {
#include "assets/actor_113000_animation_05D94_bank4.inc"
};

static AnimationRecord _gActor113000Animation05D94Records[126] = {
#include "assets/actor_113000_animation_05D94_records.inc"
};

static u16 _gActor113000Animation05D94Indices[20] = {
#include "assets/actor_113000_animation_05D94_indices.inc"
};

static AnimationSet _gActor113000Animation05D94 = {
    _gActor113000Animation05D94Records,
    _gActor113000Animation05D94Indices,
    { NULL, _gActor113000Animation05D94Bank1, NULL, NULL, _gActor113000Animation05D94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation061F4Bank1[3] = {
#include "assets/actor_113000_animation_061F4_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation061F4Bank4[27] = {
#include "assets/actor_113000_animation_061F4_bank4.inc"
};

static AnimationRecord _gActor113000Animation061F4Records[224] = {
#include "assets/actor_113000_animation_061F4_records.inc"
};

static u16 _gActor113000Animation061F4Indices[20] = {
#include "assets/actor_113000_animation_061F4_indices.inc"
};

static AnimationSet _gActor113000Animation061F4 = {
    _gActor113000Animation061F4Records,
    _gActor113000Animation061F4Indices,
    { NULL, _gActor113000Animation061F4Bank1, NULL, NULL, _gActor113000Animation061F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation0657CBank1[3] = {
#include "assets/actor_113000_animation_0657C_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation0657CBank4[31] = {
#include "assets/actor_113000_animation_0657C_bank4.inc"
};

static AnimationRecord _gActor113000Animation0657CRecords[166] = {
#include "assets/actor_113000_animation_0657C_records.inc"
};

static u16 _gActor113000Animation0657CIndices[20] = {
#include "assets/actor_113000_animation_0657C_indices.inc"
};

static AnimationSet _gActor113000Animation0657C = {
    _gActor113000Animation0657CRecords,
    _gActor113000Animation0657CIndices,
    { NULL, _gActor113000Animation0657CBank1, NULL, NULL, _gActor113000Animation0657CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation06D10Bank1[7] = {
#include "assets/actor_113000_animation_06D10_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation06D10Bank4[186] = {
#include "assets/actor_113000_animation_06D10_bank4.inc"
};

static AnimationRecord _gActor113000Animation06D10Records[258] = {
#include "assets/actor_113000_animation_06D10_records.inc"
};

static u16 _gActor113000Animation06D10Indices[20] = {
#include "assets/actor_113000_animation_06D10_indices.inc"
};

static AnimationSet _gActor113000Animation06D10 = {
    _gActor113000Animation06D10Records,
    _gActor113000Animation06D10Indices,
    { NULL, _gActor113000Animation06D10Bank1, NULL, NULL, _gActor113000Animation06D10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation073A8Bank1[24] = {
#include "assets/actor_113000_animation_073A8_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation073A8Bank4[136] = {
#include "assets/actor_113000_animation_073A8_bank4.inc"
};

static AnimationRecord _gActor113000Animation073A8Records[194] = {
#include "assets/actor_113000_animation_073A8_records.inc"
};

static u16 _gActor113000Animation073A8Indices[20] = {
#include "assets/actor_113000_animation_073A8_indices.inc"
};

static AnimationSet _gActor113000Animation073A8 = {
    _gActor113000Animation073A8Records,
    _gActor113000Animation073A8Indices,
    { NULL, _gActor113000Animation073A8Bank1, NULL, NULL, _gActor113000Animation073A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation078B8Bank1[7] = {
#include "assets/actor_113000_animation_078B8_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation078B8Bank4[108] = {
#include "assets/actor_113000_animation_078B8_bank4.inc"
};

static AnimationRecord _gActor113000Animation078B8Records[175] = {
#include "assets/actor_113000_animation_078B8_records.inc"
};

static u16 _gActor113000Animation078B8Indices[20] = {
#include "assets/actor_113000_animation_078B8_indices.inc"
};

static AnimationSet _gActor113000Animation078B8 = {
    _gActor113000Animation078B8Records,
    _gActor113000Animation078B8Indices,
    { NULL, _gActor113000Animation078B8Bank1, NULL, NULL, _gActor113000Animation078B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation07E6CBank1[15] = {
#include "assets/actor_113000_animation_07E6C_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation07E6CBank4[120] = {
#include "assets/actor_113000_animation_07E6C_bank4.inc"
};

static AnimationRecord _gActor113000Animation07E6CRecords[180] = {
#include "assets/actor_113000_animation_07E6C_records.inc"
};

static u16 _gActor113000Animation07E6CIndices[20] = {
#include "assets/actor_113000_animation_07E6C_indices.inc"
};

static AnimationSet _gActor113000Animation07E6C = {
    _gActor113000Animation07E6CRecords,
    _gActor113000Animation07E6CIndices,
    { NULL, _gActor113000Animation07E6CBank1, NULL, NULL, _gActor113000Animation07E6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113000Animation080E4Bank1[3] = {
#include "assets/actor_113000_animation_080E4_bank1.inc"
};

static AnimationPackedRotation _gActor113000Animation080E4Bank4[31] = {
#include "assets/actor_113000_animation_080E4_bank4.inc"
};

static AnimationRecord _gActor113000Animation080E4Records[98] = {
#include "assets/actor_113000_animation_080E4_records.inc"
};

static u16 _gActor113000Animation080E4Indices[20] = {
#include "assets/actor_113000_animation_080E4_indices.inc"
};

static AnimationSet _gActor113000Animation080E4 = {
    _gActor113000Animation080E4Records,
    _gActor113000Animation080E4Indices,
    { NULL, _gActor113000Animation080E4Bank1, NULL, NULL, _gActor113000Animation080E4Bank4, NULL, NULL, NULL },
};

u_long D_actor_113000_80139F2C[256] = {
#include "assets/actor_113000_image_0810C.inc"
};

GpuImageUpload D_actor_113000_8013A32C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 32, 16 }, D_actor_113000_80139F2C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_113000_8013A34C[256] = {
#include "assets/actor_113000_image_0852C.inc"
};

GpuImageUpload D_actor_113000_8013A74C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 32, 16 }, D_actor_113000_8013A34C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_113000_8013A76C[256] = {
#include "assets/actor_113000_image_0894C.inc"
};

GpuImageUpload D_actor_113000_8013AB6C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 32, 16 }, D_actor_113000_8013A76C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_113000_8013AB8C[9] = {
    NULL,
    &_gActor113000Animation05D94,
    &_gActor113000Animation061F4,
    &_gActor113000Animation0657C,
    &_gActor113000Animation06D10,
    &_gActor113000Animation073A8,
    &_gActor113000Animation078B8,
    &_gActor113000Animation07E6C,
    &_gActor113000Animation080E4,
};

AnimationSet** D_actor_113000_8013ABB0[1] = {
    D_actor_113000_8013AB8C,
};

TaskDesc D_actor_113000_8013ABB4 = { { { TASK_BODY_TMD, 192 } }, _actor113000Task, { .model = &_gActor113000RupertBroderickHurtBody } };

TaskMessageEntry D_actor_113000_8013ABC0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor113000PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor113000SetModelDraw },
    { 2016, _actor113000SetEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Queues a closed or half-open eye image, restarts its dwell and advances the blink.
///
/// Requires a live TMD `actorTask` and its writable `work`, with `blinkStep`
/// equal to `ACTOR_113000_BLINK_CLOSED` or `ACTOR_113000_BLINK_HALF`. Resets
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
static inline void _actor113000AdvanceBlinkImage(Task* actorTask, _Actor113000Work* work,
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
static void _actor113000TickBlink(Task* task)
{
    _Actor113000Work* work;
    RECT              eyeRect;

    work      = task->work;
    eyeRect.x = 0;
    eyeRect.y = ACTOR_113000_EYES_Y_ROWS;
    eyeRect.w = ACTOR_113000_EYES_WIDTH_WORDS;
    eyeRect.h = ACTOR_113000_EYES_HEIGHT_ROWS;

    switch (work->blinkStep) {
        case ACTOR_113000_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _actor113000AdvanceBlinkImage(task, work, &D_actor_113000_8013AB6C[0], &eyeRect);
            }
            break;
        case ACTOR_113000_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _actor113000AdvanceBlinkImage(task, work, &D_actor_113000_8013A74C[0], &eyeRect);
            }
            break;
        case ACTOR_113000_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &D_actor_113000_8013A32C[0], &eyeRect);
                work->blinkStep = ACTOR_113000_BLINK_NONE;
            }
            break;
    }
}

/// Dispatches the hurt Rupert actor's initialization, update or teardown state.
///
/// `task` must be live with state 0 (initialize), 1 (update) or 2 (exit), and
/// the descriptor-created twenty-part TMD body. The state is not bounds-checked.
/// `spawnArg2.pointer` must hold the task-owned live `Enemy` released on exit.
static void _actor113000Task(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_113000_80131E24;
    states.funcs[task->state](task);
}

/// Initializes the hurt Rupert actor as a hidden, script-controlled model.
///
/// Requires the descriptor-created TMD body and its task-owned live `Enemy` in
/// `spawnArg2.pointer`. Owns a zeroed work block until
/// task teardown; the model borrows its lighting matrices for that lifetime.
/// Starts with no animation or pending buffer release, installs the message
/// handlers and advances to the update state. Allocation failure exits the task.
static void _actor113000Spawn(Task* task)
{
    enum {
        ACTOR_113000_BUFFER_FREE_NONE         = -1,
        ACTOR_113000_INITIAL_SHADOW_HALF_SIZE = 512,
    };
    _Actor113000Work* work;
    TmdObject*        model;
    VECTOR3           groundPoint;
    u16               modelFlags;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MODEL_STATE_NONE;
    work->bank          = ACTOR_MODEL_STATE_NONE;
    work->field_4C6     = 0;
    work->freeCountdown = ACTOR_113000_BUFFER_FREE_NONE;
    modelFlags          = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags        = modelFlags;
    // Retain the shadow branch even though initialization has just hidden the model.
    if (!(modelFlags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_113000_INITIAL_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    _actor113000InitLighting(task);
    task->msgTable     = D_actor_113000_8013ABC0;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Updates the hurt Rupert actor's animation, shadow, lighting, eyes and pending buffer release.
///
/// Requires the live twenty-part TMD body and initialized work; animation slots
/// 1..19 must be seeded once playback starts. Lighting refreshes while the view
/// is ready, independently of visibility. A release countdown of 2 frees the
/// primitive buffer on the third update, then becomes inactive at -1.
static void _actor113000Update(Task* task)
{
    enum { ACTOR_113000_SHADOW_HALF_SIZE = 768 };
    _Actor113000Work* work;
    TmdObject*        model;
    GfxCoord*         partCoords;
    VECTOR3           groundPoint;
    s32               slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (work->ticking != 0) {
        // Slot zero is the script-placed root, outside clip playback.
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_113000_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        partCoords                 = task->extra.tmd->coords;
        partCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&partCoords[1]);
        worldCoordSetModelLighting(model, partCoords[1].workm.t, 0, ARRAY_SIZE(work->light.m));
    }
    _actor113000TickBlink(task);
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
static void _actor113000InitLighting(Task* task)
{
    _Actor113000Work* work;
    GfxCoord*         partCoords;
    TmdObject*        model;

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
static s32 _actor113000PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum { ACTOR_113000_ANIMATION_BLEND_FRAMES = 6 };
    _Actor113000Work* work;
    TmdObject*        model;
    s32               slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->bank) {
        // Bind the bank before selecting a clip, including the first request.
        work->bank   = request->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_113000_8013ABB0[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_113000_ANIMATION_BLEND_FRAMES);
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

#include "../../shared/actor_messages_place_euler.inc.c"

/// Changes the hurt Rupert actor's drawing and primitive-buffer policy.
///
/// Requires its live TMD body and initialized work. Mode 0 hides and enables
/// automatic buffer recovery; 1 shows, attempts missing-buffer allocation and
/// enables recovery; 2 hides, disables recovery and schedules release on the
/// third update; 3 shows with recovery disabled. Other flags are preserved.
/// Showing does not cancel a pending release. The message ID and second payload
/// are ignored. Returns 0 for modes 0..3, even if allocation fails; otherwise 1.
static s32 _actor113000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { ACTOR_113000_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*        model;
    _Actor113000Work* work;
    s32               result;

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
        case ACTOR_113000_DRAW_SHOW_SKIP_AUTO_BUFFER:
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
static s32 _actor113000SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg)
{
    enum {
        ACTOR_113000_EYES_OPEN           = 0,
        ACTOR_113000_EYES_CLOSED         = 1,
        ACTOR_113000_EYES_OPEN_ALTERNATE = 2,
        ACTOR_113000_EYES_BLINK          = 3,
        ACTOR_113000_BLINK_FRAME_DELAY   = 1,
    };
    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             result;

    result    = 0;
    eyeRect.x = 0;
    eyeRect.y = ACTOR_113000_EYES_Y_ROWS;
    eyeRect.w = ACTOR_113000_EYES_WIDTH_WORDS;
    eyeRect.h = ACTOR_113000_EYES_HEIGHT_ROWS;

    switch (eyeMode) {
        case ACTOR_113000_EYES_CLOSED:
            uploadList = &D_actor_113000_8013AB6C[0];
            break;
        case ACTOR_113000_EYES_OPEN:
        case ACTOR_113000_EYES_OPEN_ALTERNATE:
            uploadList = &D_actor_113000_8013A32C[0];
            break;
        case ACTOR_113000_EYES_BLINK: {
            _Actor113000Work* stepWork;
            _Actor113000Work* delayWork;

            stepWork                   = task->work;
            stepWork->blinkStep        = ACTOR_113000_BLINK_CLOSED;
            delayWork                  = task->work;
            delayWork->blinkFrameDelay = ACTOR_113000_BLINK_FRAME_DELAY;
            uploadList                 = &D_actor_113000_8013A74C[0];
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
