#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "types.h"

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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The actor's animation work area. `field_352` is the pose the actor asks
/// for, `field_354` the pose its slots were last re-queued for and
/// `field_356` the frame count accumulated while the two agree:
/// `Actor01500_Fn02958` re-seeds the slots from the per-state id table
/// when they differ and ticks them while they match.
typedef struct Actor101500Work {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[7];
    /* 0x12C */ byte                  field_12C[0x70]; // pose buffer, handed to `animationInitContext`
    /* 0x19C */ MATRIX                field_19C;       // model colour matrix
    /* 0x1BC */ MATRIX                field_1BC;       // model light matrix
    /* 0x1DC */ WorldCollisionBody    field_1DC;
    /* 0x1FC */ WorldCollisionContact field_1FC[3];
    /* 0x244 */ WorldCollisionBody    field_244;
    /* 0x264 */ WorldCollisionContact field_264[5];
    /* 0x2DC */ WorldCollisionBody    field_2DC;
    /* 0x2FC */ WorldCollisionContact field_2FC[1];
    /* 0x314 */ EffectSpawnArg        field_314; // record the hit's effect is spawned with
    /* 0x31C */ VECTOR3               field_31C; // position before this frame's step
    /* 0x328 */ byte                  pad_328[4];
    /* 0x32C */ MATRIX                field_32C;
    /* 0x34C */ s32                   field_34C;
    /* 0x350 */ s16                   field_350; // hit cooldown, reloaded from `Gp_GetIdParam2`
    /* 0x352 */ u16                   field_352;
    /* 0x354 */ s16                   field_354;
    /* 0x356 */ u16                   field_356;
    /* 0x358 */ s16                   field_358;
    /* 0x35A */ s16                   field_35A;
    /* 0x35C */ s16                   field_35C;
    /* 0x35E */ u16                   field_35E;
    /* 0x360 */ s16                   field_360;
    /* 0x362 */ s16                   field_362;
    /* 0x364 */ s16                   field_364;
    /* 0x366 */ s16                   field_366;
    /* 0x368 */ s16                   field_368;
    /* 0x36A */ s16                   field_36A;
    /* 0x36C */ s16                   field_36C;
    /* 0x36E */ s16                   field_36E;
    /* 0x370 */ s16                   field_370;
    /* 0x372 */ u16                   field_372;
    /* 0x374 */ s16                   field_374;
    /* 0x376 */ s16                   field_376;
    /* 0x378 */ s16                   field_378;
    /* 0x37A */ s16                   field_37A;
    /* 0x37C */ s16                   field_37C;
    /* 0x37E */ s16                   field_37E;
    /* 0x380 */ s16                   field_380;
    /* 0x382 */ s16                   field_382; // spawn variant, `AreaPlacement.variant`
} Actor101500Work;

/// Per-state animation id handed to `animationSeekSlotWithBlend`, indexed by `field_352`.
extern s16 Actor01500_D0A050[];

/// Fifteen vertical bob offsets cycled by `field_37C` while `field_352` is 5.
extern s16 Actor01500_D0A070[];

/// Sixteen frame counts the hovering states reload `field_362` from, picked
/// by a `gRandomLcgState` draw.
extern u16 Actor01500_D09FC8[];

/// Sixteen distances `field_35E` is reloaded from when the actor starts to
/// advance, picked by a `gRandomLcgState` draw.
extern u16 Actor01500_D09FE8[];

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Pair packed into the third collision object's `key` at spawn.
extern DamageAttack Actor01500_D09FB4;
/// The enemy's parameter record; `hpMax` seeds the hit points.
extern EnemyParams Actor01500_D09FB8;
/// Animation bank handed to `animationInitContext`.
extern AnimationSet* Actor01500_D0A014[15];

/// The four model streams `Actor01500_Fn01AB0` spawns effects from.
static TmdSource _gActor01500MindSucklerBurstHead;
static TmdSource _gActor01500MindSucklerBurstWing;
static TmdSource _gActor01500MindSucklerBurstStinger;
static TmdSource _gActor01500MindSucklerBurstTail;

/// Points `Actor01500_Fn020D8` measures against: the XZ of
/// `Actor01500_D0A090` is where it heads, its Y (`Actor01500_D0A090.vy`) the
/// height it settles below, and `gPlayerStatus.coordMtx` passing the X/Z bounds of
/// `Actor01500_D0A098` ends the state.
extern SVECTOR Actor01500_D0A090;
extern SVECTOR Actor01500_D0A098;

static void Actor01500_Fn00094(Enemy* arg0, Task* arg1);
static void Actor01500_Fn004EC(Task* actor);
static void Actor01500_Fn00AFC(Task* actor, s32 damage);
static void Actor01500_Fn00CA4(Task* actor);
static void Actor01500_Fn00FC4(Task* actor);
static void Actor01500_Fn011B0(Task* actor);
static void Actor01500_Fn015DC(Task* actor);
static void Actor01500_Fn01708(Task* actor);
static void Actor01500_Fn01838(Task* actor);
static void Actor01500_Fn01988(Task* actor);
static void Actor01500_Fn01AB0(Task* arg0);
static void Actor01500_Fn01DF0(Enemy* arg0, Task* arg1);
static void Actor01500_Fn020D8(Task* actor);
static void Actor01500_Fn02428(Task* task);
static void Actor01500_Fn02484(Enemy* enemy, Task* actor);
static void Actor01500_Fn025C8(Task* actor);
static void Actor01500_Fn026D8(Task* actor);
static void Actor01500_Fn027B0(Task* actor);
static void Actor01500_Fn0288C(Task* actor);
static void Actor01500_Fn028B0(Task* actor);
static void Actor01500_Fn02958(Task* actor);
static void Actor01500_Fn02A1C(Task* actor);
static void Actor01500_Fn02B14(Task* actor);
static void Actor01500_Fn02B70(Task* actor);
static void Actor01500_Fn02C34(Task* actor);

/// The actor's three task states - spawn, per-frame tick and teardown - run
/// by `Actor01500_Fn02428`.
static const EnemyTaskFuncTable3 Actor01500_D00004 = {
    {
        Actor01500_Fn00094,
        Actor01500_Fn02484,
        Actor01500_Fn01DF0,
    },
};

static void Actor01500_Fn02428(Task*);

static TmdBone _gActor01500MindSucklerBodySkeleton[7] = {
#include "assets/mind_suckler_body_skeleton.inc"
};

static u32 _gActor01500MindSucklerBodyPartVerts[7] = {
#include "assets/mind_suckler_body_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBodyVerts[90] = {
#include "assets/mind_suckler_body_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBodyNormals[96] = {
#include "assets/mind_suckler_body_normals.inc"
};

static u32 _gActor01500MindSucklerBodyStream[927] = {
#include "assets/mind_suckler_body_stream.inc"
};

