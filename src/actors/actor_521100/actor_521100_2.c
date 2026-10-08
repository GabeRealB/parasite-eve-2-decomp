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

#include "../../shared/actor_messages.h"
#include "../../shared/footstep_walk.h"

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

/// Borrowed ANMC-woman work published by spawn and refreshed by task dispatch.
///
/// Valid only while her task-owned allocation is live. Teardown does not clear
/// this singleton reference; animation and message handlers require the live task.
static _Actor521100AnmcWomanWork* _gActor521100AnmcWomanWork;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_521100_8016A358[6];
extern TaskDesc         D_actor_521100_8016A388[];
extern AnimationSet*    D_actor_521100_8016A3A0[11];

extern EffectSpawnArg D_actor_521100_8016A3CC;
extern u16            D_actor_521100_8016A3D4;

/// The task `_actor521100AnmcWomanTask` runs, stored by its create state
/// `_actor521100AnmcWomanSpawn`.
extern Task*    D_actor_521100_8016A3DC;
extern Task*    D_actor_521100_8016A3E0;
extern Task*    D_actor_521100_8016A3E4;
extern GfxCoord D_actor_521100_8016A3E8;

static void _actor521100AnmcWomanSpawn(Enemy* enemy, Task* task);
static void _actor521100AnmcWomanUpdate(Task* task);
static void _actor521100AnmcWomanFlattenState(Enemy* enemy, Task* task);
static void _actor521100UpdateAnmcWomanFlattenColor(Enemy* enemy, Task* task);
static void _actor521100TickAnmcWoman(Enemy* unusedEnemy, Task* task);
static void _actor521100AnmcWomanExit(Task* task);
static void _actor521100AnmcWomanTickAnim(void);
static void _actor521100AnmcWomanResetAnim(void);
static void _actor521100AnmcWomanBlendAnim(void);
static void _actor521100AnmcWomanFlatten(Task* task);

static s32  _actor521100AnmcWomanPlayAnim(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor521100AnmcWomanSetModelDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _actor521100AnmcWomanApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static s32  _actor521100AnmcWomanWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);
static void _actor521100AnmcWomanEffectTask(Task* task);
static void _actor521100AnmcWomanTask(Task* task);

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
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor521100AnmcWomanPlayAnim },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor521100AnmcWomanSetModelDrawFlags },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor521100AnmcWomanApplyCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor521100AnmcWomanWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_521100_8016A388[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor521100AnmcWomanTask, { .model = &_gActor521100AnmcWoman2Body } },
    { { { TASK_BODY_NONE, 192 } }, _actor521100AnmcWomanEffectTask, { .value = 0 } },
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

Task* D_actor_521100_8016A3DC;

Task* D_actor_521100_8016A3E0;

Task* D_actor_521100_8016A3E4;

GfxCoord D_actor_521100_8016A3E8;

s32 actor521100SetModelDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument)
{
    TmdObject*       obj;
    Actor521100Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    if (!(flags & ACTOR_MESSAGE_PAIR_SHOW)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    work->modelDrawFlags = flags;
    return 0;
}

s32 actor521100ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_521100_COMMAND_START_FIRE  = 0,
        ACTOR_521100_COMMAND_HIDE_WEAPON = 1,
        ACTOR_521100_FIRE_FULL           = 1
    };
    Actor521100Work* work;

    work = task->work;
    switch (request->command) {
        case ACTOR_521100_COMMAND_START_FIRE:
            work->eventBurnStage = ACTOR_521100_FIRE_FULL;
            work->stateCounter   = 0;
            work->stateElapsed   = 0;
            break;
        case ACTOR_521100_COMMAND_HIDE_WEAPON:
            work->weaponHidden = request->command;
            break;
    }
    return 0;
}

s32 actor521100Activate(Task* task, s32 messageId, s32 unusedFirstArgument, s32 unusedArgument)
{
    Actor521100Work* work;

    work            = task->work;
    work->activated = true;
    sceneAcquireBattleRef(0);
    return 0;
}

s32 actor521100IsPresent(Task* task, s32 messageId, s32 unusedFirstArgument, s32 unusedArgument)
{
    Actor521100Work* work;

    work = task->work;
    return work->present;
}

