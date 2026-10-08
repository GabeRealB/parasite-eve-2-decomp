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
#include "gameplay/message.h"
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
// Which of the two Odd Stranger builds this package is (odd_stranger.h).
#define ODD_STRANGER_VARIANT 2
#include "../../shared/odd_stranger.h"

static void _oddStrangerTick(Enemy* enemy, Task* actor);

static const OddStrangerStateTable gOddStrangerStates;

/// Payload of the `0x3FF` message `_oddStrangerGrabStrike` sends: the same
/// 0x14-byte animation record other actors keep as `AnimationPlayRequest` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern AnimationPlayRequest gOddStrangerPlayerAnim;

extern AnimationSet* D_actor_401800_801559F8[];
extern AnimationSet* D_actor_401800_801559F0[];

/// Twelve `SVECTOR` hit positions `_oddStrangerSpawnHitEffect` picks from by
/// the hit bearing: offsets 0..3 for a front hit, 5 or 7 for a rear hit,
/// and 8..11 for the signed side sectors. The fourth halfword (`pad`, unused by the effect itself) is the
/// model part index `effectSpawnHit` anchors the spawned effect to. Same role
/// `Actor00100_D1B9F4` plays for `Actor00100_Fn03340`.
extern SVECTOR gOddStrangerHitOffsets[12];

/// Animation bank both `animationInitContext` contexts are initialised from. Same
/// role `Actor01900_D17174` plays for actor 01900.
extern AnimationSet* gOddStrangerAnimSets[46];

/// Enemy description record the init body copies `hpMax` out of into
/// `Enemy.hp` and points `Enemy.param` at. Same role
/// `D_actor_401300_80141FA0` plays for actor 401300.
extern EnemyParams D_actor_401800_8013E6F0;

/// The three `ActorStrangerVariant` tunings the init body picks from by the
/// spawn argument's low nibble: `[0]` when it is 2, `[2]` when it is 1, `[1]`
/// otherwise. Same table shape as `Actor01900_D0AC64`.
extern ActorStrangerVariant D_actor_401800_8013E700[];

/// Handler table the actor's task receives in `Task::msgTable`; same role
/// `Actor01900_D1728C` plays for actor 01900.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_401800_80155A80[8];

/// Payload `_oddStrangerGrabPull` fills and sends with message 0x3E9.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Frame counter the chase body of `_oddStrangerCircleDash` accumulates its step
/// `slideStep` into and the init body clears; the aim-and-rescale body reads it
/// back as the phase of the step it walks. Same role `Actor01900_D172FC` plays
/// for actor 01900.
extern u16 gOddStrangerChaseDistance;

/// The block `_oddStrangerDormantScripted` posts into `gOddStrangerAnimSets[16]`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `gActor401300Animation20D98`.
extern AnimationSet gOddStrangerDormantAnimSet;

/// Gameplay slot `effectSpawn` effects read their model data from; set before
/// each spawn in `_actor401800BurstDeath`.

/// The records closing three of the overlay's model streams, which
/// `_actor401800BurstDeath` points `D_80114B34[5].data.model` at before spawning: the
/// body-part effects on updates 3, 5, 7 and 9. The entry particle uses no model
/// from this slot.
extern TmdSource gOddStrangerBurstModelA;
static TmdSource _gActor401800Model12280;
extern TmdSource gOddStrangerBurstModelC;

#include "../../shared/actor_contacts.h"

static void _actor401800Initialize(Enemy* enemy, Task* actor);
static void _actor401800Hidden(Task* task);
static void _actor401800RiseFront(Task* task);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Integer part of the last movement step `func_actor_401800_80132C68`
/// applied, nudged one unit outward where the step had a fractional part.
static SVECTOR ActorContact_ScratchPosition;

extern OddStrangerTransformStorage gOddStrangerGrabTransform;

extern GameActorButtonPressHold D_actor_401800_80155AF8;

/// Clip-transition table the cross-fade reads: one byte per (previous clip,
/// requested clip) pair, rows of 0x2D, handed to `animationSeekSlotWithBlend` as the
/// transition argument.
extern s8 gOddStrangerTransitions[45][45];

static AnimationSet _gActor401800Animation1F3F4;
static AnimationSet _gActor401800Animation1FDEC;
static AnimationSet _gActor401800Animation2074C;

static TmdSource _gActor401800OddStrangerBody;
static s32       _actor401800IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra);
static void      _actor401800Task(Task* task);

static AnimationSet _gActor401800Animation15118;
static AnimationSet _gActor401800Animation156F8;
static AnimationSet _gActor401800Animation15CDC;
static AnimationSet _gActor401800Animation16730;
static AnimationSet _gActor401800Animation17CE4;
static AnimationSet _gActor401800Animation18074;
static AnimationSet _gActor401800Animation188AC;
static AnimationSet _gActor401800Animation18DFC;
static AnimationSet _gActor401800Animation19D14;
static AnimationSet _gActor401800Animation1A0B8;
static AnimationSet _gActor401800Animation1A46C;
static AnimationSet _gActor401800Animation1A7F0;
static AnimationSet _gActor401800Animation1AEE4;
static AnimationSet _gActor401800Animation1B748;
static AnimationSet _gActor401800Animation1BFC0;
static AnimationSet _gActor401800Animation1C610;
static AnimationSet _gActor401800Animation1CFE4;
static AnimationSet _gActor401800Animation1DE5C;
static AnimationSet _gActor401800Animation1ED6C;
static AnimationSet _gActor401800Animation210E0;
static AnimationSet _gActor401800Animation21AB8;
static AnimationSet _gActor401800Animation224A8;

DamageAttack D_actor_401800_8013E6E8[2] = {
    { 26, 7 },
    { 18, 7 },
};

EnemyParams D_actor_401800_8013E6F0 = { D_actor_401800_8013E6E8, 180, 34, 34, 3, 100, 10, 100, 0 };

ActorStrangerVariant D_actor_401800_8013E700[3] = {
    { 40, 400, 7, 2000, { 0, 0, 0, 0 } },
    { 20, 400, 9, 2500, { 0, 0, 0, 0 } },
    { 0, 600, 5, 3000, { 0, 0, 0, 0 } },
};

static TmdBone _gActor401800OddStrangerBodySkeleton[19] = {
#include "assets/odd_stranger_body_skeleton.inc"
};

static u32 _gActor401800OddStrangerBodyPartVerts[19] = {
#include "assets/odd_stranger_body_partVerts.inc"
};

static SVECTOR _gActor401800OddStrangerBodyVerts[295] = {
#include "assets/odd_stranger_body_verts.inc"
};

static SVECTOR _gActor401800OddStrangerBodyNormals[335] = {
#include "assets/odd_stranger_body_normals.inc"
};

static u32 _gActor401800OddStrangerBodyStream[3795] = {
#include "assets/odd_stranger_body_stream.inc"
};

static TmdSource _gActor401800OddStrangerBody = {
    0,
    19204,
    7092,
    19,
    _gActor401800OddStrangerBodyPartVerts,
    _gActor401800OddStrangerBodyVerts,
    _gActor401800OddStrangerBodyNormals,
    _gActor401800OddStrangerBodySkeleton,
    _gActor401800OddStrangerBodyStream,
};

static TmdBone _gActor401800Model11D14Skeleton[1] = {
#include "assets/actor_401800_model_11D14_skeleton.inc"
};

static u32 _gActor401800Model11D14PartVerts[1] = {
#include "assets/actor_401800_model_11D14_partVerts.inc"
};

static SVECTOR _gActor401800Model11D14Verts[24] = {
#include "assets/actor_401800_model_11D14_verts.inc"
};

static SVECTOR _gActor401800Model11D14Normals[34] = {
#include "assets/actor_401800_model_11D14_normals.inc"
};

static u32 _gActor401800Model11D14Stream[218] = {
#include "assets/actor_401800_model_11D14_stream.inc"
};

TmdSource gOddStrangerBurstModelA = {
    0,
    1452,
    0,
    1,
    _gActor401800Model11D14PartVerts,
    _gActor401800Model11D14Verts,
    _gActor401800Model11D14Normals,
    _gActor401800Model11D14Skeleton,
    _gActor401800Model11D14Stream,
};

static TmdBone _gActor401800Model12280Skeleton[1] = {
#include "assets/actor_401800_model_12280_skeleton.inc"
};

static u32 _gActor401800Model12280PartVerts[1] = {
#include "assets/actor_401800_model_12280_partVerts.inc"
};

static SVECTOR _gActor401800Model12280Verts[22] = {
#include "assets/actor_401800_model_12280_verts.inc"
};

static SVECTOR _gActor401800Model12280Normals[33] = {
#include "assets/actor_401800_model_12280_normals.inc"
};

static u32 _gActor401800Model12280Stream[229] = {
#include "assets/actor_401800_model_12280_stream.inc"
};

static TmdSource _gActor401800Model12280 = {
    0,
    1488,
    0,
    1,
    _gActor401800Model12280PartVerts,
    _gActor401800Model12280Verts,
    _gActor401800Model12280Normals,
    _gActor401800Model12280Skeleton,
    _gActor401800Model12280Stream,
};

static TmdBone _gActor401800Model129F8Skeleton[1] = {
#include "assets/actor_401800_model_129F8_skeleton.inc"
};

static u32 _gActor401800Model129F8PartVerts[1] = {
#include "assets/actor_401800_model_129F8_partVerts.inc"
};

static SVECTOR _gActor401800Model129F8Verts[47] = {
#include "assets/actor_401800_model_129F8_verts.inc"
};

