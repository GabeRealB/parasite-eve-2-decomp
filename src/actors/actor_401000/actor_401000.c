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

/// Overlay-data word `oddStrangerDormant` points
/// `gOddStrangerAnimSets[16]` at on entering its state.
extern AnimationSet gOddStrangerDormantAnimSet;

/// Message 0x3FF payload of `oddStrangerGrabHold` and
/// `oddStrangerGrabRelease`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern AnimationPlayRequest gOddStrangerPlayerAnim;

/// Animation blocks selected for the grab by the player-character flag.
extern AnimationSet* D_actor_401000_80154F00[7];

/// Free-running scroll the actor's forward draw accumulates into:
/// `oddStrangerChase` adds `slideStep` to it every frame, and the
/// walk state zeroes it on entry. The same slot `Actor01900` keeps in
/// `Actor01900_D172FC`.
extern u16 gOddStrangerChaseDistance;

/// Twelve `SVECTOR` hit positions `oddStrangerSpawnHitEffect` picks from by
/// damage magnitude. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to. Same table as the
/// `Actor00100_D1B9F4` one `Actor00100_Fn03340` reads.
extern SVECTOR gOddStrangerHitOffsets[12];

/// The two `ActorHeightClamp` rows `func_actor_401000_801352DC` and
/// `func_actor_401000_80135374` walk.
extern ActorHeightClamp D_actor_401000_80154FD0[];

/// Message 0x3E9 payload of `oddStrangerGrab`: the player task's
/// world position, then the yaw from the actor to it, handed straight to the
/// slot-3 handler. The 401000 twin of the block `func_actor_401300_80138800`
/// keeps inline at `_Actor401300Work.playerPlacement`.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401000_8013B1E4`.

/// The records closing four of the overlay's model streams, which
/// `func_actor_401000_8013B1E4` points `D_80114B34[5].data.model` at before spawning, one per
/// animation-latch key frame (`stateTimer` 3, 5, 7, 8).
extern TmdSource gOddStrangerBurstModelA;
static TmdSource _gActor401000Model123EC;
extern TmdSource gOddStrangerBurstModelC;
extern TmdSource gOddStrangerBurstModelB;

/// Handlers defined after the state table and the init that name them.
static void func_actor_401000_8013DB10(Task* arg0);
static void func_actor_401000_8013DEC8(Task* arg0);

/// Integer part of the last delta `ActorContact_PushContact` resolved.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
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

s32 func_actor_401000_8013D68C(Task*, s32, s32, s32);
s32 oddStrangerApplyCommand(Task*, s32, u16*, s32);

