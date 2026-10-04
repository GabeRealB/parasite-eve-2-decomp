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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/object_fields.h"
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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#define MAGGOT_CATERPILLAR_KIND MAGGOT
#include "../../shared/maggot_caterpillar.h"

extern ActorSpriteUv gMaggotCaterpillarPuffCells[];
extern s16           gMaggotCaterpillarPuffRadius[];

extern EnemyParams   gMaggotCaterpillarParams;
extern SVECTOR       gMaggotCaterpillarLeapInSpots[];
extern s16           gMaggotCaterpillarLeapInYaws[];
extern SVECTOR       gMaggotCaterpillarDropInSpots[];
extern s16           gMaggotCaterpillarDropInYaws[];
extern TaskDesc      gMaggotCaterpillarBodyTask;
extern AnimationSet* gMaggotCaterpillarAnimSets[15];
extern u16           gMaggotCaterpillarIdleDelay[];
extern u16           gMaggotCaterpillarRoamDelay[];
extern u16           gMaggotCaterpillarDropSpeed[];
extern s16           gMaggotCaterpillarLeapInDelay[];
extern s16           gMaggotCaterpillarDropInDelay[];
extern s16           gMaggotCaterpillarDropInSpeed[];
extern s16           gMaggotCaterpillarSprayTail;
extern s16           gMaggotCaterpillarPounceLead;
extern s16           gMaggotCaterpillarPounceStride[][2];
extern s16           gMaggotCaterpillarReboundStride[][2];
extern DamageAttack  gMaggotCaterpillarAttacks[6];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 gMaggotCaterpillarAnimBlend[];

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern TmdSource gMaggotCaterpillarHuskModel;

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

static TmdBone _gActor02600CaterpillarMaggotBurstHeadSkeleton[1] = {
#include "assets/caterpillar_maggot_burst_head_skeleton.inc"
};

static u32 _gActor02600CaterpillarMaggotBurstHeadPartVerts[1] = {
#include "assets/caterpillar_maggot_burst_head_partVerts.inc"
};

static SVECTOR _gActor02600CaterpillarMaggotBurstHeadVerts[40] = {
#include "assets/caterpillar_maggot_burst_head_verts.inc"
};

static SVECTOR _gActor02600CaterpillarMaggotBurstHeadNormals[40] = {
#include "assets/caterpillar_maggot_burst_head_normals.inc"
};

static u32 _gActor02600CaterpillarMaggotBurstHeadStream[310] = {
#include "assets/caterpillar_maggot_burst_head_stream.inc"
};

TmdSource gMaggotCaterpillarHuskModel = {
    0,
    2144,
    0,
    1,
    _gActor02600CaterpillarMaggotBurstHeadPartVerts,
    _gActor02600CaterpillarMaggotBurstHeadVerts,
    _gActor02600CaterpillarMaggotBurstHeadNormals,
    _gActor02600CaterpillarMaggotBurstHeadSkeleton,
    _gActor02600CaterpillarMaggotBurstHeadStream,
};

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

DamageAttack gMaggotCaterpillarAttacks[6] = {
    { 14, 0 },
    { 20, 0 },
    { 10, 1 },
    { 16, 6 },
    { 24, 6 },
    { 10, 0 },
};

EnemyParams gMaggotCaterpillarParams = { gMaggotCaterpillarAttacks, 160, 16, 68, 1, 100, 10, 100, 5 };

u16 gMaggotCaterpillarIdleDelay[8] = {
    25,
    10,
    10,
    10,
    10,
    10,
    10,
    35,
};

u16 gMaggotCaterpillarRoamDelay[8] = {
    1600,
    2400,
    2400,
    2400,
    2400,
    2400,
    2400,
    1600,
};

u16 gMaggotCaterpillarDropSpeed[8] = {
    100,
    100,
    180,
    130,
    100,
    100,
    100,
    100,
};

s16 gMaggotCaterpillarLeapInDelay[4] = {
    0,
    2,
    4,
    6,
};

SVECTOR gMaggotCaterpillarLeapInSpots[4] = {
    { -3000, 0, -600, 0 },
    { -3000, 0, -1600, 0 },
    { -2000, 0, -600, 0 },
    { -2000, 0, -1600, 0 },
};

s16 gMaggotCaterpillarLeapInYaws[4] = {
    0,
    1024,
    2048,
    3072,
};

s16 gMaggotCaterpillarDropInDelay[4] = {
    5,
    10,
    15,
    20,
};

s16 gMaggotCaterpillarDropInSpeed[4] = {
    100,
    100,
    100,
    100,
};

SVECTOR gMaggotCaterpillarDropInSpots[4] = {
    { -7000, 0, -7600, 0 },
    { -5600, 0, -7500, 0 },
    { -4700, 0, -6500, 0 },
    { -5600, 0, -5600, 0 },
};

s16 gMaggotCaterpillarDropInYaws[4] = {
    600,
    1024,
    2048,
    1800,
};

s16 gMaggotCaterpillarAnimBlend[3] = {
    0,
    10,
    0,
};

s16 gMaggotCaterpillarSprayTail = 8;

s16 gMaggotCaterpillarPounceLead = 8;

s32 Actor02600_D08A1C[5] = {
    0,
    0,
    0,
    0,
    4,
};

s16 gMaggotCaterpillarPounceStride[9][2] = {
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

s16 gMaggotCaterpillarReboundStride[9][2] = {
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

ActorSpriteUv gMaggotCaterpillarPuffCells[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

s16 gMaggotCaterpillarPuffRadius[14] = {
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

TaskDesc gMaggotCaterpillarBodyTask = { { { TASK_BODY_TMD, 96 } }, maggotCaterpillarTask, { .model = &_gActor02600CaterpillarMaggotBody } };

TaskDesc Actor02600_D08AC0 = { { { TASK_BODY_COORD, 96 } }, maggotCaterpillarPuffTask, { .value = 0 } };

AnimationSet* gMaggotCaterpillarAnimSets[15] = {
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

/// State handlers of the projectile task `maggotCaterpillarPuffTask` dispatches,
/// indexed by `Task::state`: setup, per-frame tick and `enemyDestroy`.
static const EnemyTaskFuncTable3 gMaggotCaterpillarPuffStates = {
    {
        maggotCaterpillarPuffSetup,
        maggotCaterpillarPuffTick,
        enemyDestroy,
    },
};

/// State handlers of the actor task `maggotCaterpillarTask` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const EnemyTaskFuncTable3 gMaggotCaterpillarStates = {
    {
        maggotCaterpillarSpawn,
        maggotCaterpillarTick,
        maggotCaterpillarDyingState,
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

#include "../../shared/maggot_caterpillar_spawn.inc.c"

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