/// Binds the ANMC woman's animation context and requests her initial walk clip.
///
/// Requires her live published work and nineteen-part model allocation, plus
/// the loaded eleven-entry clip table. Borrows that allocation's coordinates,
/// work-owned encoded pose buffers and clip data for the lifetime of playback.
/// Requests a reset to clip 1, leaving the slots and pose buffers untouched;
/// the following animation update seeds slots 1..18 without advancing them.
static inline void _actor521100AnmcWomanPrepareAnimation(TmdObject* model)
{
    animationBindModelContext(&_gActor521100AnmcWomanWork->rig.anim, D_actor_521100_8016A3A0, model, _gActor521100AnmcWomanWork->rig.poses);
    _gActor521100AnmcWomanWork->st.animId = ACTOR_521100_ANMC_WOMAN_ANIM_WALK;
    _gActor521100AnmcWomanWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
}

/// Initializes and publishes the scripted ANMC woman's model, work and animation.
///
/// Requires a live Enemy and nineteen-part TMD model in task state 0. Allocates
/// zeroed task-owned work or destroys the enemy on failure. Parents the root to
/// the view, excludes lock-on, and samples lighting at the cached root XYZ with
/// Y minus 800 in world units. The model borrows work-owned lighting matrices;
/// animation borrows the loaded clip table, poses and coordinates. Installs
/// messages/exit, resets clip 1 without ticking it, then advances to state 1.
/// Singleton work/task references are valid only until teardown begins.
static void _actor521100AnmcWomanSpawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_521100_ANMC_WOMAN_OT_OFFSET             = 1,
        ACTOR_521100_ANMC_WOMAN_LIGHT_SAMPLE_Y_OFFSET = 800,
    };
    VECTOR3                    lightSamplePosition;
    _Actor521100AnmcWomanWork* work;
    TmdObject*                 model;
    GfxCoord*                  rootCoord;

    model                      = task->extra.tmd;
    rootCoord                  = model->coords;
    work                       = memCalloc(sizeof(*work), false);
    _gActor521100AnmcWomanWork = work;
    task->work                 = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback = _actor521100AnmcWomanExit;
    // Task-owned work backs matrices borrowed by the model and its playback rig.
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_521100_ANMC_WOMAN_OT_OFFSET;
    model->lightMtx                  = &_gActor521100AnmcWomanWork->light;
    model->colorMtx                  = &_gActor521100AnmcWomanWork->color;
    lightSamplePosition.vx           = rootCoord->workm.t[0];
    lightSamplePosition.vy           = rootCoord->workm.t[1] - ACTOR_521100_ANMC_WOMAN_LIGHT_SAMPLE_Y_OFFSET;
    D_actor_521100_8016A3DC          = task;
    lightSamplePosition.vz           = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightSamplePosition, 0, ARRAY_SIZE(model->colorMtx->m[0]));
    _actor521100AnmcWomanPrepareAnimation(model);
    task->msgTable = D_actor_521100_8016A358;
    _actor521100AnmcWomanUpdate(task);
    task->state += 1;
}

