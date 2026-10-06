#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Steps of the palette greying, kept in `_Actor311900Work::clutGreyStep`.
///
/// A figure's tick performs one step a frame and moves on to the next, so the
/// read-back, the conversion and the upload fall on three successive frames.
enum {
    ACTOR_311900_CLUT_GREY_READ    = 0, // Read the figure's two CLUT rows back from VRAM
    ACTOR_311900_CLUT_GREY_CONVERT = 1, // Raise the three channels of every entry to its brightest one
    ACTOR_311900_CLUT_GREY_UPLOAD  = 2, // Write the two rows back to VRAM
    ACTOR_311900_CLUT_GREY_DONE    = 3, // Greyed; the tick leaves the rows alone
};

/// Ticks the advancing figure moves forward for, counted in
/// `_Actor311900Work::advanceFrames`, before its scene is flagged done and its
/// task moves on to its teardown.
enum { ACTOR_311900_ADVANCE_FRAMES = 0x5A };

/// Mapped camera indices and the seen bit for the security-monitor figures.
enum {
    ACTOR_311900_RUPERT_VIEW = 0xA,
    ACTOR_311900_SWAT_VIEW   = 0xB,
    ACTOR_311900_RUPERT_SEEN = 1 << 1,
};

/// Both figures play the only populated entry of their animation set tables.
enum { ACTOR_311900_ANIMATION_SET = 1 };

/// Work block of either figure the package shows, allocated zeroed at its full
/// size by the figure's spawn state and kept at `Task::work` for the task's
/// life.
///
/// It opens with the rig of the figure's twenty-part body model and the
/// animation request its update serves, in the steps an enemy's animation
/// takes (`ACTOR_ENEMY_ANIM_BLEND`, `_RESET`, `_TICK`). Each spawn requests
/// clip 1 from its start and nothing requests another, so the blended step is
/// served but never asked for. The model object borrows `light` and `color`
/// for as long as the block lives.
///
/// What follows the matrices is the figures' own. Each is drawn only while its
/// own view of the room is up and greys its texture palette over its first
/// three ticks. The figure of the package's first state table also moves
/// forward along its own Z axis once its view has been shown, and ends its
/// scene after `ACTOR_311900_ADVANCE_FRAMES` ticks of that; the other figure
/// leaves `advanceFrames` and `advancing` zero.
typedef struct {
    ActorAnimRig20 rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s16            animState;     // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16            appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16            animId;        // Clip the next reseed selects, an index into the figure's animation set table
    u16            animFrames;    // Updates since the last reseed; counted, never read
    u8             animRate;      // Rate given every slot ahead of a reseed, in sixteenths of a frame; nothing sets it, so it is 0, which a reseed from the clip's start replaces with `ANIMATION_RATE_ONE`
    byte           pad_47D[0x7];  // Never accessed
    MATRIX         light;         // Light-direction matrix lent to the model object
    MATRIX         color;         // Light-colour matrix lent to the model object
    s16            advanceFrames; // Ticks the figure has moved forward for; reaching `ACTOR_311900_ADVANCE_FRAMES` ends its scene
    s16            advancing;     // Set once the figure's view has been shown, never cleared (0 standing where it was placed, 1 moving forward every tick)
    u8             clutGreyStep;  // Step of the palette greying (`ACTOR_311900_CLUT_GREY_*`)
} _Actor311900Work;
STATIC_ASSERT_SIZEOF(_Actor311900Work, 0x4CC);

/// The animation data `animationInitContext` builds the first setup path's clip
/// context from; the spawn hands it over whole, so it is only ever a byte
/// address here.
extern u8 D_actor_311900_8016EBE8[];

/// The animation data the second setup path builds its clip context from.
extern u8 D_actor_311900_8016EBF4[];

/// The palette rows `_actor311900TickPaletteGreying` reads back, greys and uploads.
extern u16 D_actor_311900_8016EC18[][0x100];

