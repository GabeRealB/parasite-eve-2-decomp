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
#define MAGGOT_CATERPILLAR_KIND CATERPILLAR
#include "../../shared/maggot_caterpillar.h"

extern void* D_80067704[1];

extern TmdSource     gMaggotCaterpillarHuskModel;
extern DamageAttack  gMaggotCaterpillarAttacks[6];
extern EnemyParams   gMaggotCaterpillarParams;
extern u16           gMaggotCaterpillarIdleDelay[];
extern u16           gMaggotCaterpillarRoamDelay[];
extern u16           gMaggotCaterpillarDropSpeed[];
extern s16           gMaggotCaterpillarLeapInDelay[];
extern SVECTOR       gMaggotCaterpillarLeapInSpots[];
extern s16           gMaggotCaterpillarLeapInYaws[];
extern s16           gMaggotCaterpillarDropInDelay[];
extern s16           gMaggotCaterpillarDropInSpeed[];
extern SVECTOR       gMaggotCaterpillarDropInSpots[];
extern s16           gMaggotCaterpillarDropInYaws[];
extern s16           gMaggotCaterpillarAnimBlend[];
extern s16           gMaggotCaterpillarSprayTail;
extern s16           gMaggotCaterpillarPounceLead;
extern s16           gMaggotCaterpillarPounceStride[][2];
extern s16           gMaggotCaterpillarReboundStride[][2];
extern ActorSpriteUv gMaggotCaterpillarPuffCells[];
extern s16           gMaggotCaterpillarPuffRadius[];
extern TaskDesc      gMaggotCaterpillarBodyTask;
extern AnimationSet* gMaggotCaterpillarAnimSets[15];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gActor05500Actor105500Animation061A0;
static AnimationSet _gActor05500Actor105500Animation06454;
static AnimationSet _gActor05500Actor105500Animation068D8;
static AnimationSet _gActor05500Actor105500Animation06E1C;
static AnimationSet _gActor05500Actor105500Animation074E4;
static AnimationSet _gActor05500Actor105500Animation077AC;
static AnimationSet _gActor05500Actor105500Animation0793C;
static AnimationSet _gActor05500Actor105500Animation07B50;
static AnimationSet _gActor05500Actor105500Animation07E90;
static AnimationSet _gActor05500Actor105500Animation08148;
static AnimationSet _gActor05500Actor105500Animation083F4;
static AnimationSet _gActor05500Actor105500Animation08780;
static AnimationSet _gActor05500Actor105500Animation08930;
static TmdSource    _gActor05500CaterpillarMaggotBody;

static TmdBone _gActor05500CaterpillarMaggotBodySkeleton[8] = {
#include "assets/caterpillar_maggot_body_skeleton.inc"
};

static u32 _gActor05500CaterpillarMaggotBodyPartVerts[8] = {
#include "assets/caterpillar_maggot_body_partVerts.inc"
};

static SVECTOR _gActor05500CaterpillarMaggotBodyVerts[101] = {
#include "assets/caterpillar_maggot_body_verts.inc"
};

static SVECTOR _gActor05500CaterpillarMaggotBodyNormals[107] = {
#include "assets/caterpillar_maggot_body_normals.inc"
};

static u32 _gActor05500CaterpillarMaggotBodyStream[1012] = {
#include "assets/caterpillar_maggot_body_stream.inc"
};

static TmdSource _gActor05500CaterpillarMaggotBody = {
    0,
    5400,
    1872,
    8,
    _gActor05500CaterpillarMaggotBodyPartVerts,
    _gActor05500CaterpillarMaggotBodyVerts,
    _gActor05500CaterpillarMaggotBodyNormals,
    _gActor05500CaterpillarMaggotBodySkeleton,
    _gActor05500CaterpillarMaggotBodyStream,
};

static TmdBone _gActor05500CaterpillarMaggotBurstHeadSkeleton[1] = {
#include "assets/caterpillar_maggot_burst_head_skeleton.inc"
};

static u32 _gActor05500CaterpillarMaggotBurstHeadPartVerts[1] = {
#include "assets/caterpillar_maggot_burst_head_partVerts.inc"
};

static SVECTOR _gActor05500CaterpillarMaggotBurstHeadVerts[40] = {
#include "assets/caterpillar_maggot_burst_head_verts.inc"
};

