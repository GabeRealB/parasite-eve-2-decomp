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
#define MAGGOT_CATERPILLAR_KIND MAGGOT
#include "../../shared/maggot_caterpillar.h"

enum { MAGGOT_CATERPILLAR_BODY_PRIORITY = 96 };

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gActor02600Actor102600Animation06198;
static AnimationSet _gActor02600Actor102600Animation0644C;
static AnimationSet _gActor02600Actor102600Animation068D0;
static AnimationSet _gActor02600Actor102600Animation06E14;
static AnimationSet _gActor02600Actor102600Animation074DC;
static AnimationSet _gActor02600Actor102600Animation077A4;
static AnimationSet _gActor02600Actor102600Animation07934;
static AnimationSet _gActor02600Actor102600Animation07B48;
static AnimationSet _gActor02600Actor102600Animation07E88;
static AnimationSet _gActor02600Actor102600Animation08140;
static AnimationSet _gActor02600Actor102600Animation083EC;
static AnimationSet _gActor02600Actor102600Animation08778;
static AnimationSet _gActor02600Actor102600Animation08928;
static TmdSource    _gActor02600CaterpillarMaggotBody;

static TmdBone _gActor02600CaterpillarMaggotBodySkeleton[8] = {
#include "assets/caterpillar_maggot_body_skeleton.inc"
};

static u32 _gActor02600CaterpillarMaggotBodyPartVerts[8] = {
#include "assets/caterpillar_maggot_body_partVerts.inc"
};

static SVECTOR _gActor02600CaterpillarMaggotBodyVerts[101] = {
#include "assets/caterpillar_maggot_body_verts.inc"
};

static SVECTOR _gActor02600CaterpillarMaggotBodyNormals[107] = {
#include "assets/caterpillar_maggot_body_normals.inc"
};

static u32 _gActor02600CaterpillarMaggotBodyStream[1012] = {
#include "assets/caterpillar_maggot_body_stream.inc"
};

static TmdSource _gActor02600CaterpillarMaggotBody = {
    0,
    5400,
    1872,
    8,
    _gActor02600CaterpillarMaggotBodyPartVerts,
    _gActor02600CaterpillarMaggotBodyVerts,
    _gActor02600CaterpillarMaggotBodyNormals,
    _gActor02600CaterpillarMaggotBodySkeleton,
    _gActor02600CaterpillarMaggotBodyStream,
};

#include "../../shared/maggot_caterpillar_husk_model.inc.c"

static AnimationPackedPose _gActor02600Actor102600Animation06198Bank1[10] = {
#include "assets/actor_102600_animation_06198_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation06198Bank4[45] = {
#include "assets/actor_102600_animation_06198_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation06198Records[74] = {
#include "assets/actor_102600_animation_06198_records.inc"
};

