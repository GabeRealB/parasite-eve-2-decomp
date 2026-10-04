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
#include "gameplay/geometry.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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
#include "main/random.h"
#include "main/gfx.h"
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

/// Private work block of the actor 01900 task, hanging off `Task::work`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `WorldCollisionBody` list nodes unlinked by the
/// destroy callback and the two child tasks it kills. `Actor01900_Fn0A764`
/// masks `field_A08.flags` and `field_B48.flags`, which is what fixes those
/// two offsets as `WorldCollisionBody` rather than opaque padding.
typedef struct Actor01900Work {
    /* 0x000 */ s16                   field_0;
    /* 0x002 */ s16                   field_2;
    /* 0x004 */ s16                   field_4;
    /* 0x006 */ s16                   field_6;
    /* 0x008 */ s16                   field_8;
    /* 0x00A */ byte                  pad_A[2];
    /* 0x00C */ ActorPatrolPoint      field_C[2];
    /* 0x014 */ s16                   field_14;
    /* 0x016 */ byte                  pad_16[0x6];
    /* 0x01C */ ActorAnimRig19        rig;
    /* 0x458 */ ActorAnimRig19        blend;
    /* 0x894 */ s32                   field_894;
    /* 0x898 */ s16                   field_898;
    /* 0x89A */ s16                   field_89A;
    /* 0x89C */ s16                   field_89C;
    /* 0x89E */ s16                   field_89E;
    /* 0x8A0 */ u16                   field_8A0;
    /* 0x8A2 */ s16                   field_8A2;
    /* 0x8A4 */ s16                   field_8A4;
    /* 0x8A6 */ s16                   field_8A6;
    /* 0x8A8 */ s16                   field_8A8;
    /* 0x8AA */ s16                   field_8AA;
    /* 0x8AC */ s16                   field_8AC;
    /* 0x8AE */ s16                   field_8AE;
    /* 0x8B0 */ s16                   field_8B0;
    /* 0x8B2 */ byte                  pad_8B2[2];
    /* 0x8B4 */ s32                   field_8B4;
    /* 0x8B8 */ EffectSpawnArg        field_8B8;
    /* 0x8C0 */ byte                  pad_8C0[8];
    /* 0x8C8 */ WorldCollisionBody    field_8C8;
    /* 0x8E8 */ WorldCollisionContact field_8E8;
    /* 0x900 */ byte                  pad_900[0x108];
    /* 0xA08 */ WorldCollisionBody    field_A08;
    /* 0xA28 */ WorldCollisionContact field_A28;
    /* 0xA40 */ byte                  pad_A40[0x108];
    /* 0xB48 */ WorldCollisionBody    field_B48;
    /* 0xB68 */ WorldCollisionContact field_B68;
    /* 0xB80 */ byte                  pad_B80[0x30];
    /* 0xBB0 */ MATRIX                field_BB0; // `TmdObject.lightMtx` light matrix
    /* 0xBD0 */ MATRIX                field_BD0; // `TmdObject.colorMtx` color matrix
    /* 0xBF0 */ byte                  pad_BF0[0x20];
    /* 0xC10 */ s16                   field_C10;
    /* 0xC12 */ s16                   field_C12;
    /* 0xC14 */ s16                   field_C14;
    /* 0xC16 */ byte                  pad_C16[2];
    /* 0xC18 */ SVECTOR               field_C18;
    /* 0xC20 */ s16                   field_C20;
    /* 0xC22 */ s16                   field_C22;
    /* 0xC24 */ s16                   field_C24;
    /* 0xC26 */ s16                   field_C26;
    /* 0xC28 */ s16                   field_C28;
    /* 0xC2A */ s16                   field_C2A;
    /* 0xC2C */ s16                   field_C2C;
    /* 0xC2E */ s16                   field_C2E;
    /* 0xC30 */ s16                   field_C30;
    /* 0xC32 */ s16                   field_C32;
    /* 0xC34 */ u8                    field_C34[3];
    /* 0xC37 */ u8                    field_C37;
    /* 0xC38 */ Task*                 field_C38;
    /* 0xC3C */ Task*                 field_C3C;
    /* 0xC40 */ s16                   field_C40;
    /* 0xC42 */ s16                   field_C42;
    /* 0xC44 */ s16                   field_C44;
    /* 0xC46 */ byte                  pad_C46[2];
    /// Ring of the last seven view-space positions `Actor01900_Fn09D3C`
    /// records, one per step; `field_C98` is the write cursor.
    /* 0xC48 */ SVECTOR field_C48[7];
    /* 0xC80 */ byte    pad_C80[0x18];
    /* 0xC98 */ s16     field_C98;
} Actor01900Work;

/// The actor's state handlers, indexed by `Actor01900Work::field_0`.
/// `Actor01900_Fn09D3C` copies the table to its frame before dispatching.
typedef struct Actor01900StateTable {
    TaskFunc fn[32];
} Actor01900StateTable;
STATIC_ASSERT_SIZEOF(Actor01900StateTable, 0x80);

extern EnemyParams        Actor01900_D0AC54;
extern ActorSpawnParamRow Actor01900_D0AC64[];
extern AnimationSet*      Actor01900_D17174[46];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01900_D1728C[8];
static AnimationSet     _gActor01900Actor101900Animation16960;
extern ActorHeightClamp Actor01900_D172CC[];
/// Twelve preset hit-reaction directions `Actor01900_Fn02664` copies from;
/// `pad` carries the index of the coordinate the effect is attached to.
extern SVECTOR   Actor01900_D1722C[];
static TmdSource _gActor01900StrangerBurstHand;
extern s16       Actor01900_D172FC;

#include "../../shared/actor_contacts.h"

static void Actor01900_Fn02A50(Task* arg0);
static void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id);
static void Actor01900_Fn01C94(Task* arg0);
static void Actor01900_Fn0AB1C(Task* arg0);
static void Actor01900_Fn0A6CC(Task* task);
static s32  Actor01900_Fn03FF8(Task* arg0, WorldCollisionContact* recs, s16 count);
static void Actor01900_Fn08724(Task* arg0);
static void Actor01900_Fn0A7C0(Task* arg0);
static void Actor01900_Fn03C04(GameLocationKey* session, GfxCoord* coord);
s32         Actor01900_Fn0A31C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3);
s32         Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2, s32 arg3);

/* Inline bodies behind `Actor01900_Fn080A8`. Same shapes as
 * `actor_400100_facing.h` and `ActorsShared80135a60`; inlining is what keeps
 * each `SCRATCH_STACK_CURSOR_SLOT` access out of a register CSE would share. */

static TmdSource _gActor01900GrinningStrangerBody;
s32              Actor01900_Fn0A31C(Task*, s32, AnimationPlayRequest*, s32);
s32              Actor01900_Fn0A59C(Task*, s32, s32, s32);
s32              Actor01900_Fn0A5A4(Task*, s32, u16*, s32);
s32              Actor01900_Fn0A314(Task*, s32, s32, s32);
void             Actor01900_Fn0ABE4(Task*);

DamageAttack Actor01900_D0AC4C[2] = {
    { 16, 7 },
    { 22, 0 },
};

