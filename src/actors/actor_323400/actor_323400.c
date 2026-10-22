#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor/actor_messages.h"

/// Selects the scripted, noncombat Desert Chaser implementation for actor_323400.
///
/// Keep this preprocessor binding fixed through the shared header and fragments;
/// it selects the work layout and three lifecycle handlers, with no combat bodies.
#define DESERT_CHASER_BUILD DESERT_CHASER_CUTSCENE
#include "../../shared/creatures/desert_chaser/desert_chaser.h"

/// Psy-Q `RotMatrixY`.

/// Whole-unit step `_actorContactApplyGridPushback` last applied to its coordinate.
static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

static DesertChaserEffectArgStorage _gDesertChaserEffectArg;

static void _actor323400PlayClip13State(Enemy* enemy, Task* task);
static void _actor323400PlaySceneAnimation(Enemy* enemy, Task* task);
static void _actor323400BlendClip2State(Enemy* enemy, Task* task);

#include "../../shared/actor/actor_contacts.h"

/// Scripted behavior handlers indexed by `DesertChaserWork::state` in 0..3.
///
/// Slots are hidden, requested scene animation, clip 13, then blend clip 2.
/// Every handler receives the live Enemy and Task; the frame driver copies the
/// complete four-entry value before dispatch.
static const EnemyTaskFuncTable4 _gDesertChaserStates = { {
    _desertChaserHideState,
    _actor323400PlaySceneAnimation,
    _actor323400PlayClip13State,
    _actor323400BlendClip2State,
} };

/// Lifecycle handlers indexed by `Task::state`: spawn, frame update, destruction.
///
/// The dispatcher copies all three Enemy/Task callbacks; state must stay in 0..2.
static const DesertChaserTaskStates _gDesertChaserTaskStates = { {
    _desertChaserSpawn,
    _desertChaserFrameState,
    enemyDestroy,
} };

static TmdSource _gChaserDesertBody;
static s32       _actor323400ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedArg);
static void      _actor323400IgnoreMessage2015(Task* unusedTask, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg);

