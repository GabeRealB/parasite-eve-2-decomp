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

#include "main/areas.h"
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
/// spawn step and kept both at `Task::work` and in `_gScriptedWalkWork`, which
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

/// Borrowed work block used by this actor's scripted-walk animation fragments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The included fragments require it to remain live; task teardown releases
/// it without clearing this pointer. The carrier's remaining state drives its
/// head turn; it does not implement the scripted walk's movement.
static _Actor420700Work* _gScriptedWalkWork;

/// The actor's own task, the `task` the state-0 handler
/// `_actor420700Spawn` is entered with. Its `Task::extra` holds the
/// `TmdObject` whose trailing coordinate array `_actor420700HeadHatTask`
/// hangs the model task's own root off, at frame 4.
extern Task* D_actor_420700_8013EFE4;

/// The first task the state-0 handler spawns, the frame-4 model task
/// `_actor420700HeadHatTask`; the actor's exit callback kills it.
extern Task* D_actor_420700_8013EFE8;

/// The second task the state-0 handler spawns, the frame-8 model task
/// `_actor420700ShotgunTask`.
extern Task* D_actor_420700_8013EFEC;

static void _actor420700Exit(Task* task);
static void _actor420700UpdateAnimation(Task* task);
static void _actor420700BlendAnimation(void);

enum {
    ACTOR_420700_ATTACHMENT_INIT     = 0,
    ACTOR_420700_ATTACHMENT_LIGHTING = 1,
    ACTOR_420700_LIGHTING_Y_OFFSET   = -800,
};

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_420700_8013EF48[4];
extern TaskDesc         D_actor_420700_8013EF68[];
extern u8               D_actor_420700_8013EF8C[];
extern s32              D_actor_420700_8013EFF0;
extern s32              D_actor_420700_8013EFF4;

static TmdSource _gActor420700GaryDouglasBody;
static TmdSource _gActor420700GaryDouglasHeadHat;
static TmdSource _gActor420700GaryDouglasShotgun;
static void      _actor420700GaryDouglasTask(Task* task);
static void      _actor420700HeadHatTask(Task* task);
static void      _actor420700ShotgunTask(Task* task);