/// Applies a pending animation reseed or advances the ANMC woman's walk and poses.
///
/// Requires her published live work and an initialized nineteen-part rig.
/// Blend/reset states reseed and become tick without advancing a pose that call.
/// Tick moves only clip 1 with nonzero travel, by 20 parent-coordinate units
/// along local +Z, then advances parts 1..18. Frozen actors still consume travel
/// frames and animate, although their translation is suppressed. Other states
/// do nothing. Travel is a signed halfword and is tested for nonzero, not positive.
static void _actor521100AnmcWomanUpdate(Task* task)
{
    _Actor521100AnmcWomanWork* work;
    s16                        animId;

    work = _gActor521100AnmcWomanWork;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _actor521100AnmcWomanBlendAnim();
        _gActor521100AnmcWomanWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _actor521100AnmcWomanResetAnim();
        _gActor521100AnmcWomanWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        animId = work->st.animId;
        if (animId == ACTOR_521100_ANMC_WOMAN_ANIM_WALK && work->st.travel != 0) {
            // Preserve the countdown even when the movement helper suppresses translation.
            _actorMovementStepForward(task->extra.tmd->coords, ACTOR_521100_ANMC_WOMAN_WALK_STRIDE);
            _gActor521100AnmcWomanWork->st.travel--;
        }
        _actor521100AnmcWomanTickAnim();
        return;
    }
}
/// Shrinks and fades the ANMC woman while advancing her body poses.
///
/// State 2 requires live work, rig and model. BEGIN saves the yaw-only root
/// matrix at Q12 full height; SHRINK scales from that snapshot, enables
/// semitransparency at frame 10 and starts corpse burn at frame 15. The burn
/// uses a pre-update root snapshot with X/Z offsets -500/-100 in
/// parent-coordinate units. DONE leaves the final pose
/// and lighting untouched. Other steps tick slots 1..18 and refresh tint.
static void _actor521100AnmcWomanFlattenState(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_521100_ANMC_WOMAN_BURN_OFFSET_X = 500,
        ACTOR_521100_ANMC_WOMAN_BURN_OFFSET_Z = 100,
        ACTOR_521100_ANMC_WOMAN_BURN_BURSTS   = 5,
    };
    GfxCoord                   burnAnchor;
    TmdObject*                 model;
    GfxCoord*                  rootCoord;
    _Actor521100AnmcWomanWork* work;
    s32                        slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = task->work;
    // Burn placement copies this snapshot; its callbacks never follow the retained address.
    burnAnchor = *rootCoord;

    switch (work->st.flattenStep) {
        case ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN:
            work->st.flattenFrames = 0;
            work->st.flattenScaleY = ONE;
            gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
            work->st.savedRootMtx = rootCoord->coord;
            work->st.flattenStep  = ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK;
            break;

        case ACTOR_521100_ANMC_WOMAN_FLATTEN_SHRINK:
            _actor521100AnmcWomanFlatten(task);
            work->st.flattenFrames++;
            if (work->st.flattenFrames == ACTOR_521100_ANMC_WOMAN_FLATTEN_FADE_FRAMES) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->st.flattenFrames == ACTOR_521100_ANMC_WOMAN_FLATTEN_BURN_FRAMES) {
                // Preserve the root's parent link in the temporary effect anchor.
                burnAnchor.coord.t[0] -= ACTOR_521100_ANMC_WOMAN_BURN_OFFSET_X;
                burnAnchor.coord.t[2] -= ACTOR_521100_ANMC_WOMAN_BURN_OFFSET_Z;
                effectSpawn(EFFECT_CORPSE_BURN, &burnAnchor, ACTOR_521100_ANMC_WOMAN_BURN_BURSTS, NULL);
            }
            break;

        case ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE:
            return;
    }

    slotIndex = 1;
    do {
        animationTickDirectSlot(&_gActor521100AnmcWomanWork->rig.anim, &_gActor521100AnmcWomanWork->rig.slots[slotIndex]);
        slotIndex++;
    } while (slotIndex < (s32)ARRAY_SIZE(_gActor521100AnmcWomanWork->rig.slots));

    _actor521100UpdateAnmcWomanFlattenColor(enemy, task);
}
/// Relights the flattening ANMC woman and randomizes her colour-basis scale.
///
/// Requires her live singleton work and model part 1. Composes that part before
/// sampling actor colour. Three successive unsigned LCG draws scale the Q12
/// flatten amount by factors from 0.5 through just below 1.5; ScaleMatrixL
/// applies those per-axis weights to the colour basis, retaining its ambient
/// translation. Reserves one VECTOR on the initialized scratch stack and
/// releases it before return.
static void _actor521100UpdateAnmcWomanFlattenColor(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_521100_FLATTEN_COLOR_SAMPLE_PART  = 1,
        ACTOR_521100_FLATTEN_COLOR_RANDOM_BIAS  = 32768,
        ACTOR_521100_FLATTEN_COLOR_RANDOM_SCALE = 65536,
    };
    _Actor521100AnmcWomanWork* work;
    GfxCoord*                  sampleCoord;
    VECTOR*                    scratchPosition;

    sampleCoord                  = &task->extra.tmd->coords[ACTOR_521100_FLATTEN_COLOR_SAMPLE_PART];
    scratchPosition              = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = scratchPosition;
    actorRenderComposeCoord(sampleCoord);
    scratchPosition->vx = sampleCoord->workm.t[0];
    scratchPosition->vy = sampleCoord->workm.t[1];
    scratchPosition->vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, scratchPosition, 0, 0);
    // Apply independent Q12 channel weights after refreshing base lighting.
    work                = _gActor521100AnmcWomanWork;
    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    scratchPosition->vx = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + ACTOR_521100_FLATTEN_COLOR_RANDOM_BIAS) / ACTOR_521100_FLATTEN_COLOR_RANDOM_SCALE;
    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    scratchPosition->vy = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + ACTOR_521100_FLATTEN_COLOR_RANDOM_BIAS) / ACTOR_521100_FLATTEN_COLOR_RANDOM_SCALE;
    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    scratchPosition->vz = work->st.flattenScaleY * (s32)((gRandomLcgState >> 16) + ACTOR_521100_FLATTEN_COLOR_RANDOM_BIAS) / ACTOR_521100_FLATTEN_COLOR_RANDOM_SCALE;
    ScaleMatrixL(&work->color, scratchPosition);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Emits periodic effects anchored to the ANMC woman or the player.