EnemyParams Actor01900_D0AC54 = { Actor01900_D0AC4C, 160, 42, 48, 4, 100, 10, 100, 0 };

ActorSpawnParamRow Actor01900_D0AC64[3] = {
    { 15, 400, 8, 2000, { 0, 0, 0, 0 } },
    { 0, 300, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 200, 7, 3000, { 0, 0, 0, 0 } },
};

static TmdBone _gActor01900GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor01900GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor01900GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor01900GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor01900GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor01900GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor01900GrinningStrangerBodyPartVerts,
    _gActor01900GrinningStrangerBodyVerts,
    _gActor01900GrinningStrangerBodyNormals,
    _gActor01900GrinningStrangerBodySkeleton,
    _gActor01900GrinningStrangerBodyStream,
};

static TmdBone _gActor01900StrangerBurstHandSkeleton[3] = {
#include "assets/stranger_burst_hand_skeleton.inc"
};

static u32 _gActor01900StrangerBurstHandPartVerts[3] = {
#include "assets/stranger_burst_hand_partVerts.inc"
};

static SVECTOR _gActor01900StrangerBurstHandVerts[33] = {
#include "assets/stranger_burst_hand_verts.inc"
};

static SVECTOR _gActor01900StrangerBurstHandNormals[45] = {
#include "assets/stranger_burst_hand_normals.inc"
};

static u32 _gActor01900StrangerBurstHandStream[357] = {
#include "assets/stranger_burst_hand_stream.inc"
};

static TmdSource _gActor01900StrangerBurstHand = {
    0,
    2008,
    304,
    3,
    _gActor01900StrangerBurstHandPartVerts,
    _gActor01900StrangerBurstHandVerts,
    _gActor01900StrangerBurstHandNormals,
    _gActor01900StrangerBurstHandSkeleton,
    _gActor01900StrangerBurstHandStream,
};

static AnimationPackedPose _gActor01900Actor101900Animation115E8Bank1[19] = {
#include "assets/actor_101900_animation_115E8_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation115E8Bank4[259] = {
#include "assets/actor_101900_animation_115E8_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation115E8Records[337] = {
#include "assets/actor_101900_animation_115E8_records.inc"
};

