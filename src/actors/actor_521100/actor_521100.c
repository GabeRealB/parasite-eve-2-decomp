#include "actors/actor_521100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_521100_private.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
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
#include "../../shared/actor_messages.h"
#include "../../shared/no9_golem.h"

typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task*);
        s32 (*call1)(Task*);
        s32 (*call2)(Task*, s32, AnimationPlayRequest*);
        s32 (*call3)(Task*, s32, ActorCommand* request);
        s32 (*call4)(Task*, s32, ActorTransform*);
        s32 (*call5)(Task*, s32, s32);
    } handler;
} Actor521100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor521100MessageEntry, 8);

extern Actor521100MessageEntry D_actor_521100_8015F6FC[8];

s32 func_actor_521100_80135BEC(Task*);

s32 func_actor_521100_80135C14(Task*, s32, AnimationPlayRequest*);

typedef struct Actor521100FireScratch {
    /* 0x00 */ VECTOR               pos;
    /* 0x10 */ VECTOR               delta;
    /* 0x20 */ SVECTOR              vec;
    /* 0x28 */ AnimationPlayRequest msg;
    /* 0x3C */ ActorTransform       aim;
} Actor521100FireScratch;
STATIC_ASSERT_SIZEOF(Actor521100FireScratch, 0x54);

extern s16 D_actor_521100_8015F570[];

extern s16 D_actor_521100_8015F684[];

/// The three waypoints the state-6 body `func_actor_521100_80134774` walks the
/// actor to, one per phase `field_6A0` it switches on: `(-4000, 0, -2000)` for
/// phases 0 and 2, and `(-5250, 0, -1200)` for phase 1. Only `vx` and `vz` are
/// read, and only when the actor is too far from the player for that phase to
/// aim at it; the y of all three is zero, as the positions are on the floor.
extern VECTOR D_actor_521100_8015F654[];

/// Sixteen frames of the burn-out effect the state bodies at `field_6A0 == 1`
/// pick between on their last frame, indexed by the 4 bits under the top half
/// of an LCG draw (`(rng >> 16) & 0xF`). The sibling state body
/// `func_actor_521100_801357F0` reads the table one slot down at 0x8015F5F4.
extern u16 D_actor_521100_8015F634[];

/// The other sixteen-frame burn-out table, read by the state-5 body
/// `func_actor_521100_801357F0` off the same LCG draw bits the state-3 body
/// `func_actor_521100_8013570C` indexes `D_actor_521100_8015F634` with.
extern u16 D_actor_521100_8015F5F4[];

/// The burn-out effect table the sequence resets read, one 0x20-byte table
/// below `D_actor_521100_8015F5F4`: the state-1 body
/// `func_actor_521100_801335B4` draws from it both on the frame the actor
/// catches alight and on the frame the reset latch `field_6AA` has run out.
extern u16 D_actor_521100_8015F5D4[];

/// The 4-byte pair `func_actor_521100_801335B4` packs a type-2 record into and
/// copies onto both `obj57C` / `obj59C` at `field_18`, where the sibling
/// overlays' burn-out bodies put the same pair. Same shape as
/// `D_actor_510900_80167968` and `D_actor_400100_*`.
extern DamageAttack D_actor_521100_8015F550[4];

/// One signed halfword choice in a three-row, two-choice transition table.
/// The selector combines the row and random-column byte offsets before
/// accessing this member. The member access also keeps GCC's structure-memory
/// annotation, allowing the independent RNG write to retain its schedule.
typedef struct Actor521100StateChoice {
    s16 state;
} Actor521100StateChoice;
STATIC_ASSERT_SIZEOF(Actor521100StateChoice, 2);

extern s16                    D_actor_521100_8015F57C[16];
extern Actor521100StateChoice D_actor_521100_8015F59C[6];
extern s16                    D_actor_521100_8015F5A8[16];
extern Actor521100StateChoice D_actor_521100_8015F5C8[6];

extern u16 D_actor_521100_8015F614[];

/// Main-executable global with no module header yet: the remaining-enemy count.

extern EnemyParams D_actor_521100_8015F560;
extern TaskDesc    D_actor_521100_8015F6E4[];

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2);
static void func_actor_521100_80132958(Task* arg0);
static s32  func_actor_521100_80132C70(Task* arg0);
static void func_actor_521100_80132DE8(Task* arg0);
static void func_actor_521100_80133104(Task* arg0);
static void func_actor_521100_8013334C(Task* arg0);
static void func_actor_521100_801335B4(Task* arg0);
static void func_actor_521100_801339B0(Task* arg0);
static void func_actor_521100_80134658(Task* arg0);
static void func_actor_521100_80134774(Task* arg0);
static void func_actor_521100_80134C38(Task* arg0);
static void func_actor_521100_80134D88(Task* arg0);
static void func_actor_521100_80135024(Task* arg0);
static void func_actor_521100_80135230(Task* arg0);
static void func_actor_521100_801353CC(Enemy* arg0, Task* arg1);
static void func_actor_521100_80135414(Enemy* arg0, Task* arg1);
static void func_actor_521100_80135478(Enemy* arg0, Task* arg1);
static void func_actor_521100_801355C8(Task* arg0);
static void func_actor_521100_80135680(Task* arg0);
static void func_actor_521100_8013570C(Task* arg0);
static void func_actor_521100_801357F0(Task* arg0);
static void func_actor_521100_801358D4(Task* arg0);
static void func_actor_521100_80135964(Task* arg0);
static void func_actor_521100_80135A34(Task* arg0);
static void func_actor_521100_80135B40(Enemy* enemy, Task* task);
static void func_actor_521100_80135B80(Enemy* arg0, Task* task);

static TmdSource _gActor521100No9GolemDryfieldBody;
static TmdSource _gActor521100No9GolemDryfieldGunblade;
void             func_actor_521100_80135378(Task*);
void             func_actor_521100_80135AE4(Task*);

static AnimationPackedPose _gActor521100Animation05474Bank1[6] = {
#include "assets/actor_521100_animation_05474_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation05474Bank4[148] = {
#include "assets/actor_521100_animation_05474_bank4.inc"
};

static AnimationRecord _gActor521100Animation05474Records[201] = {
#include "assets/actor_521100_animation_05474_records.inc"
};

static u16 _gActor521100Animation05474Indices[20] = {
#include "assets/actor_521100_animation_05474_indices.inc"
};