static s32 _actor420700HandleAnimationMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32 _actor420700HandleModelDrawMessage(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg);
static s32 _actor420700HandleHeadTurnCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Advances automatic head-turn weight, narrowing to its stored halfword before clamping.
///
/// Requires the published live work block. The signed step is in 1/4096 units;
/// automatic mode supplies +64 or -128 and keeps the result between 0 and `ONE`.
static inline void _actor420700AdvanceAutoHeadTurn(void)
{
    _gScriptedWalkWork->st.turnWeight += D_actor_420700_8013EFF0;
    if (_gScriptedWalkWork->st.turnWeight > ONE) {
        _gScriptedWalkWork->st.turnWeight = ONE;
    }
    if (_gScriptedWalkWork->st.turnWeight < 0) {
        _gScriptedWalkWork->st.turnWeight = 0;
    }
}

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
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor420700HandleAnimationMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor420700HandleModelDrawMessage },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor420700HandleHeadTurnCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_420700_8013EF68[3] = {
    { { { TASK_BODY_TMD, 192 } }, _actor420700GaryDouglasTask, { .model = &_gActor420700GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor420700HeadHatTask, { .model = &_gActor420700GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, _actor420700ShotgunTask, { .model = &_gActor420700GaryDouglasShotgun } },
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

Task* D_actor_420700_8013EFE4;

Task* D_actor_420700_8013EFE8;

Task* D_actor_420700_8013EFEC;

s32 D_actor_420700_8013EFF0;

s32 D_actor_420700_8013EFF4;

static void _actor420700Spawn(Enemy* enemy, Task* task);
static void _actor420700Update(Enemy* enemy, Task* task);

/// Initializes Gary Douglas's singleton body, head-and-hat and shotgun tasks.
///
/// Requires a live enemy/body model and current area placement. Publishes the
/// zeroed work allocation; allocation failure destroys the enemy task. On
/// success installs teardown, view-parents the body and disables targeting,
/// then spawns both attachment tasks. The first child must exist for texture
/// binding; no failure guard is present. Binds borrowed matrices and the loaded
/// 21-entry animation bank, samples three lights at cached root view-space XYZ
/// with Y reduced by 800, starts clip 5, installs messages and advances state.
static void _actor420700Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_420700_SPAWN_HEAD_HAT_TASK = 1,
        ACTOR_420700_SPAWN_SHOTGUN_TASK  = 2,
        ACTOR_420700_SPAWN_IDLE_CLIP     = 5,
        ACTOR_420700_SPAWN_OT_OFFSET     = 1,
        ACTOR_420700_SPAWN_LIGHT_COUNT   = 3,
    };

    VECTOR            lightingPosition;
    GfxCoord*         rootCoord;
    TmdObject*        bodyModel;
    _Actor420700Work* work;

    bodyModel          = task->extra.tmd;
    rootCoord          = bodyModel->coords;
    work               = memCalloc(sizeof(_Actor420700Work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor420700Exit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    bodyModel->otOffset              = ACTOR_420700_SPAWN_OT_OFFSET;
    bodyModel->flags                 = 0;
    // The head-and-hat must spawn successfully before its texture binding.
    D_actor_420700_8013EFE4 = task;
    D_actor_420700_8013EFE8 = taskSpawnFromTable(D_actor_420700_8013EF68, ACTOR_420700_SPAWN_HEAD_HAT_TASK, 0, NULL);
    D_actor_420700_8013EFEC = taskSpawnFromTable(D_actor_420700_8013EF68, ACTOR_420700_SPAWN_SHOTGUN_TASK, 0, NULL);
    _actorRenderApplyTaskPlacementTextureOffsets(D_actor_420700_8013EFE8, enemy);
    bodyModel->lightMtx     = &_gScriptedWalkWork->light;
    bodyModel->colorMtx     = &_gScriptedWalkWork->color;
    D_actor_420700_8013EFF0 = 0;
    lightingPosition.vx     = rootCoord->workm.t[0];
    lightingPosition.vy     = rootCoord->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
    D_actor_420700_8013EFF4 = 150; // No reader is recovered; the stored value's role is unproven.
    lightingPosition.vz     = rootCoord->workm.t[2];
    worldCoordSetModelLighting(bodyModel, &lightingPosition, 0, ACTOR_420700_SPAWN_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_420700_8013EF8C, bodyModel,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_420700_SPAWN_IDLE_CLIP;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                = D_actor_420700_8013EF48;
    _actor420700UpdateAnimation(task);
    task->state++;
}

/// Updates Gary Douglas's lighting, animation and head turn once per actor tick.
///
/// Requires the published live work block, twenty-part body and live player model.
/// Head-turn weights use 1/4096 units and angles use 4096 units per turn.
/// `ACTOR_420700_TURN_AUTO` raises the weight while the player faces away,
/// releases it otherwise or during events, and holds slots 1..19 when the
/// pre-step weight is nonzero outside events. Explicit player/point modes raise
/// the weight; release and unrecognized modes lower it. Only point mode aims
/// at the fixed room focus; all other modes aim at the player.
static void _actor420700Update(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_420700_LIGHTING_PART  = 2,
        ACTOR_420700_HEAD_MAX_YAW   = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_420700_HEAD_MAX_PITCH = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_420700_HEAD_TURN_RISE = ONE / 64,
        ACTOR_420700_HEAD_TURN_STEP = ONE / 32,
        ACTOR_420700_ROOM_FOCUS_X   = 4467,
        ACTOR_420700_ROOM_FOCUS_Y   = 0,
        ACTOR_420700_ROOM_FOCUS_Z   = -1843,
    };
    VECTOR     lightingPosition;
    GfxCoord   roomFocus;    // Only the translation carries a world point for the head-aim API
    byte       unused[0x50]; // Untouched frame storage; its original role is unproven
    GfxCoord*  bodyCoords;
    GfxCoord*  playerCoords;
    GfxCoord*  lightingPart;
    GameActor* playerActor;
    s32        dx;
    s32        dz;
    s32        playerCosYaw;
    s32        slotIndex;
    u8         animationRate;

    bodyCoords   = task->extra.tmd->coords;
    lightingPart = &bodyCoords[ACTOR_420700_LIGHTING_PART];
    playerCoords = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(lightingPart);
    lightingPosition.vx = lightingPart->workm.t[0];
    lightingPosition.vy = lightingPart->workm.t[1];
    lightingPosition.vz = lightingPart->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    _actor420700UpdateAnimation(task);
    animationRate = ANIMATION_RATE_ONE;
    // The head adjustment follows the clip tick, and its weight persists between ticks.
    if (_gScriptedWalkWork->st.turnMode != ACTOR_420700_TURN_AUTO) {
        if (_gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_PLAYER ||
            _gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            _gScriptedWalkWork->st.turnWeight += ACTOR_420700_HEAD_TURN_STEP;
            if (_gScriptedWalkWork->st.turnWeight > ONE) {
                _gScriptedWalkWork->st.turnWeight = ONE;
            }
        } else {
            _gScriptedWalkWork->st.turnWeight -= ACTOR_420700_HEAD_TURN_STEP;
            if (_gScriptedWalkWork->st.turnWeight < 0) {
                _gScriptedWalkWork->st.turnWeight = 0;
            }
        }
        if (_gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            roomFocus.coord.t[0] = ACTOR_420700_ROOM_FOCUS_X;
            roomFocus.coord.t[1] = ACTOR_420700_ROOM_FOCUS_Y;
            roomFocus.coord.t[2] = ACTOR_420700_ROOM_FOCUS_Z;
            animationAimHeadAtPoint(task, &roomFocus, ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
        } else {
            animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx           = bodyCoords->coord.t[0] - playerCoords->coord.t[0];
            dz           = bodyCoords->coord.t[2] - playerCoords->coord.t[2];
            playerActor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            playerCosYaw = rcos(playerActor->rotation.vy);
            // A negative forward projection places Gary behind the player's facing.
            if (dx * rsin(playerActor->rotation.vy) + dz * playerCosYaw < 0) {
                D_actor_420700_8013EFF0 = ACTOR_420700_HEAD_TURN_RISE;
            } else {
                D_actor_420700_8013EFF0 = -ACTOR_420700_HEAD_TURN_STEP;
            }
            if (_gScriptedWalkWork->st.turnWeight != 0) {
                animationRate = 0;
            }
        } else {
            D_actor_420700_8013EFF0 = -ACTOR_420700_HEAD_TURN_STEP;
        }
        _actor420700AdvanceAutoHeadTurn();
        animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(_gScriptedWalkWork->rig.slots); slotIndex++) {
        _gScriptedWalkWork->rig.slots[slotIndex].rate = animationRate;
    }
}

/// Dispatches Gary Douglas's spawn or per-frame update and publishes his work.
///
/// Task state must be 0 (spawn) or 1 (update), with a live TMD body and enemy
/// spawn argument. Publishes the current task work before dispatch; the spawn
/// handler replaces it after allocation. The published block is borrowed until
/// actor teardown. The package's body descriptor is this entry's only consumer.
static void _actor420700GaryDouglasTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _actor420700Spawn,
        _actor420700Update,
    };

    _gScriptedWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Releases the head-and-hat attachment, enemy record and body task at actor exit.
///
/// The head-and-hat task must still be live. The shotgun task is not explicitly
/// killed here; both task globals and the borrowed work pointer are left intact.
static void _actor420700Exit(Task* task)
{
    taskKill(D_actor_420700_8013EFE8);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Attaches Gary's head-and-hat model to body part 4 and refreshes its lighting.
///
/// The body task and its coordinates must outlive this task. Initialization
/// enables drawing and enters the lighting state; later ticks sample the body
/// root's composed translation with a -800 world-unit Y offset. Other states
/// do nothing. Lighting uses the model's borrowed default matrices.
static void _actor420700HeadHatTask(Task* task)
{
    enum { ACTOR_420700_HEAD_ATTACHMENT_PART = 4 };
    TmdObject* headModel  = task->extra.tmd;
    GfxCoord*  headRoot   = headModel->coords;
    GfxCoord*  bodyCoords = D_actor_420700_8013EFE4->extra.tmd->coords;
    GfxCoord*  headPart   = bodyCoords + ACTOR_420700_HEAD_ATTACHMENT_PART;
    VECTOR     lightingPosition;

    switch (task->state) {
        case ACTOR_420700_ATTACHMENT_INIT:
            headRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            headModel->flags       = 0;
            headRoot->parent       = headPart;
            task->state++;
            break;
        case ACTOR_420700_ATTACHMENT_LIGHTING:
            lightingPosition.vx = bodyCoords->workm.t[0];
            lightingPosition.vy = bodyCoords->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
            lightingPosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(headModel, &lightingPosition, 0, ARRAY_SIZE(headModel->colorMtx->m[0]));
            break;
    }
}

/// Applies a pending animation reset/blend or ticks the actor's playing slots.
///
/// Uses the published work block; `task` is unused. Reset and blend requests
/// enter `ACTOR_ENEMY_ANIM_TICK` without a further playback tick. Other states
/// leave playback unchanged. The body, rig and selected clip must remain live.
static void _actor420700UpdateAnimation(Task* task)
{
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _actor420700BlendAnimation();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _scriptedWalkResetAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _scriptedWalkTickAnim();
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

/// Blends slots 1..19 from their current poses to the requested clip over eight frames.
///
/// The published rig must be bound to a live body and a non-null loaded set
/// at `st.animId`, with tracks for all driven slots. Captures each slot's ticked
/// pose and seeks its existing track to record offset zero, retaining its rate.
/// Leaves slot 0 and the animation state unchanged, and records `appliedAnimId`.
static void _actor420700BlendAnimation(void)
{
    enum { ACTOR_420700_ANIMATION_BLEND_FRAMES = 8 };
    s32 slotIndex;

    slotIndex = 1;
    do {
        animationSeekSlotWithBlend(&_gScriptedWalkWork->rig.anim, slotIndex, _gScriptedWalkWork->st.animId, 0, ACTOR_420700_ANIMATION_BLEND_FRAMES);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(_gScriptedWalkWork->rig.slots));
    _gScriptedWalkWork->st.appliedAnimId = _gScriptedWalkWork->st.animId;
}

/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` by immediately resetting or blending the body clip.
///
/// Borrows `request` through dispatch; the published work and body must be live.
/// Selector 1 adds 10, selector 2 adds 17, and every other selector adds zero.
/// Valid ids for non-null local sets are 0..10 for selector 1, 0..3 for selector
/// 2, and 1..20 otherwise. Only the signed raw id < 21 is checked; negative ids
/// and offset sums are not validated. Returns -1 for raw ids >= 21, else 0.
/// Nonzero `blend` uses eight frames regardless of `blendFrames`; collision
/// participation, `task`, `messageId` and the second payload are unused.
static s32 _actor420700HandleAnimationMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        ACTOR_420700_ANIMATION_REQUEST_LIMIT = 21,
        ACTOR_420700_ANIMATION_BANK_1        = 1,
        ACTOR_420700_ANIMATION_BANK_2        = 2,
        ACTOR_420700_ANIMATION_BANK_1_BASE   = 10,
        ACTOR_420700_ANIMATION_BANK_2_BASE   = 17,
    };
    s32               setOffset;
    _Actor420700Work* work;

    if (request->animationId < ACTOR_420700_ANIMATION_REQUEST_LIMIT) {
        switch (request->source.index) {
            case ACTOR_420700_ANIMATION_BANK_1:
                setOffset = ACTOR_420700_ANIMATION_BANK_1_BASE;
                break;
            case ACTOR_420700_ANIMATION_BANK_2:
                setOffset = ACTOR_420700_ANIMATION_BANK_2_BASE;
                break;
            default:
                setOffset = 0;
                break;
        }
        work            = _gScriptedWalkWork;
        work->st.animId = request->animationId + setOffset;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state = ACTOR_ENEMY_ANIM_BLEND;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_A = 0;
        _actor420700UpdateAnimation(D_actor_420700_8013EFE4);
        return 0;
    }
    return -1;
}

/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for the body, head-and-hat and shotgun models.
///
/// All three tasks/models must be live. Bit 0 of `drawFlags` enables active
/// drawing; bit 1 disables automatic buffer allocation. Replaces all prior
/// flags, ignores other input bits and allocates/releases no buffers. Always
/// returns 0; the receiver, message id and second payload are unused.
static s32 _actor420700HandleModelDrawMessage(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg)
{
    enum {
        ACTOR_420700_DRAW_VISIBLE        = 1 << 0,
        ACTOR_420700_DRAW_NO_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* bodyModel    = D_actor_420700_8013EFE4->extra.tmd;
    TmdObject* headHatModel = D_actor_420700_8013EFE8->extra.tmd;
    TmdObject* shotgunModel = D_actor_420700_8013EFEC->extra.tmd;

    if (drawFlags & ACTOR_420700_DRAW_VISIBLE) {
        bodyModel->flags    = 0;
        headHatModel->flags = 0;
        shotgunModel->flags = 0;
    } else {
        bodyModel->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        headHatModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        shotgunModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_420700_DRAW_NO_AUTO_BUFFER) {
        bodyModel->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        headHatModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        shotgunModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Applies a trailer-coach head-turn command delivered by `ACTOR_COMMAND_MESSAGE_APPLY`.
///
/// Borrows `command` through dispatch. Only stage 2/area 27 commands are accepted;
/// a different namespace returns -1 without touching work. Player/point modes
/// start at weight 0, release starts at `ONE`, and auto keeps the current weight.
/// Stores other commands too, leaving their weight unchanged; the update treats
/// them as release. Returns 0 on acceptance. Requires the published live work;
/// the receiver, message id and second payload are unused.
static s32 _actor420700HandleHeadTurnCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum { ACTOR_420700_HEAD_TURN_COMMAND_CONTEXT = (GAME_AREA_DRYFIELD_TRAILER_COACH << 8) | GAME_STAGE_DRYFIELD };

    if (command->context.key != ACTOR_420700_HEAD_TURN_COMMAND_CONTEXT) {
        return -1;
    }
    _gScriptedWalkWork->st.turnMode = command->command;
    switch (command->command) {
        case ACTOR_420700_TURN_AUTO:
            break;
        case ACTOR_420700_TURN_PLAYER:
        case ACTOR_420700_TURN_POINT:
            _gScriptedWalkWork->st.turnWeight = 0;
            break;
        case ACTOR_420700_TURN_RELEASE:
            _gScriptedWalkWork->st.turnWeight = ONE;
            break;
    }
    return 0;
}

/// Attaches Gary's shotgun to body part 8 and refreshes its lighting.
///
/// The body task and its coordinates must outlive this task. Initialization
/// enables drawing, gives the model an ordering-table bias of -2 and enters
/// the lighting state. Later ticks sample the body root's composed translation
/// with a -800 world-unit Y offset. Other states do nothing. Lighting uses
/// the model's borrowed default matrices.
static void _actor420700ShotgunTask(Task* task)
{
    enum {
        ACTOR_420700_SHOTGUN_ATTACHMENT_PART = 8,
        ACTOR_420700_SHOTGUN_OT_BIAS         = -2,
    };
    TmdObject* shotgunModel   = task->extra.tmd;
    GfxCoord*  shotgunRoot    = shotgunModel->coords;
    GfxCoord*  bodyCoords     = D_actor_420700_8013EFE4->extra.tmd->coords;
    GfxCoord*  attachmentPart = bodyCoords + ACTOR_420700_SHOTGUN_ATTACHMENT_PART;
    VECTOR     lightingPosition;

    switch (task->state) {
        case ACTOR_420700_ATTACHMENT_INIT:
            shotgunRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            shotgunModel->flags       = 0;
            shotgunModel->otOffset    = ACTOR_420700_SHOTGUN_OT_BIAS;
            shotgunRoot->parent       = attachmentPart;
            task->state++;
            break;
        case ACTOR_420700_ATTACHMENT_LIGHTING:
            lightingPosition.vx = bodyCoords->workm.t[0];
            lightingPosition.vy = bodyCoords->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
            lightingPosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(shotgunModel, &lightingPosition, 0, ARRAY_SIZE(shotgunModel->colorMtx->m[0]));
            break;
    }
}