static void _actor311900InitRupert(Enemy* enemy, Task* task);
static void _actor311900UpdateRupert(Enemy* enemy, Task* task);
static void _actor311900InitSwat(Enemy* enemy, Task* task);
static void _actor311900UpdateSwat(Enemy* enemy, Task* task);
static s32  _actorMovementStepForwardNonzero(GfxCoord* coord, s16 stepDistance);
static void _actor311900InitRupertLighting(Task* task);
static void _actor311900InitSwatLighting(Task* task);
static void _actor311900TickPaletteGreying(Task* task, s32 clutRowOffset, s16 firstPaletteRow);
static void _actor311900UpdateAnimation(Task* task);

static TmdSource _gActor311900RupertBroderickBody1;
static TmdSource _gActor311900SwatMember2Body;
static void      _actor311900RupertTask(Task* task);
static void      _actor311900SwatTask(Task* task);

static TmdBone _gActor311900RupertBroderickBody1Skeleton[20] = {
#include "assets/rupert_broderick_body_1_skeleton.inc"
};

static u32 _gActor311900RupertBroderickBody1PartVerts[20] = {
#include "assets/rupert_broderick_body_1_partVerts.inc"
};

static SVECTOR _gActor311900RupertBroderickBody1Verts[386] = {
#include "assets/rupert_broderick_body_1_verts.inc"
};

static SVECTOR _gActor311900RupertBroderickBody1Normals[385] = {
#include "assets/rupert_broderick_body_1_normals.inc"
};

static u32 _gActor311900RupertBroderickBody1Stream[4327] = {
#include "assets/rupert_broderick_body_1_stream.inc"
};

static TmdSource _gActor311900RupertBroderickBody1 = {
    0,
    23980,
    6012,
    20,
    _gActor311900RupertBroderickBody1PartVerts,
    _gActor311900RupertBroderickBody1Verts,
    _gActor311900RupertBroderickBody1Normals,
    _gActor311900RupertBroderickBody1Skeleton,
    _gActor311900RupertBroderickBody1Stream,
};

static TmdBone _gActor311900SwatMember2BodySkeleton[20] = {
#include "assets/swat_member_2_body_skeleton.inc"
};

static u32 _gActor311900SwatMember2BodyPartVerts[20] = {
#include "assets/swat_member_2_body_partVerts.inc"
};

static SVECTOR _gActor311900SwatMember2BodyVerts[360] = {
#include "assets/swat_member_2_body_verts.inc"
};

static SVECTOR _gActor311900SwatMember2BodyNormals[358] = {
#include "assets/swat_member_2_body_normals.inc"
};

static u32 _gActor311900SwatMember2BodyStream[3973] = {
#include "assets/swat_member_2_body_stream.inc"
};

static TmdSource _gActor311900SwatMember2Body = {
    0,
    21176,
    6792,
    20,
    _gActor311900SwatMember2BodyPartVerts,
    _gActor311900SwatMember2BodyVerts,
    _gActor311900SwatMember2BodyNormals,
    _gActor311900SwatMember2BodySkeleton,
    _gActor311900SwatMember2BodyStream,
};

static AnimationPackedPose _gActor311900Animation0C9A8Bank1[11] = {
#include "assets/actor_311900_animation_0C9A8_bank1.inc"
};

static AnimationPackedRotation _gActor311900Animation0C9A8Bank4[201] = {
#include "assets/actor_311900_animation_0C9A8_bank4.inc"
};

static AnimationRecord _gActor311900Animation0C9A8Records[290] = {
#include "assets/actor_311900_animation_0C9A8_records.inc"
};

static u16 _gActor311900Animation0C9A8Indices[20] = {
#include "assets/actor_311900_animation_0C9A8_indices.inc"
};