static u16 _gActor01900Actor101900Animation115E8Indices[20] = {
#include "assets/actor_101900_animation_115E8_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation115E8 = {
    _gActor01900Actor101900Animation115E8Records,
    _gActor01900Actor101900Animation115E8Indices,
    { NULL, _gActor01900Actor101900Animation115E8Bank1, NULL, NULL, _gActor01900Actor101900Animation115E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation119ECBank1[6] = {
#include "assets/actor_101900_animation_119EC_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation119ECBank4[91] = {
#include "assets/actor_101900_animation_119EC_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation119ECRecords[128] = {
#include "assets/actor_101900_animation_119EC_records.inc"
};

static u16 _gActor01900Actor101900Animation119ECIndices[20] = {
#include "assets/actor_101900_animation_119EC_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation119EC = {
    _gActor01900Actor101900Animation119ECRecords,
    _gActor01900Actor101900Animation119ECIndices,
    { NULL, _gActor01900Actor101900Animation119ECBank1, NULL, NULL, _gActor01900Actor101900Animation119ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation125D0Bank1[23] = {
#include "assets/actor_101900_animation_125D0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation125D0Bank4[297] = {
#include "assets/actor_101900_animation_125D0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation125D0Records[375] = {
#include "assets/actor_101900_animation_125D0_records.inc"
};

static u16 _gActor01900Actor101900Animation125D0Indices[20] = {
#include "assets/actor_101900_animation_125D0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation125D0 = {
    _gActor01900Actor101900Animation125D0Records,
    _gActor01900Actor101900Animation125D0Indices,
    { NULL, _gActor01900Actor101900Animation125D0Bank1, NULL, NULL, _gActor01900Actor101900Animation125D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation12AF0Bank1[7] = {
#include "assets/actor_101900_animation_12AF0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation12AF0Bank4[127] = {
#include "assets/actor_101900_animation_12AF0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation12AF0Records[160] = {
#include "assets/actor_101900_animation_12AF0_records.inc"
};

static u16 _gActor01900Actor101900Animation12AF0Indices[20] = {
#include "assets/actor_101900_animation_12AF0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation12AF0 = {
    _gActor01900Actor101900Animation12AF0Records,
    _gActor01900Actor101900Animation12AF0Indices,
    { NULL, _gActor01900Actor101900Animation12AF0Bank1, NULL, NULL, _gActor01900Actor101900Animation12AF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation12F44Bank1[8] = {
#include "assets/actor_101900_animation_12F44_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation12F44Bank4[89] = {
#include "assets/actor_101900_animation_12F44_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation12F44Records[144] = {
#include "assets/actor_101900_animation_12F44_records.inc"
};

static u16 _gActor01900Actor101900Animation12F44Indices[20] = {
#include "assets/actor_101900_animation_12F44_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation12F44 = {
    _gActor01900Actor101900Animation12F44Records,
    _gActor01900Actor101900Animation12F44Indices,
    { NULL, _gActor01900Actor101900Animation12F44Bank1, NULL, NULL, _gActor01900Actor101900Animation12F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation132D0Bank1[10] = {
#include "assets/actor_101900_animation_132D0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation132D0Bank4[71] = {
#include "assets/actor_101900_animation_132D0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation132D0Records[106] = {
#include "assets/actor_101900_animation_132D0_records.inc"
};

static u16 _gActor01900Actor101900Animation132D0Indices[20] = {
#include "assets/actor_101900_animation_132D0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation132D0 = {
    _gActor01900Actor101900Animation132D0Records,
    _gActor01900Actor101900Animation132D0Indices,
    { NULL, _gActor01900Actor101900Animation132D0Bank1, NULL, NULL, _gActor01900Actor101900Animation132D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation13658Bank1[6] = {
#include "assets/actor_101900_animation_13658_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation13658Bank4[77] = {
#include "assets/actor_101900_animation_13658_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation13658Records[111] = {
#include "assets/actor_101900_animation_13658_records.inc"
};

static u16 _gActor01900Actor101900Animation13658Indices[20] = {
#include "assets/actor_101900_animation_13658_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation13658 = {
    _gActor01900Actor101900Animation13658Records,
    _gActor01900Actor101900Animation13658Indices,
    { NULL, _gActor01900Actor101900Animation13658Bank1, NULL, NULL, _gActor01900Actor101900Animation13658Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation139DCBank1[13] = {
#include "assets/actor_101900_animation_139DC_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation139DCBank4[58] = {
#include "assets/actor_101900_animation_139DC_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation139DCRecords[108] = {
#include "assets/actor_101900_animation_139DC_records.inc"
};

static u16 _gActor01900Actor101900Animation139DCIndices[20] = {
#include "assets/actor_101900_animation_139DC_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation139DC = {
    _gActor01900Actor101900Animation139DCRecords,
    _gActor01900Actor101900Animation139DCIndices,
    { NULL, _gActor01900Actor101900Animation139DCBank1, NULL, NULL, _gActor01900Actor101900Animation139DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation14164Bank1[20] = {
#include "assets/actor_101900_animation_14164_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation14164Bank4[166] = {
#include "assets/actor_101900_animation_14164_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation14164Records[236] = {
#include "assets/actor_101900_animation_14164_records.inc"
};

static u16 _gActor01900Actor101900Animation14164Indices[20] = {
#include "assets/actor_101900_animation_14164_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation14164 = {
    _gActor01900Actor101900Animation14164Records,
    _gActor01900Actor101900Animation14164Indices,
    { NULL, _gActor01900Actor101900Animation14164Bank1, NULL, NULL, _gActor01900Actor101900Animation14164Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation14938Bank1[20] = {
#include "assets/actor_101900_animation_14938_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation14938Bank4[177] = {
#include "assets/actor_101900_animation_14938_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation14938Records[244] = {
#include "assets/actor_101900_animation_14938_records.inc"
};

static u16 _gActor01900Actor101900Animation14938Indices[20] = {
#include "assets/actor_101900_animation_14938_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation14938 = {
    _gActor01900Actor101900Animation14938Records,
    _gActor01900Actor101900Animation14938Indices,
    { NULL, _gActor01900Actor101900Animation14938Bank1, NULL, NULL, _gActor01900Actor101900Animation14938Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation15420Bank1[27] = {
#include "assets/actor_101900_animation_15420_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation15420Bank4[250] = {
#include "assets/actor_101900_animation_15420_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation15420Records[347] = {
#include "assets/actor_101900_animation_15420_records.inc"
};

static u16 _gActor01900Actor101900Animation15420Indices[20] = {
#include "assets/actor_101900_animation_15420_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation15420 = {
    _gActor01900Actor101900Animation15420Records,
    _gActor01900Actor101900Animation15420Indices,
    { NULL, _gActor01900Actor101900Animation15420Bank1, NULL, NULL, _gActor01900Actor101900Animation15420Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation15F20Bank1[46] = {
#include "assets/actor_101900_animation_15F20_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation15F20Bank4[218] = {
#include "assets/actor_101900_animation_15F20_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation15F20Records[328] = {
#include "assets/actor_101900_animation_15F20_records.inc"
};

static u16 _gActor01900Actor101900Animation15F20Indices[20] = {
#include "assets/actor_101900_animation_15F20_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation15F20 = {
    _gActor01900Actor101900Animation15F20Records,
    _gActor01900Actor101900Animation15F20Indices,
    { NULL, _gActor01900Actor101900Animation15F20Bank1, NULL, NULL, _gActor01900Actor101900Animation15F20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation16960Bank1[12] = {
#include "assets/actor_101900_animation_16960_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation16960Bank4[254] = {
#include "assets/actor_101900_animation_16960_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation16960Records[346] = {
#include "assets/actor_101900_animation_16960_records.inc"
};

static u16 _gActor01900Actor101900Animation16960Indices[20] = {
#include "assets/actor_101900_animation_16960_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation16960 = {
    _gActor01900Actor101900Animation16960Records,
    _gActor01900Actor101900Animation16960Indices,
    { NULL, _gActor01900Actor101900Animation16960Bank1, NULL, NULL, _gActor01900Actor101900Animation16960Bank4, NULL, NULL, NULL },
};

s8 Actor01900_D16988[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

AnimationSet* Actor01900_D17174[46] = {
    NULL,
    NULL,
    &_gActor01900Actor101900Animation15F20,
    &_gActor01900Actor101900Animation15F20,
    &_gActor01900Actor101900Animation115E8,
    NULL,
    NULL,
    &_gActor01900Actor101900Animation15420,
    &_gActor01900Actor101900Animation125D0,
    &_gActor01900Actor101900Animation12AF0,
    &_gActor01900Actor101900Animation132D0,
    &_gActor01900Actor101900Animation13658,
    NULL,
    &_gActor01900Actor101900Animation119EC,
    NULL,
    NULL,
    NULL,
    &_gActor01900Actor101900Animation12F44,
    NULL,
    &_gActor01900Actor101900Animation12AF0,
    &_gActor01900Actor101900Animation14164,
    &_gActor01900Actor101900Animation14938,
    NULL,
    &_gActor01900Actor101900Animation13658,
    &_gActor01900Actor101900Animation139DC,
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
};

SVECTOR Actor01900_D1722C[12] = {
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

TaskMessageEntry Actor01900_D1728C[8] = {
    { 2015, Actor01900_Fn0A314 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, Actor01900_Fn0A31C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { 2014, Actor01900_Fn0A59C },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor01900_Fn0A5A4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorHeightClamp Actor01900_D172CC[3] = {
    { 1, 3, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 5, 29, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

s16 Actor01900_D172FC = 0;

TaskDesc Actor01900_D17300 = { { { TASK_BODY_TMD, 96 } }, Actor01900_Fn0ABE4, { .model = &_gActor01900GrinningStrangerBody } };

static SVECTOR ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// Cross-fade lengths in frames, indexed by the clip being left and the clip
/// being entered. `Actor01900_Fn01C94` reads one entry per animation change.
extern s8 Actor01900_D16988[][0x2D];

static SVECTOR ActorContact_ScratchPosition;

static const Actor01900StateTable Actor01900_D001BC;

static void Actor01900_Fn03710(Task* arg0);

static void Actor01900_Fn03854(Task* arg0);

static void Actor01900_Fn042BC(Task* arg0);

static void Actor01900_Fn04D14(Task* arg0);

static void Actor01900_Fn0551C(Task* arg0);

static void Actor01900_Fn05B4C(Task* arg0);

static void Actor01900_Fn05F38(Task* arg0);

static void Actor01900_Fn06100(Task* arg0);

static void Actor01900_Fn06634(Task* arg0);

static void Actor01900_Fn06904(Task* arg0);

static void Actor01900_Fn06B4C(Task* arg0);

static void Actor01900_Fn06F40(Task* arg0);

static void Actor01900_Fn07810(Task* arg0);

static void Actor01900_Fn07BA8(Task* arg0);

static void Actor01900_Fn080A8(Task* arg0);

static void Actor01900_Fn083E8(Task* arg0);

static void Actor01900_Fn0892C(Task* arg0);

static void Actor01900_Fn09694(Task* arg0);

static void Actor01900_Fn09BE8(Task* arg0);

static void Actor01900_Fn0A764(Task* arg0);

static void Actor01900_Fn0A868(Task* arg0);

static void Actor01900_Fn0A914(Task* arg0);

static void Actor01900_Fn0A9C0(Task* arg0);

static void Actor01900_Fn0AA78(Task* arg0);

static void Actor01900_Fn0ABA0(Enemy* enemy, Task* task);

static __inline__ void Actor01900_MoveForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_StepForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_StepForwardHead(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_ResetYaw(GfxCoord* coord);

static void            Actor01900_Fn01950(Task* arg0);
static s32             Actor01900_Fn01A7C(Actor01900Work* work);
static __inline__ void Actor01900_BindMatrices(Task* actor);
static void            Actor01900_Fn02018(Enemy* enemy, Task* actor);
static __inline__ s32  Actor01900_FindHit(WorldCollisionContact* records, SVECTOR* pos);
static __inline__ s32  Actor01900_ArmIfPlayerLevel(Task* arg0);
static __inline__ s32  Actor01900_HasHeightClamp(GameLocationKey* session);
static s32             Actor01900_Fn03C98(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static __inline__ s32  Actor01900_HasHit(WorldCollisionContact* records);
static void            Actor01900_Fn09D3C(Enemy* enemy, Task* actor);

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `actorMoveForwardNonzero`.
static __inline__ void Actor01900_MoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gteVec                        = vec;
        if (amount != 0) {
            gfxReadMatrixZAxis(&coord->coord, vec);
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

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen, without `Actor01900_MoveForward`'s zero-amount guard. Same body as
/// `actorMoveForward`.
static __inline__ void Actor01900_StepForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += vec->vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `Actor01900_StepForward` with the X component read back through `head`,
/// as `Actor01900_MoveForward` does.
static __inline__ void Actor01900_StepForwardHead(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
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

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale.
static __inline__ void Actor01900_ResetYaw(GfxCoord* coord)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    coord->coord.m[2][2] = (u16)blk->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/player_detection_sight.inc.c"

/// Advances the actor's animation one tick. Joints 1-10 are sampled from both
/// the main and the blend animation and passed to `Gp_AnimWritePoseCopy` with
/// weights `field_8AC` and 0x1000 - `field_8AC`; joints 11-18 tick the main
/// animation alone. The per-joint rates come from `field_8A2` and `field_8AA`.
static void Actor01900_Fn01950(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    Actor01900Work*   work;

    work   = (Actor01900Work*)((Actor01900Work*)arg0->work);
    weight = work->field_8AC;
    anim   = &work->rig.anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->field_8AA;
            work->rig.slots[i].rate   = (work->field_8A2 - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->field_8A2 - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

static s32 Actor01900_Fn01A7C(Actor01900Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 7:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0xF) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x14) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
        case 3:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0x24) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x2C) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            work->field_8B4 = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        case 4:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0xC && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            work->field_8B4 = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        case 11:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            work->field_8B4 = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        default:
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

/// Per-frame animation driver: services a pending clip change, advances the
/// body and blend animations, eases the head toward its target yaw and emits
/// whatever sound event the current clip has reached.
///
/// `field_898` is the pending-change request: 1 cross-fades into `field_89E`
/// over the table's frame count, 2 restarts it outright, and both settle to 3.
/// `field_8A6` does the same for the blend animation and `field_8A8`.
static void Actor01900_Fn01C94(Task* arg0)
{
    Actor01900Work* work;
    Actor01900Work* w1;
    Actor01900Work* w2;
    Actor01900Work* w3;
    Enemy*          enemy;
    s16             cur;
    s16             dst;
    s16             raw;
    s32             clamped;
    s32             i;
    s32             i2;
    s32             i3;
    s32             i4;
    s32             snd;
    s32             id;
    s32             pan;
    u16             cur_u;
    u16             dst_u;

    work  = (Actor01900Work*)((Actor01900Work*)arg0->work);
    enemy = arg0->spawnArg2.pointer;
    if (work->field_898 == 1) {
        w1 = work;
        if (work->field_89C != work->field_89E) {
            for (i = 1; i < 0x13; i++) {
                w1->rig.slots[i].rate = w1->field_8A2;
                animationSeekSlotWithBlend(&w1->rig.anim, i, w1->field_89E, 0,
                                           (s32)Actor01900_D16988[w1->field_89C][w1->field_89E]);
            }
            /* Keeps this store from being merged with the identical one the
               `field_898 == 2` path makes just below. */
            w1->field_89C = (s16)(u16)w1->field_89E;
        }
        goto block_9;
    }
    if (work->field_898 == 2) {
        w2 = work;
        i2 = 1;
        do {
            w2->rig.slots[i2].rate = w2->field_8A2;
            animationResetSlot(&w2->rig.anim, i2, w2->field_89E);
            i2++;
        } while (i2 < 0x13);
        w2->field_89C = (s16)(u16)w2->field_89E;
    block_9:
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    }
    if (work->field_8A6 == 2) {
        i3            = 1;
        w1            = (Actor01900Work*)((Actor01900Work*)arg0->work);
        w1->field_8AA = 0x30;
        w1->field_8AC = 0x800;
        do {
            w1->rig.slots[i3].rate = w1->field_8AA;
            animationResetSlot(&w1->blend.anim, i3, w1->field_8A8);
            i3++;
        } while (i3 < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if (work->field_89A == 0) {
        w3 = (Actor01900Work*)((Actor01900Work*)arg0->work);
        i4 = 1;
        do {
            w3->rig.slots[i4].rate = w3->field_8A2;
            animationTickSlot(&w3->rig.anim, i4);
            i4++;
        } while (i4 < 0x13);
    } else {
        Actor01900_Fn01950(arg0);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->field_89A = 0;
        }
    }
    dst   = work->field_8AE;
    cur   = work->field_8B0;
    dst_u = (u16)work->field_8AE;
    cur_u = (u16)work->field_8B0;
    if (dst > cur) {
        if ((dst - cur) >= 0x101) {
            work->field_8B0 = cur_u + 0x100;
        } else {
            goto block_25;
        }
    } else if ((cur - dst) >= 0x101) {
        work->field_8B0 = cur_u - 0x100;
    } else {
    block_25:
        work->field_8B0 = (s16)dst_u;
    }
    raw     = work->field_8B0;
    clamped = (u16)work->field_8B0;
    if (raw != 0) {
        if (raw >= 0x401) {
            clamped = 0x400;
        }
        if (raw < -0x400) {
            clamped = -0x400;
        }
        ActorContact_TurnJoint(arg0->extra.tmd->coords + 5, (s16)(((s16)clamped * 2) / 3));
        ActorContact_TurnJoint(arg0->extra.tmd->coords + 2,
                               (s16)((s32)((s16)clamped + ((u32)(clamped << 0x10) >> 0x1F)) >> 1));
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    snd = Actor01900_Fn01A7C((Actor01900Work*)work);
    if (snd != 0) {
        id  = snd | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

/// Binds the actor model's light and colour matrices to the pair kept in its
/// work block.
static __inline__ void Actor01900_BindMatrices(Task* actor)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_BB0;
    obj->colorMtx = &work->field_BD0;
}

/// Enemy init: allocates the work block, sets up both animation contexts,
/// the three hit/body `WorldCollisionBody` nodes and the patrol points, then picks the
/// starting state from the spawn flags and rescales the model.
static void Actor01900_Fn02018(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    Actor01900Work*     work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xC9C, 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = Actor01900_Fn0A6CC;
    Actor01900_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)Actor01900_D0AC54.hpMax;
    enemy->param                  = &Actor01900_D0AC54;
    enemy->recs                   = &work->field_8E8;
    animationInitContext(&((Actor01900Work*)work)->rig.anim, Actor01900_D17174, obj,
                         ((Actor01900Work*)work)->rig.poses, ((Actor01900Work*)work)->rig.slots);
    animationInitContext(&((Actor01900Work*)work)->blend.anim, Actor01900_D17174, obj,
                         ((Actor01900Work*)work)->blend.poses, ((Actor01900Work*)work)->blend.slots);
    work->field_898 = 2;
    work->field_89A = 0;
    work->field_89E = 2;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    Actor01900_Fn01C94(actor);

    work->field_A08.context.contacts = &work->field_A28;
    work->field_A08.coord            = root;
    work->field_A08.pos.vx           = 0;
    work->field_A08.pos.vy           = -0x100;
    work->field_A08.pos.vz           = 0;
    work->field_A08.key              = 0x30013;
    work->field_A08.radius           = 0x180;
    work->field_A08.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_A08);
    work->field_C10       = 0;
    work->field_A08.flags = (work->field_A08.flags | WORLD_COLLISION_BODY_GRID_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_InitRec18Table(work->field_A08.context.contacts, 0xC, 0);

    body                   = &work->field_8C8;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = &work->field_8E8;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30000;
    body->radius           = 0x180;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, 0xC, 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->field_B48;
    head->coord            = &actor->extra.tmd->coords[4];
    head->context.contacts = &work->field_B68;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x180;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->context.contacts, 1, 0);
    work->field_B48.key = Gp_PackObjPair(enemy, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = Actor01900_D1728C;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &actor->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    kind                       = actor->spawnArg1.value >> 16;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x17;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->field_C2C = Actor01900_D0AC64[0].field_0;
            work->field_C2E = Actor01900_D0AC64[0].field_2;
            work->field_C30 = Actor01900_D0AC64[0].field_4;
            work->field_C32 = Actor01900_D0AC64[0].field_6;
            break;
        case 1:
            work->field_C2C = Actor01900_D0AC64[2].field_0;
            work->field_C2E = Actor01900_D0AC64[2].field_2;
            work->field_C30 = Actor01900_D0AC64[2].field_4;
            work->field_C32 = Actor01900_D0AC64[2].field_6;
            break;
        case 0:
        default:
            work->field_C2C = Actor01900_D0AC64[1].field_0;
            work->field_C2E = Actor01900_D0AC64[1].field_2;
            work->field_C30 = Actor01900_D0AC64[1].field_4;
            work->field_C32 = Actor01900_D0AC64[1].field_6;
            break;
    }

    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->field_C98 = 0;
    actor->state++;
}

/// Spawns the hit-reaction effect for a blow arriving at `yaw`: carves one
/// `SVECTOR` off the scratch head, fills it with one of the twelve presets in
/// `Actor01900_D1722C` picked from the magnitude and sign of `yaw` plus a
/// random draw, hands it to `func_800FDB18` together with the parameter of
/// `id`, and releases the scratch again.
static void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id)
{
    SVECTOR*        dir;
    s32             absAng;
    Actor01900Work* work;

    dir    = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    absAng = (yaw >= 0) ? yaw : -yaw;
    work   = arg0->work;
    if (absAng < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *dir = Actor01900_D1722C[0];
                break;
            case 1:
                *dir = Actor01900_D1722C[1];
                break;
            case 2:
                *dir = Actor01900_D1722C[2];
                break;
            case 3:
                *dir = Actor01900_D1722C[3];
                break;
            default:
                *dir = Actor01900_D1722C[4];
                break;
        }
    } else if (absAng > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *dir = Actor01900_D1722C[5];
                break;
            case 1:
                *dir = Actor01900_D1722C[6];
                break;
            default:
                *dir = Actor01900_D1722C[7];
                break;
        }
    } else if (yaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[8];
        } else {
            *dir = Actor01900_D1722C[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[10];
        } else {
            *dir = Actor01900_D1722C[11];
        }
    }
    work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, &arg0->extra.tmd->coords[dir->pad], dir, &work->field_8B8);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// First `WorldCollisionContact` among the twelve at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor01900_FindHit(WorldCollisionContact* records, SVECTOR* pos)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static void Actor01900_Fn02A50(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    Actor01900Work*  work;
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
    if (enemy->hp > 0) {
        head  = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s     = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->id = Actor01900_FindHit(&work->field_8E8, &head[-1].hitPos);
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            work->field_C40                       = 0;
            work->field_C42                       = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = arg0->extra.tmd->coords->workm.t[1];
            s->dir.vz = arg0->extra.tmd->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            Actor01900_Fn02664(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                gfxReadMatrixZAxis(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_C14 > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
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
                if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                    damage    = s->damage * 2;
                    s->damage = damage;
                    if (damage != 0) {
                        s->effect = 4;
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_C12 += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD) && config->hp > 0 && work->field_C44 == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_C10 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    state = work->field_0;
                    if (state == 0x13 || state == 0xF || state == 4 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else if (work->field_C12 >= 0x4C || s->crit == 1) {
                        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        state                  = work->field_0;
                        if (state != 0x13 && state != 0x1F && state != 0x11) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state != 0x11 && state != 4) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    } else {
                        work->field_0 = 4;
                    }
                    break;
                case 3:
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags  &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 8:
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    }
                    break;
                case 9:
                    work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
            }
            work->field_C14 = 5;
        } else if (work->field_C14 <= 0) {
            work->field_C12 = 0;
        } else {
            work->field_C14--;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                enemy->hp             -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                state = work->field_0;
                if (state == 7 || state == 0xB || state == 0x12 || state == 0x1B) {
                    work->field_0 = 5;
                } else if (state == 4) {
                    work->field_2 = -1;
                } else if (state != 0xF) {
                    if (state == 0x13 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if (work->field_89E == 2 || work->field_89E == 3) {
                        work->field_0 = 0x1E;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 == 4) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11) {
                    work->field_0 = 0x13;
                }
            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                work->field_0 = 0x1F;
            } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11 && work->field_0 != 0x15 && work->field_0 != 0 && work->field_0 != 0x1D && work->field_0 != 0x1E) {
                work->field_0 = 0x13;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}

static void Actor01900_Fn03710(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;
    TmdObject*      obj;
    s32             step;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x17;
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            Actor01900_Fn01C94(arg0);
        } while ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) < 6U);
        work->field_8A2 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    step                                  = (s16)work->field_8A2 / 2;
    work->field_8A2                       = (u16)step;
    if (step == 1) {
        work->field_8A2 = -0x10;
    }
    if ((s16)work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    Actor01900_Fn01C94(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_0         = 0x11;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

/// Raises the player's weapon when the player is not already in state 2 and
/// stands within 0x1F4 of the actor in Y. Nonzero when it armed.
static __inline__ s32 Actor01900_ArmIfPlayerLevel(Task* arg0)
{
    Task* player;
    s32   dy;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        dy = arg0->extra.tmd->coords->coord.t[1] - player->extra.tmd->coords->coord.t[1];
        if (ABS(dy) < 0x1F4) {
            Gp_ArmStateF0(1);
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers, arms the player if
/// they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x10 and rescales it by 0x1194.
static void Actor01900_Fn03854(Task* arg0)
{
    Actor01900Work*    work;
    ActorChaseScratch* yaw;
    GfxCoord*          coord;
    TmdObject*         obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 1;
        work->field_8A2       = 0x10;
        work->field_89E       = 9;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A08.flags = (u16)(work->field_A08.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
        work->field_8C8.radius = 0x180;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
        yaw                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->rig.slots[1].status.fields.flags & 0x100) {
            work->field_0 = 7;
        }
        yaw->turn       = actorPositionYaw(arg0, &yaw->delta, &gPlayerStatus);
        work->field_8AE = yaw->turn;
        if (yaw->turn >= 0x11) {
            yaw->turn = 0x10;
        }
        if (yaw->turn < -0x10) {
            yaw->turn = -0x10;
        }
        coord      = arg0->extra.tmd->coords;
        yaw->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, yaw->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        Actor01900_Fn01C94(arg0);
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
    }
}

static void Actor01900_Fn03C04(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
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

/// `Actor01900_Fn03C04`'s row scan without the clamp: nonzero when the
/// current room has an `Actor01900_D172CC` row.
static __inline__ s32 Actor01900_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

static s32 Actor01900_Fn03C98(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorStepDelta* head;
    ActorStepDelta* s;
    ActorStepDelta* blk;
    s16             vy;
    SVECTOR*        step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                                 = SCRATCH_STACK_CURSOR(ActorStepDelta);
    blk                                  = head - 1;
    SCRATCH_STACK_CURSOR(ActorStepDelta) = blk;
    s                                    = blk;
    s->moved                             = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.fixed.vx.word >> 16;
        s->step.vy = s->delta.fixed.vy.word >> 16;
        s->step.vz = s->delta.fixed.vz.word >> 16;
        if (Actor01900_HasHeightClamp(&gGameSession->location.loc)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x180) {
                s->step.vy = (vy <= 0) ? -0x180 : 0x180;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xC0) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xC0);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.fixed.vx.word & 0xFFFF) {
            if (s->delta.fixed.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.fixed.vz.word & 0xFFFF) {
            if (s->delta.fixed.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor01900_HasHeightClamp(&gGameSession->location.loc)) {
        Actor01900_Fn03C04(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.fixed.vx.word != 0 || s->delta.fixed.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorStepDelta);
    return s->moved;
}

/// Pushes the actor's root coordinate by half of each nearby kind 0x10000 /
/// 0x30000 record's offset, walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update is load-bearing: loop.c counts both copies
/// before cross-jumping merges them, which keeps `count`'s sign extension in the loop.
static s32 Actor01900_Fn03FF8(Task* arg0, WorldCollisionContact* recs, s16 count)
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
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            worldCollisionCalcContactViewOffset(&s->pos, &recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0xC0) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0xC0);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPushScratch);
    return s->hit;
}

/// Circling state: turns toward the player at most 0x30 per step while walking,
/// switching to state 0xA when lined up and far enough, 0xB when close and in
/// front, or 0x1B after 0x5B steps.
static void Actor01900_Fn042BC(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* s;
    s32                diff;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x42;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8C8.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    work->field_6++;
    work->field_8++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    s = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (Actor01900_Fn03C98(arg0->extra.tmd->coords, &work->field_A28, 0xC, 0x60) != 1) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_8E8, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    s->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                          (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    s->yaw          = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw          = actorNormalizeYaw(s->yaw);
    coord           = arg0->extra.tmd->coords;
    s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->turn;
    diff            = s->yaw - s->playerYaw;
    if (ABS(diff) < 0x44 && work->field_C30 + work->field_C42 / 2 < work->field_6 && ABS(s->turn) < 0x80) {
        if (overlayOutOfRange(&s->delta, 0x708)) {
            work->field_0 = 0xA;
        }
    }
    if (detectSightBlocked(arg0) != 1) {
        work->field_6++;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!overlayOutOfRange(&s->delta, 0x2BC)) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C28 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C28 = -1;
            } else {
                work->field_C28 = 1;
            }
        }
        if (work->field_C28 == 1) {
            s->turn += 0x300;
        } else {
            s->turn -= 0x300;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C28 = -work->field_C28;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = arg0->extra.tmd->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
        } else {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
        }
    } else if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_C37 != 0) {
        work->field_C37--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn04D14(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* s;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0xC0;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->field_C26   = 8;
        work->field_6     = 0;
        work->field_8     = 0;
        Actor01900_D172FC = 0;
        work->field_C40++;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    s                                     = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                               (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = actorNormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->extra.tmd->coords;
    s->turn = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                                 = arg0->extra.tmd->coords;
    work->field_8AE                       = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_C24                       = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C24 = work->field_C24 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C24 = 2;
    }
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->field_C24);
    Actor01900_D172FC += work->field_C24;
    if (work->field_C26 == 8 && work->field_8A2 >= 0x18) {
        work->field_C26 = -1;
    }
    if (work->field_C26 == -1 && work->field_8A2 == 0x12) {
        work->field_C26 = 0;
        work->field_6   = 0;
    }
    if (work->field_C26 == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                  (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = actorNormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw <= 0x400) {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += work->field_C26;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn0551C(Task* arg0)
{
    Actor01900Work*    work;
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
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing          = arg0->extra.tmd->coords;
        s->angle        = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C20 = s->angle;
        work->field_C22 = s->angle + (u16)s->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    s                                       = head - 1;
    Actor01900_Fn01C94(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (work->field_C20 == work->field_C22) {
        if (work->field_C40 < 2 || overlayOutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        }
    }
    if (work->field_C20 > work->field_C22) {
        work->field_C20 -= 0x89;
        if (work->field_C20 < work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    if (work->field_C20 < work->field_C22) {
        work->field_C20 += 0x89;
        if (work->field_C20 > work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->field_C20, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn05B4C(Task* arg0)
{
    Actor01900Work*    work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;

    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    work                                    = arg0->work;
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0xC0;
        work->field_6          = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C28 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C28 = 1;
            } else {
                work->field_C28 = -1;
            }
        }
        if (work->field_C28 == 1) {
            work->field_89E = 0x15;
            if (work->field_C42 == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->field_C2E + angle;
            } else {
                aim->turn += work->field_C2E;
            }
            work->field_C28 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C42 == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->field_C2E;
            } else {
                aim->turn -= work->field_C2E;
            }
            work->field_C28 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        Actor01900_Fn01C94(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->field_C18;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C2A = 0xDE;
        work->field_C42++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C2A);
        gte_ldsv(&work->field_C18);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C2A >> 1);
        gte_ldsv(&work->field_C18);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 0) {
            work->field_C2A >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
        work->field_2 = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn05F38(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;
    GfxCoord*       coord;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        work->field_6   = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 0x10) < 7U) {
        coord = arg0->extra.tmd->coords;
        Actor01900_StepForward(coord, -0x78);
        ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (enemy->node.state.parts.targeted == 1) {
            work->field_0 = 10;
        } else {
            work->field_0 = 6;
        }
    }
}

static void Actor01900_Fn06100(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 8;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    coord           = arg0->extra.tmd->coords;
    aim->turn       = actorNormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = aim->turn;
    if (aim->turn < 0x200) {
        overlayOutOfRange(&aim->delta, 0x384);
    }
    if (aim->turn > 0x40) {
        aim->turn = 0x40;
    }
    if (aim->turn < -0x40) {
        aim->turn = -0x40;
    }
    facing     = arg0->extra.tmd->coords;
    aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn06634(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8C8.radius        = 0x180;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_89E               = 0xA;
        work->field_89A               = 0;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0 && work->field_C34[0] != 1 && work->field_C34[1] != 3 && work->field_C34[2] != 2) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->field_89E == 0xA) {
        Actor01900_StepForwardHead(arg0->extra.tmd->coords, -0x57);
    }
    Actor01900_Fn01C94(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_8E8, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            Actor01900_Fn01C94(arg0);
        }
        if ((work->rig.slots[1].status.fields.flags & 0x100) && work->field_89E == 0xB) {
            work->field_8C8.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

static void Actor01900_Fn06904(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;
    TmdObject*      obj;
    s16             cur;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj->flags                    = 0;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_6                 = 0;
    }
    if (work->field_6 < 0x401) {
        switch ((s16)(work->field_6++ - 0x18)) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0x13);
                break;
            case 5:
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 17:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 39:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
        }
    }
}

/// Arms `gSceneCombatState` and returns 1 when the player is within 500 units of the
/// actor's height (and not in `field_954` state 2).
static void Actor01900_Fn06B4C(Task* arg0)
{
    SVECTOR         delta;
    SVECTOR*        d;
    Actor01900Work* work;
    Enemy*          enemy;
    TmdObject*      obj;
    GfxCoord*       coord;
    s32             sound;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                   = arg0->extra.tmd;
        Actor01900_D17174[16] = &_gActor01900Actor101900Animation16960;
        work->field_89E       = 0x10;
        work->field_898       = 2;
        obj->flags            = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius        = 0x180;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x10;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        work->field_894               = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xF && work->field_894 != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) &&
        (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 9, 0, 0)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sound           = 0x51090009;
        if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
            sound = 0x51090008;
        }
        switch ((u8)Gp_GetViewIndex()) {
            case 2:
                SndEvt_EnqueueType6(sound, 0x64, 0);
                break;
            case 3:
                SndEvt_EnqueueType6(sound, 0x50, 0x1F);
                break;
            case 4:
            default:
                SndEvt_EnqueueType6(sound, 0x40, 0x4C);
                break;
        }
    }
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 5 && work->field_894 != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        work->field_8B8.coord      = arg0->extra.tmd->coords + 1;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 3, 0, 0) || (u8)Gp_GetViewIndex() != 0x10) {
            func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->field_8B8);
        }
    }
    work->field_894 = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy           = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, work->field_C32)) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
        if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
            work->field_0 = 6;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x20 per step and swapping waypoints on arrival or after 0x15 steps; switches
/// to state 6 when the player comes within `field_C32`, or within 0xFA0 and in
/// front.
static void Actor01900_Fn06F40(Task* arg0)
{
    Actor01900Work*   work;
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
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8C8.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    s           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    s->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    s->delta.vy = 0;
    s->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    Actor01900_Fn01C94(arg0);
    coord           = arg0->extra.tmd->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
    }
    if ((arg0->spawnArg1.value >> 16) != 0x10) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) == 1 && ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    } else {
        if ((ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) == 1 ||
             ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_8E8, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (detectSightBlocked(arg0) != 1) {
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
        if (!overlayOutOfRange(&s->delta, work->field_C32)) {
            if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                work->field_0 = 6;
            }
        } else if (!overlayOutOfRange(&s->delta, 0xFA0)) {
            coord    = arg0->extra.tmd->coords;
            s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                    work->field_0 = 6;
                }
            }
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void Actor01900_Fn07810(Task* arg0)
{
    Actor01900Work*   work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2.pointer;
        obj             = arg0->extra.tmd;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius        = 0x180;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn            = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
    work->field_8AE = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->field_C24);
    if (work->field_C24 > 0) {
        next            = work->field_C24 - 0xA;
        work->field_C24 = next;
        if ((s16)next < 0) {
            work->field_C24 = 0;
        }
    }
    Actor01900_Fn01C94(arg0);
    if ((work->rig.slots[1].status.fields.flags & 0x100) || work->field_C24 == 0) {
        work->field_0 = 9;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void Actor01900_Fn07BA8(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        return;
    }
    Actor01900_Fn01C94(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        Actor01900_Fn01C94(arg0);
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 0x11) {
        work->field_6++;
        Actor01900_StepForward(arg0->extra.tmd->coords, -0x10);
        if (ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn080A8(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->field_0 = 7;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (aim->turn > 0) {
        aim->turn = 0;
    }
    if (aim->turn < 0) {
        aim->turn = 0;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    Actor01900_Fn01C94(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn the actor toward the player at up to 0x28 per call. Takes a 0x10-byte
/// scratch block from the scratch stack for the offset to the player and the
/// yaw, steps `field_8AE` toward that yaw, then rebuilds the root coordinate's
/// Y rotation from its own facing. The `field_4` branch is the state's entry.
static void Actor01900_Fn083E8(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Actor01900_Fn01C94(arg0);
        Actor01900_Fn01C94(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->field_8AE < aim->turn) {
        if (aim->turn - work->field_8AE > 0x28) {
            work->field_8AE += 0x28;
        } else {
            work->field_8AE = aim->turn;
        }
    } else if (work->field_8AE - aim->turn > 0x28) {
        work->field_8AE -= 0x28;
    } else {
        work->field_8AE = aim->turn;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->field_898 = 2;
    Actor01900_Fn01C94(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn08724(Task* arg0)
{
    SVECTOR         vec;
    EffectWork*     eff;
    Actor01900Work* work;
    Enemy*          enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_8C8.radius        = 0x180;
        work->field_A08.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0x13);
    }
    work->field_6++;
    switch (work->field_6) {
        case 3:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vz                   = 0x64;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
            goto body;
        case 4:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec);
        body:
            if (eff != NULL) {
                actorTintTask(eff->task, enemy);
            }
            break;
    }
    if (work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

static void Actor01900_Fn0892C(Task* arg0)
{
    SVECTOR         vec;
    EffectWork*     eff;
    Actor01900Work* work;
    Enemy*          enemy;
    s16             cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8C8.radius        = 0x180;
        work->field_A08.flags         = (u16)(work->field_A08.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        work->field_89E               = 2;
        work->field_898               = 1;
        work->field_8A2               = 0x10;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    work->field_6++;
    switch (work->field_89E) {
        case 2:
            if (work->field_6 >= 0x10 && (work->rig.slots[1].status.fields.flags & 2)) {
                work->field_89E = 0x18;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            Actor01900_StepForwardHead(arg0->extra.tmd->coords, 0xA);
            ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC);
            if (work->field_6 == 3) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
                actorTintEffect(eff, enemy);
            }
            if (work->field_6 == 5) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL);
                actorTintEffect(eff, enemy);
            }
            break;
        case 0x18:
            if (!(work->rig.slots[1].status.fields.flags & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0x13);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 39:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_0          = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 2);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 3);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 4);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 5);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 6);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 7);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 8);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 9);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 10);
}

/// Whether any of the three `WorldCollisionContact` at `records` carries an id with high
/// word 1, stopping at the first empty record.
static __inline__ s32 Actor01900_HasHit(WorldCollisionContact* records)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers and arms the player
/// if they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x30, rescales it by 0x1194, and once the
/// actor is out of range of the player hands the work state on.
static void Actor01900_Fn09694(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 4;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6          = 0;
        work->field_8          = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    work->field_6++;
    if (work->field_6 == 0x16) {
        work->field_B48.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->field_6 == 0x1D) {
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (Actor01900_HasHit(&work->field_B68) == 1) {
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    if (work->field_6 < 0xE) {
        coord = arg0->extra.tmd->coords;
        aim->turn =
            actorNormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        facing     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (overlayOutOfRange(&aim->delta, 0x2BC)) {
            work->field_0 = 6;
        } else {
            work->field_0 = 0xE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn09BE8(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8C8.radius        = 0x180;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0xB;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    Actor01900_Fn01C94(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_8E8, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, &work->field_A28, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->field_8C8.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

static void Actor01900_Fn09D3C(Enemy* enemy, Task* actor)
{
    VECTOR               pos;
    Actor01900StateTable states;
    Actor01900Work*      work;
    ActorViewScratch*    scratch;
    ActorViewScratch*    head;
    s32                  state;

    work   = actor->work;
    states = Actor01900_D001BC;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
    }

    head                                   = SCRATCH_STACK_CURSOR(ActorViewScratch);
    SCRATCH_STACK_CURSOR(ActorViewScratch) = head - 1;
    scratch                                = head - 1;

    if (work->field_C10 > 0) {
        work->field_C10 = (s16)((u16)work->field_C10 - 1);
    } else {
        Actor01900_Fn02A50(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    state         = work->field_0;
    if ((state == 0x1C) || (state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x1E)) {
        work->field_8C8.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_8C8.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    states.fn[work->field_0](actor);
    Gp_ClearRec18Occupied(&work->field_A28);
    Gp_ClearRec18Occupied(&work->field_8E8);
    Gp_ClearRec18Occupied(&work->field_B68);
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);

    work->field_C48[work->field_C98].vx = scratch->pos.vx;
    work->field_C48[work->field_C98].vy = scratch->pos.vy;
    work->field_C48[work->field_C98].vz = scratch->pos.vz;

    SCRATCH_STACK_RELEASE_BYTES(0x18);
    work->field_C98 = (u16)work->field_C98 + 1;
    if (work->field_C98 == 7) {
        work->field_C98 = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_C48[work->field_C98].vx;
        enemy->bodyPos.vy = work->field_C48[work->field_C98].vy;
        enemy->bodyPos.vz = work->field_C48[work->field_C98].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

s32 Actor01900_Fn0A314(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The actor's state handlers, indexed by `Actor01900Work::field_0`; empty
/// slots are states the actor never enters. `Actor01900_Fn09D3C` copies the
/// table to its frame before dispatching.
static const Actor01900StateTable Actor01900_D001BC = { {
    Actor01900_Fn0A764,
    Actor01900_Fn0A7C0,
    Actor01900_Fn0A868,
    Actor01900_Fn0A914,
    Actor01900_Fn03710,
    Actor01900_Fn0A9C0,
    Actor01900_Fn03854,
    Actor01900_Fn042BC,
    Actor01900_Fn04D14,
    Actor01900_Fn0551C,
    Actor01900_Fn05B4C,
    Actor01900_Fn09694,
    NULL,
    NULL,
    Actor01900_Fn05F38,
    Actor01900_Fn0AA78,
    NULL,
    Actor01900_Fn0AB1C,
    Actor01900_Fn06100,
    Actor01900_Fn06634,
    NULL,
    Actor01900_Fn06904,
    NULL,
    Actor01900_Fn06B4C,
    Actor01900_Fn06F40,
    Actor01900_Fn07BA8,
    Actor01900_Fn07810,
    Actor01900_Fn080A8,
    Actor01900_Fn083E8,
    Actor01900_Fn08724,
    Actor01900_Fn0892C,
    Actor01900_Fn09BE8,
} };

/// The actor task's dispatcher table, indexed by `Task::state`: spawn
/// (`Actor01900_Fn02018`), a three-frame wait (`Actor01900_Fn0ABA0`), the
/// per-frame tick (`Actor01900_Fn09D3C`) and teardown.
static const EnemyTaskFuncTable4 Actor01900_D0023C = { {
    Actor01900_Fn02018,
    Actor01900_Fn0ABA0,
    Actor01900_Fn09D3C,
    enemyDestroy,
} };

s32 Actor01900_Fn0A31C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    Actor01900Work* work = arg0->work;

    switch (arg2->animationId) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

s32 Actor01900_Fn0A59C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 1;
}

s32 Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    u16             room;
    u16             state;
    u16             state2;
    Actor01900Work* work;

    work               = arg0->work;
    work->field_C34[0] = ((u8*)arg2)[0];
    work->field_C34[1] = ((u8*)arg2)[1];
    work->field_C34[2] = ((u8*)arg2)[2];
    room               = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                       = 0x1C;
                arg0->extra.tmd->coords->coord.t[0] = -0x595;
                arg0->extra.tmd->coords->coord.t[1] = 0;
                arg0->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

static void Actor01900_Fn0A6CC(Task* task)
{
    Actor01900Work* work;
    Enemy*          enemy;

    work  = (Actor01900Work*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->field_C38 != NULL) {
            taskKill(work->field_C38);
        }
        if (work->field_C3C != NULL) {
            taskKill(work->field_C3C);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}

static void Actor01900_Fn0A764(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->field_B48.flags                                     = (u16)(work->field_B48.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A08.flags                                     = (u16)(work->field_A08.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static void Actor01900_Fn0A7C0(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A08.flags = (u16)(work->field_A08.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
    }
}

static void Actor01900_Fn0A868(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A08.flags = (u16)(work->field_A08.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
    }
}

static void Actor01900_Fn0A914(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A08.flags = (u16)(work->field_A08.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
    }
}

static void Actor01900_Fn0A9C0(Task* arg0)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x12;
        work->field_89E        = 0xD;
        work->field_89A        = 0;
        work->field_B48.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->field_0 = 7;
    }
}

static void Actor01900_Fn0AA78(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8C8.radius        = 0x180;
        work->field_B48.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A08.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 8;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        work->field_8A2               = work->field_8A4;
    }
    Actor01900_Fn01C94(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->field_0 = 7;
    }
}

static void Actor01900_Fn0AB1C(Task* arg0)
{
    Actor01900Work* work;
    Enemy*          enemy;
    u32             rng;
    s16             timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        work->field_6   = ((rng >> 16) & 0xF) + work->field_C2C;
    }
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if (timer < 0) {
        work->field_0 = 0xF;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

static void Actor01900_Fn0ABA0(Enemy* enemy, Task* task)
{
    u16             count;
    Actor01900Work* work;

    work          = (Actor01900Work*)task->work;
    count         = work->field_6 + 1;
    work->field_6 = count;
    if ((s16)count >= 3) {
        task->state++;
    }
}

void Actor01900_Fn0ABE4(Task* arg0)
{
    EnemyTaskFuncTable4 sp;

    sp = Actor01900_D0023C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