/// Five authored attack records retained by the scripted chaser's enemy parameters.
///
/// Entries carry ordinary-hit reaction 0 and powers 30, 30, 18, 18 and 65535.
/// This noncombat build installs the parameters but never delivers these attacks;
/// the final power has no established sentinel meaning.
static DamageAttack _gDesertChaserAttacks[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

/// Enemy-kind parameters retained by the scripted Desert Chaser.
///
/// Spawn borrows this record in Enemy.param but makes the actor non-lockable
/// with zero live HP and no combat contacts. Nominal HP, reward and reaction
/// values remain authored data; the five attack records are not delivered by
/// this build. Keep this carrier loaded until its enemy is destroyed.
static EnemyParams _gDesertChaserParams = {
    .attacks              = _gDesertChaserAttacks,
    .hpMax                = 200,
    .exp                  = 75,
    .bp                   = 50,
    .mp                   = 4,
    .critChance           = 100,
    .buildupSteps         = 10,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

/// Four armed Desert Chaser timing tunings retained by this scripted build.
///
/// Down time, windup, roam-look delay and pursuit holdoff are whole update
/// counts. This noncombat build never selects a variant; the armed builds use
/// the same four records. Preserve their original halfword order and extent.
static DesertChaserVariant _gDesertChaserUnusedVariants[4] = {
    { 60, 36, 10, 150 },
    { 40, 26, 10, 120 },
    { 20, 18, 10, 120 },
    { 60, 60, 10, 150 },
};

#include "assets/enemies/chaser/desert/body.inc"

#include "assets/rooms/dryfield/breezeway/desert_chaser_3_clips.inc"

/// Whole normal-rate blend frames indexed by previous and requested clip IDs.
///
/// Both IDs must be in 0..44 and select loaded tracks. Signed bytes promote
/// to the blend duration; zero switches without a timed blend. This carrier
/// supplies its own table to the included Desert Chaser animation driver.
static s8 _gDesertChaserClipBlendFrames[45][45] = {
    /*  0 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  1 */ { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  2 */ { 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  3 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  4 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  5 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  6 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  7 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  8 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  9 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 10 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 11 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 12 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 13 */ { 5, 7, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 14 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 15 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 16 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 17 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 18 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 19 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 20 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 21 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 22 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 23 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 24 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 25 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 26 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 27 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 28 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 29 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 30 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 31 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 32 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 33 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 34 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 35 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 36 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 37 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 38 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 39 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 40 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 41 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 42 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 43 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 44 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// This carrier's scripted Desert Chaser animation-source storage.
static DesertChaserAnimationSourceStorage _gDesertChaserAnimationSource = {
    {
        &_gChaserDesertWalkClip,
        &_gChaserDesertIdleClip,
        &_gChaserDesertWindupClip,
        &_gChaserDesertLungeClip,
        &_gChaserDesertLungeEndClip,
        &_gChaserDesertThrowClip,
        &_gChaserDesertLeapBackClip,
        &_gChaserDesertSteerClip,
        &_gChaserDesertResumePursuitClip,
        &_gChaserDesertFlinchLightClip,
        &_gChaserDesertKnockDownClip,
        &_gChaserDesertAnim11Clip,
        &_gChaserDesertRiseClip,
        &_gDryfieldBreezewayChaserOpeningSceneClip,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &_gStingerPuppetPlayerKnockbackFrontClip,
        &_gStingerPuppetPlayerKnockbackRecoverFrontClip,
        &_gAyaHitHeadClip,
        NULL,
        NULL,
        &_gStingerPuppetPlayerKnockbackBackClip,
        &_gStingerPuppetPlayerKnockbackRecoverBackClip,
        &_gAyaHitBodyClip,
    },
    {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        3,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        60,
        0,
        244,
        255,
        30,
        0,
        2,
        0,
        206,
        255,
        126,
        255,
        29,
        0,
        2,
        0,
        20,
        0,
        186,
        255,
        25,
        0,
        2,
        0,
        226,
        255,
        191,
        255,
        25,
        0,
        2,
        0,
        60,
        0,
        136,
        255,
        30,
        0,
        2,
        0,
        20,
        0,
        236,
        255,
        251,
        255,
        2,
        0,
        241,
        255,
        206,
        255,
        0,
        0,
        2,
        0,
        2,
        0,
        10,
        0,
        241,
        255,
        2,
        0,
        14,
        0,
        0,
        0,
        0,
        0,
        7,
        0,
        25,
        0,
        0,
        0,
        0,
        0,
        2,
        0,
        242,
        255,
        0,
        0,
        0,
        0,
        9,
        0,
        231,
        255,
        0,
        0,
        0,
        0,
        2,
        0,
    },
};

enum { ACTOR_323400_MESSAGE_NO_OP = 2015 };

/// Scripted chaser messages, terminated by `TASK_MESSAGE_TABLE_END`.
///
/// Dispatch accepts the no-op, drawing, presence, yaw placement, scene command
/// and animation request messages. Pointer payloads are borrowed for the call.
static TaskMessageEntry _gDesertChaserMessages[7] = {
    { ACTOR_323400_MESSAGE_NO_OP, _actor323400IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _desertChaserSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor323400ApplyCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _desertChaserMsgPlayAnim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { ACTOR_323400_DESERT_CHASER_TASK_PRIORITY = 96 };

TaskDesc gActor323400DesertChaserTaskDesc = {
    .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_323400_DESERT_CHASER_TASK_PRIORITY } },
    .callback = _desertChaserTask,
    .data     = { .model = &_gChaserDesertBody },
};

static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &(ActorContact_ScratchPosition.step);
}

static DesertChaserEffectArgStorage _gDesertChaserEffectArg;

#include "../../shared/actor/actor_contacts.h"

#include "../../shared/actor/actor_contacts.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_blend_tick.inc.c"

/// Emits one-shot animation dust cues and returns an EVT sound script, or zero.
///
/// Walking tests slots 9, 7, 14 and 17; other clips test slot 1. The low ten pose record-index bits select a
/// cue; per-slot history suppresses repeats until a non-cue clears the history.
/// The task and work must belong to the same live chaser, with slots and model
/// parts 0..17 available. Dust offsets use model-part units and are borrowed
/// for placement; dust playback does not follow the retained offset pointer.
/// Sound placement and panning belong to the caller.
static s32 _desertChaserAnimCues(Task* task, DesertChaserWork* work)
{
    SVECTOR effectOffset;
    s32     resetCueHistory;
    resetCueHistory = 1;
    // The low ten record-index bits identify cues, not elapsed animation frames.
    switch (work->animId) {
        case 0: {
            s32 cueIndex = work->rig.slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x58) {
                previousCueIndex = work->lastCueFrames[9];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[9] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[9] = previousCueIndex;
                resetCueHistory        = 0;
            }
        }
            {
                s32 cueIndex = work->rig.slots[7].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x3E) {
                    previousCueIndex = work->lastCueFrames[7];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[7] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_1;
                    }
                    work->lastCueFrames[7] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x84) {
                    previousCueIndex = work->lastCueFrames[14];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[14] = cueIndex;
                        effectOffset.vz         = 0;
                        effectOffset.vx         = 0;
                        effectOffset.vy         = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_2;
                    }
                    work->lastCueFrames[14] = previousCueIndex;
                    resetCueHistory         = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[17].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xA9) {
                    previousCueIndex = work->lastCueFrames[17];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[17] = cueIndex;
                        effectOffset.vz         = 0;
                        effectOffset.vx         = 0;
                        effectOffset.vy         = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_1;
                    }
                    work->lastCueFrames[17] = previousCueIndex;
                    resetCueHistory         = 0;
                }
            }
            break;

        case 10: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(0)) {
                        effectSpawn(EFFECT_DUST_PUFF, task->extra.tmd->coords, (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 2560), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_05;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
        } break;

        case 3: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x4) {
                resetCueHistory  = 0;
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    return DESERT_CHASER_SOUND_CUE_04;
                }
                work->lastCueFrames[1] = previousCueIndex;
            }
        }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x8) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        return DESERT_CHASER_SOUND_CUE_03;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            break;

        case 6: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return 0;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
        }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xB) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xC) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 576), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 768), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 832), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xD) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 768), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            break;
    }

    // An intervening non-cue record rearms the one-shot cue history.
    if (resetCueHistory == 1) {
        memFillBytes(work->lastCueFrames, 0, sizeof(work->lastCueFrames));
    }
    return 0;
}