///
/// `spawnArg1.value` is 0 for the woman and 1 for the player; `spawnArg2.pointer`
/// borrows the woman's live model task. Both selected models must remain live.
/// Every eight calls, starting at frame zero, copies the selected coordinate
/// into the shared effect anchor and emits the configured hit effect. Offsets
/// use that coordinate's parent space; the woman's Y offset includes a random
/// jitter of -100..99 units. At frame 131 kills this task and clears its slot.
/// `Task::state` counts elapsed frames and still increments after the kill.
static void _actor521100AnmcWomanEffectTask(Task* task)
{
    enum {
        ACTOR_521100_ANMC_WOMAN_EFFECT_AT_WOMAN      = 0,
        ACTOR_521100_ANMC_WOMAN_EFFECT_PERIOD_FRAMES = 8,
        ACTOR_521100_ANMC_WOMAN_EFFECT_END_FRAME     = 131,
        ACTOR_521100_ANMC_WOMAN_EFFECT_RANDOM_SCALE  = 0x10000,
        ACTOR_521100_ANMC_WOMAN_EFFECT_RANDOM_CENTER = 0x8000
    };
    Task* womanTask;

    womanTask = task->spawnArg2.pointer;
    if (!(task->state & (ACTOR_521100_ANMC_WOMAN_EFFECT_PERIOD_FRAMES - 1))) {
        // Copy the parent link with the local matrix before composing the effect anchor.
        if (task->spawnArg1.value == ACTOR_521100_ANMC_WOMAN_EFFECT_AT_WOMAN) {
            D_actor_521100_8016A3E8             = womanTask->extra.tmd->coords[1];
            D_actor_521100_8016A3E8.coord.t[2] += 0x32;
            gRandomLcgState                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_actor_521100_8016A3E8.coord.t[1] -= 0xFA + (s32)((gRandomLcgState >> 16) - ACTOR_521100_ANMC_WOMAN_EFFECT_RANDOM_CENTER) * 0xC8 / ACTOR_521100_ANMC_WOMAN_EFFECT_RANDOM_SCALE;
            D_actor_521100_8016A3E8.coord.t[0] -= 0x32;
        } else {
            D_actor_521100_8016A3E8             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[0];
            D_actor_521100_8016A3E8.coord.t[1] -= 0x384;
            D_actor_521100_8016A3E8.coord.t[0] += 0x2BC;
        }
        D_actor_521100_8016A3E8.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&D_actor_521100_8016A3E8);
        D_actor_521100_8016A3CC.coord = &D_actor_521100_8016A3E8;
        effectSpawnHit(D_actor_521100_8016A3D4, &D_actor_521100_8016A3E8, NULL, &D_actor_521100_8016A3CC);
    }
    if (task->state >= ACTOR_521100_ANMC_WOMAN_EFFECT_END_FRAME) {
        taskKill(task);
        if (task->spawnArg1.value == ACTOR_521100_ANMC_WOMAN_EFFECT_AT_WOMAN) {
            D_actor_521100_8016A3E0 = NULL;
        } else {
            D_actor_521100_8016A3E4 = NULL;
        }
    }
    task->state++;
}
/// State table the overlay dispatches through, indexed by `Task::state`:
/// create, update and flatten.
static const EnemyTaskFuncTable3 D_actor_521100_80131E68 = { {
    _actor521100AnmcWomanSpawn,
    _actor521100TickAnmcWoman,
    _actor521100AnmcWomanFlattenState,
} };