static u16 _gActor02600Actor102600Animation06198Indices[8] = {
#include "assets/actor_102600_animation_06198_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation06198 = {
    _gActor02600Actor102600Animation06198Records,
    _gActor02600Actor102600Animation06198Indices,
    { NULL, _gActor02600Actor102600Animation06198Bank1, NULL, NULL, _gActor02600Actor102600Animation06198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation0644CBank1[11] = {
#include "assets/actor_102600_animation_0644C_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation0644CBank4[44] = {
#include "assets/actor_102600_animation_0644C_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation0644CRecords[82] = {
#include "assets/actor_102600_animation_0644C_records.inc"
};

static u16 _gActor02600Actor102600Animation0644CIndices[8] = {
#include "assets/actor_102600_animation_0644C_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation0644C = {
    _gActor02600Actor102600Animation0644CRecords,
    _gActor02600Actor102600Animation0644CIndices,
    { NULL, _gActor02600Actor102600Animation0644CBank1, NULL, NULL, _gActor02600Actor102600Animation0644CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation068D0Bank1[13] = {
#include "assets/actor_102600_animation_068D0_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation068D0Bank4[96] = {
#include "assets/actor_102600_animation_068D0_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation068D0Records[140] = {
#include "assets/actor_102600_animation_068D0_records.inc"
};

static u16 _gActor02600Actor102600Animation068D0Indices[8] = {
#include "assets/actor_102600_animation_068D0_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation068D0 = {
    _gActor02600Actor102600Animation068D0Records,
    _gActor02600Actor102600Animation068D0Indices,
    { NULL, _gActor02600Actor102600Animation068D0Bank1, NULL, NULL, _gActor02600Actor102600Animation068D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation06E14Bank1[31] = {
#include "assets/actor_102600_animation_06E14_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation06E14Bank4[82] = {
#include "assets/actor_102600_animation_06E14_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation06E14Records[148] = {
#include "assets/actor_102600_animation_06E14_records.inc"
};

static u16 _gActor02600Actor102600Animation06E14Indices[8] = {
#include "assets/actor_102600_animation_06E14_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation06E14 = {
    _gActor02600Actor102600Animation06E14Records,
    _gActor02600Actor102600Animation06E14Indices,
    { NULL, _gActor02600Actor102600Animation06E14Bank1, NULL, NULL, _gActor02600Actor102600Animation06E14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation074DCBank1[27] = {
#include "assets/actor_102600_animation_074DC_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation074DCBank4[148] = {
#include "assets/actor_102600_animation_074DC_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation074DCRecords[191] = {
#include "assets/actor_102600_animation_074DC_records.inc"
};

static u16 _gActor02600Actor102600Animation074DCIndices[8] = {
#include "assets/actor_102600_animation_074DC_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation074DC = {
    _gActor02600Actor102600Animation074DCRecords,
    _gActor02600Actor102600Animation074DCIndices,
    { NULL, _gActor02600Actor102600Animation074DCBank1, NULL, NULL, _gActor02600Actor102600Animation074DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation077A4Bank1[11] = {
#include "assets/actor_102600_animation_077A4_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation077A4Bank4[51] = {
#include "assets/actor_102600_animation_077A4_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation077A4Records[80] = {
#include "assets/actor_102600_animation_077A4_records.inc"
};

static u16 _gActor02600Actor102600Animation077A4Indices[8] = {
#include "assets/actor_102600_animation_077A4_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation077A4 = {
    _gActor02600Actor102600Animation077A4Records,
    _gActor02600Actor102600Animation077A4Indices,
    { NULL, _gActor02600Actor102600Animation077A4Bank1, NULL, NULL, _gActor02600Actor102600Animation077A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation07934Bank1[6] = {
#include "assets/actor_102600_animation_07934_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation07934Bank4[26] = {
#include "assets/actor_102600_animation_07934_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation07934Records[42] = {
#include "assets/actor_102600_animation_07934_records.inc"
};

static u16 _gActor02600Actor102600Animation07934Indices[8] = {
#include "assets/actor_102600_animation_07934_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation07934 = {
    _gActor02600Actor102600Animation07934Records,
    _gActor02600Actor102600Animation07934Indices,
    { NULL, _gActor02600Actor102600Animation07934Bank1, NULL, NULL, _gActor02600Actor102600Animation07934Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation07B48Bank1[9] = {
#include "assets/actor_102600_animation_07B48_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation07B48Bank4[34] = {
#include "assets/actor_102600_animation_07B48_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation07B48Records[58] = {
#include "assets/actor_102600_animation_07B48_records.inc"
};

static u16 _gActor02600Actor102600Animation07B48Indices[8] = {
#include "assets/actor_102600_animation_07B48_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation07B48 = {
    _gActor02600Actor102600Animation07B48Records,
    _gActor02600Actor102600Animation07B48Indices,
    { NULL, _gActor02600Actor102600Animation07B48Bank1, NULL, NULL, _gActor02600Actor102600Animation07B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation07E88Bank1[13] = {
#include "assets/actor_102600_animation_07E88_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation07E88Bank4[58] = {
#include "assets/actor_102600_animation_07E88_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation07E88Records[97] = {
#include "assets/actor_102600_animation_07E88_records.inc"
};

static u16 _gActor02600Actor102600Animation07E88Indices[8] = {
#include "assets/actor_102600_animation_07E88_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation07E88 = {
    _gActor02600Actor102600Animation07E88Records,
    _gActor02600Actor102600Animation07E88Indices,
    { NULL, _gActor02600Actor102600Animation07E88Bank1, NULL, NULL, _gActor02600Actor102600Animation07E88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation08140Bank1[9] = {
#include "assets/actor_102600_animation_08140_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation08140Bank4[53] = {
#include "assets/actor_102600_animation_08140_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation08140Records[80] = {
#include "assets/actor_102600_animation_08140_records.inc"
};

static u16 _gActor02600Actor102600Animation08140Indices[8] = {
#include "assets/actor_102600_animation_08140_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation08140 = {
    _gActor02600Actor102600Animation08140Records,
    _gActor02600Actor102600Animation08140Indices,
    { NULL, _gActor02600Actor102600Animation08140Bank1, NULL, NULL, _gActor02600Actor102600Animation08140Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation083ECBank1[9] = {
#include "assets/actor_102600_animation_083EC_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation083ECBank4[49] = {
#include "assets/actor_102600_animation_083EC_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation083ECRecords[81] = {
#include "assets/actor_102600_animation_083EC_records.inc"
};

static u16 _gActor02600Actor102600Animation083ECIndices[8] = {
#include "assets/actor_102600_animation_083EC_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation083EC = {
    _gActor02600Actor102600Animation083ECRecords,
    _gActor02600Actor102600Animation083ECIndices,
    { NULL, _gActor02600Actor102600Animation083ECBank1, NULL, NULL, _gActor02600Actor102600Animation083ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation08778Bank1[20] = {
#include "assets/actor_102600_animation_08778_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation08778Bank4[61] = {
#include "assets/actor_102600_animation_08778_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation08778Records[92] = {
#include "assets/actor_102600_animation_08778_records.inc"
};

static u16 _gActor02600Actor102600Animation08778Indices[8] = {
#include "assets/actor_102600_animation_08778_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation08778 = {
    _gActor02600Actor102600Animation08778Records,
    _gActor02600Actor102600Animation08778Indices,
    { NULL, _gActor02600Actor102600Animation08778Bank1, NULL, NULL, _gActor02600Actor102600Animation08778Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02600Actor102600Animation08928Bank1[5] = {
#include "assets/actor_102600_animation_08928_bank1.inc"
};

static AnimationPackedRotation _gActor02600Actor102600Animation08928Bank4[19] = {
#include "assets/actor_102600_animation_08928_bank4.inc"
};

static AnimationRecord _gActor02600Actor102600Animation08928Records[60] = {
#include "assets/actor_102600_animation_08928_records.inc"
};

static u16 _gActor02600Actor102600Animation08928Indices[8] = {
#include "assets/actor_102600_animation_08928_indices.inc"
};

static AnimationSet _gActor02600Actor102600Animation08928 = {
    _gActor02600Actor102600Animation08928Records,
    _gActor02600Actor102600Animation08928Indices,
    { NULL, _gActor02600Actor102600Animation08928Bank1, NULL, NULL, _gActor02600Actor102600Animation08928Bank4, NULL, NULL, NULL },
};

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
    .data     = { .model = &_gActor02600CaterpillarMaggotBody },
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
static AnimationSet* _gMaggotCaterpillarAnimSets[15] = {
    NULL,
    &_gActor02600Actor102600Animation06198,
    &_gActor02600Actor102600Animation0644C,
    &_gActor02600Actor102600Animation068D0,
    &_gActor02600Actor102600Animation06E14,
    &_gActor02600Actor102600Animation074DC,
    &_gActor02600Actor102600Animation077A4,
    &_gActor02600Actor102600Animation07934,
    NULL,
    &_gActor02600Actor102600Animation07B48,
    &_gActor02600Actor102600Animation07E88,
    &_gActor02600Actor102600Animation08140,
    &_gActor02600Actor102600Animation083EC,
    &_gActor02600Actor102600Animation08778,
    &_gActor02600Actor102600Animation08928,
};

#include "../../shared/maggot_caterpillar_resolve_contacts.inc.c"

/// Puff enemy callbacks indexed by task state: 0 setup, 1 flight, 2 destroy.
///
/// The dispatcher copies the typed callbacks before passing the live
/// enemy and task; destruction releases their owned resources.
static const EnemyTaskFuncTable3 _gMaggotCaterpillarPuffStates = {
    {
        _maggotCaterpillarPuffSetup,
        _maggotCaterpillarPuffTick,
        enemyDestroy,
    },
};

/// Body enemy callbacks indexed by task state: 0 spawn, 1 active, 2 dying.
///
/// The dispatcher copies the typed callbacks before passing the live
/// enemy and task. The dying callback completes the enemy's lifetime.
static const EnemyTaskFuncTable3 _gMaggotCaterpillarStates = {
    {
        _maggotCaterpillarSpawn,
        _maggotCaterpillarTick,
        _maggotCaterpillarDyingState,
    },
};

#include "../../shared/maggot_caterpillar_wait_state.inc.c"

#include "../../shared/maggot_caterpillar_aim_state.inc.c"

#include "../../shared/maggot_caterpillar_ambush_state.inc.c"

#include "../../shared/maggot_caterpillar_roam_state.inc.c"

#include "../../shared/maggot_caterpillar_spray.inc.c"

#include "../../shared/maggot_caterpillar_pounce.inc.c"

#include "../../shared/maggot_caterpillar_hurt.inc.c"

#include "../../shared/maggot_caterpillar_entrance.inc.c"

#include "../../shared/maggot_caterpillar_burn.inc.c"

#include "../../shared/maggot_caterpillar_turn.inc.c"

#include "../../shared/maggot_caterpillar_inlines.inc.c"

#include "../../shared/maggot_caterpillar_dying.inc.c"

#include "../../shared/maggot_caterpillar_puff_tick.inc.c"

#include "../../shared/maggot_caterpillar_draw_puff.inc.c"

#include "../../shared/maggot_caterpillar_draw_thread.inc.c"

/// Selects this carrier's body/puff descriptor run for the shared spawn fragment.
///
/// Yields a `TaskDesc*` borrowed while the body lives; keep the package loaded
/// while body or puff tasks live. The retained scalar body declaration does
/// not cover index 1, which reaches the separately declared adjacent puff.
#define MAGGOT_CATERPILLAR_TASK_TABLE (&gActor02600MaggotCaterpillarBodyTask)
#include "../../shared/maggot_caterpillar_spawn.inc.c"
#undef MAGGOT_CATERPILLAR_TASK_TABLE

#include "../../shared/maggot_caterpillar_tick.inc.c"

#include "../../shared/maggot_caterpillar_status.inc.c"

#include "../../shared/maggot_caterpillar_run_behaviour.inc.c"

#include "../../shared/maggot_caterpillar_stun.inc.c"

#include "../../shared/maggot_caterpillar_move.inc.c"

#include "../../shared/maggot_caterpillar_tick_anim.inc.c"

#include "../../shared/maggot_caterpillar_update_color.inc.c"

#include "../../shared/maggot_caterpillar_shadow.inc.c"

#include "../../shared/maggot_caterpillar_squash.inc.c"

#include "../../shared/maggot_caterpillar_husk.inc.c"

#include "../../shared/maggot_caterpillar_shrink_node.inc.c"

#include "../../shared/maggot_caterpillar_puff_task.inc.c"

#include "../../shared/maggot_caterpillar_puff_setup.inc.c"

#include "../../shared/maggot_caterpillar_task.inc.c"

DEFINE_ALIAS(gActor02600MaggotCaterpillarBodyTask, gActor202600MaggotCaterpillarBodyTask);