#include "../../shared/creatures/desert_chaser/desert_chaser_anim_tick.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_spawn.inc.c"

/// Emits the scene clip's counter-driven dust and placement-channel sound cues.
///
/// Requires matching live enemy/work and model coordinates 0..17. Increments the
/// wrapping s16 timer before cues at 9/10/12/13 ticks. Limb dust requires enabled
/// room effects; tick 13's body dust and tick 10's placement-tagged sound always
/// run. Audio pan/depth narrow to signed bytes. Dust options pack twelve-bit size
/// and four-bit frame period; the dust task draws from copied part-local offsets.
static inline void _actor323400EmitClip13TimedCues(Enemy* enemy, Task* task, DesertChaserWork* work)
{
    enum {
        ACTOR_323400_SCENE_PART_9_DUST_TICK  = 9,
        ACTOR_323400_SCENE_SOUND_TICK        = 10,
        ACTOR_323400_SCENE_PART_14_DUST_TICK = 12,
        ACTOR_323400_SCENE_LAST_DUST_TICK    = 13
    };
    enum { ACTOR_323400_CLIP_13_SOUND_SCRIPT = 0x4001000E };

    s32     soundScript;
    s32     soundPan;
    SVECTOR bodyDustOffset;
    SVECTOR limbDustOffset;
    switch (++work->stateTimer) {
        case ACTOR_323400_SCENE_PART_9_DUST_TICK: {
            SVECTOR* dustOffset = &limbDustOffset;
            limbDustOffset.vz   = 0;
            dustOffset->vx      = 0;
            dustOffset->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1024), dustOffset);
            }
            break;
        }
        case ACTOR_323400_SCENE_SOUND_TICK: {
            SVECTOR* dustOffset = &limbDustOffset;
            limbDustOffset.vz   = 0;
            dustOffset->vx      = 0;
            dustOffset->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1024), dustOffset);
            }
            soundScript = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_323400_CLIP_13_SOUND_SCRIPT;
            soundPan    = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundScript, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        }
        case ACTOR_323400_SCENE_PART_14_DUST_TICK: {
            SVECTOR* dustOffset = &limbDustOffset;
            limbDustOffset.vz   = 0;
            dustOffset->vx      = 0;
            dustOffset->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1536), dustOffset);
            }
            break;
        }
        case ACTOR_323400_SCENE_LAST_DUST_TICK: {
            SVECTOR* dustOffset = &limbDustOffset;
            limbDustOffset.vz   = 0;
            dustOffset->vx      = 0;
            dustOffset->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1280), dustOffset);
            }
            limbDustOffset.vz = 0;
            dustOffset->vx    = 0;
            dustOffset->vy    = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), dustOffset);
            }
            bodyDustOffset.vy = 0x3E8;
            bodyDustOffset.vx = 0;
            bodyDustOffset.vz = -0x12C;
            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[1], (DESERT_CHASER_CUE_DUST_RECURSIVE | (5 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 2304), &bodyDustOffset);
            break;
        }
    }
}

/// Plays Breezeway scene clip 13 with timed dust and a placement-selected sound.
///
/// Requires live cutscene work, matching enemy and model parts 0..17. State-2
/// entry disables locking, enables drawing and buffers, and resets clip/timer
/// at normal rate. Later calls increment the wrapping halfword timer first:
/// counts 9/10/12/13 emit limb dust only while room effects are enabled, count
/// 10 requests placement-channel sound script 14, and count 13 always emits
/// body dust. Animation ticks after these cues, then the root is marked dirty.
/// Offsets use part-local units; dust arguments pack twelve-bit size and four-bit period.
static void _actor323400PlayClip13State(Enemy* enemy, Task* task)
{
    enum { ACTOR_323400_SCENE_CLIP_13 = 13 };

    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate       = ANIMATION_RATE_ONE;
        work->animId         = ACTOR_323400_SCENE_CLIP_13;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        work->stateTimer     = 0;
        _desertChaserAnimTick(task);
        return;
    }
    // These scene effects sample the previous pose before animation advances.
    _actor323400EmitClip13TimedCues(enemy, task, work);
    _desertChaserAnimTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/creatures/desert_chaser/desert_chaser_frame.inc.c"

