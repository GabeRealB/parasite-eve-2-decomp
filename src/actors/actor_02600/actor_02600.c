#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// Selects the Maggot collision identity, Caterpillar flag and behavior radii.
///
/// The first inclusion of the guarded shared header consumes this build-only
/// selector; the fragments use the derived constants after it is undefined.
#define MAGGOT_CATERPILLAR_KIND MAGGOT_CATERPILLAR_KIND_MAGGOT
#include "../../shared/creatures/maggot_caterpillar/maggot_caterpillar.h"
#undef MAGGOT_CATERPILLAR_KIND

enum { MAGGOT_CATERPILLAR_BODY_PRIORITY = 96 };

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gCrawlerIdleClip;
static AnimationSet _gCrawlerCrawlClip;
static AnimationSet _gCrawlerSprayClip;
static AnimationSet _gCrawlerPounceClip;
static AnimationSet _gCrawlerReboundClip;
static AnimationSet _gCrawlerHangClip;
static AnimationSet _gCrawlerDropClip;
static AnimationSet _gCrawlerLandClip;
static AnimationSet _gCrawlerGetUpClip;
static AnimationSet _gCrawlerHurtClip;
static AnimationSet _gCrawlerDormantClip;
static AnimationSet _gCrawlerWakeClip;
static AnimationSet _gCrawlerStunClip;
static TmdSource    _gCrawlerBody;

#include "assets/enemies/crawler/body.inc"

#include "../../shared/creatures/maggot_caterpillar/maggot_caterpillar_husk_model.inc.c"

#include "assets/enemies/crawler/maggot/package_1_clips.inc"

/// Contact attacks indexed by `MAGGOT_CATERPILLAR_ATTACK_*`.
///
/// Six entries, without a terminator: ordinary late/early bite, darkness puff,
/// burning late/early bite and ambush drop. Power is base HP damage before
/// combat scaling; reaction selects the player's hit presentation/status.
static DamageAttack _gMaggotCaterpillarAttacks[6] = {
    [MAGGOT_CATERPILLAR_ATTACK_BITE_LATE]          = { 14, GAME_ACTOR_REACTION_ORDINARY },
    [MAGGOT_CATERPILLAR_ATTACK_BITE_EARLY]         = { 20, GAME_ACTOR_REACTION_ORDINARY },
    [MAGGOT_CATERPILLAR_ATTACK_PUFF]               = { 10, GAME_ACTOR_REACTION_DARKNESS },
    [MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_LATE]  = { 16, MAGGOT_CATERPILLAR_ATTACK_REACTION_BODY_BLAST },
    [MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_EARLY] = { 24, MAGGOT_CATERPILLAR_ATTACK_REACTION_BODY_BLAST },
    [MAGGOT_CATERPILLAR_ATTACK_DROP]               = { 10, GAME_ACTOR_REACTION_ORDINARY },
};

/// Shared combat parameters for every live enemy of this carrier's kind.
///
/// The enemy borrows this record and its attack table while the package is
/// loaded. Rewards accrue on release; reaction counts are scaled by hit grade.
static EnemyParams _gMaggotCaterpillarParams = {
    .attacks              = _gMaggotCaterpillarAttacks,
    .hpMax                = 160,
    .exp                  = 16,
    .bp                   = 68,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 10,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 5,
};

/// Base idle ticks by placement row 0..7, before a random 0..15-tick spread.
static u16 _gMaggotCaterpillarIdleDelay[8] = {
    25,
    10,
    10,
    10,
    10,
    10,
    10,
    35,
};

/// Base crawl distance in coordinate units by placement row 0..7.
///
/// Roaming adds 0..1023 units and spends the distance by each tick's forward
/// speed; this is a distance budget rather than a timer.
static u16 _gMaggotCaterpillarRoamDistance[8] = {
    1600,
    2400,
    2400,
    2400,
    2400,
    2400,
    2400,
    1600,
};

/// Ambush downward speed in coordinate units per tick, by placement row 0..7.
static u16 _gMaggotCaterpillarDropSpeed[8] = {
    100,
    100,
    180,
    130,
    100,
    100,
    100,
    100,
};

/// Ticks to wait after the room releases a leap entrance, by variant 0..3.
///
/// Countdown starts on the next tick; zero still advances to the start phase
/// through that countdown rather than skipping the phase.
static s16 _gMaggotCaterpillarLeapInDelay[4] = {
    0,
    2,
    4,
    6,
};

/// Root positions in parent-coordinate units for already-cleared leap entrances.
///
/// Placement variant 0..3 selects the position used for a dormant revisit after
/// the forked-road event; these positions do not drive the live entrance leap.
static SVECTOR _gMaggotCaterpillarLeapInSpots[4] = {
    { -3000, 0, -600, 0 },
    { -3000, 0, -1600, 0 },
    { -2000, 0, -600, 0 },
    { -2000, 0, -1600, 0 },
};

/// Root yaw for already-cleared leap entrances, by variant 0..3; 4096 is a turn.
static s16 _gMaggotCaterpillarLeapInYaws[4] = {
    0,
    1024,
    2048,
    3072,
};

/// Ticks to wait after the room releases a drop entrance, by variant 0..3.
static s16 _gMaggotCaterpillarDropInDelay[4] = {
    5,
    10,
    15,
    20,
};