static TmdSource _gActor01500MindSucklerBody = {
    0,
    5292,
    1000,
    7,
    _gActor01500MindSucklerBodyPartVerts,
    _gActor01500MindSucklerBodyVerts,
    _gActor01500MindSucklerBodyNormals,
    _gActor01500MindSucklerBodySkeleton,
    _gActor01500MindSucklerBodyStream,
};

static TmdBone _gActor01500MindSucklerBurstHeadSkeleton[1] = {
#include "assets/mind_suckler_burst_head_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstHeadPartVerts[1] = {
#include "assets/mind_suckler_burst_head_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstHeadVerts[13] = {
#include "assets/mind_suckler_burst_head_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstHeadNormals[13] = {
#include "assets/mind_suckler_burst_head_normals.inc"
};

static u32 _gActor01500MindSucklerBurstHeadStream[121] = {
#include "assets/mind_suckler_burst_head_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstHead = {
    0,
    768,
    0,
    1,
    _gActor01500MindSucklerBurstHeadPartVerts,
    _gActor01500MindSucklerBurstHeadVerts,
    _gActor01500MindSucklerBurstHeadNormals,
    _gActor01500MindSucklerBurstHeadSkeleton,
    _gActor01500MindSucklerBurstHeadStream,
};

static TmdBone _gActor01500MindSucklerBurstWingSkeleton[1] = {
#include "assets/mind_suckler_burst_wing_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstWingPartVerts[1] = {
#include "assets/mind_suckler_burst_wing_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstWingVerts[5] = {
#include "assets/mind_suckler_burst_wing_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstWingNormals[5] = {
#include "assets/mind_suckler_burst_wing_normals.inc"
};

static u32 _gActor01500MindSucklerBurstWingStream[42] = {
#include "assets/mind_suckler_burst_wing_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstWing = {
    0,
    240,
    0,
    1,
    _gActor01500MindSucklerBurstWingPartVerts,
    _gActor01500MindSucklerBurstWingVerts,
    _gActor01500MindSucklerBurstWingNormals,
    _gActor01500MindSucklerBurstWingSkeleton,
    _gActor01500MindSucklerBurstWingStream,
};

static TmdBone _gActor01500MindSucklerBurstStingerSkeleton[1] = {
#include "assets/mind_suckler_burst_stinger_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstStingerPartVerts[1] = {
#include "assets/mind_suckler_burst_stinger_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstStingerVerts[9] = {
#include "assets/mind_suckler_burst_stinger_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstStingerNormals[9] = {
#include "assets/mind_suckler_burst_stinger_normals.inc"
};

static u32 _gActor01500MindSucklerBurstStingerStream[68] = {
#include "assets/mind_suckler_burst_stinger_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstStinger = {
    0,
    420,
    0,
    1,
    _gActor01500MindSucklerBurstStingerPartVerts,
    _gActor01500MindSucklerBurstStingerVerts,
    _gActor01500MindSucklerBurstStingerNormals,
    _gActor01500MindSucklerBurstStingerSkeleton,
    _gActor01500MindSucklerBurstStingerStream,
};

static TmdBone _gActor01500MindSucklerBurstTailSkeleton[1] = {
#include "assets/mind_suckler_burst_tail_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstTailPartVerts[1] = {
#include "assets/mind_suckler_burst_tail_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstTailVerts[9] = {
#include "assets/mind_suckler_burst_tail_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstTailNormals[9] = {
#include "assets/mind_suckler_burst_tail_normals.inc"
};

static u32 _gActor01500MindSucklerBurstTailStream[63] = {
#include "assets/mind_suckler_burst_tail_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstTail = {
    0,
    392,
    0,
    1,
    _gActor01500MindSucklerBurstTailPartVerts,
    _gActor01500MindSucklerBurstTailVerts,
    _gActor01500MindSucklerBurstTailNormals,
    _gActor01500MindSucklerBurstTailSkeleton,
    _gActor01500MindSucklerBurstTailStream,
};

static AnimationPackedPose _gActor01500Actor101500Animation04DE0Bank1[13] = {
#include "assets/actor_101500_animation_04DE0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation04DE0Bank4[65] = {
#include "assets/actor_101500_animation_04DE0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation04DE0Records[94] = {
#include "assets/actor_101500_animation_04DE0_records.inc"
};