/// Runs the ANMC woman's create, update or flatten task state.
///
/// `Task::state` must be 0, 1 or 2 respectively. The second spawn argument
/// borrows her live `Enemy`. Publishes `Task::work` before dispatch; the create
/// state establishes the allocation, and later callbacks require it to remain
/// live. The published pointer is not cleared by the exit callback.
static void _actor521100AnmcWomanTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;
    // Filled here and then neither read nor passed on, so what the record is
    // for is unproven, as is the signedness of its fields. The stores establish
    // these four bytes; the frame has room for up to four more behind them.
    struct {
        u8  field_0;
        u8  field_1;
        u16 field_2;
    } unread;

    stateHandlers              = D_actor_521100_80131E68;
    unread.field_0             = 2;
    unread.field_1             = 9;
    unread.field_2             = 1;
    _gActor521100AnmcWomanWork = task->work;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Composes and lights the ANMC woman before advancing her behavior.
///
/// Requires initialized work/model and the published singleton work pointer.
/// Lighting samples the composed position with Y lowered by 800 game units.
/// The update may subsequently change the pose; the Enemy argument is unused.
static void _actor521100TickAnmcWoman(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_521100_ANMC_WOMAN_LIGHT_Y_OFFSET = 800,
    };
    TmdObject* model;
    GfxCoord*  rootCoord;
    VECTOR     lightingPosition;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1] - ACTOR_521100_ANMC_WOMAN_LIGHT_Y_OFFSET;
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, 3);
    _actor521100AnmcWomanUpdate(task);
}

/// Releases the ANMC woman's borrowed enemy and begins model-task teardown.
///
/// Installed as the model task's exit callback; `spawnArg2.pointer` must still
/// address its live `Enemy`. Leaves the singleton task/work pointers unchanged,
/// so message dispatch must cease when teardown starts.
static void _actor521100AnmcWomanExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Advances the ANMC woman's initialized animation slots 1..18 into their coordinates.
///
/// Requires the published live rig and animation scratch/GTE state. Each slot's
/// track index equals its array index: the direct API recovers the array base
/// from that index and stores it in the context. Root slot 0 is not animated.
static void _actor521100AnmcWomanTickAnim(void)
{
    s32 slotIndex;

    slotIndex = 1;
    do {
        animationTickDirectSlot(&_gActor521100AnmcWomanWork->rig.anim, &_gActor521100AnmcWomanWork->rig.slots[slotIndex]);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(_gActor521100AnmcWomanWork->rig.slots));
}