static AnimationSet _gActor311900Animation0C9A8 = {
    _gActor311900Animation0C9A8Records,
    _gActor311900Animation0C9A8Indices,
    { NULL, _gActor311900Animation0C9A8Bank1, NULL, NULL, _gActor311900Animation0C9A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor311900Animation0CDA0Bank1[6] = {
#include "assets/actor_311900_animation_0CDA0_bank1.inc"
};

static AnimationPackedRotation _gActor311900Animation0CDA0Bank4[75] = {
#include "assets/actor_311900_animation_0CDA0_bank4.inc"
};

static AnimationRecord _gActor311900Animation0CDA0Records[141] = {
#include "assets/actor_311900_animation_0CDA0_records.inc"
};

static u16 _gActor311900Animation0CDA0Indices[20] = {
#include "assets/actor_311900_animation_0CDA0_indices.inc"
};

static AnimationSet _gActor311900Animation0CDA0 = {
    _gActor311900Animation0CDA0Records,
    _gActor311900Animation0CDA0Indices,
    { NULL, _gActor311900Animation0CDA0Bank1, NULL, NULL, _gActor311900Animation0CDA0Bank4, NULL, NULL, NULL },
};

u8 D_actor_311900_8016EBE8[12] = {
    0,
    0,
    0,
    0,
    200,
    231,
    22,
    128,
    0,
    0,
    0,
    0,
};

u8 D_actor_311900_8016EBF4[12] = {
    0,
    0,
    0,
    0,
    192,
    235,
    22,
    128,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_311900_8016EC00 = { { { TASK_BODY_TMD, 96 } }, _actor311900SwatTask, { .model = &_gActor311900SwatMember2Body } };

TaskDesc D_actor_311900_8016EC0C = { { { TASK_BODY_TMD, 96 } }, _actor311900RupertTask, { .model = &_gActor311900RupertBroderickBody1 } };

u16 D_actor_311900_8016EC18[4][256];

/// Greys a figure's two adjacent texture palettes over three calls.
///
/// `task->work` must hold a live `_Actor311900Work`. `clutRowOffset` selects
/// VRAM rows 245 + offset and 246 + offset, each 256 RGB555 entries at X = 0.
/// `firstPaletteRow` selects two rows of `D_actor_311900_8016EC18`: 0 for
/// Rupert (offset 2), or 2 for the SWAT figure (offset 4). Keep that pair
/// unchanged through read-back, conversion and upload; later calls do nothing.
/// Conversion raises all channels to their maximum and forces the STP bit on.
static void _actor311900TickPaletteGreying(Task* task, s32 clutRowOffset, s16 firstPaletteRow)
{
    enum {
        ACTOR_311900_CLUT_BASE_Y       = 0xF5,
        ACTOR_311900_CLUT_CHANNEL_MASK = 0x1F,
        ACTOR_311900_CLUT_GREEN_SHIFT  = 5,
        ACTOR_311900_CLUT_BLUE_SHIFT   = 10,
        ACTOR_311900_CLUT_STP          = 0x8000,
    };
    RECT              rect;
    _Actor311900Work* work;
    s32               entryIndex;

    /// Raises one RGB555 palette entry's channels to their maximum and sets STP.
    ///
    /// `entry` must be a writable u16 lvalue without side effects: it is evaluated
    /// four times. Uses this function's `ACTOR_311900_CLUT_*` format constants;
    /// the entry expression must not refer to the temporary names red, green or blue.
#define ACTOR_311900_GREY_CLUT_ENTRY(entry)                                                                                        \
    do {                                                                                                                           \
        u16 red;                                                                                                                   \
        u16 green;                                                                                                                 \
        u16 blue;                                                                                                                  \
        red   = (entry) & ACTOR_311900_CLUT_CHANNEL_MASK;                                                                          \
        green = ((entry) >> ACTOR_311900_CLUT_GREEN_SHIFT) & ACTOR_311900_CLUT_CHANNEL_MASK;                                       \
        blue  = ((entry) >> ACTOR_311900_CLUT_BLUE_SHIFT) & ACTOR_311900_CLUT_CHANNEL_MASK;                                        \
        if (red < green) {                                                                                                         \
            red = green;                                                                                                           \
        } else {                                                                                                                   \
            green = red;                                                                                                           \
        }                                                                                                                          \
        if (green < blue) {                                                                                                        \
            green = blue;                                                                                                          \
        } else {                                                                                                                   \
            blue = green;                                                                                                          \
        }                                                                                                                          \
        if (blue < red) {                                                                                                          \
            blue = red;                                                                                                            \
        } else {                                                                                                                   \
            red = blue;                                                                                                            \
        }                                                                                                                          \
        (entry) = red | (green << ACTOR_311900_CLUT_GREEN_SHIFT) | (blue << ACTOR_311900_CLUT_BLUE_SHIFT) | ACTOR_311900_CLUT_STP; \
    } while (0)

    work = task->work;
    // Separate GPU read-back, CPU conversion and upload across successive ticks.
    if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_READ) {
        rect.x = 0;
        rect.y = clutRowOffset + ACTOR_311900_CLUT_BASE_Y;
        rect.w = ARRAY_SIZE(D_actor_311900_8016EC18[0]);
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[firstPaletteRow]);
        rect.x = 0;
        rect.y = clutRowOffset + ACTOR_311900_CLUT_BASE_Y + 1;
        rect.w = ARRAY_SIZE(D_actor_311900_8016EC18[0]);
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[firstPaletteRow + 1]);
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_CONVERT;
    } else if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_CONVERT) {
        for (entryIndex = 0; entryIndex < ARRAY_SIZE(D_actor_311900_8016EC18[0]); entryIndex++) {
            ACTOR_311900_GREY_CLUT_ENTRY(D_actor_311900_8016EC18[firstPaletteRow][entryIndex]);
        }
        for (entryIndex = 0; entryIndex < ARRAY_SIZE(D_actor_311900_8016EC18[0]); entryIndex++) {
            ACTOR_311900_GREY_CLUT_ENTRY(D_actor_311900_8016EC18[firstPaletteRow + 1][entryIndex]);
        }
#undef ACTOR_311900_GREY_CLUT_ENTRY
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_UPLOAD;
    } else if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_UPLOAD) {
        rect.x = 0;
        rect.y = clutRowOffset + ACTOR_311900_CLUT_BASE_Y;
        rect.w = ARRAY_SIZE(D_actor_311900_8016EC18[0]);
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[firstPaletteRow]);
        rect.x = 0;
        rect.y = clutRowOffset + ACTOR_311900_CLUT_BASE_Y + 1;
        rect.w = ARRAY_SIZE(D_actor_311900_8016EC18[0]);
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[firstPaletteRow + 1]);
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_DONE;
    }
}