/// Scripted drop speed in coordinate units per tick, by entrance variant 0..3.
static s16 _gMaggotCaterpillarDropInSpeed[4] = {
    100,
    100,
    100,
    100,
};

/// Root positions in parent-coordinate units for already-cleared drop entrances.
///
/// Placement variant 0..3 selects the dormant revisit position after roof-garden
/// progress reaches 2; the live entrance instead drops from its placement.
static SVECTOR _gMaggotCaterpillarDropInSpots[4] = {
    { -7000, 0, -7600, 0 },
    { -5600, 0, -7500, 0 },
    { -4700, 0, -6500, 0 },
    { -5600, 0, -5600, 0 },
};

/// Root yaw for already-cleared drop entrances, by variant 0..3; 4096 is a turn.
static s16 _gMaggotCaterpillarDropInYaws[4] = {
    600,
    1024,
    2048,
    1800,
};

/// Initial pose-blend duration in ticks, indexed by `MAGGOT_CATERPILLAR_ANIM_*`.
///
/// Idle blends for ten ticks, spray and pounce for eight, stun for four; the
/// other clips start immediately. The stored run has sixteen halfwords, but
/// indices 0, 8 and 15 have no animation set and must not be requested.
static s16 _gMaggotCaterpillarAnimBlendFrames[16] = {
    0,
    10,
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
    0,
    0,
    4,
    0,
};

/// Nine intervals of forward displacement for the bite and entrance leaps.
///
/// Thresholds are inclusive clip frames plus the pounce blend duration; steps
/// are coordinate units per tick along the root's yaw.
static MaggotCaterpillarStrideStep _gMaggotCaterpillarPounceStride[9] = {
    { 20, 0 },
    { 21, 180 },
    { 22, 300 },
    { 30, 216 },
    { 35, 134 },
    { 38, 13 },
    { 40, 10 },
    { 43, 13 },
    { 50, 3 },
};

/// Nine intervals of signed forward displacement for a blocked bite's rebound.
///
/// Inclusive thresholds use rebound clip ticks directly. Negative coordinate
/// steps move backward along the root's yaw; no step is contributed after 18.
static MaggotCaterpillarStrideStep _gMaggotCaterpillarReboundStride[9] = {
    { 2, 250 },
    { 3, 10 },
    { 5, -30 },
    { 7, -55 },
    { 9, -80 },
    { 10, -300 },
    { 13, -97 },
    { 15, -55 },
    { 18, -10 },
};

/// Eight 32-pixel atlas-cell origins for the puff billboard.
///
/// Ages 0..14 select cells 0..7 by age / 2. Coordinates are relative to the
/// parent's texture page and CLUT; each cell covers offsets 0..31 on both axes.
static ActorSpriteUv _gMaggotCaterpillarPuffCells[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

/// Billboard half-size in coordinate units for puff ages 0..13.
///
/// Projection multiplies by 0x300 and divides by projected Z / 4. The retained
/// age-14 draw reads the adjacent body descriptor's flags halfword (1), beyond
/// this array's fourteen entries; the intended storage relationship is unproven.
static s16 _gMaggotCaterpillarPuffRadius[14] = {
    8,
    12,
    16,
    20,
    24,
    28,
    32,
    32,
    32,
    36,
    36,
    36,
    40,
    0,
};

TaskDesc gActor02600MaggotCaterpillarBodyTask = {
    .header   = { .fields = { TASK_BODY_TMD, MAGGOT_CATERPILLAR_BODY_PRIORITY } },
    .callback = _maggotCaterpillarTask,
    .data     = { .model = &_gCrawlerBody },
};

/// Coordinate-body spawn recipe for a puff child, at execution priority 96.
///
/// The puff borrows its live parent model for launch and atlas lookup. Its
/// callback allocates projectile work and dispatches setup, flight and teardown;
/// descriptor data is ignored for coordinate bodies. Keep it immediately after
/// the body recipe: the retained spawn table reaches it at index 1.
static TaskDesc _gMaggotCaterpillarPuffTaskDesc = {
    .header   = { .fields = { TASK_BODY_COORD, MAGGOT_CATERPILLAR_BODY_PRIORITY } },
    .callback = _maggotCaterpillarPuffTask,
    .data     = { .value = 0 },
};

/// Borrowed model clips indexed by `MAGGOT_CATERPILLAR_ANIM_*`.
///
/// Fifteen pointers cover indices 0..14; 0 and 8 are NULL and must not be
/// requested. The model and clip storage stay loaded throughout body playback.
#include "assets/enemies/crawler/maggot/bank.inc"

#include "../../shared/creatures/maggot_caterpillar/maggot_caterpillar_resolve_contacts.inc.c"

/// Selects this carrier's body/puff descriptor run for the shared spawn fragment.
///
/// Yields a `TaskDesc*` borrowed while the body lives; keep the package loaded
/// while body or puff tasks live. The retained scalar body declaration does
/// not cover index 1, which reaches the separately declared adjacent puff.
#define MAGGOT_CATERPILLAR_TASK_TABLE (&gActor02600MaggotCaterpillarBodyTask)
#include "../../shared/creatures/maggot_caterpillar/maggot_caterpillar.inc.c"

DEFINE_ALIAS(gActor02600MaggotCaterpillarBodyTask, gActor202600MaggotCaterpillarBodyTask);
