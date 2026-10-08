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

#include "main/areas.h"
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
#include "../../shared/actor_contacts.h"
// Which of the two Odd Stranger builds this package is (odd_stranger.h).
#define ODD_STRANGER_VARIANT 1
#include "../../shared/odd_stranger.h"

/// Animation bank `func_actor_401000_80133274` hands to both `animationInitContext`
/// calls; the same `s32` the 401300 sibling keeps in `D_actor_401300_80158838`.
extern AnimationSet* gOddStrangerAnimSets[46];

/// Parameter pair `func_actor_401000_80133274` installs as `Enemy::param`
/// and reads `hpMax` out of as the actor's initial `field_40`.
extern EnemyParams D_actor_401000_8013E09C;

/// Three combat-parameter records `func_actor_401000_80133274` picks between
/// with `Task::spawnArg1 & 0xF`.
extern ActorStrangerVariant D_actor_401000_8013E0AC[3];

/// Animation table `func_actor_401000_80133274` writes to `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_401000_80154F90[8];

/// Overlay-data word `_oddStrangerDormantScripted` points
/// `gOddStrangerAnimSets[16]` at on entering its state.
extern AnimationSet gOddStrangerDormantAnimSet;

/// Message 0x3FF payload of `_oddStrangerGrabStrike` and
/// `_oddStrangerGrabRelease`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern AnimationPlayRequest gOddStrangerPlayerAnim;

/// Animation blocks selected for the grab by the player-character flag.
extern AnimationSet* D_actor_401000_80154F00[7];

/// Free-running scroll the actor's forward draw accumulates into:
/// `_oddStrangerCircleDash` adds `slideStep` to it every frame, and the
/// walk state zeroes it on entry. The same slot `Actor01900` keeps in
/// `Actor01900_D172FC`.
extern u16 gOddStrangerChaseDistance;

/// Twelve `SVECTOR` hit positions `_oddStrangerSpawnHitEffect` picks from by
/// the hit bearing relative to its facing. The fourth halfword (`pad`, unused
/// by the effect) is the model part index the spawned effect anchors to. Same table as the
/// `Actor00100_D1B9F4` one `Actor00100_Fn03340` reads.
extern SVECTOR gOddStrangerHitOffsets[12];

/// The two `ActorHeightClamp` rows `_actor401000ClampRootHeight` and
/// `_actor401000ApplyGridPushback` walk.
extern ActorHeightClamp D_actor_401000_80154FD0[];

/// Message 0x3E9 payload of `_oddStrangerGrabPull`: the player task's
/// world position, then the yaw from the actor to it, handed straight to the
/// slot-3 handler. The 401000 twin of the block `_actor401300StateGrabPull`
/// keeps inline at `_Actor401300Work.playerPlacement`.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Gameplay slot `effectSpawn` effects read their model data from; set before
/// each spawn in `func_actor_401000_8013B1E4`.

/// The records closing four of the overlay's model streams, which
/// `func_actor_401000_8013B1E4` points `D_80114B34[5].data.model` at before spawning, one per
/// animation-latch key frame (`stateTimer` 3, 5, 7, 8).
extern TmdSource gOddStrangerBurstModelA;
static TmdSource _gActor401000Model123EC;
extern TmdSource gOddStrangerBurstModelC;
extern TmdSource gOddStrangerBurstModelB;

/// Handlers defined after the state table and the init that name them.
static void _actor401000Hidden(Task* task);
static void _actor401000RiseFront(Task* task);

/// Integer part of the last delta `_actorContactApplyGridPushback` resolved.
extern SVECTOR ActorContact_ScratchPosition;

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

extern OddStrangerTransformStorage gOddStrangerGrabTransform;

extern GameActorButtonPressHold D_actor_401000_80155038;

/// Transition table the clip change seeks through: one byte per
/// (playing clip, requested clip) pair, 0x2D requested clips to a row.
extern s8 gOddStrangerTransitions[45][45];

static TmdSource _gActor401000StrangerBody;
void             func_actor_401000_8013E038(Task*);

static AnimationSet _gActor401000Animation20660;
static AnimationSet _gActor401000Animation21058;
static AnimationSet _gActor401000Animation219B8;

static AnimationSet _gActor401000Animation14E90;
static AnimationSet _gActor401000Animation15914;
static AnimationSet _gActor401000Animation16384;
static AnimationSet _gActor401000Animation16964;
static AnimationSet _gActor401000Animation16F48;
static AnimationSet _gActor401000Animation1799C;
static AnimationSet _gActor401000Animation186E0;
static AnimationSet _gActor401000Animation18F50;
static AnimationSet _gActor401000Animation192E0;
static AnimationSet _gActor401000Animation19B18;
static AnimationSet _gActor401000Animation1A068;
static AnimationSet _gActor401000Animation1AF80;
static AnimationSet _gActor401000Animation1B324;
static AnimationSet _gActor401000Animation1B6D8;
static AnimationSet _gActor401000Animation1BA5C;
static AnimationSet _gActor401000Animation1C150;
static AnimationSet _gActor401000Animation1C9B4;
static AnimationSet _gActor401000Animation1D22C;
static AnimationSet _gActor401000Animation1D87C;
static AnimationSet _gActor401000Animation1E250;
static AnimationSet _gActor401000Animation1F0C8;
static AnimationSet _gActor401000Animation1FFD8;

DamageAttack D_actor_401000_8013E094[2] = {
    { 20, 7 },
    { 18, 0 },
};

EnemyParams D_actor_401000_8013E09C = { D_actor_401000_8013E094, 180, 42, 82, 4, 100, 10, 100, 0 };

ActorStrangerVariant D_actor_401000_8013E0AC[3] = {
    { 20, 900, 12, 2000, { 0, 0, 0, 0 } },
    { 10, 800, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 500, 12, 3000, { 0, 0, 0, 0 } },
};

static TmdBone _gActor401000StrangerBodySkeleton[19] = {
#include "assets/stranger_body_skeleton.inc"
};

static u32 _gActor401000StrangerBodyPartVerts[19] = {
#include "assets/stranger_body_partVerts.inc"
};

static SVECTOR _gActor401000StrangerBodyVerts[311] = {
#include "assets/stranger_body_verts.inc"
};

static SVECTOR _gActor401000StrangerBodyNormals[309] = {
#include "assets/stranger_body_normals.inc"
};

static u32 _gActor401000StrangerBodyStream[4027] = {
#include "assets/stranger_body_stream.inc"
};

static TmdSource _gActor401000StrangerBody = {
    0,
    20180,
    7860,
    19,
    _gActor401000StrangerBodyPartVerts,
    _gActor401000StrangerBodyVerts,
    _gActor401000StrangerBodyNormals,
    _gActor401000StrangerBodySkeleton,
    _gActor401000StrangerBodyStream,
};

static TmdBone _gActor401000StrangerBurstHandSkeleton[3] = {
#include "assets/stranger_burst_hand_skeleton.inc"
};

static u32 _gActor401000StrangerBurstHandPartVerts[3] = {
#include "assets/stranger_burst_hand_partVerts.inc"
};

static SVECTOR _gActor401000StrangerBurstHandVerts[33] = {
#include "assets/stranger_burst_hand_verts.inc"
};

static SVECTOR _gActor401000StrangerBurstHandNormals[45] = {
#include "assets/stranger_burst_hand_normals.inc"
};

static u32 _gActor401000StrangerBurstHandStream[357] = {
#include "assets/stranger_burst_hand_stream.inc"
};

TmdSource gOddStrangerBurstModelA = {
    0,
    2008,
    304,
    3,
    _gActor401000StrangerBurstHandPartVerts,
    _gActor401000StrangerBurstHandVerts,
    _gActor401000StrangerBurstHandNormals,
    _gActor401000StrangerBurstHandSkeleton,
    _gActor401000StrangerBurstHandStream,
};

static TmdBone _gActor401000Model123ECSkeleton[3] = {
#include "assets/actor_401000_model_123EC_skeleton.inc"
};

static u32 _gActor401000Model123ECPartVerts[3] = {
#include "assets/actor_401000_model_123EC_partVerts.inc"
};

static SVECTOR _gActor401000Model123ECVerts[37] = {
#include "assets/actor_401000_model_123EC_verts.inc"
};

static SVECTOR _gActor401000Model123ECNormals[50] = {
#include "assets/actor_401000_model_123EC_normals.inc"
};

static u32 _gActor401000Model123ECStream[394] = {
#include "assets/actor_401000_model_123EC_stream.inc"
};