/// Completes a clip reseed and schedules playback with a fresh tick count.
///
/// `playbackWork` and `reseedWork` must alias the same live writable figure
/// work block, with its animated slots already reseeded for `animId`.
/// `appliedAnimId` records that signed 16-bit animation-set table index.
/// Entering `ACTOR_ENEMY_ANIM_TICK` clears `animFrames`, the wrapping 16-bit
/// driver-tick count; the caller first advances the slots on its next update.
/// Both pointers are borrowed for this call.
static __inline__ void _actor311900CommitAnimationRequest(_Actor311900Work* playbackWork, _Actor311900Work* reseedWork)
{
    reseedWork->appliedAnimId = reseedWork->animId;
    playbackWork->animState   = ACTOR_ENEMY_ANIM_TICK;
    playbackWork->animFrames  = 0;
}

/// Applies an animation request or advances the figure's nineteen animated parts.
///
/// The live work block must have its context bound to the twenty-part model,
/// matching slots, pose buffers and loaded set table. Slot 0 is the root and
/// is left alone. Reset and blend requests select `animId`, record it in
/// `appliedAnimId`, clear the update count and enter `ACTOR_ENEMY_ANIM_TICK`.
/// Blend starts at the track's first record with zero transition frames.
/// Tick increments the wrapping 16-bit count and advances slots 1 through 19;
/// it does not end the task when a track reaches its end.
/// Other request states leave the work block untouched.
static void _actor311900UpdateAnimation(Task* task)
{
    _Actor311900Work* work;
    _Actor311900Work* reseedWork;
    _Actor311900Work* playbackWork;
    s32               blendSlot;
    s32               resetSlot;
    s32               tickSlot;

    work = task->work;
    // Reseeding leaves the root untouched and schedules playback for the next update.
    if (work->animState == ACTOR_ENEMY_ANIM_BLEND) {
        reseedWork = task->work;
        for (blendSlot = 1; blendSlot < ARRAY_SIZE(reseedWork->rig.slots); blendSlot++) {
            reseedWork->rig.slots[blendSlot].rate = reseedWork->animRate;
            animationSeekSlotWithBlend(&reseedWork->rig.anim, blendSlot, reseedWork->animId, 0, 0);
        }
        _actor311900CommitAnimationRequest(work, reseedWork);
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_RESET) {
        reseedWork = task->work;
        for (resetSlot = 1; resetSlot < ARRAY_SIZE(reseedWork->rig.slots); resetSlot++) {
            reseedWork->rig.slots[resetSlot].rate = reseedWork->animRate;
            animationResetSlot(&reseedWork->rig.anim, resetSlot, reseedWork->animId);
        }
        _actor311900CommitAnimationRequest(work, reseedWork);
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_TICK) {
        work->animFrames++;
        playbackWork = task->work;
        for (tickSlot = 1; tickSlot < ARRAY_SIZE(playbackWork->rig.slots); tickSlot++) {
            animationTickSlot(&playbackWork->rig.anim, tickSlot);
        }
    }
}

