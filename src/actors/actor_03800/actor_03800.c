#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

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

typedef struct Actor103800Work {
    /* 0x000 */ AnimationContext       anim;
    /* 0x014 */ AnimationSlot          slots[6];
    /* 0x104 */ byte                   field_104[0x60];
    /* 0x164 */ MATRIX                 field_164;
    /* 0x184 */ MATRIX                 field_184;
    /* 0x1A4 */ byte                   field_1A4[8];
    /* 0x1AC */ void*                  field_1AC;
    /* 0x1B0 */ WorldCollisionContact* field_1B0;
    /* 0x1B4 */ u16                    field_1B4;
    /* 0x1B6 */ s16                    field_1B6;
    /* 0x1B8 */ u16                    field_1B8;
    /* 0x1BA */ byte                   pad_1BA[2];
    /* 0x1BC */ u32                    field_1BC;
    /* 0x1C0 */ u16                    field_1C0;
    /* 0x1C2 */ u16                    field_1C2;
    /* 0x1C4 */ WorldCollisionContact  field_1C4[3];
    /* 0x20C */ byte                   field_20C[8];
    /* 0x214 */ void*                  field_214;
    /* 0x218 */ WorldCollisionContact* field_218;
    /* 0x21C */ u16                    field_21C;
    /* 0x21E */ s16                    field_21E;
    /* 0x220 */ u16                    field_220;
    /* 0x222 */ byte                   pad_222[2];
    /* 0x224 */ u32                    field_224;
    /* 0x228 */ s16                    field_228;
    /* 0x22A */ u16                    field_22A;
    /* 0x22C */ WorldCollisionContact  field_22C[4];
    /* 0x28C */ byte                   field_28C[8];
    /* 0x294 */ void*                  field_294;
    /* 0x298 */ WorldCollisionContact* field_298;
    /* 0x29C */ u16                    field_29C;
    /* 0x29E */ s16                    field_29E;
    /* 0x2A0 */ u16                    field_2A0;
    /* 0x2A2 */ byte                   pad_2A2[2];
    /* 0x2A4 */ u32                    field_2A4;
    /* 0x2A8 */ u16                    field_2A8;
    /* 0x2AA */ u16                    field_2AA;
    /* 0x2AC */ WorldCollisionContact  field_2AC[1];
    /* 0x2C4 */ EffectSpawnArg         field_2C4; // record the death effect is spawned with
    /* 0x2CC */ MATRIX                 field_2CC;
    /* 0x2EC */ s16                    field_2EC;
    /* 0x2EE */ s16                    field_2EE;
    /* 0x2F0 */ s16                    field_2F0;
    /* 0x2F2 */ byte                   pad_2F2[2];
    /// Coordinate node `Actor03800_Fn003B8` publishes on `field_344` for the
    /// detached modes (spawn kinds 1 and 2): it is seeded from the model's own
    /// `TmdObject::coords`, parented to `gGfxViewCoord` and then turned
    /// by 0x400 / 0x800 about X. `coord.coord.t` is the saved world translation
    /// the idle and detach ticks restore after rebuilding the rotation.
    /* 0x2F4 */ GfxCoord  coord;
    /* 0x344 */ GfxCoord* field_344;
    /* 0x348 */ u16       field_348;
    /* 0x34A */ s16       field_34A;
    /* 0x34C */ u16       field_34C;
    /* 0x34E */ s16       field_34E;
    /* 0x350 */ s16       field_350;
    /* 0x352 */ s16       field_352;
    /* 0x354 */ s16       field_354;
    /* 0x356 */ s16       field_356;
    /* 0x358 */ s16       field_358;
    /* 0x35A */ s16       field_35A;
    /* 0x35C */ s16       field_35C;
    /* 0x35E */ s16       field_35E;
    /* 0x360 */ s16       field_360;
    /* 0x362 */ s16       field_362;
    /* 0x364 */ s16       field_364;
    /* 0x366 */ s16       field_366;
    /* 0x368 */ s16       field_368;
    /* 0x36A */ s16       field_36A;
    /* 0x36C */ s16       field_36C;
    /* 0x36E */ s16       field_36E;
    /* 0x370 */ s16       field_370;
    /* 0x372 */ s16       field_372;
    /* 0x374 */ s16       field_374;
    /* 0x376 */ byte      pad_376[2];
    /* 0x378 */ s16       field_378;
    /* 0x37A */ s16       field_37A;
    /* 0x37C */ s16       field_37C;
    /* 0x37E */ s16       field_37E;
} Actor103800Work;
STATIC_ASSERT_SIZEOF(Actor103800Work, 0x380);

typedef struct Actor03800TurnScratch {
    /* 0x00 */ SVECTOR rotation;
    /* 0x08 */ MATRIX  matrix;
} Actor03800TurnScratch;
STATIC_ASSERT_SIZEOF(Actor03800TurnScratch, 0x28);

typedef struct Actor03800MoveScratch {
    VECTOR  delta;
    SVECTOR normal;
} Actor03800MoveScratch;
STATIC_ASSERT_SIZEOF(Actor03800MoveScratch, 0x18);

extern void*     D_80067704[1];
static TmdSource _gActor03800BlackBeetleEffect1;
static TmdSource _gActor03800BlackBeetleEffect2;
static TmdSource _gActor03800BlackBeetleEffect3;
static TmdSource _gActor03800BlackBeetleEffect4;
static TmdSource _gActor03800BlackBeetleEffect5;

extern s16           Actor03800_D05F90[];
extern s16           Actor03800_D05FA8[];
extern DamageAttack  Actor03800_D05F40[1];
extern EnemyParams   Actor03800_D05F44;
extern AnimationSet* Actor03800_D05F60[12];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void Actor03800_Fn000B8(Enemy* arg0, Task* arg1);
static void Actor03800_Fn003B8(Task* arg0);
static void Actor03800_Fn00974(Task* arg0);
static void Actor03800_Fn00A98(Task* arg0);
static void Actor03800_Fn026F8(Task* arg0);
static void Actor03800_Fn02848(Task* arg0);
static void Actor03800_Fn02998(Enemy* arg0, Task* arg1);
static void Actor03800_Fn02E50(Task* arg0);
static void Actor03800_Fn03008(Task* actor, u32 variant);
static void Actor03800_Fn031B8(Enemy* arg0, Task* arg1);
static void Actor03800_Fn032D8(Task* arg0);
static void Actor03800_Fn03420(Task* arg0);
static void Actor03800_Fn034B0(Task* arg0);
static void Actor03800_Fn03594(Task* arg0);
static void Actor03800_Fn03628(Task* arg0);
static void Actor03800_Fn036EC(Task* arg0);
static void Actor03800_Fn03744(Task* arg0);
static void Actor03800_Fn037E0(Task* arg0);