static TmdSource _gActor401000Model123EC = {
    0,
    2228,
    368,
    3,
    _gActor401000Model123ECPartVerts,
    _gActor401000Model123ECVerts,
    _gActor401000Model123ECNormals,
    _gActor401000Model123ECSkeleton,
    _gActor401000Model123ECStream,
};

static TmdBone _gActor401000Model12F70Skeleton[3] = {
#include "assets/actor_401000_model_12F70_skeleton.inc"
};

static u32 _gActor401000Model12F70PartVerts[3] = {
#include "assets/actor_401000_model_12F70_partVerts.inc"
};

static SVECTOR _gActor401000Model12F70Verts[76] = {
#include "assets/actor_401000_model_12F70_verts.inc"
};

static SVECTOR _gActor401000Model12F70Normals[76] = {
#include "assets/actor_401000_model_12F70_normals.inc"
};

static u32 _gActor401000Model12F70Stream[772] = {
#include "assets/actor_401000_model_12F70_stream.inc"
};

TmdSource gOddStrangerBurstModelC = {
    0,
    4600,
    688,
    3,
    _gActor401000Model12F70PartVerts,
    _gActor401000Model12F70Verts,
    _gActor401000Model12F70Normals,
    _gActor401000Model12F70Skeleton,
    _gActor401000Model12F70Stream,
};

static TmdBone _gActor401000Model13E40Skeleton[1] = {
#include "assets/actor_401000_model_13E40_skeleton.inc"
};

static u32 _gActor401000Model13E40PartVerts[1] = {
#include "assets/actor_401000_model_13E40_partVerts.inc"
};

static SVECTOR _gActor401000Model13E40Verts[37] = {
#include "assets/actor_401000_model_13E40_verts.inc"
};

static SVECTOR _gActor401000Model13E40Normals[42] = {
#include "assets/actor_401000_model_13E40_normals.inc"
};

static u32 _gActor401000Model13E40Stream[332] = {
#include "assets/actor_401000_model_13E40_stream.inc"
};

TmdSource gOddStrangerBurstModelB = {
    0,
    2192,
    0,
    1,
    _gActor401000Model13E40PartVerts,
    _gActor401000Model13E40Verts,
    _gActor401000Model13E40Normals,
    _gActor401000Model13E40Skeleton,
    _gActor401000Model13E40Stream,
};

static AnimationPackedPose _gActor401000Animation14E90Bank1[28] = {
#include "assets/actor_401000_animation_14E90_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation14E90Bank4[247] = {
#include "assets/actor_401000_animation_14E90_bank4.inc"
};

static AnimationRecord _gActor401000Animation14E90Records[362] = {
#include "assets/actor_401000_animation_14E90_records.inc"
};

static u16 _gActor401000Animation14E90Indices[20] = {
#include "assets/actor_401000_animation_14E90_indices.inc"
};