static u16 _gActor01500Actor101500Animation04DE0Indices[8] = {
#include "assets/actor_101500_animation_04DE0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation04DE0 = {
    _gActor01500Actor101500Animation04DE0Records,
    _gActor01500Actor101500Animation04DE0Indices,
    { NULL, _gActor01500Actor101500Animation04DE0Bank1, NULL, NULL, _gActor01500Actor101500Animation04DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation05130Bank1[13] = {
#include "assets/actor_101500_animation_05130_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation05130Bank4[65] = {
#include "assets/actor_101500_animation_05130_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation05130Records[94] = {
#include "assets/actor_101500_animation_05130_records.inc"
};

static u16 _gActor01500Actor101500Animation05130Indices[8] = {
#include "assets/actor_101500_animation_05130_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation05130 = {
    _gActor01500Actor101500Animation05130Records,
    _gActor01500Actor101500Animation05130Indices,
    { NULL, _gActor01500Actor101500Animation05130Bank1, NULL, NULL, _gActor01500Actor101500Animation05130Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation06660Bank1[88] = {
#include "assets/actor_101500_animation_06660_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation06660Bank4[480] = {
#include "assets/actor_101500_animation_06660_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation06660Records[598] = {
#include "assets/actor_101500_animation_06660_records.inc"
};

static u16 _gActor01500Actor101500Animation06660Indices[8] = {
#include "assets/actor_101500_animation_06660_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation06660 = {
    _gActor01500Actor101500Animation06660Records,
    _gActor01500Actor101500Animation06660Indices,
    { NULL, _gActor01500Actor101500Animation06660Bank1, NULL, NULL, _gActor01500Actor101500Animation06660Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07B90Bank1[88] = {
#include "assets/actor_101500_animation_07B90_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07B90Bank4[480] = {
#include "assets/actor_101500_animation_07B90_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07B90Records[598] = {
#include "assets/actor_101500_animation_07B90_records.inc"
};

static u16 _gActor01500Actor101500Animation07B90Indices[8] = {
#include "assets/actor_101500_animation_07B90_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07B90 = {
    _gActor01500Actor101500Animation07B90Records,
    _gActor01500Actor101500Animation07B90Indices,
    { NULL, _gActor01500Actor101500Animation07B90Bank1, NULL, NULL, _gActor01500Actor101500Animation07B90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07C7CBank1[2] = {
#include "assets/actor_101500_animation_07C7C_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07C7CBank4[9] = {
#include "assets/actor_101500_animation_07C7C_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07C7CRecords[30] = {
#include "assets/actor_101500_animation_07C7C_records.inc"
};

static u16 _gActor01500Actor101500Animation07C7CIndices[8] = {
#include "assets/actor_101500_animation_07C7C_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07C7C = {
    _gActor01500Actor101500Animation07C7CRecords,
    _gActor01500Actor101500Animation07C7CIndices,
    { NULL, _gActor01500Actor101500Animation07C7CBank1, NULL, NULL, _gActor01500Actor101500Animation07C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07D68Bank1[2] = {
#include "assets/actor_101500_animation_07D68_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07D68Bank4[9] = {
#include "assets/actor_101500_animation_07D68_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07D68Records[30] = {
#include "assets/actor_101500_animation_07D68_records.inc"
};

static u16 _gActor01500Actor101500Animation07D68Indices[8] = {
#include "assets/actor_101500_animation_07D68_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07D68 = {
    _gActor01500Actor101500Animation07D68Records,
    _gActor01500Actor101500Animation07D68Indices,
    { NULL, _gActor01500Actor101500Animation07D68Bank1, NULL, NULL, _gActor01500Actor101500Animation07D68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation08844Bank1[71] = {
#include "assets/actor_101500_animation_08844_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation08844Bank4[176] = {
#include "assets/actor_101500_animation_08844_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation08844Records[292] = {
#include "assets/actor_101500_animation_08844_records.inc"
};

static u16 _gActor01500Actor101500Animation08844Indices[8] = {
#include "assets/actor_101500_animation_08844_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation08844 = {
    _gActor01500Actor101500Animation08844Records,
    _gActor01500Actor101500Animation08844Indices,
    { NULL, _gActor01500Actor101500Animation08844Bank1, NULL, NULL, _gActor01500Actor101500Animation08844Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation092F4Bank1[67] = {
#include "assets/actor_101500_animation_092F4_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation092F4Bank4[176] = {
#include "assets/actor_101500_animation_092F4_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation092F4Records[293] = {
#include "assets/actor_101500_animation_092F4_records.inc"
};

static u16 _gActor01500Actor101500Animation092F4Indices[8] = {
#include "assets/actor_101500_animation_092F4_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation092F4 = {
    _gActor01500Actor101500Animation092F4Records,
    _gActor01500Actor101500Animation092F4Indices,
    { NULL, _gActor01500Actor101500Animation092F4Bank1, NULL, NULL, _gActor01500Actor101500Animation092F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09650Bank1[14] = {
#include "assets/actor_101500_animation_09650_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09650Bank4[65] = {
#include "assets/actor_101500_animation_09650_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09650Records[94] = {
#include "assets/actor_101500_animation_09650_records.inc"
};

static u16 _gActor01500Actor101500Animation09650Indices[8] = {
#include "assets/actor_101500_animation_09650_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09650 = {
    _gActor01500Actor101500Animation09650Records,
    _gActor01500Actor101500Animation09650Indices,
    { NULL, _gActor01500Actor101500Animation09650Bank1, NULL, NULL, _gActor01500Actor101500Animation09650Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09708Bank1[2] = {
#include "assets/actor_101500_animation_09708_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09708Bank4[5] = {
#include "assets/actor_101500_animation_09708_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09708Records[21] = {
#include "assets/actor_101500_animation_09708_records.inc"
};

static u16 _gActor01500Actor101500Animation09708Indices[8] = {
#include "assets/actor_101500_animation_09708_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09708 = {
    _gActor01500Actor101500Animation09708Records,
    _gActor01500Actor101500Animation09708Indices,
    { NULL, _gActor01500Actor101500Animation09708Bank1, NULL, NULL, _gActor01500Actor101500Animation09708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation098C0Bank1[7] = {
#include "assets/actor_101500_animation_098C0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation098C0Bank4[30] = {
#include "assets/actor_101500_animation_098C0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation098C0Records[45] = {
#include "assets/actor_101500_animation_098C0_records.inc"
};

static u16 _gActor01500Actor101500Animation098C0Indices[8] = {
#include "assets/actor_101500_animation_098C0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation098C0 = {
    _gActor01500Actor101500Animation098C0Records,
    _gActor01500Actor101500Animation098C0Indices,
    { NULL, _gActor01500Actor101500Animation098C0Bank1, NULL, NULL, _gActor01500Actor101500Animation098C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09A78Bank1[7] = {
#include "assets/actor_101500_animation_09A78_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09A78Bank4[30] = {
#include "assets/actor_101500_animation_09A78_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09A78Records[45] = {
#include "assets/actor_101500_animation_09A78_records.inc"
};

static u16 _gActor01500Actor101500Animation09A78Indices[8] = {
#include "assets/actor_101500_animation_09A78_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09A78 = {
    _gActor01500Actor101500Animation09A78Records,
    _gActor01500Actor101500Animation09A78Indices,
    { NULL, _gActor01500Actor101500Animation09A78Bank1, NULL, NULL, _gActor01500Actor101500Animation09A78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09BC0Bank1[6] = {
#include "assets/actor_101500_animation_09BC0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09BC0Bank4[17] = {
#include "assets/actor_101500_animation_09BC0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09BC0Records[33] = {
#include "assets/actor_101500_animation_09BC0_records.inc"
};

static u16 _gActor01500Actor101500Animation09BC0Indices[8] = {
#include "assets/actor_101500_animation_09BC0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09BC0 = {
    _gActor01500Actor101500Animation09BC0Records,
    _gActor01500Actor101500Animation09BC0Indices,
    { NULL, _gActor01500Actor101500Animation09BC0Bank1, NULL, NULL, _gActor01500Actor101500Animation09BC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09F8CBank1[16] = {
#include "assets/actor_101500_animation_09F8C_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09F8CBank4[75] = {
#include "assets/actor_101500_animation_09F8C_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09F8CRecords[106] = {
#include "assets/actor_101500_animation_09F8C_records.inc"
};

static u16 _gActor01500Actor101500Animation09F8CIndices[8] = {
#include "assets/actor_101500_animation_09F8C_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09F8C = {
    _gActor01500Actor101500Animation09F8CRecords,
    _gActor01500Actor101500Animation09F8CIndices,
    { NULL, _gActor01500Actor101500Animation09F8CBank1, NULL, NULL, _gActor01500Actor101500Animation09F8CBank4, NULL, NULL, NULL },
};

DamageAttack Actor01500_D09FB4 = { 8, 7 };

EnemyParams Actor01500_D09FB8 = { &Actor01500_D09FB4, 50, 12, 36, 2, 100, 1, 100, 0 };

u16 Actor01500_D09FC8[16] = {
    30,
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    50,
    52,
    54,
    56,
    58,
    60,
    65,
};

u16 Actor01500_D09FE8[16] = {
    1000,
    1050,
    1100,
    1150,
    1200,
    1200,
    1250,
    1250,
    1300,
    1300,
    1350,
    1400,
    1450,
    1500,
    1550,
    1600,
};

TaskDesc Actor01500_D0A008 = { { { TASK_BODY_TMD, 96 } }, Actor01500_Fn02428, { .model = &_gActor01500MindSucklerBody } };

AnimationSet* Actor01500_D0A014[15] = {
    NULL,
    &_gActor01500Actor101500Animation04DE0,
    &_gActor01500Actor101500Animation05130,
    &_gActor01500Actor101500Animation06660,
    &_gActor01500Actor101500Animation07B90,
    &_gActor01500Actor101500Animation07C7C,
    &_gActor01500Actor101500Animation07D68,
    &_gActor01500Actor101500Animation08844,
    &_gActor01500Actor101500Animation092F4,
    &_gActor01500Actor101500Animation09650,
    &_gActor01500Actor101500Animation09708,
    &_gActor01500Actor101500Animation098C0,
    &_gActor01500Actor101500Animation09A78,
    &_gActor01500Actor101500Animation09BC0,
    &_gActor01500Actor101500Animation09F8C,
};

s16 Actor01500_D0A050[16] = {
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
    4,
    4,
    4,
    4,
    0,
};

s16 Actor01500_D0A070[16] = {
    0,
    10,
    19,
    24,
    25,
    22,
    15,
    5,
    -5,
    -15,
    -22,
    -25,
    -24,
    -19,
    -10,
    0,
};

SVECTOR Actor01500_D0A090 = { 1110, -0x2EE0, -2500, 0 };

SVECTOR Actor01500_D0A098 = { 1900, -0x2EE0, 1000, 0 };

/// Spawn handler: allocates the work area, binds the model and collision
/// objects, and seeds the pose from the spawn variant in `AreaPlacement.variant`.
static void Actor01500_Fn00094(Enemy* arg0, Task* arg1)
{
    Actor101500Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    AreaPlacement*         place;
    WorldCollisionContact* records;
    u32                    draw;
    s32                    i;
    s32                    r;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x384U, false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_19C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = &arg1->extra.tmd->coords[2];
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &Actor01500_D09FB8;
    arg0->recs                   = work->field_1FC;
    arg0->hp                     = Actor01500_D09FB8.hpMax;
    work->field_314.coord        = coord;
    work->field_314.spawnArgLo   = 0x300;
    work->field_314.spawnArgHi   = 1;
    place                        = arg0->place;
    switch (work->field_382 = place->variant) {
        case 0:
            work->field_36E = arg0->place->mode & 1;
            work->field_370 = (arg0->place->mode >> 1) & 1;
            switch (work->field_36E) {
                case 0:
                    work->field_352 = 1;
                    work->field_354 = 1;
                    work->field_34C = 0;
                    break;
                case 1:
                    work->field_352 = 2;
                    work->field_354 = 2;
                    work->field_34C = 0;
                    break;
            }
            work->field_362 = 0;
            draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_364        = ((draw >> 16) & 0x1F) + 1;
            work->field_372        = (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) + 0x800) & 0xFFF;
            break;
        case 1:
            work->field_370 = 1;
            work->field_352 = 5;
            work->field_354 = 5;
            work->field_35A = 9;
            work->field_35C = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_362 = ((gRandomLcgState >> 16) & 0x3F) + 0x3C;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_364 = (gRandomLcgState >> 16) & 0x1FF;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            break;
    }
    (Gp_IncStateF0Ref)(0);
    animationInitContext(&work->anim, Actor01500_D0A014, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_12C,
                         work->slots);
    for (i = 1; i < 7; i++) {
        animationResetSlot(&work->anim, i, (s16)work->field_352);
    }
    if (work->field_382 == 0) {
        draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        r                      = (draw >> 16) & 0x3F;
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend(&work->anim, i, (s16)(work->field_352), 0, r);
        }
    }
    work->field_1DC.coord            = &arg1->extra.tmd->coords[2];
    work->field_1DC.context.contacts = work->field_1FC;
    work->field_1DC.pos.vx           = 0;
    work->field_1DC.pos.vy           = 0;
    work->field_1DC.pos.vz           = 0;
    work->field_1DC.key              = 0x3000F;
    work->field_1DC.radius           = 0x12C;
    work->field_1DC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_1DC);
    Gp_InitRec18Table(work->field_1FC, 3, 0);
    work->field_244.context.contacts = work->field_264;
    work->field_244.coord            = coord;
    work->field_244.pos.vx           = 0;
    work->field_1DC.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    if (work->field_36E == 0) {
        work->field_244.pos.vy = 0;
        work->field_244.pos.vz = -0x12C;
    } else {
        work->field_244.pos.vy = 0x12C;
        work->field_244.pos.vz = 0;
    }
    work->field_244.key    = 0x3000F;
    work->field_244.radius = 0x12C;
    work->field_244.flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_244);
    Gp_InitRec18Table(work->field_264, 5, 0);
    records                          = work->field_2FC;
    work->field_2DC.coord            = coord;
    work->field_2DC.context.contacts = records;
    work->field_2DC.pos.vx           = 0;
    work->field_2DC.pos.vy           = 0;
    work->field_2DC.pos.vz           = 0x190;
    work->field_244.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->field_2DC.key              = Gp_PackPair(&Actor01500_D09FB4, 0);
    work->field_2DC.radius           = 0x12C;
    work->field_2DC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->field_2DC);
    Gp_InitRec18Table(records, 1, 0);
    work->field_2DC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->state            = 1;
}

/// Per-frame contact pass: applies the collision step, reacts to the three
/// contact records (damage from actors, push-out from walls) and clears them.
static void Actor01500_Fn004EC(Task* actor)
{
    Actor101500Work*       work;
    ActorPushFrame*        frame;
    s32                    push;
    VECTOR*                normal;
    GfxCoord*              coord;
    GfxCoord*              sourceCoord;
    WorldCollisionContact* effectRec;
    s16                    cooldown;
    s32                    result;
    s32                    i;
    s32                    depth;
    s32                    boundedDepth;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    wallDx;
    s32                    wallDy;
    s32                    wallDz;
    u32                    lastId;
    u32                    id;
    u32                    hitId;
    u32                    damage;

    push   = 0;
    lastId = 0;
    work   = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorPushFrame);
    frame  = SCRATCH_STACK_CURSOR(ActorPushFrame);
    coord  = actor->extra.tmd->coords;
    result = func_800E0C10(work->field_264, &frame->delta, 5, NULL);
    if (result != 0) {
        if (work->field_370 == 0 && work->field_35A == 3 && frame->delta.fixed.vy.word == 0) {
            work->field_35A        = 6;
            work->field_358        = 0;
            work->field_244.pos.vy = 0;
            work->field_244.pos.vz = -300;
            gRandomLcgState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_362        = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            for (i = 0; i < 5; i++) {
                if ((work->field_264[i].key.value & 0xFFFF0000) == 0x100000) {
                    frame->dx       = work->field_264[i].response.direction.vx;
                    frame->dz       = work->field_264[i].response.direction.vz;
                    work->field_372 = (ratan2(frame->dx, frame->dz) + 0x800) & 0xFFF;
                    break;
                }
            }
        }
        if (work->field_35A == 4 && frame->delta.fixed.vy.word < -0xDDA) {
            work->field_35A = 5;
            work->field_362 = 0;
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
                if (work->field_35A != 4) {
                    coord->coord.t[0] = work->field_31C.vx;
                    coord->coord.t[1] = work->field_31C.vy;
                    coord->coord.t[2] = work->field_31C.vz;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_264);
    if (work->field_350 != 0) {
        cooldown        = (u16)work->field_350 - 1;
        work->field_350 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_350 = 0;
        }
    }
    normal = &frame->normal;
    for (i = 0; i < 3; i++) {
        id = work->field_1FC[i].key.value;
        switch (id >> 0x10) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_350 == 0) {
                    sourceCoord            = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                    dx                     = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vx = dx;
                    dy                     = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vy = dy;
                    dz                     = sourceCoord->coord.t[2] - coord->coord.t[2];
                    frame->delta.vector.vz = dz;
                    damage                 = Gp_ComputeDamage(work->field_1FC[i].key.value, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2.pointer, work->field_1FC[i].key.value, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    func_800DA6E8(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    func_800E2C78(actor->spawnArg2.pointer, work->field_1FC[i].key.value, damage, 0);
                    Actor01500_Fn00AFC(actor, damage);
                    switch (Gp_GetIdParam0(work->field_1FC[i].key.value) & 0xFFFF) {
                        case 0:
                        case 5:
                        case 7:
                            break;
                        case 1:
                            Gp_SetObjFlag1(actor->spawnArg2.pointer);
                            break;
                        case 3:
                            Gp_SetObjFlag4(actor->spawnArg2.pointer, work->field_1FC[i].key.value, 0);
                            break;
                        case 4:
                        case 6:
                            if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                                work->field_37E = 1;
                            }
                            break;
                        case 2:
                        case 8:
                        case 9:
                            Gp_SetObjFlag2(actor->spawnArg2.pointer, work->field_1FC[i].key.value, 0);
                            break;
                    }
                    hitId = work->field_1FC[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(hitId) & 0xFFFF, coord, NULL, &work->field_314);
                    }
                    damage = Gp_GetIdParam2(work->field_1FC[i].key.value);
                    if ((s32)damage > 0) {
                        work->field_350 = damage;
                    }
                }
                break;
            case 3:
                wallDx                 = coord->workm.t[0] - work->field_1FC[i].point.vx;
                frame->delta.vector.vx = wallDx;
                wallDy                 = coord->workm.t[1] - work->field_1FC[i].point.vy;
                frame->delta.vector.vy = wallDy;
                wallDz                 = coord->workm.t[2] - work->field_1FC[i].point.vz;
                frame->delta.vector.vz = wallDz;
                depth                  = work->field_1FC[i].distance - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->dir);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (s32)(push * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1FC);
    effectRec = work->field_2FC;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_2DC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(effectRec);
        work->field_36A = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPushFrame);
}

static void Actor01500_Fn00AFC(Task* actor, s32 damage)
{
    Enemy*           enemy;
    Actor101500Work* work;
    GfxCoord*        coord;
    s32              id;

    enemy      = actor->spawnArg2.pointer;
    work       = actor->work;
    coord      = actor->extra.tmd->coords;
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        work->field_378 = 1;
    }
    id = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400F0004;
    SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    if (enemy->hp <= (Actor01500_D09FB8.hpMax * 60) / 100) {
        work->field_358 = 2;
        if (work->field_35A != 5) {
            work->field_35A        = 4;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
        }
    } else {
        work->field_35A = 7;
        switch (work->field_358) {
            case 0:
                if (work->field_36E == 0) {
                    work->field_352 = 11;
                    work->field_354 = 1;
                } else {
                    work->field_352 = 12;
                    work->field_354 = 2;
                }
                break;
            case 1:
                work->field_352 = 13;
                work->field_354 = 6;
                break;
            case 2:
                work->field_352 = 13;
                work->field_354 = 14;
                break;
        }
        work->field_34C = 0;
        work->field_356 = 0;
    }
    Gp_SetStateF0Byte3(2);
}

/// Idle hover: waits for the player to come within 2500 units (then switches
/// to pose 3, or 4 when `field_36E` is set) or for a disturbance - a random
/// timeout, a `gSceneCombatState.signals.bytes.actionFlags` trigger or lost hit points - that sends it into
/// pose 7/8 with a fresh `Actor01500_D09FC8` countdown.
static void Actor01500_Fn00CA4(Task* actor)
{
    Actor101500Work* work;
    GfxCoord*        coord;
    VECTOR*          frame;
    s32              flag;
    s16              pose;
    s16              pose2;
    s16              val;

    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    frame = SCRATCH_STACK_CURSOR(VECTOR);
    work  = actor->work;
    coord = actor->extra.tmd->coords;
    flag  = 0;
    if (work->field_37A != 0) {
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_356 = 0;
        pose2           = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        work->field_358 = 1;
        work->field_364 = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 0xF;
        work->field_362 = pose2;
    }
    work->field_376 = 0;
    frame->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    frame->vy       = 0;
    frame->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (SquareRoot0(frame->vx * frame->vx + frame->vz * frame->vz) < 2500) {
        work->field_35A = 1;
        pose            = 3;
        if (work->field_36E != 0) {
            pose = 4;
        }
        work->field_352 = pose;
        work->field_34C = 0x400F0001;
        work->field_362 = 0;
        work->field_364 = 0;
        Gp_ArmStateF0(1);
    } else {
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
            if (work->field_362 == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_362 = ((gRandomLcgState >> 16) & 0x1F) + 1;
            }
        }
        if (work->field_362 != 0) {
            work->field_362--;
            if (work->field_362 <= 0) {
                flag = 1;
            }
        }
        work->field_364--;
        if (work->field_364 == 0) {
            if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
                flag = 1;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_364 = ((gRandomLcgState >> 16) & 0x1F) + 1;
        }
        if (((Enemy*)actor->spawnArg2.pointer)->hp != Actor01500_D09FB8.hpMax) {
            flag = 1;
        }
        if (flag != 0) {
            work->field_35A = 2;
            pose2           = 7;
            if (work->field_36E != 0) {
                pose2 = 8;
            }
            work->field_352 = pose2;
            val             = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->field_364 = 0;
            work->field_358 = 1;
            work->field_37A = 1;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            work->field_362 = val;
            Gp_ArmStateF0(1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Once the pose has run 30 frames, re-aims `field_374` along the coordinate's
/// facing and walks the actor 40 units back along it (state 1 also rises, faster
/// early on); from frame 59 it queues pose 5 with two `gRandomLcgState` draws.
static void Actor01500_Fn00FC4(Task* actor)
{
    Actor101500Work* work;
    GfxCoord*        coord;
    s16              angle;
    u32              rnd;
    u32              rnd2;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if ((s16)work->field_356 >= 0x1E) {
        work->field_374 = angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        switch (work->field_36E) {
            case 0:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                break;
            case 1:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                if ((s16)work->field_356 < 0x24) {
                    coord->coord.t[1] += 0x6E;
                } else if ((s16)work->field_356 < 0x2E) {
                    coord->coord.t[1] += 0x23;
                } else {
                    coord->coord.t[1] += 0xF;
                }
                break;
        }
        if ((s16)work->field_356 >= 0x3B) {
            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_35A = 3;
            work->field_352 = 5;
            work->field_35C = 0;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            gRandomLcgState = rnd;
            work->field_362 = ((rnd >> 16) & 0x3F) + 0x3C;
            rnd2            = rnd * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rnd2;
            work->field_364 = (rnd2 >> 16) & 0x1FF;
        }
    }
}

/// Approach state, stepped by `field_35C`: settle vertically against the
/// height `gPlayerStatus.coordMtx` gives while turning to the player, then advance by a
/// random `Actor01500_D09FE8` distance; within 1000 units of the player it
/// switches to pose 10 and rises until its collision object reports contact
/// or it passes the height limit, then settles again.
static void Actor01500_Fn011B0(Task* actor)
{
    VECTOR3*         vec;
    Actor101500Work* work;
    GfxCoord*        coord;
    u32              seed;
    s32              off;
    u16              val;
    u16              val2;
    u16*             tbl;
    u8*              head;
    s32              diff;
    s32              dist;
    s32              y;
    s32              off2;
    s32              diff2;
    s32              dist2;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    work                     = actor->work;
    coord                    = actor->extra.tmd->coords;
    switch (work->field_35C) {
        case 0:
            off  = work->field_364 + 0x708;
            diff = gPlayerStatus.coordMtx->t[1] - off - coord->coord.t[1];
            dist = abs(diff);
            if (dist < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = diff > 0 ? 30 : -30;
            }
            vec->vx                = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val             = Actor01500_D09FE8[(gRandomLcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            vec->vx         = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy         = 0;
            vec->vz         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_372 = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376 = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = Actor01500_D09FC8;
                work->field_352 = 5;
                seed            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val2            = tbl[(seed >> 16) & 0xF];
                gRandomLcgState = seed;
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            if (work->field_36C == 0) {
                vec->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                if (SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 1000) {
                    work->field_352        = 10;
                    work->field_35C        = 3;
                    work->field_34C        = 0;
                    work->field_36C        = 1;
                    work->field_2DC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            break;
        case 3:
            diff2           = gPlayerStatus.coordMtx->t[1] - 0x640;
            work->field_360 = 100;
            work->field_366 = 180;
            if (diff2 < coord->coord.t[1] || work->field_36A != 0) {
                work->field_35C        = 4;
                work->field_36A        = 0;
                work->field_352        = 6;
                gRandomLcgState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_362        = ((gRandomLcgState >> 16) & 0xF) + 15;
                work->field_2DC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 4:
            off2  = coord->coord.t[1] + 0x708;
            diff2 = gPlayerStatus.coordMtx->t[1] - off2;
            dist2 = abs(diff2);
            if (dist2 < 0x60 || --work->field_362 <= 0) {
                work->field_35C = 2;
            } else {
                work->field_366 = diff2 > 0 ? 0x60 : -0x60;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Faces the actor toward the player on the XZ plane and raises `field_378`
/// once the player leaves the vertical band (500 above, 1800 below) or
/// `field_362` counts past 1800 frames.
static void Actor01500_Fn015DC(Task* actor)
{
    Actor101500Work* work;
    GfxCoord*        coord;
    VECTOR*          head;
    VECTOR*          blk;

    work                         = actor->work;
    coord                        = actor->extra.tmd->coords;
    work->field_352              = 0xE;
    work->field_360              = 5;
    work->field_376              = 5;
    work->field_34C              = 0;
    work->field_366              = 0x80;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    blk                          = head - 1;
    head[-1].vx                  = coord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
    blk->vy                      = 0;
    blk->vz                      = coord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = blk;
    work->field_372              = ratan2((s16)head[-1].vx, (s16)blk->vz) & 0xFFF;
    if (coord->coord.t[1] > gPlayerStatus.coordMtx->t[1] + 500 ||
        coord->coord.t[1] < gPlayerStatus.coordMtx->t[1] - 1800 ||
        ++work->field_362 > 1800) {
        work->field_378 = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Leaves the idle poses once `field_356` frames have run: state 0 switches to
/// pose 7 (8 when `field_36E` is set), state 1 to pose 5 with a random
/// `field_362` delay, state 2 to pose 14.
static void Actor01500_Fn01708(Task* actor)
{
    Actor101500Work* work = actor->work;
    s16              pose;
    u32              rnd;
    u16              val;
    u16*             tbl;

    switch (work->field_358) {
        case 0:
            pose = 7;
            if ((s16)work->field_356 >= 20) {
                work->field_35A = 0;
                if (work->field_36E != 0) {
                    pose = 8;
                }
                work->field_34C = 0x400F0002;
                work->field_352 = pose;
                work->field_35C = 0;
                work->field_380 = 15;
            }
            break;
        case 1:
            if ((s16)work->field_356 >= 10) {
                tbl             = Actor01500_D09FC8;
                work->field_35A = 3;
                work->field_352 = 5;
                work->field_35C = 0;
                rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                val             = tbl[(rnd >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_362 = val;
            }
            break;
        case 2:
            if ((s16)work->field_356 > 0) {
                work->field_35A = 5;
                work->field_362 = 0;
                work->field_352 = 14;
                work->field_34C = 0;
            }
            break;
    }
}

/// Turns the actor toward `field_372` by at most `field_376` per call, taking
/// the short way round the 0x1000 circle, then rebuilds its rotation matrix.
static void Actor01500_Fn01838(Task* arg0)
{
    Actor101500Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_372;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_374 = ang;
    if (adiff < 0x800) {
        step = work->field_376;
        if (step >= adiff) {
            work->field_374 = want;
        } else {
            next = work->field_374;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_374 = next;
        }
    } else {
        step = work->field_376;
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
        work->field_374 = work->field_372;
        goto done;
    turn:
        wrapStep = work->field_376;
        cur      = work->field_374;
        if (diff > 0) {
            work->field_374 = cur - wrapStep;
        } else {
            work->field_374 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_374;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void Actor01500_Fn01988(Task* arg0)
{
    Actor101500Work* work;
    GfxCoord*        coord;
    s16              bob;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    bob   = 0;
    if ((s16)work->field_352 == 5) {
        work->field_37C++;
        if (work->field_37C >= 15) {
            work->field_37C = 0;
        }
        bob = Actor01500_D0A070[work->field_37C];
    }
    work->field_31C.vx = coord->coord.t[0];
    work->field_31C.vy = coord->coord.t[1];
    work->field_31C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_360) >> 12;
    coord->coord.t[1] += work->field_366 + bob;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_360) >> 12;
    if (coord->coord.t[1] - gPlayerStatus.coordMtx->t[1] > 5000) {
        arg0->state     = 2;
        work->field_35A = 8;
        work->field_35C = 4;
        work->field_362 = 0;
    }
}

static void Actor01500_Fn01AB0(Task* arg0)
{
    GameLocationKey  key;
    u8               areaByte0;
    u32              raw1, index1;
    EffectWork*      effect1;
    TmdObject*       model1;
    AreaPlacement*   entry1;
    GameLocationKey* sessionKey1;
    u32              raw2, index2;
    EffectWork*      effect2;
    TmdObject*       model2;
    AreaPlacement*   entry2;
    GameLocationKey* sessionKey2;
    u32              raw3, index3;
    EffectWork*      effect3;
    TmdObject*       model3;
    AreaPlacement*   entry3;
    GameLocationKey* sessionKey3;
    u32              raw4, index4;
    EffectWork*      effect4;
    TmdObject*       model4;
    AreaPlacement*   entry4;
    GameLocationKey* sessionKey4;

    D_80067704[0] = &_gActor01500MindSucklerBurstHead;
    effect1       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->location.loc;
        raw1        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model1      = effect1->task->extra.tmd;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->location.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
        model1->texturePageOffset = entry1->texturePageOffset;
        model1->clutRowOffset     = entry1->clutRowOffset;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstWing;
    effect2       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->location.loc;
        raw2        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model2      = effect2->task->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->location.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstStinger;
    effect3       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->location.loc;
        raw3        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model3      = effect3->task->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->location.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstTail;
    effect4       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect4 != NULL) {
        sessionKey4 = &gGameSession->location.loc;
        raw4        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model4      = effect4->task->extra.tmd;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->location.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry4                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index4);
        model4->texturePageOffset = entry4->texturePageOffset;
        model4->clutRowOffset     = entry4->clutRowOffset;
        if (model4->buffer != NULL) {
            tmdProcessStream(model4);
            tmdProcessStream(model4);
        }
    }
}

/// Per-frame handler for the death sequence. Scene mode 1 only refreshes the
/// actor colour and mode 2 hides the model. Otherwise `field_35C` steps: state 0
/// saves the model matrix and unlinks the actor, 1 runs `Actor01500_Fn02C34`
/// and spawns an effect at frame 15, 3 frees the model's buffers once
/// `field_37E` passes 1 and 4 unlinks on its first frame; 1, 3 and 4 move to
/// 2 once `field_362` runs out, and 2 destroys the enemy.
static void Actor01500_Fn01DF0(Enemy* arg0, Task* arg1)
{
    VECTOR           pos;
    Actor101500Work* work;
    TmdObject*       model;
    GfxCoord*        coord;
    GfxCoord*        sub;

    model = arg1->extra.tmd;
    work  = arg1->work;
    coord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            sub = &coord[1];
            goto update;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    switch (work->field_35C) {
        case 0:
            work->field_368 = 0x1000;
            work->field_32C = coord->coord;
            arg0->recs      = 0;
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_1DC);
            Gp_UnlinkObj(&work->field_244);
            Gp_UnlinkObj(&work->field_2DC);
            Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            Gp_ReleaseStateF0Add(arg1, 0xF);
            work->field_362 = 0;
            work->field_35C = 1;
            if (work->field_37E != 0) {
                model->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->field_35C = 3;
            }
            break;
        case 1:
            Actor01500_Fn02C34(arg1);
            work->field_362++;
            if (work->field_362 == 10) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->field_362 == 15) {
                Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
            }
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 2:
            enemyDestroy(arg0, arg1);
            return;
        case 3:
            if (work->field_37E != 0) {
                if (work->field_37E >= 2) {
                    work->field_37E = 0;
                    Tmd_FreeBuffers(model);
                    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    Actor01500_Fn01AB0(arg1);
                } else {
                    work->field_37E++;
                }
            }
            work->field_362++;
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 4:
            if (work->field_362 == 0) {
                worldTargetUnlinkNode(&arg0->node);
                Gp_UnlinkObj(&work->field_1DC);
                Gp_UnlinkObj(&work->field_244);
                Gp_UnlinkObj(&work->field_2DC);
                Gp_ReleaseStateF0Add(arg1, 0xF);
            }
            work->field_362++;
            if (work->field_362 >= 61) {
                work->field_35C = 2;
            }
            break;
    }
    sub = arg1->extra.tmd->coords;
    sub = &sub[1];
update:
    pos.vx = sub->workm.t[0];
    pos.vy = sub->workm.t[1];
    pos.vz = sub->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
}

static void Actor01500_Fn020D8(Task* arg0)
{
    u8*              head;
    VECTOR3*         stk;
    VECTOR3*         vec;
    Actor101500Work* work;
    GfxCoord*        coord;
    s32              dy;
    s32              ady;
    u16              val;
    u16              val2;
    u16*             tbl;
    s32              off;
    s32              delay;

    head                     = SCRATCH_STACK_CURSOR(u8);
    stk                      = (VECTOR3*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)stk;
    vec                      = stk;
    work                     = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    switch (work->field_35C) {
        case 0:
            off = work->field_364 + 800;
            dy  = Actor01500_D0A090.vy - off - coord->coord.t[1];
            ady = abs(dy);
            if (ady < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = dy > 0 ? 30 : -30;
            }
            vec->vx                = Actor01500_D0A090.vx - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = Actor01500_D0A090.vz - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val             = Actor01500_D09FE8[(gRandomLcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            ((VECTOR3*)(head - 0x10))->vx = Actor01500_D0A090.vx - coord->coord.t[0];
            stk->vy                       = 0;
            stk->vz                       = Actor01500_D0A090.vz - coord->coord.t[2];
            work->field_372               = ratan2((s16)((VECTOR3*)(head - 0x10))->vx, (s16)stk->vz) & 0xFFF;
            work->field_376               = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = Actor01500_D09FC8;
                work->field_352 = 5;
                val2            = tbl[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            break;
    }
    if (gPlayerStatus.coordMtx->t[0] > Actor01500_D0A098.vx && gPlayerStatus.coordMtx->t[2] < Actor01500_D0A098.vz) {
        work->field_35A = 3;
        delay           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x3F) + 60;
        work->field_352 = 5;
        work->field_380 = 15;
        work->field_35C = 0;
        work->field_34C = 0x400F0002;
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_362 = delay;
        work->field_364 = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1FF;
        Gp_ArmStateF0(1);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Runs the task's current state handler from `Actor01500_D00004`, copying
/// the table onto the stack before the call.
static void Actor01500_Fn02428(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01500_D00004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame handler. Scene mode 1 only recolours and shadows the actor and
/// mode 2 hides it; otherwise it applies pending hit reactions and contacts,
/// hands off to the teardown state once `field_378` is raised in the
/// states that allow it, runs the behaviour state, turns, moves, animates
/// and voices the actor and rebuilds its root coordinate.
static void Actor01500_Fn02484(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor101500Work* work;
    s32              state;
    s32              one;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    one   = 1;
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
    obj->flags                   = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor01500_Fn025C8(arg1);
    }
    Actor01500_Fn004EC(arg1);
    if (work->field_378 != 0) {
        if ((work->field_35A == 5) || (work->field_37E != 0)) {
            work->field_35A = 8;
            work->field_35C = 0;
            arg1->state     = 2;
        }
    }
    Actor01500_Fn026D8(arg1);
    if (work->field_376 != 0) {
        Actor01500_Fn01838(arg1);
    }
    Actor01500_Fn01988(arg1);
    Actor01500_Fn02958(arg1);
    Actor01500_Fn02A1C(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    Actor01500_Fn02B14(arg1);
    Actor01500_Fn02B70(arg1);
}

/// Stagger and buildup both knock the actor into pose 13; damage over time
/// ticks the effect and applies each hit.
static void Actor01500_Fn025C8(Task* actor)
{
    Enemy*           enemy;
    Actor101500Work* work;
    s32              damage;
    u8               flags;

    enemy = actor->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = actor->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(enemy);
        if (damage != 0) {
            Actor01500_Fn00AFC(actor, damage);
            func_800DA6E8(&enemy->node, damage, 0);
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Runs the behaviour state `field_35A` selects. State 8 has no handler.
static void Actor01500_Fn026D8(Task* arg0)
{
    switch (((Actor101500Work*)arg0->work)->field_35A) {
        case 0:
            Actor01500_Fn00CA4(arg0);
            break;
        case 1:
            Actor01500_Fn027B0(arg0);
            break;
        case 2:
            Actor01500_Fn00FC4(arg0);
            break;
        case 3:
            Actor01500_Fn011B0(arg0);
            break;
        case 4:
            Actor01500_Fn0288C(arg0);
            break;
        case 5:
            Actor01500_Fn015DC(arg0);
            break;
        case 6:
            Actor01500_Fn028B0(arg0);
            break;
        case 7:
            Actor01500_Fn01708(arg0);
            break;
        case 9:
            Actor01500_Fn020D8(arg0);
            break;
    }
}

/// Counts `field_362` up, raising the state-F0 flags at frame 60; from frame 90
/// it switches to pose 7 (8 when `field_36E` is set) and reloads the counter
/// with a random delay.
static void Actor01500_Fn027B0(Task* actor)
{
    Actor101500Work* work = actor->work;
    s16              pose;
    u32              rnd;
    u16              val;

    if (++work->field_362 == 60) {
        Gp_SetStateF0Byte3(1);
        Gp_SetStateF0Bit(1);
    }
    pose = 7;
    if (work->field_362 >= 90) {
        work->field_35A = 2;
        if (work->field_36E != 0) {
            pose = 8;
        }
        work->field_352 = pose;
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        val             = Actor01500_D09FC8[(rnd >> 16) & 0xF];
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}

/// Stagger state, entered from a hit reaction or at low hit points: a slow
/// forward drift (`field_360`) with a climb of 0x80 a frame (`field_366`),
/// the second collision object's centre moved to (0, -300, 0).
static void Actor01500_Fn0288C(Task* arg0)
{
    Actor101500Work* work = arg0->work;

    work->field_360        = 0x14;
    work->field_366        = 0x80;
    work->field_244.pos.vy = -300;
    work->field_244.pos.vz = 0;
}

/// Countdown pose. Requests pose 9 and clears the move state each frame; when
/// `field_362` runs out it switches to pose 7 and reloads the countdown from a
/// `gRandomLcgState` draw into `Actor01500_D09FC8`.
static void Actor01500_Fn028B0(Task* actor)
{
    Actor101500Work* work = actor->work;
    u32              rnd;
    u16              val;
    u16*             tbl;

    work->field_352 = 9;
    work->field_34C = 0;
    work->field_360 = 0;
    work->field_366 = 0;
    if (--work->field_362 == 0) {
        tbl             = Actor01500_D09FC8;
        work->field_358 = 1;
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_35C = 0;
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        val             = tbl[(rnd >> 16) & 0xF];
        work->field_36E = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}

/// Animation tick. When the pose the actor asks for differs from the one its
/// slots were last queued for, every slot is re-seeded from the per-state
/// animation id table and the frame counter is cleared; while the two agree
/// each slot is ticked and the frame counter advances by one.
static void Actor01500_Fn02958(Task* arg0)
{
    Actor101500Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if ((s16)work->field_352 != work->field_354) {
        work->field_354 = work->field_352;
        work->field_356 = 0;
        value           = Actor01500_D0A050[(s16)work->field_352];
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend(&work->anim, i, (s16)work->field_352, 0, value);
        }
    } else {
        work->field_356++;
        for (i = 1; i < 7; i++) {
            animationTickSlot(&work->anim, i);
        }
    }
}

/// Voice tick: while `field_34C` holds a sound id, plays it every third
/// animation frame, tagged with the placement number, and once `field_380`
/// runs out switches to the 0x400F0003 cue.
static void Actor01500_Fn02A1C(Task* arg0)
{
    s16              timer;
    s32              soundId;
    s32              objectSoundId;
    s32              pan;
    GfxCoord*        object;
    Actor101500Work* work;

    work          = arg0->work;
    objectSoundId = work->field_34C;
    object        = arg0->extra.tmd->coords;
    if (objectSoundId != 0) {
        if ((s16)((s16)work->field_356 % 3) == 1) {
            soundId = objectSoundId | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan     = (s8)worldCoordGetOriginAudioPan(object);
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
        }
        if (work->field_380 > 0) {
            timer           = (u16)work->field_380 - 1;
            work->field_380 = timer;
            if ((timer << 0x10) <= 0) {
                work->field_34C = 0x400F0003;
                work->field_380 = 0;
            }
        }
    }
}

/// Hands `Gp_UpdateActorColor` the world position of the model's second
/// coordinate, with no blend parameters.
static void Actor01500_Fn02B14(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = &arg0->extra.tmd->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Ground shadow for the actor: carves a `VECTOR3` off the scratchpad and fills
/// it from the root coordinate's world translation - straight out of `workm.t`
/// in state 5, otherwise from the hit point `func_800EA1A8` finds casting a
/// ray down. The shade passed to `Gp_DrawEffGroundQuad` is `0x80` in state 5,
/// otherwise `func_800EA318`'s reading of the ray's drop.
static void Actor01500_Fn02B70(Task* arg0)
{
    Actor101500Work* work;
    GfxCoord*        coord;
    VECTOR3*         vec;
    VECTOR*          head;
    s16              hit;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(VECTOR) = head - 1;
    vec                          = (VECTOR3*)(head - 1);
    if (work->field_35A != 5) {
        hit = func_800EA1A8(MATRIX_TRANS(&coord->workm), vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0x80);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Death shrink: restores the root coordinate from the matrix `field_32C`
/// saved when the death sequence began and squashes it along Y by
/// `field_368`, which winds down by 0x50 a frame until it reaches 0x200. The
/// scale is applied through an identity rotation carved off the scratchpad,
/// `ScaleMatrix` and `MulMatrix`, and `composeStamp` is cleared so the coordinate's work
/// matrix is rebuilt.
static void Actor01500_Fn02C34(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor101500Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_368 >= 0x201) {
        work->field_368 = (u16)work->field_368 - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_368;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_32C;
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