/// The actor's first state table - `_actor311900InitRupert`'s setup,
/// `_actor311900UpdateRupert`'s tick and teardown - dispatched through by
/// `_actor311900RupertTask`.
static const EnemyTaskFuncTable3 D_actor_311900_80161E24 = {
    _actor311900InitRupert,
    _actor311900UpdateRupert,
    enemyDestroy,
};

/// The actor's second state table - `_actor311900InitSwat`'s setup,
/// `_actor311900UpdateSwat`'s tick and teardown - dispatched through by
/// `_actor311900SwatTask`.
static const EnemyTaskFuncTable3 D_actor_311900_80161E30 = {
    _actor311900InitSwat,
    _actor311900UpdateSwat,
    enemyDestroy,
};

/// Dispatches Rupert's security-monitor task through initialization, playback and teardown.
///
/// `task->state` must be 0, 1 or 2, with a live owned `Enemy` in
/// `spawnArg2.pointer`. Initialization allocates the task's work; playback
/// requires that allocation. The selected handler may release both objects.
static void _actor311900RupertTask(Task* task)
{
    Enemy*              enemy;
    EnemyTaskFuncTable3 stateHandlers;

    enemy         = task->spawnArg2.pointer;
    stateHandlers = D_actor_311900_80161E24;
    stateHandlers.funcs[task->state](enemy, task);
}

/// Initializes Rupert's animated model for his one-shot security-monitor scene.
///
/// `enemy` and its owning TMD `task` must be live, with the twenty-part model
/// loaded. An already-seen scene or failed work allocation destroys them.
/// Otherwise the model borrows lighting and animation storage from the zeroed
/// work block until task teardown, and its root is parented to the view frame.
/// Playback starts on set 1; only Rupert's mapped camera draws the figure.
static void _actor311900InitRupert(Enemy* enemy, Task* task)
{
    _Actor311900Work* work;
    GfxCoord*         rootCoord;
    TmdObject*        model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & ACTOR_311900_RUPERT_SEEN) ||
        (work = memCalloc(sizeof(_Actor311900Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    _actor311900InitRupertLighting(task);
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_311900_8016EBE8, model, work->rig.poses,
                         work->rig.slots);
    rootCoord->parent   = &gGfxViewCoord;
    work->animState     = ACTOR_ENEMY_ANIM_RESET;
    work->animId        = ACTOR_311900_ANIMATION_SET;
    work->advanceFrames = 0;
    work->advancing     = 0;
    if ((viewGetMappedIndex() & 0xFF) == ACTOR_311900_RUPERT_VIEW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    _actor311900UpdateAnimation(task);
    task->state += 1;
}

/// Advances Rupert's security-monitor scene and completes it after ninety walking updates.
///
/// Requires the initialized task and its live work and model. Viewing Rupert
/// records the seen bit and permanently starts his forward walk; later camera
/// changes hide the model without stopping playback or the walk counter.
/// Each walking tick requests 36 parent-coordinate units along local Z.
/// Actor freezing suppresses displacement, but the counter still advances.
/// Completion records the scene-done flag and selects teardown for the next call.
static void _actor311900UpdateRupert(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_311900_RUPERT_ADVANCE_DISTANCE = 0x24, // Parent-coordinate units per tick
        ACTOR_311900_RUPERT_CLUT_ROW_OFFSET  = 2,
        ACTOR_311900_RUPERT_PALETTE_ROW      = 0,
    };
    _Actor311900Work* work;
    GfxCoord*         rootCoord;
    TmdObject*        model;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    _actor311900TickPaletteGreying(task, ACTOR_311900_RUPERT_CLUT_ROW_OFFSET, ACTOR_311900_RUPERT_PALETTE_ROW);
    if ((viewGetMappedIndex() & 0xFF) == ACTOR_311900_RUPERT_VIEW) {
        gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN, gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) | ACTOR_311900_RUPERT_SEEN);
        model->flags    = 0;
        work->advancing = 1;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Once seen, the walk continues even while another camera hides the model.
    if (work->advancing == 1) {
        work->advanceFrames++;
        _actorMovementStepForwardNonzero(rootCoord, ACTOR_311900_RUPERT_ADVANCE_DISTANCE);
    }
    _actor311900UpdateAnimation(task);
    if (work->advanceFrames >= ACTOR_311900_ADVANCE_FRAMES) {
        gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_CAM_A_SCENE_DONE, 1);
        task->state++;
    }
}

