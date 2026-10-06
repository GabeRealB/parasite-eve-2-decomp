#include "actor_521100_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Clip of `_Actor521100AnmcWomanState::animId` the woman is spawned in, and
/// the only one a walk moves her under.
#define ACTOR_521100_ANMC_WOMAN_ANIM_WALK 1

/// Distance the woman's walk covers each frame, along the root coordinate's
/// local Z axis. A walk request divides the ground distance to its target by
/// this to get `_Actor521100AnmcWomanState::travel`.
#define ACTOR_521100_ANMC_WOMAN_WALK_STRIDE 20

/// Values of `_Actor521100AnmcWomanState::flattenStep`: how far the flatten
/// that ends the woman has got.
enum {
    ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN  = 0, // Not started: the next frame turns the root to `yaw`, saves it and starts at full height
    ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK = 1, // The height is lowered each frame
    ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE   = 2, // The height has reached its floor: the model is left as it is and no longer animated
};

/// What `_Actor521100AnmcWomanState::flattenScaleY` loses each frame of the
/// shrink, and the height the shrink stops at, 4096 being full height.
#define ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_STEP  0x10
#define ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_FLOOR 0x100

/// Values of `_Actor521100AnmcWomanState::flattenFrames` at which the shrinking
/// model turns semi-transparent and at which the burn effect is spawned
/// beside it.
#define ACTOR_521100_ANMC_WOMAN_FLATTEN_FADE_FRAMES 10
#define ACTOR_521100_ANMC_WOMAN_FLATTEN_BURN_FRAMES 15

/// The animation request, the walk and the flatten the ANMC woman keeps right
/// after her rig.
///
/// A play request stores the clip in `animId` and the reseed to perform in
/// `state`; the update that performs it copies the clip to `appliedAnimId`.
/// A clip id is the request's animation number plus one and indexes the
/// woman's own animation table, whose entry 0 is empty. A walk request turns
/// the root coordinate to face its target, leaving the heading in `yaw`, and
/// stores the frames of walking in `travel`.
///
/// The flatten is how the woman ends: her model is squashed along its Y axis,
/// a little further each frame, while its light colour is scaled by the same
/// factor with a random flicker. Each frame rebuilds the root coordinate from
/// `savedRootMtx` and the current `flattenScaleY`, so the scale never
/// compounds.
///
/// The block has `ActorEnemyState`'s size, and the members it shares with
/// that type sit where that type has them; the flatten occupies bytes that
/// type gives another role or leaves unnamed. No access to `pad_30` or
/// `pad_34` has been observed, and the block is allocated zeroed.
typedef struct {
    s16    state;         // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16    appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16    animId;        // Clip requested, by spawn (`ACTOR_521100_ANMC_WOMAN_ANIM_WALK`) or the last play request
    s16    field_6;       // Cleared by each play request; never read, role unproven
    s16    flattenStep;   // `ACTOR_521100_ANMC_WOMAN_FLATTEN_*`
    s16    flattenFrames; // Frames the flatten has shrunk for
    s16    flattenScaleY; // Height of the flattening model, 4096 = full
    MATRIX savedRootMtx;  // Root coordinate's local matrix as the flatten began, turned to `yaw`; each shrink frame rescales a copy of it
    byte   pad_30[0x2];
    s16    yaw;           // Heading last given the root coordinate, 4096 to a turn
    byte   pad_34[0x2];
    s16    travel;        // Frames of forward movement the walk has left, `ACTOR_521100_ANMC_WOMAN_WALK_STRIDE` units each
} _Actor521100AnmcWomanState;
STATIC_ASSERT_SIZEOF(_Actor521100AnmcWomanState, 0x38);

/// Work block of the ANMC woman, the scripted figure this package carries
/// beside the golem: allocated zeroed at its full size by her task's spawn
/// state and kept both at `Task::work` and in a global her message handlers
/// reach it through.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives. Slots 1 to 18 of the rig are driven; slot 0, the root's, is never
/// started.
typedef struct {
    MATRIX                     light; // Light-direction matrix lent to the model object
    MATRIX                     color; // Light-colour matrix lent to the model object; the flatten scales it down
    ActorAnimRig19             rig;   // Playback storage of the nineteen-part model
    _Actor521100AnmcWomanState st;    // Animation request, flatten, heading and frames of walk left
} _Actor521100AnmcWomanWork;
STATIC_ASSERT_SIZEOF(_Actor521100AnmcWomanWork, 0x4B4);