/// Ignores the cutscene chaser's message 2015 without changing any state.
///
/// All arguments are unused. The result register is left unspecified; senders
/// must ignore the dispatch result.
static void _actor323400IgnoreMessage2015(Task* unusedTask, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg)
{
}

#include "../../shared/creatures/desert_chaser/desert_chaser_visibility.inc.c"

#include "../../shared/actor/actor_messages_is_present.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_place_yaw_first.inc.c"

/// Applies the Breezeway cutscene chaser's stage/area command.
///
/// Borrows a complete command for this call and requires initialized work/model.
/// Always records stage, area and the low command byte. Dryfield/Breezeway
/// commands 0/2 hide; 1 places the root at (17200, 1, 2700) parent-coordinate
/// units, dirties composition and starts clip 13's state. Other contexts or
/// selectors leave placement and state intact. Returns 0; the message ID and
/// second payload are unused.
static s32 _actor323400ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_323400_COMMAND_CONTEXT        = (GAME_AREA_DRYFIELD_BREEZEWAY << 8) | GAME_STAGE_DRYFIELD,
        ACTOR_323400_COMMAND_HIDE           = 0,
        ACTOR_323400_COMMAND_PLAY_CLIP_13   = 1,
        ACTOR_323400_COMMAND_HIDE_ALTERNATE = 2,
        ACTOR_323400_STATE_HIDDEN           = 0,
        ACTOR_323400_STATE_PLAY_CLIP_13     = 2,
        ACTOR_323400_SCENE_ROOT_X           = 17200,
        ACTOR_323400_SCENE_ROOT_Z           = 2700
    };
    DesertChaserWork* work;
    u16               commandSelector;

    work = task->work;

    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = (u8)command->command;

    if (command->context.key == ACTOR_323400_COMMAND_CONTEXT) {
        commandSelector = command->command;
        switch (commandSelector) {
            case ACTOR_323400_COMMAND_PLAY_CLIP_13:
                task->extra.tmd->coords->coord.t[0]   = ACTOR_323400_SCENE_ROOT_X;
                task->extra.tmd->coords->coord.t[1]   = commandSelector;
                task->extra.tmd->coords->coord.t[2]   = ACTOR_323400_SCENE_ROOT_Z;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_323400_STATE_PLAY_CLIP_13;
                break;
            case ACTOR_323400_COMMAND_HIDE:
            case ACTOR_323400_COMMAND_HIDE_ALTERNATE:
                work->state = ACTOR_323400_STATE_HIDDEN;
                break;
        }
    }
    return 0;
}

#include "../../shared/creatures/desert_chaser/desert_chaser_play_anim.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_exit.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_part_effect.inc.c"

#include "../../shared/creatures/desert_chaser/desert_chaser_hide.inc.c"

/// Resets the Breezeway chaser's selected scene clip and advances it each call.
///
/// Requires live cutscene work, matching enemy and an eighteen-part model.
/// State-1 entry clears target/model flags, allocates primitive buffers and resets playback
/// at normal rate and zeroes both turn targets. Every call then ticks animation;
/// the selected clip and the rig's borrowed model/data must remain live.
static void _actor323400PlaySceneAnimation(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate       = ANIMATION_RATE_ONE;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(task);
    } else {
        _desertChaserAnimTick(task);
    }
}

/// Blends the Breezeway chaser into clip 2 and advances its scene animation.
///
/// Requires live cutscene work, matching enemy and an eighteen-part model.
/// State-3 entry disables locking, enables drawing and buffers, and requests
/// clip 2 at normal rate with zero turn targets. The animation driver uses the
/// previous-to-next clip table's blend length. Later calls tick animation and
/// mark the root dirty; entry does not perform that final dirty store.
static void _actor323400BlendClip2State(Enemy* enemy, Task* task)
{
    enum { ACTOR_323400_SCENE_CLIP_2 = 2 };

    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate       = ANIMATION_RATE_ONE;
        work->animId         = ACTOR_323400_SCENE_CLIP_2;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(task);
    } else {
        _desertChaserAnimTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

#include "../../shared/creatures/desert_chaser/desert_chaser_task.inc.c"

#undef DESERT_CHASER_BUILD
