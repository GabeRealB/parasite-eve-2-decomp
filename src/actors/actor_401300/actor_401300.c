#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
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
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"

/// XZ patrol point in `Actor401300Work::field_C`. Same shape as
/// `Actor01900Waypoint`.
typedef struct Actor401300Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401300Waypoint;

/// Private work block of the actor 401300 task, hanging off `Task::work`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x18
/// (the heading `func_actor_401300_80141614` reads back from the root
/// coordinate, one halfword later than `ActorYawWork::yaw`), the
/// three `WorldCollisionBody` collision bodies `func_actor_401300_80141758` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword the
/// teardown-ish `func_actor_401300_80141EF8` tests before it stamps the enemy's
/// `field_40` with the -999 sentinel. The block is a good deal larger - sibling
/// `func_actor_401300_80141C88` reads animation state at 0x89C..0xC0E of the
/// same pointer - so the struct stays open-ended.
///
/// The display nodes do *not* sit at the same addresses as the same-shaped
/// teardown of actor 01900/401800, which keeps its three at 0x8C8/0xA08/0xB48.
/// Animation halfwords sit 4 bytes later than `Actor01900Work` (0x89C vs
/// 0x898); the three `WorldCollisionBody` nodes sit 0xA8 later (0x970/0xAB0/0xBF0).
typedef struct Actor401300Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ s16  field_6;
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// XZ patrol points: the spawn position and one step along its facing.
    /* 0x00C */ Actor401300Waypoint field_C[2];
    /// Lunge step length: `func_actor_401300_8013E930`'s clamped player
    /// distance over 18.
    /* 0x014 */ s16  field_14;
    /* 0x016 */ s16  field_16;
    /* 0x018 */ s16  yaw;
    /* 0x01A */ byte pad_1A[0x44];
    /* 0x05E */ u16  field_5E;
    /* 0x060 */ byte pad_60[0xC];
    /* 0x06C */ u16  field_6C;
    /* 0x06E */ byte pad_6E[0x82A];
    /* 0x898 */ s32  field_898;
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ s16  field_8A0;
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ s16  field_8A6;
    /* 0x8A8 */ s16  field_8A8;
    /* 0x8AA */ s16  field_8AA;
    /* 0x8AC */ s16  field_8AC;
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ s16  field_8B2;
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s16  field_8B6;
    /* 0x8B8 */ s16  field_8B8;
    /* 0x8BA */ s16  field_8BA;
    /* 0x8BC */ s32  field_8BC;
    /// Effect anchor `func_actor_401300_80139134` places at the actor's
    /// view-space position before spawning effect 0x600A5.
    /* 0x8C0 */ GfxCoord       field_8C0;
    /* 0x910 */ EffectSpawnArg field_910;
    /* 0x918 */ byte           pad_918[8];
    /// Fixed pose `func_actor_401300_80134454` anchors above the root
    /// coordinate (identity rotation, 0x15E up) for the `field_AB0` node.
    /* 0x920 */ GfxCoord           field_920;
    /* 0x970 */ WorldCollisionBody field_970;
    /// Contact records, walked twelve at a time by the movement helpers.
    /* 0x990 */ WorldCollisionContact field_990[12];
    /* 0xAB0 */ WorldCollisionBody    field_AB0;
    /// Contact records the `field_AB0` node's context points at.
    /* 0xAD0 */ WorldCollisionContact field_AD0[12];
    /* 0xBF0 */ WorldCollisionBody    field_BF0;
    /* 0xC10 */ WorldCollisionContact sensorContacts[1]; // Single result for the sensor body
                                                         /// Light matrix `func_actor_401300_80134454` binds to the model's
                                                         /// `TmdObject::lightMtx` (the color matrix is `field_C48`).
    /* 0xC28 */ MATRIX field_C28;
    /// Saved at 0xC48 and copied over 0xC68 when
    /// `func_actor_401300_80139520` enters its state.
    /* 0xC48 */ MATRIX  field_C48;
    /* 0xC68 */ MATRIX  field_C68;
    /* 0xC88 */ s16     field_C88;
    /* 0xC8A */ s16     field_C8A;
    /* 0xC8C */ SVECTOR field_C8C;
    /* 0xC94 */ s16     field_C94;
    /* 0xC96 */ s16     field_C96;
    /* 0xC98 */ s16     field_C98;
    /* 0xC9A */ byte    pad_C9A[2];
    /* 0xC9C */ s16     field_C9C;
    /* 0xC9E */ s16     field_C9E;
    /* 0xCA0 */ u16     field_CA0;
    /* 0xCA2 */ s16     field_CA2;
    /* 0xCA4 */ s16     field_CA4;
    /* 0xCA6 */ byte    pad_CA6[2];
    /// Copy of the first three bytes of the last event
    /// `func_actor_401300_80132554` handled.
    /* 0xCA8 */ u8                   field_CA8[3];
    /* 0xCAB */ byte                 pad_CAB;
    /* 0xCAC */ AnimationPlayRequest field_CAC;
    /* 0xCC0 */ s32                  field_CC0[3];
    /* 0xCCC */ byte                 pad_CCC[4];
    /* 0xCD0 */ s16                  field_CD0;
    /* 0xCD2 */ u8                   field_CD2;
    /* 0xCD3 */ byte                 pad_CD3;
    /// Player position and facing sent with message 0x3E9 by
    /// `func_actor_401300_80138800`.
    /* 0xCD4 */ VECTOR  field_CD4;
    /* 0xCE4 */ SVECTOR field_CE4;
    /// Payload `func_actor_401300_80138160` sends with message 0x3F8.
    /* 0xCEC */ byte field_CEC[0x14];
    /* 0xD00 */ s32  field_D00;
    /// Root position saved by `func_actor_401300_8013F628` on entry.
    /* 0xD04 */ s16  field_D04;
    /* 0xD06 */ s16  field_D06;
    /* 0xD08 */ s16  field_D08;
    /* 0xD0A */ byte pad_D0A[2];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xD0C */ Task*   field_D0C;
    /* 0xD10 */ Task*   field_D10;
    /* 0xD14 */ SVECTOR home;
    /* 0xD1C */ s16     field_D1C;
    /* 0xD1E */ s16     field_D1E;
    /* 0xD20 */ s16     field_D20;
    /* 0xD22 */ s16     field_D22;
    /* 0xD24 */ byte    pad_D24[4];
    /// Ring of the last seven view-space positions of coordinate 2, written by
    /// `func_actor_401300_801405DC`; `field_D78` is the write cursor.
    /* 0xD28 */ SVECTOR field_D28[7];
    /* 0xD60 */ byte    pad_D60[0x18];
    /* 0xD78 */ s16     field_D78;
    /* 0xD7A */ byte    pad_D7A[2];
} Actor401300Work;
STATIC_ASSERT_SIZEOF(Actor401300Work, 0xD7C);

/// The actor's state handlers, indexed by `Actor401300Work::field_0`.
/// `func_actor_401300_801405DC` copies the table to its frame before
/// dispatching.
typedef struct Actor401300StateTable {
    TaskFunc fn[41];
} Actor401300StateTable;
STATIC_ASSERT_SIZEOF(Actor401300StateTable, 0xA4);

/// Animation view of the same work block, as `func_actor_401300_80133324`
/// reads it: the `Actor01900AnimWork` layout shifted 4 bytes later, like the
/// rest of this overlay's animation fields.
typedef struct Actor401300AnimWork {
    /* 0x000 */ byte           pad_0[0x20];
    /* 0x020 */ ActorAnimRig19 rig;
    /* 0x45C */ ActorAnimRig19 blend;
    /* 0x898 */ byte           pad_898[0x8];
    /* 0x8A0 */ s16            field_8A0;
    /* 0x8A2 */ s16            field_8A2;
    /* 0x8A4 */ byte           pad_8A4[2];
    /* 0x8A6 */ s16            field_8A6;
    /* 0x8A8 */ byte           pad_8A8[4];
    /* 0x8AC */ s16            field_8AC;
    /* 0x8AE */ s16            field_8AE;
    /* 0x8B0 */ s16            field_8B0;
} Actor401300AnimWork;

/// Event record `func_actor_401300_80132554` dispatches on: `w[0]` is the
/// event kind (0x301, 0xB05, 0x1D05) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor401300Work::field_CA8`.
typedef union Actor401300Event {
    u8  b[3];
    u16 w[2];
} Actor401300Event;

extern ActorHeightClamp D_actor_401300_801589C8[];

/// Halfword table in the overlay's data; element 0 is the value the 0xB05/0xC
/// event writes into `Enemy::hp`. Declared as an array: a scalar lets
/// the scheduler hoist its load above the preceding store.

/// Per-animation reset argument for `animationSeekSlotWithBlend`, indexed by the previous
/// and the new animation id (`field_8A0`, `field_8A2`).
extern s8 D_actor_401300_8015804C[][45];

/// Two rest/target rotation pairs `func_actor_401300_80133834` blends by
/// `0x200 - t` (in 1/512ths) into coord 7 and coord 8.
extern SVECTOR D_actor_401300_801589F8[2];
extern SVECTOR D_actor_401300_80158A08[2];

/// Data `func_actor_401300_80134454` wires up at init: the enemy parameter
/// record (`Enemy::param`), the three per-variant `field_CA0..CA4`
/// triples selected by `spawnArg1 & 0xF`, the animation bank passed to
/// `animationBindContext`, the 0x3FF message seed, and the task's `field_24`.
extern EnemyParams   D_actor_401300_80141FA0;
extern SVECTOR       D_actor_401300_80141FB0[3];
extern AnimationSet* D_actor_401300_80158838[46];
/// The animation block the 0x3FF payload in `field_CAC` hands the player.
extern AnimationSet*        D_actor_401300_801588F0[9];
extern AnimationPlayRequest D_actor_401300_80158914;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*);
        s32  (*call1)(Task*, s32, Actor401300Event*);
        s32  (*call2)(Task*, s32, AnimationPlayRequest*);
        s32  (*call3)(Task*, s32, ActorTransform*);
        s32  (*call4)(Task*, s32, s32);
        void (*call5)(void);
    } handler;
} Actor401300MessageEntry;
STATIC_ASSERT_SIZEOF(Actor401300MessageEntry, 8);

extern Actor401300MessageEntry D_actor_401300_80158988[8];

/// Twelve vectors `func_actor_401300_80134BA4` picks from by LCG, grouped by
/// `|arg1|`: 0-4 below 0x200, 5-7 above 0x600, else 8-9 / 10-11 by sign.
/// `pad` is the model coordinate index passed to `func_800FDB18`.
extern SVECTOR D_actor_401300_80158928[12];

/// Offset to the player, distance and turn for the pursuit state.
typedef struct Actor401300PursuitScratch {
    /* 0x00 */ s32     dx;
    /* 0x04 */ s32     dy;
    /* 0x08 */ s32     dz;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ s32     dist;
    /* 0x14 */ SVECTOR delta;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     pad_22;
} Actor401300PursuitScratch;
STATIC_ASSERT_SIZEOF(Actor401300PursuitScratch, 0x24);