static SVECTOR _gActor05500CaterpillarMaggotBurstHeadNormals[40] = {
#include "assets/caterpillar_maggot_burst_head_normals.inc"
};

static u32 _gActor05500CaterpillarMaggotBurstHeadStream[310] = {
#include "assets/caterpillar_maggot_burst_head_stream.inc"
};

TmdSource gMaggotCaterpillarHuskModel = {
    0,
    2144,
    0,
    1,
    _gActor05500CaterpillarMaggotBurstHeadPartVerts,
    _gActor05500CaterpillarMaggotBurstHeadVerts,
    _gActor05500CaterpillarMaggotBurstHeadNormals,
    _gActor05500CaterpillarMaggotBurstHeadSkeleton,
    _gActor05500CaterpillarMaggotBurstHeadStream,
};

static AnimationPackedPose _gActor05500Actor105500Animation061A0Bank1[10] = {
#include "assets/actor_105500_animation_061A0_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation061A0Bank4[45] = {
#include "assets/actor_105500_animation_061A0_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation061A0Records[74] = {
#include "assets/actor_105500_animation_061A0_records.inc"
};

static u16 _gActor05500Actor105500Animation061A0Indices[8] = {
#include "assets/actor_105500_animation_061A0_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation061A0 = {
    _gActor05500Actor105500Animation061A0Records,
    _gActor05500Actor105500Animation061A0Indices,
    { NULL, _gActor05500Actor105500Animation061A0Bank1, NULL, NULL, _gActor05500Actor105500Animation061A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation06454Bank1[11] = {
#include "assets/actor_105500_animation_06454_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation06454Bank4[44] = {
#include "assets/actor_105500_animation_06454_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation06454Records[82] = {
#include "assets/actor_105500_animation_06454_records.inc"
};

static u16 _gActor05500Actor105500Animation06454Indices[8] = {
#include "assets/actor_105500_animation_06454_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation06454 = {
    _gActor05500Actor105500Animation06454Records,
    _gActor05500Actor105500Animation06454Indices,
    { NULL, _gActor05500Actor105500Animation06454Bank1, NULL, NULL, _gActor05500Actor105500Animation06454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation068D8Bank1[13] = {
#include "assets/actor_105500_animation_068D8_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation068D8Bank4[96] = {
#include "assets/actor_105500_animation_068D8_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation068D8Records[140] = {
#include "assets/actor_105500_animation_068D8_records.inc"
};

static u16 _gActor05500Actor105500Animation068D8Indices[8] = {
#include "assets/actor_105500_animation_068D8_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation068D8 = {
    _gActor05500Actor105500Animation068D8Records,
    _gActor05500Actor105500Animation068D8Indices,
    { NULL, _gActor05500Actor105500Animation068D8Bank1, NULL, NULL, _gActor05500Actor105500Animation068D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation06E1CBank1[31] = {
#include "assets/actor_105500_animation_06E1C_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation06E1CBank4[82] = {
#include "assets/actor_105500_animation_06E1C_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation06E1CRecords[148] = {
#include "assets/actor_105500_animation_06E1C_records.inc"
};

static u16 _gActor05500Actor105500Animation06E1CIndices[8] = {
#include "assets/actor_105500_animation_06E1C_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation06E1C = {
    _gActor05500Actor105500Animation06E1CRecords,
    _gActor05500Actor105500Animation06E1CIndices,
    { NULL, _gActor05500Actor105500Animation06E1CBank1, NULL, NULL, _gActor05500Actor105500Animation06E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation074E4Bank1[27] = {
#include "assets/actor_105500_animation_074E4_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation074E4Bank4[148] = {
#include "assets/actor_105500_animation_074E4_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation074E4Records[191] = {
#include "assets/actor_105500_animation_074E4_records.inc"
};

static u16 _gActor05500Actor105500Animation074E4Indices[8] = {
#include "assets/actor_105500_animation_074E4_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation074E4 = {
    _gActor05500Actor105500Animation074E4Records,
    _gActor05500Actor105500Animation074E4Indices,
    { NULL, _gActor05500Actor105500Animation074E4Bank1, NULL, NULL, _gActor05500Actor105500Animation074E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation077ACBank1[11] = {
#include "assets/actor_105500_animation_077AC_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation077ACBank4[51] = {
#include "assets/actor_105500_animation_077AC_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation077ACRecords[80] = {
#include "assets/actor_105500_animation_077AC_records.inc"
};

static u16 _gActor05500Actor105500Animation077ACIndices[8] = {
#include "assets/actor_105500_animation_077AC_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation077AC = {
    _gActor05500Actor105500Animation077ACRecords,
    _gActor05500Actor105500Animation077ACIndices,
    { NULL, _gActor05500Actor105500Animation077ACBank1, NULL, NULL, _gActor05500Actor105500Animation077ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation0793CBank1[6] = {
#include "assets/actor_105500_animation_0793C_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation0793CBank4[26] = {
#include "assets/actor_105500_animation_0793C_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation0793CRecords[42] = {
#include "assets/actor_105500_animation_0793C_records.inc"
};

static u16 _gActor05500Actor105500Animation0793CIndices[8] = {
#include "assets/actor_105500_animation_0793C_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation0793C = {
    _gActor05500Actor105500Animation0793CRecords,
    _gActor05500Actor105500Animation0793CIndices,
    { NULL, _gActor05500Actor105500Animation0793CBank1, NULL, NULL, _gActor05500Actor105500Animation0793CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation07B50Bank1[9] = {
#include "assets/actor_105500_animation_07B50_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation07B50Bank4[34] = {
#include "assets/actor_105500_animation_07B50_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation07B50Records[58] = {
#include "assets/actor_105500_animation_07B50_records.inc"
};

static u16 _gActor05500Actor105500Animation07B50Indices[8] = {
#include "assets/actor_105500_animation_07B50_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation07B50 = {
    _gActor05500Actor105500Animation07B50Records,
    _gActor05500Actor105500Animation07B50Indices,
    { NULL, _gActor05500Actor105500Animation07B50Bank1, NULL, NULL, _gActor05500Actor105500Animation07B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation07E90Bank1[13] = {
#include "assets/actor_105500_animation_07E90_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation07E90Bank4[58] = {
#include "assets/actor_105500_animation_07E90_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation07E90Records[97] = {
#include "assets/actor_105500_animation_07E90_records.inc"
};

static u16 _gActor05500Actor105500Animation07E90Indices[8] = {
#include "assets/actor_105500_animation_07E90_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation07E90 = {
    _gActor05500Actor105500Animation07E90Records,
    _gActor05500Actor105500Animation07E90Indices,
    { NULL, _gActor05500Actor105500Animation07E90Bank1, NULL, NULL, _gActor05500Actor105500Animation07E90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation08148Bank1[9] = {
#include "assets/actor_105500_animation_08148_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation08148Bank4[53] = {
#include "assets/actor_105500_animation_08148_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation08148Records[80] = {
#include "assets/actor_105500_animation_08148_records.inc"
};

static u16 _gActor05500Actor105500Animation08148Indices[8] = {
#include "assets/actor_105500_animation_08148_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation08148 = {
    _gActor05500Actor105500Animation08148Records,
    _gActor05500Actor105500Animation08148Indices,
    { NULL, _gActor05500Actor105500Animation08148Bank1, NULL, NULL, _gActor05500Actor105500Animation08148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation083F4Bank1[9] = {
#include "assets/actor_105500_animation_083F4_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation083F4Bank4[49] = {
#include "assets/actor_105500_animation_083F4_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation083F4Records[81] = {
#include "assets/actor_105500_animation_083F4_records.inc"
};

static u16 _gActor05500Actor105500Animation083F4Indices[8] = {
#include "assets/actor_105500_animation_083F4_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation083F4 = {
    _gActor05500Actor105500Animation083F4Records,
    _gActor05500Actor105500Animation083F4Indices,
    { NULL, _gActor05500Actor105500Animation083F4Bank1, NULL, NULL, _gActor05500Actor105500Animation083F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation08780Bank1[20] = {
#include "assets/actor_105500_animation_08780_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation08780Bank4[61] = {
#include "assets/actor_105500_animation_08780_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation08780Records[92] = {
#include "assets/actor_105500_animation_08780_records.inc"
};

static u16 _gActor05500Actor105500Animation08780Indices[8] = {
#include "assets/actor_105500_animation_08780_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation08780 = {
    _gActor05500Actor105500Animation08780Records,
    _gActor05500Actor105500Animation08780Indices,
    { NULL, _gActor05500Actor105500Animation08780Bank1, NULL, NULL, _gActor05500Actor105500Animation08780Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05500Actor105500Animation08930Bank1[5] = {
#include "assets/actor_105500_animation_08930_bank1.inc"
};

static AnimationPackedRotation _gActor05500Actor105500Animation08930Bank4[19] = {
#include "assets/actor_105500_animation_08930_bank4.inc"
};

static AnimationRecord _gActor05500Actor105500Animation08930Records[60] = {
#include "assets/actor_105500_animation_08930_records.inc"
};

static u16 _gActor05500Actor105500Animation08930Indices[8] = {
#include "assets/actor_105500_animation_08930_indices.inc"
};

static AnimationSet _gActor05500Actor105500Animation08930 = {
    _gActor05500Actor105500Animation08930Records,
    _gActor05500Actor105500Animation08930Indices,
    { NULL, _gActor05500Actor105500Animation08930Bank1, NULL, NULL, _gActor05500Actor105500Animation08930Bank4, NULL, NULL, NULL },
};

DamageAttack gMaggotCaterpillarAttacks[6] = {
    { 10, 0 },
    { 16, 0 },
    { 8, 1 },
    { 12, 6 },
    { 18, 6 },
    { 8, 0 },
};

EnemyParams gMaggotCaterpillarParams = { gMaggotCaterpillarAttacks, 80, 6, 28, 1, 100, 20, 100, 0 };

u16 gMaggotCaterpillarIdleDelay[8] = {
    40,
    35,
    30,
    25,
    20,
    15,
    10,
    5,
};

u16 gMaggotCaterpillarRoamDelay[8] = {
    1600,
    1800,
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
};

u16 gMaggotCaterpillarDropSpeed[8] = {
    100,
    110,
    120,
    130,
    140,
    150,
    160,
    170,
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

s32 Actor05500_D08A24[5] = {
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

TaskDesc gMaggotCaterpillarBodyTask = { { { TASK_BODY_TMD, 96 } }, maggotCaterpillarTask, { .model = &_gActor05500CaterpillarMaggotBody } };

TaskDesc Actor05500_D08AC8 = { { { TASK_BODY_COORD, 96 } }, maggotCaterpillarPuffTask, { .value = 0 } };

AnimationSet* gMaggotCaterpillarAnimSets[15] = {
    NULL,
    &_gActor05500Actor105500Animation061A0,
    &_gActor05500Actor105500Animation06454,
    &_gActor05500Actor105500Animation068D8,
    &_gActor05500Actor105500Animation06E1C,
    &_gActor05500Actor105500Animation074E4,
    &_gActor05500Actor105500Animation077AC,
    &_gActor05500Actor105500Animation0793C,
    NULL,
    &_gActor05500Actor105500Animation07B50,
    &_gActor05500Actor105500Animation07E90,
    &_gActor05500Actor105500Animation08148,
    &_gActor05500Actor105500Animation083F4,
    &_gActor05500Actor105500Animation08780,
    &_gActor05500Actor105500Animation08930,
};

#include "../../shared/maggot_caterpillar_resolve_contacts.inc.c"

/// State handlers of the task `maggotCaterpillarPuffTask` dispatches, indexed by the
/// task's state: `maggotCaterpillarPuffSetup` sets up its collision object,
/// `maggotCaterpillarPuffTick` runs it, and `enemyDestroy` tears it down.
static const EnemyTaskFuncTable3 gMaggotCaterpillarPuffStates = {
    {
        maggotCaterpillarPuffSetup,
        maggotCaterpillarPuffTick,
        enemyDestroy,
    },
};

/// State handlers of the task `maggotCaterpillarTask` dispatches, indexed by the
/// task's state: `maggotCaterpillarSpawn` allocates and sets up the work block,
/// `maggotCaterpillarTick` runs the actor, and `maggotCaterpillarDyingState` handles its
/// last state.
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