AnimationSet gActor521100Animation05474 = {
    _gActor521100Animation05474Records,
    _gActor521100Animation05474Indices,
    { NULL, _gActor521100Animation05474Bank1, NULL, NULL, _gActor521100Animation05474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation059C4Bank1[9] = {
#include "assets/actor_521100_animation_059C4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation059C4Bank4[131] = {
#include "assets/actor_521100_animation_059C4_bank4.inc"
};

static AnimationRecord _gActor521100Animation059C4Records[162] = {
#include "assets/actor_521100_animation_059C4_records.inc"
};

static u16 _gActor521100Animation059C4Indices[20] = {
#include "assets/actor_521100_animation_059C4_indices.inc"
};

AnimationSet gActor521100Animation059C4 = {
    _gActor521100Animation059C4Records,
    _gActor521100Animation059C4Indices,
    { NULL, _gActor521100Animation059C4Bank1, NULL, NULL, _gActor521100Animation059C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation05F98Bank1[8] = {
#include "assets/actor_521100_animation_05F98_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation05F98Bank4[117] = {
#include "assets/actor_521100_animation_05F98_bank4.inc"
};

static AnimationRecord _gActor521100Animation05F98Records[212] = {
#include "assets/actor_521100_animation_05F98_records.inc"
};

static u16 _gActor521100Animation05F98Indices[20] = {
#include "assets/actor_521100_animation_05F98_indices.inc"
};

AnimationSet gActor521100Animation05F98 = {
    _gActor521100Animation05F98Records,
    _gActor521100Animation05F98Indices,
    { NULL, _gActor521100Animation05F98Bank1, NULL, NULL, _gActor521100Animation05F98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation0623CBank1[5] = {
#include "assets/actor_521100_animation_0623C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation0623CBank4[32] = {
#include "assets/actor_521100_animation_0623C_bank4.inc"
};

static AnimationRecord _gActor521100Animation0623CRecords[102] = {
#include "assets/actor_521100_animation_0623C_records.inc"
};

static u16 _gActor521100Animation0623CIndices[20] = {
#include "assets/actor_521100_animation_0623C_indices.inc"
};

AnimationSet gActor521100Animation0623C = {
    _gActor521100Animation0623CRecords,
    _gActor521100Animation0623CIndices,
    { NULL, _gActor521100Animation0623CBank1, NULL, NULL, _gActor521100Animation0623CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation07168Bank1[47] = {
#include "assets/actor_521100_animation_07168_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation07168Bank4[243] = {
#include "assets/actor_521100_animation_07168_bank4.inc"
};

static AnimationRecord _gActor521100Animation07168Records[567] = {
#include "assets/actor_521100_animation_07168_records.inc"
};

static u16 _gActor521100Animation07168Indices[20] = {
#include "assets/actor_521100_animation_07168_indices.inc"
};

AnimationSet gActor521100Animation07168 = {
    _gActor521100Animation07168Records,
    _gActor521100Animation07168Indices,
    { NULL, _gActor521100Animation07168Bank1, NULL, NULL, _gActor521100Animation07168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation07BA4Bank1[16] = {
#include "assets/actor_521100_animation_07BA4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation07BA4Bank4[246] = {
#include "assets/actor_521100_animation_07BA4_bank4.inc"
};

static AnimationRecord _gActor521100Animation07BA4Records[341] = {
#include "assets/actor_521100_animation_07BA4_records.inc"
};

static u16 _gActor521100Animation07BA4Indices[20] = {
#include "assets/actor_521100_animation_07BA4_indices.inc"
};

AnimationSet gActor521100Animation07BA4 = {
    _gActor521100Animation07BA4Records,
    _gActor521100Animation07BA4Indices,
    { NULL, _gActor521100Animation07BA4Bank1, NULL, NULL, _gActor521100Animation07BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation0820CBank1[14] = {
#include "assets/actor_521100_animation_0820C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation0820CBank4[148] = {
#include "assets/actor_521100_animation_0820C_bank4.inc"
};

static AnimationRecord _gActor521100Animation0820CRecords[200] = {
#include "assets/actor_521100_animation_0820C_records.inc"
};

static u16 _gActor521100Animation0820CIndices[20] = {
#include "assets/actor_521100_animation_0820C_indices.inc"
};

AnimationSet gActor521100Animation0820C = {
    _gActor521100Animation0820CRecords,
    _gActor521100Animation0820CIndices,
    { NULL, _gActor521100Animation0820CBank1, NULL, NULL, _gActor521100Animation0820CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation09030Bank1[25] = {
#include "assets/actor_521100_animation_09030_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation09030Bank4[369] = {
#include "assets/actor_521100_animation_09030_bank4.inc"
};

static AnimationRecord _gActor521100Animation09030Records[441] = {
#include "assets/actor_521100_animation_09030_records.inc"
};

static u16 _gActor521100Animation09030Indices[20] = {
#include "assets/actor_521100_animation_09030_indices.inc"
};

AnimationSet gActor521100Animation09030 = {
    _gActor521100Animation09030Records,
    _gActor521100Animation09030Indices,
    { NULL, _gActor521100Animation09030Bank1, NULL, NULL, _gActor521100Animation09030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation091C0Bank1[2] = {
#include "assets/actor_521100_animation_091C0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation091C0Bank4[17] = {
#include "assets/actor_521100_animation_091C0_bank4.inc"
};

static AnimationRecord _gActor521100Animation091C0Records[57] = {
#include "assets/actor_521100_animation_091C0_records.inc"
};

static u16 _gActor521100Animation091C0Indices[20] = {
#include "assets/actor_521100_animation_091C0_indices.inc"
};

AnimationSet gActor521100Animation091C0 = {
    _gActor521100Animation091C0Records,
    _gActor521100Animation091C0Indices,
    { NULL, _gActor521100Animation091C0Bank1, NULL, NULL, _gActor521100Animation091C0Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor521100No9GolemDryfieldBodySkeleton[19] = {
#include "assets/no9_golem_dryfield_body_skeleton.inc"
};

static u32 _gActor521100No9GolemDryfieldBodyPartVerts[19] = {
#include "assets/no9_golem_dryfield_body_partVerts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldBodyVerts[432] = {
#include "assets/no9_golem_dryfield_body_verts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldBodyNormals[444] = {
#include "assets/no9_golem_dryfield_body_normals.inc"
};

static u32 _gActor521100No9GolemDryfieldBodyStream[4749] = {
#include "assets/no9_golem_dryfield_body_stream.inc"
};

static TmdSource _gActor521100No9GolemDryfieldBody = {
    0,
    26564,
    6624,
    19,
    _gActor521100No9GolemDryfieldBodyPartVerts,
    _gActor521100No9GolemDryfieldBodyVerts,
    _gActor521100No9GolemDryfieldBodyNormals,
    _gActor521100No9GolemDryfieldBodySkeleton,
    _gActor521100No9GolemDryfieldBodyStream,
};

static TmdBone _gActor521100No9GolemDryfieldGunbladeSkeleton[1] = {
#include "assets/no9_golem_dryfield_gunblade_skeleton.inc"
};

static u32 _gActor521100No9GolemDryfieldGunbladePartVerts[1] = {
#include "assets/no9_golem_dryfield_gunblade_partVerts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldGunbladeVerts[43] = {
#include "assets/no9_golem_dryfield_gunblade_verts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldGunbladeNormals[41] = {
#include "assets/no9_golem_dryfield_gunblade_normals.inc"
};

static u32 _gActor521100No9GolemDryfieldGunbladeStream[326] = {
#include "assets/no9_golem_dryfield_gunblade_stream.inc"
};

static TmdSource _gActor521100No9GolemDryfieldGunblade = {
    0,
    2300,
    0,
    1,
    _gActor521100No9GolemDryfieldGunbladePartVerts,
    _gActor521100No9GolemDryfieldGunbladeVerts,
    _gActor521100No9GolemDryfieldGunbladeNormals,
    _gActor521100No9GolemDryfieldGunbladeSkeleton,
    _gActor521100No9GolemDryfieldGunbladeStream,
};

static AnimationPackedPose _gActor521100Animation10EACBank1[21] = {
#include "assets/actor_521100_animation_10EAC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation10EACBank4[317] = {
#include "assets/actor_521100_animation_10EAC_bank4.inc"
};

static AnimationRecord _gActor521100Animation10EACRecords[382] = {
#include "assets/actor_521100_animation_10EAC_records.inc"
};

static u16 _gActor521100Animation10EACIndices[20] = {
#include "assets/actor_521100_animation_10EAC_indices.inc"
};

AnimationSet gActor521100Animation10EAC = {
    _gActor521100Animation10EACRecords,
    _gActor521100Animation10EACIndices,
    { NULL, _gActor521100Animation10EACBank1, NULL, NULL, _gActor521100Animation10EACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation11614Bank1[12] = {
#include "assets/actor_521100_animation_11614_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation11614Bank4[187] = {
#include "assets/actor_521100_animation_11614_bank4.inc"
};

static AnimationRecord _gActor521100Animation11614Records[231] = {
#include "assets/actor_521100_animation_11614_records.inc"
};

static u16 _gActor521100Animation11614Indices[20] = {
#include "assets/actor_521100_animation_11614_indices.inc"
};

AnimationSet gActor521100Animation11614 = {
    _gActor521100Animation11614Records,
    _gActor521100Animation11614Indices,
    { NULL, _gActor521100Animation11614Bank1, NULL, NULL, _gActor521100Animation11614Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation12204Bank1[20] = {
#include "assets/actor_521100_animation_12204_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation12204Bank4[321] = {
#include "assets/actor_521100_animation_12204_bank4.inc"
};

static AnimationRecord _gActor521100Animation12204Records[363] = {
#include "assets/actor_521100_animation_12204_records.inc"
};

static u16 _gActor521100Animation12204Indices[20] = {
#include "assets/actor_521100_animation_12204_indices.inc"
};

AnimationSet gActor521100Animation12204 = {
    _gActor521100Animation12204Records,
    _gActor521100Animation12204Indices,
    { NULL, _gActor521100Animation12204Bank1, NULL, NULL, _gActor521100Animation12204Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1271CBank1[5] = {
#include "assets/actor_521100_animation_1271C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1271CBank4[99] = {
#include "assets/actor_521100_animation_1271C_bank4.inc"
};

static AnimationRecord _gActor521100Animation1271CRecords[192] = {
#include "assets/actor_521100_animation_1271C_records.inc"
};

static u16 _gActor521100Animation1271CIndices[20] = {
#include "assets/actor_521100_animation_1271C_indices.inc"
};

AnimationSet gActor521100Animation1271C = {
    _gActor521100Animation1271CRecords,
    _gActor521100Animation1271CIndices,
    { NULL, _gActor521100Animation1271CBank1, NULL, NULL, _gActor521100Animation1271CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation13454Bank1[21] = {
#include "assets/actor_521100_animation_13454_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation13454Bank4[354] = {
#include "assets/actor_521100_animation_13454_bank4.inc"
};

static AnimationRecord _gActor521100Animation13454Records[409] = {
#include "assets/actor_521100_animation_13454_records.inc"
};

static u16 _gActor521100Animation13454Indices[20] = {
#include "assets/actor_521100_animation_13454_indices.inc"
};

AnimationSet gActor521100Animation13454 = {
    _gActor521100Animation13454Records,
    _gActor521100Animation13454Indices,
    { NULL, _gActor521100Animation13454Bank1, NULL, NULL, _gActor521100Animation13454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation13B8CBank1[16] = {
#include "assets/actor_521100_animation_13B8C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation13B8CBank4[177] = {
#include "assets/actor_521100_animation_13B8C_bank4.inc"
};

static AnimationRecord _gActor521100Animation13B8CRecords[217] = {
#include "assets/actor_521100_animation_13B8C_records.inc"
};

static u16 _gActor521100Animation13B8CIndices[20] = {
#include "assets/actor_521100_animation_13B8C_indices.inc"
};

AnimationSet gActor521100Animation13B8C = {
    _gActor521100Animation13B8CRecords,
    _gActor521100Animation13B8CIndices,
    { NULL, _gActor521100Animation13B8CBank1, NULL, NULL, _gActor521100Animation13B8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation143F0Bank1[14] = {
#include "assets/actor_521100_animation_143F0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation143F0Bank4[220] = {
#include "assets/actor_521100_animation_143F0_bank4.inc"
};

static AnimationRecord _gActor521100Animation143F0Records[255] = {
#include "assets/actor_521100_animation_143F0_records.inc"
};

static u16 _gActor521100Animation143F0Indices[20] = {
#include "assets/actor_521100_animation_143F0_indices.inc"
};

AnimationSet gActor521100Animation143F0 = {
    _gActor521100Animation143F0Records,
    _gActor521100Animation143F0Indices,
    { NULL, _gActor521100Animation143F0Bank1, NULL, NULL, _gActor521100Animation143F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation14918Bank1[9] = {
#include "assets/actor_521100_animation_14918_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation14918Bank4[124] = {
#include "assets/actor_521100_animation_14918_bank4.inc"
};

static AnimationRecord _gActor521100Animation14918Records[159] = {
#include "assets/actor_521100_animation_14918_records.inc"
};

static u16 _gActor521100Animation14918Indices[20] = {
#include "assets/actor_521100_animation_14918_indices.inc"
};

AnimationSet gActor521100Animation14918 = {
    _gActor521100Animation14918Records,
    _gActor521100Animation14918Indices,
    { NULL, _gActor521100Animation14918Bank1, NULL, NULL, _gActor521100Animation14918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation14C38Bank1[5] = {
#include "assets/actor_521100_animation_14C38_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation14C38Bank4[55] = {
#include "assets/actor_521100_animation_14C38_bank4.inc"
};

static AnimationRecord _gActor521100Animation14C38Records[110] = {
#include "assets/actor_521100_animation_14C38_records.inc"
};

static u16 _gActor521100Animation14C38Indices[20] = {
#include "assets/actor_521100_animation_14C38_indices.inc"
};

AnimationSet gActor521100Animation14C38 = {
    _gActor521100Animation14C38Records,
    _gActor521100Animation14C38Indices,
    { NULL, _gActor521100Animation14C38Bank1, NULL, NULL, _gActor521100Animation14C38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15168Bank1[10] = {
#include "assets/actor_521100_animation_15168_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15168Bank4[121] = {
#include "assets/actor_521100_animation_15168_bank4.inc"
};

static AnimationRecord _gActor521100Animation15168Records[161] = {
#include "assets/actor_521100_animation_15168_records.inc"
};

static u16 _gActor521100Animation15168Indices[20] = {
#include "assets/actor_521100_animation_15168_indices.inc"
};

AnimationSet gActor521100Animation15168 = {
    _gActor521100Animation15168Records,
    _gActor521100Animation15168Indices,
    { NULL, _gActor521100Animation15168Bank1, NULL, NULL, _gActor521100Animation15168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation152F8Bank1[2] = {
#include "assets/actor_521100_animation_152F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation152F8Bank4[17] = {
#include "assets/actor_521100_animation_152F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation152F8Records[57] = {
#include "assets/actor_521100_animation_152F8_records.inc"
};

static u16 _gActor521100Animation152F8Indices[20] = {
#include "assets/actor_521100_animation_152F8_indices.inc"
};

AnimationSet gActor521100Animation152F8 = {
    _gActor521100Animation152F8Records,
    _gActor521100Animation152F8Indices,
    { NULL, _gActor521100Animation152F8Bank1, NULL, NULL, _gActor521100Animation152F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15904Bank1[9] = {
#include "assets/actor_521100_animation_15904_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15904Bank4[151] = {
#include "assets/actor_521100_animation_15904_bank4.inc"
};

static AnimationRecord _gActor521100Animation15904Records[189] = {
#include "assets/actor_521100_animation_15904_records.inc"
};

static u16 _gActor521100Animation15904Indices[20] = {
#include "assets/actor_521100_animation_15904_indices.inc"
};

AnimationSet gActor521100Animation15904 = {
    _gActor521100Animation15904Records,
    _gActor521100Animation15904Indices,
    { NULL, _gActor521100Animation15904Bank1, NULL, NULL, _gActor521100Animation15904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15E28Bank1[8] = {
#include "assets/actor_521100_animation_15E28_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15E28Bank4[116] = {
#include "assets/actor_521100_animation_15E28_bank4.inc"
};

static AnimationRecord _gActor521100Animation15E28Records[169] = {
#include "assets/actor_521100_animation_15E28_records.inc"
};

static u16 _gActor521100Animation15E28Indices[20] = {
#include "assets/actor_521100_animation_15E28_indices.inc"
};

AnimationSet gActor521100Animation15E28 = {
    _gActor521100Animation15E28Records,
    _gActor521100Animation15E28Indices,
    { NULL, _gActor521100Animation15E28Bank1, NULL, NULL, _gActor521100Animation15E28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation16628Bank1[10] = {
#include "assets/actor_521100_animation_16628_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation16628Bank4[195] = {
#include "assets/actor_521100_animation_16628_bank4.inc"
};

static AnimationRecord _gActor521100Animation16628Records[267] = {
#include "assets/actor_521100_animation_16628_records.inc"
};

static u16 _gActor521100Animation16628Indices[20] = {
#include "assets/actor_521100_animation_16628_indices.inc"
};

AnimationSet gActor521100Animation16628 = {
    _gActor521100Animation16628Records,
    _gActor521100Animation16628Indices,
    { NULL, _gActor521100Animation16628Bank1, NULL, NULL, _gActor521100Animation16628Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation16F90Bank1[16] = {
#include "assets/actor_521100_animation_16F90_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation16F90Bank4[237] = {
#include "assets/actor_521100_animation_16F90_bank4.inc"
};

static AnimationRecord _gActor521100Animation16F90Records[297] = {
#include "assets/actor_521100_animation_16F90_records.inc"
};

static u16 _gActor521100Animation16F90Indices[20] = {
#include "assets/actor_521100_animation_16F90_indices.inc"
};

AnimationSet gActor521100Animation16F90 = {
    _gActor521100Animation16F90Records,
    _gActor521100Animation16F90Indices,
    { NULL, _gActor521100Animation16F90Bank1, NULL, NULL, _gActor521100Animation16F90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation18B20Bank1[53] = {
#include "assets/actor_521100_animation_18B20_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation18B20Bank4[738] = {
#include "assets/actor_521100_animation_18B20_bank4.inc"
};

static AnimationRecord _gActor521100Animation18B20Records[847] = {
#include "assets/actor_521100_animation_18B20_records.inc"
};

static u16 _gActor521100Animation18B20Indices[20] = {
#include "assets/actor_521100_animation_18B20_indices.inc"
};

AnimationSet gActor521100Animation18B20 = {
    _gActor521100Animation18B20Records,
    _gActor521100Animation18B20Indices,
    { NULL, _gActor521100Animation18B20Bank1, NULL, NULL, _gActor521100Animation18B20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1A8E4Bank1[50] = {
#include "assets/actor_521100_animation_1A8E4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1A8E4Bank4[812] = {
#include "assets/actor_521100_animation_1A8E4_bank4.inc"
};

static AnimationRecord _gActor521100Animation1A8E4Records[923] = {
#include "assets/actor_521100_animation_1A8E4_records.inc"
};

static u16 _gActor521100Animation1A8E4Indices[20] = {
#include "assets/actor_521100_animation_1A8E4_indices.inc"
};

AnimationSet gActor521100Animation1A8E4 = {
    _gActor521100Animation1A8E4Records,
    _gActor521100Animation1A8E4Indices,
    { NULL, _gActor521100Animation1A8E4Bank1, NULL, NULL, _gActor521100Animation1A8E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1B8F8Bank1[26] = {
#include "assets/actor_521100_animation_1B8F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1B8F8Bank4[434] = {
#include "assets/actor_521100_animation_1B8F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation1B8F8Records[497] = {
#include "assets/actor_521100_animation_1B8F8_records.inc"
};

static u16 _gActor521100Animation1B8F8Indices[20] = {
#include "assets/actor_521100_animation_1B8F8_indices.inc"
};

AnimationSet gActor521100Animation1B8F8 = {
    _gActor521100Animation1B8F8Records,
    _gActor521100Animation1B8F8Indices,
    { NULL, _gActor521100Animation1B8F8Bank1, NULL, NULL, _gActor521100Animation1B8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1C104Bank1[15] = {
#include "assets/actor_521100_animation_1C104_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1C104Bank4[197] = {
#include "assets/actor_521100_animation_1C104_bank4.inc"
};

static AnimationRecord _gActor521100Animation1C104Records[253] = {
#include "assets/actor_521100_animation_1C104_records.inc"
};

static u16 _gActor521100Animation1C104Indices[20] = {
#include "assets/actor_521100_animation_1C104_indices.inc"
};

AnimationSet gActor521100Animation1C104 = {
    _gActor521100Animation1C104Records,
    _gActor521100Animation1C104Indices,
    { NULL, _gActor521100Animation1C104Bank1, NULL, NULL, _gActor521100Animation1C104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1C58CBank1[5] = {
#include "assets/actor_521100_animation_1C58C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1C58CBank4[87] = {
#include "assets/actor_521100_animation_1C58C_bank4.inc"
};

static AnimationRecord _gActor521100Animation1C58CRecords[168] = {
#include "assets/actor_521100_animation_1C58C_records.inc"
};

static u16 _gActor521100Animation1C58CIndices[20] = {
#include "assets/actor_521100_animation_1C58C_indices.inc"
};

AnimationSet gActor521100Animation1C58C = {
    _gActor521100Animation1C58CRecords,
    _gActor521100Animation1C58CIndices,
    { NULL, _gActor521100Animation1C58CBank1, NULL, NULL, _gActor521100Animation1C58CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation20880Bank1[147] = {
#include "assets/actor_521100_animation_20880_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation20880Bank4[1395] = {
#include "assets/actor_521100_animation_20880_bank4.inc"
};

static AnimationRecord _gActor521100Animation20880Records[2429] = {
#include "assets/actor_521100_animation_20880_records.inc"
};

static u16 _gActor521100Animation20880Indices[20] = {
#include "assets/actor_521100_animation_20880_indices.inc"
};

AnimationSet gActor521100Animation20880 = {
    _gActor521100Animation20880Records,
    _gActor521100Animation20880Indices,
    { NULL, _gActor521100Animation20880Bank1, NULL, NULL, _gActor521100Animation20880Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation20F94Bank1[11] = {
#include "assets/actor_521100_animation_20F94_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation20F94Bank4[162] = {
#include "assets/actor_521100_animation_20F94_bank4.inc"
};

static AnimationRecord _gActor521100Animation20F94Records[238] = {
#include "assets/actor_521100_animation_20F94_records.inc"
};

static u16 _gActor521100Animation20F94Indices[20] = {
#include "assets/actor_521100_animation_20F94_indices.inc"
};

AnimationSet gActor521100Animation20F94 = {
    _gActor521100Animation20F94Records,
    _gActor521100Animation20F94Indices,
    { NULL, _gActor521100Animation20F94Bank1, NULL, NULL, _gActor521100Animation20F94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2177CBank1[14] = {
#include "assets/actor_521100_animation_2177C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2177CBank4[170] = {
#include "assets/actor_521100_animation_2177C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2177CRecords[274] = {
#include "assets/actor_521100_animation_2177C_records.inc"
};

static u16 _gActor521100Animation2177CIndices[20] = {
#include "assets/actor_521100_animation_2177C_indices.inc"
};

AnimationSet gActor521100Animation2177C = {
    _gActor521100Animation2177CRecords,
    _gActor521100Animation2177CIndices,
    { NULL, _gActor521100Animation2177CBank1, NULL, NULL, _gActor521100Animation2177CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation220B8Bank1[13] = {
#include "assets/actor_521100_animation_220B8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation220B8Bank4[206] = {
#include "assets/actor_521100_animation_220B8_bank4.inc"
};

static AnimationRecord _gActor521100Animation220B8Records[326] = {
#include "assets/actor_521100_animation_220B8_records.inc"
};

static u16 _gActor521100Animation220B8Indices[20] = {
#include "assets/actor_521100_animation_220B8_indices.inc"
};

AnimationSet gActor521100Animation220B8 = {
    _gActor521100Animation220B8Records,
    _gActor521100Animation220B8Indices,
    { NULL, _gActor521100Animation220B8Bank1, NULL, NULL, _gActor521100Animation220B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation229A0Bank1[12] = {
#include "assets/actor_521100_animation_229A0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation229A0Bank4[194] = {
#include "assets/actor_521100_animation_229A0_bank4.inc"
};

static AnimationRecord _gActor521100Animation229A0Records[320] = {
#include "assets/actor_521100_animation_229A0_records.inc"
};

static u16 _gActor521100Animation229A0Indices[20] = {
#include "assets/actor_521100_animation_229A0_indices.inc"
};

AnimationSet gActor521100Animation229A0 = {
    _gActor521100Animation229A0Records,
    _gActor521100Animation229A0Indices,
    { NULL, _gActor521100Animation229A0Bank1, NULL, NULL, _gActor521100Animation229A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2318CBank1[10] = {
#include "assets/actor_521100_animation_2318C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2318CBank4[175] = {
#include "assets/actor_521100_animation_2318C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2318CRecords[282] = {
#include "assets/actor_521100_animation_2318C_records.inc"
};

static u16 _gActor521100Animation2318CIndices[20] = {
#include "assets/actor_521100_animation_2318C_indices.inc"
};

AnimationSet gActor521100Animation2318C = {
    _gActor521100Animation2318CRecords,
    _gActor521100Animation2318CIndices,
    { NULL, _gActor521100Animation2318CBank1, NULL, NULL, _gActor521100Animation2318CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation239C0Bank1[12] = {
#include "assets/actor_521100_animation_239C0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation239C0Bank4[179] = {
#include "assets/actor_521100_animation_239C0_bank4.inc"
};

static AnimationRecord _gActor521100Animation239C0Records[290] = {
#include "assets/actor_521100_animation_239C0_records.inc"
};

static u16 _gActor521100Animation239C0Indices[20] = {
#include "assets/actor_521100_animation_239C0_indices.inc"
};

AnimationSet gActor521100Animation239C0 = {
    _gActor521100Animation239C0Records,
    _gActor521100Animation239C0Indices,
    { NULL, _gActor521100Animation239C0Bank1, NULL, NULL, _gActor521100Animation239C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation24F5CBank1[48] = {
#include "assets/actor_521100_animation_24F5C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation24F5CBank4[554] = {
#include "assets/actor_521100_animation_24F5C_bank4.inc"
};

static AnimationRecord _gActor521100Animation24F5CRecords[665] = {
#include "assets/actor_521100_animation_24F5C_records.inc"
};

static u16 _gActor521100Animation24F5CIndices[20] = {
#include "assets/actor_521100_animation_24F5C_indices.inc"
};

AnimationSet gActor521100Animation24F5C = {
    _gActor521100Animation24F5CRecords,
    _gActor521100Animation24F5CIndices,
    { NULL, _gActor521100Animation24F5CBank1, NULL, NULL, _gActor521100Animation24F5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation25ACCBank1[13] = {
#include "assets/actor_521100_animation_25ACC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation25ACCBank4[259] = {
#include "assets/actor_521100_animation_25ACC_bank4.inc"
};

static AnimationRecord _gActor521100Animation25ACCRecords[414] = {
#include "assets/actor_521100_animation_25ACC_records.inc"
};

static u16 _gActor521100Animation25ACCIndices[20] = {
#include "assets/actor_521100_animation_25ACC_indices.inc"
};

AnimationSet gActor521100Animation25ACC = {
    _gActor521100Animation25ACCRecords,
    _gActor521100Animation25ACCIndices,
    { NULL, _gActor521100Animation25ACCBank1, NULL, NULL, _gActor521100Animation25ACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26024Bank1[8] = {
#include "assets/actor_521100_animation_26024_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26024Bank4[117] = {
#include "assets/actor_521100_animation_26024_bank4.inc"
};

static AnimationRecord _gActor521100Animation26024Records[181] = {
#include "assets/actor_521100_animation_26024_records.inc"
};

static u16 _gActor521100Animation26024Indices[20] = {
#include "assets/actor_521100_animation_26024_indices.inc"
};

AnimationSet gActor521100Animation26024 = {
    _gActor521100Animation26024Records,
    _gActor521100Animation26024Indices,
    { NULL, _gActor521100Animation26024Bank1, NULL, NULL, _gActor521100Animation26024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26E3CBank1[25] = {
#include "assets/actor_521100_animation_26E3C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26E3CBank4[372] = {
#include "assets/actor_521100_animation_26E3C_bank4.inc"
};

static AnimationRecord _gActor521100Animation26E3CRecords[435] = {
#include "assets/actor_521100_animation_26E3C_records.inc"
};

static u16 _gActor521100Animation26E3CIndices[20] = {
#include "assets/actor_521100_animation_26E3C_indices.inc"
};

AnimationSet gActor521100Animation26E3C = {
    _gActor521100Animation26E3CRecords,
    _gActor521100Animation26E3CIndices,
    { NULL, _gActor521100Animation26E3CBank1, NULL, NULL, _gActor521100Animation26E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26FCCBank1[2] = {
#include "assets/actor_521100_animation_26FCC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26FCCBank4[17] = {
#include "assets/actor_521100_animation_26FCC_bank4.inc"
};

static AnimationRecord _gActor521100Animation26FCCRecords[57] = {
#include "assets/actor_521100_animation_26FCC_records.inc"
};

static u16 _gActor521100Animation26FCCIndices[20] = {
#include "assets/actor_521100_animation_26FCC_indices.inc"
};

AnimationSet gActor521100Animation26FCC = {
    _gActor521100Animation26FCCRecords,
    _gActor521100Animation26FCCIndices,
    { NULL, _gActor521100Animation26FCCBank1, NULL, NULL, _gActor521100Animation26FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation271A8Bank1[2] = {
#include "assets/actor_521100_animation_271A8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation271A8Bank4[17] = {
#include "assets/actor_521100_animation_271A8_bank4.inc"
};

static AnimationRecord _gActor521100Animation271A8Records[76] = {
#include "assets/actor_521100_animation_271A8_records.inc"
};

static u16 _gActor521100Animation271A8Indices[20] = {
#include "assets/actor_521100_animation_271A8_indices.inc"
};

AnimationSet gActor521100Animation271A8 = {
    _gActor521100Animation271A8Records,
    _gActor521100Animation271A8Indices,
    { NULL, _gActor521100Animation271A8Bank1, NULL, NULL, _gActor521100Animation271A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation27A78Bank1[19] = {
#include "assets/actor_521100_animation_27A78_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation27A78Bank4[211] = {
#include "assets/actor_521100_animation_27A78_bank4.inc"
};

static AnimationRecord _gActor521100Animation27A78Records[276] = {
#include "assets/actor_521100_animation_27A78_records.inc"
};

static u16 _gActor521100Animation27A78Indices[20] = {
#include "assets/actor_521100_animation_27A78_indices.inc"
};

AnimationSet gActor521100Animation27A78 = {
    _gActor521100Animation27A78Records,
    _gActor521100Animation27A78Indices,
    { NULL, _gActor521100Animation27A78Bank1, NULL, NULL, _gActor521100Animation27A78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation27DB0Bank1[6] = {
#include "assets/actor_521100_animation_27DB0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation27DB0Bank4[63] = {
#include "assets/actor_521100_animation_27DB0_bank4.inc"
};

static AnimationRecord _gActor521100Animation27DB0Records[105] = {
#include "assets/actor_521100_animation_27DB0_records.inc"
};

static u16 _gActor521100Animation27DB0Indices[20] = {
#include "assets/actor_521100_animation_27DB0_indices.inc"
};

AnimationSet gActor521100Animation27DB0 = {
    _gActor521100Animation27DB0Records,
    _gActor521100Animation27DB0Indices,
    { NULL, _gActor521100Animation27DB0Bank1, NULL, NULL, _gActor521100Animation27DB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation28380Bank1[11] = {
#include "assets/actor_521100_animation_28380_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation28380Bank4[131] = {
#include "assets/actor_521100_animation_28380_bank4.inc"
};

static AnimationRecord _gActor521100Animation28380Records[188] = {
#include "assets/actor_521100_animation_28380_records.inc"
};

static u16 _gActor521100Animation28380Indices[20] = {
#include "assets/actor_521100_animation_28380_indices.inc"
};

AnimationSet gActor521100Animation28380 = {
    _gActor521100Animation28380Records,
    _gActor521100Animation28380Indices,
    { NULL, _gActor521100Animation28380Bank1, NULL, NULL, _gActor521100Animation28380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation28DD8Bank1[17] = {
#include "assets/actor_521100_animation_28DD8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation28DD8Bank4[212] = {
#include "assets/actor_521100_animation_28DD8_bank4.inc"
};

static AnimationRecord _gActor521100Animation28DD8Records[379] = {
#include "assets/actor_521100_animation_28DD8_records.inc"
};

static u16 _gActor521100Animation28DD8Indices[20] = {
#include "assets/actor_521100_animation_28DD8_indices.inc"
};

AnimationSet gActor521100Animation28DD8 = {
    _gActor521100Animation28DD8Records,
    _gActor521100Animation28DD8Indices,
    { NULL, _gActor521100Animation28DD8Bank1, NULL, NULL, _gActor521100Animation28DD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation291F8Bank1[8] = {
#include "assets/actor_521100_animation_291F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation291F8Bank4[93] = {
#include "assets/actor_521100_animation_291F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation291F8Records[127] = {
#include "assets/actor_521100_animation_291F8_records.inc"
};

static u16 _gActor521100Animation291F8Indices[20] = {
#include "assets/actor_521100_animation_291F8_indices.inc"
};

AnimationSet gActor521100Animation291F8 = {
    _gActor521100Animation291F8Records,
    _gActor521100Animation291F8Indices,
    { NULL, _gActor521100Animation291F8Bank1, NULL, NULL, _gActor521100Animation291F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation29534Bank1[6] = {
#include "assets/actor_521100_animation_29534_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation29534Bank4[64] = {
#include "assets/actor_521100_animation_29534_bank4.inc"
};

static AnimationRecord _gActor521100Animation29534Records[105] = {
#include "assets/actor_521100_animation_29534_records.inc"
};

static u16 _gActor521100Animation29534Indices[20] = {
#include "assets/actor_521100_animation_29534_indices.inc"
};

AnimationSet gActor521100Animation29534 = {
    _gActor521100Animation29534Records,
    _gActor521100Animation29534Indices,
    { NULL, _gActor521100Animation29534Bank1, NULL, NULL, _gActor521100Animation29534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation29A88Bank1[10] = {
#include "assets/actor_521100_animation_29A88_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation29A88Bank4[117] = {
#include "assets/actor_521100_animation_29A88_bank4.inc"
};

static AnimationRecord _gActor521100Animation29A88Records[174] = {
#include "assets/actor_521100_animation_29A88_records.inc"
};

static u16 _gActor521100Animation29A88Indices[20] = {
#include "assets/actor_521100_animation_29A88_indices.inc"
};

AnimationSet gActor521100Animation29A88 = {
    _gActor521100Animation29A88Records,
    _gActor521100Animation29A88Indices,
    { NULL, _gActor521100Animation29A88Bank1, NULL, NULL, _gActor521100Animation29A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2A998Bank1[28] = {
#include "assets/actor_521100_animation_2A998_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2A998Bank4[400] = {
#include "assets/actor_521100_animation_2A998_bank4.inc"
};

static AnimationRecord _gActor521100Animation2A998Records[460] = {
#include "assets/actor_521100_animation_2A998_records.inc"
};

static u16 _gActor521100Animation2A998Indices[20] = {
#include "assets/actor_521100_animation_2A998_indices.inc"
};

AnimationSet gActor521100Animation2A998 = {
    _gActor521100Animation2A998Records,
    _gActor521100Animation2A998Indices,
    { NULL, _gActor521100Animation2A998Bank1, NULL, NULL, _gActor521100Animation2A998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2B89CBank1[33] = {
#include "assets/actor_521100_animation_2B89C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2B89CBank4[385] = {
#include "assets/actor_521100_animation_2B89C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2B89CRecords[457] = {
#include "assets/actor_521100_animation_2B89C_records.inc"
};

static u16 _gActor521100Animation2B89CIndices[20] = {
#include "assets/actor_521100_animation_2B89C_indices.inc"
};

AnimationSet gActor521100Animation2B89C = {
    _gActor521100Animation2B89CRecords,
    _gActor521100Animation2B89CIndices,
    { NULL, _gActor521100Animation2B89CBank1, NULL, NULL, _gActor521100Animation2B89CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2C378Bank1[19] = {
#include "assets/actor_521100_animation_2C378_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2C378Bank4[289] = {
#include "assets/actor_521100_animation_2C378_bank4.inc"
};

static AnimationRecord _gActor521100Animation2C378Records[329] = {
#include "assets/actor_521100_animation_2C378_records.inc"
};

static u16 _gActor521100Animation2C378Indices[20] = {
#include "assets/actor_521100_animation_2C378_indices.inc"
};

AnimationSet gActor521100Animation2C378 = {
    _gActor521100Animation2C378Records,
    _gActor521100Animation2C378Indices,
    { NULL, _gActor521100Animation2C378Bank1, NULL, NULL, _gActor521100Animation2C378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2CCA8Bank1[17] = {
#include "assets/actor_521100_animation_2CCA8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2CCA8Bank4[234] = {
#include "assets/actor_521100_animation_2CCA8_bank4.inc"
};

static AnimationRecord _gActor521100Animation2CCA8Records[283] = {
#include "assets/actor_521100_animation_2CCA8_records.inc"
};

static u16 _gActor521100Animation2CCA8Indices[20] = {
#include "assets/actor_521100_animation_2CCA8_indices.inc"
};

AnimationSet gActor521100Animation2CCA8 = {
    _gActor521100Animation2CCA8Records,
    _gActor521100Animation2CCA8Indices,
    { NULL, _gActor521100Animation2CCA8Bank1, NULL, NULL, _gActor521100Animation2CCA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2D708Bank1[21] = {
#include "assets/actor_521100_animation_2D708_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2D708Bank4[261] = {
#include "assets/actor_521100_animation_2D708_bank4.inc"
};

static AnimationRecord _gActor521100Animation2D708Records[320] = {
#include "assets/actor_521100_animation_2D708_records.inc"
};

static u16 _gActor521100Animation2D708Indices[20] = {
#include "assets/actor_521100_animation_2D708_indices.inc"
};

AnimationSet gActor521100Animation2D708 = {
    _gActor521100Animation2D708Records,
    _gActor521100Animation2D708Indices,
    { NULL, _gActor521100Animation2D708Bank1, NULL, NULL, _gActor521100Animation2D708Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_521100_8015F550[4] = {
    { 15, 7 },
    { 25, 7 },
    { 40, 7 },
    { 10, 0 },
};

EnemyParams D_actor_521100_8015F560 = { D_actor_521100_8015F550, 1100, 800, 300, 50, 0, 0, 0, 0 };

s16 D_actor_521100_8015F570[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

s16 D_actor_521100_8015F57C[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
    2,
};

Actor521100StateChoice D_actor_521100_8015F59C[6] = {
    { 2 },
    { 2 },
    { 0 },
    { 2 },
    { 0 },
    { 0 },
};

s16 D_actor_521100_8015F5A8[16] = {
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
};

Actor521100StateChoice D_actor_521100_8015F5C8[6] = {
    { 1 },
    { 2 },
    { 0 },
    { 2 },
    { 0 },
    { 1 },
};

u16 D_actor_521100_8015F5D4[16] = { 0 };

u16 D_actor_521100_8015F5F4[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    15,
    15,
    15,
    15,
    30,
    30,
    30,
    30,
};

u16 D_actor_521100_8015F614[16] = {
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
};

u16 D_actor_521100_8015F634[16] = { 0 };

VECTOR D_actor_521100_8015F654[3] = {
    { -4000, 0, -2000, 0 },
    { -5250, 0, -1200, 0 },
    { -4000, 0, -2000, 0 },
};

s16 D_actor_521100_8015F684[48] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_521100_8015F6E4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_521100_80135378, { .model = &_gActor521100No9GolemDryfieldBody } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_521100_80135AE4, { .model = &_gActor521100No9GolemDryfieldGunblade } },
};

Actor521100MessageEntry D_actor_521100_8015F6FC[8] = {
    { 2014, { .call1 = func_actor_521100_80135BEC } },
    { ACTOR_MESSAGE_PLAY_ANIMATION, { .call2 = func_actor_521100_80135C14 } },
    { ACTOR_MESSAGE_PLACE, { .call4 = actorMsgPlaceRotMatrix } },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .call5 = func_actor_521100_80135D10 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call3 = func_actor_521100_80135D58 } },
    { 2007, { .call1 = func_actor_521100_80135D9C } },
    { ACTOR_MESSAGE_IS_PRESENT, { .call0 = func_actor_521100_80135DC8 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

static void           func_actor_521100_80131E8C(Enemy* enemy, Task* task);
static __inline__ s32 Actor521100_GetHitType(s32 key);

/// Spawn state of the actor: allocates its 0x6C0 work block, registers the
/// enemy on the lock-on list with its parameter record, contact table and body
/// coordinate (the model's fourth part), and starts the animation on clip 0x15.
/// It then links the actor's collision bodies, spawns a second enemy from
/// `D_actor_521100_8015F6E4` with this one as its parent, dresses that enemy's
/// model with the texture page and CLUT of this enemy's placement, and links
/// two more pairs of bodies, one of them placed on the second enemy's model.
///
/// Every body's coordinate is assigned first in its block: the model pointer is
/// reloaded from the task each time, and that load has to precede the stores
/// into the work block, which it cannot be scheduled across.
static void func_actor_521100_80131E8C(Enemy* enemy, Task* task)
{
    GameLocationKey  key;
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor521100Work* work;
    Enemy*           spawned;
    TmdObject*       model;
    GameLocationKey* sessionKey;
    AreaPlacement*   place;
    s32              idx;
    u32              raw;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6C0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord         = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx    = 0;
    enemy->bodyPos.vy    = 0;
    enemy->bodyPos.vz    = 0;
    enemy->param         = &D_actor_521100_8015F560;
    enemy->recs          = work->rec534;
    enemy->hp            = D_actor_521100_8015F560.hpMax;
    work->eff.coord      = &task->extra.tmd->coords[3];
    work->eff.spawnArgLo = 0x400;
    work->eff.spawnArgHi = 3;
    animationInitContext(&work->rig.anim, D_actor_521100_8015F73C, obj, work->rig.poses, work->rig.slots);
    work->field_686 = 0x15;
    work->field_688 = 0x15;
    i               = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->field_686);
        i++;
    } while (i < 0x13);
    work->field_6B2 = 1;
    work->field_6B6 = -1;
    work->field_6B8 = -1;

    work->obj47C.coord            = task->extra.tmd->coords;
    work->obj47C.context.contacts = work->rec49C;
    work->obj47C.pos.vx           = 0;
    work->obj47C.pos.vy           = -0x190;
    work->obj47C.pos.vz           = 0;
    work->obj47C.key              = 0x30022;
    work->obj47C.radius           = 0x190;
    work->obj47C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(work->rec49C, 5, 0);
    work->obj47C.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->obj514.coord            = &task->extra.tmd->coords[3];
    work->obj514.context.contacts = work->rec534;
    work->obj514.pos.vx           = 0;
    work->obj514.pos.vy           = 0;
    work->obj514.pos.vz           = 0;
    work->obj514.key              = 0x30022;
    work->obj514.radius           = 0x190;
    work->obj514.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj514);
    Gp_InitRec18Table(work->rec534, 3, 0);
    work->obj514.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    spawned    = Gp_SpawnEnemyFromTable(D_actor_521100_8015F6E4, 1, 0, enemy);
    model      = spawned->task->extra.tmd;
    raw        = enemy->placeKey;
    sessionKey = &gGameSession->location.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    idx        = raw >> 12;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    /* offset + base, as in the sibling spawn bodies: the ROM adds the scaled
       index onto the table. */
    place                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    work->field_654 = spawned->task;

    work->obj57C.coord            = spawned->task->extra.tmd->coords;
    work->obj57C.pos.vx           = -0x226;
    work->obj57C.context.contacts = work->rec5BC;
    work->obj57C.pos.vy           = 0x64;
    work->obj57C.pos.vz           = 0;
    work->obj57C.key              = 0;
    work->obj57C.radius           = 0x1C2;
    work->obj57C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj57C);
    Gp_InitRec18Table(work->rec5BC, 1, 0);
    work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj59C.coord            = &task->extra.tmd->coords[7];
    work->obj59C.context.contacts = work->rec5BC;
    work->obj59C.pos.vx           = 0;
    work->obj59C.pos.vy           = 0;
    work->obj59C.pos.vz           = 0;
    work->obj59C.key              = 0;
    work->obj59C.radius           = 0x1C2;
    work->obj59C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj59C);

    work->shape.ends[0].vz = 0x5DC;
    work->shape.ends[0].vx = 0;
    work->shape.ends[0].vy = 0;
    work->shape.ends[1].vx = 0;
    work->shape.ends[1].vy = 0;
    work->shape.ends[1].vz = 0;
    work->shape.end0Radius = 1;
    work->shape.end1Radius = 1;
    work->shape.contacts   = work->rec62C;
    work->obj59C.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj5D4.coord           = task->extra.tmd->coords;
    work->obj5D4.context.capsule = &work->shape;
    work->obj5D4.pos.vy          = -0x1F4;
    work->obj5D4.pos.vx          = 0;
    work->obj5D4.pos.vz          = 0;
    work->obj5D4.key             = 0;
    work->obj5D4.radius          = 0;
    work->obj5D4.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->obj5D4);
    Gp_InitRec18Table(work->rec62C, 1, 0);
    work->obj5D4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    work->obj5F4.coord            = task->extra.tmd->coords;
    work->obj5F4.pos.vy           = -0x320;
    work->obj5F4.context.contacts = work->rec62C;
    work->obj5F4.pos.vx           = 0;
    work->obj5F4.pos.vz           = 0x4E2;
    work->obj5F4.key              = 0;
    work->obj5F4.radius           = 0x1C2;
    work->obj5F4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj5F4);
    work->obj5F4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    task->msgTable = D_actor_521100_8015F6FC;
    task->state    = 1;
}

static __inline__ s32 Actor521100_GetHitType(s32 key)
{
    if (key & 0x8000) {
        return 1;
    }
    return D_actor_521100_8015F684[key & 0x3F];
}

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2)
{
    ActorDeltaFrame48* scratch;
    Actor521100Work*   work;
    Enemy*             enemy;
    GfxCoord*          coord;
    u32                lastId;
    u32                sound;
    u32                kind;
    u32                damage;
    u32                rng;
    u32                rng2;
    u32                r;
    s32                result;
    s32                dx;
    s32                coordX;
    s32                dz;
    s32                absDiff;
    s32                r2;
    s32                angle;
    s32                angle2;
    s32                hitType;
    s32                i;
    s16                diff;
    s16                wrap;
    s16                cooldown;
    s32                pan;
    s32                pan1;
    s32                pan2;
    s32                depth;
    s16                wait;

    lastId  = 0;
    work    = arg0->work;
    scratch = (ActorDeltaFrame48*)SCRATCH_STACK_RESERVE_BYTES(0x48);
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    result  = func_800E0C10(work->rec49C, &scratch->delta, 5, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_64C;
            coord->coord.t[1] = work->field_64E;
            coord->coord.t[2] = work->field_650;
            break;
    }
    Gp_ClearRec18Occupied(work->rec49C);
    if (work->field_684 != 0) {
        cooldown        = (u16)work->field_684 - 1;
        work->field_684 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_684 = 0;
        }
    }
    if (work->field_6AC != 0) {
        work->field_6AC = (u16)work->field_6AC - 1;
    }
    for (i = 0; i < 3; i++) {
        kind = (u16)(work->rec534[i].key.value >> 0x10);
        if (kind < 2) {
            continue;
        }
        if (kind != 2) {
            continue;
        }
        if (work->field_684 != 0) {
            continue;
        }
        coordX                   = coord->coord.t[0];
        dx                       = gPlayerStatus.coordMtx->t[0] - coordX;
        scratch->delta.vector.vx = dx;
        scratch->delta.vector.vy = 0;
        dz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->delta.vector.vz = dz;
        damage                   = Gp_ComputeDamage(work->rec534[i].key.value, SquareRoot0(dx * dx + dz * dz), 0, 0);
        hitType                  = Actor521100_GetHitType(work->rec534[i].key.value);
        if (hitType == 1) {
            if (work->field_69E == 2) {
                hitType = 0;
            } else {
                diff    = work->field_696 - (ratan2((s16)scratch->delta.vector.vx, (s16)scratch->delta.vector.vz) & 0xFFF);
                absDiff = abs(diff);
                if (absDiff < 0x800) {
                    wrap = absDiff;
                } else if (diff > 0) {
                    wrap = 0x1000 - diff;
                } else {
                    wrap = diff + 0x1000;
                }
                if ((wrap >= 0x301) || (work->field_6AE == 1)) {
                    hitType = 2;
                }
            }
        }
        switch (hitType) {
            case 0:
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = rng >> 0x10;
                angle           = (r & 0x7F) + 0x40;
                gRandomLcgState = rng;
                if (!(r & 1)) {
                    angle = -angle;
                }
                work->field_678.vx = angle;
                r2                 = (s16)r >> 8;
                angle2             = (r2 & 0x7F) + 0x40;
                if (!(r2 & 1)) {
                    angle2 = -angle2;
                }
                work->field_678.vy = angle2;
                work->field_680    = 1;
                damage           >>= 1;
                if ((Gp_GetIdParam0(work->rec534[i].key.value) & 0xFFFF) == 5) {
                    damage *= 2;
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if ((work->field_69E == 0) && (work->field_6AC <= 0)) {
                    work->field_69E = 5;
                    work->field_6A0 = 0;
                    rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_6AC = ((rng2 >> 0x10) & 0xFF) + 0x96;
                    gRandomLcgState = rng2;
                    sound           = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0006;
                    pan             = (s8)worldCoordGetOriginAudioPan(coord);
                    depth           = (s8)worldCoordGetOriginAudioDepth(coord);
                    SndEvt_EnqueueType6((s32)sound, pan, depth);
                    goto damage_done;
                }
                goto damage_done;
            case 1:
                work->field_69E = 4;
                work->field_6A0 = 0;
                if (work->rec534[i].key.value & 0x8000) {
                    damage >>= 2;
                } else {
                    damage >>= 3;
                }
                sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0003;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan1, depth);
                goto damage_done;
            case 2:
                work->field_69E = 3;
                work->field_6A0 = 0;
                work->field_6AE = 0;
                if (work->rec534[i].key.value & 0x8000) {
                    damage *= 2;
                } else {
                    damage >>= 1;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((gRandomLcgState >> 16) & 1) {
                    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0004;
                } else {
                    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0005;
                }
                pan2  = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan2, depth);
        }
    damage_done:
        func_800E2C78(enemy, (s32)work->rec534[i].key.value, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        enemy->hp = (u16)enemy->hp - damage;
        if (lastId != work->rec534[i].key.value) {
            lastId = work->rec534[i].key.value;
            func_800FDB18(Gp_GetIdParam1(work->rec534[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL, &work->eff);
        }
        wait = Gp_GetIdParam2(work->rec534[i].key.value);
        if (wait > 0) {
            work->field_684 = wait;
        }
    }
    Gp_ClearRec18Occupied(work->rec534);
    if (work->rec5BC[0].flags & 1) {
        work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->rec5BC);
        work->field_6A6 = 1;
    }
    work->field_6BE = 0;
    if (work->rec62C[0].flags & 1) {
        work->field_6BE = 1;
        Gp_ClearRec18Occupied(work->rec62C);
    }
    if (enemy->hp <= 0) {
        if (gPlayerStatus.hp > 0) {
            work->field_6B2 = 0;
        } else {
            enemy->hp = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x48);
}

static void func_actor_521100_80132958(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    u16*             tbl;
    u16*             tbl1;
    u16*             tbl2;
    u32              rng;
    u32              rng1;
    u32              rng2;
    s16              state;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;

    coord                        = arg0->extra.tmd->coords;
    work                         = arg0->work;
    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    scratchEnd[-1].vx            = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    work->field_6AA              = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
    angle                        = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
    work->field_698              = angle;
    if (gGameSession->location.loc.view == 2) {
        work->field_69E = 6;
        work->field_6A0 = 0;
    } else {
        state = work->field_6A0;
        switch (state) {
            case 0:
                work->field_69A = 0;
                work->field_69C = 0;
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer < 0) {
                    work->field_6A0 = 1;
                    work->field_686 = 0x12;
                    tbl             = D_actor_521100_8015F614;
                    rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rng;
                    work->field_68E = tbl[(rng >> 16) & 0xF];
                } else if (func_actor_521100_80132C70(arg0) == 0) {
                    func_actor_521100_80135680(arg0);
                }
                break;
            case 1:
                if ((work->field_6BE == state) && (work->field_6AA < 0x7D0)) {
                    delta     = (u16)angle - (u16)work->field_696;
                    magnitude = abs(delta);
                    if (magnitude < 0x800) {
                        wrapped = magnitude;
                    } else {
                        if (delta > 0) {
                            wrapped = 0x1000 - delta;
                        } else {
                            wrapped = delta + 0x1000;
                        }
                    }
                    if (wrapped < 0x100) {
                        work->field_68E = 0;
                    }
                }
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer <= 0) {
                    work->field_69C = 0x78;
                    work->field_69A = 0;
                    tbl1            = D_actor_521100_8015F5F4;
                    rng1            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rng1;
                    timer           = tbl1[(rng1 >> 16) & 0xF];
                    work->field_68E = timer;
                    if (timer == 0) {
                        func_actor_521100_80135680(arg0);
                        if (work->field_69E == 0) {
                            tbl2            = D_actor_521100_8015F614;
                            rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rng2;
                            work->field_68E = tbl2[(rng2 >> 16) & 0xF];
                        }
                    } else {
                        work->field_6A0 = 0;
                        work->field_686 = 1;
                    }
                } else {
                    work->field_69A = 0x14;
                    work->field_69C = 0x78;
                    func_actor_521100_80132C70(arg0);
                }
                break;
        }
    }
    work->field_6AE = 0;
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Asks the player for the hold (message 0x3F8, range 0x19) once the actor has
/// swung its heading to within 0x20 of the slot-3 task's own and is lined up
/// to latch on. The heading error is the 12-bit difference between the work
/// block's `field_698` and `field_696`, wrapped into [-0x800, 0x800] and then
/// narrowed by `field_69C` being armed with 0x50; the request goes out only
/// while fewer than 0x4E2 units of the actor's health are left, the latch
/// `field_6BE` is clear, `gPlayerStatus.hp` (the player's current HP) is positive
/// and the player's own `GameActor::mode` is not its mode 2. On acceptance
/// the body rearms the motion state (2 into `field_69E`, 0xA frames of blend
/// into `field_686`, the 0xA/0xFF/0x80 pad lerp) and returns 1; the 0x3F8
/// query buffer is the 0x18 bytes pushed on the scratch-pad stack.
static s32 func_actor_521100_80132C70(Task* arg0)
{
    Actor521100Work* work;
    Task*            player;
    GpDelayArg*      msg;
    s16              diff;
    s32              adiff;
    s16              wrap;
    s32              ret;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    msg    = (GpDelayArg*)SCRATCH_STACK_RESERVE_BYTES(0x18);

    diff  = work->field_698 - work->field_696;
    adiff = diff >= 0 ? diff : -diff;
    ret   = 0;
    if (adiff < 0x800) {
        wrap = adiff;
    } else if (diff > 0) {
        wrap = 0x1000 - diff;
    } else {
        wrap = diff + 0x1000;
    }
    if ((wrap < 0x400) && (work->field_6AA < 0x4E2) && (work->field_6BE == 0) && (gPlayerStatus.hp > 0) && (work->field_69C = 0x50, (wrap < 0x20)) && (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        msg->field_14 = 0x19;
        if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3F8, msg, 0) == 0) {
            ret             = 1;
            work->field_6A8 = 0;
            work->field_69E = 2;
            work->field_6A0 = 0;
            work->field_6A2 = 0;
            work->field_686 = 0xA;
            work->field_69A = 0;
            work->field_69C = 0;
            Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
    return ret;
}
static void func_actor_521100_80132DE8(Task* arg0)
{
    Actor521100Work*        work;
    GfxCoord*               coord;
    VECTOR*                 head;
    VECTOR*                 vec;
    Actor521100StateChoice* pairNear;
    Actor521100StateChoice* pairFar;
    s16*                    flatNear;
    s16*                    flatFar;
    u16                     prev;
    u32                     rngPN;
    u32                     rngPF;
    u32                     rngFN;
    u32                     rngFF;
    s32                     packed;
    s32                     next;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    head  = SCRATCH_STACK_CURSOR(VECTOR);
    vec   = head - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    head[-1].vx                  = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];

    work->field_6AA = SquareRoot0(head[-1].vx * head[-1].vx + vec->vz * vec->vz);
    work->field_698 = ratan2((s16)head[-1].vx, (s16)vec->vz) & 0xFFF;

    switch (work->field_6A0) {
        case 0:
            if (((u32)((u8)Gp_StateC08.field_A - 2) >= 2U) && (gGameSession->location.loc.view != 2)) {
                if (work->field_6AA < 0x8FC) {
                    if (work->field_6B8 == work->field_6B6) {
                        pairNear        = D_actor_521100_8015F59C;
                        rngPN           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rngPN;
                        next            = ((Actor521100StateChoice*)((u8*)pairNear + (work->field_6B6 * 4 + ((rngPN >> 16) & 1) * 2)))->state;
                    } else {
                        flatNear        = D_actor_521100_8015F57C;
                        rngFN           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        next            = flatNear[(rngFN >> 16) & 0xF];
                        gRandomLcgState = rngFN;
                    }
                } else {
                    if (work->field_6B8 == work->field_6B6) {
                        pairFar         = D_actor_521100_8015F5C8;
                        rngPF           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rngPF;
                        next            = ((Actor521100StateChoice*)((u8*)pairFar + (work->field_6B6 * 4 + ((rngPF >> 16) & 1) * 2)))->state;
                    } else {
                        flatFar         = D_actor_521100_8015F5A8;
                        rngFF           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        next            = flatFar[(rngFF >> 16) & 0xF];
                        gRandomLcgState = rngFF;
                    }
                }
            } else {
                next = 2;
            }

            prev            = (u16)work->field_6B6;
            work->field_6B6 = next;
            work->field_6B8 = prev;

            switch (next) {
                case 0:
                    work->field_6A0  = 1;
                    work->field_686  = 5;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 0);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 1:
                    work->field_6A0  = 2;
                    work->field_686  = 6;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 1);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 2:
                    work->field_6A0  = 3;
                    work->field_6A2  = 0;
                    work->field_686  = 3;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 2);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
            }
            break;
        case 1:
            func_actor_521100_80133104(arg0);
            break;
        case 2:
            func_actor_521100_8013334C(arg0);
            break;
        case 3:
            func_actor_521100_801335B4(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// The scratch head is taken through `ActorScratchStack` rather than as
/// `SCRATCH_STACK_CURSOR`, which does not compile the same.
static void func_actor_521100_80133104(Task* arg0)
{
    GfxCoord*        coord;
    SVECTOR*         vec;
    SVECTOR*         head;
    s16*             clipPtr;
    s16              frame2;
    s16              frame3;
    s16              frame;
    s16              clip;
    s16              speed;
    s32              snd;
    s32              pan;
    GfxCoord*        effectCoord;
    s32              effect;
    s32              kind;
    SVECTOR*         offset;
    u16*             tbl;
    u16              clipId;
    u16              part;
    u32              rng;
    Actor521100Work* work;

    head                                                  = ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head;
    vec                                                   = head - 1;
    ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head = vec;
    work                                                  = arg0->work;
    frame                                                 = (s16)work->field_68A;
    clipPtr                                               = &D_actor_521100_8015F894[work->field_686];
    clip                                                  = *clipPtr;
    clipId                                                = (u16)*clipPtr;
    coord                                                 = arg0->extra.tmd->coords;
    if (frame == (clip + 0x1A)) {
        effect      = 0x60188;
        kind        = 0xC;
        effectCoord = coord;
        SOFT_TOUCH_REG(effectCoord);
        offset = NULL;
        SOFT_TOUCH_REG4(effect, kind, effectCoord, offset);
        Gp_SpawnEff(effect, &effectCoord[8], kind, offset);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == (clip + 0x1E)) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, work->field_654->extra.tmd->coords, 0, vec);
    }
    frame2 = (s16)work->field_68A;
    if (frame2 == ((s16)clipId + 0x1C)) {
        work->field_6AE    = 1;
        work->obj57C.flags = (u16)(work->obj57C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj59C.flags = (u16)(work->obj59C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
        snd                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0008;
        pan                = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == ((s16)clipId + 0x28)) {
            work->field_6A6    = 0;
            work->obj57C.flags = (u16)(work->obj57C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            work->obj59C.flags = (u16)(work->obj59C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
    }
    frame3 = (s16)work->field_68A;
    if (frame3 >= ((s16)clipId + 0x1C)) {
        if (((s16)clipId + 0x1E) >= frame3) {
            speed = 0x64;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= ((s16)clipId + 0x7A)) {
        work->field_686 = 1;
        tbl             = D_actor_521100_8015F5F4;
        rng             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->field_69E = 0;
        work->field_6A0 = 0;
        part            = tbl[(rng >> 16) & 0xF];
        gRandomLcgState = rng;
        work->field_6AE = 0;
        work->field_68E = part;
    }
    ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head = (SVECTOR*)((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head + 1;
}

/// Runs one frame of the burn-out sequence timed off the clip the slots are
/// playing: `D_actor_521100_8015F894[field_686]` is the clip's own length, read
/// signed and again unsigned because the cue frames below need it both ways,
/// and `field_68A` is the frame counter the blend in
/// `func_actor_521100_80135964` ticks. The counter is re-read at each cue
/// rather than carried, so the effects spawned in between cannot leave a stale
/// copy behind.
///
/// The cues, all offsets from that length: under +0x28 the turn limit
/// `field_69C` is held at 0x50; at +0x23 effect 0x60188 drops onto the attach
/// coordinate eight slots along and the 0xA/0x40/0xFF pad lerp starts; at
/// +0x27 the 8-byte scratch `SVECTOR` is thrown to (-0x320, 0x64, 0) and handed
/// to effect 0x6009C on the coordinate `field_654`'s own display object
/// carries; and +0x23 again, this time against the unsigned length, arms
/// `field_6AE` and raises the two record flags at 0x59A / 0x5BA together, then
/// cues `SndEvt_EnqueueType6` with the actor's pan and depth narrowed to bytes.
/// +0x2D hands the flags back down and clears the parked animation `field_6A6`.
///
/// The two ends are the motion: `field_69A` is held at 0x88 of forward speed
/// while the counter is between +0x20 and +0x2A of the length, and is zero
/// everywhere else, and past +0x90 the sequence starts over - clip 1, a fresh
/// effect id out of `D_actor_521100_8015F5F4` (the top four bits of an LCG
/// draw) into `field_68E`, and the state latch `field_69E`, its phase
/// `field_6A0` and the armed flag all cleared.
static void func_actor_521100_8013334C(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    u32              rng;
    s16              clip;
    u16              clipId;
    s16              turn;
    s16              speed;
    s16              frame;
    s16              frame2;
    s32              frame3;
    s32              snd;
    s32              pan;

    work                          = arg0->work;
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    clip                          = D_actor_521100_8015F894[work->field_686];
    clipId                        = D_actor_521100_8015F894[work->field_686];
    coord                         = arg0->extra.tmd->coords;

    turn = 0;
    if ((s16)work->field_68A < clip + 0x28) {
        turn = 0x50;
    }
    work->field_69C = turn;

    frame = (s16)work->field_68A;
    if (frame == clip + 0x23) {
        Gp_SpawnEff(EFFECT_NO9_GOLEM_SWING_TRAIL, arg0->extra.tmd->coords + 8, 0xC, NULL);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x27) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, work->field_654->extra.tmd->coords, 0, vec);
    }

    frame2 = (s16)work->field_68A;
    if (frame2 == (s16)clipId + 0x23) {
        work->field_6AE     = 1;
        work->obj57C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj59C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        snd                 = (((u32)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0008;
        pan                 = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == (s16)clipId + 0x2D) {
            work->field_6A6     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }

    frame3 = (s16)work->field_68A;
    if ((s16)clipId + 0x20 < frame3) {
        if ((s16)clipId + 0x2A >= frame3) {
            speed = 0x88;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= (s16)clipId + 0x90) {
        work->field_686 = 1;
        work->field_69E = 0;
        work->field_6A0 = 0;
        tbl             = D_actor_521100_8015F5F4;
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        work->field_68E = tbl[(rng >> 16) & 0xF];
        work->field_6AE = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
/// Step-0 body of the burn-out sequence: the transition into it and the two
/// respawn draws. `field_6A2` is a four-phase latch. Phase 0 waits out the clip
/// long enough for the actor to commit (`D_actor_521100_8015F894[field_686]`
/// plus 0x38) and then latches clip 4 and hands phase 1 a fresh effect id out
/// of `D_actor_521100_8015F5D4`, cueing the 0x401C0007 sound with the actor's
/// own pan and depth. Phase 1 counts the effect id down (unsigned `field_68E`,
/// tested as a halfword) and, when it lands, either arms the two collision
/// nodes with the type-2 pair and asks for clip 8, or - once `field_6AA` has
/// run out - drops the actor back to idle with a draw out of
/// `D_actor_521100_8015F5F4`.
///
/// Phase 2 walks the turn limit `field_69C` 0x3C up while the clip is young,
/// fires the 0x401C0009 cue, the effect 0x60188 on the eighth coordinate and
/// the 0xA/0x40/0xFF pad lerp together on the clip's 0x20th frame, holds the
/// forward speed at 0x64 across the 0x22..0x26 window, and at 0x27 latches
/// phase 3 and hands both record flags back. Phase 3 waits 0x5E frames and then
/// picks the finish off `coord->coord.t[0]`: under -0xFA0 the actor stays
/// burning (phase 2 of the latch, or 1 in the session's mode 2), otherwise it
/// resets to idle with the clip-1 draw.
static void func_actor_521100_801335B4(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    u32              rng;
    u16              timer;
    s16              turn;
    s32              snd;
    s32              pair;

    SCRATCH_STACK_RESERVE_BYTES(0x18);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->field_6A2) {
        case 0:
            if ((s16)work->field_68A >= D_actor_521100_8015F894[work->field_686] + 0x38) {
                u16* tbl        = D_actor_521100_8015F5D4;
                work->field_686 = 4;
                work->field_6A2 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_68E = tbl[(gRandomLcgState >> 16) & 0xF];
                snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0007;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 1:
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer > 0) {
                break;
            }
            if (work->field_6AA >= 0xDAC) {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_6A2 = 0;
                work->field_686 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_68E = tbl[(gRandomLcgState >> 16) & 0xF];
            } else {
                work->field_6A2  = 2;
                work->field_686  = 8;
                pair             = Gp_PackPair(D_actor_521100_8015F550, 2);
                work->obj57C.key = pair;
                work->obj59C.key = pair;
            }
            break;
        case 2:
            turn = 0;
            if ((s16)work->field_68A < 0x20) {
                turn = 0x3C;
            }
            work->field_69C = turn;
            if ((s16)work->field_68A == 0x20) {
                work->field_6AE     = 1;
                work->obj57C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                snd                 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0009;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                Gp_SpawnEff(EFFECT_NO9_GOLEM_SWING_TRAIL, arg0->extra.tmd->coords + 8, 8, NULL);
                Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
            }
            if ((u32)(work->field_68A - 0x22) < 5) {
                work->field_69A = 0x64;
            } else {
                work->field_69A = 0;
            }
            if ((s16)work->field_68A >= 0x27) {
                work->field_6A2     = 3;
                work->field_686     = 7;
                work->field_6A6     = 0;
                work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 3:
            if ((s16)work->field_68A < 0x5E) {
                break;
            }
            if (gGameSession->location.loc.view == 2) {
                work->field_69E = 6;
                if (coord->coord.t[0] < -0xFA0) {
                    work->field_6A0 = 1;
                } else {
                    work->field_6A0 = 0;
                }
            } else if (coord->coord.t[0] < -0xFA0) {
                work->field_69E = 6;
                work->field_6A0 = 2;
            } else {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_686 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_68E = tbl[(gRandomLcgState >> 16) & 0xF];
            }
            work->field_6AE = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_521100_801339B0(Task* arg0)
{
    Actor521100Work*        work;
    GfxCoord*               coord;
    GfxCoord*               pcoord;
    Task*                   player;
    Actor521100FireScratch* sc;
    s32                     flag;
    s32                     i;
    s32                     snd;
    s32                     absDiff;
    s32                     angle;
    s16                     state;
    s16                     turn;
    u16                     timer;
    u32                     rng;
    u16*                    tbl;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(0x54);
    sc = SCRATCH_STACK_CURSOR(Actor521100FireScratch);

    switch (work->field_6A0) {
        case 0:
            if ((s16)work->field_68A == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            } else if ((s16)work->field_68A == 0xC) {
                flag                         = (gPlayerStatus.coordMtx->m[0][2] * coord->coord.m[0][2] + gPlayerStatus.coordMtx->m[1][2] * coord->coord.m[1][2] + gPlayerStatus.coordMtx->m[2][2] * coord->coord.m[2][2]);
                work->field_6A4              = (u32)flag >> 31;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = work->field_6A4 ? 2 : 6;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            } else if ((s16)work->field_68A >= 0x2B) {
                work->field_6A0              = 1;
                work->field_686              = 0xB;
                work->field_68E              = 0;
                work->field_690              = 0;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = work->field_6A4 ? 3 : 7;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            if ((u16)(work->field_68A - 4) < 9) {
                sc->vec.vz = 0x4E2;
                sc->vec.vx = 0;
                sc->vec.vy = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                sc->pos.vx   = coord->coord.t[0] + sc->pos.vx;
                sc->pos.vy   = coord->coord.t[1] + sc->pos.vy;
                sc->pos.vz   = coord->coord.t[2] + sc->pos.vz;
                pcoord       = player->extra.tmd->coords;
                sc->delta.vx = sc->pos.vx - pcoord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = sc->pos.vz - pcoord->coord.t[2];
                if ((SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x32) || ((s16)work->field_68A == 0xC)) {
                    sc->aim.pos.vx = sc->pos.vx;
                    sc->aim.pos.vy = sc->pos.vy;
                    sc->aim.pos.vz = sc->pos.vz;
                } else {
                    VectorNormal(&sc->delta, &sc->pos);
                    sc->aim.pos.vx = pcoord->coord.t[0] + ((sc->pos.vx * 0x32) >> 12);
                    sc->aim.pos.vy = pcoord->coord.t[1] + ((sc->pos.vy * 0x32) >> 12);
                    sc->aim.pos.vz = pcoord->coord.t[2] + ((sc->pos.vz * 0x32) >> 12);
                }
                angle          = ratan2(pcoord->coord.m[0][2], pcoord->coord.m[2][2]) & 0xFFF;
                flag           = (s16)work->field_696 - angle;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vz = 0;
                if ((s16)work->field_68A == 0xC) {
                    sc->aim.rot.vy = work->field_696;
                } else {
                    absDiff = flag >= 0 ? flag : -flag;
                    if ((u32)(absDiff - 0x400) >= 0x801U) {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = work->field_696;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle + 0x64;
                        } else {
                            sc->aim.rot.vy = angle - 0x64;
                        }
                    } else {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = (work->field_696 + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle - 0x64;
                        } else {
                            sc->aim.rot.vy = angle + 0x64;
                        }
                    }
                }
                TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->aim, 0);
            }
            break;
        case 1:
            flag = 0;
            if (work->field_68E == 2) {
                Gp_SpawnPadLerp(5, 0xC0, 0x80);
            }
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer <= 0) {
                if (gPlayerStatus.hp <= D_actor_521100_8015F570[gSceneCombatState.difficulty]) {
                    work->field_686              = 0x14;
                    work->field_6A0              = 5;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = work->field_6A4 ? 0xC : 0xD;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                    flag = 1;
                } else {
                    work->field_68E = 0x20;
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_521100_8015F550, 3), 0);
                }
            }
            if (flag != 1) {
                flag = 0;
                if ((work->field_6A8 == 1) && (gPlayerStatus.hp < 0x3D) && (((D_actor_521100_8015F560.hpMax / 3) & 0xFFFF) >= ((Enemy*)arg0->spawnArg2.pointer)->hp)) {
                    flag = work->field_6A4 == 1;
                }
                if (flag != 0) {
                    work->field_6A0              = 3;
                    work->field_6A8              = 0;
                    work->field_68A              = 0;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = 5;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                } else {
                    if (work->field_6A8 == 0) {
                        timer           = work->field_690 + 1;
                        work->field_690 = timer;
                        if ((s16)timer < 0x97) {
                            break;
                        }
                    }
                    work->field_6A0              = 2;
                    work->field_686              = 0x13;
                    work->field_6A8              = 0;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = work->field_6A4 ? 9 : 0xA;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                }
            }
            break;
        case 2:
            if ((s16)work->field_68A == 0x22) {
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if ((s16)work->field_68A == 0x25) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000F;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_68A < 0x45) {
                for (i = 0; i < 0x11; i++) {
                    if ((s16)work->field_68A < D_actor_521100_8015F80C[work->field_6A4][i].field_0) {
                        sc->vec.vx = D_actor_521100_8015F80C[work->field_6A4][i].field_2;
                        break;
                    }
                }
                sc->vec.vy = 0;
                sc->vec.vz = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                pcoord         = player->extra.tmd->coords;
                sc->aim.pos.vx = pcoord->coord.t[0] + sc->pos.vx;
                sc->aim.pos.vy = pcoord->coord.t[1] + sc->pos.vy;
                sc->aim.pos.vz = pcoord->coord.t[2] + sc->pos.vz;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = work->field_696;
                sc->aim.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->aim, 0);
            }
            if ((s16)work->field_68A == 0x23) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
            }
            if ((s16)work->field_68A == 0x45) {
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = 0xB;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            turn = 0;
            if ((s16)work->field_68A < 0x5F) {
                turn = -0x10;
            }
            work->field_69A = turn;
            if ((s16)work->field_68A == 0x6F) {
                coord          = player->extra.tmd->coords;
                sc->aim.pos.vx = coord->coord.t[0];
                sc->aim.pos.vy = coord->coord.t[1];
                sc->aim.pos.vz = coord->coord.t[2];
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = (work->field_696 + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                sc->aim.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->aim, 0);
            }
            if (((s16)work->field_68A >= 0x6F) && (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if ((s16)work->field_68A >= 0xA4) {
                tbl             = D_actor_521100_8015F5F4;
                work->field_686 = 1;
                work->field_69E = 0;
                work->field_6A0 = 0;
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            break;
        case 3:
            if ((s16)work->field_68A == 0x20) {
                Gp_SpawnEff(EFFECT_DILAPIDATED_HOUSE_FIRE_BLAST, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 0xC, 0, NULL);
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000E;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_6A0              = 4;
                work->field_686              = 0xD;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = 4;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            break;
        case 4:
            if ((s16)work->field_68A == 0xF) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000C;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_68A == 0x64) {
                ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            }
            break;
        case 5:
            if ((s16)work->field_68A == 0x1A) {
                ((GameActor*)player->work)->state = 0xA;
                work->field_6A0                   = 6;
                work->field_68E                   = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                sc->vec.vx                        = 0;
                sc->vec.vy                        = -0x96;
                sc->vec.vz                        = 0xC8;
                func_800FDB18(1, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 4, &sc->vec,
                              &D_actor_521100_8015F804);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                taskMessageDispatch(player, 0x400, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case 6:
            state = (s16)work->field_68E;
            if (state != 1) {
                if (state < 2) {
                    if (state == 0) {
                        CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                        work->field_68E = 1;
                    }
                }
            } else if ((CdCmd_IsIdle() & 0xFFFF) == state) {
                coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                SndEvt_EnqueueType6(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_68E = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x54);
}

/// Step-4 body of the burn-out sequence, the fourth of the ones the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0xE, phase 1 waits out 5 blended frames and
/// asks for clip 0xF, and phase 2 waits out 0x26 of them and then either drops
/// the actor to the idle state or, when `field_6BA` asks for it, on to state 6
/// at sub-state `field_6BC`. Phase 2 latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F634`, the same 4-bit draw
/// `func_actor_521100_8013570C` makes.
static void func_actor_521100_80134658(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0xE;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 5) {
                work->field_686 = 0xF;
                work->field_6A0 = 2;
            }
            return;
        case 2:
            if ((s16)work->field_68A >= 0x26) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// State-6 body of the burn-out sequence, the last one the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch again and `D_actor_521100_8015F654` holds one waypoint per
/// phase; every phase seeds the slot blend the same way (`field_686` 0x12,
/// forward speed `field_69A` 0x14, turn limit `field_69C` 0x78) and builds the
/// vector from the attach coordinate's translation to its target into the
/// 0x18-byte scratch, of which only the `vec` half is written.
///
/// Phase 0 aims at the player (`gPlayerStatus.coordMtx->t`) and hands the actor
/// back to state 1, speed zeroed, once it is within 0x7D0 of it and the
/// player's own Z is past -0x5DC; otherwise it aims at waypoint 0 and steps the
/// phase to 1 on arrival within 0x3C. Those two paths leave the switch
/// directly, while the ones that reach neither clear `field_69E` and
/// `field_6A0` - or raise `field_6BA` only, in a session whose `field_4` is 2 -
/// and then clear `field_6BC`. Phase 1 aims at the player and falls back to
/// waypoint 1 past 0x7D0, re-aiming at the player from 0x3C of that waypoint;
/// the within-0x7D0 path and the re-aim one share the epilogue that stops the
/// actor (`field_698` re-aimed, `field_69A` 0, `field_69C` 0x78, `field_69E` 1,
/// `field_6A0` 0), while the far one goes to `game`, where the phase steps to 2
/// and both `field_6BA` / `field_6BC` are cleared, or both raised when
/// `field_4` is 2. Phase 2 aims at waypoint 2 and, from the coordinate's X past
/// -0xFA0, clears `field_69E` and `field_6A0` (the session check there only
/// clears `field_6A0`), then drops both flags.
///
/// `sc2` is a second view of the same scratch that only phase 0's else branch
/// reads: CSE folds its initialisation into a copy of `sc`, and the
/// `do { ... } while (0)` around phase 1's `ratan2` is what keeps `head` ahead
/// of that copy in the register allocator's order - see
/// `DECOMPILATION_LEARNINGS.md`, "loop_depth as an allocation weight".
static void func_actor_521100_80134774(Task* arg0)
{
    Actor521100Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    ActorFaceScratch* sc2;
    u8*               head;
    s16               state;

    head                     = SCRATCH_STACK_CURSOR(u8);
    sc                       = (ActorFaceScratch*)(head - 0x18);
    sc2                      = sc;
    SCRATCH_STACK_CURSOR(u8) = (u8*)sc;
    work                     = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    state                    = work->field_6A0;
    switch (state) {
        case 0:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x7D0) && (gPlayerStatus.coordMtx->t[2] < -0x5DC)) {
                work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
                work->field_69A = 0;
                work->field_69C = 0x78;
                work->field_69E = 1;
                work->field_6A0 = 0;
            } else {
                sc2->delta.vx   = D_actor_521100_8015F654[0].vx - coord->coord.t[0];
                sc2->delta.vy   = 0;
                sc2->delta.vz   = D_actor_521100_8015F654[0].vz - coord->coord.t[2];
                work->field_698 = (u16)(ratan2((s16)sc2->delta.vx, (s16)sc2->delta.vz) & 0xFFF);
                if (SquareRoot0((sc2->delta.vx * sc2->delta.vx) + (sc2->delta.vz * sc2->delta.vz)) < 0x3C) {
                    work->field_6A0 = 1;
                } else {
                    if (gGameSession->location.loc.view != 2) {
                        work->field_69E = 0;
                        work->field_6A0 = 0;
                        work->field_6BA = 0;
                    } else {
                        work->field_6BA = 1;
                    }
                    work->field_6BC = 0;
                }
            }
            break;
        case 1:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) >= 0x7D0) {
                sc->delta.vx = D_actor_521100_8015F654[1].vx - coord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = D_actor_521100_8015F654[1].vz - coord->coord.t[2];
                do {
                    work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
                    if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) >= 0x3C) {
                        goto game;
                    }
                } while (0);
                sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            }
            work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
            work->field_69A = 0;
            work->field_69C = 0x78;
            work->field_69E = 1;
            work->field_6A0 = 0;
            break;
        game:
            if (gGameSession->location.loc.view != 2) {
                work->field_6A0 = 2;
                work->field_6BA = 0;
                work->field_6BC = 0;
            } else {
                work->field_6BA = 1;
                work->field_6BC = 1;
            }
            break;
        case 2:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = D_actor_521100_8015F654[2].vx - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = D_actor_521100_8015F654[2].vz - coord->coord.t[2];
            work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
            if (coord->coord.t[0] < -0xFA0) {
                if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x3C) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else if (gGameSession->location.loc.view == state) {
                    work->field_6A0 = 0;
                }
            } else {
                if (gGameSession->location.loc.view != state) {
                    work->field_69E = 0;
                }
                work->field_6A0 = 0;
            }
            work->field_6BA = 0;
            work->field_6BC = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
/// Steers the actor's heading towards the work block's `field_698` at up to
/// `field_69C` of turn per frame, then builds the result into the attach
/// coordinate as a pure-yaw rotation. The heading error is `field_698` minus
/// the coordinate's own Z-axis yaw (`ratan2` of `m[0][2]` over `m[2][2]`,
/// masked to the 12 bits the rotation is measured in), taken signed; the new
/// `field_696` is the target when the error is within the turn limit, and the
/// current yaw stepped by that limit otherwise. Errors past half a turn take
/// the short way round the wrap: the limit only has to beat `0x1000` minus the
/// error (or the error plus `0x1000`) to snap, so the turn never crosses into
/// the far half. `field_696` is read back as a signed half, the form the
/// sibling overlays' work blocks declare their yaw in; this body is the same
/// one `Actor02500_Fn016FC` and `func_actor_300700_80164794` carry.
static void func_actor_521100_80134C38(Task* arg0)
{
    Actor521100Work*  work;
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
    want  = work->field_698;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_696 = ang;
    if (adiff < 0x800) {
        step = work->field_69C;
        if (step >= adiff) {
            work->field_696 = want;
        } else {
            next = (s16)work->field_696;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_696 = next;
        }
    } else {
        step = work->field_69C;
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
        work->field_696 = work->field_698;
        goto done;
    turn:
        wrapStep = work->field_69C;
        cur      = (s16)work->field_696;
        if (diff > 0) {
            work->field_696 = cur - wrapStep;
        } else {
            work->field_696 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_696;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
/// Plays the actor's footstep cues: while the animation record the cue body
/// reads carries `flags` bit 0x20 (or 0x10), a sound is queued on the frame
/// that bit has just dropped from `Actor521100Work::field_6B4`, panned and
/// depth-attenuated from the actor's display coordinate. The record is the one
/// `Gp_AnimGetRec` returns for the slot at 0x3C - the second of the 0x28-byte
/// slots the actor work blocks lay out from 0x14, the same one the other actor
/// overlays' cue bodies play from. The cue id is the `Enemy` work id's bits
/// 12+ placed in bits 8-11 with the overlay's 0x401C tag, 1 for the 0x20 foot
/// and 2 for the 0x10 one, and the record's two bits are latched for the next
/// frame at the end.
static void func_actor_521100_80134D88(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    Actor521100Work*       work;
    GfxCoord*              coord;
    const AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_6B4 & ANIMATION_RECORD_CUE_2)) {
            snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0001;
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_6B4 & ANIMATION_RECORD_CUE_1)) {
            snd  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0002;
            pan2 = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->field_6B4 = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../shared/no9_golem_aim_head.inc.c"
/// Untwists the coordinate at `field_8[3]`, which `func_actor_521100_801322F8`
/// left rotated by the random residual in `Actor521100Work::field_678` on the
/// frame the actor took a hit. The residual is turned into a matrix and
/// multiplied into that coordinate's own by `Gp_MulMatrix0`'s three `rtir`
/// passes - `rtir` multiplies the GTE rotation matrix by the vector in
/// `IR1..IR3`, so the body loads the coordinate's matrix, then each row of the
/// scratch matrix in turn, storing each result back over the coordinate. The
/// two angles are then stepped 0x20 towards zero; `field_680`, the flag the hit
/// body armed, survives while either is still moving and is cleared on the
/// frame both arrive, which is what the update body tests before calling this.
///
/// Same body as `Actor02000_Fn01698` and `func_actor_510900_80138D38`.
static void func_actor_521100_80135024(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->field_678, matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    angleX = work->field_678.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_678.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_678.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_678.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_678.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_678.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_680 = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}
/// The burn-out tick `func_actor_521100_80135414` runs while the sequence state
/// `field_68C` is non-zero. `field_68E` counts the frames since the last effect
/// and fires one once it reaches `D_actor_521100_8015F8CC[field_68C]` — every 7
/// frames while the body is alight in state 1, then 0xE and 0x1C as it burns
/// down. Every effect splashes part 3 of the model's coordinate array; in state
/// 1 a second one lands on a random other part, picked out of
/// `D_actor_521100_8015F8BC` by the top three bits of an LCG draw. `field_690`
/// is the sequence's own clock, walking the state 1 -> 2 at 0xF0 frames, 2 -> 3
/// at 0x14A and 3 -> 0 at 0x1A4, where the tick stops.
static void func_actor_521100_80135230(Task* arg0)
{
    Actor521100Work* work;
    u16              timer;
    s16*             tbl;
    s16              part;

    work            = arg0->work;
    timer           = work->field_68E + 1;
    work->field_68E = timer;
    if ((s16)timer >= D_actor_521100_8015F8CC[work->field_68C]) {
        work->field_68E = 0U;
        func_800FDB18(3, &arg0->extra.tmd->coords[3], NULL, &work->eff);
        if (work->field_68C == 1) {
            tbl             = D_actor_521100_8015F8BC;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            part            = tbl[(gRandomLcgState >> 16) & 7];
            func_800FDB18(3, &arg0->extra.tmd->coords[part], NULL, &work->eff);
        }
    }
    timer           = work->field_690 + 1;
    work->field_690 = timer;
    if ((s16)timer == 0xF0) {
        work->field_68C = 2;
    }
    if ((s16)work->field_690 == 0x14A) {
        work->field_68C = 3;
    }
    if ((s16)work->field_690 >= 0x1A4) {
        work->field_68C = 0;
    }
}

/// State handlers of the actor's second part, which `func_actor_521100_80135AE4`
/// dispatches through: the setup `func_actor_521100_80135B40`, the per-frame
/// tick `func_actor_521100_80135B80` and `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 D_actor_521100_80131E40 = { {
    func_actor_521100_80135B40,
    func_actor_521100_80135B80,
    Gp_DestroyEnemy,
} };

/// The actor's task body: runs the handler for `Task::state` out of a two-entry
/// table built on the stack - the spawn state `func_actor_521100_80131E8C`,
/// then the per-frame state `func_actor_521100_801353CC` - passing the task's
/// `Enemy` along with the task.
void func_actor_521100_80135378(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_521100_80131E8C,
        func_actor_521100_801353CC,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_521100_801353CC(Enemy* arg0, Task* arg1)
{
    Actor521100Work* work;

    work = arg1->work;
    if (gGameSession->eventState != 0) {
        work->field_682 = 1;
        func_actor_521100_80135414(arg0, arg1);
        return;
    }
    work->field_682 = 0;
    func_actor_521100_80135478(arg0, arg1);
}

static void func_actor_521100_80135414(Enemy* arg0, Task* arg1)
{
    Actor521100Work* temp_s0;

    temp_s0                      = arg1->work;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    func_actor_521100_80135964(arg1);
    func_actor_521100_80135A34(arg1);
    no9GolemDrawShadow(arg1);
    if (temp_s0->field_68C != 0) {
        func_actor_521100_80135230(arg1);
    }
}

static void func_actor_521100_80135478(Enemy* arg0, Task* arg1)
{
    GfxCoord*        temp_s2;
    TmdObject*       temp_a1;
    Actor521100Work* temp_s1;
    s32              state;
    s32              one;

    temp_a1 = arg1->extra.tmd;
    state   = gSceneCombatState.actorControl;
    temp_s1 = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
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
    temp_a1->flags                       = 0;
    temp_s1->field_654->extra.tmd->flags = 0;
    arg0->node.state.parts.flags         = WORLD_TARGET_HIDE_HP;
    goto default_body;
case2:
    temp_a1->flags                       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    temp_s1->field_654->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags         = one;
    return;
default_body:
    if (temp_s1->field_6B0 == 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    func_actor_521100_801322F8(arg1, temp_a1, one);
    func_actor_521100_801355C8(arg1);
    func_actor_521100_80134C38(arg1);
    func_actor_521100_801358D4(arg1);
    func_actor_521100_80134D88(arg1);
    func_actor_521100_80135964(arg1);
    no9GolemAimHead(arg1);
    if (temp_s1->field_680 != 0) {
        func_actor_521100_80135024(arg1);
    }
    temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(temp_s2);
case1:
    func_actor_521100_80135A34(arg1);
    no9GolemDrawShadow(arg1);
}

static void func_actor_521100_801355C8(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor521100Work*)arg0->work)->field_69E;
    switch (temp_v1) {
        case 0:
            func_actor_521100_80132958(arg0);
            return;
        case 1:
            func_actor_521100_80132DE8(arg0);
            return;
        case 2:
            func_actor_521100_801339B0(arg0);
            return;
        case 3:
            func_actor_521100_8013570C(arg0);
            return;
        case 4:
            func_actor_521100_80134658(arg0);
            return;
        case 5:
            func_actor_521100_801357F0(arg0);
            return;
        case 6:
            func_actor_521100_80134774(arg0);
        default:
            return;
    }
}

/// Steps the actor into state 1 once its facing has come within 45 degrees of
/// the angle at `field_696`, then stops it: both speeds are zeroed.
static void func_actor_521100_80135680(Task* arg0)
{
    Actor521100Work* work;
    s16              delta;
    s16              angle;
    s16              wrapped;
    s32              magnitude;

    work      = arg0->work;
    delta     = work->field_698 - work->field_696;
    magnitude = abs(delta);
    if (magnitude < 0x800) {
        angle = magnitude;
    } else {
        if (delta > 0) {
            wrapped = 0x1000 - delta;
        } else {
            wrapped = delta + 0x1000;
        }
        angle = wrapped;
    }
    if ((angle < 0x200) && (work->field_6AA < 0xDAC)) {
        work->field_69E = 1;
        work->field_6A0 = 0;
        work->field_69A = 0;
        work->field_69C = 0;
    }
}

/// Step-3 body of the burn-out sequence, the third of the three the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is its own
/// two-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0x10, phase 1 waits out 0x37 blended frames and
/// then either drops the actor to the idle state or, when `field_6BA` asks for
/// it, on to state 6 at sub-state `field_6BC`. Either way it latches clip 1 for
/// the blend and picks this frame's effect out of `D_actor_521100_8015F634`.
static void func_actor_521100_8013570C(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x10;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 0x37) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Step-5 body of the burn-out sequence, the same two-phase `field_6A0` latch
/// `func_actor_521100_8013570C` runs with the longer timing: phase 0 hands the
/// record flags at 0x59A / 0x5BA back and asks the slot blend for clip 0x11,
/// phase 1 waits out 0x48 blended frames and then either drops the actor to the
/// idle state or, when `field_6BA` asks for it, on to state 6 at sub-state
/// `field_6BC`. Either way it latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F5F4`.
static void func_actor_521100_801357F0(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x11;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 0x48) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F5F4;
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Snapshots the attach coordinate's translation into the work block, then
/// walks the coordinate forward: 0x80 up, and along its own facing axis
/// (`m[0][2]` / `m[2][2]`) scaled by the work block's speed in 12-bit fixed
/// point.
static void func_actor_521100_801358D4(Task* arg0)
{
    GfxCoord*        coord;
    Actor521100Work* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->field_64C    = coord->coord.t[0];
    work->field_64E    = coord->coord.t[1];
    work->field_650    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_69A) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_69A) >> 12;
}

/// Blends every animation slot towards the clip latched in `field_686` while
/// it differs from the clip the slots carry, then ticks them once they agree:
/// the blend runs the nineteen slots through `animationSeekSlotWithBlend` with the length
/// `D_actor_521100_8015F894` gives the incoming clip, and the tick counts the
/// agreeing frames in `field_68A`.
static void func_actor_521100_80135964(Task* arg0)
{
    Actor521100Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    val  = 0;
    if (work->field_686 != work->field_688) {
        work->field_688 = work->field_686;
        work->field_68A = 0;
        if (work->field_686 < 0x15) {
            val = D_actor_521100_8015F894[work->field_686];
        }
        i = 1;
        do {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->field_686, 0, val);
            i++;
        } while (i < 0x13);
        return;
    }
    i                = 1;
    work->field_68A += i;
    do {
        animationTickSlot(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Colours the actor from the world position of its second model coordinate,
/// handing it to `Gp_UpdateActorColor` with no blend parameters.
static void func_actor_521100_80135A34(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = &arg0->extra.tmd->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

#include "../../shared/no9_golem_draw_shadow.inc.c"

/// Task body of the actor's second part: copies `D_actor_521100_80131E40`
/// onto the stack and runs the handler for `Task::state` on the task's
/// `Enemy`.
void func_actor_521100_80135AE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_521100_80131E40;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Setup state of the actor's second part: hangs its model coordinate under
/// the parent model's ninth coordinate, draws it under the parent work block's
/// light and colour matrices, shows it and moves the task on to its tick.
static void func_actor_521100_80135B40(Enemy* enemy, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor521100Work* work;
    GfxCoord*        coord;
    GfxCoord*        parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor521100Work*)parent->work;

    coord->parent = &parentCoords[8];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

static void func_actor_521100_80135B80(Enemy* arg0, Task* task)
{
    TmdObject*       obj;
    Actor521100Work* work;
    s16              mode;

    work = (Actor521100Work*)task->parent->work;
    obj  = task->extra.tmd;
    if (work->field_682 != 0) {
        mode       = ((work->field_692 & 1) == 0) << 7;
        obj->flags = mode;
        if (work->field_692 & 2) {
            obj->flags = mode | TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        if (work->field_694 != 0) {
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

s32 func_actor_521100_80135BEC(Task* arg0)
{
    if (gPlayerStatus.hp > 0) {
        ((Actor521100Work*)arg0->work)->field_6A8 = 1;
    }
    return 0;
}

s32 func_actor_521100_80135C14(Task* arg0, s32 arg1, AnimationPlayRequest* args)
{
    Actor521100Work* work;
    s32              i;
    s32              frames;
    s16              clip;
    s16              base;

    frames = 0;
    work   = arg0->work;
    base   = 0x1D;
    if (args->source.index == 0) {
        base = 0x14;
    }
    clip            = args->animationId + base;
    work->field_686 = clip;
    work->field_688 = clip;
    if (args->blend != ANIMATION_BLEND_RESET) {
        frames = args->blendFrames;
    }
    for (i = 1; i < 0x13; i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->field_686, 0, frames);
    }
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"