/// 0x24-byte scratch stack block `func_actor_401300_8013E930` takes: the
/// offset to the player (full width for the distance, halfwords for the yaw),
/// the clamped lunge range and the wrapped turn.
typedef struct Actor401300LungeScratch {
    /* 0x00 */ VECTOR  dist;
    /* 0x10 */ SVECTOR delta;
    /* 0x18 */ s32     range;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     pad_22;
} Actor401300LungeScratch;
STATIC_ASSERT_SIZEOF(Actor401300LungeScratch, 0x24);

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401300_8013B6E8`.

/// The records closing three of the overlay's model streams, which
/// `func_actor_401300_8013B6E8` points `D_80114B34[5].data.model` at before spawning.
static TmdSource _gActor401300HornedStrangerEffect1;
static TmdSource _gActor401300HornedStrangerBurstHead;
static TmdSource _gActor401300HornedStrangerEffect2;

/// Overlay-data word `func_actor_401300_801397F8` points
/// `D_actor_401300_80158838[16]` at on entering its state.
extern AnimationSet gActor401300Animation20D98;

static void func_actor_401300_80141758(Task* task);
static void func_actor_401300_8014192C(Task* arg0);
static void func_actor_401300_801419B8(Task* arg0);
static void func_actor_401300_80141A60(Task* arg0);
static void func_actor_401300_80141B0C(Task* arg0);
static void func_actor_401300_80141BC8(Task* arg0);
static void func_actor_401300_80141C80(Task* arg0);
static void func_actor_401300_80141C88(Task* arg0);
static void func_actor_401300_80141D50(Task* arg0);
static void func_actor_401300_80141DF4(Task* arg0);
static void func_actor_401300_80141EF8(Task* task);

extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern DamageAttack D_actor_401300_80141F88[];

static AnimationSet _gActor401300Animation24A48;
static AnimationSet _gActor401300Animation251E4;
static AnimationSet _gActor401300Animation259F0;
static AnimationSet _gActor401300Animation26204;
static TmdSource    _gActor401300HornedStrangerBody;
s32                 func_actor_401300_80132554(Task*, s32, Actor401300Event*);
s32                 func_actor_401300_80141494(Task*, s32, AnimationPlayRequest*);
s32                 func_actor_401300_80141614(Task*, s32, ActorTransform* placement);
void                func_actor_401300_8014148C(void);
void                func_actor_401300_80141F2C(Task*);

DamageAttack D_actor_401300_80141F88[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams D_actor_401300_80141FA0 = {
    D_actor_401300_80141F88,
    420,
    115,
    200,
    5,
    100,
    10,
    100,
    10,
};

SVECTOR D_actor_401300_80141FB0[3] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

static TmdBone _gActor401300HornedStrangerBodySkeleton[21] = {
#include "assets/horned_stranger_body_skeleton.inc"
};

static u32 _gActor401300HornedStrangerBodyPartVerts[21] = {
#include "assets/horned_stranger_body_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerBodyVerts[293] = {
#include "assets/horned_stranger_body_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerBodyNormals[363] = {
#include "assets/horned_stranger_body_normals.inc"
};

static u32 _gActor401300HornedStrangerBodyStream[3776] = {
#include "assets/horned_stranger_body_stream.inc"
};

static TmdSource _gActor401300HornedStrangerBody = {
    0,
    18488,
    7600,
    21,
    _gActor401300HornedStrangerBodyPartVerts,
    _gActor401300HornedStrangerBodyVerts,
    _gActor401300HornedStrangerBodyNormals,
    _gActor401300HornedStrangerBodySkeleton,
    _gActor401300HornedStrangerBodyStream,
};

static TmdBone _gActor401300HornedStrangerEffect1Skeleton[1] = {
#include "assets/horned_stranger_effect_1_skeleton.inc"
};

static u32 _gActor401300HornedStrangerEffect1PartVerts[1] = {
#include "assets/horned_stranger_effect_1_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect1Verts[31] = {
#include "assets/horned_stranger_effect_1_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect1Normals[1] = {
#include "assets/horned_stranger_effect_1_normals.inc"
};

static u32 _gActor401300HornedStrangerEffect1Stream[302] = {
#include "assets/horned_stranger_effect_1_stream.inc"
};

static TmdSource _gActor401300HornedStrangerEffect1 = {
    0,
    2004,
    0,
    1,
    _gActor401300HornedStrangerEffect1PartVerts,
    _gActor401300HornedStrangerEffect1Verts,
    _gActor401300HornedStrangerEffect1Normals,
    _gActor401300HornedStrangerEffect1Skeleton,
    _gActor401300HornedStrangerEffect1Stream,
};

static TmdBone _gActor401300HornedStrangerBurstHeadSkeleton[1] = {
#include "assets/horned_stranger_burst_head_skeleton.inc"
};

static u32 _gActor401300HornedStrangerBurstHeadPartVerts[1] = {
#include "assets/horned_stranger_burst_head_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerBurstHeadVerts[70] = {
#include "assets/horned_stranger_burst_head_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerBurstHeadNormals[85] = {
#include "assets/horned_stranger_burst_head_normals.inc"
};

static u32 _gActor401300HornedStrangerBurstHeadStream[660] = {
#include "assets/horned_stranger_burst_head_stream.inc"
};

static TmdSource _gActor401300HornedStrangerBurstHead = {
    0,
    4468,
    0,
    1,
    _gActor401300HornedStrangerBurstHeadPartVerts,
    _gActor401300HornedStrangerBurstHeadVerts,
    _gActor401300HornedStrangerBurstHeadNormals,
    _gActor401300HornedStrangerBurstHeadSkeleton,
    _gActor401300HornedStrangerBurstHeadStream,
};

static TmdBone _gActor401300HornedStrangerEffect2Skeleton[1] = {
#include "assets/horned_stranger_effect_2_skeleton.inc"
};

static u32 _gActor401300HornedStrangerEffect2PartVerts[1] = {
#include "assets/horned_stranger_effect_2_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect2Verts[7] = {
#include "assets/horned_stranger_effect_2_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect2Normals[7] = {
#include "assets/horned_stranger_effect_2_normals.inc"
};

static u32 _gActor401300HornedStrangerEffect2Stream[84] = {
#include "assets/horned_stranger_effect_2_stream.inc"
};

static TmdSource _gActor401300HornedStrangerEffect2 = {
    0,
    488,
    0,
    1,
    _gActor401300HornedStrangerEffect2PartVerts,
    _gActor401300HornedStrangerEffect2Verts,
    _gActor401300HornedStrangerEffect2Normals,
    _gActor401300HornedStrangerEffect2Skeleton,
    _gActor401300HornedStrangerEffect2Stream,
};

static AnimationPackedPose _gActor401300Animation17714Bank1[28] = {
#include "assets/actor_401300_animation_17714_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation17714Bank4[247] = {
#include "assets/actor_401300_animation_17714_bank4.inc"
};

static AnimationRecord _gActor401300Animation17714Records[362] = {
#include "assets/actor_401300_animation_17714_records.inc"
};

static u16 _gActor401300Animation17714Indices[20] = {
#include "assets/actor_401300_animation_17714_indices.inc"
};

static AnimationSet _gActor401300Animation17714 = {
    _gActor401300Animation17714Records,
    _gActor401300Animation17714Indices,
    { NULL, _gActor401300Animation17714Bank1, NULL, NULL, _gActor401300Animation17714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation186D8Bank1[29] = {
#include "assets/actor_401300_animation_186D8_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation186D8Bank4[374] = {
#include "assets/actor_401300_animation_186D8_bank4.inc"
};

static AnimationRecord _gActor401300Animation186D8Records[528] = {
#include "assets/actor_401300_animation_186D8_records.inc"
};

static u16 _gActor401300Animation186D8Indices[20] = {
#include "assets/actor_401300_animation_186D8_indices.inc"
};

static AnimationSet _gActor401300Animation186D8 = {
    _gActor401300Animation186D8Records,
    _gActor401300Animation186D8Indices,
    { NULL, _gActor401300Animation186D8Bank1, NULL, NULL, _gActor401300Animation186D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation19700Bank1[27] = {
#include "assets/actor_401300_animation_19700_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation19700Bank4[406] = {
#include "assets/actor_401300_animation_19700_bank4.inc"
};

static AnimationRecord _gActor401300Animation19700Records[527] = {
#include "assets/actor_401300_animation_19700_records.inc"
};

static u16 _gActor401300Animation19700Indices[20] = {
#include "assets/actor_401300_animation_19700_indices.inc"
};

static AnimationSet _gActor401300Animation19700 = {
    _gActor401300Animation19700Records,
    _gActor401300Animation19700Indices,
    { NULL, _gActor401300Animation19700Bank1, NULL, NULL, _gActor401300Animation19700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1A170Bank1[24] = {
#include "assets/actor_401300_animation_1A170_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1A170Bank4[248] = {
#include "assets/actor_401300_animation_1A170_bank4.inc"
};

static AnimationRecord _gActor401300Animation1A170Records[328] = {
#include "assets/actor_401300_animation_1A170_records.inc"
};

static u16 _gActor401300Animation1A170Indices[20] = {
#include "assets/actor_401300_animation_1A170_indices.inc"
};

static AnimationSet _gActor401300Animation1A170 = {
    _gActor401300Animation1A170Records,
    _gActor401300Animation1A170Indices,
    { NULL, _gActor401300Animation1A170Bank1, NULL, NULL, _gActor401300Animation1A170Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1A750Bank1[10] = {
#include "assets/actor_401300_animation_1A750_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1A750Bank4[138] = {
#include "assets/actor_401300_animation_1A750_bank4.inc"
};

static AnimationRecord _gActor401300Animation1A750Records[188] = {
#include "assets/actor_401300_animation_1A750_records.inc"
};

static u16 _gActor401300Animation1A750Indices[20] = {
#include "assets/actor_401300_animation_1A750_indices.inc"
};

static AnimationSet _gActor401300Animation1A750 = {
    _gActor401300Animation1A750Records,
    _gActor401300Animation1A750Indices,
    { NULL, _gActor401300Animation1A750Bank1, NULL, NULL, _gActor401300Animation1A750Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1AD34Bank1[8] = {
#include "assets/actor_401300_animation_1AD34_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1AD34Bank4[151] = {
#include "assets/actor_401300_animation_1AD34_bank4.inc"
};

static AnimationRecord _gActor401300Animation1AD34Records[182] = {
#include "assets/actor_401300_animation_1AD34_records.inc"
};

static u16 _gActor401300Animation1AD34Indices[20] = {
#include "assets/actor_401300_animation_1AD34_indices.inc"
};

static AnimationSet _gActor401300Animation1AD34 = {
    _gActor401300Animation1AD34Records,
    _gActor401300Animation1AD34Indices,
    { NULL, _gActor401300Animation1AD34Bank1, NULL, NULL, _gActor401300Animation1AD34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1B788Bank1[20] = {
#include "assets/actor_401300_animation_1B788_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1B788Bank4[228] = {
#include "assets/actor_401300_animation_1B788_bank4.inc"
};

static AnimationRecord _gActor401300Animation1B788Records[353] = {
#include "assets/actor_401300_animation_1B788_records.inc"
};

static u16 _gActor401300Animation1B788Indices[20] = {
#include "assets/actor_401300_animation_1B788_indices.inc"
};

static AnimationSet _gActor401300Animation1B788 = {
    _gActor401300Animation1B788Records,
    _gActor401300Animation1B788Indices,
    { NULL, _gActor401300Animation1B788Bank1, NULL, NULL, _gActor401300Animation1B788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1C4CCBank1[23] = {
#include "assets/actor_401300_animation_1C4CC_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1C4CCBank4[334] = {
#include "assets/actor_401300_animation_1C4CC_bank4.inc"
};

static AnimationRecord _gActor401300Animation1C4CCRecords[426] = {
#include "assets/actor_401300_animation_1C4CC_records.inc"
};

static u16 _gActor401300Animation1C4CCIndices[20] = {
#include "assets/actor_401300_animation_1C4CC_indices.inc"
};

static AnimationSet _gActor401300Animation1C4CC = {
    _gActor401300Animation1C4CCRecords,
    _gActor401300Animation1C4CCIndices,
    { NULL, _gActor401300Animation1C4CCBank1, NULL, NULL, _gActor401300Animation1C4CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1CD3CBank1[14] = {
#include "assets/actor_401300_animation_1CD3C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1CD3CBank4[216] = {
#include "assets/actor_401300_animation_1CD3C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1CD3CRecords[262] = {
#include "assets/actor_401300_animation_1CD3C_records.inc"
};

static u16 _gActor401300Animation1CD3CIndices[20] = {
#include "assets/actor_401300_animation_1CD3C_indices.inc"
};

static AnimationSet _gActor401300Animation1CD3C = {
    _gActor401300Animation1CD3CRecords,
    _gActor401300Animation1CD3CIndices,
    { NULL, _gActor401300Animation1CD3CBank1, NULL, NULL, _gActor401300Animation1CD3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1D0CCBank1[5] = {
#include "assets/actor_401300_animation_1D0CC_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1D0CCBank4[82] = {
#include "assets/actor_401300_animation_1D0CC_bank4.inc"
};

static AnimationRecord _gActor401300Animation1D0CCRecords[111] = {
#include "assets/actor_401300_animation_1D0CC_records.inc"
};

static u16 _gActor401300Animation1D0CCIndices[20] = {
#include "assets/actor_401300_animation_1D0CC_indices.inc"
};

static AnimationSet _gActor401300Animation1D0CC = {
    _gActor401300Animation1D0CCRecords,
    _gActor401300Animation1D0CCIndices,
    { NULL, _gActor401300Animation1D0CCBank1, NULL, NULL, _gActor401300Animation1D0CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1D904Bank1[16] = {
#include "assets/actor_401300_animation_1D904_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1D904Bank4[199] = {
#include "assets/actor_401300_animation_1D904_bank4.inc"
};

static AnimationRecord _gActor401300Animation1D904Records[259] = {
#include "assets/actor_401300_animation_1D904_records.inc"
};

static u16 _gActor401300Animation1D904Indices[20] = {
#include "assets/actor_401300_animation_1D904_indices.inc"
};

static AnimationSet _gActor401300Animation1D904 = {
    _gActor401300Animation1D904Records,
    _gActor401300Animation1D904Indices,
    { NULL, _gActor401300Animation1D904Bank1, NULL, NULL, _gActor401300Animation1D904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1DE54Bank1[10] = {
#include "assets/actor_401300_animation_1DE54_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1DE54Bank4[113] = {
#include "assets/actor_401300_animation_1DE54_bank4.inc"
};

static AnimationRecord _gActor401300Animation1DE54Records[177] = {
#include "assets/actor_401300_animation_1DE54_records.inc"
};

static u16 _gActor401300Animation1DE54Indices[20] = {
#include "assets/actor_401300_animation_1DE54_indices.inc"
};

static AnimationSet _gActor401300Animation1DE54 = {
    _gActor401300Animation1DE54Records,
    _gActor401300Animation1DE54Indices,
    { NULL, _gActor401300Animation1DE54Bank1, NULL, NULL, _gActor401300Animation1DE54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1ED6CBank1[26] = {
#include "assets/actor_401300_animation_1ED6C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1ED6CBank4[330] = {
#include "assets/actor_401300_animation_1ED6C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1ED6CRecords[538] = {
#include "assets/actor_401300_animation_1ED6C_records.inc"
};

static u16 _gActor401300Animation1ED6CIndices[20] = {
#include "assets/actor_401300_animation_1ED6C_indices.inc"
};

static AnimationSet _gActor401300Animation1ED6C = {
    _gActor401300Animation1ED6CRecords,
    _gActor401300Animation1ED6CIndices,
    { NULL, _gActor401300Animation1ED6CBank1, NULL, NULL, _gActor401300Animation1ED6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F110Bank1[10] = {
#include "assets/actor_401300_animation_1F110_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F110Bank4[73] = {
#include "assets/actor_401300_animation_1F110_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F110Records[110] = {
#include "assets/actor_401300_animation_1F110_records.inc"
};

static u16 _gActor401300Animation1F110Indices[20] = {
#include "assets/actor_401300_animation_1F110_indices.inc"
};

static AnimationSet _gActor401300Animation1F110 = {
    _gActor401300Animation1F110Records,
    _gActor401300Animation1F110Indices,
    { NULL, _gActor401300Animation1F110Bank1, NULL, NULL, _gActor401300Animation1F110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F4C4Bank1[6] = {
#include "assets/actor_401300_animation_1F4C4_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F4C4Bank4[81] = {
#include "assets/actor_401300_animation_1F4C4_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F4C4Records[118] = {
#include "assets/actor_401300_animation_1F4C4_records.inc"
};

static u16 _gActor401300Animation1F4C4Indices[20] = {
#include "assets/actor_401300_animation_1F4C4_indices.inc"
};

static AnimationSet _gActor401300Animation1F4C4 = {
    _gActor401300Animation1F4C4Records,
    _gActor401300Animation1F4C4Indices,
    { NULL, _gActor401300Animation1F4C4Bank1, NULL, NULL, _gActor401300Animation1F4C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F848Bank1[13] = {
#include "assets/actor_401300_animation_1F848_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F848Bank4[58] = {
#include "assets/actor_401300_animation_1F848_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F848Records[108] = {
#include "assets/actor_401300_animation_1F848_records.inc"
};

static u16 _gActor401300Animation1F848Indices[20] = {
#include "assets/actor_401300_animation_1F848_indices.inc"
};

static AnimationSet _gActor401300Animation1F848 = {
    _gActor401300Animation1F848Records,
    _gActor401300Animation1F848Indices,
    { NULL, _gActor401300Animation1F848Bank1, NULL, NULL, _gActor401300Animation1F848Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1FF3CBank1[15] = {
#include "assets/actor_401300_animation_1FF3C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1FF3CBank4[159] = {
#include "assets/actor_401300_animation_1FF3C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1FF3CRecords[221] = {
#include "assets/actor_401300_animation_1FF3C_records.inc"
};

static u16 _gActor401300Animation1FF3CIndices[20] = {
#include "assets/actor_401300_animation_1FF3C_indices.inc"
};

static AnimationSet _gActor401300Animation1FF3C = {
    _gActor401300Animation1FF3CRecords,
    _gActor401300Animation1FF3CIndices,
    { NULL, _gActor401300Animation1FF3CBank1, NULL, NULL, _gActor401300Animation1FF3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation20D98Bank1[18] = {
#include "assets/actor_401300_animation_20D98_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation20D98Bank4[361] = {
#include "assets/actor_401300_animation_20D98_bank4.inc"
};

static AnimationRecord _gActor401300Animation20D98Records[484] = {
#include "assets/actor_401300_animation_20D98_records.inc"
};

static u16 _gActor401300Animation20D98Indices[20] = {
#include "assets/actor_401300_animation_20D98_indices.inc"
};

AnimationSet gActor401300Animation20D98 = {
    _gActor401300Animation20D98Records,
    _gActor401300Animation20D98Indices,
    { NULL, _gActor401300Animation20D98Bank1, NULL, NULL, _gActor401300Animation20D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation21618Bank1[26] = {
#include "assets/actor_401300_animation_21618_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation21618Bank4[165] = {
#include "assets/actor_401300_animation_21618_bank4.inc"
};

static AnimationRecord _gActor401300Animation21618Records[281] = {
#include "assets/actor_401300_animation_21618_records.inc"
};

static u16 _gActor401300Animation21618Indices[20] = {
#include "assets/actor_401300_animation_21618_indices.inc"
};

static AnimationSet _gActor401300Animation21618 = {
    _gActor401300Animation21618Records,
    _gActor401300Animation21618Indices,
    { NULL, _gActor401300Animation21618Bank1, NULL, NULL, _gActor401300Animation21618Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation21DE0Bank1[24] = {
#include "assets/actor_401300_animation_21DE0_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation21DE0Bank4[150] = {
#include "assets/actor_401300_animation_21DE0_bank4.inc"
};

static AnimationRecord _gActor401300Animation21DE0Records[256] = {
#include "assets/actor_401300_animation_21DE0_records.inc"
};

static u16 _gActor401300Animation21DE0Indices[20] = {
#include "assets/actor_401300_animation_21DE0_indices.inc"
};

static AnimationSet _gActor401300Animation21DE0 = {
    _gActor401300Animation21DE0Records,
    _gActor401300Animation21DE0Indices,
    { NULL, _gActor401300Animation21DE0Bank1, NULL, NULL, _gActor401300Animation21DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation2231CBank1[18] = {
#include "assets/actor_401300_animation_2231C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation2231CBank4[107] = {
#include "assets/actor_401300_animation_2231C_bank4.inc"
};

static AnimationRecord _gActor401300Animation2231CRecords[154] = {
#include "assets/actor_401300_animation_2231C_records.inc"
};

static u16 _gActor401300Animation2231CIndices[20] = {
#include "assets/actor_401300_animation_2231C_indices.inc"
};

static AnimationSet _gActor401300Animation2231C = {
    _gActor401300Animation2231CRecords,
    _gActor401300Animation2231CIndices,
    { NULL, _gActor401300Animation2231CBank1, NULL, NULL, _gActor401300Animation2231CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation22A58Bank1[22] = {
#include "assets/actor_401300_animation_22A58_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation22A58Bank4[147] = {
#include "assets/actor_401300_animation_22A58_bank4.inc"
};

static AnimationRecord _gActor401300Animation22A58Records[230] = {
#include "assets/actor_401300_animation_22A58_records.inc"
};

static u16 _gActor401300Animation22A58Indices[20] = {
#include "assets/actor_401300_animation_22A58_indices.inc"
};

static AnimationSet _gActor401300Animation22A58 = {
    _gActor401300Animation22A58Records,
    _gActor401300Animation22A58Indices,
    { NULL, _gActor401300Animation22A58Bank1, NULL, NULL, _gActor401300Animation22A58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation2303CBank1[15] = {
#include "assets/actor_401300_animation_2303C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation2303CBank4[129] = {
#include "assets/actor_401300_animation_2303C_bank4.inc"
};

static AnimationRecord _gActor401300Animation2303CRecords[183] = {
#include "assets/actor_401300_animation_2303C_records.inc"
};

static u16 _gActor401300Animation2303CIndices[20] = {
#include "assets/actor_401300_animation_2303C_indices.inc"
};

static AnimationSet _gActor401300Animation2303C = {
    _gActor401300Animation2303CRecords,
    _gActor401300Animation2303CIndices,
    { NULL, _gActor401300Animation2303CBank1, NULL, NULL, _gActor401300Animation2303CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation23444Bank1[6] = {
#include "assets/actor_401300_animation_23444_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation23444Bank4[70] = {
#include "assets/actor_401300_animation_23444_bank4.inc"
};

static AnimationRecord _gActor401300Animation23444Records[150] = {
#include "assets/actor_401300_animation_23444_records.inc"
};

static u16 _gActor401300Animation23444Indices[20] = {
#include "assets/actor_401300_animation_23444_indices.inc"
};

static AnimationSet _gActor401300Animation23444 = {
    _gActor401300Animation23444Records,
    _gActor401300Animation23444Indices,
    { NULL, _gActor401300Animation23444Bank1, NULL, NULL, _gActor401300Animation23444Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation23A98Bank1[18] = {
#include "assets/actor_401300_animation_23A98_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation23A98Bank4[146] = {
#include "assets/actor_401300_animation_23A98_bank4.inc"
};

static AnimationRecord _gActor401300Animation23A98Records[185] = {
#include "assets/actor_401300_animation_23A98_records.inc"
};

static u16 _gActor401300Animation23A98Indices[20] = {
#include "assets/actor_401300_animation_23A98_indices.inc"
};

static AnimationSet _gActor401300Animation23A98 = {
    _gActor401300Animation23A98Records,
    _gActor401300Animation23A98Indices,
    { NULL, _gActor401300Animation23A98Bank1, NULL, NULL, _gActor401300Animation23A98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation24200Bank1[19] = {
#include "assets/actor_401300_animation_24200_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation24200Bank4[174] = {
#include "assets/actor_401300_animation_24200_bank4.inc"
};

static AnimationRecord _gActor401300Animation24200Records[223] = {
#include "assets/actor_401300_animation_24200_records.inc"
};

static u16 _gActor401300Animation24200Indices[20] = {
#include "assets/actor_401300_animation_24200_indices.inc"
};

static AnimationSet _gActor401300Animation24200 = {
    _gActor401300Animation24200Records,
    _gActor401300Animation24200Indices,
    { NULL, _gActor401300Animation24200Bank1, NULL, NULL, _gActor401300Animation24200Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation24A48Bank1[21] = {
#include "assets/actor_401300_animation_24A48_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation24A48Bank4[193] = {
#include "assets/actor_401300_animation_24A48_bank4.inc"
};

static AnimationRecord _gActor401300Animation24A48Records[254] = {
#include "assets/actor_401300_animation_24A48_records.inc"
};

static u16 _gActor401300Animation24A48Indices[20] = {
#include "assets/actor_401300_animation_24A48_indices.inc"
};

static AnimationSet _gActor401300Animation24A48 = {
    _gActor401300Animation24A48Records,
    _gActor401300Animation24A48Indices,
    { NULL, _gActor401300Animation24A48Bank1, NULL, NULL, _gActor401300Animation24A48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation251E4Bank1[20] = {
#include "assets/actor_401300_animation_251E4_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation251E4Bank4[172] = {
#include "assets/actor_401300_animation_251E4_bank4.inc"
};

static AnimationRecord _gActor401300Animation251E4Records[235] = {
#include "assets/actor_401300_animation_251E4_records.inc"
};

static u16 _gActor401300Animation251E4Indices[20] = {
#include "assets/actor_401300_animation_251E4_indices.inc"
};

static AnimationSet _gActor401300Animation251E4 = {
    _gActor401300Animation251E4Records,
    _gActor401300Animation251E4Indices,
    { NULL, _gActor401300Animation251E4Bank1, NULL, NULL, _gActor401300Animation251E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation259F0Bank1[15] = {
#include "assets/actor_401300_animation_259F0_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation259F0Bank4[206] = {
#include "assets/actor_401300_animation_259F0_bank4.inc"
};

static AnimationRecord _gActor401300Animation259F0Records[244] = {
#include "assets/actor_401300_animation_259F0_records.inc"
};

static u16 _gActor401300Animation259F0Indices[20] = {
#include "assets/actor_401300_animation_259F0_indices.inc"
};

static AnimationSet _gActor401300Animation259F0 = {
    _gActor401300Animation259F0Records,
    _gActor401300Animation259F0Indices,
    { NULL, _gActor401300Animation259F0Bank1, NULL, NULL, _gActor401300Animation259F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation26204Bank1[14] = {
#include "assets/actor_401300_animation_26204_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation26204Bank4[207] = {
#include "assets/actor_401300_animation_26204_bank4.inc"
};

static AnimationRecord _gActor401300Animation26204Records[248] = {
#include "assets/actor_401300_animation_26204_records.inc"
};

static u16 _gActor401300Animation26204Indices[20] = {
#include "assets/actor_401300_animation_26204_indices.inc"
};

static AnimationSet _gActor401300Animation26204 = {
    _gActor401300Animation26204Records,
    _gActor401300Animation26204Indices,
    { NULL, _gActor401300Animation26204Bank1, NULL, NULL, _gActor401300Animation26204Bank4, NULL, NULL, NULL },
};

s8 D_actor_401300_8015804C[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 3, 3, 5, 0, 0, 0, 5, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_401300_80158838[46] = { NULL, NULL, &_gActor401300Animation17714, &_gActor401300Animation21618, NULL, NULL, NULL, NULL, &_gActor401300Animation1C4CC, &_gActor401300Animation1D904, &_gActor401300Animation1F110, &_gActor401300Animation1F4C4, &_gActor401300Animation1AD34, &_gActor401300Animation1A750, &_gActor401300Animation1ED6C, &_gActor401300Animation1B788, NULL, &_gActor401300Animation1DE54, &_gActor401300Animation1A170, &_gActor401300Animation1FF3C, NULL, NULL, &_gActor401300Animation1CD3C, &_gActor401300Animation1F4C4, &_gActor401300Animation1AD34, &_gActor401300Animation186D8, &_gActor401300Animation19700, &_gActor401300Animation21DE0, &_gActor401300Animation2231C, &_gActor401300Animation22A58, &_gActor401300Animation2303C, &_gActor401300Animation23444, &_gActor401300Animation23A98, &_gActor401300Animation24200, &_gActor401300Animation1D0CC, &_gActor401300Animation1F848, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

AnimationSet* D_actor_401300_801588F0[9] = {
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor401300Animation24A48,
    &_gActor401300Animation251E4,
    &_gActor401300Animation259F0,
    &_gActor401300Animation26204,
    NULL,
};

AnimationPlayRequest D_actor_401300_80158914 = { { .sets = D_actor_401300_801588F0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

SVECTOR D_actor_401300_80158928[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

Actor401300MessageEntry D_actor_401300_80158988[8] = {
    { 2015, { .call5 = func_actor_401300_8014148C } },
    { ACTOR_MESSAGE_PLAY_ANIMATION, { .call2 = func_actor_401300_80141494 } },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .call4 = actorMsgSetVisibility } },
    { ACTOR_MESSAGE_IS_PRESENT, { .call0 = actorMsgIsPresent } },
    { ACTOR_MESSAGE_PLACE, { .call3 = func_actor_401300_80141614 } },
    { 2014, { .call0 = actorMsgReleaseHold } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_401300_80132554 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

ActorHeightClamp D_actor_401300_801589C8[3] = {
    { 1, 3, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 5, 29, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

SVECTOR D_actor_401300_801589F8[2] = {
    { 937, 44, 712, 0 },
    { 29, -106, 193, 0 },
};

SVECTOR D_actor_401300_80158A08[2] = {
    { 940, -28, -709, 0 },
    { 30, 130, -190, 0 },
};

TaskDesc D_actor_401300_80158A18 = { { { TASK_BODY_TMD, 96 } }, func_actor_401300_80141F2C, { .model = &_gActor401300HornedStrangerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

static s32             func_actor_401300_80132910(Task* arg0, WorldCollisionContact* recs, s16 count);
static void            func_actor_401300_80132BE4(GameLocationKey* session, GfxCoord* coord);
static __inline__ s32  Actor401300_HasHeightClamp(GameLocationKey* session);
static s32             func_actor_401300_80132C78(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static void            func_actor_401300_80133254(Task* arg0);
static void            func_actor_401300_80133324(Task* arg0);
static s32             func_actor_401300_8013346C(Actor401300Work* work);
static void            func_actor_401300_80133834(Task* arg0, s16 arg1);
static __inline__ void Actor401300_SpawnEff(s32 id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z);
static __inline__ void Actor401300_SpawnEffZero(s32 id, GfxCoord* coord, s32 flags);
static __inline__ void Actor401300_SpawnEffVar(s32* id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z);
static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GfxCoord* coord, s32 flags);
static __inline__ s32  Actor401300_InRange(Task* arg0);
static __inline__ void Actor401300_ResetAnim(Task* arg0);
static __inline__ void Actor401300_ResetBlendAnim(Task* arg0);
static __inline__ void Actor401300_TickAnim(Task* arg0);
static void            func_actor_401300_80133A3C(Task* arg0);
static __inline__ void Actor401300_BindMatrices(Task* actor);
static __inline__ void Actor401300_InitPose(GfxCoord* coord, Actor401300Work* work);
static void            func_actor_401300_80134454(Enemy* enemy, Task* actor);
static void            func_actor_401300_80134BA4(Task* arg0, s16 arg1, s32 arg2);
static void            func_actor_401300_80134F90(Task* arg0);
static void            func_actor_401300_80135DDC(Task* arg0);
static void            func_actor_401300_80135FC4(Task* arg0);
static void            func_actor_401300_80136238(Task* arg0);
static void            func_actor_401300_801365F8(Task* arg0);
static __inline__ void Actor401300_MoveBy(GfxCoord* coord, s16 amount);
static __inline__ s32  Actor401300_Abs(s32 x);
static void            func_actor_401300_80136CE8(Task* arg0);
static void            func_actor_401300_801376E4(Task* arg0);
static void            func_actor_401300_80137D78(Task* arg0);
static void            func_actor_401300_80138160(Task* arg0);
static void            func_actor_401300_80138800(Task* arg0);
static void            func_actor_401300_80138B24(Task* arg0);
static void            func_actor_401300_80138CF8(Task* arg0);
static void            func_actor_401300_80138FCC(Task* arg0);
static __inline__ void Actor401300_RescaleYawXZ(GfxCoord* coord, s32 xz, s16 y);
static void            func_actor_401300_80139134(Task* arg0);
static void            func_actor_401300_80139520(Task* arg0);
static void            func_actor_401300_801397F8(Task* arg0);
static void            func_actor_401300_80139AB0(Task* arg0);
static void            func_actor_401300_8013A208(Task* arg0);
static void            func_actor_401300_8013A5C0(Task* arg0);
static void            func_actor_401300_8013AAE8(Task* arg0);
static void            func_actor_401300_8013AE48(Task* arg0);
static void            func_actor_401300_8013B6E8(Task* arg0);
static void            func_actor_401300_8013BB30(Task* arg0);
static void            func_actor_401300_8013CBAC(Task* arg0);
static void            func_actor_401300_8013D2AC(Task* arg0);
static void            func_actor_401300_8013D6C4(Task* arg0);
static __inline__ void Actor401300_ResetActorYaw(Task* actor);
static __inline__ s32  Actor401300_Yaw(GfxCoord* coord);
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GfxCoord* coord, s16 amount);
static void            func_actor_401300_8013DADC(Task* arg0);
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GfxCoord* coord, s16 amount);
static void            func_actor_401300_8013E930(Task* arg0);
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale);
static void            func_actor_401300_8013F628(Task* arg0);
static void            func_actor_401300_80140300(Task* arg0);
static void            func_actor_401300_8014046C(Task* arg0);
static __inline__ s32  Actor401300_InRangeFlag(Task* arg0);
static __inline__ s32  Actor401300_HasRec10000(WorldCollisionContact* recs);
static __inline__ void Actor401300_SnapPlayerHeight(Task* actor);
static void            func_actor_401300_801405DC(Enemy* enemy, Task* actor);
static s32             func_actor_401300_801417F0(Task* arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

s32 func_actor_401300_80132554(Task* arg0, s32 arg1, Actor401300Event* arg2)
{
    Actor401300Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;

    work->field_CA8[0] = arg2->b[0];
    work->field_CA8[1] = arg2->b[1];
    work->field_CA8[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x17;
            return 1;
        }
    } else if (arg2->w[0] == 0xB05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
            case 0xC:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                    work->field_0 = 6;
                    enemy->hp     = D_actor_401300_80141FA0.hpMax;
                    (Gp_IncStateF0Ref)(0);
                }
                return 1;
        }
    } else if (arg2->w[0] == 0x1D05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

#include "../../shared/player_detection_reach.inc.c"

/// Pushes the root coordinate by a quarter of each kind 0x10000 / 0x30000 record's
/// offset (skipping 0x3000D), walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update keeps `count`'s sign extension in the loop,
/// as in `Actor01900_Fn03FF8`.
static s32 func_actor_401300_80132910(Task* arg0, WorldCollisionContact* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    head                                    = SCRATCH_STACK_CURSOR(ActorPushScratch);
    blk                                     = head - 1;
    SCRATCH_STACK_CURSOR(ActorPushScratch)  = blk;
    s                                       = blk;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
    s->pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        if ((s->kind == 0x10000 || s->kind == 0x30000) && recs[s->i].key.value != 0x3000D) {
            if (s->kind == 0x10000) {
                s->hit = 1;
            }
            worldCollisionCalcContactViewOffset(&s->pos, &recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x140) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x140);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPushScratch);
    return s->hit;
}

static void func_actor_401300_80132BE4(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

static __inline__ s32 Actor401300_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

static s32 func_actor_401300_80132C78(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorStepDelta* head;
    ActorStepDelta* s;
    ActorStepDelta* blk;
    s16             vy;
    SVECTOR*        step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    head                                 = SCRATCH_STACK_CURSOR(ActorStepDelta);
    blk                                  = head - 1;
    SCRATCH_STACK_CURSOR(ActorStepDelta) = blk;
    s                                    = blk;
    s->moved                             = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.word >> 16;
        s->step.vy = s->delta.vy.word >> 16;
        s->step.vz = s->delta.vz.word >> 16;
        if (Actor401300_HasHeightClamp(&gGameSession->location.loc)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x15E) {
                s->step.vy = (vy <= 0) ? -0x15E : 0x15E;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xAF) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xAF);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.word & 0xFFFF) {
            if (s->delta.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.word & 0xFFFF) {
            if (s->delta.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401300_HasHeightClamp(&gGameSession->location.loc)) {
        func_actor_401300_80132BE4(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.word != 0 || s->delta.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorStepDelta);
    return s->moved;
}

#include "../../shared/player_detection_sight.inc.c"

static void func_actor_401300_80133254(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;

    if (work->field_8A0 != work->field_8A2) {
        for (i = 1; i < 0x13; i++) {
            work->rig.slots[i].rate = work->field_8A6;
            if (i < 7) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->field_8A2, 0,
                                           D_actor_401300_8015804C[work->field_8A0][work->field_8A2]);
            } else if (i >= 9) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->field_8A2, 0,
                                           D_actor_401300_8015804C[work->field_8A0][work->field_8A2]);
            }
        }
        work->field_8A0 = work->field_8A2;
    }
}

static void func_actor_401300_80133324(Task* arg0)
{
    AnimationPose        pose;
    AnimationPose        blendPose;
    s16                  weight;
    s16                  i;
    Actor401300AnimWork* work;

    work   = (Actor401300AnimWork*)arg0->work;
    weight = work->field_8B0;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->field_8AE;
            work->rig.slots[i].rate   = (work->field_8A6 - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                animationTickSlotPose(&work->rig.anim, i, &pose, 0);
                animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
                Gp_AnimWritePoseCopy(&work->rig.anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->rig.slots[i].rate = (work->field_8A6 - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Returns the sound to play when the current animation (`field_8A2`) reaches
/// one of its cue frames, once per frame reached; `field_8BC` holds the last cue
/// frame seen. The frame is re-read at every use: caching it in a local moves
/// CSE's choice of register for the repeat-frame store.
static s32 func_actor_401300_8013346C(Actor401300Work* work)
{
    switch ((s16)(work->field_8A2 - 2)) {
        case 1:
            if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x15) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 0:
            if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x1A) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 23:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 24:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 10:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 32:
            if ((work->field_5E & 0x3FF) == 0x4) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 9:
            if ((work->field_5E & 0x3FF) == 0x5) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 7:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0006;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 30:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000A;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 31:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000B;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 25:
            if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 26:
            if ((work->field_5E & 0x3FF) == 0x12) {
                if (work->field_8BC != 0xE) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 27:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 28:
            if ((work->field_5E & 0x3FF) == 0x9) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0012;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
    }
    return 0;
}

static void func_actor_401300_80133834(Task* arg0, s16 arg1)
{
    SVECTOR* sc;

    sc     = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    sc->vx = D_actor_401300_801589F8[1].vx +
             ((D_actor_401300_801589F8[0].vx - D_actor_401300_801589F8[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_801589F8[1].vy +
             ((D_actor_401300_801589F8[0].vy - D_actor_401300_801589F8[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_801589F8[1].vz +
             ((D_actor_401300_801589F8[0].vz - D_actor_401300_801589F8[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->extra.tmd->coords[7].coord);
    sc->vx = D_actor_401300_80158A08[1].vx +
             ((D_actor_401300_80158A08[0].vx - D_actor_401300_80158A08[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_80158A08[1].vy +
             ((D_actor_401300_80158A08[0].vy - D_actor_401300_80158A08[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_80158A08[1].vz +
             ((D_actor_401300_80158A08[0].vz - D_actor_401300_80158A08[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->extra.tmd->coords[8].coord);
    arg0->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(8);
    arg0->extra.tmd->coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns effect `id` on `coord` at the offset (`x`, `y`, `z`).
static __inline__ void Actor401300_SpawnEff(s32 id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZero(s32 id, GfxCoord* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(id, coord, flags, &pos);
}

/// `Actor401300_SpawnEff` for an effect id held in a global. Taking the
/// global's address rather than its value is a matching requirement: the `lui`
/// is then evaluated with the arguments and the load itself after them, which
/// is the order the scheduler needs.
static __inline__ void Actor401300_SpawnEffVar(s32* id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GfxCoord* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

/// 1 when coordinate 1's view-space Z is in [-299, 2300): the body of
/// `func_actor_401300_801417F0`, with `actorTransformToView` written
/// out so `outp` is initialised after `svp`. The `if` that re-tests `ret` keeps
/// jump from folding the result into a bare `sltiu`.
static __inline__ s32 Actor401300_InRange(Task* arg0)
{
    SVECTOR   out;
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp;
    GfxCoord* view;
    VECTOR*   vecp;
    s32*      flagp;
    SVECTOR*  outp;
    GfxCoord* p;
    s32       ret;

    memset(&out, 0, 8);
    p     = &arg0->extra.tmd->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->parent != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->parent;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    ret = (u16)(out.vz + 0x12B) < 0xA27;
    if (ret != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

static __inline__ void Actor401300_ResetAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->field_8A6;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->rig.anim, i, work->field_8A2, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->rig.anim, i, work->field_8A2, i - 2, i);
        }
    }
    work->field_8A0 = work->field_8A2;
}

static __inline__ void Actor401300_ResetBlendAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work            = (Actor401300AnimWork*)arg0->work;
    work->field_8AE = 0x30;
    work->field_8B0 = 0x800;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->field_8AE;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->blend.anim, i, work->field_8AC, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->blend.anim, i, work->field_8AC, i - 2, i);
        }
    }
}

static __inline__ void Actor401300_TickAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->field_8A6;
        if (i < 7) {
            animationTickSlot(&work->rig.anim, i);
        } else if (i >= 9) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Per-frame animation and effect update: restarts or ticks the animation
/// slots, eases the yaw of coordinates 5/2 and the blend weight, then spawns
/// the current animation's effects and plays its cue sound.
static void func_actor_401300_80133A3C(Task* arg0)
{
    s32              i;
    s32              snd;
    s16              yaw;
    s32              inRange;
    Actor401300Work* work;
    Enemy*           enemy;

    /* Set here so CSE keeps `inRange` distinct from the helper's result. */
    inRange = 0;
    work    = arg0->work;
    enemy   = arg0->spawnArg2.pointer;
    if (work->field_89C == 1) {
        func_actor_401300_80133254(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    } else if (work->field_89C == 2) {
        Actor401300_ResetAnim(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    }
    if (work->field_8AA == 2) {
        Actor401300_ResetBlendAnim(arg0);
        work->field_8AA = 3;
    }
    work->field_8A4++;
    if (work->field_89E == 0) {
        Actor401300_TickAnim(arg0);
    } else {
        func_actor_401300_80133324(arg0);
        if (((Actor401300AnimWork*)work)->blend.slots[1].flags & ANIMATION_SLOT_SETTLED) {
            work->field_89E = 0;
        }
    }
    if (work->field_8B2 > work->field_8B4) {
        if (work->field_8B2 - work->field_8B4 > 0x100) {
            work->field_8B4 += 0x100;
        } else {
            work->field_8B4 = work->field_8B2;
        }
    } else if (-(work->field_8B2 - work->field_8B4) > 0x100) {
        work->field_8B4 -= 0x100;
    } else {
        work->field_8B4 = work->field_8B2;
    }
    if (work->field_8B4 != 0) {
        yaw = work->field_8B4;
        if (work->field_8B4 > 0x400) {
            yaw = 0x400;
        }
        if (work->field_8B4 < -0x400) {
            yaw = -0x400;
        }
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[5], (yaw * 2) / 3);
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[2], yaw / 2);
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->field_8B8 != work->field_8B6) {
        if (work->field_8B6 < work->field_8B8) {
            work->field_8B8 -= work->field_8BA;
            if (work->field_8B8 < work->field_8B6) {
                work->field_8B8 = work->field_8B6;
            }
        } else {
            work->field_8B8 += work->field_8BA;
            if (work->field_8B6 < work->field_8B8) {
                work->field_8B8 = work->field_8B6;
            }
        }
    }
    func_actor_401300_80133834(arg0, work->field_8B8);
    snd     = func_actor_401300_8013346C(work);
    inRange = Actor401300_InRange(arg0);
    if (inRange == 1) {
        if (work->field_8A2 == 2) {
            if (gDisplayState.animFrame % 6 == 0) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 3) {
            if ((gDisplayState.animFrame & 1) == inRange) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[18], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (!(gDisplayState.animFrame & 1)) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[15], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 9 || work->field_8A2 == 25 || work->field_8A2 == 26) {
            if (gDisplayState.animFrame % 5 == 0) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        }
        if (snd != 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            switch (snd) {
                case 0x400D0001:
                case 0x400D0003:
                    Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[18], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0006;
                    break;
                case 0x400D0002:
                case 0x400D0004:
                    Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[15], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0007;
                    break;
                case 0x400D0005:
                case 0x400D000B:
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    snd = 0x551D0005;
                    break;
            }
        }
    }
    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
        switch (snd) {
            case 0x400D0001:
            case 0x400D0003:
                Actor401300_SpawnEff(0x60054, &arg0->extra.tmd->coords[18], 0x800022C0, 0, 0x15E, -100);
                break;
            case 0x400D0002:
            case 0x400D0004:
                Actor401300_SpawnEff(0x60054, &arg0->extra.tmd->coords[15], 0x800022F0, 0, 0x15E, -100);
                break;
            case 0x400D0005:
            case 0x400D000B:
                Actor401300_SpawnEffZero(0x60054, &arg0->extra.tmd->coords[1], 0x80004800);
                Actor401300_SpawnEffZero(0x60054, &arg0->extra.tmd->coords[1], 0x80004800);
                break;
        }
    }
    if (snd != 0) {
        i = snd | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        SndEvt_EnqueueType6(i, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401300_BindMatrices(Task* actor)
{
    Actor401300Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_C28;
    obj->colorMtx = &work->field_C48;
}

/// Rebuilds the root coordinate's scaled Y rotation and seeds the combat
/// defaults while the rotation scratch block is still held.
static __inline__ void Actor401300_InitPose(GfxCoord* coord, Actor401300Work* work)
{
    ActorScaleRotScratch* top;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    top                                        = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = top - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;
    ang                                        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle                                 = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0] = (u16)(top - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
    work->field_D78      = 0;
    work->field_C8A      = 0;
    work->field_CAC      = D_actor_401300_80158914;
    work->field_8B8      = 0x100;
    work->field_8B6      = 0x170;
    work->field_8BA      = 0x20;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

static void func_actor_401300_80134454(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    Actor401300Work*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    GfxRotationWords*   mw;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xD7C, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    if ((actor->spawnArg1.value >> 16) != 2) {
        (Gp_IncStateF0Ref)(0);
    }
    actor->exitCallback = func_actor_401300_80141758;
    Actor401300_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401300_80141FA0.hpMax;
    enemy->param                  = &D_actor_401300_80141FA0;
    enemy->recs                   = work->field_990;
    animationBindContext(&((Actor401300AnimWork*)work)->rig.anim, D_actor_401300_80158838, obj,
                         ((Actor401300AnimWork*)work)->rig.poses, ((Actor401300AnimWork*)work)->rig.slots);
    animationBindContext(&((Actor401300AnimWork*)work)->blend.anim, D_actor_401300_80158838, obj,
                         ((Actor401300AnimWork*)work)->blend.poses, ((Actor401300AnimWork*)work)->blend.slots);
    work->field_89C = 2;
    work->field_89E = 0;
    work->field_8A2 = 2;
    work->field_8B4 = 0;
    work->field_8B2 = 0;
    work->field_8A8 = 0x10;
    work->field_8A6 = 0x10;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->field_8A8++;
    } else {
        work->field_8A8--;
    }
    func_actor_401300_80133A3C(actor);

    work->field_920.parent       = &gGfxViewCoord;
    mw                           = (GfxRotationWords*)&work->field_920.coord;
    mw->m00M01                   = ONE;
    mw->m02M10                   = 0;
    mw->m11M12                   = ONE;
    mw->m20M21                   = 0;
    mw->m22                      = ONE;
    work->field_920.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
    work->field_920.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - 0x15E;
    work->field_920.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
    work->field_920.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->field_920);

    work->field_AB0.context.contacts = work->field_AD0;
    work->field_AB0.coord            = &work->field_920;
    work->field_AB0.pos.vx           = 0;
    work->field_AB0.pos.vy           = 0;
    work->field_AB0.pos.vz           = 0;
    work->field_AB0.key              = 0x3000D;
    work->field_AB0.radius           = 0x15E;
    work->field_AB0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_AB0);
    work->field_C88        = 0;
    work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->field_AB0.context.contacts, 0xC, 0);

    body                   = &work->field_970;
    body->context.contacts = work->field_990;
    body->key              = 0x30000;
    body->coord            = &gGfxViewCoord;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->radius           = 0x280;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, 0xC, 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->field_BF0;
    head->coord            = &actor->extra.tmd->coords[3];
    head->context.contacts = work->sensorContacts;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x200;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->context.contacts, 1, 0);

    work->field_16     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = D_actor_401300_80158988;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_910.coord      = &actor->extra.tmd->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    switch ((u8)(actor->spawnArg1.value >> 16)) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            enemy->hp     = -999;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        case 0x20:
            work->field_2 = -1;
            work->field_0 = 0x27;
            enemy->hp     = 0x50;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->field_CA0 = D_actor_401300_80141FB0[0].vx;
            work->field_CA2 = D_actor_401300_80141FB0[0].vy;
            work->field_CA4 = D_actor_401300_80141FB0[0].vz;
            break;
        case 1:
            work->field_CA0 = D_actor_401300_80141FB0[2].vx;
            work->field_CA2 = D_actor_401300_80141FB0[2].vy;
            work->field_CA4 = D_actor_401300_80141FB0[2].vz;
            break;
        case 0:
        default:
            work->field_CA0 = D_actor_401300_80141FB0[1].vx;
            work->field_CA2 = D_actor_401300_80141FB0[1].vy;
            work->field_CA4 = D_actor_401300_80141FB0[1].vz;
            break;
    }

    Actor401300_InitPose(actor->extra.tmd->coords, work);
    actor->state++;
}

static void func_actor_401300_80134BA4(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401300Work* work;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401300_80158928[0];
                break;
            case 1:
                *sc = D_actor_401300_80158928[1];
                break;
            case 2:
                *sc = D_actor_401300_80158928[2];
                break;
            case 3:
                *sc = D_actor_401300_80158928[3];
                break;
            default:
                *sc = D_actor_401300_80158928[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401300_80158928[5];
                break;
            case 1:
                *sc = D_actor_401300_80158928[6];
                break;
            default:
                *sc = D_actor_401300_80158928[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[8];
        } else {
            *sc = D_actor_401300_80158928[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[10];
        } else {
            *sc = D_actor_401300_80158928[11];
        }
    }
    work->field_910.coord      = &arg0->extra.tmd->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], sc, &work->field_910);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_401300_80134F90(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    Actor401300Work* work;
    Enemy*           enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GfxCoord*        coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s16              state;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0 && (work->field_0 != 8 || work->field_8A2 != 0x20)) {
        head  = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s     = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, work->field_990);
        if (s->id == 0) {
            s->id = actorFindHit(&s->hitPos, work->field_AD0);
        }
        if (s->id != 0) {
            work->field_D1C        = 0;
            work->field_D1E        = 0;
            work->field_970.radius = 0x280;
            if (s->id & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            func_actor_401300_80134BA4(arg0, s->yaw, s->id);
            work->field_8B4 = 0;
            work->field_8B2 = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x11 && state != 0xF && state != 0x10 &&
                state != 0x27 && state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_89E == 1) {
                    gte_lddp(-5);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-10);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0]  += s->dir.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->dir.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->dir.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx        = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x25 && state != 0x26 && state != 0xF && state != 0x10 && state != 0x27 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            effect = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_D20 == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400D0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400D0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_C88 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x27 && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (work->field_0 == 4) {
                        work->field_2 = -1;
                    } else if (work->field_0 != 0xF && work->field_0 != 0x10) {
                        if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0x27 || work->field_0 == 4 || work->field_0 == 0x11) {
                            if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                                work->field_89E = 1;
                                work->field_8AC = 0xB;
                            } else {
                                work->field_89E = 1;
                                work->field_8AC = 0x22;
                            }
                            work->field_8AA = 2;
                        } else if (s->crit == 1) {
                            if (work->field_0 != 4 && work->field_0 != 0x27 && work->field_0 != 0x11) {
                                work->field_0 = 5;
                            }
                        } else {
                            work->field_8AC = 0xD;
                            work->field_89E = 1;
                            work->field_8AA = 2;
                        }
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 0x27 || state == 4) {
                        work->field_0 = 4;
                        work->field_2 = -1;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 5;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x27 && state != 4 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                    break;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x27 || work->field_0 == 0x11) {
                        if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                            work->field_89E = 1;
                            work->field_8AC = 0xB;
                        } else if (work->field_8A2 == 0x22 || work->field_8A2 == 0x18 || work->field_8A2 == 0x16 || work->field_8A2 == 0xC) {
                            work->field_89E = 1;
                            work->field_8AC = 0x22;
                        } else {
                            work->field_89E = 1;
                            work->field_8AC = 0xD;
                        }
                    } else {
                        work->field_89E = 1;
                        work->field_8AC = 0xD;
                    }
                    work->field_8AA = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4) {
                    state = work->field_8A2;
                    if (state == 2 || state == 3 || state == 0x1B || state == 0x1C || state == 0x1D) {
                        work->field_0 = 0x28;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                            work->field_2 = -1;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                            work->field_2 = -1;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                }
            } else {
                state = work->field_0;
                if (state != 0x13 && state != 0x14 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                    work->field_0 = 0x13;
                }
            }
            if (enemy->hp <= 0) {
                enemy->hp       = 0;
                work->field_C8A = 1;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}

static void func_actor_401300_80135DDC(Task* arg0)
{
    Actor401300Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->field_8A2 == 11 || work->field_8A2 == 23) {
            work->field_8A2 = 0x17;
        } else if (work->field_8A2 == 12 || work->field_8A2 == 34 || work->field_8A2 == 24) {
            work->field_8A2 = 0x18;
        }
        if ((u16)(work->field_8A2 - 0x17) >= 2) {
            work->field_8A2 = 0x17;
        }
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_8A2 == 0x17 && (work->field_5E & 0x3FF) >= 6) &&
                 !(work->field_8A2 == 0x18 && (work->field_5E & 0x3FF) >= 9));
        work->field_8A6 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_8A6                       = work->field_8A6 / 2;
    if (work->field_8A6 == 1) {
        work->field_8A6 = -0x10;
    }
    if (work->field_8A6 == -1) {
        work->field_8A6 = 0x10;
    }
    func_actor_401300_80133A3C(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->field_8A6       = 0x10;
        if ((arg0->spawnArg1.value >> 16) == 0x20) {
            work->field_0 = 0x27;
        } else {
            work->field_0 = 0x11;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

static void func_actor_401300_80135FC4(Task* arg0)
{
    Actor401300Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    s16              i     = 0;
    u16              r;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x17;
        work->field_8B6        = 0x40;
        work->field_8B8        = 0x40;
        work->field_8BA        = 0x20;
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_6C & 0x100) && ++i < 0xFF);
        work->field_8A6 = 0x20;
        return;
    }
    if (++work->field_6 == 0) {
        work->field_89C = 2;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        r               = (gRandomLcgState >> 16) % 3;
        switch (r) {
            case 0:
                work->field_8A6 = 0x20;
                break;
            case 1:
                work->field_8A6 = 0x30;
                break;
            case 2:
            default:
                work->field_8A6 = 0x40;
                break;
        }
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_8A6 = 0x10;
    } else if (work->field_6 > 0) {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_8A6                       = work->field_8A6 / 2;
        if (work->field_8A6 == 1) {
            work->field_8A6 = -0x10;
        }
        if (work->field_8A6 == -1) {
            work->field_8A6 = 0x10;
        }
        func_actor_401300_80133A3C(arg0);
    } else if (work->field_89E == 1 || !(work->field_6C & 0x100)) {
        work->field_8A6 = 0x10;
        func_actor_401300_80133A3C(arg0);
    }
    if (work->field_6 >= 7) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_6   = -((gRandomLcgState >> 16) & 0xFF);
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

static void func_actor_401300_80136238(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 9;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->field_970.radius = 0x280;
        Gp_ArmStateF0(1);
        work->field_8B6 = 0x200;
        work->field_8BA = 0x20;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x200) {
            work->field_8BA = 0x80;
            work->field_8B6 = 0x190;
        } else {
            work->field_8B6 = 0x200;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        if (detectSightBlocked(arg0) == 1 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8B2 = aim->turn;
    if (aim->turn > 0x10) {
        aim->turn = 0x10;
    }
    if (aim->turn < -0x10) {
        aim->turn = -0x10;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_801365F8(Task* arg0)
{
    Actor401300Work*           work;
    GameActor*                 player;
    PlayerStatus*              config;
    TmdObject*                 obj;
    GfxCoord*                  coord;
    GfxCoord*                  c1;
    GfxCoord*                  c2;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* sc;
    SVECTOR*                   delta;
    s32                        angle;
    s32                        dist;
    s32                        dx;
    s32                        dy;
    s32                        dz;
    s32                        mask;

    config = &gPlayerStatus;
    work   = arg0->work;
    player = (GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    mask   = 0xF0;
    if (((arg0->spawnArg1.value >> 16) & mask) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_89E        = 0;
        work->field_8A2        = 3;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_8A6        = work->field_8A8;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0x40;
        work->field_8BA = 0x10;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    work->field_6++;
    head                                            = SCRATCH_STACK_CURSOR(Actor401300PursuitScratch);
    delta                                           = &head[-1].delta;
    c1                                              = arg0->extra.tmd->coords;
    head[-1].delta.vx                               = gPlayerStatus.coordMtx->t[0] - c1->coord.t[0];
    delta->vy                                       = gPlayerStatus.coordMtx->t[1] - c1->coord.t[1];
    delta->vz                                       = gPlayerStatus.coordMtx->t[2] - c1->coord.t[2];
    SCRATCH_STACK_CURSOR(Actor401300PursuitScratch) = head - 1;
    sc                                              = head - 1;
    arg0->extra.tmd->coords->composeStamp           = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    c2              = arg0->extra.tmd->coords;
    angle           = ratan2(head[-1].delta.vx, delta->vz);
    sc->angle       = actorNormalizeYaw(angle - ratan2(-c2->coord.m[2][0], c2->coord.m[2][2]));
    work->field_8B2 = sc->angle;
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f)) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f);
    }
    if (sc->angle > 0x30) {
        sc->angle = 0x30;
    } else if (sc->angle < -0x30) {
        sc->angle = -0x30;
    } else {
        sc->dx = dx = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        sc->dy = dy = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        sc->dz = dz = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dist        = SquareRoot0(dx * dx + dy * dy + dz * dz);
        sc->dist    = dist;
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED && work->field_6 >= 0x28) {
            if (dist > 4000) {
                work->field_0 = 0x21;
            } else if (dist > 2000) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) < 5) {
                    work->field_0 = 0x21;
                } else {
                    work->field_0 = 0x22;
                }
            } else if (dist < 1000) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) < 7) {
                    work->field_0 = 0x1F;
                } else {
                    work->field_0 = 0x20;
                }
            }
        }
    }
    coord      = arg0->extra.tmd->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, sc->angle, 1);

    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg0->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(Actor401300PursuitScratch);
}

static __inline__ void Actor401300_MoveBy(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* v;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        v                             = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(v);
            gte_gpf12();
            gte_stsv(v);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static __inline__ s32 Actor401300_Abs(s32 x)
{
    if (x < 0) {
        x = -x;
    }
    return x;
}

static void func_actor_401300_80136CE8(Task* arg0)
{
    Actor401300Work*           work;
    Enemy*                     enemy;
    TmdObject*                 obj;
    GfxCoord*                  coord;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* blk;
    Actor401300PursuitScratch* s;
    s32                        z;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_89E        = 0;
        work->field_8A2        = 3;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_8A6        = work->field_8A8;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x40;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8BA = 0x10;
        z               = arg0->extra.tmd->coords->coord.t[2];
        if (z > 0x1B58) {
            work->home.vx  = -0xB54;
            work->home.vz  = 0x2198;
            work->home.vy  = 0;
            work->home.pad = 0x400;
        } else if (z > 0x1068) {
            work->home.vx  = 0;
            work->home.vy  = 0;
            work->home.vz  = 0x189C;
            work->home.pad = 0;
        } else if (z > 0x384) {
            work->home.vx  = 0x12C0;
            work->home.vy  = 0;
            work->home.vz  = 0x1AF4;
            work->home.pad = 0;
        } else if (z > -0x898) {
            work->home.vx  = 0x12C0;
            work->home.vz  = -0x1388;
            work->home.vy  = 0;
            work->home.pad = 0x800;
        } else {
            work->home.vx  = -0xB4;
            work->home.vy  = 0;
            work->home.vz  = -0x960;
            work->home.pad = 0;
        }
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    head = SCRATCH_STACK_CURSOR(Actor401300PursuitScratch);
    blk  = head - 1;
    work->field_6++;
    SCRATCH_STACK_CURSOR(Actor401300PursuitScratch) = blk;
    func_actor_401300_80133A3C(arg0);
    s = blk;
    switch (work->field_8A2) {
        case 3:
            blk->delta.vx = work->home.vx - arg0->extra.tmd->coords->coord.t[0];
            head[-1].dx   = blk->delta.vx;
            blk->delta.vy = work->home.vy - arg0->extra.tmd->coords->coord.t[1];
            blk->dy       = blk->delta.vy;
            blk->delta.vz = work->home.vz - arg0->extra.tmd->coords->coord.t[2];
            blk->dz       = blk->delta.vz;
            blk->dist     = SquareRoot0(head[-1].dx * head[-1].dx + blk->dy * blk->dy + blk->dz * blk->dz);
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) != 1) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            coord           = arg0->extra.tmd->coords;
            s->angle        = actorNormalizeYaw(ratan2(head[-1].delta.vx, head[-1].delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            work->field_8B2 = s->angle;
            if (s->angle > 0x30) {
                s->angle = 0x30;
            } else if (s->angle < -0x30) {
                s->angle = -0x30;
            } else if ((s->dist < 0x898 && Actor401300_Abs(actorNormalizeYaw(ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - work->home.pad)) < 0x200) || work->field_6 > 0xB4) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                SndEvt_EnqueueType6(SOUND_NEO_ARK_WOODLAND_STRANGER_WITHDRAW, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords), (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            s->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f)) != 0) {
                Actor401300_MoveBy(arg0->extra.tmd->coords, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f));
            }
            actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            break;
        case 0x20:
            s->angle = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
            actorMoveForward(arg0->extra.tmd->coords, 0x12C);
            actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            if (work->field_6C & 0x100) {
                work->field_0 = 0;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, enemy->hp, 0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor401300PursuitScratch);
}

static void func_actor_401300_801376E4(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* head;
    ActorChaseScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        s                                                         = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 3;
        work->field_89E        = 0;
        work->field_8B2        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing          = arg0->extra.tmd->coords;
        s->angle        = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C94 = s->angle;
        work->field_C96 = s->angle + (u16)s->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    s                                       = head - 1;
    func_actor_401300_80133A3C(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (work->field_C94 == work->field_C96) {
        if (work->field_D1C < 2 || overlayOutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C94 > work->field_C96) {
        work->field_C94 -= 0x89;
        if (work->field_C94 < work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    if (work->field_C94 < work->field_C96) {
        work->field_C94 += 0x89;
        if (work->field_C94 > work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->field_C94, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x28) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x28);
        }
    } else {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x14) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x14);
        }
    }
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) != 1) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_80137D78(Task* arg0)
{
    Actor401300Work*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x140;
        work->field_6          = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C9C == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C9C = 1;
            } else {
                work->field_C9C = -1;
            }
        }
        if (work->field_C9C == 1) {
            work->field_8A2 = 0x15;
            if (work->field_D1E == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->field_CA2 + angle;
            } else {
                aim->turn += work->field_CA2;
            }
            work->field_C9C = -1;
        } else {
            work->field_8A2 = 0x14;
            if (work->field_D1E == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->field_CA2;
            } else {
                aim->turn -= work->field_CA2;
            }
            work->field_C9C = 1;
        }
        work->field_89C = 1;
        work->field_8A6 = 0xC;
        work->field_89E = 0;
        func_actor_401300_80133A3C(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->field_C8C;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C9E = 0xDE;
        work->field_D1E++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 0) {
        gte_lddp(work->field_C9E);
        gte_ldsv(&work->field_C8C);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C9E >> 1);
        gte_ldsv(&work->field_C8C);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_80138160(Task* arg0)
{
    SVECTOR          pos;
    Actor401300Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    GameActor*       player;
    PlayerStatus*    config;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    config = &gPlayerStatus;
    if (work->field_4 != 0) {
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 1;
        work->field_8A6               = 0x10;
        work->field_8A2               = 4;
        func_actor_401300_80133A3C(arg0);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, actorPositionYaw(arg0, &pos, config), 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
        pos.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                                = 0;
        pos.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8B2                       = 0;
        work->field_8B4                       = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_D1E                       = 0;
        work->field_D20                       = 0;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &pos, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !overlayOutOfRange(&pos, 0x44C)) {
            work->field_CAC.source.sets = D_actor_401300_801588F0;
            work->field_D00             = 8;
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &work->field_CEC, 0) == 0) {
                work->field_0               = 0xC;
                work->field_D20             = 1;
                work->field_CAC.animationId = 1;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                work->field_CC0[2] = 0;
                work->field_CC0[1] = 0;
                work->field_CC0[0] = 0;
                work->field_CD0    = 7;
                work->field_CD2    = 1;
                work->field_D22    = 0;
            }
        }
    }
    if (work->field_8A2 == 4 && (work->field_6C & 0x100)) {
        work->field_0 = 7;
    }
    if ((work->field_5E & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!overlayOutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[0]                    += pos.vx;
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[2]                    += pos.vz;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static void func_actor_401300_80138800(Task* arg0)
{
    SVECTOR          dir;
    Actor401300Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->field_970.radius                  = 0x280;
        work->field_BF0.flags                  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags                  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags           = 0;
        work->field_89C                         = 1;
        work->field_8A6                         = 0x10;
        work->field_8A2                         = 5;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(player->extra.tmd->coords);
        work->field_CD4.vx = player->extra.tmd->coords->coord.t[0];
        work->field_CD4.vy = player->extra.tmd->coords->coord.t[1];
        work->field_CD4.vz = player->extra.tmd->coords->coord.t[2];
        pdir               = &dir;
        dir.vx             = (u16)arg0->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        dir.vy             = 0;
        dir.vz             = (u16)arg0->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + dir.vx;
        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + dir.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_CE4.vx                    = 0;
        work->field_CE4.vy                    = ratan2(dir.vx, dir.vz);
        work->field_CE4.vz                    = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &work->field_CD4, 0);
    }
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, 0);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, 0);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
    if (work->field_8A2 == 5 && (work->field_6C & 0x100)) {
        work->field_910.coord      = &arg0->extra.tmd->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[5], NULL, &work->field_910);
        work->field_0 = 0xD;
    }
}

static void func_actor_401300_80138B24(Task* arg0)
{
    Actor401300Work* work   = arg0->work;
    Enemy*           enemy  = arg0->spawnArg2.pointer;
    Task*            player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->field_4 != 0) {
        work->field_8A6 = 0x10;
        work->field_8A2 = 6;
        work->field_89C = 2;
        if ((s16)taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0) == 1) {
            ((GameActor*)player->work)->state = 0xA;
        }
        work->field_CAC.animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
        work->field_D22 = 0;
    }
    if (work->field_6C & 2) {
        work->field_910.coord      = &arg0->extra.tmd->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[5], NULL, &work->field_910);
        work->field_0 = 0xE;
    }
    work->field_898 = work->field_5E & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, 0);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, 0);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
}

static void func_actor_401300_80138CF8(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 1;
        work->field_8A2               = 0xA;
        work->field_89E               = 0;
        work->field_8A6               = 0x10;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->field_8A2 == 0xA && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x57) != 0) {
        actorMoveForward(arg0->extra.tmd->coords, -0x57);
    }
    func_actor_401300_80133A3C(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        if (work->field_8A2 == 0xA) {
            work->field_8A2 = 0xB;
            work->field_89C = 2;
            func_actor_401300_80133A3C(arg0);
        }
        if ((work->field_6C & 0x100) && work->field_8A2 == 0xB) {
            work->field_970.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            if (enemy->hp <= 0) {
                work->field_0 = 0x15;
            } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        }
    }
}

static void func_actor_401300_80138FCC(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 1;
        work->field_8A2               = 0xC;
        work->field_8A6               = 0x10;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, scaled by `xz` on X/Z
/// and `y` on Y. `actorRescaleYaw` with a separate Y scale.
static __inline__ void Actor401300_RescaleYawXZ(GfxCoord* coord, s32 xz, s16 y)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = xz;
    blk->scale.vy = y;
    blk->scale.vz = xz;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Collapse state: spawns effect 0x600A5 at the actor's view-space position on
/// frame 30, switches the light mode on 30/42, and from frame 26 squashes the
/// root coordinate's Y scale; state 0x24 follows after frame 64.
static void func_actor_401300_80139134(Task* arg0)
{
    Actor401300Work*  work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxRotationWords* w;
    SVECTOR           pos;
    s16               t;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj->flags                    = 0;
        work->field_BF0.flags         = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags         = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_6                 = 0;
        work->field_8A6               = 8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 <= 0x400) {
        switch (++work->field_6) {
            case 30:
                w         = (GfxRotationWords*)&work->field_8C0.coord;
                w->m00M01 = ONE;
                w->m02M10 = 0;
                w->m11M12 = ONE;
                w->m20M21 = 0;
                w->m22    = ONE;
                pos.vx    = 0;
                pos.vy    = 0;
                pos.vz    = 0;
                actorTransformToView(&arg0->extra.tmd->coords[2], &pos);
                work->field_8C0.parent       = &gGfxViewCoord;
                work->field_8C0.coord.t[0]   = pos.vx;
                work->field_8C0.coord.t[1]   = arg0->extra.tmd->coords->coord.t[1];
                work->field_8C0.coord.t[2]   = pos.vz;
                work->field_8C0.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&work->field_8C0);
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &work->field_8C0, 3, NULL);
                break;
            case 48:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 42:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 64:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->field_6;
        if (t >= 0x1A) {
            Actor401300_RescaleYawXZ(arg0->extra.tmd->coords, 0x1964, 0x1964 - (t - 0x14) * 16);
        }
        if (work->field_6 > 0x40 && work->field_D20 == 0) {
            work->field_0 = 0x24;
        }
    }
}

static void func_actor_401300_80139520(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj        = arg0->extra.tmd;
        enemy      = arg0->spawnArg2.pointer;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->field_6                 = 0;
        work->field_C68               = work->field_C48;
        work->field_8A2               = 0xE;
        work->field_89C               = 1;
        work->field_8A6               = work->field_8A8;
    }
    if (work->field_6 > 0x960) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6++;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_8A2 == 0xE && (work->field_6C & 2)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->field_8A2 = 0xF;
            work->field_89C = 1;
            func_actor_401300_80133A3C(arg0);
        }
    }
    if (work->field_8A2 == 0xF && (work->field_6C & 0x100)) {
        work->field_8A2 = 0xE;
        work->field_89C = 1;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_801397F8(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        D_actor_401300_80158838[16] = &gActor401300Animation20D98;
        work->field_8A2             = 0x10;
        work->field_89C             = 2;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B4               = 0;
        work->field_8A6               = 0x10;
        work->field_8B2               = 0;
        work->field_6                 = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51030008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_6 = 1;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 4 && work->field_8BC != (work->field_5E & 0x3FF)) {
        work->field_910.coord      = arg0->extra.tmd->coords + 1;
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->field_910);
    }
    work->field_8BC = work->field_5E & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy           = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

static void func_actor_401300_80139AB0(Task* arg0)
{
    Actor401300Work*  work;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* s;
    GfxCoord*         facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 2;
        work->field_8B6        = 0x60;
        work->field_8BA        = 8;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x60) {
            work->field_8B6 = 0x20;
        } else {
            work->field_8B6 = 0x60;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    s           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    s->delta.vx = work->field_C[work->field_16].x - arg0->extra.tmd->coords->coord.t[0];
    s->delta.vy = 0;
    s->delta.vz = work->field_C[work->field_16].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&s->delta, 0xA0)) {
        if (work->field_16 == 0) {
            work->field_16 = 1;
        } else {
            work->field_16 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    coord           = arg0->extra.tmd->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2 = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    if (work->field_89E == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0xA);
        }
    }
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (!overlayOutOfRange(&s->delta, 0x7D0)) {
        work->field_0 = 6;
    } else if (!overlayOutOfRange(&s->delta, 0xFA0)) {
        coord    = arg0->extra.tmd->coords;
        s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        if (ABS(s->angle) < 0x300) {
            work->field_0 = 6;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void func_actor_401300_8013A208(Task* arg0)
{
    Actor401300Work*  work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2.pointer;
        obj             = arg0->extra.tmd;
        work->field_8A2 = 0x12;
        work->field_89C = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B4               = 0;
        work->field_8A6               = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn            = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
    work->field_8B2 = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, work->field_C98) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_C98);
    }
    if (work->field_C98 > 0) {
        next            = work->field_C98 - 0xA;
        work->field_C98 = next;
        if ((s16)next < 0) {
            work->field_C98 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_6C & 0x100) || work->field_C98 == 0) {
        work->field_0 = 9;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void func_actor_401300_8013A5C0(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x16;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8B2 = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->field_8A2 == 2) {
        work->field_8A6 = 0x16;
        work->field_8A2 = 0x11;
        work->field_89C = 1;
        work->field_6   = 0;
        func_actor_401300_80133A3C(arg0);
    }
    if (aim->turn > 0x80) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_8A2 == 0x11) {
        work->field_6++;
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x10) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, -0x10);
        }
        if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
            func_actor_401300_80132910(arg0, work->field_990, 0xC);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->field_6 >= 0x13) {
            if (work->field_8B2 <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013AAE8(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->field_6C & 0x100) || work->field_6 >= 0xB) {
        work->field_0 = 0xB;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8B2 = aim->turn;
    if (aim->turn > 0x20) {
        aim->turn = 0x20;
    }
    if (aim->turn < -0x20) {
        aim->turn = -0x20;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013AE48(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 8;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_6   = 0;
        work->field_8B4 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->field_8B2 < aim->turn) {
        if (aim->turn - work->field_8B2 > 0x28) {
            work->field_8B2 += 0x28;
        } else {
            work->field_8B2 = aim->turn;
        }
    } else if (work->field_8B2 - aim->turn > 0x28) {
        work->field_8B2 -= 0x28;
    } else {
        work->field_8B2 = aim->turn;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40, 0);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80, 0);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80, 0);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80, 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100, 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
    } else {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
        if (aim->turn > 0x24) {
            aim->turn = 0x24;
        } else if (aim->turn < -0x24) {
            aim->turn = -0x24;
        }
        if (ABS(aim->turn) < 0x24) {
            work->field_0 = 7;
        }
        coord      = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013B6E8(Task* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    Enemy*           enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_970.radius        = 0x280;
        work->field_AB0.flags         = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        work->field_BF0.flags         = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8B2               = 0;
        work->field_6                 = 0U;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect1;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect1;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect2;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 8) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerBurstHead;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D && work->field_D20 == 0) {
        work->field_0 = 0x24;
    }
}

static void func_actor_401300_8013BB30(Task* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    Enemy*           enemy;
    u16              next;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_970.radius        = 0x280;
        work->field_AB0.flags         = (u16)(work->field_AB0.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        work->field_BF0.flags         = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8B2               = 0;
        work->field_6                 = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        work->field_8A2               = 2;
        work->field_89C               = 1;
        work->field_8A6               = 0x10;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_8A2) {
        case 2:
            if ((s16)next >= 0x10 && (work->field_6C & 2)) {
                work->field_8A2 = 0x23;
                work->field_89C = 2;
                work->field_8A6 = 0x10;
                work->field_89E = 0;
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 0xA);
            }
            ActorContact_PushContact(arg0->extra.tmd->coords, work->field_AD0, 0xC);
            if (work->field_6 == 3) {
                D_80114B34[5].data.model = &_gActor401300HornedStrangerBurstHead;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect2;
                actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
            }
            break;
        case 0x23:
            if (!(work->field_6C & 0x100)) {
                work->field_6 = 0;
            }
            switch (work->field_6) {
                case 3:
                    break;
                case 30:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    vec.vx = 0;
                    vec.vy = 0;
                    vec.vz = 0;
                    actorTransformToView(arg0->extra.tmd->coords + 2, &vec);
                    work->field_8C0.parent       = &gGfxViewCoord;
                    work->field_8C0.coord.t[0]   = vec.vx;
                    work->field_8C0.coord.t[1]   = arg0->extra.tmd->coords->coord.t[1];
                    work->field_8C0.coord.t[2]   = vec.vz;
                    work->field_8C0.composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(&work->field_8C0);
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, &work->field_8C0, 2, NULL);
                    break;
                case 48:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 42:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 64:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_0          = 0x24;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                Actor401300_RescaleYawXZ(arg0->extra.tmd->coords, 0x1964, 0x1964 - (cur - 0x14) * 16);
            }
            break;
    }
    func_actor_401300_80133A3C(arg0);
    actorResetYaw(arg0->extra.tmd->coords + 2);
    actorResetYaw(arg0->extra.tmd->coords + 3);
    actorResetYaw(arg0->extra.tmd->coords + 4);
    actorResetYaw(arg0->extra.tmd->coords + 5);
    actorResetYaw(arg0->extra.tmd->coords + 6);
    actorResetYaw(arg0->extra.tmd->coords + 7);
    actorResetYaw(arg0->extra.tmd->coords + 8);
    actorResetYaw(arg0->extra.tmd->coords + 9);
    actorResetYaw(arg0->extra.tmd->coords + 10);
    actorResetYaw(arg0->extra.tmd->coords + 11);
    actorResetYaw(arg0->extra.tmd->coords + 12);
}

static void func_actor_401300_8013CBAC(Task* arg0)
{
    Actor401300Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    GfxCoord*          root;
    GfxCoord*          root2;
    PlayerStatus*      config;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    s16                yaw;
    s32                angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x24;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_D1C        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
    }
    work->field_6++;
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    config                                = &gPlayerStatus;
    root                                  = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = config->coordMtx->t[0] - root->coord.t[0];
    aim->delta.vy                         = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz                         = config->coordMtx->t[2] - root->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    aim->playerYaw    = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                               (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    root2             = arg0->extra.tmd->coords;
    head[-1].delta.vx = config->coordMtx->t[0] - root2->coord.t[0];
    aim->delta.vy     = config->coordMtx->t[1] - root2->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root2->coord.t[2];
    yaw               = ratan2(head[-1].delta.vx, aim->delta.vz) + 0x800;
    aim->yaw          = yaw;
    aim->yaw          = actorNormalizeYaw(yaw);
    coord             = arg0->extra.tmd->coords;
    angle             = ratan2(aim->delta.vx, aim->delta.vz);
    aim->turn         = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2   = aim->turn;
    if (aim->turn < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x44C)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->turn > 0x20) {
        aim->turn = 0x20;
    }
    if (aim->turn < -0x20) {
        aim->turn = -0x20;
    }
    facing     = arg0->extra.tmd->coords;
    aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_8A2 == 2) {
        if (work->field_89E == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x16) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 0x16);
            }
        } else {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 5) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 5);
            }
        }
    } else if (work->field_6C & 0x100) {
        work->field_8A2 = 2;
        work->field_89C = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013D2AC(Task* arg0)
{
    Actor401300Work*   work;
    Enemy*             enemy;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    ActorChaseScratch* aim;
    s32                angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x19;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair(enemy, 0);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    if (work->field_6 >= 0x29) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x19:
            work->field_BF0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x28:
            work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = arg0->extra.tmd->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->turn       = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        coord2     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_401300_8013D6C4(Task* arg0)
{
    Actor401300Work*   work;
    Enemy*             enemy;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    ActorChaseScratch* aim;
    s32                angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1A;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair(enemy, 1);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_AD0, 0xC);
    if (work->field_6 >= 0x26) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x15:
            work->field_BF0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x25:
            work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = arg0->extra.tmd->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->turn       = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        coord2     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1964);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// `actorRescaleYaw` at 0x1964 on the actor's root coordinate, with
/// the root's `composeStamp` cleared again before the scratch block is released.
static __inline__ void Actor401300_ResetActorYaw(Task* actor)
{
    GfxCoord*             coord;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    coord                                      = actor->extra.tmd->coords;
    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]                   = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1]                   = (u16)blk->m.m[0][1];
    coord->coord.m[0][2]                   = (u16)blk->m.m[0][2];
    coord->coord.m[1][0]                   = (u16)blk->m.m[1][0];
    coord->coord.m[1][1]                   = (u16)blk->m.m[1][1];
    coord->coord.m[1][2]                   = (u16)blk->m.m[1][2];
    coord->coord.m[2][0]                   = (u16)blk->m.m[2][0];
    coord->coord.m[2][1]                   = (u16)blk->m.m[2][1];
    m22                                    = (u16)blk->m.m[2][2];
    coord->composeStamp                    = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2]                   = m22;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Facing yaw of `coord`.
static __inline__ s32 Actor401300_Yaw(GfxCoord* coord)
{
    return ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

/// `actorMoveForward` testing the flag byte through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (save->state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void func_actor_401300_8013DADC(Task* arg0)
{
    Actor401300Work*   work;
    Task*              task;
    GameActor*         player;
    PlayerStatus*      config;
    Enemy*             enemy;
    TmdObject*         obj;
    GfxCoord*          root;
    SVECTOR**          scratch;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    s16                amount;
    s16                ret;
    McSaveData*        save;

    work   = arg0->work;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player = (GameActor*)task->work;
    enemy  = arg0->spawnArg2.pointer;

    if (work->field_4 != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1B;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
        return;
    }
    scratch = (SVECTOR**)SCRATCH_HEAD_ADDR;
    config  = &gPlayerStatus;
    work->field_6++;
    root              = arg0->extra.tmd->coords;
    head              = (ActorChaseScratch*)SCRATCH_HEAD_AT(scratch, SVECTOR);
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    aim               = (ActorChaseScratch*)(SCRATCH_HEAD_AT(scratch, SVECTOR) = (SVECTOR*)(head - 1));
    aim->delta.vy     = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root->coord.t[2];
    save              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1B:
            if ((aim->angle = func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->angle != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            work->field_8B2 = 0;
            aim->turn       = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6 >= 0xB) {
                if (abs(aim->turn) < 0x200) {
                    if (!overlayOutOfRange(&aim->delta, 0x7D0)) {
                        work->field_8A2 = 0x1C;
                        work->field_89C = 1;
                        work->field_6   = 0;
                    }
                }
            }
            if (abs(aim->turn) > 0x400) {
                work->field_8A2 = 0x1C;
                work->field_89C = 1;
                work->field_6   = 0;
            }
            if (aim->turn > 6) {
                aim->turn = 6;
            } else if (aim->turn < -6) {
                aim->turn = -6;
            }
            aim->turn += Actor401300_Yaw(arg0->extra.tmd->coords);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x70) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 0x70);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1C:
            if ((aim->angle = func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->angle != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            aim->turn = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x1D;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && abs(aim->turn) < 0x100 &&
                enemy->hp > 0) {
                work->field_D00 = 0x7F;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    work->field_D20             = 1;
                    work->field_CAC.source.sets = D_actor_401300_801588F0;
                    work->field_CC0[2]          = 0;
                    work->field_CC0[1]          = 0;
                    work->field_CC0[0]          = 0;
                    work->field_CD0             = 7;
                    work->field_CD2             = 1;
                    aim->delta.vx               = -aim->delta.vx;
                    aim->delta.vy               = -aim->delta.vy;
                    aim->delta.vz               = -aim->delta.vz;
                    aim->turn                   = actorYawTo(task->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
                    if (abs(aim->turn) < 0x400) {
                        amount                      = -0x64;
                        work->field_CAC.animationId = 4;
                        work->field_CE4.vy          = aim->turn + Actor401300_Yaw(task->extra.tmd->coords);
                        ret                         = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 4), 0);
                    } else {
                        amount                      = 0x64;
                        work->field_CAC.animationId = 5;
                        work->field_CE4.vy          = aim->turn + Actor401300_Yaw(task->extra.tmd->coords) + 0x800;
                        ret                         = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 5), 0);
                    }
                    if (ret == 1) {
                        player->state = 0xA;
                    }
                    work->field_CD4.vx = task->extra.tmd->coords->coord.t[0];
                    work->field_CD4.vy = task->extra.tmd->coords->coord.t[1];
                    work->field_CD4.vz = task->extra.tmd->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x3E9, &work->field_CD4, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                    work->field_D22 = 0;
                    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &aim->delta);
                    aim->delta.vy = 0;
                    VectorNormalSS(&aim->delta, &aim->delta);
                    gte_lddp(amount);
                    gte_ldsv(&aim->delta);
                    gte_gpf12();
                    gte_stsv(&aim->delta);
                    work->field_CC0[0] = aim->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = aim->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                    work->field_8A2    = 0x1E;
                    work->field_89C    = 2;
                    work->field_6      = 0;
                }
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA8) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 0xA8);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1E:
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            if (work->field_6 < 8) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x79) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->extra.tmd->coords, -0x79);
                }
            } else if ((u16)(work->field_6 - 8) < 6) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x19) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->extra.tmd->coords, -0x19);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        case 0x1D:
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// `actorMoveForwardNonzero` testing `actorsFrozen` through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (save->state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gteVec                        = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void func_actor_401300_8013E930(Task* arg0)
{
    Actor401300Work*         work;
    Task*                    task;
    GameActor*               player;
    PlayerStatus*            config;
    Enemy*                   enemy;
    TmdObject*               obj;
    GfxCoord*                root;
    SVECTOR**                scratch;
    Actor401300LungeScratch* head;
    Actor401300LungeScratch* blk;
    SVECTOR*                 delta;
    SVECTOR*                 vec;
    s16                      cur;
    s16                      amount;
    s16                      ret;
    McSaveData*              save;

    work   = arg0->work;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player = (GameActor*)task->work;
    config = &gPlayerStatus;
    save   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    enemy  = arg0->spawnArg2.pointer;

    if (work->field_4 != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1F;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = arg0->extra.tmd->coords->coord.t[0];
        work->field_D06 = arg0->extra.tmd->coords->coord.t[1];
        work->field_D08 = arg0->extra.tmd->coords->coord.t[2];
        return;
    }
    scratch = (SVECTOR**)SCRATCH_HEAD_ADDR;
    work->field_6++;
    root              = arg0->extra.tmd->coords;
    head              = (Actor401300LungeScratch*)SCRATCH_HEAD_AT(scratch, SVECTOR);
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    delta             = &head[-1].delta;
    delta->vy         = config->coordMtx->t[1] - root->coord.t[1];
    blk               = (Actor401300LungeScratch*)(SCRATCH_HEAD_AT(scratch, SVECTOR) = (SVECTOR*)(head - 1));
    delta->vz         = config->coordMtx->t[2] - root->coord.t[2];
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1F:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            work->field_8B2 = 0;
            blk->angle      = actorYawTo(arg0->extra.tmd->coords, head[-1].delta.vx, delta->vz);
            if (work->field_6 >= 0xB) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                blk->dist.vx    = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
                blk->dist.vy    = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
                blk->dist.vz    = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
                blk->range      = SquareRoot0(blk->dist.vx * blk->dist.vx + blk->dist.vy * blk->dist.vy + blk->dist.vz * blk->dist.vz) + 1000;
                if (blk->range > 5000) {
                    blk->range = 5000;
                } else if (blk->range < 3000) {
                    blk->range = 3000;
                }
                work->field_14  = blk->range / 18;
                work->field_8B6 = 0;
                work->field_6   = 0;
                work->field_8BA = 0x20;
            }
            if (abs(blk->angle) > 0x400) {
                work->field_0 = 7;
            }
            if (blk->angle > 0x10) {
                blk->angle = 0x10;
            } else if (blk->angle < -0x10) {
                blk->angle = -0x10;
            }
            blk->angle += Actor401300_Yaw(arg0->extra.tmd->coords);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, blk->angle, 1);
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x20:
            work->field_970.radius = 0x140;
            func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && work->field_6 >= 8 &&
                enemy->hp > 0) {
                work->field_D00 = 0x7F;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    SndEvt_EnqueueType6(SOUND_PLAYER_STRUCK, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                        (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                    work->field_D20             = 1;
                    work->field_CAC.source.sets = D_actor_401300_801588F0;
                    blk->delta.vx               = work->field_D04 - task->extra.tmd->coords->coord.t[0];
                    blk->delta.vy               = work->field_D06 - task->extra.tmd->coords->coord.t[1];
                    blk->delta.vz               = work->field_D08 - task->extra.tmd->coords->coord.t[2];
                    blk->angle                  = actorYawTo(task->extra.tmd->coords, head[-1].delta.vx, delta->vz);
                    if (abs(blk->angle) < 0x400) {
                        amount                      = -0x46;
                        work->field_CAC.animationId = 4;
                        work->field_CE4.vy          = blk->angle + Actor401300_Yaw(task->extra.tmd->coords);
                        ret                         = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 2), 0);
                    } else {
                        amount                      = 0x46;
                        work->field_CAC.animationId = 5;
                        work->field_CE4.vy          = blk->angle + Actor401300_Yaw(task->extra.tmd->coords) + 0x800;
                        ret                         = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 3), 0);
                    }
                    if (ret == 1) {
                        player->state = 0xA;
                    }
                    work->field_CD4.vx = task->extra.tmd->coords->coord.t[0];
                    work->field_CD4.vy = task->extra.tmd->coords->coord.t[1];
                    work->field_CD4.vz = task->extra.tmd->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x3E9, &work->field_CD4, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                    work->field_D22 = 0;
                    vec             = &blk->delta;
                    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, vec);
                    blk->delta.vy = 0;
                    VectorNormalSS(vec, vec);
                    gte_lddp(amount);
                    gte_ldsv(vec);
                    gte_gpf12();
                    gte_stsv(vec);
                    work->field_CC0[0] = blk->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = blk->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                }
            }
            if (work->field_D20 == 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_14);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57) == 0) {
                cur = work->field_6;
                if (cur < 0x11 && work->field_D20 == 0) {
                    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (s16)(0x54 - cur * 0x54 / 16)) != 0) {
                        Actor401300_MoveForwardNonzeroSave(save, arg0->extra.tmd->coords, 0x54 - work->field_6 * 0x54 / 16);
                    }
                }
            }
            func_actor_401300_80132910(arg0, work->field_990, 0xC);
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor401300LungeScratch);
}

/// Scale `m` uniformly by `scale` (4.12), translation included.
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale)
{
    ActorScaleMatrixScratch* head;
    ActorScaleMatrixScratch* blk;

    head                                          = SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch);
    blk                                           = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch) = blk;
    blk->scale.vz                                 = scale;
    blk->scale.vy                                 = scale;
    head[-1].scale.vx                             = scale;
    ScaleMatrix(m, &blk->scale);
    blk->trans.vx = m->t[0];
    blk->trans.vy = m->t[1];
    blk->trans.vz = m->t[2];
    gte_lddp(scale);
    gte_ldsv(&blk->trans);
    gte_gpf12();
    gte_stsv(&blk->trans);
    m->t[0] = blk->trans.vx;
    m->t[1] = blk->trans.vy;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
    m->t[2] = blk->trans.vz;
}

static void func_actor_401300_8013F628(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    PlayerStatus*    config;
    GfxCoord*        root;
    SVECTOR**        scratch;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              mod;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj = arg0->extra.tmd;
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x20;
        work->field_89E        = 0;
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = arg0->extra.tmd->coords->coord.t[0];
        work->field_D06 = arg0->extra.tmd->coords->coord.t[1];
        work->field_D08 = arg0->extra.tmd->coords->coord.t[2];
        Actor401300_ScaleMatrix(&work->field_C48, 0);
        return;
    }
    scratch = (SVECTOR**)SCRATCH_HEAD_ADDR;
    config  = &gPlayerStatus;
    work->field_6++;
    root                              = arg0->extra.tmd->coords;
    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    head[-1].vx                       = config->coordMtx->t[0] - root->coord.t[0];
    vec                               = head - 1;
    vec->vy                           = config->coordMtx->t[1] - root->coord.t[1];
    vec->vz                           = config->coordMtx->t[2] - root->coord.t[2];
    SCRATCH_HEAD_AT(scratch, SVECTOR) = head - 3;
    if (work->field_6 < 0x12) {
        Actor401300_ScaleMatrix(&work->field_C48, (work->field_6 << 12) / 30);
        if (gGameSession->location.loc.area == 0xB) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 2, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
                }
            }
        } else if (gGameSession->location.loc.area == 0x1D) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 2, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    Gp_SpawnEff(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
                }
            }
        }
    }
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x20:
            work->field_970.radius = 0x500;
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (work->field_D20 == 0 && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x78) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 0x78);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            cur                    = work->field_6;
            if (cur < 0x11 && work->field_D20 == 0) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x54 - cur * 0x54 / 16) != 0) {
                    actorMoveForwardNonzero(arg0->extra.tmd->coords, 0x54 - work->field_6 * 0x54 / 16);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
}

static void func_actor_401300_80140300(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 2;
        work->field_8A2               = 0xB;
        work->field_8A6               = 0x10;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x40;
        work->field_8B8        = 0xC8;
        work->field_8BA        = 0x40;
        work->field_970.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

static void func_actor_401300_8014046C(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 2;
        work->field_8A2               = 0x22;
        work->field_8A6               = 0x10;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        work->field_8B6               = 0x40;
        work->field_8B8               = 0xC8;
        work->field_8BA               = 0x40;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_970.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->field_AD0, 0xC, 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

// This is a decompilation attempt by the m2c tool.

/// `Actor401300_InRange` with the flag kept apart from the comparison. The
/// dead `ret = cmp` in each arm stops jump from turning the if/else into a
/// store-flag, which would fold `cmp` and `ret` into one register.
static __inline__ s32 Actor401300_InRangeFlag(Task* arg0)
{
    SVECTOR   out;
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp;
    GfxCoord* view;
    VECTOR*   vecp;
    s32*      flagp;
    SVECTOR*  outp;
    GfxCoord* p;
    s32       ret;
    s32       cmp;

    memset(&out, 0, 8);
    p     = &arg0->extra.tmd->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->parent != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->parent;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    cmp = (u16)(out.vz + 0x12B) < 0xA27;
    if (cmp == 0) {
        ret = cmp;
        ret = 0;
    } else {
        ret = cmp;
        ret = 1;
    }
    return ret;
}

/// 1 when the first of `recs` is a kind 0x10000 record.
static __inline__ s32 Actor401300_HasRec10000(WorldCollisionContact* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key.value)
            break;
        if ((recs[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Snaps the player's height to the actor's when it is locked (`field_CD0`
/// 7) and has drifted 0x321 or more away.
static __inline__ void Actor401300_SnapPlayerHeight(Task* actor)
{
    Actor401300Work* work;
    Task*            slot;
    GfxCoord*        playerCoord;
    GfxCoord*        actorCoord;

    work = actor->work;
    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((slot != NULL) && (work->field_CD0 == 7)) {
        playerCoord = slot->extra.tmd->coords;
        actorCoord  = actor->extra.tmd->coords;
        if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x321) {
            playerCoord->coord.t[1]               = actorCoord->coord.t[1];
            slot->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static const Actor401300StateTable D_actor_401300_80131F34 = { {
    func_actor_401300_8014192C,
    func_actor_401300_801419B8,
    func_actor_401300_80141A60,
    func_actor_401300_80141B0C,
    func_actor_401300_80135DDC,
    func_actor_401300_80141BC8,
    func_actor_401300_80136238,
    func_actor_401300_801365F8,
    func_actor_401300_80136CE8,
    func_actor_401300_801376E4,
    func_actor_401300_80137D78,
    func_actor_401300_80138160,
    func_actor_401300_80138800,
    func_actor_401300_80138B24,
    func_actor_401300_80141C80,
    func_actor_401300_80141C88,
    func_actor_401300_80141D50,
    func_actor_401300_80141DF4,
    NULL,
    func_actor_401300_80138CF8,
    func_actor_401300_80138FCC,
    func_actor_401300_80139134,
    func_actor_401300_80139520,
    func_actor_401300_801397F8,
    func_actor_401300_80139AB0,
    func_actor_401300_8013A5C0,
    func_actor_401300_8013A208,
    func_actor_401300_8013AAE8,
    func_actor_401300_8013AE48,
    func_actor_401300_8013B6E8,
    func_actor_401300_8013CBAC,
    func_actor_401300_8013D2AC,
    func_actor_401300_8013D6C4,
    func_actor_401300_8013DADC,
    func_actor_401300_8013E930,
    func_actor_401300_8013F628,
    func_actor_401300_80141EF8,
    func_actor_401300_80140300,
    func_actor_401300_8014046C,
    func_actor_401300_80135FC4,
    func_actor_401300_8013BB30,
} };

static void func_actor_401300_801405DC(Enemy* enemy, Task* actor)
{
    VECTOR                pos;
    Actor401300StateTable states;
    Actor401300Work*      work;
    ActorViewScratch*     scratch;
    ActorViewScratch*     head;
    Task*                 player;
    PlayerStatus*         config;
    s32                   state;
    s32                   action;

    work   = actor->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config = &gPlayerStatus;
    states = D_actor_401300_80131F34;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->field_AD0);
            Gp_ClearRec18Occupied(work->field_990);
            Gp_ClearRec18Occupied(work->sensorContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->field_AD0);
            Gp_ClearRec18Occupied(work->field_990);
            Gp_ClearRec18Occupied(work->sensorContacts);
            return;
    }

    head                                   = SCRATCH_STACK_CURSOR(ActorViewScratch);
    SCRATCH_STACK_CURSOR(ActorViewScratch) = head - 1;
    scratch                                = head - 1;

    if (work->field_C88 > 0) {
        work->field_C88 = (s16)((u16)work->field_C88 - 1);
    } else {
        func_actor_401300_80134F90(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
        scratch->pos.vx = 0;
        scratch->pos.vy = 0;
        scratch->pos.vz = 0;
        actorTransformToView(actor->extra.tmd->coords + 1, &scratch->pos);
        work->field_970.pos.vx       = scratch->pos.vx;
        work->field_970.pos.vy       = scratch->pos.vy;
        work->field_970.pos.vz       = scratch->pos.vz;
        work->field_920.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
        work->field_920.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - 0x15E;
        work->field_920.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
        work->field_920.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&work->field_920);
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(actor->extra.tmd->coords);
        state = work->field_0;
    }
    if ((state == 0x15) || (state == 0) || (state == 0x24) || (state == 0x1D) || (state == 0x28)) {
        work->field_970.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_970.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        if ((u32)((u16)work->field_0 - 0x21) < 2U) {
            work->field_AB0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        } else {
            work->field_AB0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        }
    }
    if ((Actor401300_HasRec10000(work->sensorContacts) == 1) || (enemy->hp <= 0)) {
        work->field_BF0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    Gp_ClearRec18Occupied(work->field_AD0);
    Gp_ClearRec18Occupied(work->field_990);
    Gp_ClearRec18Occupied(work->sensorContacts);

    if (work->field_D20 == 1) {
        state = work->field_0;
        if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
            Actor401300_SnapPlayerHeight(actor);
        }
        action          = work->field_CAC.animationId;
        work->field_D22 = (u16)(work->field_D22 + 1);
        switch (action) {
            case 0:
            case 1:
            case 2:
            case 3:
                break;
            case 4:
                if ((s16)work->field_D22 == 0xF) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(SOUND_NEO_ARK_WOODLAND_STRANGER_HIT, (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                            (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    } else {
                        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 0x13), (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                            (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &player->extra.tmd->coords[1], 0x80003A00, NULL);
                    }
                }
                if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3FE, work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 5:
                if ((s16)work->field_D22 == 0xD) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(SOUND_NEO_ARK_WOODLAND_STRANGER_HIT, (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                            (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    } else {
                        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 0x13), (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                            (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &player->extra.tmd->coords[1], 0x80003A00, NULL);
                    }
                }
                if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3FE, work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 6:
            case 7:
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->field_CAC.animationId) {
                case 0:
                    break;
                case 1:
                    if (config->hp > 0) {
                        work->field_CAC.animationId = 2;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 2:
                    if (config->hp > 0) {
                        work->field_CAC.animationId = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 4:
                    if (config->hp > 0) {
                        work->field_CAC.animationId = 6;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 5:
                    if (config->hp > 0) {
                        work->field_CAC.animationId = 7;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 3:
                case 6:
                case 7:
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->field_D20 = 0;
                    break;
            }
        }
    }
    if ((work->field_C8A == 1) && (work->field_D20 == 0)) {
        Gp_ReleaseStateF0Add(actor, 0xD);
        work->field_C8A = 0;
    }
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);

    work->field_D28[work->field_D78].vx = scratch->pos.vx;
    work->field_D28[work->field_D78].vy = scratch->pos.vy;
    work->field_D28[work->field_D78].vz = scratch->pos.vz;

    SCRATCH_STACK_RELEASE_BYTES(0x18);
    work->field_D78 = (u16)work->field_D78 + 1;
    if (work->field_D78 == 7) {
        work->field_D78 = 0;
    }
    if ((u32)((u16)work->field_8A2 - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_D28[work->field_D78].vx;
        enemy->bodyPos.vy = work->field_D28[work->field_D78].vy;
        enemy->bodyPos.vz = work->field_D28[work->field_D78].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

void func_actor_401300_8014148C(void)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401300_80141F2C`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const GpEnemyTaskFuncTable3 D_actor_401300_8013201C = { {
    func_actor_401300_80134454,
    func_actor_401300_801405DC,
    Gp_DestroyEnemy,
} };