TaskMessageEntry D_actor_401000_80154F90[8] = {
    { 2015, func_actor_401000_8013D68C },
    { ACTOR_MESSAGE_PLAY_ANIMATION, oddStrangerPlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, oddStrangerApplyCommand },
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

static __inline__ void Actor401000_BindMatrices(Task* actor);
static __inline__ void Actor401000_InitPose(GfxCoord* coord, OddStrangerWork* work);
static void            func_actor_401000_80133274(Enemy* enemy, Task* actor);
static void            func_actor_401000_801352DC(GameLocationKey* session, GfxCoord* coord);
static __inline__ s32  Actor401000_HasHeightClamp(GameLocationKey* session);
static s32             func_actor_401000_80135374(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static void            func_actor_401000_80135AA4(Task* arg0);
static void            func_actor_401000_801378DC(Task* arg0);
static void            func_actor_401000_801388F4(Task* arg0);
static void            func_actor_401000_80138BB4(Task* arg0);
static void            func_actor_401000_80138F50(Task* arg0);
static void            func_actor_401000_8013A930(Task* arg0);
static void            func_actor_401000_8013B1E4(Task* arg0);
static void            func_actor_401000_8013CD9C(Task* arg0);
static void            func_actor_401000_8013CEF0(Task* arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401000_BindMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

/// Rebuilds the root coordinate's Y rotation at the actor's 0x1194 scale and
/// drops both obstacle tables.
static __inline__ void Actor401000_InitPose(GfxCoord* coord, OddStrangerWork* work)
{
    actorRescaleYaw(coord, 0x1194);
    work->bodyPosCursor = 0;
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
}

/// Enemy init: allocates the 0xC80-byte work block, binds the model's light
/// and colour matrices to its copies, seeds both animation contexts and the
/// three `WorldCollisionBody` nodes, then picks the opening clip from the low bits of
/// `Enemy::placeKey` and the `downFramesBase` parameter run from the spawn flags.
/// The tail rebuilds the root coordinate through `Actor401000_InitPose`.
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
    actor->exitCallback = oddStrangerExit;
    Actor401000_BindMatrices(actor);
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
    oddStrangerDrive(actor);

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

    Actor401000_InitPose(actor->extra.tmd->coords, work);

    actor->state++;
}

#include "../../shared/odd_stranger_spawn_hit_effect.inc.c"

#include "../../shared/odd_stranger_take_hit.inc.c"

#include "../../shared/odd_stranger_stunned.inc.c"

#include "../../shared/odd_stranger_face_player.inc.c"

static void func_actor_401000_801352DC(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->stage && session->area == row->area) {
            lo     = row->minY;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->maxY < offset) {
                coord->coord.t[1] = row->maxY;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

/// Whether `D_actor_401000_80154FD0` has a row matching the session's
/// `GameLocationKey::stage` / `area` pair. The helper behind both
/// height-clamp probes of `func_actor_401000_80135374`; the second probe is
/// followed by the `func_actor_401000_801352DC` call itself, which walks the
/// same rows to clamp the root Y. Same helper as `Actor401300_HasHeightClamp`.
static __inline__ s32 Actor401000_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->stage && session->area == row->area) {
            return 1;
        }
    }
    return 0;
}

/// Caps the Y step at ±0x12C: once while a height-clamp row matches, where an
/// in-range step is done, and once more regardless.
static inline void Actor401000_CapStepY(ActorContactCappedPushScratch* s)
{
    s16 vy;
    s16 clamped;

    if (Actor401000_HasHeightClamp(&gGameSession->location.loc)) {
        vy = s->step.vy;
        if (((vy >= 0) ? vy : -vy) <= 0x12C) {
            return;
        }
        clamped    = (vy <= 0) ? -0x12C : 0x12C;
        s->step.vy = clamped;
    }
    vy = s->step.vy;
    if (((vy >= 0) ? vy : -vy) <= 0x12C) {
        return;
    }
    s->step.vy = (vy <= 0) ? -0x12C : 0x12C;
}

/// Root-coordinate step, the 401000 twin of `func_actor_401300_80132C78`:
/// carve the 0x20-byte `ActorContactCappedPushScratch` off the scratch stack,
/// fill its delta from the `rec` obstacle record, clamp the Y step to ±0x12C while a
/// height-clamp row matches, hand the XZ step to the GTE normalisation once it
/// passes 0x96, and step the root coordinate by each component. Reports
/// whether anything moved.
///
/// The clamp is `Actor401000_CapStepY`. Its `clamped` temporary is still
/// needed for register allocation: without it `$s0`/`$s1` swap between the
/// block pointer and the `step` local.
static s32 func_actor_401000_80135374(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorContactCappedPushScratch* head;
    ActorContactCappedPushScratch* s;
    SVECTOR*                       step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                                                = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = head - 1;
    s                                                   = head - 1;
    s->moved                                            = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.fixed.vx.word >> 16;
        s->step.vy = s->delta.fixed.vy.word >> 16;
        s->step.vz = s->delta.fixed.vz.word >> 16;
        Actor401000_CapStepY(s);
        coord->coord.t[1] += s->step.vy;
        s->stepLength      = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->stepLength      = SquareRoot0(s->stepLength);
        step               = &s->step;
        if (s->stepLength >= 0x96) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0x96);
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
    if (Actor401000_HasHeightClamp(&gGameSession->location.loc)) {
        func_actor_401000_801352DC(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.fixed.vx.word != 0 || s->delta.fixed.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return s->moved;
}

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
            actorCalcPush(&s->position, &recs[s->recordIndex], &s->offset);
            s->offsetLength = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->offsetLength = SquareRoot0(s->offsetLength);
            if (s->offsetLength >= 0x6B) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x6B);
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
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return s->hit;
}

