#include "actors/actor_420700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/scripted_walk.h"

/// The animation request and the head turn the actor keeps right after its
/// rig.
///
/// The scripted walk library's slot reset reads `animId` and records
/// `appliedAnimId` through the block's `st` member, so those two keep the
/// library's names. The first three members are the ones `ActorEnemyState`
/// opens with, used the same way; from `turnMode` on the layout is the
/// package's own, so the block is not that type.
typedef struct {
    s16 state;         // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16 appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16 animId;        // Clip the last play request selected, an index into the actor's animation sets
    s16 turnMode;      // What the head turn does each frame (`ACTOR_420700_TURN_*`), as the last actor command set it
    s16 turnWeight;    // Share of the remaining angle the head turns each frame, 0 to `ONE` for none to all of it
    s16 field_A;       // Cleared by each play request; never read, role unproven
} _Actor420700State;
STATIC_ASSERT_SIZEOF(_Actor420700State, 0xC);

/// What the actor's head turn does each frame, kept in
/// `_Actor420700State::turnMode`: the command `ACTOR_COMMAND_MESSAGE_APPLY`
/// carries.
enum {
    ACTOR_420700_TURN_AUTO    = 0, // Toward the player, the weight rising while the player faces away from the actor and falling otherwise
    ACTOR_420700_TURN_PLAYER  = 1, // Toward the player, the weight rising from 0
    ACTOR_420700_TURN_RELEASE = 2, // Toward the player, the weight falling from `ONE`
    ACTOR_420700_TURN_POINT   = 3, // Toward a fixed point of the room, the weight rising from 0
};

/// Work block of the overlay's actor, allocated zeroed at its full size by the
/// spawn step and kept both at `Task::work` and in `gScriptedWalkWork`, which
/// the actor's task handler republishes every tick.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives. The matrices, the rig and the request at the start of `st` sit where
/// the scripted walkers' blocks keep theirs, which is what lets the package
/// carry that library's slot tick and slot reset under the library's name for
/// the block. It has no walk, and the rest of `st` is its own.
typedef struct {
    MATRIX            light;         // Light-direction matrix lent to the model object
    MATRIX            color;         // Light-colour matrix lent to the model object
    ActorAnimRig20    rig;           // Playback storage of the model's parts; slots 1 to 19 are driven
    _Actor420700State st;            // Animation request and head-turn state
    byte              pad_4C0[0xE0]; // Rest of the allocation; the package neither reads nor writes it, contents unproven
} _Actor420700Work;
STATIC_ASSERT_SIZEOF(_Actor420700Work, 0x5A0);

/// The work block above, published by the task dispatcher
/// `func_actor_420700_80132340` and by the state-0 handler.
extern _Actor420700Work* gScriptedWalkWork;

/// The actor's own task, the `task` the state-0 handler
/// `func_actor_420700_80131E24` is entered with. Its `Task::extra` holds the
/// `TmdObject` whose trailing coordinate array `func_actor_420700_801323D8`
/// hangs the model task's own root off, at frame 4.
extern Task* D_actor_420700_8013EFE4;

/// The first task the state-0 handler spawns, the frame-4 model task
/// `func_actor_420700_801323D8`; the actor's exit callback kills it.
extern Task* D_actor_420700_8013EFE8;

/// The second task the state-0 handler spawns, the frame-8 model task
/// `func_actor_420700_801327EC`.
extern Task* D_actor_420700_8013EFEC;

static void func_actor_420700_8013239C(Task* task);
static void func_actor_420700_80132478(Task* task);
static void func_actor_420700_801325C8(void);

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_420700_8013EF48[4];
extern TaskDesc         D_actor_420700_8013EF68[];
extern u8               D_actor_420700_8013EF8C[];
extern s32              D_actor_420700_8013EFF0;
extern s32              D_actor_420700_8013EFF4;

static TmdSource _gActor420700GaryDouglasBody;
static TmdSource _gActor420700GaryDouglasHeadHat;
static TmdSource _gActor420700GaryDouglasShotgun;
void             func_actor_420700_80132340(Task*);
void             func_actor_420700_801323D8(Task*);
void             func_actor_420700_801327EC(Task*);

