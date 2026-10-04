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

static const OddStrangerStateTable gOddStrangerStates;

/// Payload of the `0x3FF` message `oddStrangerGrabHold` sends: the same
/// 0x14-byte animation record other actors keep as `AnimationPlayRequest` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern AnimationPlayRequest gOddStrangerPlayerAnim;

extern AnimationSet* D_actor_401800_801559F8[];
extern AnimationSet* D_actor_401800_801559F0[];

/// Twelve `SVECTOR` hit positions `oddStrangerSpawnHitEffect` picks from by
/// damage magnitude: the low four when the hit is light, the high two when it
/// is heavy, and the last four on the `arg1 > 0` / `arg1 <= 0` split in
/// between. The fourth halfword (`pad`, unused by the effect itself) is the
/// model part index `func_800FDB18` anchors the spawned effect to. Same role
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

/// Payload `oddStrangerGrab` fills and sends with message 0x3E9.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Frame counter the chase body of `func_actor_80136EAC` accumulates its step
/// `slideStep` into and the init body clears; the aim-and-rescale body reads it
/// back as the phase of the step it walks. Same role `Actor01900_D172FC` plays
/// for actor 01900.
extern u16 gOddStrangerChaseDistance;

/// The block `oddStrangerDormant` posts into `gOddStrangerAnimSets[16]`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `gActor401300Animation20D98`.
extern AnimationSet gOddStrangerDormantAnimSet;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401800_8013BB10`.

/// The records closing three of the overlay's model streams, which
/// `func_actor_401800_8013BB10` points `D_80114B34[5].data.model` at before spawning: the
/// 0x60030 debris burst, then the 0xA0005 fan the step counter trips at 3 and 5
/// and the two 0xA0005 bursts at 7 and 9.
extern TmdSource gOddStrangerBurstModelA;
static TmdSource _gActor401800Model12280;
extern TmdSource gOddStrangerBurstModelC;

#include "../../shared/actor_contacts.h"

static void func_actor_401800_8013423C(Enemy* enemy, Task* actor);
static void func_actor_401800_8013E138(Task* arg0);
static void func_actor_401800_8013E4F0(Task* arg0);
static void func_actor_401800_801381E4(Task* arg0);

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
s32              oddStrangerApplyCommand(Task*, s32, u16*, s32);
s32              func_actor_401800_8013DCB4(Task*, s32, s32, s32);
void             func_actor_401800_8013E68C(Task*);

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

TaskMessageEntry D_actor_401800_80155A80[8] = {
    { 2015, func_actor_401800_8013DCB4 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, oddStrangerPlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { 2014, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, oddStrangerApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 gOddStrangerChaseDistance = 0;

TaskDesc D_actor_401800_80155AC4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_401800_8013E68C, { .model = &_gActor401800OddStrangerBody } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

OddStrangerTransformStorage gOddStrangerGrabTransform;

GameActorButtonPressHold D_actor_401800_80155AF8;

static __inline__ void Actor401800_BindMatrices(Task* actor);
static __inline__ s32  Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r);
static void            func_actor_401800_80136560(Task* arg0);
static __inline__ void Actor401800_ViewWalk(GfxCoord* coord, SVECTOR* svp, SVECTOR* dir);
static __inline__ void Actor401800_SetGrabAnim(void);
static void            func_actor_401800_8013945C(Task* arg0);
static void            func_actor_401800_8013971C(Task* arg0);
static void            func_actor_401800_80139870(Task* arg0);
static void            func_actor_401800_801399C4(Task* arg0);
static void            func_actor_401800_80139D60(Task* arg0);
static void            func_actor_401800_8013B784(Task* arg0);
static void            func_actor_401800_8013BB10(Task* arg0);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Binds the work block's light and color matrices onto the model object.
/// Same body as `Actor01900_BindMatrices` / `Actor401300_BindMatrices`.
static __inline__ void Actor401800_BindMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

/// Enemy init: allocates the work block, binds the model matrices, sets up both
/// animation contexts and the three hit/body `WorldCollisionBody` nodes, then picks the
/// starting state and tint row from the spawn flags and rescales the model.
/// Same body as `Actor01900_Fn02018` / `func_actor_401300_80134454`.
static void func_actor_401800_8013423C(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    OddStrangerWork*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(OddStrangerWork), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = oddStrangerExit;
    Actor401800_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401800_8013E6F0.hpMax;
    enemy->param                  = &D_actor_401800_8013E6F0;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, gOddStrangerAnimSets, obj,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, gOddStrangerAnimSets, obj,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ODD_STRANGER_ANIM_REQUEST_RESET;
    work->animId        = 2;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = 0x10;
    work->animRate      = 0x10;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->chaseRate++;
    } else {
        work->chaseRate--;
    }
    oddStrangerDrive(actor);

    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = root;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0xAC;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30012;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    body                   = &work->hitBody;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->hitContacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30000;
    body->radius           = 0x12C;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->attackBody;
    head->coord            = &actor->extra.tmd->coords[6];
    head->context.contacts = work->attackContacts;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x180;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

    work->patrolTarget      = 0;
    work->patrolPoints[0].x = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->patrolPoints[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->patrolPoints[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = D_actor_401800_80155A80;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    kind                       = (actor->spawnArg1.value >> 16);
    switch (kind & 0xF) {
        case 2:
            work->prevState = -1;
            work->state     = ODD_STRANGER_STATE_HIDDEN;
            break;
        case 4:
            work->prevState = -1;
            work->state     = ODD_STRANGER_STATE_DORMANT;
            break;
        default:
            work->prevState = -1;
            work->state     = ODD_STRANGER_STATE_PATROL;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->downFramesBase = D_actor_401800_8013E700[0].downFramesBase;
            work->sidestepAngle  = D_actor_401800_8013E700[0].sidestepAngle;
            work->sidestepDelay  = D_actor_401800_8013E700[0].sidestepDelay;
            work->noticeRadius   = D_actor_401800_8013E700[0].noticeRadius;
            break;
        case 1:
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

    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->bodyPosCursor = 0;
    actor->state++;
}

#include "../../shared/odd_stranger_spawn_hit_effect.inc.c"

#include "../../shared/odd_stranger_take_hit.inc.c"

#include "../../shared/odd_stranger_stunned.inc.c"

#include "../../shared/odd_stranger_face_player.inc.c"

/// Push the root coordinate out of a `WorldCollisionContact` table: take a 0x34 scratch, seed
/// its position from the second coordinate, then walk the records until `count`
/// or a zero `key`. Each kind 0x10000 / 0x30000 record contributes half its
/// offset along X and Z, normalised to length 0x96 first when it is longer than
/// that; `hit` reports whether one was seen.
/// Same body as `Actor01900_Fn03FF8` / `func_actor_401300_80132910`, with the
/// coordinate update written out in both arms of the length test.
s32 oddStrangerPushContacts(Task* arg0, WorldCollisionContact* recs, s16 count)
{
    ActorBodyPushScratch* head;
    ActorBodyPushScratch* s;
    ActorBodyPushScratch* blk;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].composeStamp    = GRAPHICS_COORD_DIRTY;
    head                                       = SCRATCH_STACK_CURSOR(ActorBodyPushScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorBodyPushScratch) = blk;
    s                                          = blk;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
    s->position.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->position.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->position.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit         = 0;
    for (s->recordIndex = 0; s->recordIndex < count; s->recordIndex++) {
        if (recs[s->recordIndex].key.value == 0) {
            s->marks[s->recordIndex] = ACTOR_BODY_PUSH_MARK_END;
            break;
        }
        s->kind = recs[s->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            worldCollisionCalcContactViewOffset(&s->position, &recs[s->recordIndex], &s->offset);
            s->offsetLength = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->offsetLength = SquareRoot0(s->offsetLength);
            if (s->offsetLength >= 0x96) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x96);
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
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return s->hit;
}

static __inline__ s32 Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r)
{
    OverlayRangeScratch* head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    head[-1].dx                               = d->vx;
    blk                                       = head - 1;
    blk->dz                                   = d->vz;
    blk->radius                               = r;
    head[-1].dx                              *= head[-1].dx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = blk;
    blk->dz                                  *= blk->dz;
    blk->radius                              *= blk->radius;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = head;
    ret                                       = head[-1].dx + blk->dz >= blk->radius;
    return ret;
}

static void func_actor_401800_80136560(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          turnCoord;
    GfxCoord*          facing;
    void**             scratch;
    ActorChaseScratch* head;
    ActorChaseScratch* block;
    ActorChaseScratch* chase;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->state = ODD_STRANGER_STATE_STALK;
        return;
    }
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->hitBody.radius    = 0x12C;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = 3;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        oddStrangerDrive(arg0);
        work->dashCount   = 0;
        work->stateTimer  = 0;
        work->exitCounter = 0;
        return;
    }
    work->stateTimer++;
    scratch                                     = SCRATCH_HEAD_ADDR;
    head                                        = SCRATCH_HEAD_AT(scratch, ActorChaseScratch);
    block                                       = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorChaseScratch) = block;
    chase                                       = block;
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    coord                                 = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    chase->delta.vy                       = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    chase->delta.vz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    chase->playerYaw     = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                                  gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    coord                = arg0->extra.tmd->coords;
    head[-1].delta.vx    = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    chase->delta.vy      = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    chase->delta.vz      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yawFromPlayer = actorNormalizeYaw(chase->yawFromPlayer);
    turnCoord            = arg0->extra.tmd->coords;
    chase->turn          = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]));
    work->lookYawTarget  = chase->turn;
    if (abs(chase->yawFromPlayer - chase->playerYaw) < 0x44) {
        if (((s16)work->sidestepDelay + work->sidestepCount / 2) < work->stateTimer) {
            if (abs(chase->turn) < 0x80) {
                if (Actor401800_ChaseOutOfRange(&chase->delta, 0x708) && detectSightBlocked(arg0) != 1) {
                    work->state = ODD_STRANGER_STATE_SIDESTEP;
                }
            }
        }
    }
    if (chase->turn < 0x200) {
        if (!Actor401800_ChaseOutOfRange(&chase->delta, 0x44C) && detectSightBlocked(arg0) != 1 && work->grabCooldown == 0) {
            work->state = ODD_STRANGER_STATE_GRAB;
        }
    }
    if (chase->turn > 0x30) {
        chase->turn = 0x30;
    }
    if (chase->turn < -0x30) {
        chase->turn = -0x30;
    }
    facing       = arg0->extra.tmd->coords;
    chase->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 3) {
        if (work->blendActive == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->chaseRate + 2) * 0x42) / 18) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->chaseRate + 2) * 0x42) / 18);
            }
        } else if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, (((work->chaseRate + 2) * 0x42) / 18) >> 2) != 0) {
            actorMoveForwardNonzero(arg0->extra.tmd->coords, (((work->chaseRate + 2) * 0x42) / 18) >> 2);
        }
    } else if (work->rig.slots[1].status.fields.flags & 1) {
        work->animId      = 3;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

#include "../../shared/odd_stranger_chase.inc.c"

#include "../../shared/odd_stranger_turn_around.inc.c"

#include "../../shared/odd_stranger_sidestep.inc.c"

static __inline__ void Actor401800_ViewWalk(GfxCoord* coord, SVECTOR* svp, SVECTOR* dir)
{
    VECTOR    vec;
    SVECTOR*  outp;
    u8*       head;
    VECTOR*   vecp;
    GfxCoord* p;
    GfxCoord* view;
    s32       flag;
    s32*      flagp;
    Task*     player;

    player                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 8;
    outp                     = (SVECTOR*)(head - 8);
    view                     = &gGfxViewCoord;
    vecp                     = &vec;
    flagp                    = &flag;
    outp->vx                 = 0;
    outp->vy                 = 0;
    outp->vz                 = 0;
    p                        = &player->extra.tmd->coords[1];
    svp->vx                  = outp->vx;
    svp->vy                  = outp->vy;
    svp->vz                  = outp->vz;
    for (;;) {
        if (p->parent != NULL) {
            if (p != view) {
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr();
                gte_stlvnl(vecp);
                gte_stflg(flagp);
                svp->vx = vec.vx;
                svp->vy = vec.vy;
                svp->vz = vec.vz;
                p       = p->parent;
                continue;
            }
            outp->vx = svp->vx;
            outp->vy = svp->vy;
            outp->vz = svp->vz;
        }
        break;
    }
    dir->vx = outp->vx - coord->coord.t[0];
    dir->vy = 0;
    dir->vz = outp->vz - coord->coord.t[2];
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static __inline__ void Actor401800_SetGrabAnim(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
        gOddStrangerPlayerAnim.source.sets = D_actor_401800_801559F8;
    } else {
        gOddStrangerPlayerAnim.source.sets = D_actor_401800_801559F0;
    }
}

static void func_actor_401800_801381E4(Task* arg0)
{
    Enemy*           enemy;
    Task*            player;
    GameActor*       gactor;
    PlayerStatus*    config;
    OddStrangerWork* work;
    SVECTOR          dir;
    SVECTOR          sv;
    s32              ang;
    u16              step;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gactor = (GameActor*)player->work;
    config = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->grabCooldown            = 0xA;
        work->hitBody.radius          = 0x12C;
        work->sidestepCount           = 0;
        work->playerHeld              = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        work->animId                  = 4;
        oddStrangerDrive(arg0);
        ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->grabStartPos.vx = arg0->extra.tmd->coords->coord.t[0];
        work->grabStartPos.vy = arg0->extra.tmd->coords->coord.t[1];
        work->grabStartPos.vz = arg0->extra.tmd->coords->coord.t[2];
        work->stateTimer      = 0;
        return;
    }
    step             = (u16)work->stateTimer + 1;
    work->stateTimer = step;
    if ((s16)step == 1) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->grabStartPos.vx = arg0->extra.tmd->coords->coord.t[0];
        work->grabStartPos.vy = arg0->extra.tmd->coords->coord.t[1];
        work->grabStartPos.vz = arg0->extra.tmd->coords->coord.t[2];
        Actor401800_ViewWalk(arg0->extra.tmd->coords, &sv, &dir);
        ang = actorViewYaw(arg0->extra.tmd->coords, &dir);
        gfxRotMatrixY(&arg0->extra.tmd->coords[0].coord, ang, 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        dir.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy                                = 0;
        dir.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
        work->grabCooldown                    = 0xA;
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x10 && gactor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        Actor401800_ViewWalk(arg0->extra.tmd->coords, &sv, &dir);
        ang = actorViewYaw(arg0->extra.tmd->coords, &dir);
        if (ang < 0) {
            ang = -ang;
        }
        if (ang < 0x20) {
            if (!oddStrangerOutOfRange(&dir, 0x5DC)) {
                Actor401800_SetGrabAnim();
                D_actor_401800_80155AF8.pressCount = 8;
                do {
                    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_401800_80155AF8, 0) == 0) {
                        work->state                        = ODD_STRANGER_STATE_GRAB_PULL;
                        work->playerHeld                   = 1;
                        gOddStrangerPlayerAnim.animationId = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &gOddStrangerPlayerAnim, 0);
                    }
                } while (0);
            }
        }
    }
    if (work->animId == 4 && (work->rig.slots[1].status.fields.flags & 1)) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 0x11U) {
        dir.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy = 0;
        dir.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!oddStrangerOutOfRange(&dir, 0x578)) {
            VectorNormalSS(&dir, &dir);
            gte_lddp(0xA);
            gte_ldsv(&dir);
            gte_gpf12();
            gte_stsv(&dir);
            arg0->extra.tmd->coords->coord.t[0]  += dir.vx;
            arg0->extra.tmd->coords->coord.t[2]  += dir.vz;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
}

#include "../../shared/odd_stranger_grab.inc.c"

#include "../../shared/odd_stranger_grab_hold.inc.c"

#include "../../shared/odd_stranger_grab_release.inc.c"

/// Per-frame body of the live actor armed into state 1: raises the same
/// animation slots as `func_actor_401800_8013971C` but leaves `animId = 0xA`
/// (with `animRequest = 1` and `blendActive` cleared), then, while that slot is
/// still `0xA`, advances the actor along its own local Z by a fixed `-0x57`
/// once `detectPlayerOutOfReach` says the path is clear. The `0xA` branch
/// then flips the slots to `0xB`/2 and ticks the animation a second time before
/// the two contact records are rebuilt, after which work bit 0 picks `state`
/// from the enemy's HP sign and its buildup bit (`reactionFlags`).
static void func_actor_401800_8013945C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = 0xA;
        work->blendActive             = 0;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if ((work->animId == 0xA) && ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x57) != 0)) {
        actorStepForward(arg0->extra.tmd->coords, -0x57);
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].status.fields.flags & 1) && (work->animId == 0xA)) {
        work->animId      = 0xB;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
        oddStrangerDrive(arg0);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & 1) && (work->animId == 0xB)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

/// Per-frame body of the live actor: arms the animation slots and the two
/// `hitBody` / `gridBody` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `state` from the
/// enemy's HP sign and its buildup bit (`reactionFlags`). Same body as `Actor01900_Fn09BE8`.
static void func_actor_401800_8013971C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0xB;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

/// Second per-frame body of the live actor: as `func_actor_401800_8013971C`,
/// but it arms the animation slots with `animId = 0x19` and skips the
/// `field_5A` clip rebuild.
static void func_actor_401800_80139870(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0x19;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

/// Third per-frame body of the live actor: `func_actor_401800_8013971C` with
/// the animation slots armed at 1 / 0xC, and its `state` selector driven by
/// work bit 0 instead of bit 8. Same body as `func_actor_401800_8013971C`
/// apart from those three constants.
static void func_actor_401800_801399C4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = 0xC;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 1) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

#include "../../shared/odd_stranger_die.inc.c"

/// Countdown body: on the live-actor flag re-allocates the model's buffers,
/// keeps a copy of `colorMtx` in `savedColorMtx` and restarts the step
/// counter in state 0xE. The counter then runs to 0x961, rerolling the LCG each
/// frame past it and bailing for that frame on every 0xF-th draw; the surviving
/// frames re-test the squared XZ offset to the player against
/// `noticeRadius` and arm `gSceneCombatState` state 6 on a miss — bit 0x50000 there arms
/// it the same way. After the shared per-frame tick the body flips between
/// states 0xE and 0xF, one LCG draw per attempt, on the two `rig.slots[1].status` mask
/// bits. Same shape as `oddStrangerDormant`.
static void func_actor_401800_80139D60(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    u16              step;
    u32              lcg;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj        = arg0->extra.tmd;
        enemy      = arg0->spawnArg2.pointer;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = 0xE;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    step = (u16)work->stateTimer;
    if (work->stateTimer >= 0x961) {
        lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = lcg;
        if (!((lcg >> 16) & 0xF)) {
            return;
        }
    } else {
        work->stateTimer = (s16)(step + 1);
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!oddStrangerOutOfRange(d, work->noticeRadius)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    oddStrangerDrive(arg0);
    if (work->animId == 0xE) {
        if (work->rig.slots[1].status.fields.flags & 2) {
            lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = lcg;
            if ((lcg >> 16) & 1) {
                work->animId      = 0xF;
                work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
                oddStrangerDrive(arg0);
            }
        }
    }
    if (work->animId == 0xF && (work->rig.slots[1].status.fields.flags & 1)) {
        work->animId      = 0xE;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        oddStrangerDrive(arg0);
    }
}

#include "../../shared/odd_stranger_dormant.inc.c"

#include "../../shared/odd_stranger_patrol.inc.c"

#include "../../shared/odd_stranger_advance.inc.c"

#include "../../shared/odd_stranger_back_off.inc.c"

#include "../../shared/odd_stranger_hold_aim.inc.c"

/// Aim the actor at the player and rescale its root coordinate, turning the
/// stored yaw toward the target by at most 0x28 a frame instead of the hard
/// clamp `oddStrangerFacePlayer` uses. On the live flag it resets the
/// model buffers and re-arms the step countdown; otherwise it hands the
/// player offset and the new yaw to the actor's state body and rebuilds the
/// matrix at scale 0x1194.
static void func_actor_401800_8013B784(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->hitBody.radius    = 0x12C;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = 0x10;
        work->animId            = 0x13;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        oddStrangerDrive(arg0);
        oddStrangerDrive(arg0);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->lookYawTarget < aim->turn) {
        if (aim->turn - work->lookYawTarget >= 0x29) {
            work->lookYawTarget = (u16)work->lookYawTarget + 0x28;
        } else {
            work->lookYawTarget = aim->turn;
        }
    } else if (work->lookYawTarget - aim->turn >= 0x29) {
        work->lookYawTarget = (u16)work->lookYawTarget - 0x28;
    } else {
        work->lookYawTarget = aim->turn;
    }
    if (work->lookYawTarget == aim->turn && detectSightBlocked(arg0) != 1 && work->grabCooldown == 0) {
        work->state = ODD_STRANGER_STATE_GRAB;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
    oddStrangerDrive(arg0);
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Step-driven effect spawner for the actor's live ramp: while the spawn flag
/// is set the actor crouches (0x8C8 node pitched to 0x12C, 0xA08 flags bit
/// 0x4000 cleared), plays the 0x60030 debris burst and hands the task to the
/// state-F0 list; the step counter then fires the 0xA0005 effects at 3, 5, 7
/// and 9, each tinted from the enemy's area record, and parks the actor at 0x3D.
static void func_actor_401800_8013BB10(Task* arg0)
{
    SVECTOR          vec;
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = 0x12C;
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0U;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelA;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
    }
    if (work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor401800Model12280;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec), enemy);
    }
    if (work->stateTimer == 7) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelA;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if (work->stateTimer == 9) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelC;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if (work->stateTimer >= 0x3D) {
        work->state = ODD_STRANGER_STATE_HIDDEN;
    }
}

#include "../../shared/odd_stranger_walking_death.inc.c"

#include "../../shared/odd_stranger_stalk.inc.c"

static const OddStrangerStateTable gOddStrangerStates = { {
    func_actor_401800_8013E138,
    oddStrangerScriptPose2,
    oddStrangerScriptPose3,
    oddStrangerScriptPoseB,
    oddStrangerStunned,
    oddStrangerScriptPoseD,
    oddStrangerFacePlayer,
    func_actor_401800_80136560,
    oddStrangerChase,
    oddStrangerTurnAround,
    oddStrangerSidestep,
    func_actor_401800_801381E4,
    oddStrangerGrab,
    oddStrangerGrabHold,
    oddStrangerGrabRelease,
    oddStrangerScriptPose8,
    func_actor_401800_8013E4F0,
    oddStrangerIdle,
    NULL,
    func_actor_401800_8013945C,
    func_actor_401800_801399C4,
    oddStrangerDie,
    func_actor_401800_80139D60,
    oddStrangerDormant,
    oddStrangerPatrol,
    oddStrangerBackOff,
    oddStrangerAdvance,
    oddStrangerHoldAim,
    func_actor_401800_8013B784,
    func_actor_401800_8013BB10,
    oddStrangerStalk,
    func_actor_401800_8013971C,
    func_actor_401800_80139870,
    oddStrangerWalkingDeath,
} };

#include "../../shared/odd_stranger_tick.inc.c"

s32 func_actor_401800_8013DCB4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The enemy task's three handlers, indexed by `Task::state`: the first
/// initialises the actor, the second runs it every frame, and the third tears
/// the enemy down.
static const EnemyTaskFuncTable3 D_actor_401800_80132064 = { {
    func_actor_401800_8013423C,
    oddStrangerTick,
    enemyDestroy,
} };

#include "../../shared/odd_stranger_play_message.inc.c"

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

#include "../../shared/odd_stranger_apply_command.inc.c"

#include "../../shared/odd_stranger_exit.inc.c"

static void func_actor_401800_8013E138(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

#include "../../shared/odd_stranger_script_pose_2.inc.c"

#include "../../shared/odd_stranger_script_pose_3.inc.c"

#include "../../shared/odd_stranger_script_pose_b.inc.c"

#include "../../shared/odd_stranger_script_pose_d.inc.c"

#include "../../shared/odd_stranger_script_pose_8.inc.c"

static void func_actor_401800_8013E4F0(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x12C;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0x16;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        work->animRate                = work->chaseRate;
    }
    work->stateTimer = (u16)(work->stateTimer + 1);
    oddStrangerDrive(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}

#include "../../shared/odd_stranger_idle.inc.c"

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401800_8013E68C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_401800_80132064;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