/// Dispatches the SWAT figure's security-monitor task through initialization, playback and teardown.
///
/// `task->state` must be 0, 1 or 2, with a live owned `Enemy` in
/// `spawnArg2.pointer`. Playback requires initialization's work allocation.
/// The selected handler may release both objects; playback stays in state 1.
static void _actor311900SwatTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_311900_80161E30;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Initializes the stationary SWAT figure shown on its security-monitor camera.
///
/// `enemy` and its owning TMD `task` must be live, with the twenty-part model
/// loaded. Route progress at or beyond 3, or a failed work allocation, destroys
/// them. Otherwise the model borrows the zeroed work block's lighting and
/// animation storage until teardown. Its root is parented to the view frame,
/// and set 1 begins playback; camera visibility is applied on the next tick.
static void _actor311900InitSwat(Enemy* enemy, Task* task)
{
    enum { ACTOR_311900_SWAT_ROUTE_LIMIT = 3 };
    _Actor311900Work* work;
    GfxCoord*         rootCoord;
    TmdObject*        model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) >= ACTOR_311900_SWAT_ROUTE_LIMIT ||
        (work = memCalloc(sizeof(_Actor311900Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    _actor311900InitSwatLighting(task);
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_311900_8016EBF4, model, work->rig.poses,
                         work->rig.slots);
    rootCoord->parent = &gGfxViewCoord;
    work->animState   = ACTOR_ENEMY_ANIM_RESET;
    work->animId      = ACTOR_311900_ANIMATION_SET;
    _actor311900UpdateAnimation(task);
    task->state += 1;
}

/// Greys and animates the stationary SWAT figure, drawing it only on its mapped camera.
///
/// Requires the initialized task and its live work and model. Palette processing
/// and animation continue on other views. This handler does not advance task state.
static void _actor311900UpdateSwat(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_311900_SWAT_CLUT_ROW_OFFSET = 4,
        ACTOR_311900_SWAT_PALETTE_ROW     = 2,
    };
    TmdObject* model;

    model = task->extra.tmd;
    _actor311900TickPaletteGreying(task, ACTOR_311900_SWAT_CLUT_ROW_OFFSET, ACTOR_311900_SWAT_PALETTE_ROW);
    if ((viewGetMappedIndex() & 0xFF) == ACTOR_311900_SWAT_VIEW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    _actor311900UpdateAnimation(task);
}

/// Translates a coordinate along its normalized local Z axis for a nonzero signed step.
///
/// `coord` must be live and writable. `stepDistance` is in parent-coordinate
/// units; negative steps move backward. Normalization removes matrix scale,
/// then GTE fixed-point scaling narrows the displacement to signed halfwords.
/// The initialized scratch stack must have room for one aligned `SVECTOR`.
/// It is released before return; GTE state is overwritten and no pointer retained.
/// Returns the requested step, or 0 when live `actorsFrozen` equals 1.
/// A zero step reserves and releases scratch without dirtying the coordinate.
static s32 _actorMovementStepForwardNonzero(GfxCoord* coord, s16 stepDistance)
{
    enum { ACTOR_MOVEMENT_FROZEN = 1 };
    SVECTOR* scratchEnd;
    SVECTOR* displacement;
    SVECTOR* gteDisplacement;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == ACTOR_MOVEMENT_FROZEN) {
        return 0;
    }
    scratchEnd                    = SCRATCH_STACK_CURSOR(SVECTOR);
    displacement                  = scratchEnd - 1;
    gteDisplacement               = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = displacement;
    if (stepDistance != 0) {
        // The local axis and translation share the coordinate's parent space.
        gfxReadMatrixZAxis(&coord->coord, displacement);
        VectorNormalSS(displacement, displacement);
        gte_lddp(stepDistance);
        gte_ldsv(gteDisplacement);
        gte_gpf12();
        gte_stsv(gteDisplacement);
        coord->coord.t[0]  += scratchEnd[-1].vx;
        coord->coord.t[1]  += displacement->vy;
        coord->coord.t[2]  += displacement->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return stepDistance;
}

/// Installs Rupert's fixed monochrome lighting matrices on his monitor model.
///
/// The task must own a live zeroed work block and TMD model. All three colour
/// rows sum the light channels with unit coefficients. The direction matrix
/// has unit coefficients except zero at [1][0] and [2][2]. The renderer borrows
/// both matrices until work release; their zero translations give no background colour.
static void _actor311900InitRupertLighting(Task* task)
{
    TmdObject*        model;
    _Actor311900Work* work;

    work  = task->work;
    model = task->extra.tmd;

    // Seed through the packed view, then install the figure's coefficients.
    gfxSetRotIdentity(&work->light);
    gfxSetRotIdentity(&work->color);

    model->lightMtx = &work->light;

    work->color.m[0][0] = ONE;
    work->color.m[0][1] = ONE;
    work->color.m[0][2] = ONE;
    work->color.m[1][0] = ONE;
    work->color.m[1][1] = ONE;
    work->color.m[1][2] = ONE;
    work->color.m[2][0] = ONE;
    work->color.m[2][1] = ONE;
    work->color.m[2][2] = ONE;

    work->light.m[0][0] = ONE;
    work->light.m[0][1] = ONE;
    work->light.m[0][2] = ONE;
    work->light.m[1][0] = 0;
    work->light.m[1][1] = ONE;
    work->light.m[1][2] = ONE;
    work->light.m[2][0] = ONE;
    work->light.m[2][1] = ONE;
    work->light.m[2][2] = 0;

    model->colorMtx = &work->color;
}

/// Installs the SWAT figure's fixed monochrome lighting matrices on its monitor model.
///
/// The task must own a live zeroed work block and TMD model. All three colour
/// rows sum the light channels with unit coefficients. The direction matrix
/// is all unit coefficients except negative unity at [0][0]. The renderer
/// borrows both matrices until work release; their translations stay zero.
static void _actor311900InitSwatLighting(Task* task)
{
    TmdObject*        model;
    _Actor311900Work* work;

    work  = task->work;
    model = task->extra.tmd;

    // Seed through the packed view, then install the figure's coefficients.
    gfxSetRotIdentity(&work->light);
    gfxSetRotIdentity(&work->color);

    model->lightMtx = &work->light;

    work->color.m[0][0] = ONE;
    work->color.m[0][1] = ONE;
    work->color.m[0][2] = ONE;
    work->color.m[1][0] = ONE;
    work->color.m[1][1] = ONE;
    work->color.m[1][2] = ONE;
    work->color.m[2][0] = ONE;
    work->color.m[2][1] = ONE;
    work->color.m[2][2] = ONE;

    work->light.m[0][0] = -ONE;
    work->light.m[0][1] = ONE;
    work->light.m[0][2] = ONE;
    work->light.m[1][0] = ONE;
    work->light.m[1][1] = ONE;
    work->light.m[1][2] = ONE;
    work->light.m[2][0] = ONE;
    work->light.m[2][1] = ONE;
    work->light.m[2][2] = ONE;

    model->colorMtx = &work->color;
}