/// Restarts the ANMC woman's nonroot tracks and records the requested clip.
///
/// Requires the published live rig bound to the eleven-entry set table.
/// Each direct reset maps track and coordinate to the slot index, normalizes
/// zero to set 1 and negative IDs to their magnitude, and sets normal rate.
/// Retains the preliminary rate store of 1/16 frame, overwritten by the reset.
/// `st.appliedAnimId` records the request, not its normalized ID; no pose is ticked.
static void _actor521100AnmcWomanResetAnim(void)
{
    s32 slotIndex;

    slotIndex = 1;
    do {
        _gActor521100AnmcWomanWork->rig.slots[slotIndex].rate = FOOTSTEP_WALK_PRE_RESET_RATE;
        animationInitDirectSlot(&_gActor521100AnmcWomanWork->rig.anim, &_gActor521100AnmcWomanWork->rig.slots[slotIndex], slotIndex,
                                _gActor521100AnmcWomanWork->st.animId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(_gActor521100AnmcWomanWork->rig.slots));
    _gActor521100AnmcWomanWork->st.appliedAnimId = _gActor521100AnmcWomanWork->st.animId;
}

/// Starts the ANMC woman's nonroot tracks with an eight-frame demo transition.
///
/// Requires the published live rig with initialized slots and a valid requested
/// set. Demo scene 1 captures the current poses and blends to each track's first
/// record; other scenes reset with zero/negative-ID normalization and ignore the
/// blend duration. Records the requested ID in `st.appliedAnimId` afterwards.
static void _actor521100AnmcWomanBlendAnim(void)
{
    s32 slotIndex;

    slotIndex = 1;
    do {
        animationStartDirectSlot(&_gActor521100AnmcWomanWork->rig.anim, &_gActor521100AnmcWomanWork->rig.slots[slotIndex], slotIndex,
                                 _gActor521100AnmcWomanWork->st.animId, 0, FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(_gActor521100AnmcWomanWork->rig.slots));
    _gActor521100AnmcWomanWork->st.appliedAnimId = _gActor521100AnmcWomanWork->st.animId;
}

/// Restores the woman's saved root and applies a Q12 Y scale without compounding it.
///
/// Borrows live work, root and reserved scratch storage. Translation is restored
/// with the matrix; only rotation is scaled. The caller releases the scratch.
static __inline__ void _actor521100AnmcWomanRescaleRoot(GfxCoord* rootCoord, const _Actor521100AnmcWomanWork* work,
                                                        ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->st.flattenScaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->st.savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Lowers the ANMC woman's root Y scale and finishes her flatten at its floor.
///
/// Requires her live work, root coordinate and matrix saved at flatten start.
/// Height is Q12 (ONE is full height): subtracts 16 while above 256, otherwise
/// marks the flatten done. Restores the saved matrix before scaling to prevent
/// accumulation; X/Z stay at unit scale. Invalidates composition and releases
/// one `ActorScaleScratch` from the initialized scratch stack before return.
static void _actor521100AnmcWomanFlatten(Task* task)
{
    ActorScaleScratch*         scratchHead;
    ActorScaleScratch*         scratch;
    _Actor521100AnmcWomanWork* work;
    GfxCoord*                  rootCoord;

    scratchHead = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work        = task->work;
    scratch     = (SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratchHead - 1);
    rootCoord   = task->extra.tmd->coords;
    if (work->st.flattenScaleY > ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_FLOOR) {
        work->st.flattenScaleY -= ACTOR_521100_ANMC_WOMAN_FLATTEN_SCALE_STEP;
    } else {
        work->st.flattenStep = ACTOR_521100_ANMC_WOMAN_FLATTEN_DONE;
    }
    _actor521100AnmcWomanRescaleRoot(rootCoord, work, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
/// Immediately restarts the ANMC woman's animation from a borrowed request.
///
/// Requests 0..9 select local sets 1..10. Only the upper bound is checked;
/// negative values retain the direct reset API's normalization after narrowing
/// to a signed halfword. Requires her published live task and work. Ignores the
/// request's source, blend and collision options, the receiver, message ID and
/// second payload; retains no request pointer. Returns zero after a reset, or
/// -1 for an ID rejected by the upper-bound check.
static s32 _actor521100AnmcWomanPlayAnim(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    Task* dispatcher;

    if (request->animationId + 1 < ARRAY_SIZE(D_actor_521100_8016A3A0)) {
        _gActor521100AnmcWomanWork->st.animId  = request->animationId + 1;
        dispatcher                             = D_actor_521100_8016A3DC;
        _gActor521100AnmcWomanWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
        _gActor521100AnmcWomanWork->st.field_6 = 0;
        _actor521100AnmcWomanUpdate(dispatcher);
        return 0;
    }
    return -1;
}

/// Replaces the published ANMC woman's model flags from a draw request.
///
/// Requires her live published model task. `ACTOR_MESSAGE_PAIR_SHOW` clears
/// the model flags; without it, sets only active-draw exclusion. The
/// SKIP_AUTO_BUFFER request adds that flag; other request bits are ignored.
/// Allocates and frees no buffers. Ignores the receiver, message ID and second
/// payload; returns zero.
static s32 _actor521100AnmcWomanSetModelDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument)
{
    TmdObject* obj;

    obj = D_actor_521100_8016A3DC->extra.tmd;
    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Placement records yaw in the live ANMC woman, independently of footstep-walker state.
#undef FOOTSTEP_WALK_PLACE_WORK
#define FOOTSTEP_WALK_PLACE_WORK _gActor521100AnmcWomanWork
#include "../../shared/footstep_walk_place.inc.c"
#undef FOOTSTEP_WALK_PLACE_WORK
#define FOOTSTEP_WALK_PLACE_WORK _gFootstepWalkWork

/// Applies a scene command to the ANMC woman and her two periodic effect emitters.
///
/// Requires her live published work and a readable ActorCommand; retains no
/// payload pointer. Commands 0/3 select ordinary update, with 3 first killing
/// both emitters. Command 2 restarts flattening. Commands 1/4 spawn an emitter
/// at the woman/player, overwriting its slot without killing a previous task;
/// spawn failure is retained as NULL. Other commands do nothing. Ignores the
/// message ID and second payload. Returns zero.
static s32 _actor521100AnmcWomanApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        ACTOR_521100_ANMC_WOMAN_COMMAND_UPDATE        = 0,
        ACTOR_521100_ANMC_WOMAN_COMMAND_EFFECT_WOMAN  = 1,
        ACTOR_521100_ANMC_WOMAN_COMMAND_FLATTEN       = 2,
        ACTOR_521100_ANMC_WOMAN_COMMAND_STOP_EFFECTS  = 3,
        ACTOR_521100_ANMC_WOMAN_COMMAND_EFFECT_PLAYER = 4,
        ACTOR_521100_ANMC_WOMAN_TASK_UPDATE           = 1,
        ACTOR_521100_ANMC_WOMAN_TASK_FLATTEN          = 2,
        ACTOR_521100_ANMC_WOMAN_EFFECT_TASK_INDEX     = 1,
        ACTOR_521100_ANMC_WOMAN_EFFECT_WOMAN          = 0,
        ACTOR_521100_ANMC_WOMAN_EFFECT_PLAYER         = 1,
    };
    switch (command->command) {
        case ACTOR_521100_ANMC_WOMAN_COMMAND_EFFECT_WOMAN:
            D_actor_521100_8016A3E0 = taskSpawnFromTable(D_actor_521100_8016A388, ACTOR_521100_ANMC_WOMAN_EFFECT_TASK_INDEX, ACTOR_521100_ANMC_WOMAN_EFFECT_WOMAN, task);
            break;

        case ACTOR_521100_ANMC_WOMAN_COMMAND_FLATTEN:
            _gActor521100AnmcWomanWork->st.flattenStep = ACTOR_521100_ANMC_WOMAN_FLATTEN_BEGIN;
            task->state                                = ACTOR_521100_ANMC_WOMAN_TASK_FLATTEN;
            break;

        case ACTOR_521100_ANMC_WOMAN_COMMAND_STOP_EFFECTS:
            if (D_actor_521100_8016A3E0 != NULL) {
                taskKill(D_actor_521100_8016A3E0);
                D_actor_521100_8016A3E0 = NULL;
            }
            if (D_actor_521100_8016A3E4 != NULL) {
                taskKill(D_actor_521100_8016A3E4);
                D_actor_521100_8016A3E4 = NULL;
            }
            // Resume ordinary update after both emitters have been stopped.

        case ACTOR_521100_ANMC_WOMAN_COMMAND_UPDATE:
            task->state = ACTOR_521100_ANMC_WOMAN_TASK_UPDATE;
            break;

        case ACTOR_521100_ANMC_WOMAN_COMMAND_EFFECT_PLAYER:
            D_actor_521100_8016A3E4 = taskSpawnFromTable(D_actor_521100_8016A388, ACTOR_521100_ANMC_WOMAN_EFFECT_TASK_INDEX, ACTOR_521100_ANMC_WOMAN_EFFECT_PLAYER, task);
            break;
    }
    return 0;
}
/// Faces a target and records whole frames of travel for the ANMC woman's walk.
///
/// Requires her published live work and the receiver's model root. Borrows only
/// target X/Z in the root parent's coordinate units; Y and rotation are ignored.
/// Stores yaw in 1/4096 turns, replacing pitch, roll and scale, and narrows
/// floor(planar distance / 20) to signed-halfword travel. Squared offsets must
/// fit signed 32-bit arithmetic; normal travel must fit 0..32767 frames.
/// Does not select an animation: movement occurs only while clip 1 is ticking.
/// Ignores the message ID and second payload; retains no pointer. Returns zero.
static s32 _actor521100AnmcWomanWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument)
{
    GfxCoord* rootCoord;
    s32       offsetX;
    s32       offsetZ;
    s16       yaw;

    rootCoord                          = task->extra.tmd->coords;
    offsetX                            = target->pos.vx - rootCoord->coord.t[0];
    offsetZ                            = target->pos.vz - rootCoord->coord.t[2];
    yaw                                = ratan2(offsetX, offsetZ);
    _gActor521100AnmcWomanWork->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    // Travel is truncated to whole frames; the update keeps the original nonzero test.
    _gActor521100AnmcWomanWork->st.travel = SquareRoot0(offsetX * offsetX + offsetZ * offsetZ) / ACTOR_521100_ANMC_WOMAN_WALK_STRIDE;
    return 0;
}