/// State handlers `Actor03800_Fn0315C` dispatches, indexed by the task's state
/// (`Task::state`): the spawn state that allocates the work block and
/// moves to state 1, the per-frame tick, and the state-2 handler the tick hands
/// over to, which carries the death sequence.
static const EnemyTaskFuncTable3 Actor03800_D00004 = {
    {
        Actor03800_Fn000B8,
        Actor03800_Fn031B8,
        Actor03800_Fn02998,
    },
};

static AnimationSet _gActor03800Actor103800Animation04B24;
static AnimationSet _gActor03800Actor103800Animation04D20;
static AnimationSet _gActor03800Actor103800Animation04FE0;
static AnimationSet _gActor03800Actor103800Animation05534;
static AnimationSet _gActor03800Actor103800Animation055F0;
static AnimationSet _gActor03800Actor103800Animation05734;
static AnimationSet _gActor03800Actor103800Animation05824;
static AnimationSet _gActor03800Actor103800Animation05988;
static AnimationSet _gActor03800Actor103800Animation05B84;
static AnimationSet _gActor03800Actor103800Animation05E14;
static AnimationSet _gActor03800Actor103800Animation05F18;
static TmdSource    _gActor03800BlackBeetleBody;
static void         Actor03800_Fn0315C(Task*);

static TmdBone _gActor03800BlackBeetleBodySkeleton[6] = {
#include "assets/black_beetle_body_skeleton.inc"
};

static u32 _gActor03800BlackBeetleBodyPartVerts[6] = {
#include "assets/black_beetle_body_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleBodyVerts[33] = {
#include "assets/black_beetle_body_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleBodyNormals[42] = {
#include "assets/black_beetle_body_normals.inc"
};

static u32 _gActor03800BlackBeetleBodyStream[482] = {
#include "assets/black_beetle_body_stream.inc"
};

static TmdSource _gActor03800BlackBeetleBody = {
    0,
    2120,
    1072,
    6,
    _gActor03800BlackBeetleBodyPartVerts,
    _gActor03800BlackBeetleBodyVerts,
    _gActor03800BlackBeetleBodyNormals,
    _gActor03800BlackBeetleBodySkeleton,
    _gActor03800BlackBeetleBodyStream,
};

static TmdBone _gActor03800BlackBeetleEffect1Skeleton[1] = {
#include "assets/black_beetle_effect_1_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect1PartVerts[1] = {
#include "assets/black_beetle_effect_1_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect1Verts[8] = {
#include "assets/black_beetle_effect_1_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect1Normals[8] = {
#include "assets/black_beetle_effect_1_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect1Stream[76] = {
#include "assets/black_beetle_effect_1_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect1 = {
    0,
    452,
    0,
    1,
    _gActor03800BlackBeetleEffect1PartVerts,
    _gActor03800BlackBeetleEffect1Verts,
    _gActor03800BlackBeetleEffect1Normals,
    _gActor03800BlackBeetleEffect1Skeleton,
    _gActor03800BlackBeetleEffect1Stream,
};

static TmdBone _gActor03800BlackBeetleEffect2Skeleton[1] = {
#include "assets/black_beetle_effect_2_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect2PartVerts[1] = {
#include "assets/black_beetle_effect_2_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect2Verts[4] = {
#include "assets/black_beetle_effect_2_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect2Normals[4] = {
#include "assets/black_beetle_effect_2_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect2Stream[30] = {
#include "assets/black_beetle_effect_2_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect2 = {
    0,
    160,
    0,
    1,
    _gActor03800BlackBeetleEffect2PartVerts,
    _gActor03800BlackBeetleEffect2Verts,
    _gActor03800BlackBeetleEffect2Normals,
    _gActor03800BlackBeetleEffect2Skeleton,
    _gActor03800BlackBeetleEffect2Stream,
};

static TmdBone _gActor03800BlackBeetleEffect3Skeleton[1] = {
#include "assets/black_beetle_effect_3_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect3PartVerts[1] = {
#include "assets/black_beetle_effect_3_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect3Verts[4] = {
#include "assets/black_beetle_effect_3_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect3Normals[4] = {
#include "assets/black_beetle_effect_3_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect3Stream[30] = {
#include "assets/black_beetle_effect_3_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect3 = {
    0,
    160,
    0,
    1,
    _gActor03800BlackBeetleEffect3PartVerts,
    _gActor03800BlackBeetleEffect3Verts,
    _gActor03800BlackBeetleEffect3Normals,
    _gActor03800BlackBeetleEffect3Skeleton,
    _gActor03800BlackBeetleEffect3Stream,
};

static TmdBone _gActor03800BlackBeetleEffect4Skeleton[1] = {
#include "assets/black_beetle_effect_4_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect4PartVerts[1] = {
#include "assets/black_beetle_effect_4_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect4Verts[4] = {
#include "assets/black_beetle_effect_4_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect4Normals[2] = {
#include "assets/black_beetle_effect_4_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect4Stream[18] = {
#include "assets/black_beetle_effect_4_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect4 = {
    0,
    104,
    0,
    1,
    _gActor03800BlackBeetleEffect4PartVerts,
    _gActor03800BlackBeetleEffect4Verts,
    _gActor03800BlackBeetleEffect4Normals,
    _gActor03800BlackBeetleEffect4Skeleton,
    _gActor03800BlackBeetleEffect4Stream,
};

static TmdBone _gActor03800BlackBeetleEffect5Skeleton[1] = {
#include "assets/black_beetle_effect_5_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect5PartVerts[1] = {
#include "assets/black_beetle_effect_5_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect5Verts[4] = {
#include "assets/black_beetle_effect_5_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect5Normals[2] = {
#include "assets/black_beetle_effect_5_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect5Stream[18] = {
#include "assets/black_beetle_effect_5_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect5 = {
    0,
    104,
    0,
    1,
    _gActor03800BlackBeetleEffect5PartVerts,
    _gActor03800BlackBeetleEffect5Verts,
    _gActor03800BlackBeetleEffect5Normals,
    _gActor03800BlackBeetleEffect5Skeleton,
    _gActor03800BlackBeetleEffect5Stream,
};

static AnimationPackedPose _gActor03800Actor103800Animation04B24Bank1[7] = {
#include "assets/actor_103800_animation_04B24_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04B24Bank4[24] = {
#include "assets/actor_103800_animation_04B24_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04B24Records[69] = {
#include "assets/actor_103800_animation_04B24_records.inc"
};