extern _Actor521100AnmcWomanWork* D_actor_521100_8016A3D8;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_521100_8016A358[6];
extern TaskDesc         D_actor_521100_8016A388[];
extern AnimationSet*    D_actor_521100_8016A3A0[11];

extern EffectSpawnArg D_actor_521100_8016A3CC;
extern u16            D_actor_521100_8016A3D4;

/// The task `func_actor_521100_80136604` runs, stored by its create state
/// `func_actor_521100_80135DDC`.
extern Task*    D_actor_521100_8016A3DC;
extern Task*    D_actor_521100_8016A3E0;
extern Task*    D_actor_521100_8016A3E4;
extern GfxCoord D_actor_521100_8016A3E8;

static void func_actor_521100_80135DDC(Enemy* spawnArg2, Task* task);
static void func_actor_521100_80135F2C(Task* task);
static void func_actor_521100_801360C4(Enemy* spawnArg2, Task* task);
static void func_actor_521100_80136290(Enemy* arg0, Task* task);
static void func_actor_521100_80136680(Enemy* arg0, Task* task);
static void func_actor_521100_801366FC(Task* task);
static void func_actor_521100_80136724(void);
static void func_actor_521100_8013677C(void);
static void func_actor_521100_80136820(void);
static void func_actor_521100_801368B0(Task* task);