s32 func_actor_420700_80132644(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_420700_801326F4(Task*, s32, s32, s32);
s32 func_actor_420700_80132784(Task* task, s32 msgId, ActorCommand* args, s32 arg3);

static AnimationPackedPose _gActor420700Animation00DE0Bank1[7] = {
#include "assets/actor_420700_animation_00DE0_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation00DE0Bank4[65] = {
#include "assets/actor_420700_animation_00DE0_bank4.inc"
};

static AnimationRecord _gActor420700Animation00DE0Records[123] = {
#include "assets/actor_420700_animation_00DE0_records.inc"
};

static u16 _gActor420700Animation00DE0Indices[20] = {
#include "assets/actor_420700_animation_00DE0_indices.inc"
};

AnimationSet gActor420700Animation00DE0 = {
    _gActor420700Animation00DE0Records,
    _gActor420700Animation00DE0Indices,
    { NULL, _gActor420700Animation00DE0Bank1, NULL, NULL, _gActor420700Animation00DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation013A8Bank1[19] = {
#include "assets/actor_420700_animation_013A8_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation013A8Bank4[119] = {
#include "assets/actor_420700_animation_013A8_bank4.inc"
};

static AnimationRecord _gActor420700Animation013A8Records[174] = {
#include "assets/actor_420700_animation_013A8_records.inc"
};

static u16 _gActor420700Animation013A8Indices[20] = {
#include "assets/actor_420700_animation_013A8_indices.inc"
};

AnimationSet gActor420700Animation013A8 = {
    _gActor420700Animation013A8Records,
    _gActor420700Animation013A8Indices,
    { NULL, _gActor420700Animation013A8Bank1, NULL, NULL, _gActor420700Animation013A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0164CBank1[2] = {
#include "assets/actor_420700_animation_0164C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0164CBank4[48] = {
#include "assets/actor_420700_animation_0164C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0164CRecords[95] = {
#include "assets/actor_420700_animation_0164C_records.inc"
};

static u16 _gActor420700Animation0164CIndices[20] = {
#include "assets/actor_420700_animation_0164C_indices.inc"
};

AnimationSet gActor420700Animation0164C = {
    _gActor420700Animation0164CRecords,
    _gActor420700Animation0164CIndices,
    { NULL, _gActor420700Animation0164CBank1, NULL, NULL, _gActor420700Animation0164CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation01878Bank1[2] = {
#include "assets/actor_420700_animation_01878_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation01878Bank4[34] = {
#include "assets/actor_420700_animation_01878_bank4.inc"
};

static AnimationRecord _gActor420700Animation01878Records[79] = {
#include "assets/actor_420700_animation_01878_records.inc"
};

static u16 _gActor420700Animation01878Indices[20] = {
#include "assets/actor_420700_animation_01878_indices.inc"
};

AnimationSet gActor420700Animation01878 = {
    _gActor420700Animation01878Records,
    _gActor420700Animation01878Indices,
    { NULL, _gActor420700Animation01878Bank1, NULL, NULL, _gActor420700Animation01878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation01C5CBank1[8] = {
#include "assets/actor_420700_animation_01C5C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation01C5CBank4[77] = {
#include "assets/actor_420700_animation_01C5C_bank4.inc"
};

static AnimationRecord _gActor420700Animation01C5CRecords[128] = {
#include "assets/actor_420700_animation_01C5C_records.inc"
};

static u16 _gActor420700Animation01C5CIndices[20] = {
#include "assets/actor_420700_animation_01C5C_indices.inc"
};

AnimationSet gActor420700Animation01C5C = {
    _gActor420700Animation01C5CRecords,
    _gActor420700Animation01C5CIndices,
    { NULL, _gActor420700Animation01C5CBank1, NULL, NULL, _gActor420700Animation01C5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation01EBCBank1[2] = {
#include "assets/actor_420700_animation_01EBC_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation01EBCBank4[37] = {
#include "assets/actor_420700_animation_01EBC_bank4.inc"
};

static AnimationRecord _gActor420700Animation01EBCRecords[89] = {
#include "assets/actor_420700_animation_01EBC_records.inc"
};

static u16 _gActor420700Animation01EBCIndices[20] = {
#include "assets/actor_420700_animation_01EBC_indices.inc"
};

AnimationSet gActor420700Animation01EBC = {
    _gActor420700Animation01EBCRecords,
    _gActor420700Animation01EBCIndices,
    { NULL, _gActor420700Animation01EBCBank1, NULL, NULL, _gActor420700Animation01EBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation02328Bank1[7] = {
#include "assets/actor_420700_animation_02328_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation02328Bank4[96] = {
#include "assets/actor_420700_animation_02328_bank4.inc"
};

static AnimationRecord _gActor420700Animation02328Records[146] = {
#include "assets/actor_420700_animation_02328_records.inc"
};

static u16 _gActor420700Animation02328Indices[20] = {
#include "assets/actor_420700_animation_02328_indices.inc"
};

AnimationSet gActor420700Animation02328 = {
    _gActor420700Animation02328Records,
    _gActor420700Animation02328Indices,
    { NULL, _gActor420700Animation02328Bank1, NULL, NULL, _gActor420700Animation02328Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation02F18Bank1[28] = {
#include "assets/actor_420700_animation_02F18_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation02F18Bank4[301] = {
#include "assets/actor_420700_animation_02F18_bank4.inc"
};

static AnimationRecord _gActor420700Animation02F18Records[359] = {
#include "assets/actor_420700_animation_02F18_records.inc"
};

static u16 _gActor420700Animation02F18Indices[20] = {
#include "assets/actor_420700_animation_02F18_indices.inc"
};

AnimationSet gActor420700Animation02F18 = {
    _gActor420700Animation02F18Records,
    _gActor420700Animation02F18Indices,
    { NULL, _gActor420700Animation02F18Bank1, NULL, NULL, _gActor420700Animation02F18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0316CBank1[3] = {
#include "assets/actor_420700_animation_0316C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0316CBank4[28] = {
#include "assets/actor_420700_animation_0316C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0316CRecords[92] = {
#include "assets/actor_420700_animation_0316C_records.inc"
};

static u16 _gActor420700Animation0316CIndices[20] = {
#include "assets/actor_420700_animation_0316C_indices.inc"
};

AnimationSet gActor420700Animation0316C = {
    _gActor420700Animation0316CRecords,
    _gActor420700Animation0316CIndices,
    { NULL, _gActor420700Animation0316CBank1, NULL, NULL, _gActor420700Animation0316CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation03404Bank1[6] = {
#include "assets/actor_420700_animation_03404_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation03404Bank4[38] = {
#include "assets/actor_420700_animation_03404_bank4.inc"
};

static AnimationRecord _gActor420700Animation03404Records[90] = {
#include "assets/actor_420700_animation_03404_records.inc"
};

static u16 _gActor420700Animation03404Indices[20] = {
#include "assets/actor_420700_animation_03404_indices.inc"
};

AnimationSet gActor420700Animation03404 = {
    _gActor420700Animation03404Records,
    _gActor420700Animation03404Indices,
    { NULL, _gActor420700Animation03404Bank1, NULL, NULL, _gActor420700Animation03404Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation035D4Bank1[3] = {
#include "assets/actor_420700_animation_035D4_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation035D4Bank4[30] = {
#include "assets/actor_420700_animation_035D4_bank4.inc"
};

static AnimationRecord _gActor420700Animation035D4Records[57] = {
#include "assets/actor_420700_animation_035D4_records.inc"
};

static u16 _gActor420700Animation035D4Indices[20] = {
#include "assets/actor_420700_animation_035D4_indices.inc"
};

AnimationSet gActor420700Animation035D4 = {
    _gActor420700Animation035D4Records,
    _gActor420700Animation035D4Indices,
    { NULL, _gActor420700Animation035D4Bank1, NULL, NULL, _gActor420700Animation035D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation03DD8Bank1[11] = {
#include "assets/actor_420700_animation_03DD8_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation03DD8Bank4[186] = {
#include "assets/actor_420700_animation_03DD8_bank4.inc"
};

static AnimationRecord _gActor420700Animation03DD8Records[274] = {
#include "assets/actor_420700_animation_03DD8_records.inc"
};

static u16 _gActor420700Animation03DD8Indices[20] = {
#include "assets/actor_420700_animation_03DD8_indices.inc"
};

AnimationSet gActor420700Animation03DD8 = {
    _gActor420700Animation03DD8Records,
    _gActor420700Animation03DD8Indices,
    { NULL, _gActor420700Animation03DD8Bank1, NULL, NULL, _gActor420700Animation03DD8Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor420700GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor420700GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor420700GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor420700GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor420700GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor420700GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor420700GaryDouglasBodyPartVerts,
    _gActor420700GaryDouglasBodyVerts,
    _gActor420700GaryDouglasBodyNormals,
    _gActor420700GaryDouglasBodySkeleton,
    _gActor420700GaryDouglasBodyStream,
};

static TmdBone _gActor420700GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor420700GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor420700GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor420700GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor420700GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor420700GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor420700GaryDouglasHeadHatPartVerts,
    _gActor420700GaryDouglasHeadHatVerts,
    _gActor420700GaryDouglasHeadHatNormals,
    _gActor420700GaryDouglasHeadHatSkeleton,
    _gActor420700GaryDouglasHeadHatStream,
};

static TmdBone _gActor420700GaryDouglasShotgunSkeleton[1] = {
#include "assets/gary_douglas_shotgun_skeleton.inc"
};

static u32 _gActor420700GaryDouglasShotgunPartVerts[1] = {
#include "assets/gary_douglas_shotgun_partVerts.inc"
};

static SVECTOR _gActor420700GaryDouglasShotgunVerts[26] = {
#include "assets/gary_douglas_shotgun_verts.inc"
};

static SVECTOR _gActor420700GaryDouglasShotgunNormals[26] = {
#include "assets/gary_douglas_shotgun_normals.inc"
};

static u32 _gActor420700GaryDouglasShotgunStream[182] = {
#include "assets/gary_douglas_shotgun_stream.inc"
};

static TmdSource _gActor420700GaryDouglasShotgun = {
    0,
    1276,
    0,
    1,
    _gActor420700GaryDouglasShotgunPartVerts,
    _gActor420700GaryDouglasShotgunVerts,
    _gActor420700GaryDouglasShotgunNormals,
    _gActor420700GaryDouglasShotgunSkeleton,
    _gActor420700GaryDouglasShotgunStream,
};

static AnimationPackedPose _gActor420700Animation0A464Bank1[2] = {
#include "assets/actor_420700_animation_0A464_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0A464Bank4[30] = {
#include "assets/actor_420700_animation_0A464_bank4.inc"
};

static AnimationRecord _gActor420700Animation0A464Records[96] = {
#include "assets/actor_420700_animation_0A464_records.inc"
};

static u16 _gActor420700Animation0A464Indices[20] = {
#include "assets/actor_420700_animation_0A464_indices.inc"
};

static AnimationSet _gActor420700Animation0A464 = {
    _gActor420700Animation0A464Records,
    _gActor420700Animation0A464Indices,
    { NULL, _gActor420700Animation0A464Bank1, NULL, NULL, _gActor420700Animation0A464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0A610Bank1[2] = {
#include "assets/actor_420700_animation_0A610_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0A610Bank4[20] = {
#include "assets/actor_420700_animation_0A610_bank4.inc"
};

static AnimationRecord _gActor420700Animation0A610Records[61] = {
#include "assets/actor_420700_animation_0A610_records.inc"
};

static u16 _gActor420700Animation0A610Indices[20] = {
#include "assets/actor_420700_animation_0A610_indices.inc"
};

static AnimationSet _gActor420700Animation0A610 = {
    _gActor420700Animation0A610Records,
    _gActor420700Animation0A610Indices,
    { NULL, _gActor420700Animation0A610Bank1, NULL, NULL, _gActor420700Animation0A610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0A7BCBank1[2] = {
#include "assets/actor_420700_animation_0A7BC_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0A7BCBank4[20] = {
#include "assets/actor_420700_animation_0A7BC_bank4.inc"
};

static AnimationRecord _gActor420700Animation0A7BCRecords[61] = {
#include "assets/actor_420700_animation_0A7BC_records.inc"
};

static u16 _gActor420700Animation0A7BCIndices[20] = {
#include "assets/actor_420700_animation_0A7BC_indices.inc"
};

static AnimationSet _gActor420700Animation0A7BC = {
    _gActor420700Animation0A7BCRecords,
    _gActor420700Animation0A7BCIndices,
    { NULL, _gActor420700Animation0A7BCBank1, NULL, NULL, _gActor420700Animation0A7BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0A96CBank1[2] = {
#include "assets/actor_420700_animation_0A96C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0A96CBank4[22] = {
#include "assets/actor_420700_animation_0A96C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0A96CRecords[60] = {
#include "assets/actor_420700_animation_0A96C_records.inc"
};

static u16 _gActor420700Animation0A96CIndices[20] = {
#include "assets/actor_420700_animation_0A96C_indices.inc"
};

static AnimationSet _gActor420700Animation0A96C = {
    _gActor420700Animation0A96CRecords,
    _gActor420700Animation0A96CIndices,
    { NULL, _gActor420700Animation0A96CBank1, NULL, NULL, _gActor420700Animation0A96CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0AD64Bank1[2] = {
#include "assets/actor_420700_animation_0AD64_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0AD64Bank4[79] = {
#include "assets/actor_420700_animation_0AD64_bank4.inc"
};

static AnimationRecord _gActor420700Animation0AD64Records[149] = {
#include "assets/actor_420700_animation_0AD64_records.inc"
};

static u16 _gActor420700Animation0AD64Indices[20] = {
#include "assets/actor_420700_animation_0AD64_indices.inc"
};

static AnimationSet _gActor420700Animation0AD64 = {
    _gActor420700Animation0AD64Records,
    _gActor420700Animation0AD64Indices,
    { NULL, _gActor420700Animation0AD64Bank1, NULL, NULL, _gActor420700Animation0AD64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0AF1CBank1[2] = {
#include "assets/actor_420700_animation_0AF1C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0AF1CBank4[24] = {
#include "assets/actor_420700_animation_0AF1C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0AF1CRecords[60] = {
#include "assets/actor_420700_animation_0AF1C_records.inc"
};

static u16 _gActor420700Animation0AF1CIndices[20] = {
#include "assets/actor_420700_animation_0AF1C_indices.inc"
};

static AnimationSet _gActor420700Animation0AF1C = {
    _gActor420700Animation0AF1CRecords,
    _gActor420700Animation0AF1CIndices,
    { NULL, _gActor420700Animation0AF1CBank1, NULL, NULL, _gActor420700Animation0AF1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0B0E8Bank1[2] = {
#include "assets/actor_420700_animation_0B0E8_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0B0E8Bank4[29] = {
#include "assets/actor_420700_animation_0B0E8_bank4.inc"
};

static AnimationRecord _gActor420700Animation0B0E8Records[60] = {
#include "assets/actor_420700_animation_0B0E8_records.inc"
};

static u16 _gActor420700Animation0B0E8Indices[20] = {
#include "assets/actor_420700_animation_0B0E8_indices.inc"
};

static AnimationSet _gActor420700Animation0B0E8 = {
    _gActor420700Animation0B0E8Records,
    _gActor420700Animation0B0E8Indices,
    { NULL, _gActor420700Animation0B0E8Bank1, NULL, NULL, _gActor420700Animation0B0E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0B3B0Bank1[2] = {
#include "assets/actor_420700_animation_0B3B0_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0B3B0Bank4[40] = {
#include "assets/actor_420700_animation_0B3B0_bank4.inc"
};

static AnimationRecord _gActor420700Animation0B3B0Records[112] = {
#include "assets/actor_420700_animation_0B3B0_records.inc"
};

static u16 _gActor420700Animation0B3B0Indices[20] = {
#include "assets/actor_420700_animation_0B3B0_indices.inc"
};

static AnimationSet _gActor420700Animation0B3B0 = {
    _gActor420700Animation0B3B0Records,
    _gActor420700Animation0B3B0Indices,
    { NULL, _gActor420700Animation0B3B0Bank1, NULL, NULL, _gActor420700Animation0B3B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0B57CBank1[2] = {
#include "assets/actor_420700_animation_0B57C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0B57CBank4[29] = {
#include "assets/actor_420700_animation_0B57C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0B57CRecords[60] = {
#include "assets/actor_420700_animation_0B57C_records.inc"
};

static u16 _gActor420700Animation0B57CIndices[20] = {
#include "assets/actor_420700_animation_0B57C_indices.inc"
};

static AnimationSet _gActor420700Animation0B57C = {
    _gActor420700Animation0B57CRecords,
    _gActor420700Animation0B57CIndices,
    { NULL, _gActor420700Animation0B57CBank1, NULL, NULL, _gActor420700Animation0B57CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0B7DCBank1[2] = {
#include "assets/actor_420700_animation_0B7DC_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0B7DCBank4[30] = {
#include "assets/actor_420700_animation_0B7DC_bank4.inc"
};

static AnimationRecord _gActor420700Animation0B7DCRecords[96] = {
#include "assets/actor_420700_animation_0B7DC_records.inc"
};

static u16 _gActor420700Animation0B7DCIndices[20] = {
#include "assets/actor_420700_animation_0B7DC_indices.inc"
};

static AnimationSet _gActor420700Animation0B7DC = {
    _gActor420700Animation0B7DCRecords,
    _gActor420700Animation0B7DCIndices,
    { NULL, _gActor420700Animation0B7DCBank1, NULL, NULL, _gActor420700Animation0B7DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0B9D0Bank1[2] = {
#include "assets/actor_420700_animation_0B9D0_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0B9D0Bank4[34] = {
#include "assets/actor_420700_animation_0B9D0_bank4.inc"
};

static AnimationRecord _gActor420700Animation0B9D0Records[65] = {
#include "assets/actor_420700_animation_0B9D0_records.inc"
};

static u16 _gActor420700Animation0B9D0Indices[20] = {
#include "assets/actor_420700_animation_0B9D0_indices.inc"
};

static AnimationSet _gActor420700Animation0B9D0 = {
    _gActor420700Animation0B9D0Records,
    _gActor420700Animation0B9D0Indices,
    { NULL, _gActor420700Animation0B9D0Bank1, NULL, NULL, _gActor420700Animation0B9D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0BC64Bank1[2] = {
#include "assets/actor_420700_animation_0BC64_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0BC64Bank4[39] = {
#include "assets/actor_420700_animation_0BC64_bank4.inc"
};

static AnimationRecord _gActor420700Animation0BC64Records[100] = {
#include "assets/actor_420700_animation_0BC64_records.inc"
};

static u16 _gActor420700Animation0BC64Indices[20] = {
#include "assets/actor_420700_animation_0BC64_indices.inc"
};

static AnimationSet _gActor420700Animation0BC64 = {
    _gActor420700Animation0BC64Records,
    _gActor420700Animation0BC64Indices,
    { NULL, _gActor420700Animation0BC64Bank1, NULL, NULL, _gActor420700Animation0BC64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0BE58Bank1[2] = {
#include "assets/actor_420700_animation_0BE58_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0BE58Bank4[34] = {
#include "assets/actor_420700_animation_0BE58_bank4.inc"
};

static AnimationRecord _gActor420700Animation0BE58Records[65] = {
#include "assets/actor_420700_animation_0BE58_records.inc"
};

static u16 _gActor420700Animation0BE58Indices[20] = {
#include "assets/actor_420700_animation_0BE58_indices.inc"
};

static AnimationSet _gActor420700Animation0BE58 = {
    _gActor420700Animation0BE58Records,
    _gActor420700Animation0BE58Indices,
    { NULL, _gActor420700Animation0BE58Bank1, NULL, NULL, _gActor420700Animation0BE58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0C150Bank1[2] = {
#include "assets/actor_420700_animation_0C150_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0C150Bank4[60] = {
#include "assets/actor_420700_animation_0C150_bank4.inc"
};

static AnimationRecord _gActor420700Animation0C150Records[104] = {
#include "assets/actor_420700_animation_0C150_records.inc"
};

static u16 _gActor420700Animation0C150Indices[20] = {
#include "assets/actor_420700_animation_0C150_indices.inc"
};

static AnimationSet _gActor420700Animation0C150 = {
    _gActor420700Animation0C150Records,
    _gActor420700Animation0C150Indices,
    { NULL, _gActor420700Animation0C150Bank1, NULL, NULL, _gActor420700Animation0C150Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0C4D0Bank1[4] = {
#include "assets/actor_420700_animation_0C4D0_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0C4D0Bank4[67] = {
#include "assets/actor_420700_animation_0C4D0_bank4.inc"
};

static AnimationRecord _gActor420700Animation0C4D0Records[125] = {
#include "assets/actor_420700_animation_0C4D0_records.inc"
};

static u16 _gActor420700Animation0C4D0Indices[20] = {
#include "assets/actor_420700_animation_0C4D0_indices.inc"
};

static AnimationSet _gActor420700Animation0C4D0 = {
    _gActor420700Animation0C4D0Records,
    _gActor420700Animation0C4D0Indices,
    { NULL, _gActor420700Animation0C4D0Bank1, NULL, NULL, _gActor420700Animation0C4D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0C764Bank1[2] = {
#include "assets/actor_420700_animation_0C764_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0C764Bank4[34] = {
#include "assets/actor_420700_animation_0C764_bank4.inc"
};

static AnimationRecord _gActor420700Animation0C764Records[105] = {
#include "assets/actor_420700_animation_0C764_records.inc"
};

static u16 _gActor420700Animation0C764Indices[20] = {
#include "assets/actor_420700_animation_0C764_indices.inc"
};

static AnimationSet _gActor420700Animation0C764 = {
    _gActor420700Animation0C764Records,
    _gActor420700Animation0C764Indices,
    { NULL, _gActor420700Animation0C764Bank1, NULL, NULL, _gActor420700Animation0C764Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0C9CCBank1[2] = {
#include "assets/actor_420700_animation_0C9CC_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0C9CCBank4[43] = {
#include "assets/actor_420700_animation_0C9CC_bank4.inc"
};

static AnimationRecord _gActor420700Animation0C9CCRecords[85] = {
#include "assets/actor_420700_animation_0C9CC_records.inc"
};

static u16 _gActor420700Animation0C9CCIndices[20] = {
#include "assets/actor_420700_animation_0C9CC_indices.inc"
};

static AnimationSet _gActor420700Animation0C9CC = {
    _gActor420700Animation0C9CCRecords,
    _gActor420700Animation0C9CCIndices,
    { NULL, _gActor420700Animation0C9CCBank1, NULL, NULL, _gActor420700Animation0C9CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0CC08Bank1[2] = {
#include "assets/actor_420700_animation_0CC08_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0CC08Bank4[25] = {
#include "assets/actor_420700_animation_0CC08_bank4.inc"
};

static AnimationRecord _gActor420700Animation0CC08Records[92] = {
#include "assets/actor_420700_animation_0CC08_records.inc"
};

static u16 _gActor420700Animation0CC08Indices[20] = {
#include "assets/actor_420700_animation_0CC08_indices.inc"
};

static AnimationSet _gActor420700Animation0CC08 = {
    _gActor420700Animation0CC08Records,
    _gActor420700Animation0CC08Indices,
    { NULL, _gActor420700Animation0CC08Bank1, NULL, NULL, _gActor420700Animation0CC08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0CF0CBank1[2] = {
#include "assets/actor_420700_animation_0CF0C_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0CF0CBank4[65] = {
#include "assets/actor_420700_animation_0CF0C_bank4.inc"
};

static AnimationRecord _gActor420700Animation0CF0CRecords[102] = {
#include "assets/actor_420700_animation_0CF0C_records.inc"
};

static u16 _gActor420700Animation0CF0CIndices[20] = {
#include "assets/actor_420700_animation_0CF0C_indices.inc"
};

static AnimationSet _gActor420700Animation0CF0C = {
    _gActor420700Animation0CF0CRecords,
    _gActor420700Animation0CF0CIndices,
    { NULL, _gActor420700Animation0CF0CBank1, NULL, NULL, _gActor420700Animation0CF0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor420700Animation0D100Bank1[2] = {
#include "assets/actor_420700_animation_0D100_bank1.inc"
};

static AnimationPackedRotation _gActor420700Animation0D100Bank4[28] = {
#include "assets/actor_420700_animation_0D100_bank4.inc"
};

static AnimationRecord _gActor420700Animation0D100Records[71] = {
#include "assets/actor_420700_animation_0D100_records.inc"
};

static u16 _gActor420700Animation0D100Indices[20] = {
#include "assets/actor_420700_animation_0D100_indices.inc"
};

static AnimationSet _gActor420700Animation0D100 = {
    _gActor420700Animation0D100Records,
    _gActor420700Animation0D100Indices,
    { NULL, _gActor420700Animation0D100Bank1, NULL, NULL, _gActor420700Animation0D100Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_420700_8013EF48[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_420700_80132644 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_420700_801326F4 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_420700_80132784 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_420700_8013EF68[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_420700_80132340, { .model = &_gActor420700GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_420700_801323D8, { .model = &_gActor420700GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_420700_801327EC, { .model = &_gActor420700GaryDouglasShotgun } },
};

u8 D_actor_420700_8013EF8C[84] = {
    0,
    0,
    0,
    0,
    132,
    194,
    19,
    128,
    48,
    196,
    19,
    128,
    220,
    197,
    19,
    128,
    140,
    199,
    19,
    128,
    132,
    203,
    19,
    128,
    60,
    205,
    19,
    128,
    8,
    207,
    19,
    128,
    208,
    209,
    19,
    128,
    156,
    211,
    19,
    128,
    252,
    213,
    19,
    128,
    240,
    215,
    19,
    128,
    132,
    218,
    19,
    128,
    120,
    220,
    19,
    128,
    112,
    223,
    19,
    128,
    240,
    226,
    19,
    128,
    132,
    229,
    19,
    128,
    236,
    231,
    19,
    128,
    40,
    234,
    19,
    128,
    44,
    237,
    19,
    128,
    32,
    239,
    19,
    128,
};

_Actor420700Work* gScriptedWalkWork;

Task* D_actor_420700_8013EFE4;

Task* D_actor_420700_8013EFE8;

Task* D_actor_420700_8013EFEC;

s32 D_actor_420700_8013EFF0;

s32 D_actor_420700_8013EFF4;

static void func_actor_420700_80131E24(Enemy* enemy, Task* task);
static void func_actor_420700_80132064(Enemy* enemy, Task* task);

/// Step 0 of the `func_actor_420700_80132340` dispatcher: allocate and publish the
/// work block, spawn the two model tasks, texture the first from the placement
/// the actor was spawned from, then seed the model's matrices and animation
/// context before running the first step body.
static void func_actor_420700_80131E24(Enemy* enemy, Task* task)
{
    VECTOR            vec;
    GfxCoord*         coord;
    TmdObject*        obj;
    _Actor420700Work* work;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(sizeof(_Actor420700Work), 0);
    gScriptedWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_420700_8013239C;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    D_actor_420700_8013EFE4          = task;
    D_actor_420700_8013EFE8          = Task_SpawnFromTable(D_actor_420700_8013EF68, 1, 0, 0);
    D_actor_420700_8013EFEC          = Task_SpawnFromTable(D_actor_420700_8013EF68, 2, 0, 0);
    actorTintTask(D_actor_420700_8013EFE8, enemy);
    obj->lightMtx           = &gScriptedWalkWork->light;
    obj->colorMtx           = &gScriptedWalkWork->color;
    D_actor_420700_8013EFF0 = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_420700_8013EFF4 = 0x96;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_420700_8013EF8C, obj,
                         gScriptedWalkWork->rig.poses, gScriptedWalkWork->rig.slots);
    gScriptedWalkWork->st.animId = 5;
    gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable               = D_actor_420700_8013EF48;
    func_actor_420700_80132478(task);
    task->state++;
}

/// Step 1 of the `func_actor_420700_80132340` dispatcher: refresh the model's third
/// coordinate and colour the actor from its world translation, run
/// `func_actor_420700_80132478`, then step the `st.turnWeight` ramp by the mode in
/// `st.turnMode` and pass it as the weight of `func_800B0928` aimed at the
/// slot-3 task (modes 0, 1 and 2) or of `func_800B0CF4` aimed at a fixed
/// world point (mode 3).
///
/// Mode 0 chooses its own step each frame: +0x40 while the actor lies behind
/// the slot-3 actor's `field_52` heading, -0x80 otherwise or while an event
/// is running. In that mode the animation slots after the first are held
/// (rate 0) once the ramp is off zero; otherwise they run at one frame per
/// tick.
static void func_actor_420700_80132064(Enemy* enemy, Task* task)
{
    VECTOR     pos;
    GfxCoord   target[2];
    GfxCoord*  coords;
    GfxCoord*  player;
    GfxCoord*  part;
    GameActor* actor;
    s32        dx;
    s32        dz;
    s32        c;
    s32        i;
    u8         rate;

    coords = task->extra.tmd->coords;
    part   = &coords[2];
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    Gp_UpdateCoord(part);
    pos.vx = part->workm.t[0];
    pos.vy = part->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    func_actor_420700_80132478(task);
    rate = 0x10;
    if (gScriptedWalkWork->st.turnMode != ACTOR_420700_TURN_AUTO) {
        if (gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_PLAYER ||
            gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            gScriptedWalkWork->st.turnWeight += 0x80;
            if (gScriptedWalkWork->st.turnWeight > ONE) {
                gScriptedWalkWork->st.turnWeight = ONE;
            }
        } else {
            gScriptedWalkWork->st.turnWeight -= 0x80;
            if (gScriptedWalkWork->st.turnWeight < 0) {
                gScriptedWalkWork->st.turnWeight = 0;
            }
        }
        if (gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            target[0].coord.t[0] = 0x1173;
            target[0].coord.t[1] = 0;
            target[0].coord.t[2] = -0x733;
            func_800B0CF4(task, target, 0x200, 0x100, gScriptedWalkWork->st.turnWeight);
        } else {
            func_800B0928(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, gScriptedWalkWork->st.turnWeight);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx    = coords->coord.t[0] - player->coord.t[0];
            dz    = coords->coord.t[2] - player->coord.t[2];
            actor = (GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            c     = rcos(actor->rotation.vy);
            if (dx * rsin(actor->rotation.vy) + dz * c < 0) {
                D_actor_420700_8013EFF0 = 0x40;
            } else {
                D_actor_420700_8013EFF0 = -0x80;
            }
            if (gScriptedWalkWork->st.turnWeight != 0) {
                rate = 0;
            }
        } else {
            D_actor_420700_8013EFF0 = -0x80;
        }
        gScriptedWalkWork->st.turnWeight += D_actor_420700_8013EFF0;
        if (gScriptedWalkWork->st.turnWeight > ONE) {
            gScriptedWalkWork->st.turnWeight = ONE;
        }
        if (gScriptedWalkWork->st.turnWeight < 0) {
            gScriptedWalkWork->st.turnWeight = 0;
        }
        func_800B0928(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, gScriptedWalkWork->st.turnWeight);
    }
    for (i = 1; i < 0x14; i++) {
        gScriptedWalkWork->rig.slots[i].rate = rate;
    }
}

/// Task handler of the actor: republishes the task's work block in
/// `gScriptedWalkWork`, so the rest of the overlay can reach it without
/// the task, then runs the handler for the task's state from a two-entry table
/// built on the stack -- the spawn step `func_actor_420700_80131E24` or the
/// per-frame step `func_actor_420700_80132064`.
void func_actor_420700_80132340(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_420700_80131E24,
        func_actor_420700_80132064,
    };

    gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Exit callback of the actor's task: kills the frame-4 model task and
/// destroys the enemy.
static void func_actor_420700_8013239C(Task* arg0)
{
    taskKill(D_actor_420700_8013EFE8);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

/// State handler of the frame-4 model task: the spawn tick clears its root
/// coordinate's `composeStamp` and the model's flags, which leaves it visible, and hangs
/// the root off frame 4 of the actor's own model, stepping to state 1; every
/// later tick hands the actor model's root translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the model's colour matrix.
void func_actor_420700_801323D8(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_420700_8013EFE4->extra.tmd->coords;
    GfxCoord*  part  = parts + 4;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            coord->parent       = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
static void func_actor_420700_80132478(Task* task)
{
    if (gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        func_actor_420700_801325C8();
        gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        scriptedWalkResetAnim();
        gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        scriptedWalkTickAnim();
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
static void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        animationSeekSlotWithBlend(&gScriptedWalkWork->rig.anim, i, gScriptedWalkWork->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    gScriptedWalkWork->st.appliedAnimId = gScriptedWalkWork->st.animId;
}

/// Starts the requested local clip, translating its bank selector to a clip offset.
///
/// Selectors 1 and 2 add 10 and 17 respectively; other selectors add zero.
/// Rejects clip ids 21 and above. A nonzero blend request selects an
/// eight-frame transition; the requested duration is unused.
s32 func_actor_420700_80132644(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    s32               offset;
    _Actor420700Work* work;

    if (args->animationId < 0x15) {
        switch (args->source.index) {
            case 1:
                offset = 0xA;
                break;
            case 2:
                offset = 0x11;
                break;
            default:
                offset = 0;
                break;
        }
        work            = gScriptedWalkWork;
        work->st.animId = (u16)args->animationId + offset;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = ACTOR_ENEMY_ANIM_BLEND;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        gScriptedWalkWork->st.field_A = 0;
        func_actor_420700_80132478(D_actor_420700_8013EFE4);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: rewrites the flags of the actor's three models -- its
/// own and those of the frame-4 and frame-8 model tasks. Bit 0 of the argument
/// shows all three (flags 0) when set and hides them (0x80) when clear; bit 1
/// then ORs 0x4 into all three. Always returns 0.
///
/// The argument is the handler table's third slot, not the second, so the three
/// objects it loads land in `$a3` / `$a0` / `$v1` rather than shifted one down.
s32 func_actor_420700_801326F4(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* actor = D_actor_420700_8013EFE4->extra.tmd;
    TmdObject* model = D_actor_420700_8013EFE8->extra.tmd;
    TmdObject* twin  = D_actor_420700_8013EFEC->extra.tmd;

    if (arg2 & 1) {
        actor->flags = 0;
        model->flags = 0;
        twin->flags  = 0;
    } else {
        actor->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        twin->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        actor->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        twin->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7DB handler: records the `st.turnMode` mode the ramp
/// `func_actor_420700_80132064` runs and seeds `st.turnWeight` at the end that mode
/// walks away from -- 0 for the rising modes 1 and 3, `ONE` for the falling
/// mode 2. Mode 0 is accepted as a no-op, and a block whose leading id is not
/// 0x1B02 is rejected with -1 without touching the work block.
///
/// The empty `case 0` is what the decision tree is built from: with the three
/// live cases alone GCC balances the list at the middle node and comes out one
/// test short, and adding the fourth node is what makes it split at the first
/// case instead. See DECOMPILATION_LEARNINGS.md, "An empty case node changes
/// the switch decision tree".
s32 func_actor_420700_80132784(Task* task, s32 arg1, ActorCommand* args, s32 arg3)
{
    if (args->context.key != 0x1B02) {
        return -1;
    }
    gScriptedWalkWork->st.turnMode = args->command;
    switch (args->command) {
        case ACTOR_420700_TURN_AUTO:
            break;
        case ACTOR_420700_TURN_PLAYER:
        case ACTOR_420700_TURN_POINT:
            gScriptedWalkWork->st.turnWeight = 0;
            break;
        case ACTOR_420700_TURN_RELEASE:
            gScriptedWalkWork->st.turnWeight = ONE;
            break;
    }
    return 0;
}

/// State handler of the frame-8 model task: the same as the frame-4 one,
/// `func_actor_420700_801323D8`, except that it hangs its root off frame 8 of
/// the actor's own model and its spawn tick also sets the model's `otOffset` to
/// -2.
void func_actor_420700_801327EC(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_420700_8013EFE4->extra.tmd->coords;
    GfxCoord*  part  = parts + 8;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            extra->otOffset     = -2;
            coord->parent       = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}