static u16 _gActor03800Actor103800Animation04B24Indices[6] = {
#include "assets/actor_103800_animation_04B24_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04B24 = {
    _gActor03800Actor103800Animation04B24Records,
    _gActor03800Actor103800Animation04B24Indices,
    { NULL, _gActor03800Actor103800Animation04B24Bank1, NULL, NULL, _gActor03800Actor103800Animation04B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation04D20Bank1[12] = {
#include "assets/actor_103800_animation_04D20_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04D20Bank4[12] = {
#include "assets/actor_103800_animation_04D20_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04D20Records[66] = {
#include "assets/actor_103800_animation_04D20_records.inc"
};

static u16 _gActor03800Actor103800Animation04D20Indices[6] = {
#include "assets/actor_103800_animation_04D20_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04D20 = {
    _gActor03800Actor103800Animation04D20Records,
    _gActor03800Actor103800Animation04D20Indices,
    { NULL, _gActor03800Actor103800Animation04D20Bank1, NULL, NULL, _gActor03800Actor103800Animation04D20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation04FE0Bank1[17] = {
#include "assets/actor_103800_animation_04FE0_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04FE0Bank4[43] = {
#include "assets/actor_103800_animation_04FE0_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04FE0Records[69] = {
#include "assets/actor_103800_animation_04FE0_records.inc"
};

static u16 _gActor03800Actor103800Animation04FE0Indices[6] = {
#include "assets/actor_103800_animation_04FE0_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04FE0 = {
    _gActor03800Actor103800Animation04FE0Records,
    _gActor03800Actor103800Animation04FE0Indices,
    { NULL, _gActor03800Actor103800Animation04FE0Bank1, NULL, NULL, _gActor03800Actor103800Animation04FE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05534Bank1[31] = {
#include "assets/actor_103800_animation_05534_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05534Bank4[95] = {
#include "assets/actor_103800_animation_05534_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05534Records[140] = {
#include "assets/actor_103800_animation_05534_records.inc"
};

static u16 _gActor03800Actor103800Animation05534Indices[6] = {
#include "assets/actor_103800_animation_05534_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05534 = {
    _gActor03800Actor103800Animation05534Records,
    _gActor03800Actor103800Animation05534Indices,
    { NULL, _gActor03800Actor103800Animation05534Bank1, NULL, NULL, _gActor03800Actor103800Animation05534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation055F0Bank1[2] = {
#include "assets/actor_103800_animation_055F0_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation055F0Bank4[4] = {
#include "assets/actor_103800_animation_055F0_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation055F0Records[24] = {
#include "assets/actor_103800_animation_055F0_records.inc"
};

static u16 _gActor03800Actor103800Animation055F0Indices[6] = {
#include "assets/actor_103800_animation_055F0_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation055F0 = {
    _gActor03800Actor103800Animation055F0Records,
    _gActor03800Actor103800Animation055F0Indices,
    { NULL, _gActor03800Actor103800Animation055F0Bank1, NULL, NULL, _gActor03800Actor103800Animation055F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05734Bank1[7] = {
#include "assets/actor_103800_animation_05734_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05734Bank4[14] = {
#include "assets/actor_103800_animation_05734_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05734Records[33] = {
#include "assets/actor_103800_animation_05734_records.inc"
};

static u16 _gActor03800Actor103800Animation05734Indices[6] = {
#include "assets/actor_103800_animation_05734_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05734 = {
    _gActor03800Actor103800Animation05734Records,
    _gActor03800Actor103800Animation05734Indices,
    { NULL, _gActor03800Actor103800Animation05734Bank1, NULL, NULL, _gActor03800Actor103800Animation05734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05824Bank1[4] = {
#include "assets/actor_103800_animation_05824_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05824Bank4[12] = {
#include "assets/actor_103800_animation_05824_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05824Records[23] = {
#include "assets/actor_103800_animation_05824_records.inc"
};

static u16 _gActor03800Actor103800Animation05824Indices[6] = {
#include "assets/actor_103800_animation_05824_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05824 = {
    _gActor03800Actor103800Animation05824Records,
    _gActor03800Actor103800Animation05824Indices,
    { NULL, _gActor03800Actor103800Animation05824Bank1, NULL, NULL, _gActor03800Actor103800Animation05824Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05988Bank1[7] = {
#include "assets/actor_103800_animation_05988_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05988Bank4[19] = {
#include "assets/actor_103800_animation_05988_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05988Records[36] = {
#include "assets/actor_103800_animation_05988_records.inc"
};

static u16 _gActor03800Actor103800Animation05988Indices[6] = {
#include "assets/actor_103800_animation_05988_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05988 = {
    _gActor03800Actor103800Animation05988Records,
    _gActor03800Actor103800Animation05988Indices,
    { NULL, _gActor03800Actor103800Animation05988Bank1, NULL, NULL, _gActor03800Actor103800Animation05988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05B84Bank1[12] = {
#include "assets/actor_103800_animation_05B84_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05B84Bank4[12] = {
#include "assets/actor_103800_animation_05B84_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05B84Records[66] = {
#include "assets/actor_103800_animation_05B84_records.inc"
};

static u16 _gActor03800Actor103800Animation05B84Indices[6] = {
#include "assets/actor_103800_animation_05B84_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05B84 = {
    _gActor03800Actor103800Animation05B84Records,
    _gActor03800Actor103800Animation05B84Indices,
    { NULL, _gActor03800Actor103800Animation05B84Bank1, NULL, NULL, _gActor03800Actor103800Animation05B84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05E14Bank1[16] = {
#include "assets/actor_103800_animation_05E14_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05E14Bank4[39] = {
#include "assets/actor_103800_animation_05E14_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05E14Records[64] = {
#include "assets/actor_103800_animation_05E14_records.inc"
};

static u16 _gActor03800Actor103800Animation05E14Indices[6] = {
#include "assets/actor_103800_animation_05E14_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05E14 = {
    _gActor03800Actor103800Animation05E14Records,
    _gActor03800Actor103800Animation05E14Indices,
    { NULL, _gActor03800Actor103800Animation05E14Bank1, NULL, NULL, _gActor03800Actor103800Animation05E14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05F18Bank1[4] = {
#include "assets/actor_103800_animation_05F18_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05F18Bank4[12] = {
#include "assets/actor_103800_animation_05F18_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05F18Records[28] = {
#include "assets/actor_103800_animation_05F18_records.inc"
};

static u16 _gActor03800Actor103800Animation05F18Indices[6] = {
#include "assets/actor_103800_animation_05F18_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05F18 = {
    _gActor03800Actor103800Animation05F18Records,
    _gActor03800Actor103800Animation05F18Indices,
    { NULL, _gActor03800Actor103800Animation05F18Bank1, NULL, NULL, _gActor03800Actor103800Animation05F18Bank4, NULL, NULL, NULL },
};

DamageAttack Actor03800_D05F40[1] = {
    { 6, 7 },
};

EnemyParams Actor03800_D05F44 = { Actor03800_D05F40, 280, 15, 53, 1, 0, 10, 100, 0 };

TaskDesc Actor03800_D05F54 = { { { TASK_BODY_TMD, 96 } }, Actor03800_Fn0315C, { .model = &_gActor03800BlackBeetleBody } };

AnimationSet* Actor03800_D05F60[12] = {
    NULL,
    &_gActor03800Actor103800Animation04B24,
    &_gActor03800Actor103800Animation04D20,
    &_gActor03800Actor103800Animation04FE0,
    &_gActor03800Actor103800Animation05534,
    &_gActor03800Actor103800Animation055F0,
    &_gActor03800Actor103800Animation05734,
    &_gActor03800Actor103800Animation05824,
    &_gActor03800Actor103800Animation05988,
    &_gActor03800Actor103800Animation05B84,
    &_gActor03800Actor103800Animation05E14,
    &_gActor03800Actor103800Animation05F18,
};

s16 Actor03800_D05F90[12] = {
    0,
    8,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
};

s16 Actor03800_D05FA8[2] = {
    2,
    1,
};

static void        Actor03800_Fn01150(Task* arg0);
static void        Actor03800_Fn012B4(Task* arg0);
static void        Actor03800_Fn01520(Task* arg0);
static void        Actor03800_Fn0166C(Task* arg0);
static void        Actor03800_Fn01948(Task* arg0);
static void        Actor03800_Fn01AD0(Task* arg0);
static void        Actor03800_Fn01C50(Task* arg0);
static void        Actor03800_Fn01EEC(Task* arg0);
static void        Actor03800_Fn02068(Task* arg0);
static void        Actor03800_Fn021E4(Task* arg0);
static void        Actor03800_Fn02584(Task* arg0);
static inline void _actor03800TickAnim(Task* task);

static void Actor03800_Fn000B8(Enemy* arg0, Task* arg1)
{
    WorldCollisionBody*    obj;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    Actor103800Work*       work;
    s32                    i;
    TmdObject*             extra;

    extra = arg1->extra.tmd;
    work  = memCalloc(0x384, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work      = work;
    extra->lightMtx = &work->field_184;
    extra->flags    = 0;
    extra->colorMtx = &work->field_164;
    Gp_IncStateF0Ref(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->field_344->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = &arg1->extra.tmd->coords[3];
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->recs                   = work->field_1C4;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &Actor03800_D05F44;
    arg0->hp                     = (s16)Actor03800_D05F44.hpMax;
    work->field_2C4.coord        = &arg1->extra.tmd->coords[3];
    work->field_2C4.spawnArgLo   = 0x200;
    work->field_2C4.spawnArgHi   = 1;
    animationInitContext(&work->anim, Actor03800_D05F60, extra, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_104, work->slots);
    for (i = 1; i < 6; i++) {
        animationResetSlot(&work->anim, i, 1);
    }
    work->field_1AC = arg1->extra.tmd->coords;
    records1        = work->field_1C4;
    work->field_1B6 = -0xFA;
    work->field_1B0 = records1;
    work->field_1B4 = 0;
    work->field_1B8 = 0;
    work->field_1BC = 0x30026;
    work->field_1C0 = 0xFA;
    work->field_1C2 = 1U;
    Gp_LinkObj(2, (WorldCollisionBody*)work->field_1A4);
    Gp_InitRec18Table(records1, 3, 0);
    work->field_214 = arg1->extra.tmd->coords;
    records2        = work->field_22C;
    work->field_21E = -0x12C;
    work->field_218 = records2;
    work->field_21C = 0;
    work->field_220 = 0;
    work->field_224 = 0x30026;
    work->field_228 = 0x12C;
    work->field_22A = 1U;
    Gp_LinkObj(2, (WorldCollisionBody*)work->field_20C);
    Gp_InitRec18Table(records2, 4, 0);
    switch (work->field_350) {
        case 0:
            work->field_1C2 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_22A |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 1:
            work->field_1C2 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_22A &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 2:
            work->field_1C2 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_22A &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 3:
            work->field_1C2 &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_22A &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
    }
    obj             = (WorldCollisionBody*)work->field_28C;
    work->field_294 = arg1->extra.tmd->coords;
    records3        = work->field_2AC;
    work->field_29E = -0xFA;
    work->field_2A0 = 0xFA;
    work->field_2A8 = 0xC8;
    work->field_298 = records3;
    work->field_29C = 0;
    work->field_2A4 = 0;
    work->field_2AA = 1U;
    Gp_LinkObj(3, obj);
    Gp_InitRec18Table(records3, 1, 0);
    work->field_2AA = (u16)(work->field_2AA | WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->state     = 1;
}

/// Applies the spawn variant (`AreaPlacement::mode`) to the freshly allocated
/// work block: the tens digit picks the mode (`field_350`) and the units digit
/// of mode 0 the idle pose, seeding the look-around countdown from the LCG.
/// Modes 1 and 2 instead detach the model: the work block's own coordinate is
/// seeded from the model's, parented to `gGfxViewCoord` and published on
/// `field_344`, while the model's coordinate is reset to an identity rotation
/// at the origin and re-parented under it. `Gp_MulMatrix0`-style GTE column
/// products then turn the detached coordinate by 0x400 / 0x800 about X.
static void Actor03800_Fn003B8(Task* arg0)
{
    Actor103800Work* work;
    Enemy*           ctx;
    GfxCoord*        src;
    GfxMatrix*       mtx;
    GfxMatrix*       srcmtx;
    GfxMatrix*       mtx2;
    GfxMatrix*       srcmtx2;
    SVECTOR          rot;
    MATRIX           mat;
    s16              mode;
    s16              kind;

    ctx  = (Enemy*)arg0->spawnArg2.pointer;
    work = (Actor103800Work*)arg0->work;
    src  = arg0->extra.tmd->coords;
    mode = ctx->place->mode / 10;

    work->field_350 = mode;
    switch (mode) {
        case 0:
            kind            = ctx->place->mode % 10;
            work->field_352 = kind;
            switch (kind) {
                case 0:
                    work->field_348 = 1;
                    work->field_37A = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_356 = ((gRandomLcgState >> 0x10) & 0xFF) + 0x5A;
                    break;
                case 1:
                    work->field_348 = 2;
                    work->field_356 = 0;
                    work->field_37A = 1;
                    break;
            }
            work->field_366 = 0x80;
            work->field_372 = 0x80;
            work->field_344 = arg0->extra.tmd->coords;
            break;
        case 1:
            work->field_352 = 8;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx                       = (GfxMatrix*)&work->coord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;

            work->coord.parent     = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx                       = (GfxMatrix*)&src->coord;
            srcmtx->rotationWords.m00M01 = ONE;
            srcmtx->rotationWords.m02M10 = 0;
            srcmtx->rotationWords.m11M12 = ONE;
            srcmtx->rotationWords.m20M21 = 0;
            srcmtx->rotationWords.m22    = ONE;

            src->parent     = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x400;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 2:
            work->field_352 = 9;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx2                       = (GfxMatrix*)&work->coord.coord;
            mtx2->rotationWords.m00M01 = ONE;
            mtx2->rotationWords.m02M10 = 0;
            mtx2->rotationWords.m11M12 = ONE;
            mtx2->rotationWords.m20M21 = 0;
            mtx2->rotationWords.m22    = ONE;

            work->coord.parent     = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx2                       = (GfxMatrix*)&src->coord;
            srcmtx2->rotationWords.m00M01 = ONE;
            srcmtx2->rotationWords.m02M10 = 0;
            srcmtx2->rotationWords.m11M12 = ONE;
            srcmtx2->rotationWords.m20M21 = 0;
            srcmtx2->rotationWords.m22    = ONE;

            src->parent     = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x800;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 3:
            work->field_352 = 0xB;
            work->field_366 = 0;
            work->field_372 = -1;
            work->field_344 = arg0->extra.tmd->coords;
            break;
    }
}

static void Actor03800_Fn00974(Task* arg0)
{
    Enemy*           ctx;
    Actor103800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_BUILDUP) {
        if (work->field_350 == 0) {
            ctx->reactionFlags = flags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->field_352    = 6;
            work->field_354    = 0;
            work->field_356    = 0;
            work->field_37E    = 1;
        } else if ((work->field_352 != 0xA) || (work->field_354 >= 4)) {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->field_352 = 7;
            } else {
                work->field_352 = 5;
            }
            work->field_354 = 0;
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Keeps the deepest contact seen so far: when `depth` beats `best`, it
/// becomes the new `best`, and `frame->dir` the direction to push out along -
/// the offset in `frame->delta` normalised and carried into the collision
/// grid's frame.
#define _ACTOR03800_KEEP_DEEPEST(best, depth, frame)                                                   \
    do {                                                                                               \
        if ((best) < (depth)) {                                                                        \
            (best) = (depth);                                                                          \
            VectorNormal(&(frame)->delta.vector, &(frame)->normal);                                    \
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &(frame)->normal, &(frame)->dir); \
        }                                                                                              \
    } while (0)

static void Actor03800_Fn00A98(Task* arg0)
{
    Actor103800Work*    work;
    ActorWallPushFrame* frame;
    Enemy*              ctx;
    GfxCoord*           coord;
    GfxCoord*           sourceCoord;
    s32                 push;
    s32                 reaction;
    s32                 result;
    s32                 i;
    s32                 depth;
    s32                 boundedDepth;
    s32                 dx;
    s32                 dy;
    s32                 dz;
    u32                 lastId;
    u32                 id;
    u32                 hitId;
    u32                 damage;

    push     = 0;
    reaction = 0;
    lastId   = 0;
    work     = arg0->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorWallPushFrame);
    frame  = SCRATCH_STACK_CURSOR(ActorWallPushFrame);
    coord  = work->field_344;
    ctx    = arg0->spawnArg2.pointer;
    result = func_800E0C10(work->field_22C, &frame->delta, 4, NULL);
    if (result != 0) {
        for (i = 0; i < 4; i++) {
            if ((work->field_22C[i].key.value & 0xFFFF0000) == 0x100000) {
                if (work->field_22C[i].response.direction.vy >= -0xDDA) {
                    if (work->field_36E == 0) {
                        work->field_370 = 1;
                        break;
                    }
                } else {
                    work->field_374 = 1;
                }
            }
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.fixed.vx.halves.integer;
                coord->coord.t[1] += frame->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += frame->delta.fixed.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->field_2EC;
                coord->coord.t[1] = work->field_2EE;
                coord->coord.t[2] = work->field_2F0;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_22C);
    if (work->field_34E != 0) {
        if (--work->field_34E <= 0) {
            work->field_34E = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = work->field_1C4[i].key.value;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->field_34E == 0) {
                    sourceCoord            = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vector.vx = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vy = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vz = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage                 = Gp_ComputeDamage(work->field_1C4[i].key.value, SquareRoot0((frame->delta.vector.vx * frame->delta.vector.vx) + (frame->delta.vector.vy * frame->delta.vector.vy) + (frame->delta.vector.vz * frame->delta.vector.vz)), 0, 0);
                    if (work->field_36E == 0) {
                        if (Gp_RollEnemyChance(ctx, work->field_1C4[i].key.value, 0) != 0) {
                            damage *= 4;
                            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, NULL);
                        }
                    } else if (!(work->field_1C4[i].key.value & 0x8000) && (damage != 0)) {
                        damage *= 3;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 4, NULL);
                    }
                    func_800DA6E8(&ctx->node, damage, 0);
                    func_800E2C78(ctx, work->field_1C4[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        reaction = 2;
                    }
                    switch (Gp_GetIdParam0(work->field_1C4[i].key.value) & 0xFFFF) {
                        case 0:
                        default:
                            break;
                        case 3:
                            Gp_SetObjFlag4(ctx, work->field_1C4[i].key.value, 0);
                            break;
                        case 4:
                            if (ctx->hp > 0) {
                                if (work->field_36E == 0) {
                                    reaction = 1;
                                }
                            } else {
                                work->field_368 = 1;
                            }
                            break;
                        case 6:
                            if (ctx->hp <= 0) {
                                work->field_368 = 1;
                            } else if (work->field_36E == 0) {
                                reaction = 1;
                            }
                            break;
                        case 8:
                            if (work->field_36E == 0 && reaction == 0) {
                                Gp_SetObjFlag2(ctx, work->field_1C4[i].key.value, 0);
                            }
                            break;
                        case 1:
                        case 2:
                        case 5:
                        case 9:
                            if (work->field_36E == 0 && reaction == 0) {
                                reaction = 1;
                            }
                            break;
                    }
                    switch (reaction) {
                        case 0:
                            if (work->field_352 != 0xA && damage != 0) {
                                work->field_352 = 5;
                                work->field_354 = 0;
                            }
                            break;
                        case 1:
                            work->field_352 = 3;
                            work->field_354 = 0;
                            break;
                        case 2:
                            work->field_352 = 7;
                            work->field_354 = 0;
                            break;
                    }
                    hitId = work->field_1C4[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, arg0->extra.tmd->coords + 3, NULL, &work->field_2C4);
                    }
                    result = Gp_GetIdParam2(work->field_1C4[i].key.value);
                    if (result > 0) {
                        work->field_34E = result;
                    }
                }
                break;
            case 1:
                dx                     = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vector.vx = dx;
                dy                     = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vector.vy = dy;
                dz                     = coord->workm.t[2] - work->field_1C4[i].point.vz;
                frame->delta.vector.vz = dz;
                depth                  = work->field_1C4[i].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
            case 3:
                dx                     = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vector.vx = dx;
                dy                     = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vector.vy = dy;
                dz                     = coord->workm.t[2] - work->field_1C4[i].point.vz;
                frame->delta.vector.vz = dz;
                depth                  = work->field_1C4[i].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
        }
    }
    if (push > 0 && work->field_350 == 0) {
        coord->coord.t[0] += (push * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    work->field_36C = 0;
    result          = Gp_CountRec18Hi(work->field_2AC, 0x10000);
    if (result != 0) {
        Gp_ArmStateF0(1);
        work->field_36C = 1;
        work->field_37A = 1;
        if ((work->field_350 == 0) && (work->field_36E == 0) && (work->field_352 != 0xC)) {
            work->field_352 = 0xC;
            work->field_354 = 0;
        }
    }
    Gp_ClearRec18Occupied(work->field_2AC);
    SCRATCH_STACK_RELEASE_BLOCK(ActorWallPushFrame);
}

static void Actor03800_Fn01150(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                turn            = (gRandomLcgState >> 16) & 0x3FF;
                if (((gRandomLcgState >> 16) & 0x400) == 0) {
                    turn = -turn;
                }
                work->field_348 = 2;
                work->field_36A = 1;
                work->field_364 = (work->field_362 + turn) & 0xFFF;
            }
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                if (work->field_37A == 0) {
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_356 = ((gRandomLcgState >> 16) & 0xFF) + 0x5A;
                } else {
                    work->field_352 = 1;
                    work->field_354 = 0;
                    if (work->field_36A == 0) {
                        work->field_36A = 1;
                    }
                }
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->field_352 = 2;
        work->field_354 = 0;
        if (work->field_36A == 0) {
            work->field_36A = 1;
        }
        work->field_37A = 1;
    }
}

static void Actor03800_Fn012B4(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;
    s32              delta;
    s32              turn2;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35E = 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            turn            = (gRandomLcgState >> 16) & 0x1FF;
            if (((gRandomLcgState >> 16) & 0x400) == 0) {
                turn = -turn;
            }
            delta = turn;
            if (work->field_370 != 0) {
                delta           = turn + 0x800;
                work->field_370 = 0;
            }
            work->field_348 = 2;
            work->field_354 = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            turn            = 0; /* dead store: keeps `turn` cse-canonical over `delta` */
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_356 = ((gRandomLcgState >> 16) & 0xF) + 0x19;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_358 = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_356 = ((gRandomLcgState >> 16) & 0xF) + 0x19;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                turn2           = (gRandomLcgState >> 16) & 0x1FF;
                if (((gRandomLcgState >> 16) & 0x400) == 0) {
                    turn2 = -turn2;
                }
                work->field_364 = (work->field_362 + turn2) & 0xFFF;
            }
            if (work->field_370 != 0) {
                if (work->field_35C < 0x1E) {
                    work->field_352 = 0;
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_356 = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                } else {
                    work->field_352 = 0xC;
                    work->field_354 = 0;
                }
            }
            work->field_358--;
            if (work->field_358 <= 0) {
                work->field_352 = 0;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_356 = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->field_352 = 2;
        work->field_354 = 0;
    }
}

static void Actor03800_Fn01520(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            vec.vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_364 = ratan2((s16)vec.vx, (s16)vec.vz) & 0xFFF;
            work->field_348 = 9;
            if (work->field_36A == 0) {
                work->field_36A = 1;
            }
            work->field_354 = 1;
            break;
        case 1:
            work->field_360 = 0x28;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                work->field_354 = 2;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_356 = ((gRandomLcgState >> 16) & 0x1F) + 0x78;
            }
            break;
        case 2:
            work->field_360 = 0;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            break;
    }
}

static void Actor03800_Fn0166C(Task* arg0)
{
    Actor03800MoveScratch* scratch;
    Actor103800Work*       work;
    Enemy*                 ctx;
    GfxCoord*              coord;
    s16                    state;
    s32                    snd;
    s32                    pan;
    s32                    pan2;

    scratch = (Actor03800MoveScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    state   = work->field_354;
    coord   = work->field_344;
    switch (state) {
        case 0:
            work->field_348  = 3;
            work->field_354  = 1;
            work->field_36A  = 0;
            work->field_36E  = 1;
            work->field_2AA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            snd              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260003;
            pan              = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if ((u32)(work->field_34C - 2) < 12) {
                scratch->delta.vx = coord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
                scratch->delta.vy = coord->coord.t[1] - gPlayerStatus.coordMtx->t[1];
                scratch->delta.vz = coord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
                VectorNormalS(&scratch->delta, &scratch->normal);
                coord->coord.t[0] += (scratch->normal.vx * 17) >> 9;
                coord->coord.t[2] += (scratch->normal.vz * 17) >> 9;
            } else {
                work->field_35C = 0;
                work->field_35E = 0;
            }
            if ((s16)work->field_34C == 12) {
                snd  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260002;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_34C >= 29) {
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
            break;
        case 2:
            work->field_37C--;
            if (work->field_37C <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void Actor03800_Fn01948(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    GfxCoord*        coord;
    s16              state;
    s32              snd;
    s32              pan;

    state = work->field_354;
    coord = work->field_344;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                if (work->field_36E == 0) {
                    work->field_348 = 0xB;
                    work->field_356 = 0xC;
                } else {
                    work->field_348 = 6;
                    work->field_356 = 0x13;
                }
            } else {
                work->field_348 = 7;
                work->field_356 = 0;
                work->field_352 = 0xA;
            }
            work->field_354 = 1;
            work->field_34A = 1;
            work->field_36A = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_360 = 0;
            snd             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260003;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            work->field_356 -= 1;
            if (work->field_356 > 0) {
                break;
            }
            if (work->field_36E == 0) {
                if (work->field_37E == 0) {
                    work->field_352 = 2;
                    work->field_354 = 0;
                } else {
                    work->field_352 = 6;
                    work->field_354 = 0;
                    work->field_356 = 0;
                }
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
            }
            break;
    }
}

static void Actor03800_Fn01AD0(Task* arg0)
{
    Enemy*           ctx;
    Actor103800Work* work;

    work            = arg0->work;
    ctx             = arg0->spawnArg2.pointer;
    work->field_360 = 0;
    work->field_35C = 0;
    work->field_35E = 0;
    work->field_356--;
    if (work->field_356 <= 0) {
        if (work->field_36E == 0) {
            work->field_348 = 0xB;
        } else {
            work->field_348 = 6;
        }
        work->field_34A = 1;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_356 = ((gRandomLcgState >> 16) & 7) + 3;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work->field_37E = 0;
        if (work->field_350 == 0) {
            if (work->field_36E == 0) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
        } else {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
}

static void Actor03800_Fn01C50(Task* arg0)
{
    SVECTOR          rotation;
    MATRIX           matrix;
    Actor103800Work* work;
    GfxCoord*        coord;
    s16              state;

    work  = arg0->work;
    state = work->field_354;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                arg0->state     = 2;
                work->field_354 = 0;
                return;
            }
            work->field_366  = 0x80;
            work->field_372  = 0x80;
            work->field_354  = 1;
            work->field_22A |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            return;
        case 1:
            if (work->field_374 != 0) {
                work->field_354 = 2;
                return;
            }
        default:
            return;
        case 2:
            work->field_348 = 5;
            work->field_354 = 3;
            return;
        case 3:
            rotation.vx = 0;
            rotation.vy = (u16)work->field_362;
            rotation.vz = 0;
            RotMatrix(&rotation, &matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->parent       = &gGfxViewCoord;
            coord->coord        = work->field_2CC;
            coord->coord.t[0]   = work->coord.coord.t[0];
            coord->coord.t[1]   = work->coord.coord.t[1];
            coord->coord.t[2]   = work->coord.coord.t[2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            work->field_356 = 0xF;
            work->field_344 = coord;
            work->field_354 = 4;
            return;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_378 = 1;
                work->field_354 = 0;
                arg0->state     = 2;
            }
            break;
    }
}

/// Second copy of the idle "look around" tick; identical body to
/// `Actor03800_Fn02068`, which the overlay carries twice.
static void Actor03800_Fn01EEC(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_354 = 1;
            rand            = gRandomLcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = (gRandomLcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->field_36C != 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = (gRandomLcgState >> 16 & 0xF) + 0xF;
    }
}

/// Idle "look around" tick. State 0 counts `field_356` down and, on expiry,
/// picks a new facing `field_364` within +/-0x3FF of the current one; state 1
/// waits for the turn to finish and re-arms the countdown. Either way, an
/// active `gSceneCombatState.signals.bytes.actionFlags` bit (1 or 4) or a non-zero `field_36C` aborts
/// back to state 0 with a short delay.
static void Actor03800_Fn02068(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_354 = 1;
            rand            = gRandomLcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = (gRandomLcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->field_36C != 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = (gRandomLcgState >> 16 & 0xF) + 0xF;
    }
}

static void Actor03800_Fn021E4(Task* arg0)
{
    Enemy*                 ctx;
    Actor103800Work*       work;
    GfxCoord*              coord;
    Actor03800TurnScratch* scratch;
    s32                    sound;
    s32                    pan;

    scratch = (Actor03800TurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(*scratch));
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    ctx     = arg0->spawnArg2.pointer;
    switch (work->field_354) {
        case 0:
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
            }
            break;
        case 1:
            work->field_366  = 0x100;
            work->field_372  = 0x80;
            work->field_22A |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            if (work->field_374 != 0) {
                work->field_374  = 0;
                work->field_354  = 2;
                work->field_366  = 0x80;
                work->field_2AA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sound            = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260002;
                pan              = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 2:
            work->field_348 = 8;
            work->field_354 = 3;
            break;
        case 3:
            scratch->rotation.vx = 0;
            scratch->rotation.vy = (u16)work->field_362;
            scratch->rotation.vz = 0;
            RotMatrix(&scratch->rotation, &scratch->matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&scratch->matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&scratch->matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&scratch->matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->coord        = work->field_2CC;
            coord->coord.t[0]   = work->coord.coord.t[0];
            coord->coord.t[1]   = work->coord.coord.t[1];
            coord->coord.t[2]   = work->coord.coord.t[2];
            coord->parent       = &gGfxViewCoord;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            work->field_354 = 4;
            work->field_344 = coord;
            work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            break;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// State 12 of `Actor03800_Fn032D8`: pick and hold a turn direction while the
/// actor is aiming at the player. State 0 chooses the side to turn towards -
/// clockwise (1) when the player is in front of the actor's local +Z axis,
/// anticlockwise (2) otherwise - and a pending `field_370` request forces the
/// clockwise side. States 1 and 2 drive `field_35C` by -/+0x7D while the yaw
/// error `field_34C` is inside 8..0x10 and hand over to state 3 once it grows
/// past 0x10; state 3 finishes the move and hands back to `field_352` 1.
static void Actor03800_Fn02584(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            vec.vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_348 = 0xA;
            if (work->field_370 != 0) {
                work->field_370 = 0;
                work->field_354 = 1;
            } else {
                work->field_354 =
                    ((vec.vx * coord->coord.m[0][2]) + (vec.vz * coord->coord.m[2][2]) > 0) ? 1 : 2;
            }
            work->field_2AA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;

        case 1:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = -0x7D;
                break;
            }
            goto turn_done;

        case 2:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = 0x7D;
                break;
            }
        turn_done:
            work->field_35C = 0;
            if ((s16)work->field_34C >= 0x11) {
                work->field_354 = 3;
            }
            break;

        case 3:
            if ((s16)work->field_34C >= 0x14) {
                work->field_352  = 1;
                work->field_354  = 0;
                work->field_372  = 0x80;
                work->field_2AA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
}

static void Actor03800_Fn026F8(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_364;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_362 = ang;
    if (adiff < 0x800) {
        step = work->field_360;
        if (step >= adiff) {
            work->field_362 = want;
        } else {
            next = work->field_362;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_362 = next;
        }
    } else {
        step = work->field_360;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_362 = work->field_364;
        goto done;
    turn:
        wrapStep = work->field_360;
        cur      = work->field_362;
        if (diff > 0) {
            work->field_362 = cur - wrapStep;
        } else {
            work->field_362 = cur + wrapStep;
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_362;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor03800_Fn02848(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    s16              next;
    s16              speed;
    u16              value;
    s32              scale;
    Actor103800Work* work2;

    work  = arg0->work;
    coord = work->field_344;
    work2 = work;
    if (work->field_35E != 0) {
        next            = (u16)work->field_35C + (u16)work->field_35E;
        work->field_35C = next;
        if (next >= 0x33) {
            work->field_35C = 0x32;
        }
    }
    speed = work2->field_35C;
    if (speed > 0) {
        scale = speed * 0x190;
        value = Actor03800_D05F40[0].power + ((Actor03800_D05F40[0].power * scale) / 10000);
    } else {
        value = Actor03800_D05F40[0].power;
    }
    work2->field_2A4 = (((s16)value | (Actor03800_D05F40[0].reaction << DAMAGE_ATTACK_REACTION_SHIFT)) & 0xFFFF) |
                       DAMAGE_ATTACK_CATEGORY;
    work->field_2EC    = (s16)coord->coord.t[0];
    work->field_2EE    = (s16)coord->coord.t[1];
    work->field_2F0    = (s16)coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_35C) >> 0xC;
    coord->coord.t[1] += work->field_366;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_35C) >> 0xC;
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void _actor03800TickAnim(Task* task)
{
    Actor103800Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if ((s16)work->field_348 != work->field_34A) {
        work->field_34A = work->field_348;
        work->field_34C = 0;
        value           = Actor03800_D05F90[(s16)work->field_348];
        for (i = 1; i < 6; i++) {
            animationSeekSlotWithBlend(&work->anim, i, (s16)work->field_348, 0, value);
        }
    } else {
        work->field_34C++;
        for (i = 1; i < 6; i++) {
            animationTickSlot(&work->anim, i);
        }
    }
}

static void Actor03800_Fn02998(Enemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           vec;
    s32              state;
    s16              st;
    s16              phase;
    s16              anim;
    s32              snd;
    s32              pan;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    state = gSceneCombatState.actorControl;
    coord = work->field_344;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    st = work->field_354;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    if (st == 3) {
        goto case3;
    }
    return;
death:
    if (work->field_378 == 0) {
        anim = 1;
        if (work->field_36E != 0) {
            anim = 5;
        }
        work->field_348 = anim;
    }
    work->field_356 = 0;
    work->field_35A = 0x1000;
    work->field_2CC = coord->coord;
    arg0->recs      = 0;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_1A4);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_20C);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_28C);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 0x26);
    work->field_354 = 1;
    if (work->field_368 != 0) {
        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_354 = 3;
    }
    _actor03800TickAnim(arg1);
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    snd = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260004;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
dying:
    Actor03800_Fn037E0(arg1);
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->field_356 == 15) {
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
    }
    if (work->field_356 >= 0x3C) {
        work->field_354 = 2;
        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    _actor03800TickAnim(arg1);
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    enemyDestroy(arg0, arg1);
    return;
case3:
    if (work->field_368 == 0) {
        goto timer;
    }
    if (work->field_368 < 2) {
        goto inc368;
    }
    work->field_368 = 0;
    Tmd_FreeBuffers(obj);
    obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor03800_Fn02E50(arg1);
    goto timer;
inc368:
    work->field_368++;
timer:
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_354 = 2;
}

static void Actor03800_Fn02E50(Task* actor)
{
    s16  variants[4];
    s16* out;
    s32  i;
    s32  count;
    u32  first;
    s32  second;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    first           = (u16)((gRandomLcgState >> 16) % 5);
    Actor03800_Fn03008(actor, first);
    if (gSceneCombatState.battleRefs < 2) {
        count = Actor03800_D05FA8[gSceneCombatState.battleRefs];
        out   = variants;
        for (i = 0; i < 5; i++) {
            if (i != first) {
                *out++ = i;
            }
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        second          = (gRandomLcgState >> 16) & 3;
        Actor03800_Fn03008(actor, variants[second]);
        if (--count > 0) {
            s16* next = variants;

            for (i = 0; i < 5; i++) {
                if (i != first && i != second) {
                    *next++ = i;
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Actor03800_Fn03008(actor, variants[(u16)((gRandomLcgState >> 16) % 3)]);
        }
    }
}

static void Actor03800_Fn03008(Task* actor, u32 variant)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    EffectWork*      eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    switch (variant) {
        case 0:
            D_80067704[0] = &_gActor03800BlackBeetleEffect1;
            break;
        case 1:
            D_80067704[0] = &_gActor03800BlackBeetleEffect2;
            break;
        case 2:
            D_80067704[0] = &_gActor03800BlackBeetleEffect3;
            break;
        case 3:
            D_80067704[0] = &_gActor03800BlackBeetleEffect4;
            break;
        case 4:
            D_80067704[0] = &_gActor03800BlackBeetleEffect5;
            break;
    }
    eff = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 3, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    layout = Gp_GetNestedAreaRec(&key);

    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

static void Actor03800_Fn0315C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor03800_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor03800_Fn031B8(Enemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = gSceneCombatState.actorControl;
    one   = 1;
    work  = arg1->work;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg1->extra.tmd->flags       = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

static void Actor03800_Fn032D8(Task* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->work;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

static void Actor03800_Fn03420(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    s32              state;

    state = work->field_354;
    switch (state) {
        case 0:
            work->field_348 = 4;
            work->field_354 = 1;
            break;
        case 1:
            if ((s16)work->field_34C == 0x1E) {
                work->field_36E = 0;
            }
            if ((s16)work->field_34C >= 0x3A) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = state;
                }
                work->field_2AA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
}

static void Actor03800_Fn034B0(Task* arg0)
{
    TmdObject*       obj;
    Enemy*           ctx;
    Actor103800Work* work;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    switch (gSceneCombatState.shrineEnemyPhase) {
        case SCENE_COMBAT_SHRINE_HIDDEN:
            obj->flags                  = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_SHRINE_REVEALED:
            obj->flags       = 0;
            work->field_1C2 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_22A |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_2AA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_ArmStateF0(1);
            work->field_366 = 0x80;
            work->field_356 = 0x5A;
            work->field_350 = 0;
            work->field_372 = 0x80;
            return;
        case SCENE_COMBAT_SHRINE_RELEASED:
            if (--work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            return;
    }
}

static void Actor03800_Fn03594(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;

    work  = arg0->work;
    coord = work->field_344;
    if (--work->field_36A <= 0) {
        work->field_36A = 0xC;
        soundId         = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260001;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

static void Actor03800_Fn03628(Task* arg0)
{
    _actor03800TickAnim(arg0);
}

static void Actor03800_Fn036EC(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = ((Actor103800Work*)arg0->work)->field_344;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor03800_Fn03744(Task* arg0)
{
    Actor103800Work* work;
    GfxCoord*        coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = work->field_344;
    if (work->field_350 == 0) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x1F4, work->field_372);
        return;
    }
    hit = func_800EA1A8(MATRIX_TRANS(&coord->workm), &vec);
    if (hit != 0) {
        Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
    }
}

static void Actor03800_Fn037E0(Task* arg0)
{
    Actor103800Work*   work;
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    work                                    = arg0->work;
    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = work->field_344;
    if (work->field_35A >= 0x201) {
        work->field_35A = (u16)work->field_35A - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_35A;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_2CC;
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