static SVECTOR _gActor401800Model129F8Normals[68] = {
#include "assets/actor_401800_model_129F8_normals.inc"
};

static u32 _gActor401800Model129F8Stream[451] = {
#include "assets/actor_401800_model_129F8_stream.inc"
};

TmdSource gOddStrangerBurstModelC = {
    0,
    3064,
    0,
    1,
    _gActor401800Model129F8PartVerts,
    _gActor401800Model129F8Verts,
    _gActor401800Model129F8Normals,
    _gActor401800Model129F8Skeleton,
    _gActor401800Model129F8Stream,
};

static AnimationPackedPose _gActor401800Animation13C24Bank1[28] = {
#include "assets/actor_401800_animation_13C24_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation13C24Bank4[247] = {
#include "assets/actor_401800_animation_13C24_bank4.inc"
};

static AnimationRecord _gActor401800Animation13C24Records[362] = {
#include "assets/actor_401800_animation_13C24_records.inc"
};

static u16 _gActor401800Animation13C24Indices[20] = {
#include "assets/actor_401800_animation_13C24_indices.inc"
};

static AnimationSet _gActor401800Animation13C24 = {
    _gActor401800Animation13C24Records,
    _gActor401800Animation13C24Indices,
    { NULL, _gActor401800Animation13C24Bank1, NULL, NULL, _gActor401800Animation13C24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation146A8Bank1[32] = {
#include "assets/actor_401800_animation_146A8_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation146A8Bank4[223] = {
#include "assets/actor_401800_animation_146A8_bank4.inc"
};

static AnimationRecord _gActor401800Animation146A8Records[334] = {
#include "assets/actor_401800_animation_146A8_records.inc"
};

static u16 _gActor401800Animation146A8Indices[20] = {
#include "assets/actor_401800_animation_146A8_indices.inc"
};

static AnimationSet _gActor401800Animation146A8 = {
    _gActor401800Animation146A8Records,
    _gActor401800Animation146A8Indices,
    { NULL, _gActor401800Animation146A8Bank1, NULL, NULL, _gActor401800Animation146A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation15118Bank1[24] = {
#include "assets/actor_401800_animation_15118_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation15118Bank4[248] = {
#include "assets/actor_401800_animation_15118_bank4.inc"
};

static AnimationRecord _gActor401800Animation15118Records[328] = {
#include "assets/actor_401800_animation_15118_records.inc"
};

static u16 _gActor401800Animation15118Indices[20] = {
#include "assets/actor_401800_animation_15118_indices.inc"
};

static AnimationSet _gActor401800Animation15118 = {
    _gActor401800Animation15118Records,
    _gActor401800Animation15118Indices,
    { NULL, _gActor401800Animation15118Bank1, NULL, NULL, _gActor401800Animation15118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation156F8Bank1[10] = {
#include "assets/actor_401800_animation_156F8_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation156F8Bank4[138] = {
#include "assets/actor_401800_animation_156F8_bank4.inc"
};

static AnimationRecord _gActor401800Animation156F8Records[188] = {
#include "assets/actor_401800_animation_156F8_records.inc"
};

static u16 _gActor401800Animation156F8Indices[20] = {
#include "assets/actor_401800_animation_156F8_indices.inc"
};

static AnimationSet _gActor401800Animation156F8 = {
    _gActor401800Animation156F8Records,
    _gActor401800Animation156F8Indices,
    { NULL, _gActor401800Animation156F8Bank1, NULL, NULL, _gActor401800Animation156F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation15CDCBank1[8] = {
#include "assets/actor_401800_animation_15CDC_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation15CDCBank4[151] = {
#include "assets/actor_401800_animation_15CDC_bank4.inc"
};

static AnimationRecord _gActor401800Animation15CDCRecords[182] = {
#include "assets/actor_401800_animation_15CDC_records.inc"
};

static u16 _gActor401800Animation15CDCIndices[20] = {
#include "assets/actor_401800_animation_15CDC_indices.inc"
};

static AnimationSet _gActor401800Animation15CDC = {
    _gActor401800Animation15CDCRecords,
    _gActor401800Animation15CDCIndices,
    { NULL, _gActor401800Animation15CDCBank1, NULL, NULL, _gActor401800Animation15CDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation16730Bank1[20] = {
#include "assets/actor_401800_animation_16730_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation16730Bank4[228] = {
#include "assets/actor_401800_animation_16730_bank4.inc"
};

static AnimationRecord _gActor401800Animation16730Records[353] = {
#include "assets/actor_401800_animation_16730_records.inc"
};

static u16 _gActor401800Animation16730Indices[20] = {
#include "assets/actor_401800_animation_16730_indices.inc"
};

static AnimationSet _gActor401800Animation16730 = {
    _gActor401800Animation16730Records,
    _gActor401800Animation16730Indices,
    { NULL, _gActor401800Animation16730Bank1, NULL, NULL, _gActor401800Animation16730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation17474Bank1[23] = {
#include "assets/actor_401800_animation_17474_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation17474Bank4[334] = {
#include "assets/actor_401800_animation_17474_bank4.inc"
};

static AnimationRecord _gActor401800Animation17474Records[426] = {
#include "assets/actor_401800_animation_17474_records.inc"
};

static u16 _gActor401800Animation17474Indices[20] = {
#include "assets/actor_401800_animation_17474_indices.inc"
};

static AnimationSet _gActor401800Animation17474 = {
    _gActor401800Animation17474Records,
    _gActor401800Animation17474Indices,
    { NULL, _gActor401800Animation17474Bank1, NULL, NULL, _gActor401800Animation17474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation17CE4Bank1[14] = {
#include "assets/actor_401800_animation_17CE4_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation17CE4Bank4[216] = {
#include "assets/actor_401800_animation_17CE4_bank4.inc"
};

static AnimationRecord _gActor401800Animation17CE4Records[262] = {
#include "assets/actor_401800_animation_17CE4_records.inc"
};

static u16 _gActor401800Animation17CE4Indices[20] = {
#include "assets/actor_401800_animation_17CE4_indices.inc"
};

static AnimationSet _gActor401800Animation17CE4 = {
    _gActor401800Animation17CE4Records,
    _gActor401800Animation17CE4Indices,
    { NULL, _gActor401800Animation17CE4Bank1, NULL, NULL, _gActor401800Animation17CE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation18074Bank1[5] = {
#include "assets/actor_401800_animation_18074_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation18074Bank4[82] = {
#include "assets/actor_401800_animation_18074_bank4.inc"
};

static AnimationRecord _gActor401800Animation18074Records[111] = {
#include "assets/actor_401800_animation_18074_records.inc"
};

static u16 _gActor401800Animation18074Indices[20] = {
#include "assets/actor_401800_animation_18074_indices.inc"
};

static AnimationSet _gActor401800Animation18074 = {
    _gActor401800Animation18074Records,
    _gActor401800Animation18074Indices,
    { NULL, _gActor401800Animation18074Bank1, NULL, NULL, _gActor401800Animation18074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation188ACBank1[16] = {
#include "assets/actor_401800_animation_188AC_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation188ACBank4[199] = {
#include "assets/actor_401800_animation_188AC_bank4.inc"
};

static AnimationRecord _gActor401800Animation188ACRecords[259] = {
#include "assets/actor_401800_animation_188AC_records.inc"
};

static u16 _gActor401800Animation188ACIndices[20] = {
#include "assets/actor_401800_animation_188AC_indices.inc"
};

static AnimationSet _gActor401800Animation188AC = {
    _gActor401800Animation188ACRecords,
    _gActor401800Animation188ACIndices,
    { NULL, _gActor401800Animation188ACBank1, NULL, NULL, _gActor401800Animation188ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation18DFCBank1[10] = {
#include "assets/actor_401800_animation_18DFC_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation18DFCBank4[113] = {
#include "assets/actor_401800_animation_18DFC_bank4.inc"
};

static AnimationRecord _gActor401800Animation18DFCRecords[177] = {
#include "assets/actor_401800_animation_18DFC_records.inc"
};

static u16 _gActor401800Animation18DFCIndices[20] = {
#include "assets/actor_401800_animation_18DFC_indices.inc"
};

static AnimationSet _gActor401800Animation18DFC = {
    _gActor401800Animation18DFCRecords,
    _gActor401800Animation18DFCIndices,
    { NULL, _gActor401800Animation18DFCBank1, NULL, NULL, _gActor401800Animation18DFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation19D14Bank1[26] = {
#include "assets/actor_401800_animation_19D14_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation19D14Bank4[330] = {
#include "assets/actor_401800_animation_19D14_bank4.inc"
};

static AnimationRecord _gActor401800Animation19D14Records[538] = {
#include "assets/actor_401800_animation_19D14_records.inc"
};

static u16 _gActor401800Animation19D14Indices[20] = {
#include "assets/actor_401800_animation_19D14_indices.inc"
};

static AnimationSet _gActor401800Animation19D14 = {
    _gActor401800Animation19D14Records,
    _gActor401800Animation19D14Indices,
    { NULL, _gActor401800Animation19D14Bank1, NULL, NULL, _gActor401800Animation19D14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1A0B8Bank1[10] = {
#include "assets/actor_401800_animation_1A0B8_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1A0B8Bank4[73] = {
#include "assets/actor_401800_animation_1A0B8_bank4.inc"
};

static AnimationRecord _gActor401800Animation1A0B8Records[110] = {
#include "assets/actor_401800_animation_1A0B8_records.inc"
};

static u16 _gActor401800Animation1A0B8Indices[20] = {
#include "assets/actor_401800_animation_1A0B8_indices.inc"
};

static AnimationSet _gActor401800Animation1A0B8 = {
    _gActor401800Animation1A0B8Records,
    _gActor401800Animation1A0B8Indices,
    { NULL, _gActor401800Animation1A0B8Bank1, NULL, NULL, _gActor401800Animation1A0B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1A46CBank1[6] = {
#include "assets/actor_401800_animation_1A46C_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1A46CBank4[81] = {
#include "assets/actor_401800_animation_1A46C_bank4.inc"
};

static AnimationRecord _gActor401800Animation1A46CRecords[118] = {
#include "assets/actor_401800_animation_1A46C_records.inc"
};

static u16 _gActor401800Animation1A46CIndices[20] = {
#include "assets/actor_401800_animation_1A46C_indices.inc"
};

static AnimationSet _gActor401800Animation1A46C = {
    _gActor401800Animation1A46CRecords,
    _gActor401800Animation1A46CIndices,
    { NULL, _gActor401800Animation1A46CBank1, NULL, NULL, _gActor401800Animation1A46CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1A7F0Bank1[13] = {
#include "assets/actor_401800_animation_1A7F0_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1A7F0Bank4[58] = {
#include "assets/actor_401800_animation_1A7F0_bank4.inc"
};

static AnimationRecord _gActor401800Animation1A7F0Records[108] = {
#include "assets/actor_401800_animation_1A7F0_records.inc"
};

static u16 _gActor401800Animation1A7F0Indices[20] = {
#include "assets/actor_401800_animation_1A7F0_indices.inc"
};

static AnimationSet _gActor401800Animation1A7F0 = {
    _gActor401800Animation1A7F0Records,
    _gActor401800Animation1A7F0Indices,
    { NULL, _gActor401800Animation1A7F0Bank1, NULL, NULL, _gActor401800Animation1A7F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1AEE4Bank1[15] = {
#include "assets/actor_401800_animation_1AEE4_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1AEE4Bank4[159] = {
#include "assets/actor_401800_animation_1AEE4_bank4.inc"
};

static AnimationRecord _gActor401800Animation1AEE4Records[221] = {
#include "assets/actor_401800_animation_1AEE4_records.inc"
};

static u16 _gActor401800Animation1AEE4Indices[20] = {
#include "assets/actor_401800_animation_1AEE4_indices.inc"
};

static AnimationSet _gActor401800Animation1AEE4 = {
    _gActor401800Animation1AEE4Records,
    _gActor401800Animation1AEE4Indices,
    { NULL, _gActor401800Animation1AEE4Bank1, NULL, NULL, _gActor401800Animation1AEE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1B748Bank1[20] = {
#include "assets/actor_401800_animation_1B748_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1B748Bank4[190] = {
#include "assets/actor_401800_animation_1B748_bank4.inc"
};

static AnimationRecord _gActor401800Animation1B748Records[267] = {
#include "assets/actor_401800_animation_1B748_records.inc"
};

static u16 _gActor401800Animation1B748Indices[20] = {
#include "assets/actor_401800_animation_1B748_indices.inc"
};

static AnimationSet _gActor401800Animation1B748 = {
    _gActor401800Animation1B748Records,
    _gActor401800Animation1B748Indices,
    { NULL, _gActor401800Animation1B748Bank1, NULL, NULL, _gActor401800Animation1B748Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1BFC0Bank1[20] = {
#include "assets/actor_401800_animation_1BFC0_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1BFC0Bank4[196] = {
#include "assets/actor_401800_animation_1BFC0_bank4.inc"
};

static AnimationRecord _gActor401800Animation1BFC0Records[266] = {
#include "assets/actor_401800_animation_1BFC0_records.inc"
};

static u16 _gActor401800Animation1BFC0Indices[20] = {
#include "assets/actor_401800_animation_1BFC0_indices.inc"
};

static AnimationSet _gActor401800Animation1BFC0 = {
    _gActor401800Animation1BFC0Records,
    _gActor401800Animation1BFC0Indices,
    { NULL, _gActor401800Animation1BFC0Bank1, NULL, NULL, _gActor401800Animation1BFC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1C610Bank1[11] = {
#include "assets/actor_401800_animation_1C610_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1C610Bank4[150] = {
#include "assets/actor_401800_animation_1C610_bank4.inc"
};

static AnimationRecord _gActor401800Animation1C610Records[201] = {
#include "assets/actor_401800_animation_1C610_records.inc"
};

static u16 _gActor401800Animation1C610Indices[20] = {
#include "assets/actor_401800_animation_1C610_indices.inc"
};

static AnimationSet _gActor401800Animation1C610 = {
    _gActor401800Animation1C610Records,
    _gActor401800Animation1C610Indices,
    { NULL, _gActor401800Animation1C610Bank1, NULL, NULL, _gActor401800Animation1C610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1CFE4Bank1[18] = {
#include "assets/actor_401800_animation_1CFE4_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1CFE4Bank4[232] = {
#include "assets/actor_401800_animation_1CFE4_bank4.inc"
};

static AnimationRecord _gActor401800Animation1CFE4Records[323] = {
#include "assets/actor_401800_animation_1CFE4_records.inc"
};

static u16 _gActor401800Animation1CFE4Indices[20] = {
#include "assets/actor_401800_animation_1CFE4_indices.inc"
};

static AnimationSet _gActor401800Animation1CFE4 = {
    _gActor401800Animation1CFE4Records,
    _gActor401800Animation1CFE4Indices,
    { NULL, _gActor401800Animation1CFE4Bank1, NULL, NULL, _gActor401800Animation1CFE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1DE5CBank1[33] = {
#include "assets/actor_401800_animation_1DE5C_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1DE5CBank4[326] = {
#include "assets/actor_401800_animation_1DE5C_bank4.inc"
};

static AnimationRecord _gActor401800Animation1DE5CRecords[481] = {
#include "assets/actor_401800_animation_1DE5C_records.inc"
};

static u16 _gActor401800Animation1DE5CIndices[20] = {
#include "assets/actor_401800_animation_1DE5C_indices.inc"
};

static AnimationSet _gActor401800Animation1DE5C = {
    _gActor401800Animation1DE5CRecords,
    _gActor401800Animation1DE5CIndices,
    { NULL, _gActor401800Animation1DE5CBank1, NULL, NULL, _gActor401800Animation1DE5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1ED6CBank1[26] = {
#include "assets/actor_401800_animation_1ED6C_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1ED6CBank4[351] = {
#include "assets/actor_401800_animation_1ED6C_bank4.inc"
};

static AnimationRecord _gActor401800Animation1ED6CRecords[515] = {
#include "assets/actor_401800_animation_1ED6C_records.inc"
};

static u16 _gActor401800Animation1ED6CIndices[20] = {
#include "assets/actor_401800_animation_1ED6C_indices.inc"
};

static AnimationSet _gActor401800Animation1ED6C = {
    _gActor401800Animation1ED6CRecords,
    _gActor401800Animation1ED6CIndices,
    { NULL, _gActor401800Animation1ED6CBank1, NULL, NULL, _gActor401800Animation1ED6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1F3F4Bank1[15] = {
#include "assets/actor_401800_animation_1F3F4_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1F3F4Bank4[154] = {
#include "assets/actor_401800_animation_1F3F4_bank4.inc"
};

static AnimationRecord _gActor401800Animation1F3F4Records[199] = {
#include "assets/actor_401800_animation_1F3F4_records.inc"
};

static u16 _gActor401800Animation1F3F4Indices[20] = {
#include "assets/actor_401800_animation_1F3F4_indices.inc"
};

static AnimationSet _gActor401800Animation1F3F4 = {
    _gActor401800Animation1F3F4Records,
    _gActor401800Animation1F3F4Indices,
    { NULL, _gActor401800Animation1F3F4Bank1, NULL, NULL, _gActor401800Animation1F3F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation1FDECBank1[19] = {
#include "assets/actor_401800_animation_1FDEC_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation1FDECBank4[250] = {
#include "assets/actor_401800_animation_1FDEC_bank4.inc"
};

static AnimationRecord _gActor401800Animation1FDECRecords[311] = {
#include "assets/actor_401800_animation_1FDEC_records.inc"
};

static u16 _gActor401800Animation1FDECIndices[20] = {
#include "assets/actor_401800_animation_1FDEC_indices.inc"
};

static AnimationSet _gActor401800Animation1FDEC = {
    _gActor401800Animation1FDECRecords,
    _gActor401800Animation1FDECIndices,
    { NULL, _gActor401800Animation1FDECBank1, NULL, NULL, _gActor401800Animation1FDECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation2074CBank1[21] = {
#include "assets/actor_401800_animation_2074C_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation2074CBank4[233] = {
#include "assets/actor_401800_animation_2074C_bank4.inc"
};

static AnimationRecord _gActor401800Animation2074CRecords[284] = {
#include "assets/actor_401800_animation_2074C_records.inc"
};

static u16 _gActor401800Animation2074CIndices[20] = {
#include "assets/actor_401800_animation_2074C_indices.inc"
};

static AnimationSet _gActor401800Animation2074C = {
    _gActor401800Animation2074CRecords,
    _gActor401800Animation2074CIndices,
    { NULL, _gActor401800Animation2074CBank1, NULL, NULL, _gActor401800Animation2074CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation210E0Bank1[28] = {
#include "assets/actor_401800_animation_210E0_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation210E0Bank4[205] = {
#include "assets/actor_401800_animation_210E0_bank4.inc"
};

static AnimationRecord _gActor401800Animation210E0Records[304] = {
#include "assets/actor_401800_animation_210E0_records.inc"
};

static u16 _gActor401800Animation210E0Indices[20] = {
#include "assets/actor_401800_animation_210E0_indices.inc"
};

static AnimationSet _gActor401800Animation210E0 = {
    _gActor401800Animation210E0Records,
    _gActor401800Animation210E0Indices,
    { NULL, _gActor401800Animation210E0Bank1, NULL, NULL, _gActor401800Animation210E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation21AB8Bank1[30] = {
#include "assets/actor_401800_animation_21AB8_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation21AB8Bank4[219] = {
#include "assets/actor_401800_animation_21AB8_bank4.inc"
};

static AnimationRecord _gActor401800Animation21AB8Records[301] = {
#include "assets/actor_401800_animation_21AB8_records.inc"
};

static u16 _gActor401800Animation21AB8Indices[20] = {
#include "assets/actor_401800_animation_21AB8_indices.inc"
};

static AnimationSet _gActor401800Animation21AB8 = {
    _gActor401800Animation21AB8Records,
    _gActor401800Animation21AB8Indices,
    { NULL, _gActor401800Animation21AB8Bank1, NULL, NULL, _gActor401800Animation21AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation224A8Bank1[19] = {
#include "assets/actor_401800_animation_224A8_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation224A8Bank4[252] = {
#include "assets/actor_401800_animation_224A8_bank4.inc"
};

static AnimationRecord _gActor401800Animation224A8Records[307] = {
#include "assets/actor_401800_animation_224A8_records.inc"
};

static u16 _gActor401800Animation224A8Indices[20] = {
#include "assets/actor_401800_animation_224A8_indices.inc"
};

static AnimationSet _gActor401800Animation224A8 = {
    _gActor401800Animation224A8Records,
    _gActor401800Animation224A8Indices,
    { NULL, _gActor401800Animation224A8Bank1, NULL, NULL, _gActor401800Animation224A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401800Animation23304Bank1[18] = {
#include "assets/actor_401800_animation_23304_bank1.inc"
};

static AnimationPackedRotation _gActor401800Animation23304Bank4[361] = {
#include "assets/actor_401800_animation_23304_bank4.inc"
};

static AnimationRecord _gActor401800Animation23304Records[484] = {
#include "assets/actor_401800_animation_23304_records.inc"
};

static u16 _gActor401800Animation23304Indices[20] = {
#include "assets/actor_401800_animation_23304_indices.inc"
};

AnimationSet gOddStrangerDormantAnimSet = {
    _gActor401800Animation23304Records,
    _gActor401800Animation23304Indices,
    { NULL, _gActor401800Animation23304Bank1, NULL, NULL, _gActor401800Animation23304Bank4, NULL, NULL, NULL },
};

s8 gOddStrangerTransitions[45][45] = {
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 5, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

AnimationSet* gOddStrangerAnimSets[46] = {
    NULL,
    NULL,
    &_gActor401800Animation210E0,
    &_gActor401800Animation21AB8,
    &_gActor401800Animation1ED6C,
    &_gActor401800Animation1C610,
    &_gActor401800Animation1CFE4,
    &_gActor401800Animation1DE5C,
    &_gActor401800Animation224A8,
    &_gActor401800Animation188AC,
    &_gActor401800Animation1A0B8,
    &_gActor401800Animation1A46C,
    &_gActor401800Animation15CDC,
    &_gActor401800Animation156F8,
    &_gActor401800Animation19D14,
    &_gActor401800Animation16730,
    NULL,
    &_gActor401800Animation18DFC,
    &_gActor401800Animation15118,
    &_gActor401800Animation1AEE4,
    &_gActor401800Animation1B748,
    &_gActor401800Animation1BFC0,
    &_gActor401800Animation17CE4,
    &_gActor401800Animation1A46C,
    &_gActor401800Animation15CDC,
    &_gActor401800Animation18074,
    &_gActor401800Animation1A7F0,
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

AnimationSet* D_actor_401800_801559F0[2] = { 0 };

AnimationSet* D_actor_401800_801559F8[5] = {
    NULL,
    &_gActor401800Animation1F3F4,
    &_gActor401800Animation1FDEC,
    &_gActor401800Animation2074C,
    NULL,
};

AnimationPlayRequest gOddStrangerPlayerAnim = { { .sets = D_actor_401800_801559F0 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

SVECTOR gOddStrangerHitOffsets[12] = {
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

enum { ACTOR_401800_MESSAGE_IGNORE = 2015 };

TaskMessageEntry D_actor_401800_80155A80[8] = {
    { ACTOR_401800_MESSAGE_IGNORE, _actor401800IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _oddStrangerPlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, _oddStrangerApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 gOddStrangerChaseDistance = 0;

TaskDesc D_actor_401800_80155AC4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor401800Task, { .model = &_gActor401800OddStrangerBody } };

static SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

OddStrangerTransformStorage gOddStrangerGrabTransform;

GameActorButtonPressHold D_actor_401800_80155AF8;

static void _actor401800BurstDeath(Task* task);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Binds the model's lighting pointers to its Odd Stranger work storage.
///
/// Requires a live TMD task with initialized work. The work matrices must remain
/// live until the model stops using them; this does not initialize their values.
static __inline__ void _actor401800BindLightingMatrices(const Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       model;

    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Initializes this Odd Stranger's work, animation, collision and patrol state.
///
/// Requires a live enemy and TMD task with at least parts 0..6 and loaded model,
/// animation and area data. Allocation failure destroys the enemy/task. Success
/// acquires a battle hold, installs teardown, lends work matrices and contacts
/// to the model/enemy, and links two body spheres and one attack sphere.
/// Seeds a 2000-game-unit patrol segment along the root's horizontal +Z axis.
/// Spawn bits 16..19 select hidden (2), dormant (4) or ordinary patrol; low
/// nibble 2 selects tuning row 0, 1 row 2, and every other value row 1.
/// Rebuilds root yaw at Q12 scale 4500 and advances Task::state. Work and the
/// loaded package data must remain live until teardown; clobbers the GTE.
static void _actor401800Initialize(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_401800_TARGET_PART             = 2,
        ACTOR_401800_ATTACK_PART             = 6,
        ACTOR_401800_GRID_BODY_Y             = -172,
        ACTOR_401800_GRID_KEY_DETAIL         = 0x12,
        ACTOR_401800_ATTACK_RADIUS           = 384,
        ACTOR_401800_HIT_EFFECT_PART         = 1,
        ACTOR_401800_HIT_EFFECT_ARGUMENT_LOW = 0x300,
        ACTOR_401800_HIT_EFFECT_REPEAT_COUNT = 2,
        ACTOR_401800_SPAWN_BEHAVIOUR_SHIFT   = 16,
        ACTOR_401800_SPAWN_SELECTOR_MASK     = 0xF,
        ACTOR_401800_SPAWN_HIDDEN            = 2,
        ACTOR_401800_SPAWN_DORMANT           = 4,
        ACTOR_401800_PREVIOUS_STATE_NONE     = -1,
        ACTOR_401800_TUNING_FIRST_SELECTOR   = 2,
        ACTOR_401800_TUNING_THIRD_SELECTOR   = 1
    };
    SVECTOR             direction;
    VECTOR              lightingPosition;
    SVECTOR*            directionScratch;
    TmdObject*          model;
    GfxCoord*           rootCoord;
    OddStrangerWork*    work;
    WorldCollisionBody* hitBody;
    WorldCollisionBody* attackBody;
    s32                 spawnBehaviour;

    rootCoord   = actor->extra.tmd->coords;
    model       = actor->extra.tmd;
    work        = memCalloc(sizeof(OddStrangerWork), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    sceneAcquireBattleRef(0);
    actor->exitCallback = _oddStrangerExit;
    _actor401800BindLightingMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[ACTOR_401800_TARGET_PART];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401800_8013E6F0.hpMax;
    enemy->param                  = &D_actor_401800_8013E6F0;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, gOddStrangerAnimSets, model,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, gOddStrangerAnimSets, model,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ODD_STRANGER_ANIM_REQUEST_RESET;
    work->animId        = ODD_STRANGER_ANIM_WALK;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->chaseRate++;
    } else {
        work->chaseRate--;
    }
    _oddStrangerDriveAnimation(actor);

    // The root sphere handles room-grid push; the part sphere receives hits.
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = rootCoord;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = ACTOR_401800_GRID_BODY_Y;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_401800_GRID_KEY_DETAIL;
    work->gridBody.radius           = ODD_STRANGER_BODY_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    hitBody                   = &work->hitBody;
    hitBody->coord            = &actor->extra.tmd->coords[ACTOR_401800_TARGET_PART];
    hitBody->context.contacts = work->hitContacts;
    hitBody->pos.vx           = 0;
    hitBody->pos.vy           = 0;
    hitBody->pos.vz           = 0;
    hitBody->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    hitBody->radius           = ODD_STRANGER_BODY_RADIUS;
    hitBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    direction.vx                 = 0;
    direction.vy                 = 0;
    direction.vz                 = 0;
    attackBody                   = &work->attackBody;
    attackBody->coord            = &actor->extra.tmd->coords[ACTOR_401800_ATTACK_PART];
    attackBody->context.contacts = work->attackContacts;
    directionScratch             = &direction;
    attackBody->pos.vx           = directionScratch->vx;
    attackBody->pos.vy           = directionScratch->vy;
    attackBody->pos.vz           = directionScratch->vz;
    attackBody->radius           = ACTOR_401800_ATTACK_RADIUS;
    attackBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

/// Seeds a patrol segment with caller-owned forward-axis scratch.
///
/// Invoke inside a braced function body with side-effect-free task/work pointers,
/// a writable SVECTOR lvalue forwardVector, and vectorScratch == &forwardVector.
/// Arguments are evaluated repeatedly; no identifiers are captured. Clears Y,
/// normalizes and scales to 2000 game units, narrows the two waypoints to s16,
/// and clobbers the GTE. Expands to one compound statement.
#define ACTOR_401800_INITIALIZE_PATROL_POINTS(actorTask, actorWork, vectorScratch, forwardVector)         \
    {                                                                                                     \
        enum { ACTOR_401800_PATROL_LENGTH = 2000 };                                                       \
        (actorWork)->patrolTarget      = 0;                                                               \
        (actorWork)->patrolPoints[0].x = (actorTask)->extra.tmd->coords->coord.t[0];                      \
        (actorWork)->patrolPoints[0].z = (actorTask)->extra.tmd->coords->coord.t[2];                      \
        gfxReadMatrixZAxis(&(actorTask)->extra.tmd->coords->coord, (vectorScratch));                      \
        (forwardVector).vy = 0;                                                                           \
        VectorNormalSS((vectorScratch), (vectorScratch));                                                 \
        gte_lddp(ACTOR_401800_PATROL_LENGTH);                                                             \
        gte_ldsv((vectorScratch));                                                                        \
        gte_gpf12();                                                                                      \
        gte_stsv((vectorScratch));                                                                        \
        (actorWork)->patrolPoints[1].x = (actorTask)->extra.tmd->coords->coord.t[0] + (forwardVector).vx; \
        (actorWork)->patrolPoints[1].z = (actorTask)->extra.tmd->coords->coord.t[2] + (forwardVector).vz; \
    }

    ACTOR_401800_INITIALIZE_PATROL_POINTS(actor, work, directionScratch, direction);
#undef ACTOR_401800_INITIALIZE_PATROL_POINTS

    actor->msgTable         = D_actor_401800_80155A80;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[ACTOR_401800_HIT_EFFECT_PART];
    work->effectArg.spawnArgLo = ACTOR_401800_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = ACTOR_401800_HIT_EFFECT_REPEAT_COUNT;
    spawnBehaviour             = (actor->spawnArg1.value >> ACTOR_401800_SPAWN_BEHAVIOUR_SHIFT);
    switch (spawnBehaviour & ACTOR_401800_SPAWN_SELECTOR_MASK) {
        case ACTOR_401800_SPAWN_HIDDEN:
            work->prevState = ACTOR_401800_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_HIDDEN;
            break;
        case ACTOR_401800_SPAWN_DORMANT:
            work->prevState = ACTOR_401800_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_DORMANT;
            break;
        default:
            work->prevState = ACTOR_401800_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }
    switch (actor->spawnArg1.value & ACTOR_401800_SPAWN_SELECTOR_MASK) {
        case ACTOR_401800_TUNING_FIRST_SELECTOR:
            work->downFramesBase = D_actor_401800_8013E700[0].downFramesBase;
            work->sidestepAngle  = D_actor_401800_8013E700[0].sidestepAngle;
            work->sidestepDelay  = D_actor_401800_8013E700[0].sidestepDelay;
            work->noticeRadius   = D_actor_401800_8013E700[0].noticeRadius;
            break;
        case ACTOR_401800_TUNING_THIRD_SELECTOR:
            work->downFramesBase = D_actor_401800_8013E700[2].downFramesBase;
            work->sidestepAngle  = D_actor_401800_8013E700[2].sidestepAngle;
            work->sidestepDelay  = D_actor_401800_8013E700[2].sidestepDelay;
            work->noticeRadius   = D_actor_401800_8013E700[2].noticeRadius;
            break;
        case 0:
        default:
            work->downFramesBase = D_actor_401800_8013E700[1].downFramesBase;
            work->sidestepAngle  = D_actor_401800_8013E700[1].sidestepAngle;
            work->sidestepDelay  = D_actor_401800_8013E700[1].sidestepDelay;
            work->noticeRadius   = D_actor_401800_8013E700[1].noticeRadius;
            break;
    }

    _actorRenderRescaleYaw(actor->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    work->bodyPosCursor = 0;
    actor->state++;
}

#include "../../shared/odd_stranger_spawn_hit_effect.inc.c"

#include "../../shared/odd_stranger_take_hit.inc.c"

#include "../../shared/odd_stranger_stunned.inc.c"

#include "../../shared/odd_stranger_face_player.inc.c"

#include "../../shared/odd_stranger_push_body.inc.c"

/// Refreshes player heading, the offset to the player and its reverse bearing.
///
/// Requires live actor/player model roots in the same parent frame, the live
/// player-status translation and writable caller-owned scratch. Reads player
/// heading first, then refreshes delta from the player-status matrix. Offsets
/// narrow to signed-halfword game units. The reverse bearing is measured from
/// that narrowed X/Z delta; angles use 4096 units per turn and normalization
/// retains both half-turn endpoints. Other scratch fields remain unchanged.
static __inline__ void _actor401800ReadPlayerBearings(const Task* task, ActorChaseScratch* chase)
{
    chase->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                              gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
}

/// Pursues the player in `ODD_STRANGER_STATE_CHASE`, sidestepping or grabbing.
///
/// A stalking spawn switches to STALK immediately. Entry enables drawing,
/// lock-on and grid response and starts the run clip. Later ticks resolve
/// contacts, turn by at most 48 units (4096 per turn) and advance with the clip
/// rate. Clear sight permits grabs within 1100 units and sidesteps outside
/// 1800 units; the grab's signed, one-sided turn test is retained.
/// Requires live actor/player roots in the same parent frame, initialized work
/// and enough scratch space for one chase block plus the nested contact tests.
static void _actor401800Chase(Task* task)
{
    enum {
        ACTOR_401800_CHASE_SPAWN_MODE_MASK     = 0xF0,
        ACTOR_401800_CHASE_STALK_SPAWN         = 0x10,
        ACTOR_401800_CHASE_PLAYER_FACING_LIMIT = 68,
        ACTOR_401800_CHASE_SIDESTEP_TURN_LIMIT = 128,
        ACTOR_401800_CHASE_SIDESTEP_RANGE      = 1800,
        ACTOR_401800_CHASE_GRAB_RANGE          = 1100,
        ACTOR_401800_CHASE_GRAB_TURN_LIMIT     = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_401800_CHASE_TURN_STEP           = 48
    };
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          headingRoot;
    GfxCoord*          facingRoot;
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* chase;
    s32                spawnMode;

    spawnMode = (task->spawnArg1.value >> 16);
    work      = task->work;
    if ((spawnMode & ACTOR_401800_CHASE_SPAWN_MODE_MASK) == ACTOR_401800_CHASE_STALK_SPAWN) {
        work->state = ODD_STRANGER_STATE_STALK;
        return;
    }
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = ODD_STRANGER_ANIM_RUN;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        _oddStrangerDriveAnimation(task);
        work->dashCount   = 0;
        work->stateTimer  = 0;
        work->exitCounter = 0;
        return;
    }
    work->stateTimer++;
    savedCursor = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase       = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1);
    // Separate body overlap only when the room grid did not push the root.
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    // Refresh player heading and reverse bearing after advancing the animation.
    _actor401800ReadPlayerBearings(task, chase);
    headingRoot         = task->extra.tmd->coords;
    chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
    work->lookYawTarget = chase->turn;
    if (abs(chase->yawFromPlayer - chase->playerYaw) < ACTOR_401800_CHASE_PLAYER_FACING_LIMIT) {
        if (((s16)work->sidestepDelay + work->sidestepCount / 2) < work->stateTimer) {
            if (abs(chase->turn) < ACTOR_401800_CHASE_SIDESTEP_TURN_LIMIT) {
                if (_actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_401800_CHASE_SIDESTEP_RANGE) && _playerDetectionSightBlocked(task) != 1) {
                    work->state = ODD_STRANGER_STATE_SIDESTEP;
                }
            }
        }
    }
    // This signed comparison allows every turn below the positive limit.
    if (chase->turn < ACTOR_401800_CHASE_GRAB_TURN_LIMIT) {
        if (!_actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_401800_CHASE_GRAB_RANGE) && _playerDetectionSightBlocked(task) != 1 && work->grabCooldown == 0) {
            work->state = ODD_STRANGER_STATE_GRAB;
        }
    }
    if (chase->turn > ACTOR_401800_CHASE_TURN_STEP) {
        chase->turn = ACTOR_401800_CHASE_TURN_STEP;
    }
    if (chase->turn < -ACTOR_401800_CHASE_TURN_STEP) {
        chase->turn = -ACTOR_401800_CHASE_TURN_STEP;
    }
    facingRoot   = task->extra.tmd->coords;
    chase->turn += ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ODD_STRANGER_ANIM_RUN) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_BODY_RADIUS, ((work->chaseRate + 2) * 0x42) / 18) != 0) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ((work->chaseRate + 2) * 0x42) / 18);
            }
        } else if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_BODY_RADIUS, (((work->chaseRate + 2) * 0x42) / 18) >> 2) != 0) {
            _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (((work->chaseRate + 2) * 0x42) / 18) >> 2);
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = ODD_STRANGER_ANIM_RUN;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/odd_stranger_chase.inc.c"

#include "../../shared/odd_stranger_turn_around.inc.c"

#include "../../shared/odd_stranger_sidestep.inc.c"

/// Computes the horizontal offset from a reference origin to player joint 1.
///
/// Walks the live player's acyclic local-matrix chain to `gGfxViewCoord`,
/// excluding that view matrix and narrowing XYZ to signed halfwords after
/// each transform. The view must have a parent to commit the target origin;
/// an incomplete chain uses a zero target origin. `reference` must use that
/// same pre-view frame. Output X/Z use game units; output Y is zero.
/// Word-aligned `transformedPoint` receives the intermediate point even on an
/// incomplete chain; `offset` must be halfword-aligned. Both writable vectors
/// must be disjoint from each other and the hierarchy; their fourth halfwords
/// are untouched. Requires one free word-aligned SVECTOR on the initialized
/// scratch stack, restored before returning.
/// Overwrites GTE transform state; captured overflow flags are discarded.
static __inline__ void _actor401800PlayerJointOffsetXZ(const GfxCoord* reference, SVECTOR* transformedPoint, SVECTOR* offset)
{
    VECTOR          longPoint;
    SVECTOR*        playerOrigin;
    SVECTOR*        savedCursor;
    VECTOR*         longPointOutput;
    const GfxCoord* currentCoord;
    const GfxCoord* viewCoord;
    s32             gteFlags;
    s32*            gteFlagsOutput;
    Task*           player;

    player                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    savedCursor                   = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = savedCursor - 1;
    playerOrigin                  = savedCursor - 1;
    viewCoord                     = &gGfxViewCoord;
    longPointOutput               = &longPoint;
    gteFlagsOutput                = &gteFlags;
    playerOrigin->vx              = 0;
    playerOrigin->vy              = 0;
    playerOrigin->vz              = 0;
    currentCoord                  = &player->extra.tmd->coords[1];
    transformedPoint->vx          = playerOrigin->vx;
    transformedPoint->vy          = playerOrigin->vy;
    transformedPoint->vz          = playerOrigin->vz;

/// Transforms a joint point and narrows XYZ before walking to the next parent.
///
/// `localMatrix` is a stable, readable, word-aligned Q12 matrix pointer,
/// evaluated twice. `point` is a stable writable word-aligned SVECTOR pointer,
/// evaluated four times, disjoint from the matrix; its pad stays intact.
/// Captures this helper's `longPoint`, `longPointOutput` (its address) and
/// `gteFlagsOutput` (the discarded FLAG output). These staging outputs must
/// remain live throughout the parent walk; overflow flags do not affect XYZ.
/// Changes GTE state and expands to one compound statement inside braces.
#define ACTOR_401800_TRANSFORM_JOINT_POINT(localMatrix, point)  \
    {                                                           \
        gte_SetTransMatrix((localMatrix));                      \
        gte_SetRotMatrix((localMatrix));                        \
        gte_RotTrans((point), longPointOutput, gteFlagsOutput); \
        (point)->vx = longPoint.vx;                             \
        (point)->vy = longPoint.vy;                             \
        (point)->vz = longPoint.vz;                             \
    }

    // Commit the origin only after reaching the excluded view node.
    for (;;) {
        if (currentCoord->parent != NULL) {
            if (currentCoord != viewCoord) {
                ACTOR_401800_TRANSFORM_JOINT_POINT(&currentCoord->coord, transformedPoint);
                currentCoord = currentCoord->parent;
                continue;
            }
            playerOrigin->vx = transformedPoint->vx;
            playerOrigin->vy = transformedPoint->vy;
            playerOrigin->vz = transformedPoint->vz;
        }
        break;
    }
    offset->vx = playerOrigin->vx - reference->coord.t[0];
    offset->vy = 0;
    offset->vz = playerOrigin->vz - reference->coord.t[2];
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
#undef ACTOR_401800_TRANSFORM_JOINT_POINT

/// Selects the held player's animation bank for the saved character.
///
/// Character 1 uses the populated grab bank; every other value selects the
/// alternate bank. The persistent request borrows the selected overlay table.
static __inline__ void _actor401800SelectPlayerGrabAnimations(void)
{
    enum { ACTOR_401800_PRIMARY_CHARACTER_ID = 1 };
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_401800_PRIMARY_CHARACTER_ID) {
        gOddStrangerPlayerAnim.source.sets = D_actor_401800_801559F8;
    } else {
        gOddStrangerPlayerAnim.source.sets = D_actor_401800_801559F0;
    }
}

/// Saves the root origin to restore when this grab or its strike ends.
///
/// Requires live task/model storage and writable work for that same actor.
/// Saves XYZ in parent-space game units, narrowed to signed halfwords without
/// saturation; pad is unchanged. The behaviour dispatcher restores these values
/// when leaving GRAB or GRAB_STRIKE. The caller resolves contacts before saving.
static __inline__ void _actor401800SaveGrabOrigin(const Task* task, OddStrangerWork* work)
{
    work->grabStartPos.vx = task->extra.tmd->coords->coord.t[0];
    work->grabStartPos.vy = task->extra.tmd->coords->coord.t[1];
    work->grabStartPos.vz = task->extra.tmd->coords->coord.t[2];
}

/// Reaches for the player in `ODD_STRANGER_STATE_GRAB` and starts a hold.
///
/// At cue record 16, a non-scripted player within 1500 units and 32 angle
/// units can accept an eight-press escape hold, selecting GRAB_PULL and the
/// player's grab clip. From record 17, steps ten units away while within
/// 1400 units. An unsuccessful reach returns to CHASE at the clip boundary.
/// Requires live actor/player work and models, common pre-view coordinates,
/// initialized animation/collision state and scratch space for the nested tests.
/// Static hold and animation requests are borrowed during synchronous dispatch.
static void _actor401800Grab(Task* task)
{
    enum {
        ACTOR_401800_GRAB_COOLDOWN_TICKS   = 10,
        ACTOR_401800_GRAB_HOLD_RECORD      = 16,
        ACTOR_401800_GRAB_RETREAT_RECORD   = 17,
        ACTOR_401800_GRAB_TURN_LIMIT       = 32,
        ACTOR_401800_GRAB_HOLD_RANGE       = 1500,
        ACTOR_401800_GRAB_RETREAT_RANGE    = 1400,
        ACTOR_401800_GRAB_RETREAT_STEP     = 10,
        ACTOR_401800_GRAB_ESCAPE_PRESSES   = 8,
        ACTOR_401800_PLAYER_GRAB_ANIMATION = 1
    };
    Enemy*           enemy;
    Task*            player;
    GameActor*       playerWork;
    PlayerStatus*    playerStatus;
    OddStrangerWork* work;
    SVECTOR          separation;
    SVECTOR          transformedPoint;
    s32              turnError;
    u16              elapsedTicks;

    enemy        = task->spawnArg2.pointer;
    work         = task->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerWork   = player->work;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->grabCooldown            = ACTOR_401800_GRAB_COOLDOWN_TICKS;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->sidestepCount           = 0;
        work->playerHeld              = 0;
        work->attackBody.flags        = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags          = work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        work->animId                  = ODD_STRANGER_ANIM_GRAB_REACH;
        _oddStrangerDriveAnimation(task);
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        _actor401800SaveGrabOrigin(task, work);
        work->stateTimer = 0;
        return;
    }
    elapsedTicks     = (u16)work->stateTimer + 1;
    work->stateTimer = elapsedTicks;
    if ((s16)elapsedTicks == 1) {
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        _actor401800SaveGrabOrigin(task, work);
        _actor401800PlayerJointOffsetXZ(task->extra.tmd->coords, &transformedPoint, &separation);
        turnError = _actorAngleTurnToDirection(task->extra.tmd->coords, &separation);
        gfxRotMatrixY(&task->extra.tmd->coords[0].coord, turnError, GRAPHICS_ROTATION_COMPOSE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
        separation.vx                         = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        separation.vy                         = 0;
        separation.vz                         = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
        work->grabCooldown                    = ACTOR_401800_GRAB_COOLDOWN_TICKS;
    }
    _oddStrangerDriveAnimation(task);
    // The reach cue tests the animated joint origin before requesting the hold.
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_401800_GRAB_HOLD_RECORD && playerWork->mode != GAME_ACTOR_MODE_SCRIPTED) {
        _actor401800PlayerJointOffsetXZ(task->extra.tmd->coords, &transformedPoint, &separation);
        turnError = _actorAngleTurnToDirection(task->extra.tmd->coords, &separation);
        if (turnError < 0) {
            turnError = -turnError;
        }
        if (turnError < ACTOR_401800_GRAB_TURN_LIMIT) {
            if (!_oddStrangerOutOfRange(&separation, ACTOR_401800_GRAB_HOLD_RANGE)) {
                _actor401800SelectPlayerGrabAnimations();
                D_actor_401800_80155AF8.pressCount = ACTOR_401800_GRAB_ESCAPE_PRESSES;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_401800_80155AF8, 0) == 0) {
                    work->state                        = ODD_STRANGER_STATE_GRAB_PULL;
                    work->playerHeld                   = 1;
                    gOddStrangerPlayerAnim.animationId = ACTOR_401800_PLAYER_GRAB_ANIMATION;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &gOddStrangerPlayerAnim, 0);
                }
            }
        }
    }
    if (work->animId == ODD_STRANGER_ANIM_GRAB_REACH && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    // After the reach, step away along the separation and resolve contacts.
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= (u32)ACTOR_401800_GRAB_RETREAT_RECORD) {
        separation.vx = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        separation.vy = 0;
        separation.vz = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        if (!_oddStrangerOutOfRange(&separation, ACTOR_401800_GRAB_RETREAT_RANGE)) {
            VectorNormalSS(&separation, &separation);
            gte_lddp(ACTOR_401800_GRAB_RETREAT_STEP);
            gte_ldsv(&separation);
            gte_gpf12();
            gte_stsv(&separation);
            task->extra.tmd->coords->coord.t[0]  += separation.vx;
            task->extra.tmd->coords->coord.t[2]  += separation.vz;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
}

#include "../../shared/odd_stranger_grab.inc.c"

#include "../../shared/odd_stranger_grab_hold.inc.c"

#include "../../shared/odd_stranger_grab_release.inc.c"

/// Disables hit-body grid response and selects the completed-knockdown behaviour.
///
/// Requires live work and its enemy at the fall handler's boundary or settled
/// cue. Nonpositive HP selects DEATH_BURN before considering status buildup;
/// positive HP selects STATUS_HOLD with buildup, otherwise DOWN. Changes only
/// the grid-enable flag and behaviour state; the dispatcher performs entry on
/// its next update. Does not unlink bodies or alter Task::state.
static __inline__ void _actor401800CompleteKnockdown(OddStrangerWork* work, const Enemy* enemy)
{
    work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DEATH_BURN;
    } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        work->state = ODD_STRANGER_STATE_STATUS_HOLD;
    } else {
        work->state = ODD_STRANGER_STATE_DOWN;
    }
}

/// Staggers backward and falls in `ODD_STRANGER_STATE_FALL_BACK`.
///
/// Steps -87 game units while the recoil clip permits, then restarts the
/// back-lying clip. Grid contacts displace the root throughout the fall.
/// At its boundary, nonpositive HP selects DEATH_BURN, status buildup selects
/// STATUS_HOLD, and otherwise DOWN. Requires live work, enemy and model.
static void _actor401800FallBack(Task* task)
{
    enum {
        ACTOR_401800_ANIM_FALL_BACK = 10,
        ACTOR_401800_FALL_BACK_STEP = -87
    };
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_401800_ANIM_FALL_BACK;
        work->blendActive             = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if ((work->animId == ACTOR_401800_ANIM_FALL_BACK) && ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_BODY_RADIUS, ACTOR_401800_FALL_BACK_STEP) != 0)) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ACTOR_401800_FALL_BACK_STEP);
    }
    _oddStrangerDriveAnimation(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (work->animId == ACTOR_401800_ANIM_FALL_BACK)) {
        work->animId      = ODD_STRANGER_ANIM_DOWN_BACK;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
        _oddStrangerDriveAnimation(task);
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (work->animId == ODD_STRANGER_ANIM_DOWN_BACK)) {
        _actor401800CompleteKnockdown(work, enemy);
    }
}

/// Restarts a back knockdown in `ODD_STRANGER_STATE_REFALL_BACK`.
///
/// Holds the back-lying clip while resolving both contact sets. Its settled
/// boundary disables hit-body grid response and selects DEATH_BURN for
/// nonpositive HP, STATUS_HOLD for buildup, or DOWN. Requires live work,
/// enemy, model and scratch space for contact correction.
static void _actor401800RefallBack(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ODD_STRANGER_ANIM_DOWN_BACK;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401800CompleteKnockdown(work, enemy);
    }
}

/// Restarts a front knockdown in `ODD_STRANGER_STATE_REFALL_FRONT`.
///
/// Plays the front refall clip and resolves both contact sets. Its settled
/// boundary disables hit-body grid response and selects DEATH_BURN for
/// nonpositive HP, STATUS_HOLD for buildup, or DOWN. Requires live work,
/// enemy, model and scratch space for contact correction.
static void _actor401800RefallFront(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ODD_STRANGER_ANIM_REFALL_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401800CompleteKnockdown(work, enemy);
    }
}

/// Falls forward in `ODD_STRANGER_STATE_FALL_FRONT`.
///
/// Blends into the front-lying clip and resolves both contact sets. The
/// reported clip boundary disables hit-body grid response and selects
/// DEATH_BURN for nonpositive HP, STATUS_HOLD for buildup, or DOWN.
/// Requires live work, enemy, model and scratch space for contact correction.
static void _actor401800FallFront(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = ODD_STRANGER_ANIM_DOWN_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        _actor401800CompleteKnockdown(work, enemy);
    }
}

#include "../../shared/odd_stranger_die.inc.c"

/// Waits for a nearby player or combat noise in `ODD_STRANGER_STATE_DORMANT`.
///
/// Entry enables drawing and lock-on, disables root grid response and saves
/// the color matrix. Alternates two idle clips at their jump/boundary cues.
/// After 2401 ticks, one random draw in sixteen skips the whole handler tick.
/// Proximity uses the signed-halfword XZ offset and notice radius; noise also
/// selects ALERT. Requires live actor/player transforms in the same parent
/// frame, initialized work and scratch space for the range test.
static void _actor401800Dormant(Task* task)
{
    enum {
        ACTOR_401800_ANIM_DORMANT           = 14,
        ACTOR_401800_ANIM_DORMANT_VARIATION = 15,
        ACTOR_401800_DORMANT_RECHECK_TICKS  = 2401,
        ACTOR_401800_DORMANT_SKIP_MASK      = 15
    };
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    SVECTOR          toPlayer;
    u16              elapsedTicks;
    u32              randomState;

    work = task->work;
    if (work->stateEntered != 0) {
        model        = task->extra.tmd;
        enemy        = task->spawnArg2.pointer;
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = ACTOR_401800_ANIM_DORMANT;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    elapsedTicks = (u16)work->stateTimer;
    // The late random skip also skips awareness and animation advancement.
    if (work->stateTimer >= ACTOR_401800_DORMANT_RECHECK_TICKS) {
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        if (!((randomState >> 16) & ACTOR_401800_DORMANT_SKIP_MASK)) {
            return;
        }
    } else {
        work->stateTimer = (s16)(elapsedTicks + 1);
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &toPlayer);
    if (!_oddStrangerOutOfRange(&toPlayer, work->noticeRadius)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    _oddStrangerDriveAnimation(task);
    if (work->animId == ACTOR_401800_ANIM_DORMANT) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomState;
            if ((randomState >> 16) & 1) {
                work->animId      = ACTOR_401800_ANIM_DORMANT_VARIATION;
                work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
                _oddStrangerDriveAnimation(task);
            }
        }
    }
    if (work->animId == ACTOR_401800_ANIM_DORMANT_VARIATION && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_401800_ANIM_DORMANT;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        _oddStrangerDriveAnimation(task);
    }
}

#include "../../shared/odd_stranger_dormant.inc.c"

#include "../../shared/odd_stranger_patrol.inc.c"

#include "../../shared/odd_stranger_advance.inc.c"

#include "../../shared/odd_stranger_back_off.inc.c"

#include "../../shared/odd_stranger_hold_aim.inc.c"

/// Waits unlockable and aims its look in `ODD_STRANGER_STATE_AMBUSH`.
///
/// Keeps its root heading while moving the look target toward the player's
/// bearing by up to 40 angle units per tick (4096 per turn). Once equal, clear
/// sight and an expired grab cooldown select GRAB, without a range check.
/// Restarts the ambush clip each tick. Requires live actor/player roots in
/// the same parent frame, initialized work and one chase scratch block.
static void _actor401800Ambush(Task* task)
{
    enum {
        ACTOR_401800_ANIM_AMBUSH      = 19,
        ACTOR_401800_AMBUSH_LOOK_STEP = 40
    };
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          root;
    ActorChaseScratch* lookScratch;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401800_ANIM_AMBUSH;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _oddStrangerDriveAnimation(task);
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    lookScratch       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    lookScratch->turn = _actorAngleTurnToPlayer(task, &lookScratch->delta, &gPlayerStatus);
    if (work->lookYawTarget < lookScratch->turn) {
        if (lookScratch->turn - work->lookYawTarget >= (ACTOR_401800_AMBUSH_LOOK_STEP + 1)) {
            work->lookYawTarget = (u16)work->lookYawTarget + ACTOR_401800_AMBUSH_LOOK_STEP;
        } else {
            work->lookYawTarget = lookScratch->turn;
        }
    } else if (work->lookYawTarget - lookScratch->turn >= (ACTOR_401800_AMBUSH_LOOK_STEP + 1)) {
        work->lookYawTarget = (u16)work->lookYawTarget - ACTOR_401800_AMBUSH_LOOK_STEP;
    } else {
        work->lookYawTarget = lookScratch->turn;
    }
    // Settled look, clear sight and cooldown are sufficient; there is no range test.
    if (work->lookYawTarget == lookScratch->turn && _playerDetectionSightBlocked(task) != 1 && work->grabCooldown == 0) {
        work->state = ODD_STRANGER_STATE_GRAB;
    }
    root              = task->extra.tmd->coords;
    lookScratch->turn = ratan2(-root->coord.m[2][0], root->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, lookScratch->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
    _oddStrangerDriveAnimation(task);
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Runs the stationary body-part burst in ODD_STRANGER_STATE_DEATH_BURST.
///
/// Requires live work, enemy/model parts 1, 3, 9 and 12, and loaded effect banks.
/// Entry hides the body, disables lock-on/root grid response, spawns a particle
/// and releases the battle hold with rewards. Updates 3, 5, 7 and 9 launch
/// body-part models, applying the enemy's current area-placement textures.
/// Replaces bank 10's shared model descriptor before each spawn; the loaded
/// model sources must outlive the spawned effects. At update 61 selects HIDDEN
/// without destroying this task. Effect allocation failures are tolerated.
static void _actor401800BurstDeath(Task* task)
{
    enum {
        ACTOR_401800_BURST_PARTICLE_PART  = 1,
        ACTOR_401800_BURST_FIRST_PART     = 9,
        ACTOR_401800_BURST_SECOND_PART    = 12,
        ACTOR_401800_BURST_LAST_PART      = 3,
        ACTOR_401800_BURST_MODEL_SLOT     = 5,
        ACTOR_401800_BURST_LOCAL_OFFSET   = 100,
        ACTOR_401800_BURST_MODEL_ARGUMENT = 0x200, // Child-puff size 512
        ACTOR_401800_BURST_FIRST_TICK     = 3,
        ACTOR_401800_BURST_SECOND_TICK    = 5,
        ACTOR_401800_BURST_THIRD_TICK     = 7,
        ACTOR_401800_BURST_LAST_TICK      = 9,
        ACTOR_401800_BURST_HIDE_TICK      = 61
    };
    SVECTOR          burstOffset;
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              elapsedTicks;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
/// Starts stationary burst death, borrowing the caller's offset vector.
///
/// The task/work/enemy arguments must be side-effect-free live pointers and
/// localOffset a writable SVECTOR lvalue. Arguments are evaluated repeatedly;
/// no identifiers are captured. Hides drawing and lock-on, disables root-grid
/// response, resets the timer, initializes XYZ, spawns the particle and releases
/// one battle hold with rewards. Expands to one compound statement.
#define ACTOR_401800_BEGIN_BURST_DEATH(actorTask, actorWork, enemyRecord, localOffset)                                                                    \
    {                                                                                                                                                     \
        enum {                                                                                                                                            \
            ACTOR_401800_BURST_PARTICLE_PART     = 1,                                                                                                     \
            ACTOR_401800_BURST_LOCAL_OFFSET      = 100,                                                                                                   \
            ACTOR_401800_BURST_PARTICLE_ARGUMENT = 0x10300,                                                                                               \
            ACTOR_401800_BURST_REWARD_ARGUMENT   = 10                                                                                                     \
        };                                                                                                                                                \
        (actorTask)->extra.tmd->flags         = TMD_OBJECT_SKIP_ACTIVE_DRAW;                                                                              \
        (actorWork)->hitBody.radius           = ODD_STRANGER_BODY_RADIUS;                                                                                 \
        (actorWork)->gridBody.flags           = (actorWork)->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);      \
        (enemyRecord)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;                                                                                \
        (actorWork)->lookYawTarget            = 0;                                                                                                        \
        (actorWork)->stateTimer               = 0U;                                                                                                       \
        (localOffset).vx                      = ACTOR_401800_BURST_LOCAL_OFFSET;                                                                          \
        (localOffset).vz                      = 0;                                                                                                        \
        (localOffset).vy                      = 0;                                                                                                        \
        effectSpawn(EFFECT_030, (actorTask)->extra.tmd->coords + ACTOR_401800_BURST_PARTICLE_PART, ACTOR_401800_BURST_PARTICLE_ARGUMENT, &(localOffset)); \
        sceneReleaseBattleRefWithRewards((actorTask), ACTOR_401800_BURST_REWARD_ARGUMENT);                                                                \
    }

    if (work->stateEntered != 0) {
        ACTOR_401800_BEGIN_BURST_DEATH(task, work, enemy, burstOffset);
    }
#undef ACTOR_401800_BEGIN_BURST_DEATH
    elapsedTicks     = work->stateTimer + 1;
    work->stateTimer = elapsedTicks;
    // Publish each model immediately before its independent effect is spawned.
    if ((s16)elapsedTicks == ACTOR_401800_BURST_FIRST_TICK) {
        D_80114B34[ACTOR_401800_BURST_MODEL_SLOT].data.model = &gOddStrangerBurstModelA;
        burstOffset.vz                                       = ACTOR_401800_BURST_LOCAL_OFFSET;
        burstOffset.vy                                       = 0;
        burstOffset.vx                                       = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + ACTOR_401800_BURST_FIRST_PART, ACTOR_401800_BURST_MODEL_ARGUMENT, &burstOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401800_BURST_SECOND_TICK) {
        D_80114B34[ACTOR_401800_BURST_MODEL_SLOT].data.model = &_gActor401800Model12280;
        // The binary leaves this invocation's Z offset uninitialized.
        burstOffset.vy = 0;
        burstOffset.vx = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + ACTOR_401800_BURST_SECOND_PART, ACTOR_401800_BURST_MODEL_ARGUMENT, &burstOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401800_BURST_THIRD_TICK) {
        D_80114B34[ACTOR_401800_BURST_MODEL_SLOT].data.model = &gOddStrangerBurstModelA;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + ACTOR_401800_BURST_PARTICLE_PART, ACTOR_401800_BURST_MODEL_ARGUMENT, NULL), enemy);
    }
    if (work->stateTimer == ACTOR_401800_BURST_LAST_TICK) {
        D_80114B34[ACTOR_401800_BURST_MODEL_SLOT].data.model = &gOddStrangerBurstModelC;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + ACTOR_401800_BURST_LAST_PART, ACTOR_401800_BURST_MODEL_ARGUMENT, NULL), enemy);
    }
    if (work->stateTimer >= ACTOR_401800_BURST_HIDE_TICK) {
        work->state = ODD_STRANGER_STATE_HIDDEN;
    }
}

#include "../../shared/odd_stranger_walking_death.inc.c"

#include "../../shared/odd_stranger_stalk.inc.c"

static const OddStrangerStateTable gOddStrangerStates = { {
    _actor401800Hidden,
    _oddStrangerPlayWalk,
    _oddStrangerPlayRun,
    _oddStrangerPlayDown,
    _oddStrangerStatusHold,
    _oddStrangerFlinch,
    _oddStrangerAlert,
    _actor401800Chase,
    _oddStrangerCircleDash,
    _oddStrangerTurnAround,
    _oddStrangerSidestep,
    _actor401800Grab,
    _oddStrangerGrabPull,
    _oddStrangerGrabStrike,
    _oddStrangerGrabRelease,
    _oddStrangerRiseBack,
    _actor401800RiseFront,
    _oddStrangerDown,
    NULL,
    _actor401800FallBack,
    _actor401800FallFront,
    _oddStrangerDeathBurn,
    _actor401800Dormant,
    _oddStrangerDormantScripted,
    _oddStrangerPatrol,
    _oddStrangerBackOff,
    _oddStrangerSlide,
    _oddStrangerWatch,
    _actor401800Ambush,
    _actor401800BurstDeath,
    _oddStrangerStalk,
    _actor401800RefallBack,
    _actor401800RefallFront,
    _oddStrangerWalkingDeath,
} };

#include "../../shared/odd_stranger_tick.inc.c"

/// Ignores message 2015 without accessing the receiver or either payload.
///
/// The binary leaves the reply register unspecified, so callers must ignore
/// the result. The signed callback signature retains that fall-through.
static s32 _actor401800IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra)
{
}

/// The enemy task's three handlers, indexed by `Task::state`: the first
/// initialises the actor, the second runs it every frame, and the third tears
/// the enemy down.
static const EnemyTaskFuncTable3 D_actor_401800_80132064 = { {
    _actor401800Initialize,
    _oddStrangerTick,
    enemyDestroy,
} };

#include "../../shared/odd_stranger_play_message.inc.c"

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

#include "../../shared/odd_stranger_apply_command.inc.c"

#include "../../shared/odd_stranger_exit.inc.c"

/// Hides and disables lock-on on entry to `ODD_STRANGER_STATE_HIDDEN`.
///
/// Also disables attack-body pairing and root grid response. Requires a live
/// TMD task, Odd Stranger work and enemy; later ticks do nothing.
static void _actor401800Hidden(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                                              = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->attackBody.flags                                    = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags                                      = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/odd_stranger_script_pose_2.inc.c"

#include "../../shared/odd_stranger_script_pose_3.inc.c"

#include "../../shared/odd_stranger_script_pose_b.inc.c"

#include "../../shared/odd_stranger_script_pose_d.inc.c"

#include "../../shared/odd_stranger_script_pose_8.inc.c"

/// Gets up from a front knockdown in `ODD_STRANGER_STATE_RISE_FRONT`.
///
/// Restarts the front-rise clip at the pursuit rate, clears look angles and
/// counts ticks for hit reactions to interrupt the recovery. The reported
/// clip boundary selects CHASE. Requires live work, enemy and model.
static void _actor401800RiseFront(Task* task)
{
    enum { ACTOR_401800_ANIM_RISE_FRONT = 22 };
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401800_ANIM_RISE_FRONT;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        work->animRate                = work->chaseRate;
    }
    work->stateTimer = (u16)(work->stateTimer + 1);
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}

#include "../../shared/odd_stranger_idle.inc.c"

/// Dispatches this enemy task's initialize, tick or destroy handler.
///
/// `task->state` must be 0..2 and `spawnArg2.pointer` must borrow the live
/// Enemy associated with this task. Copies the three handlers before dispatch;
/// the initialization handler establishes the task work used by later ticks.
static void _actor401800Task(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = D_actor_401800_80132064;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