static void func_actor_401000_80135AA4(Task* arg0)
{
    ActorChaseScratch* head;
    ActorChaseScratch* chase;
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    s32                angle;
    s32                diff;
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
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x1AE;
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
    work->exitCounter++;
    head  = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &head[-1].delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    chase->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                              gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yawFromPlayer = actorNormalizeYaw(chase->yawFromPlayer);
    coord                = arg0->extra.tmd->coords;
    chase->turn          = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget  = chase->turn;
    diff                 = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(diff) < 0x44) {
        if (work->sidestepDelay + work->sidestepCount / 2 < work->stateTimer) {
            angle = chase->turn;
            if (angle < 0) {
                angle = -angle;
            }
            if (angle < 0x80) {
                if (oddStrangerOutOfRange(&chase->delta, 0x708) && detectSightBlocked(arg0) != 1) {
                    work->state = ODD_STRANGER_STATE_SIDESTEP;
                }
            }
        }
    }
    if (detectSightBlocked(arg0) != 1) {
        work->stateTimer++;
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < 0x200) {
            if (!oddStrangerOutOfRange(&chase->delta, 0x44C) && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            }
        }
    } else {
        work->stateTimer    = 0;
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 1) {
            chase->turn += 0x300;
        } else {
            chase->turn -= 0x300;
        }
        if (work->stateTimer >= 0x169) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    if (chase->turn > 0x40) {
        chase->turn = 0x40;
    }
    if (chase->turn < -0x40) {
        chase->turn = -0x40;
    }
    chase->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 3) {
        if (work->blendActive == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->chaseRate + 2) * 0x78) / 0x12) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12);
            }
        } else {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = 3;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    if (func_actor_401000_80135374(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x4B) != 1 && ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
        oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg0->extra.tmd->coords);
    worldCollisionClearContacts(work->hitContacts);
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    if (work->exitCounter >= 0x4C) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/odd_stranger_chase.inc.c"

#include "../../shared/odd_stranger_turn_around.inc.c"

#include "../../shared/odd_stranger_sidestep.inc.c"

static void func_actor_401000_801378DC(Task* arg0)
{
    SVECTOR          delta;
    OddStrangerWork* work;
    Enemy*           enemy;
    GameActor*       player;
    PlayerStatus*    config;
    GfxCoord*        coord;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = (GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    config = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        work->animId                  = 4;
        oddStrangerDrive(arg0);
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg0->extra.tmd->coords);
        work->sidestepCount = 0;
        work->playerHeld    = 0;
        work->grabCooldown  = 0xA;
        ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->stateTimer = 0;
        return;
    }
    if (++work->stateTimer == 1) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->grabStartPos.vx                 = arg0->extra.tmd->coords->coord.t[0];
        work->grabStartPos.vy                 = arg0->extra.tmd->coords->coord.t[1];
        work->grabStartPos.vz                 = arg0->extra.tmd->coords->coord.t[2];
        work->hitBody.radius                  = 0x1AE;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, actorPositionYaw(arg0, &delta, config), 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        delta.vx                              = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy                              = 0;
        delta.vz                              = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
        work->grabCooldown                    = 0xA;
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &delta, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !oddStrangerOutOfRange(&delta, 0x44C)) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                gOddStrangerPlayerAnim.source.sets = &D_actor_401000_80154F00[2];
            } else {
                gOddStrangerPlayerAnim.source.sets = D_actor_401000_80154F00;
            }
            D_actor_401000_80155038.pressCount = 8;
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_401000_80155038, 0) == 0) {
                work->state                        = ODD_STRANGER_STATE_GRAB_PULL;
                work->playerHeld                   = 1;
                gOddStrangerPlayerAnim.animationId = 1;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &gOddStrangerPlayerAnim, 0);
            }
        }
    }
    if (work->animId == 4 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 0x11) {
        p        = &delta;
        delta.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy = 0;
        delta.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!oddStrangerOutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[0]                    += delta.vx;
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[2]                    += delta.vz;
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

/// State 8 body, the 401000 twin of `func_actor_401300_80138CF8`: on the
/// live-actor flag, reset the two animation nodes, the root coordinate and the
/// model's facing, then slide the root along both obstacle tables and take one
/// forward step while the 0x12C probe is still in range. The tail keys the
/// actor's next state (`state`) off `Enemy.hp` / `.reactionFlags` whenever
/// the work block's pending-request bit is up.
static void func_actor_401000_801388F4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x1AE;
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
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (work->animId == 0xA && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x57) != 0) {
        _actorMovementStepForward(arg0->extra.tmd->coords, -0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (work->animId == 0xA) {
            work->animId      = 0xB;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
            oddStrangerDrive(arg0);
        }
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == 0xB) {
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

/// State 9 body, the 401000 twin of `func_actor_401300_80140300` and
/// `Actor01900_Fn09BE8`: on the live-actor flag, reset the two animation nodes,
/// the root coordinate and the model's facing, then hand the root to the
/// obstacle helper once per record table. The tail keys the actor's next state
/// (`state`) off `Enemy.hp` / `.reactionFlags` whenever the work block's
/// pending-request bit is up.
static void func_actor_401000_80138BB4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x1AE;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = 0xC;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
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

/// Per-frame tick of the 0xE/0xF animation pair, the same shape as
/// `oddStrangerDormant`. The live-actor arm allocates the model
/// buffers, keeps a copy of `colorMtx` in
/// `savedColorMtx`, and restarts the 0xE / 0x898 animation slots; the body is then
/// gated on the `stateTimer` countdown and a 0-15 `gRandomLcgState` draw. The XZ
/// offset to `gPlayerStatus.coordMtx` is probed against `noticeRadius`, and an armed
/// `gSceneCombatState` bit 0x50000, each dropping the actor to state 6. The tail runs
/// `oddStrangerDrive` and swaps `animId` between 0xE and 0xF on
/// `rig.slots[1].status` bits 1 and 2, re-running the tick after each swap.
/// Same body as `func_actor_401300_80139520`, with the pose matrix in place of
/// that one's `field_C48` / `field_C68` pair and a `noticeRadius` radius in place
/// of its literal 3000.
static void func_actor_401000_80138F50(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj        = arg0->extra.tmd;
        obj->flags = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x1AE;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = 0xE;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    if (work->stateTimer > 0x960) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->stateTimer = (u16)work->stateTimer + 1;
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
        Gp_ArmStateF0(1);
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    oddStrangerDrive(arg0);
    if (work->animId == 0xE && (work->rig.slots[1].status.fields.flags & 2)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = 0xF;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
            oddStrangerDrive(arg0);
        }
    }
    if (work->animId == 0xF && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
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

/// Turn the actor toward the player in two stages: while the `stateTimer`
/// countdown is under 0x32 the five part coordinates are reset to fixed
/// pitches, and afterwards each one unwinds by shifting its pitch down a step
/// every four frames; the turn itself is clamped to +-0x24 and drops the actor
/// to state 7 once it lines up. The `lookYawTarget` slot it drives is what
/// `oddStrangerHoldAim` writes whole; here it slides toward the target
/// by at most 0x28 a frame. Same body as `func_actor_401300_8013AE48`, with the
/// spawn arm arming the 0x13 clip, `hitBody.field_1C` written before the other
/// state words, and `gridBody.flags |= 0x4000` in place of the sibling's
/// `&= 0xBFFF`.
static void func_actor_401000_8013A930(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x1AE;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = 8;
        work->animId            = 0x13;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        oddStrangerDrive(arg0);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->lookYawTarget < aim->turn) {
        if (aim->turn - work->lookYawTarget > 0x28) {
            work->lookYawTarget += 0x28;
        } else {
            work->lookYawTarget = aim->turn;
        }
    } else if (work->lookYawTarget - aim->turn > 0x28) {
        work->lookYawTarget -= 0x28;
    } else {
        work->lookYawTarget = aim->turn;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    oddStrangerDrive(arg0);
    if (work->stateTimer < 0x32) {
        gfxRotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
    } else {
        gfxRotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40 >> ((work->stateTimer - 0x31) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80 >> ((work->stateTimer - 0x30) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80 >> ((work->stateTimer - 0x2F) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80 >> ((work->stateTimer - 0x2E) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100 >> ((work->stateTimer - 0x31) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
        if (aim->turn > 0x24) {
            aim->turn = 0x24;
        } else if (aim->turn < -0x24) {
            aim->turn = -0x24;
        }
        if (ABS(aim->turn) < 0x24 || work->stateTimer >= 0x4F) {
            work->state = ODD_STRANGER_STATE_CHASE;
        }
        coord      = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `stateTimer` from 0 to 0x3D and
/// spawns one effect per key frame, each tinted by `actorTintEffect`.
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
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &work->effectOffset);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelA;
        work->effectOffset.vz    = 0x64;
        work->effectOffset.vy    = 0;
        work->effectOffset.vx    = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &work->effectOffset), enemy);
    }
    if ((s16)work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor401000Model123EC;
        work->effectOffset.vy    = 0;
        work->effectOffset.vx    = 0;
        actorTintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->extra.tmd->coords + 12, 0x200, &work->effectOffset), enemy);
    }
    if ((s16)work->stateTimer == 7) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelB;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->stateTimer == 8) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelC;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->stateTimer >= 0x3D) {
        work->state = ODD_STRANGER_STATE_HIDDEN;
    }
}

#include "../../shared/odd_stranger_walking_death.inc.c"

#include "../../shared/odd_stranger_stalk.inc.c"

/// State 9 clip-0xB body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401000_8013CEF0`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0xB / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `Enemy.hp` / `.reactionFlags` pick the next
/// `state` whenever the request bit is up.
static void func_actor_401000_8013CD9C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x1AE;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0xB;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

/// State 10 body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401300_8014046C`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0x19 / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `Enemy.hp` / `.reactionFlags` pick the next
/// `state` whenever the request bit is up.
static void func_actor_401000_8013CEF0(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x1AE;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0x19;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (enemy->hp <= 0) {
            work->state = ODD_STRANGER_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ODD_STRANGER_STATE_STATUS_HOLD;
        } else {
            work->state = ODD_STRANGER_STATE_DOWN;
        }
    }
}