static AnimationSet _gActor401000Animation14E90 = {
    _gActor401000Animation14E90Records,
    _gActor401000Animation14E90Indices,
    { NULL, _gActor401000Animation14E90Bank1, NULL, NULL, _gActor401000Animation14E90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation15914Bank1[32] = {
#include "assets/actor_401000_animation_15914_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation15914Bank4[223] = {
#include "assets/actor_401000_animation_15914_bank4.inc"
};

static AnimationRecord _gActor401000Animation15914Records[334] = {
#include "assets/actor_401000_animation_15914_records.inc"
};

static u16 _gActor401000Animation15914Indices[20] = {
#include "assets/actor_401000_animation_15914_indices.inc"
};

static AnimationSet _gActor401000Animation15914 = {
    _gActor401000Animation15914Records,
    _gActor401000Animation15914Indices,
    { NULL, _gActor401000Animation15914Bank1, NULL, NULL, _gActor401000Animation15914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation16384Bank1[24] = {
#include "assets/actor_401000_animation_16384_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation16384Bank4[248] = {
#include "assets/actor_401000_animation_16384_bank4.inc"
};

static AnimationRecord _gActor401000Animation16384Records[328] = {
#include "assets/actor_401000_animation_16384_records.inc"
};

static u16 _gActor401000Animation16384Indices[20] = {
#include "assets/actor_401000_animation_16384_indices.inc"
};

static AnimationSet _gActor401000Animation16384 = {
    _gActor401000Animation16384Records,
    _gActor401000Animation16384Indices,
    { NULL, _gActor401000Animation16384Bank1, NULL, NULL, _gActor401000Animation16384Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation16964Bank1[10] = {
#include "assets/actor_401000_animation_16964_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation16964Bank4[138] = {
#include "assets/actor_401000_animation_16964_bank4.inc"
};

static AnimationRecord _gActor401000Animation16964Records[188] = {
#include "assets/actor_401000_animation_16964_records.inc"
};

static u16 _gActor401000Animation16964Indices[20] = {
#include "assets/actor_401000_animation_16964_indices.inc"
};

static AnimationSet _gActor401000Animation16964 = {
    _gActor401000Animation16964Records,
    _gActor401000Animation16964Indices,
    { NULL, _gActor401000Animation16964Bank1, NULL, NULL, _gActor401000Animation16964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation16F48Bank1[8] = {
#include "assets/actor_401000_animation_16F48_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation16F48Bank4[151] = {
#include "assets/actor_401000_animation_16F48_bank4.inc"
};

static AnimationRecord _gActor401000Animation16F48Records[182] = {
#include "assets/actor_401000_animation_16F48_records.inc"
};

static u16 _gActor401000Animation16F48Indices[20] = {
#include "assets/actor_401000_animation_16F48_indices.inc"
};

static AnimationSet _gActor401000Animation16F48 = {
    _gActor401000Animation16F48Records,
    _gActor401000Animation16F48Indices,
    { NULL, _gActor401000Animation16F48Bank1, NULL, NULL, _gActor401000Animation16F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1799CBank1[20] = {
#include "assets/actor_401000_animation_1799C_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1799CBank4[228] = {
#include "assets/actor_401000_animation_1799C_bank4.inc"
};

static AnimationRecord _gActor401000Animation1799CRecords[353] = {
#include "assets/actor_401000_animation_1799C_records.inc"
};

static u16 _gActor401000Animation1799CIndices[20] = {
#include "assets/actor_401000_animation_1799C_indices.inc"
};

static AnimationSet _gActor401000Animation1799C = {
    _gActor401000Animation1799CRecords,
    _gActor401000Animation1799CIndices,
    { NULL, _gActor401000Animation1799CBank1, NULL, NULL, _gActor401000Animation1799CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation186E0Bank1[23] = {
#include "assets/actor_401000_animation_186E0_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation186E0Bank4[334] = {
#include "assets/actor_401000_animation_186E0_bank4.inc"
};

static AnimationRecord _gActor401000Animation186E0Records[426] = {
#include "assets/actor_401000_animation_186E0_records.inc"
};

static u16 _gActor401000Animation186E0Indices[20] = {
#include "assets/actor_401000_animation_186E0_indices.inc"
};

static AnimationSet _gActor401000Animation186E0 = {
    _gActor401000Animation186E0Records,
    _gActor401000Animation186E0Indices,
    { NULL, _gActor401000Animation186E0Bank1, NULL, NULL, _gActor401000Animation186E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation18F50Bank1[14] = {
#include "assets/actor_401000_animation_18F50_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation18F50Bank4[216] = {
#include "assets/actor_401000_animation_18F50_bank4.inc"
};

static AnimationRecord _gActor401000Animation18F50Records[262] = {
#include "assets/actor_401000_animation_18F50_records.inc"
};

static u16 _gActor401000Animation18F50Indices[20] = {
#include "assets/actor_401000_animation_18F50_indices.inc"
};

static AnimationSet _gActor401000Animation18F50 = {
    _gActor401000Animation18F50Records,
    _gActor401000Animation18F50Indices,
    { NULL, _gActor401000Animation18F50Bank1, NULL, NULL, _gActor401000Animation18F50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation192E0Bank1[5] = {
#include "assets/actor_401000_animation_192E0_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation192E0Bank4[82] = {
#include "assets/actor_401000_animation_192E0_bank4.inc"
};

static AnimationRecord _gActor401000Animation192E0Records[111] = {
#include "assets/actor_401000_animation_192E0_records.inc"
};

static u16 _gActor401000Animation192E0Indices[20] = {
#include "assets/actor_401000_animation_192E0_indices.inc"
};

static AnimationSet _gActor401000Animation192E0 = {
    _gActor401000Animation192E0Records,
    _gActor401000Animation192E0Indices,
    { NULL, _gActor401000Animation192E0Bank1, NULL, NULL, _gActor401000Animation192E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation19B18Bank1[16] = {
#include "assets/actor_401000_animation_19B18_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation19B18Bank4[199] = {
#include "assets/actor_401000_animation_19B18_bank4.inc"
};

static AnimationRecord _gActor401000Animation19B18Records[259] = {
#include "assets/actor_401000_animation_19B18_records.inc"
};

static u16 _gActor401000Animation19B18Indices[20] = {
#include "assets/actor_401000_animation_19B18_indices.inc"
};

static AnimationSet _gActor401000Animation19B18 = {
    _gActor401000Animation19B18Records,
    _gActor401000Animation19B18Indices,
    { NULL, _gActor401000Animation19B18Bank1, NULL, NULL, _gActor401000Animation19B18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1A068Bank1[10] = {
#include "assets/actor_401000_animation_1A068_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1A068Bank4[113] = {
#include "assets/actor_401000_animation_1A068_bank4.inc"
};

static AnimationRecord _gActor401000Animation1A068Records[177] = {
#include "assets/actor_401000_animation_1A068_records.inc"
};

static u16 _gActor401000Animation1A068Indices[20] = {
#include "assets/actor_401000_animation_1A068_indices.inc"
};

static AnimationSet _gActor401000Animation1A068 = {
    _gActor401000Animation1A068Records,
    _gActor401000Animation1A068Indices,
    { NULL, _gActor401000Animation1A068Bank1, NULL, NULL, _gActor401000Animation1A068Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1AF80Bank1[26] = {
#include "assets/actor_401000_animation_1AF80_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1AF80Bank4[330] = {
#include "assets/actor_401000_animation_1AF80_bank4.inc"
};

static AnimationRecord _gActor401000Animation1AF80Records[538] = {
#include "assets/actor_401000_animation_1AF80_records.inc"
};

static u16 _gActor401000Animation1AF80Indices[20] = {
#include "assets/actor_401000_animation_1AF80_indices.inc"
};

static AnimationSet _gActor401000Animation1AF80 = {
    _gActor401000Animation1AF80Records,
    _gActor401000Animation1AF80Indices,
    { NULL, _gActor401000Animation1AF80Bank1, NULL, NULL, _gActor401000Animation1AF80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1B324Bank1[10] = {
#include "assets/actor_401000_animation_1B324_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1B324Bank4[73] = {
#include "assets/actor_401000_animation_1B324_bank4.inc"
};

static AnimationRecord _gActor401000Animation1B324Records[110] = {
#include "assets/actor_401000_animation_1B324_records.inc"
};

static u16 _gActor401000Animation1B324Indices[20] = {
#include "assets/actor_401000_animation_1B324_indices.inc"
};

static AnimationSet _gActor401000Animation1B324 = {
    _gActor401000Animation1B324Records,
    _gActor401000Animation1B324Indices,
    { NULL, _gActor401000Animation1B324Bank1, NULL, NULL, _gActor401000Animation1B324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1B6D8Bank1[6] = {
#include "assets/actor_401000_animation_1B6D8_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1B6D8Bank4[81] = {
#include "assets/actor_401000_animation_1B6D8_bank4.inc"
};

static AnimationRecord _gActor401000Animation1B6D8Records[118] = {
#include "assets/actor_401000_animation_1B6D8_records.inc"
};

static u16 _gActor401000Animation1B6D8Indices[20] = {
#include "assets/actor_401000_animation_1B6D8_indices.inc"
};

static AnimationSet _gActor401000Animation1B6D8 = {
    _gActor401000Animation1B6D8Records,
    _gActor401000Animation1B6D8Indices,
    { NULL, _gActor401000Animation1B6D8Bank1, NULL, NULL, _gActor401000Animation1B6D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1BA5CBank1[13] = {
#include "assets/actor_401000_animation_1BA5C_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1BA5CBank4[58] = {
#include "assets/actor_401000_animation_1BA5C_bank4.inc"
};

static AnimationRecord _gActor401000Animation1BA5CRecords[108] = {
#include "assets/actor_401000_animation_1BA5C_records.inc"
};

static u16 _gActor401000Animation1BA5CIndices[20] = {
#include "assets/actor_401000_animation_1BA5C_indices.inc"
};

static AnimationSet _gActor401000Animation1BA5C = {
    _gActor401000Animation1BA5CRecords,
    _gActor401000Animation1BA5CIndices,
    { NULL, _gActor401000Animation1BA5CBank1, NULL, NULL, _gActor401000Animation1BA5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1C150Bank1[15] = {
#include "assets/actor_401000_animation_1C150_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1C150Bank4[159] = {
#include "assets/actor_401000_animation_1C150_bank4.inc"
};

static AnimationRecord _gActor401000Animation1C150Records[221] = {
#include "assets/actor_401000_animation_1C150_records.inc"
};

static u16 _gActor401000Animation1C150Indices[20] = {
#include "assets/actor_401000_animation_1C150_indices.inc"
};

static AnimationSet _gActor401000Animation1C150 = {
    _gActor401000Animation1C150Records,
    _gActor401000Animation1C150Indices,
    { NULL, _gActor401000Animation1C150Bank1, NULL, NULL, _gActor401000Animation1C150Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1C9B4Bank1[20] = {
#include "assets/actor_401000_animation_1C9B4_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1C9B4Bank4[190] = {
#include "assets/actor_401000_animation_1C9B4_bank4.inc"
};

static AnimationRecord _gActor401000Animation1C9B4Records[267] = {
#include "assets/actor_401000_animation_1C9B4_records.inc"
};

static u16 _gActor401000Animation1C9B4Indices[20] = {
#include "assets/actor_401000_animation_1C9B4_indices.inc"
};

static AnimationSet _gActor401000Animation1C9B4 = {
    _gActor401000Animation1C9B4Records,
    _gActor401000Animation1C9B4Indices,
    { NULL, _gActor401000Animation1C9B4Bank1, NULL, NULL, _gActor401000Animation1C9B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1D22CBank1[20] = {
#include "assets/actor_401000_animation_1D22C_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1D22CBank4[196] = {
#include "assets/actor_401000_animation_1D22C_bank4.inc"
};

static AnimationRecord _gActor401000Animation1D22CRecords[266] = {
#include "assets/actor_401000_animation_1D22C_records.inc"
};

static u16 _gActor401000Animation1D22CIndices[20] = {
#include "assets/actor_401000_animation_1D22C_indices.inc"
};

static AnimationSet _gActor401000Animation1D22C = {
    _gActor401000Animation1D22CRecords,
    _gActor401000Animation1D22CIndices,
    { NULL, _gActor401000Animation1D22CBank1, NULL, NULL, _gActor401000Animation1D22CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1D87CBank1[11] = {
#include "assets/actor_401000_animation_1D87C_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1D87CBank4[150] = {
#include "assets/actor_401000_animation_1D87C_bank4.inc"
};

static AnimationRecord _gActor401000Animation1D87CRecords[201] = {
#include "assets/actor_401000_animation_1D87C_records.inc"
};

static u16 _gActor401000Animation1D87CIndices[20] = {
#include "assets/actor_401000_animation_1D87C_indices.inc"
};

static AnimationSet _gActor401000Animation1D87C = {
    _gActor401000Animation1D87CRecords,
    _gActor401000Animation1D87CIndices,
    { NULL, _gActor401000Animation1D87CBank1, NULL, NULL, _gActor401000Animation1D87CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1E250Bank1[18] = {
#include "assets/actor_401000_animation_1E250_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1E250Bank4[232] = {
#include "assets/actor_401000_animation_1E250_bank4.inc"
};

static AnimationRecord _gActor401000Animation1E250Records[323] = {
#include "assets/actor_401000_animation_1E250_records.inc"
};

static u16 _gActor401000Animation1E250Indices[20] = {
#include "assets/actor_401000_animation_1E250_indices.inc"
};

static AnimationSet _gActor401000Animation1E250 = {
    _gActor401000Animation1E250Records,
    _gActor401000Animation1E250Indices,
    { NULL, _gActor401000Animation1E250Bank1, NULL, NULL, _gActor401000Animation1E250Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1F0C8Bank1[33] = {
#include "assets/actor_401000_animation_1F0C8_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1F0C8Bank4[326] = {
#include "assets/actor_401000_animation_1F0C8_bank4.inc"
};

static AnimationRecord _gActor401000Animation1F0C8Records[481] = {
#include "assets/actor_401000_animation_1F0C8_records.inc"
};

static u16 _gActor401000Animation1F0C8Indices[20] = {
#include "assets/actor_401000_animation_1F0C8_indices.inc"
};

static AnimationSet _gActor401000Animation1F0C8 = {
    _gActor401000Animation1F0C8Records,
    _gActor401000Animation1F0C8Indices,
    { NULL, _gActor401000Animation1F0C8Bank1, NULL, NULL, _gActor401000Animation1F0C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation1FFD8Bank1[26] = {
#include "assets/actor_401000_animation_1FFD8_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation1FFD8Bank4[351] = {
#include "assets/actor_401000_animation_1FFD8_bank4.inc"
};

static AnimationRecord _gActor401000Animation1FFD8Records[515] = {
#include "assets/actor_401000_animation_1FFD8_records.inc"
};

static u16 _gActor401000Animation1FFD8Indices[20] = {
#include "assets/actor_401000_animation_1FFD8_indices.inc"
};

static AnimationSet _gActor401000Animation1FFD8 = {
    _gActor401000Animation1FFD8Records,
    _gActor401000Animation1FFD8Indices,
    { NULL, _gActor401000Animation1FFD8Bank1, NULL, NULL, _gActor401000Animation1FFD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation20660Bank1[15] = {
#include "assets/actor_401000_animation_20660_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation20660Bank4[154] = {
#include "assets/actor_401000_animation_20660_bank4.inc"
};

static AnimationRecord _gActor401000Animation20660Records[199] = {
#include "assets/actor_401000_animation_20660_records.inc"
};

static u16 _gActor401000Animation20660Indices[20] = {
#include "assets/actor_401000_animation_20660_indices.inc"
};

static AnimationSet _gActor401000Animation20660 = {
    _gActor401000Animation20660Records,
    _gActor401000Animation20660Indices,
    { NULL, _gActor401000Animation20660Bank1, NULL, NULL, _gActor401000Animation20660Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation21058Bank1[19] = {
#include "assets/actor_401000_animation_21058_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation21058Bank4[250] = {
#include "assets/actor_401000_animation_21058_bank4.inc"
};

static AnimationRecord _gActor401000Animation21058Records[311] = {
#include "assets/actor_401000_animation_21058_records.inc"
};

static u16 _gActor401000Animation21058Indices[20] = {
#include "assets/actor_401000_animation_21058_indices.inc"
};

static AnimationSet _gActor401000Animation21058 = {
    _gActor401000Animation21058Records,
    _gActor401000Animation21058Indices,
    { NULL, _gActor401000Animation21058Bank1, NULL, NULL, _gActor401000Animation21058Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation219B8Bank1[21] = {
#include "assets/actor_401000_animation_219B8_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation219B8Bank4[233] = {
#include "assets/actor_401000_animation_219B8_bank4.inc"
};

static AnimationRecord _gActor401000Animation219B8Records[284] = {
#include "assets/actor_401000_animation_219B8_records.inc"
};

static u16 _gActor401000Animation219B8Indices[20] = {
#include "assets/actor_401000_animation_219B8_indices.inc"
};

static AnimationSet _gActor401000Animation219B8 = {
    _gActor401000Animation219B8Records,
    _gActor401000Animation219B8Indices,
    { NULL, _gActor401000Animation219B8Bank1, NULL, NULL, _gActor401000Animation219B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401000Animation22814Bank1[18] = {
#include "assets/actor_401000_animation_22814_bank1.inc"
};

static AnimationPackedRotation _gActor401000Animation22814Bank4[361] = {
#include "assets/actor_401000_animation_22814_bank4.inc"
};

static AnimationRecord _gActor401000Animation22814Records[484] = {
#include "assets/actor_401000_animation_22814_records.inc"
};

static u16 _gActor401000Animation22814Indices[20] = {
#include "assets/actor_401000_animation_22814_indices.inc"
};

AnimationSet gOddStrangerDormantAnimSet = {
    _gActor401000Animation22814Records,
    _gActor401000Animation22814Indices,
    { NULL, _gActor401000Animation22814Bank1, NULL, NULL, _gActor401000Animation22814Bank4, NULL, NULL, NULL },
};

s8 gOddStrangerTransitions[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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
    &_gActor401000Animation14E90,
    &_gActor401000Animation15914,
    &_gActor401000Animation1FFD8,
    &_gActor401000Animation1D87C,
    &_gActor401000Animation1E250,
    &_gActor401000Animation1F0C8,
    &_gActor401000Animation186E0,
    &_gActor401000Animation19B18,
    &_gActor401000Animation1B324,
    &_gActor401000Animation1B6D8,
    &_gActor401000Animation16F48,
    &_gActor401000Animation16964,
    &_gActor401000Animation1AF80,
    &_gActor401000Animation1799C,
    NULL,
    &_gActor401000Animation1A068,
    &_gActor401000Animation16384,
    &_gActor401000Animation1C150,
    &_gActor401000Animation1C9B4,
    &_gActor401000Animation1D22C,
    &_gActor401000Animation18F50,
    &_gActor401000Animation1B6D8,
    &_gActor401000Animation16F48,
    &_gActor401000Animation192E0,
    &_gActor401000Animation1BA5C,
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

AnimationSet* D_actor_401000_80154F00[7] = {
    NULL,
    NULL,
    NULL,
    &_gActor401000Animation20660,
    &_gActor401000Animation21058,
    &_gActor401000Animation219B8,
    NULL,
};

AnimationPlayRequest gOddStrangerPlayerAnim = { { .sets = D_actor_401000_80154F00 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

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

enum { ACTOR_401000_MESSAGE_IGNORE = 2015 };

static s32 _actor401000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra);

TaskMessageEntry D_actor_401000_80154F90[8] = {
    { ACTOR_401000_MESSAGE_IGNORE, _actor401000IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _oddStrangerPlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, _oddStrangerApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorHeightClamp D_actor_401000_80154FD0[3] = {
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

u16 gOddStrangerChaseDistance = 0;

TaskDesc D_actor_401000_80155004 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_401000_8013E038, { .model = &_gActor401000StrangerBody } };

SVECTOR ActorContact_ScratchPosition;

OddStrangerTransformStorage gOddStrangerGrabTransform;

GameActorButtonPressHold D_actor_401000_80155038;

/// Active room rows and correction limits, in whole parent-coordinate units.
enum {
    ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS = 2,
    ACTOR_401000_GRID_PUSH_MAX_Y          = 300,
    ACTOR_401000_GRID_PUSH_MAX_XZ         = 150,
    ACTOR_401000_GRID_ROOT_Y_OFFSET       = 75
};

static __inline__ void _actor401000BindLightingMatrices(Task* actor);
static __inline__ void _actor401000InitRootPose(GfxCoord* rootCoord, OddStrangerWork* work);
static void            func_actor_401000_80133274(Enemy* enemy, Task* actor);
static void            _actor401000ClampRootHeight(const GameLocationKey* location, GfxCoord* root);
static __inline__ s32  _actor401000HasRoomHeightClamp(const GameLocationKey* location);
static s32             _actor401000ApplyGridPushback(GfxCoord* root, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset);
static void            _actor401000Chase(Task* task);
static void            _oddStrangerGrabReach(Task* task);
static void            _actor401000FallBack(Task* task);
static void            _actor401000FallFront(Task* task);
static void            _actor401000Dormant(Task* task);
static void            _actor401000Ambush(Task* task);
static void            func_actor_401000_8013B1E4(Task* arg0);
static void            _actor401000RefallBack(Task* task);
static void            _actor401000RefallFront(Task* task);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Binds the model's lighting matrices to storage in its task's work block.
///
/// Requires initialized `OddStrangerWork` and a live TMD body. The model borrows
/// both matrices until teardown; the work block must outlive those references.
static __inline__ void _actor401000BindLightingMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       model;

    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Initializes root yaw/scale, the position-history cursor and collision contacts.
///
/// Requires live root and initialized work/contact tables. Replaces the root's
/// rotation at ODD_STRANGER_ROOT_SCALE (signed Q12) and marks it dirty, retaining
/// translation. Resets the history cursor without clearing its samples; clears
/// occupied grid/hit contacts while retaining their end markers and body links.
static __inline__ void _actor401000InitRootPose(GfxCoord* rootCoord, OddStrangerWork* work)
{
    _actorRenderRescaleYaw(rootCoord, ODD_STRANGER_ROOT_SCALE);
    work->bodyPosCursor = 0;
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
}

/// Enemy init: allocates the 0xC80-byte work block, binds the model's light
/// and colour matrices to its copies, seeds both animation contexts and the
/// three `WorldCollisionBody` nodes, then picks the opening clip from the low bits of
/// `Enemy::placeKey` and the `downFramesBase` parameter run from the spawn flags.
/// The tail rebuilds the root coordinate through `_actor401000InitRootPose`.
static void func_actor_401000_80133274(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    OddStrangerWork*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 variant;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(OddStrangerWork), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    (sceneAcquireBattleRef)(0);
    actor->exitCallback = _oddStrangerExit;
    _actor401000BindLightingMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401000_8013E09C.hpMax;
    enemy->param                  = &D_actor_401000_8013E09C;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, gOddStrangerAnimSets, obj,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, gOddStrangerAnimSets,
                         obj, work->blend.poses,
                         work->blend.slots);
    work->animRequest   = ODD_STRANGER_ANIM_REQUEST_RESET;
    work->animId        = 2;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = 0x10;
    work->animRate      = 0x10;
    switch (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 5) {
        case 0:
            work->chaseRate = 0x11;
            break;
        case 1:
            work->chaseRate = 0xF;
            break;
        case 2:
            work->chaseRate = 0x10;
            break;
        case 3:
            work->chaseRate = 0x12;
            break;
        case 4:
        default:
            work->chaseRate = 0xE;
            break;
    }
    _oddStrangerDriveAnimation(actor);

    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = root;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0xAC;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30000;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    body                   = &work->hitBody;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->hitContacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x3000A;
    body->radius           = 0x1AE;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.key = 0x30000;

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
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, head);
    worldCollisionInitContacts(head->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

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

    actor->msgTable    = D_actor_401000_80154F90;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    variant                    = (actor->spawnArg1.value >> 16);
    switch (variant & 0xF) {
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
            tmdAllocPrimitiveBuffer(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->downFramesBase = D_actor_401000_8013E0AC[0].downFramesBase;
            work->sidestepAngle  = D_actor_401000_8013E0AC[0].sidestepAngle;
            work->sidestepDelay  = D_actor_401000_8013E0AC[0].sidestepDelay;
            work->noticeRadius   = D_actor_401000_8013E0AC[0].noticeRadius;
            break;
        case 1:
            work->downFramesBase = D_actor_401000_8013E0AC[2].downFramesBase;
            work->sidestepAngle  = D_actor_401000_8013E0AC[2].sidestepAngle;
            work->sidestepDelay  = D_actor_401000_8013E0AC[2].sidestepDelay;
            work->noticeRadius   = D_actor_401000_8013E0AC[2].noticeRadius;
            break;
        case 0:
        default:
            work->downFramesBase = D_actor_401000_8013E0AC[1].downFramesBase;
            work->sidestepAngle  = D_actor_401000_8013E0AC[1].sidestepAngle;
            work->sidestepDelay  = D_actor_401000_8013E0AC[1].sidestepDelay;
            work->noticeRadius   = D_actor_401000_8013E0AC[1].noticeRadius;
            break;
    }

    _actor401000InitRootPose(actor->extra.tmd->coords, work);

    actor->state++;
}

#include "../../shared/odd_stranger_spawn_hit_effect.inc.c"

#include "../../shared/odd_stranger_take_hit.inc.c"

#include "../../shared/odd_stranger_stunned.inc.c"

#include "../../shared/odd_stranger_face_player.inc.c"

/// Clamps the root's Y translation to this actor's bounds for the stage and area.
///
/// View and room variant do not select a row. A matching row invalidates the
/// composed transform even when Y was already in bounds; other rooms leave it
/// untouched. The final zero row of the table is outside the active domain.
static void _actor401000ClampRootHeight(const GameLocationKey* location, GfxCoord* root)
{
    const ActorHeightClamp* row;
    s32                     rootY;
    s32                     minY;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS; rowIndex++) {
        row = &D_actor_401000_80154FD0[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            minY  = row->minY;
            rootY = root->coord.t[1];
            if (rootY < minY) {
                root->coord.t[1] = minY;
            } else if (row->maxY < rootY) {
                root->coord.t[1] = row->maxY;
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

/// Returns 1 when this actor has height bounds for the supplied stage and area.
///
/// View and room-variant bytes are ignored. Only the two active table rows
/// participate; the trailing zero row is not a bound for stage zero.
static __inline__ s32 _actor401000HasRoomHeightClamp(const GameLocationKey* location)
{
    const ActorHeightClamp* row;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS; rowIndex++) {
        row = &D_actor_401000_80154FD0[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            return 1;
        }
    }
    return 0;
}

/// Limits a resolved grid correction's Y step to +/-300 parent-coordinate units.
///
/// The room-bound probe is retained before the unconditional cap. Both paths
/// leave X/Z and the original signed 16.16 correction unchanged.
static inline void _actor401000CapGridPushHeight(ActorContactCappedPushScratch* push)
{
    s16 stepY;
    s16 cappedY;

    if (_actor401000HasRoomHeightClamp(&gGameSession->location.loc)) {
        stepY = push->step.vy;
        if (((stepY >= 0) ? stepY : -stepY) <= ACTOR_401000_GRID_PUSH_MAX_Y) {
            return;
        }
        cappedY       = (stepY <= 0) ? -ACTOR_401000_GRID_PUSH_MAX_Y : ACTOR_401000_GRID_PUSH_MAX_Y;
        push->step.vy = cappedY;
    }
    stepY = push->step.vy;
    if (((stepY >= 0) ? stepY : -stepY) <= ACTOR_401000_GRID_PUSH_MAX_Y) {
        return;
    }
    push->step.vy = (stepY <= 0) ? -ACTOR_401000_GRID_PUSH_MAX_Y : ACTOR_401000_GRID_PUSH_MAX_Y;
}

/// Applies a capped room-grid correction and reports nonzero resolved X/Z.
///
/// Requires `contactCount` readable contacts (1..32767), a live root and an
/// initialized scratch stack. The resolver supplies signed 16.16 corrections:
/// Y is capped at 300 and X/Z length at 150 whole parent-coordinate units.
/// Fractional X/Z adds one further unit in its sign, after the cap. In a bounded
/// room, root Y is then clamped to the room bounds and raised by `heightOffset`.
/// This last adjustment runs even without a grid hit. Frozen actors return 0.
/// The caller invalidates root composition outside the room-clamp path.
static s32 _actor401000ApplyGridPushback(GfxCoord* root, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset)
{
    enum {
        WORLD_COLLISION_DELTA_FRACTION_BITS = 16,
        WORLD_COLLISION_DELTA_FRACTION_MASK = 0xFFFF
    };
    ActorContactCappedPushScratch* savedCursor;
    ActorContactCappedPushScratch* push;
    SVECTOR*                       step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    savedCursor                                         = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = savedCursor - 1;
    push                                                = savedCursor - 1;
    push->moved                                         = 0;
    // Apply whole-unit Y first, then capped X/Z and their fractional remainder.
    if (worldCollisionResolvePushback(contacts, &push->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        push->step.vx = savedCursor[-1].delta.fixed.vx.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        push->step.vy = push->delta.fixed.vy.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        push->step.vz = push->delta.fixed.vz.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        _actor401000CapGridPushHeight(push);
        root->coord.t[1] += push->step.vy;
        push->stepLength  = push->step.vx * push->step.vx + push->step.vz * push->step.vz;
        push->stepLength  = SquareRoot0(push->stepLength);
        step              = &push->step;
        if (push->stepLength >= ACTOR_401000_GRID_PUSH_MAX_XZ) {
            push->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(ACTOR_401000_GRID_PUSH_MAX_XZ);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            root->coord.t[0] += push->step.vx;
            root->coord.t[2] += push->step.vz;
        } else {
            root->coord.t[0] += push->step.vx;
            root->coord.t[2] += push->step.vz;
        }
        if (push->delta.fixed.vx.word & WORLD_COLLISION_DELTA_FRACTION_MASK) {
            if (push->delta.fixed.vx.word > 0) {
                root->coord.t[0]++;
            } else {
                root->coord.t[0]--;
            }
        }
        if (push->delta.fixed.vz.word & WORLD_COLLISION_DELTA_FRACTION_MASK) {
            if (push->delta.fixed.vz.word > 0) {
                root->coord.t[2]++;
            } else {
                root->coord.t[2]--;
            }
        }
    }
    // Room bounds precede the root offset even when no grid contact was found.
    if (_actor401000HasRoomHeightClamp(&gGameSession->location.loc)) {
        _actor401000ClampRootHeight(&gGameSession->location.loc, root);
        root->coord.t[1] += heightOffset;
    }
    if (push->delta.fixed.vx.word != 0 || push->delta.fixed.vz.word != 0) {
        push->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return push->moved;
}

#include "../../shared/odd_stranger_push_body.inc.c"

/// Reads the player's facing and the bearing from the player to this actor.
///
/// Requires live actor and player model roots in the same parent frame and
/// writable caller-owned chase scratch. Refreshes the signed-halfword XYZ
/// offset to the player, the player's matrix yaw and the reverse XZ bearing.
/// Angles use 4096 units per turn; the reverse bearing retains both +/-2048
/// endpoints. Reads local transforms without composing them; retains no pointer.
static __inline__ void _actor401000ReadPlayerBearings(const Task* task, ActorChaseScratch* chase)
{
    chase->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                              gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
}

/// Pursues the player, choosing a sidestep, grab or renewed alert.
///
/// A stalking spawn selects `STALK` immediately. Entry starts the run clip and
/// enables drawing, lock-on and room-grid response. Later ticks steer by up to
/// 64 angle units (4096 per turn), move with the animation rate and resolve
/// grid/body contacts. Clear sight allows a grab within 1100 units; the turn
/// test retains its one-sided comparison. After 76 pursuit ticks it alerts.
/// Requires live actor/player roots, initialized work and scratch storage.
static void _actor401000Chase(Task* task)
{
    enum {
        ACTOR_401000_CHASE_SPAWN_MODE_MASK     = 0xF0,
        ACTOR_401000_CHASE_STALK_SPAWN         = 0x10,
        ACTOR_401000_CHASE_PLAYER_FACING_LIMIT = 68,
        ACTOR_401000_CHASE_SIDESTEP_TURN_LIMIT = 128,
        ACTOR_401000_CHASE_SIDESTEP_RANGE      = 1800,
        ACTOR_401000_CHASE_GRAB_RANGE          = 1100,
        ACTOR_401000_CHASE_GRAB_TURN_LIMIT     = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_401000_CHASE_BLOCKED_TURN_BIAS   = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_401000_CHASE_SIDE_SWITCH_TICKS   = 361,
        ACTOR_401000_CHASE_TURN_STEP           = 64,
        ACTOR_401000_CHASE_CLEARANCE_RADIUS    = 300,
        ACTOR_401000_CHASE_ALERT_TICKS         = 76
    };
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* chase;
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          root;
    s32                turnMagnitude;
    s32                playerFacingError;
    s32                spawnMode;

    spawnMode = (task->spawnArg1.value >> 16);
    work      = task->work;
    if ((spawnMode & ACTOR_401000_CHASE_SPAWN_MODE_MASK) == ACTOR_401000_CHASE_STALK_SPAWN) {
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
    work->exitCounter++;
    savedCursor = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase       = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &savedCursor[-1].delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    // Compare player facing and actor bearing before choosing a sidestep or grab.
    _actor401000ReadPlayerBearings(task, chase);
    root                = task->extra.tmd->coords;
    chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
    work->lookYawTarget = chase->turn;
    playerFacingError   = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(playerFacingError) < ACTOR_401000_CHASE_PLAYER_FACING_LIMIT) {
        if (work->sidestepDelay + work->sidestepCount / 2 < work->stateTimer) {
            turnMagnitude = chase->turn;
            if (turnMagnitude < 0) {
                turnMagnitude = -turnMagnitude;
            }
            if (turnMagnitude < ACTOR_401000_CHASE_SIDESTEP_TURN_LIMIT) {
                if (_oddStrangerOutOfRange(&chase->delta, ACTOR_401000_CHASE_SIDESTEP_RANGE) && _playerDetectionSightBlocked(task) != 1) {
                    work->state = ODD_STRANGER_STATE_SIDESTEP;
                }
            }
        }
    }
    if (_playerDetectionSightBlocked(task) != 1) {
        work->stateTimer++;
        root                = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < ACTOR_401000_CHASE_GRAB_TURN_LIMIT) {
            if (!_oddStrangerOutOfRange(&chase->delta, ACTOR_401000_CHASE_GRAB_RANGE) && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            }
        }
    } else {
        // The blocked-sight path resets this timer before its retained side-switch test.
        work->stateTimer    = 0;
        root                = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 1) {
            chase->turn += ACTOR_401000_CHASE_BLOCKED_TURN_BIAS;
        } else {
            chase->turn -= ACTOR_401000_CHASE_BLOCKED_TURN_BIAS;
        }
        if (work->stateTimer >= ACTOR_401000_CHASE_SIDE_SWITCH_TICKS) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    if (chase->turn > ACTOR_401000_CHASE_TURN_STEP) {
        chase->turn = ACTOR_401000_CHASE_TURN_STEP;
    }
    if (chase->turn < -ACTOR_401000_CHASE_TURN_STEP) {
        chase->turn = -ACTOR_401000_CHASE_TURN_STEP;
    }
    chase->turn += ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ODD_STRANGER_ANIM_RUN) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_CHASE_CLEARANCE_RADIUS, ((work->chaseRate + 2) * 0x78) / 0x12) != 0) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12);
            }
        } else {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_CHASE_CLEARANCE_RADIUS, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2) != 0) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = ODD_STRANGER_ANIM_RUN;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    // Prefer grid corrections; separate body overlap only when neither grid push moved X/Z.
    if (_actor401000ApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401000_GRID_ROOT_Y_OFFSET) != 1 && _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCollisionClearContacts(work->hitContacts);
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    if (work->exitCounter >= ACTOR_401000_CHASE_ALERT_TICKS) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/odd_stranger_chase.inc.c"

#include "../../shared/odd_stranger_turn_around.inc.c"

#include "../../shared/odd_stranger_sidestep.inc.c"

/// Starts the player's struggle hold and enters pull only when it is accepted.
///
/// Requires live actor work, the player task and the selected grab animation bank.
/// Requests eight direction/face-button presses. Recovery rejects the hold with
/// reply 1, leaving actor state and animation untouched; reply 0 enters pull and
/// sets the held flag before replacing the player's bank and playing clip 1.
/// Requests are consumed synchronously; the installed animation data stays borrowed.
static __inline__ void _oddStrangerTryStartPlayerHold(OddStrangerWork* work)
{
    enum { ODD_STRANGER_GRAB_HOLD_PRESS_COUNT = 8,
           ODD_STRANGER_GRAB_PLAYER_HOLD_ANIM = 1,
           ODD_STRANGER_GRAB_HOLD_ACCEPTED    = 0 };

    D_actor_401000_80155038.pressCount = ODD_STRANGER_GRAB_HOLD_PRESS_COUNT;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_401000_80155038, 0) == ODD_STRANGER_GRAB_HOLD_ACCEPTED) {
        work->state                        = ODD_STRANGER_STATE_GRAB_PULL;
        work->playerHeld                   = 1;
        gOddStrangerPlayerAnim.animationId = ODD_STRANGER_GRAB_PLAYER_HOLD_ANIM;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &gOddStrangerPlayerAnim, 0);
    }
}

/// Reaches for the player and starts the Odd Stranger's hold on acceptance.
///
/// Requires initialized work/model, live player state and loaded grab banks.
/// Entry disables attacks and resolves grid contacts; the next tick saves the
/// root and faces the player. Offsets narrow to signed halfwords in a common
/// parent frame; angles use 4096 units per turn. Cue 16 requires a turn strictly
/// below 16 and range strictly below 1100 units. An accepted eight-press hold enters
/// GRAB_PULL and starts player animation 1. After the cue, retreats ten units
/// strictly within 1400 units, then corrects grid/body contacts. Direct retreat ignores
/// actor freeze. The animation boundary returns to CHASE.
/// Requires initialized scratch/GTE state; squared X/Z sums must fit s32.
static void _oddStrangerGrabReach(Task* task)
{
    enum {
        ODD_STRANGER_GRAB_CUE                 = 16,
        ODD_STRANGER_GRAB_TURN_LIMIT          = 16,
        ODD_STRANGER_GRAB_REACH               = 1100,
        ODD_STRANGER_GRAB_RETREAT_RADIUS      = 1400,
        ODD_STRANGER_GRAB_RETREAT_STEP        = 10,
        ODD_STRANGER_GRAB_PRIMARY_CHARACTER   = 1,
        ODD_STRANGER_GRAB_PRIMARY_BANK_OFFSET = 2,
        ODD_STRANGER_GRAB_COOLDOWN_TICKS      = 10
    };
    SVECTOR             playerOffset;
    OddStrangerWork*    work;
    Enemy*              enemy;
    const GameActor*    playerActor;
    const PlayerStatus* playerStatus;
    GfxCoord*           stepCoord;
    SVECTOR*            offsetVector;
    s16                 playerTurn;

    enemy        = task->spawnArg2.pointer;
    work         = task->work;
    playerActor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        work->animId                  = ODD_STRANGER_ANIM_GRAB_REACH;
        _oddStrangerDriveAnimation(task);
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        work->sidestepCount = 0;
        work->playerHeld    = 0;
        work->grabCooldown  = ODD_STRANGER_GRAB_COOLDOWN_TICKS;
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->stateTimer = 0;
        return;
    }
    // Resolve the entry contacts before fixing the facing on the next tick.
    if (++work->stateTimer == 1) {
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->grabStartPos.vx                 = task->extra.tmd->coords->coord.t[0];
        work->grabStartPos.vy                 = task->extra.tmd->coords->coord.t[1];
        work->grabStartPos.vz                 = task->extra.tmd->coords->coord.t[2];
        work->hitBody.radius                  = ODD_STRANGER_BODY_RADIUS;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, _actorAngleTurnToPlayer(task, &playerOffset, playerStatus), GRAPHICS_ROTATION_COMPOSE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
        playerOffset.vx                       = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy                       = 0;
        playerOffset.vz                       = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
        work->grabCooldown                    = ODD_STRANGER_GRAB_COOLDOWN_TICKS;
    }
    _oddStrangerDriveAnimation(task);
    // Only the reach cue can request a hold from a nonscripted player.
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ODD_STRANGER_GRAB_CUE && playerActor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        playerTurn = _actorAngleTurnToMatrixPosition(task, &playerOffset, gPlayerStatus.coordMtx);
        if (abs(playerTurn) < ODD_STRANGER_GRAB_TURN_LIMIT && !_oddStrangerOutOfRange(&playerOffset, ODD_STRANGER_GRAB_REACH)) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ODD_STRANGER_GRAB_PRIMARY_CHARACTER) {
                gOddStrangerPlayerAnim.source.sets = &D_actor_401000_80154F00[ODD_STRANGER_GRAB_PRIMARY_BANK_OFFSET];
            } else {
                gOddStrangerPlayerAnim.source.sets = D_actor_401000_80154F00;
            }
            _oddStrangerTryStartPlayerHold(work);
        }
    }
    if (work->animId == ODD_STRANGER_ANIM_GRAB_REACH && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    // After the reach cue, step away along the horizontal separation.
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= ODD_STRANGER_GRAB_CUE + 1) {
        offsetVector    = &playerOffset;
        playerOffset.vx = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy = 0;
        playerOffset.vz = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        if (!_oddStrangerOutOfRange(offsetVector, ODD_STRANGER_GRAB_RETREAT_RADIUS)) {
            VectorNormalSS(offsetVector, offsetVector);
            gte_lddp(ODD_STRANGER_GRAB_RETREAT_STEP);
            gte_ldsv(offsetVector);
            gte_gpf12();
            gte_stsv(offsetVector);
            stepCoord                             = task->extra.tmd->coords;
            stepCoord->coord.t[0]                += playerOffset.vx;
            stepCoord                             = task->extra.tmd->coords;
            stepCoord->coord.t[2]                += playerOffset.vz;
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

/// Staggers backward after a front hit, then plays the back-down animation.
///
/// While staggering, takes an 87-unit backward step when the player-clearance
/// probe permits it and resolves both contact tables. At the down clip boundary,
/// health and status buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000FallBack(Task* task)
{
    enum {
        ACTOR_401000_FALL_BACK_STAGGER_ANIM     = 10,
        ACTOR_401000_FALL_BACK_STEP             = 87,
        ACTOR_401000_FALL_BACK_CLEARANCE_RADIUS = 300
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
        work->animId                  = ACTOR_401000_FALL_BACK_STAGGER_ANIM;
        work->blendActive             = 0;
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
    if (work->animId == ACTOR_401000_FALL_BACK_STAGGER_ANIM && (s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_FALL_BACK_CLEARANCE_RADIUS, -ACTOR_401000_FALL_BACK_STEP) != 0) {
        _actorMovementStepForward(task->extra.tmd->coords, -ACTOR_401000_FALL_BACK_STEP);
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        // The stagger finishes before the back-down clip begins.
        if (work->animId == ACTOR_401000_FALL_BACK_STAGGER_ANIM) {
            work->animId      = ODD_STRANGER_ANIM_DOWN_BACK;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
            _oddStrangerDriveAnimation(task);
        }
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == ODD_STRANGER_ANIM_DOWN_BACK) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                    work->state = ODD_STRANGER_STATE_STATUS_HOLD;
                } else {
                    work->state = ODD_STRANGER_STATE_DOWN;
                }
            } else {
                work->state = ODD_STRANGER_STATE_DEATH_BURN;
            }
        }
    }
}

/// Restores grid response after a fall and selects the next downed state.
///
/// Borrows initialized writable body work and its live enemy record through
/// the call. Nonpositive HP selects burning death; living enemies with buildup
/// enter status hold, otherwise stay down. Other body flags are retained, and
/// the reaction flag is tested without consuming it. Call after a fall boundary
/// or a settled interrupted-rise pose; no animation or lifetime changes occur here.
static __inline__ void _actor401000FinishFall(OddStrangerWork* work, Enemy* enemy)
{
    work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DEATH_BURN;
    } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        work->state = ODD_STRANGER_STATE_STATUS_HOLD;
    } else {
        work->state = ODD_STRANGER_STATE_DOWN;
    }
}

/// Plays the front-down animation after a hit from behind.
///
/// Resolves both contact tables while the fall plays. At its clip boundary,
/// health and status buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000FallFront(Task* task)
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
        _actor401000FinishFall(work, enemy);
    }
}

#include "../../shared/odd_stranger_die.inc.c"

/// Waits in place until the player enters the notice radius or makes combat noise.
///
/// Alternates idle clips at control jumps and clip boundaries. After 2400 quiet
/// ticks, a random one-in-sixteen gate skips the whole update, including notice
/// tests. Entry saves the model's color matrix in work storage.
static void _actor401000Dormant(Task* task)
{
    enum {
        ACTOR_401000_DORMANT_IDLE_ANIM   = 14,
        ACTOR_401000_DORMANT_SHIFT_ANIM  = 15,
        ACTOR_401000_DORMANT_QUIET_TICKS = 2400,
        ACTOR_401000_DORMANT_SKIP_MASK   = 15
    };
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        root;
    SVECTOR          delta;
    SVECTOR*         playerDelta;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model        = task->extra.tmd;
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = ACTOR_401000_DORMANT_IDLE_ANIM;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    // Long-idle throttling also skips awareness and animation work on this tick.
    if (work->stateTimer > ACTOR_401000_DORMANT_QUIET_TICKS) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & ACTOR_401000_DORMANT_SKIP_MASK)) {
            return;
        }
    } else {
        work->stateTimer = (u16)work->stateTimer + 1;
    }
    root            = task->extra.tmd->coords;
    playerDelta     = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    playerDelta->vy = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
    playerDelta->vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    if (!_oddStrangerOutOfRange(playerDelta, work->noticeRadius)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        sceneEngageBattle(1);
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    _oddStrangerDriveAnimation(task);
    if (work->animId == ACTOR_401000_DORMANT_IDLE_ANIM && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = ACTOR_401000_DORMANT_SHIFT_ANIM;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
            _oddStrangerDriveAnimation(task);
        }
    }
    if (work->animId == ACTOR_401000_DORMANT_SHIFT_ANIM && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_401000_DORMANT_IDLE_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        _oddStrangerDriveAnimation(task);
    }
}

#include "../../shared/odd_stranger_dormant.inc.c"

#include "../../shared/odd_stranger_patrol.inc.c"

#include "../../shared/odd_stranger_advance.inc.c"

#include "../../shared/odd_stranger_back_off.inc.c"

#include "../../shared/odd_stranger_hold_aim.inc.c"

/// Adds a part pitch and refreshes the selected composition coordinate.
///
/// Rotation and composition indices remain separate for the retained part-5
/// rotation / part-4 refresh. Use only inside a braced statement block. `task`
/// is evaluated three times and `composeIndex` twice, so arguments must be free
/// of side effects. Pitch uses 4096 units per turn.
#define ACTOR_401000_APPLY_AMBUSH_PITCH(task, partIndex, composeIndex, pitch)                         \
    gfxRotMatrixX(&(task)->extra.tmd->coords[(partIndex)].coord, (pitch), GRAPHICS_ROTATION_COMPOSE); \
    (task)->extra.tmd->coords[(composeIndex)].composeStamp = GRAPHICS_COORD_DIRTY;                    \
    actorRenderComposeCoord(&(task)->extra.tmd->coords[(composeIndex)])

/// Holds a bent ambush pose, straightens and turns toward the player, then chases.
///
/// Entry restarts the ambush clip with two pose ticks. Look yaw advances by at
/// most 40 angle units per tick. The part pitches hold for 49 ticks and then
/// decay every four ticks; root yaw turns by at most 36 units. Alignment or tick
/// 79 selects `CHASE`. Angles use 4096 units per turn. Requires initialized
/// work, model parts 0..5, a live player and one free chase scratch block.
static void _actor401000Ambush(Task* task)
{
    enum {
        ACTOR_401000_AMBUSH_ANIM              = 19,
        ACTOR_401000_AMBUSH_POSE_HOLD_TICKS   = 50,
        ACTOR_401000_AMBUSH_CHASE_TICKS       = 79,
        ACTOR_401000_AMBUSH_LOOK_STEP         = 40,
        ACTOR_401000_AMBUSH_TURN_STEP         = 36,
        ACTOR_401000_AMBUSH_PART1_PITCH       = ACTOR_TRANSFORM_ANGLE_TURN / 64,
        ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH = ACTOR_TRANSFORM_ANGLE_TURN / 32,
        ACTOR_401000_AMBUSH_PART5_PITCH       = ACTOR_TRANSFORM_ANGLE_TURN / 16
    };
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         model;
    GfxCoord*          root;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = 8;
        work->animId            = ACTOR_401000_AMBUSH_ANIM;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = _actorAngleTurnToPlayer(task, &aim->delta, &gPlayerStatus);
    if (work->lookYawTarget < aim->turn) {
        if (aim->turn - work->lookYawTarget > ACTOR_401000_AMBUSH_LOOK_STEP) {
            work->lookYawTarget += ACTOR_401000_AMBUSH_LOOK_STEP;
        } else {
            work->lookYawTarget = aim->turn;
        }
    } else if (work->lookYawTarget - aim->turn > ACTOR_401000_AMBUSH_LOOK_STEP) {
        work->lookYawTarget -= ACTOR_401000_AMBUSH_LOOK_STEP;
    } else {
        work->lookYawTarget = aim->turn;
    }
    root      = task->extra.tmd->coords;
    aim->turn = ratan2(-root->coord.m[2][0], root->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, aim->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    _oddStrangerDriveAnimation(task);
    // Hold the bent pose, then stagger the pitch decay of its five parts.
    if (work->stateTimer < ACTOR_401000_AMBUSH_POSE_HOLD_TICKS) {
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 1, 1, ACTOR_401000_AMBUSH_PART1_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 2, 2, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 3, 3, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 4, 4, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 5, 4, ACTOR_401000_AMBUSH_PART5_PITCH);
    } else {
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 1, 1, ACTOR_401000_AMBUSH_PART1_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 1)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 2, 2, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 2)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 3, 3, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 3)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 4, 4, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 4)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 5, 4, ACTOR_401000_AMBUSH_PART5_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 1)) / 4));
        aim->turn = _actorAngleTurnToPlayer(task, &aim->delta, &gPlayerStatus);
        if (aim->turn > ACTOR_401000_AMBUSH_TURN_STEP) {
            aim->turn = ACTOR_401000_AMBUSH_TURN_STEP;
        } else if (aim->turn < -ACTOR_401000_AMBUSH_TURN_STEP) {
            aim->turn = -ACTOR_401000_AMBUSH_TURN_STEP;
        }
        if (ABS(aim->turn) < ACTOR_401000_AMBUSH_TURN_STEP || work->stateTimer >= ACTOR_401000_AMBUSH_CHASE_TICKS) {
            work->state = ODD_STRANGER_STATE_CHASE;
        }
        root       = task->extra.tmd->coords;
        aim->turn += ratan2(-root->coord.m[2][0], root->coord.m[2][2]);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, aim->turn, GRAPHICS_ROTATION_REPLACE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ACTOR_401000_APPLY_AMBUSH_PITCH

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `stateTimer` from 0 to 0x3D and
/// spawns one effect per key frame, applying area-placement texture offsets
/// through `_actorRenderApplyEffectPlacementTextureOffsets`.
/// At 0x3D the actor returns to state 0. The 401000 twin of
/// `func_actor_401300_8013B6E8`: same five clips, three of them at the same
/// node offsets (`+1`, `+9`, `+12`, `+1`, `+3` off the root coordinate) and the
/// same 0x64/0/0 spawn vector, but it reads the offset from the work block
/// rather than a stack `SVECTOR` and has no `field_D20` guard on the tail.
static void func_actor_401000_8013B1E4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = 0x1AE;
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        work->effectOffset.vx         = 0x64;
        work->effectOffset.vz         = 0;
        work->effectOffset.vy         = 0;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &work->effectOffset);
        sceneReleaseBattleRefWithRewards(arg0, 0xA);
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelA;
        work->effectOffset.vz    = 0x64;
        work->effectOffset.vy    = 0;
        work->effectOffset.vx    = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &work->effectOffset), enemy);
    }
    if ((s16)work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor401000Model123EC;
        work->effectOffset.vy    = 0;
        work->effectOffset.vx    = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(0xA0000 | 5, arg0->extra.tmd->coords + 12, 0x200, &work->effectOffset), enemy);
    }
    if ((s16)work->stateTimer == 7) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelB;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->stateTimer == 8) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelC;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->stateTimer >= 0x3D) {
        work->state = ODD_STRANGER_STATE_HIDDEN;
    }
}

#include "../../shared/odd_stranger_walking_death.inc.c"

#include "../../shared/odd_stranger_stalk.inc.c"

/// Restarts the back-down pose after a hit interrupts rising from the back.
///
/// Resolves both contact tables. Once the pose is settled, health and status
/// buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000RefallBack(Task* task)
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
        _actor401000FinishFall(work, enemy);
    }
}

/// Restarts the front-down pose after a hit interrupts rising from the front.
///
/// Resolves both contact tables. Once the pose is settled, health and status
/// buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000RefallFront(Task* task)
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
        _actor401000FinishFall(work, enemy);
    }
}

/// The actor's state handlers, indexed by `OddStrangerWork::state`. Copied to
/// the frame by `oddStrangerTick` before the dispatch, so the
/// handler may overwrite the live table entry.
static const OddStrangerStateTable gOddStrangerStates = { {
    _actor401000Hidden,
    _oddStrangerPlayWalk,
    _oddStrangerPlayRun,
    _oddStrangerPlayDown,
    _oddStrangerStatusHold,
    _oddStrangerFlinch,
    _oddStrangerAlert,
    _actor401000Chase,
    _oddStrangerCircleDash,
    _oddStrangerTurnAround,
    _oddStrangerSidestep,
    _oddStrangerGrabReach,
    _oddStrangerGrabPull,
    _oddStrangerGrabStrike,
    _oddStrangerGrabRelease,
    _oddStrangerRiseBack,
    _actor401000RiseFront,
    _oddStrangerDown,
    NULL,
    _actor401000FallBack,
    _actor401000FallFront,
    _oddStrangerDeathBurn,
    _actor401000Dormant,
    _oddStrangerDormantScripted,
    _oddStrangerPatrol,
    _oddStrangerBackOff,
    _oddStrangerSlide,
    _oddStrangerWatch,
    _actor401000Ambush,
    func_actor_401000_8013B1E4,
    _oddStrangerStalk,
    _actor401000RefallBack,
    _actor401000RefallFront,
    _oddStrangerWalkingDeath,
} };

#include "../../shared/odd_stranger_tick.inc.c"

/// Ignores message 2015 without accessing the receiver or either payload.
///
/// The binary leaves the reply register unspecified, so callers must ignore
/// the result. The signed callback signature is retained without inventing a reply.
static s32 _actor401000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401000_8013E038`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const EnemyTaskFuncTable3 D_actor_401000_8013207C = { {
    func_actor_401000_80133274,
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

/// Keeps the hidden actor out of active drawing and lock-on.
///
/// On entry disables attack pair contacts and leaves room-grid response enabled;
/// later ticks wait for another handler or room message to change the state.
static void _actor401000Hidden(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                                              = (u16)(model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/odd_stranger_script_pose_2.inc.c"

#include "../../shared/odd_stranger_script_pose_3.inc.c"

#include "../../shared/odd_stranger_script_pose_b.inc.c"

#include "../../shared/odd_stranger_script_pose_d.inc.c"

#include "../../shared/odd_stranger_script_pose_8.inc.c"

/// Rises from the front-down pose and resumes chasing at the clip boundary.
///
/// Entry enables drawing, lock-on and room-grid response and restarts the rise
/// animation at the actor's chase rate.
static void _actor401000RiseFront(Task* task)
{
    enum { ACTOR_401000_RISE_FRONT_ANIM = 22 };
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
        work->animId                  = ACTOR_401000_RISE_FRONT_ANIM;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}

#include "../../shared/odd_stranger_idle.inc.c"

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401000_8013E038(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_401000_8013207C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