s32  func_actor_521100_801369B8(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_521100_80136A1C(Task*, s32, s32, s32);
s32  func_actor_521100_80136A64(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32  func_actor_521100_80136AE0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32  func_actor_521100_80136BE8(Task* task, s32 msgId, ActorTransform* target, s32 arg3);
void func_actor_521100_80136404(Task*);
void func_actor_521100_80136604(Task*);

AnimationSet* D_actor_521100_8015F73C[36] = {
    NULL,
    &gActor521100Animation10EAC,
    &gActor521100Animation11614,
    &gActor521100Animation12204,
    &gActor521100Animation1271C,
    &gActor521100Animation1B8F8,
    &gActor521100Animation1A8E4,
    &gActor521100Animation13454,
    &gActor521100Animation13B8C,
    &gActor521100Animation143F0,
    &gActor521100Animation14918,
    &gActor521100Animation14C38,
    NULL,
    &gActor521100Animation15168,
    &gActor521100Animation152F8,
    &gActor521100Animation15904,
    &gActor521100Animation15E28,
    &gActor521100Animation16628,
    &gActor521100Animation16F90,
    &gActor521100Animation18B20,
    &gActor521100Animation1C104,
    &gActor521100Animation1C58C,
    &gActor521100Animation20880,
    &gActor521100Animation20F94,
    &gActor521100Animation2177C,
    &gActor521100Animation220B8,
    &gActor521100Animation229A0,
    &gActor521100Animation2318C,
    &gActor521100Animation239C0,
    &gActor521100Animation24F5C,
    &gActor521100Animation25ACC,
    &gActor521100Animation26024,
    &gActor521100Animation26E3C,
    &gActor521100Animation26FCC,
    &gActor521100Animation271A8,
    &gActor521100Animation27A78,
};

AnimationSet* D_actor_521100_8015F7CC[14] = {
    NULL,
    NULL,
    &gActor521100Animation27DB0,
    &gActor521100Animation28380,
    &gActor521100Animation28DD8,
    &gActor521100Animation291F8,
    &gActor521100Animation29534,
    &gActor521100Animation29A88,
    NULL,
    &gActor521100Animation2A998,
    &gActor521100Animation2B89C,
    &gActor521100Animation2C378,
    &gActor521100Animation2CCA8,
    &gActor521100Animation2D708,
};

EffectSpawnArg D_actor_521100_8015F804 = { NULL, 300, 1 };

Actor521100ThrowSpan D_actor_521100_8015F80C[2][17] = {
    { { 2, -5 }, { 5, -10 }, { 9, -5 }, { 12, -33 }, { 14, -65 }, { 17, -40 }, { 23, -48 }, { 28, -24 }, { 30, 40 }, { 33, 170 }, { 34, 290 }, { 36, 220 }, { 38, 135 }, { 43, 60 }, { 52, 33 }, { 57, 2 }, { 69, 0 } },
    { { 2, -5 }, { 5, -10 }, { 9, -5 }, { 11, -7 }, { 13, -40 }, { 14, -35 }, { 17, -33 }, { 23, -48 }, { 29, -23 }, { 31, 115 }, { 33, 160 }, { 34, 350 }, { 38, 175 }, { 57, 26 }, { 61, 36 }, { 69, 0 }, { 69, 0 } },
};

s16 D_actor_521100_8015F894[20] = {
    0,
    8,
    0,
    8,
    0,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    3,
    8,
    0,
    1,
};

s16 D_actor_521100_8015F8BC[8] = {
    1,
    4,
    6,
    10,
    13,
    14,
    16,
    17,
};

s16 D_actor_521100_8015F8CC[4] = {
    0,
    7,
    14,
    28,
};

static TmdBone _gActor521100AnmcWoman2BodySkeleton[19] = {
#include "assets/anmc_woman_2_body_skeleton.inc"
};

static u32 _gActor521100AnmcWoman2BodyPartVerts[19] = {
#include "assets/anmc_woman_2_body_partVerts.inc"
};

static SVECTOR _gActor521100AnmcWoman2BodyVerts[344] = {
#include "assets/anmc_woman_2_body_verts.inc"
};

static SVECTOR _gActor521100AnmcWoman2BodyNormals[337] = {
#include "assets/anmc_woman_2_body_normals.inc"
};

static u32 _gActor521100AnmcWoman2BodyStream[3946] = {
#include "assets/anmc_woman_2_body_stream.inc"
};

static TmdSource _gActor521100AnmcWoman2Body = {
    0,
    21340,
    6264,
    19,
    _gActor521100AnmcWoman2BodyPartVerts,
    _gActor521100AnmcWoman2BodyVerts,
    _gActor521100AnmcWoman2BodyNormals,
    _gActor521100AnmcWoman2BodySkeleton,
    _gActor521100AnmcWoman2BodyStream,
};

static AnimationPackedPose _gActor521100Animation36E34Bank1[136] = {
#include "assets/actor_521100_animation_36E34_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation36E34Bank4[1154] = {
#include "assets/actor_521100_animation_36E34_bank4.inc"
};

static AnimationRecord _gActor521100Animation36E34Records[2361] = {
#include "assets/actor_521100_animation_36E34_records.inc"
};

static u16 _gActor521100Animation36E34Indices[20] = {
#include "assets/actor_521100_animation_36E34_indices.inc"
};

static AnimationSet _gActor521100Animation36E34 = {
    _gActor521100Animation36E34Records,
    _gActor521100Animation36E34Indices,
    { NULL, _gActor521100Animation36E34Bank1, NULL, NULL, _gActor521100Animation36E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation372D8Bank1[8] = {
#include "assets/actor_521100_animation_372D8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation372D8Bank4[109] = {
#include "assets/actor_521100_animation_372D8_bank4.inc"
};

static AnimationRecord _gActor521100Animation372D8Records[144] = {
#include "assets/actor_521100_animation_372D8_records.inc"
};

static u16 _gActor521100Animation372D8Indices[20] = {
#include "assets/actor_521100_animation_372D8_indices.inc"
};

static AnimationSet _gActor521100Animation372D8 = {
    _gActor521100Animation372D8Records,
    _gActor521100Animation372D8Indices,
    { NULL, _gActor521100Animation372D8Bank1, NULL, NULL, _gActor521100Animation372D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation37730Bank1[6] = {
#include "assets/actor_521100_animation_37730_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation37730Bank4[69] = {
#include "assets/actor_521100_animation_37730_bank4.inc"
};

static AnimationRecord _gActor521100Animation37730Records[171] = {
#include "assets/actor_521100_animation_37730_records.inc"
};

static u16 _gActor521100Animation37730Indices[20] = {
#include "assets/actor_521100_animation_37730_indices.inc"
};

static AnimationSet _gActor521100Animation37730 = {
    _gActor521100Animation37730Records,
    _gActor521100Animation37730Indices,
    { NULL, _gActor521100Animation37730Bank1, NULL, NULL, _gActor521100Animation37730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation38334Bank1[21] = {
#include "assets/actor_521100_animation_38334_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation38334Bank4[294] = {
#include "assets/actor_521100_animation_38334_bank4.inc"
};

static AnimationRecord _gActor521100Animation38334Records[392] = {
#include "assets/actor_521100_animation_38334_records.inc"
};

static u16 _gActor521100Animation38334Indices[20] = {
#include "assets/actor_521100_animation_38334_indices.inc"
};

static AnimationSet _gActor521100Animation38334 = {
    _gActor521100Animation38334Records,
    _gActor521100Animation38334Indices,
    { NULL, _gActor521100Animation38334Bank1, NULL, NULL, _gActor521100Animation38334Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation38510Bank1[2] = {
#include "assets/actor_521100_animation_38510_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation38510Bank4[17] = {
#include "assets/actor_521100_animation_38510_bank4.inc"
};

static AnimationRecord _gActor521100Animation38510Records[76] = {
#include "assets/actor_521100_animation_38510_records.inc"
};

static u16 _gActor521100Animation38510Indices[20] = {
#include "assets/actor_521100_animation_38510_indices.inc"
};

static AnimationSet _gActor521100Animation38510 = {
    _gActor521100Animation38510Records,
    _gActor521100Animation38510Indices,
    { NULL, _gActor521100Animation38510Bank1, NULL, NULL, _gActor521100Animation38510Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_521100_8016A358[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_521100_801369B8 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_521100_80136A1C },
    { ACTOR_MESSAGE_PLACE, func_actor_521100_80136A64 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_521100_80136AE0 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_521100_80136BE8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_521100_8016A388[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_521100_80136604, { .model = &_gActor521100AnmcWoman2Body } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_521100_80136404, { .value = 0 } },
};

AnimationSet* D_actor_521100_8016A3A0[11] = {
    NULL,
    &_gActor521100Animation36E34,
    &_gActor521100Animation372D8,
    &_gActor521100Animation37730,
    &_gActor521100Animation38334,
    &_gActor521100Animation36E34,
    &_gActor521100Animation36E34,
    &_gActor521100Animation36E34,
    &_gActor521100Animation36E34,
    &_gActor521100Animation36E34,
    &_gActor521100Animation38510,
};

EffectSpawnArg D_actor_521100_8016A3CC = { NULL, 320, 1 };

u16 D_actor_521100_8016A3D4 = 5;

_Actor521100AnmcWomanWork* D_actor_521100_8016A3D8;

Task* D_actor_521100_8016A3DC;

Task* D_actor_521100_8016A3E0;

Task* D_actor_521100_8016A3E4;

GfxCoord D_actor_521100_8016A3E8;

s32 func_actor_521100_80135D10(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    Actor521100Work* work;

    obj  = arg0->extra.tmd;
    work = arg0->work;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    work->modelDrawFlags = arg2;
    return 0;
}

s32 func_actor_521100_80135D58(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor521100Work* work;

    work = arg0->work;
    switch (request->command) {
        case 0:
            work->eventBurnStage = 1;
            work->stateCounter   = 0;
            work->stateElapsed   = 0;
            break;
        case 1:
            work->weaponHidden = request->command;
            break;
    }
    return 0;
}

s32 func_actor_521100_80135D9C(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    ((Actor521100Work*)arg0->work)->activated = 1;
    (sceneAcquireBattleRef)(0);
    return 0;
}

s32 func_actor_521100_80135DC8(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    return ((Actor521100Work*)arg0->work)->present;
}

static void func_actor_521100_80135DDC(Enemy* spawnArg2, Task* task)
{
    VECTOR                     vec;
    _Actor521100AnmcWomanWork* mem;
    Enemy*                     enemy;
    TmdObject*                 obj;
    GfxCoord*                  coord;

    enemy                   = spawnArg2;
    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    mem                     = memCalloc(sizeof(_Actor521100AnmcWomanWork), 0);
    D_actor_521100_8016A3D8 = mem;
    task->work              = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_521100_801366FC;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->lightMtx                    = &D_actor_521100_8016A3D8->light;
    obj->colorMtx                    = &D_actor_521100_8016A3D8->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_521100_8016A3DC          = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationBindModelContext(&D_actor_521100_8016A3D8->rig.anim, D_actor_521100_8016A3A0, obj, D_actor_521100_8016A3D8->rig.poses);
    D_actor_521100_8016A3D8->st.animId = ACTOR_521100_ANMC_WOMAN_ANIM_WALK;
    D_actor_521100_8016A3D8->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                     = D_actor_521100_8016A358;
    func_actor_521100_80135F2C(task);
    task->state += 1;
}

/// The actor's step body, run every frame while `st.state` is
/// `ACTOR_ENEMY_ANIM_TICK`. The two pending reseeds, `ACTOR_ENEMY_ANIM_BLEND`
/// and `ACTOR_ENEMY_ANIM_RESET`, run their reseed body first and advance the
/// step to `ACTOR_ENEMY_ANIM_TICK`, which is why they share the tail that
/// stores it.
///
/// The tick step while a walk is in progress (`st.animId` is the walk clip and
/// `st.travel` still has frames left) advances the root coordinate one step:
/// 20 units along its local Z axis, the stride the walk-to handler divided the
/// distance by, through `actorMoveForward`. The pause check the helper makes
/// is why the step is skipped while the game is frozen - `st.travel` still
/// ticks down, so a paused actor finishes its walk.
static void func_actor_521100_80135F2C(Task* task)
{
    _Actor521100AnmcWomanWork* work;
    s16                        animId;

    work = D_actor_521100_8016A3D8;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        func_actor_521100_80136820();
        D_actor_521100_8016A3D8->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        func_actor_521100_8013677C();
        D_actor_521100_8016A3D8->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        animId = work->st.animId;
        if (animId == ACTOR_521100_ANMC_WOMAN_ANIM_WALK && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, ACTOR_521100_ANMC_WOMAN_WALK_STRIDE);
            D_actor_521100_8016A3D8->st.travel--;
        }
        func_actor_521100_80136724();
        return;
    }
}
/// State-2 body, the actor's last: it snapshots the attach coordinate onto a
/// stack `GfxCoord` - the copy the flatten's effect is placed off - and
/// runs the flatten step `st.flattenStep`. `ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN`
/// seeds the flatten (the shrink body `func_actor_521100_801368B0` scales by
/// `st.flattenScaleY`, so the seed stores `ONE` there, turns the root
/// coordinate to `st.yaw` and snapshots its local matrix into
/// `st.savedRootMtx`), `ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK` runs that body
/// and drops the 0x600A5 effect once `st.flattenFrames` reaches 0xF, and
/// `ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE` returns without animating. The other
/// two steps fall through to the slot tick and the colour step.
static void func_actor_521100_801360C4(Enemy* spawnArg2, Task* task)
{
    GfxCoord                   sp10;
    TmdObject*                 obj;
    GfxCoord*                  coord;
    _Actor521100AnmcWomanWork* work;
    s32                        i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = task->work;
    sp10  = *coord;

    switch (work->st.flattenStep) {
        case ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN:
            work->st.flattenFrames = 0;
            work->st.flattenScaleY = ONE;
            gfxRotMatrixY(&coord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
            work->st.savedRootMtx = coord->coord;
            work->st.flattenStep  = ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK;
            break;

        case ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK:
            func_actor_521100_801368B0(task);
            work->st.flattenFrames++;
            if (work->st.flattenFrames == ACTOR_521100_ANMC_WOMAN_FLATTEN_FADE_FRAMES) {
                obj->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->st.flattenFrames == ACTOR_521100_ANMC_WOMAN_FLATTEN_BURN_FRAMES) {
                sp10.coord.t[0] -= 0x1F4;
                sp10.coord.t[2] -= 0x64;
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &sp10, 5, NULL);
            }
            break;

        case ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE:
            return;
    }

    i = 1;
    do {
        animationTickDirectSlot(&D_actor_521100_8016A3D8->rig.anim, &D_actor_521100_8016A3D8->rig.slots[i]);
        i++;
    } while (i < 0x13);

    func_actor_521100_80136290(spawnArg2, task);
}
/// The flatten's colour step: takes a 0x10-byte `VECTOR` off the scratch stack,
/// fills it with the world position of the model's *second* attach coordinate
/// (the one the flatten is scaling) and hands it to `worldCoordUpdateActorColor` as the
/// colour target. The same draw then overwrites the three components with
/// `st.flattenScaleY` scaled by the top half of three successive `gRandomLcgState` draws,
/// and `ScaleMatrixL` multiplies the work block's second matrix by it.
///
/// Each draw reads `gRandomLcgState` back from the global: the initialiser's store
/// is what the next draw's shift sees, and it is why one `lw` feeds all three
/// and each draw's value gets its own register.
static void func_actor_521100_80136290(Enemy* arg0, Task* task)
{
    _Actor521100AnmcWomanWork* work;
    GfxCoord*                  coord;
    void**                     scratch;
    u8*                        head;
    VECTOR*                    block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD_AT(scratch, void) = block;
    actorRenderComposeCoord(coord);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0, block, 0, 0);
    work            = D_actor_521100_8016A3D8;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    block->vx       = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + 0x8000) / 0x10000;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    block->vy       = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + 0x8000) / 0x10000;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    block->vz       = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + 0x8000) / 0x10000;
    ScaleMatrixL(&work->color, block);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
/// Companion task body: the two tasks the `0x7DB` handler
/// `func_actor_521100_80136AE0` spawns out of the `D_actor_521100_8016A388`
/// table, which differ only in `Task::spawnArg1` and in the slot they are kept
/// in (`D_actor_521100_8016A3E0` for 0, `D_actor_521100_8016A3E4` for 1).
///
/// Every 8th frame of `Task::state` it re-anchors the global effect coordinate
/// on one of the two models' attach coordinates: with `spawnArg1` 0 the actor's
/// own second coordinate, raised 0x32, pushed back 0x32 and given a random
/// vertical jitter of `(LCG top half - 0x8000) * 200 / 0x10000` (so within
/// +/-100); with 1 the player's (slot 3) first coordinate, moved by a fixed
/// (0x2BC, -0x384). The anchor is then cleared, updated and handed to the
/// effect spawner `func_800FDB18` through the `EffectSpawnArg` record beside it.
///
/// `Task::state` is the frame counter as well as the run gate - it advances
/// every frame and the body stops re-anchoring once it reaches 0x83, killing
/// the task and clearing whichever slot holds it.
///
/// The 0x32 pair is adjusted before the `gRandomLcgState` draw, not after: that is
/// the source order that lets the draw's store sink below both halfword-field
/// loads in `sched2`, which is what puts them on $a3 rather than $a0.
void func_actor_521100_80136404(Task* task)
{
    Task* ctx;

    ctx = task->spawnArg2.pointer;
    if (!(task->state & 7)) {
        if (task->spawnArg1.value == 0) {
            D_actor_521100_8016A3E8             = ctx->extra.tmd->coords[1];
            D_actor_521100_8016A3E8.coord.t[2] += 0x32;
            gRandomLcgState                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_actor_521100_8016A3E8.coord.t[1] -= 0xFA + (s32)((gRandomLcgState >> 16) - 0x8000) * 0xC8 / 0x10000;
            D_actor_521100_8016A3E8.coord.t[0] -= 0x32;
        } else {
            D_actor_521100_8016A3E8             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[0];
            D_actor_521100_8016A3E8.coord.t[1] -= 0x384;
            D_actor_521100_8016A3E8.coord.t[0] += 0x2BC;
        }
        D_actor_521100_8016A3E8.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&D_actor_521100_8016A3E8);
        D_actor_521100_8016A3CC.coord = &D_actor_521100_8016A3E8;
        func_800FDB18(D_actor_521100_8016A3D4, &D_actor_521100_8016A3E8, NULL, &D_actor_521100_8016A3CC);
    }
    if (task->state >= 0x83) {
        taskKill(task);
        if (task->spawnArg1.value == 0) {
            D_actor_521100_8016A3E0 = NULL;
        } else {
            D_actor_521100_8016A3E4 = NULL;
        }
    }
    task->state++;
}
/// State table the overlay dispatches through, indexed by `Task::state`:
/// create, update and teardown.
static const EnemyTaskFuncTable3 D_actor_521100_80131E68 = { {
    func_actor_521100_80135DDC,
    func_actor_521100_80136680,
    func_actor_521100_801360C4,
} };

/// State dispatcher: copies the overlay's 3-entry state table onto the stack,
/// caches the work pointer, and calls the entry `Task::state` selects.
void func_actor_521100_80136604(Task* arg0)
{
    EnemyTaskFuncTable3 sp;
    // Filled here and then neither read nor passed on, so what the record is
    // for is unproven, as is the signedness of its fields. The stores establish
    // these four bytes; the frame has room for up to four more behind them.
    struct {
        u8  field_0;
        u8  field_1;
        u16 field_2;
    } unread;

    sp                      = D_actor_521100_80131E68;
    unread.field_0          = 2;
    unread.field_1          = 9;
    unread.field_2          = 1;
    D_actor_521100_8016A3D8 = arg0->work;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_521100_80136680(Enemy* arg0, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    func_actor_521100_80135F2C(task);
}

/// `Task::exitCallback` the create state `func_actor_521100_80135DDC`
/// installs: hands the task's `Enemy` back to `enemyDestroy`.
static void func_actor_521100_801366FC(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
static void func_actor_521100_80136724(void)
{
    s32 i;

    i = 1;
    do {
        animationTickDirectSlot(&D_actor_521100_8016A3D8->rig.anim, &D_actor_521100_8016A3D8->rig.slots[i]);
        i++;
    } while (i < 0x13);
}

/// Re-inits animation slots 1..0x12 from `st.animId`, forcing each slot's set
/// index to 1 first, and latches that id into `st.appliedAnimId` as the one
/// now playing.
static void func_actor_521100_8013677C(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_521100_8016A3D8->rig.slots[i].rate = 1;
        animationInitDirectSlot(&D_actor_521100_8016A3D8->rig.anim, &D_actor_521100_8016A3D8->rig.slots[i], i,
                                D_actor_521100_8016A3D8->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->st.appliedAnimId = D_actor_521100_8016A3D8->st.animId;
}

/// Reseeds animation slots 1..0x12 from `st.animId` and latches that id into
/// `st.appliedAnimId` as the one now playing.
static void func_actor_521100_80136820(void)
{
    s32 i;

    i = 1;
    do {
        animationStartDirectSlot(&D_actor_521100_8016A3D8->rig.anim, &D_actor_521100_8016A3D8->rig.slots[i], i,
                                 D_actor_521100_8016A3D8->st.animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->st.appliedAnimId = D_actor_521100_8016A3D8->st.animId;
}

/// Flatten step body, run while `st.flattenStep` is
/// `ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK`: takes an `ActorScaleScratch`
/// block from the scratch stack, splats an identity rotation into it and hands
/// it to `ScaleMatrix` with a `(ONE, st.flattenScaleY, ONE)` vector, then
/// restores the root coordinate's local matrix from `st.savedRootMtx`, the
/// snapshot the flatten's first step took, and multiplies the product into it,
/// so the scale never compounds. The scale drops 0x10 a frame; under 0x101 the
/// step advances to `ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE` and this body stops
/// running.
///
/// The scratch pointer is taken with a chained assignment on purpose: the
/// store and the callee-saved copy are what put the extra `move $s0, $v0`
/// between the `addiu` and the `sw` (and the `nop` in the load's delay slot).
static void func_actor_521100_801368B0(Task* task)
{
    ActorScaleScratch*         head;
    ActorScaleScratch*         scratch;
    _Actor521100AnmcWomanWork* work;
    GfxCoord*                  coord;

    head    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work    = task->work;
    scratch = (SCRATCH_STACK_CURSOR(ActorScaleScratch) = head - 1);
    coord   = task->extra.tmd->coords;
    if (work->st.flattenScaleY > ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_FLOOR) {
        work->st.flattenScaleY -= ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_STEP;
    } else {
        work->st.flattenStep = ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->st.flattenScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->st.savedRootMtx;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
/// Starts the actor's scripted animation selected by the request.
s32 func_actor_521100_801369B8(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    Task* dispatcher;

    if (args->animationId + 1 < 0xB) {
        D_actor_521100_8016A3D8->st.animId  = args->animationId + 1;
        dispatcher                          = D_actor_521100_8016A3DC;
        D_actor_521100_8016A3D8->st.state   = ACTOR_ENEMY_ANIM_RESET;
        D_actor_521100_8016A3D8->st.field_6 = 0;
        func_actor_521100_80135F2C(dispatcher);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler in `D_actor_521100_8016A358`: shows or hides the model of `D_actor_521100_8016A3DC`.
/// Bit 0 of `arg2` selects `TmdObject::flags` 0 (shown) or 0x80 (hidden), and
/// bit 1 ORs in 0x4.
s32 func_actor_521100_80136A1C(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = D_actor_521100_8016A3DC->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7D4 handler in `D_actor_521100_8016A358`, placing the actor: only
/// the yaw of the argument block's angles is used, kept in the work block's
/// `st.yaw` and applied with `gfxRotMatrixY`, then the position becomes the root
/// coordinate's translation and `composeStamp` is cleared.
s32 func_actor_521100_80136A64(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_521100_8016A3D8->st.yaw = yaw = placement->rot.vy;
    gfxRotMatrixY(&coord->coord, (s16)yaw, GRAPHICS_ROTATION_REPLACE);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB handler, listed in `D_actor_521100_8016A358` -- the
/// `{id, handler}` table the create body `func_actor_521100_80135DDC` installs
/// at `Task::msgTable`. `msg->field_2` picks the sub-command: 0 puts the task
/// back on its update state; 1 and 4 spawn one of the two companion tasks out
/// of the `D_actor_521100_8016A388` desc table into `D_actor_521100_8016A3E0` /
/// `D_actor_521100_8016A3E4`, leaving the state alone; 2 clears the flatten
/// step (`_Actor521100AnmcWomanState::flattenStep`) and sends the task to state 2, the
/// teardown entry `func_actor_521100_801360C4`; 3 kills both companions and
/// then falls into 0, sharing its state store.
s32 func_actor_521100_80136AE0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    switch (msg->command) {
        case 1:
            D_actor_521100_8016A3E0 = taskSpawnFromTable(D_actor_521100_8016A388, 1, 0, task);
            break;

        case 2:
            D_actor_521100_8016A3D8->st.flattenStep = ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN;
            task->state                             = 2;
            break;

        case 3:
            if (D_actor_521100_8016A3E0 != NULL) {
                taskKill(D_actor_521100_8016A3E0);
                D_actor_521100_8016A3E0 = NULL;
            }
            if (D_actor_521100_8016A3E4 != NULL) {
                taskKill(D_actor_521100_8016A3E4);
                D_actor_521100_8016A3E4 = NULL;
            }
            /* fall through -- the jump table's index 0 lands on the same store */

        case 0:
            task->state = 1;
            break;

        case 4:
            D_actor_521100_8016A3E4 = taskSpawnFromTable(D_actor_521100_8016A388, 1, 1, task);
            break;
    }
    return 0;
}
s32 func_actor_521100_80136BE8(Task* task, s32 arg1, ActorTransform* target, s32 arg3)
{
    GfxCoord* coord;
    s32       dx;
    s32       dz;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    dx                              = target->pos.vx - coord->coord.t[0];
    dz                              = target->pos.vz - coord->coord.t[2];
    yaw                             = ratan2(dx, dz);
    D_actor_521100_8016A3D8->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, GRAPHICS_ROTATION_REPLACE);
    D_actor_521100_8016A3D8->st.travel = SquareRoot0(dx * dx + dz * dz) / ACTOR_521100_ANMC_WOMAN_WALK_STRIDE;
    return 0;
}