/// The actor's state handlers, indexed by `OddStrangerWork::state`. Copied to
/// the frame by `oddStrangerTick` before the dispatch, so the
/// handler may overwrite the live table entry.
static const OddStrangerStateTable gOddStrangerStates = { {
    func_actor_401000_8013DB10,
    oddStrangerScriptPose2,
    oddStrangerScriptPose3,
    oddStrangerScriptPoseB,
    oddStrangerStunned,
    oddStrangerScriptPoseD,
    oddStrangerFacePlayer,
    func_actor_401000_80135AA4,
    oddStrangerChase,
    oddStrangerTurnAround,
    oddStrangerSidestep,
    func_actor_401000_801378DC,
    oddStrangerGrab,
    oddStrangerGrabHold,
    oddStrangerGrabRelease,
    oddStrangerScriptPose8,
    func_actor_401000_8013DEC8,
    oddStrangerIdle,
    NULL,
    func_actor_401000_801388F4,
    func_actor_401000_80138BB4,
    oddStrangerDie,
    func_actor_401000_80138F50,
    oddStrangerDormant,
    oddStrangerPatrol,
    oddStrangerBackOff,
    oddStrangerAdvance,
    oddStrangerHoldAim,
    func_actor_401000_8013A930,
    func_actor_401000_8013B1E4,
    oddStrangerStalk,
    func_actor_401000_8013CD9C,
    func_actor_401000_8013CEF0,
    oddStrangerWalkingDeath,
} };

#include "../../shared/odd_stranger_tick.inc.c"

s32 func_actor_401000_8013D68C(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

static void func_actor_401000_8013DB10(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/odd_stranger_script_pose_2.inc.c"

#include "../../shared/odd_stranger_script_pose_3.inc.c"

#include "../../shared/odd_stranger_script_pose_b.inc.c"

#include "../../shared/odd_stranger_script_pose_d.inc.c"

#include "../../shared/odd_stranger_script_pose_8.inc.c"

static void func_actor_401000_8013DEC8(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x1AE;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = 0x16;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    oddStrangerDrive(arg0);
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