s32 func_actor_401300_80141494(Task* arg0, s32 arg1, AnimationPlayRequest* arg2)
{
    Actor401300Work* work = arg0->work;

    switch (arg2->animationId) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

/// Places the model's root coordinate from `placement`: sets its translation,
/// applies the X, Y and Z rotations in turn, and caches the resulting heading
/// (`ratan2` of the matrix Z axis) in `Actor401300Work::yaw`. Always returns 1.
s32 func_actor_401300_80141614(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    s32              mx;
    s32              mz;
    Actor401300Work* work;

    work                                = (Actor401300Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    mx                                    = coord->coord.m[2][0];
    mz                                    = coord->coord.m[2][2];
    work->yaw                             = ratan2(-mx, mz);
    return 1;
}

#include "../../shared/actor_messages_release_hold.inc.c"

static void func_actor_401300_80141758(Task* task)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = (Actor401300Work*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->field_D0C != NULL) {
            taskKill(work->field_D0C);
        }
        if (work->field_D10 != NULL) {
            taskKill(work->field_D10);
        }
        Gp_UnlinkObj(&work->field_BF0);
        Gp_UnlinkObj(&work->field_970);
        Gp_UnlinkObj(&work->field_AB0);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

static s32 func_actor_401300_801417F0(Task* arg0)
{
    SVECTOR out;

    memset(&out, 0, 8);
    actorTransformToView(&arg0->extra.tmd->coords[1], &out);
    return (u16)(out.vz + 0x12B) < 0xA27;
}

static void func_actor_401300_8014192C(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;
    TmdObject*       obj;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->field_BF0.flags         = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags         = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        return;
    }
    if (enemy->hp != -0x3E7 && work->field_C8A == 0 && (arg0->spawnArg1.value >> 16) == 2) {
        enemy->hp = -0x3E7;
    }
}

static void func_actor_401300_801419B8(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 2;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141A60(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 3;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141B0C(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 0xB;
        work->field_8B6       = 0x20;
        work->field_8BA       = 8;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141BC8(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x12;
        work->field_8A2       = 0xD;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_AB0.flags = (u16)(work->field_AB0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

static void func_actor_401300_80141C80(Task* arg0)
{
}

static void func_actor_401300_80141C88(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 2;
        work->field_8A2               = 8;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        work->field_8A6               = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
}

static void func_actor_401300_80141D50(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_970.radius        = 0x280;
        work->field_BF0.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_AB0.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_89C               = 2;
        work->field_8A2               = 0x16;
        work->field_8B4               = 0;
        work->field_8B2               = 0;
        work->field_8A6               = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

static void func_actor_401300_80141DF4(Task* arg0)
{
    Actor401300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8B6 = 0x20;
        work->field_8BA = 8;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_6   = work->field_CA0 + ((gRandomLcgState >> 16) & 0xF);
    }
    func_actor_401300_80133A3C(arg0);
    if (--work->field_6 < 0) {
        switch (work->field_8A2) {
            case 11:
            case 23:
                work->field_0 = 0xF;
                break;
            case 12:
            case 24:
            case 34:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

static void func_actor_401300_80141EF8(Task* task)
{
    Actor401300Work* work  = (Actor401300Work*)task->work;
    Enemy*           enemy = task->spawnArg2.pointer;

    if (enemy->hp != -0x3E7 && work->field_C8A == 0) {
        enemy->hp = -0x3E7;
    }
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401300_80141F2C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401300_8013201C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
