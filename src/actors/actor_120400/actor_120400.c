#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_motion_play_helpers.h"
#include "../../shared/actor_messages.h"

/// Work block of Kyle Madigan's body, the package's scripted walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotionWalkWork` does - the
/// twenty-part rig, the model state and the walk a room script sends the
/// actor on - and the model object borrows `model.light` and `model.color`
/// for as long as the block lives.
///
/// What follows `walk` is the package's own: the delayed free of the model's
/// buffers once the model has been hidden. The tasks of the two hand models
/// are not kept here: each finds the body through its own spawn argument.
typedef struct {
    ActorAnimRig20  rig;            // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;          // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;           // Destination, closing rotation, per-frame velocity and step of the walk in progress
    byte            field_4FC[0x4]; // Allocated but never accessed; role unproven
    s16             freeCountdown;  // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor120400KyleMadiganWork;
STATIC_ASSERT_SIZEOF(_Actor120400KyleMadiganWork, 0x504);

/// Animation source indexed by the bank id the presets latch:
/// `gActorMotionAnimBanks[work->model.bank]` is the bank handed to
/// `animationInitContext`.
extern AnimationSet*  D_actor_120400_8013E6D0[29];
extern AnimationSet** gActorMotionAnimBanks[1];

/// The task table the parent is spawned from and its two children are spawned
/// from (entries 1 and 2), and the message table the parent points its
/// `Task::msgTable` at; both live in this overlay's trailing data.
extern TaskDesc D_actor_120400_8013E748[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_120400_8013E76C[];

static void _actor120400SpawnKyleMadiganWalker(Task* task);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor120400UpdateKyleMadiganWalker(Task* task);
static void _actor120400ExitKyleMadiganWalker(Task* task);
static void _actor120400BindKyleMadiganWalkerLighting(Task* task);
static void _actor120400IdleKyleMadiganWalk(Task* task);
static void _actor120400RunKyleMadiganWalkStep(Task* task);
static void _actor120400BeginKyleMadiganWalk(Task* task);

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `_actor120400KyleMadiganHandTask`.
static const TaskFuncTable3 D_actor_120400_80131E24 = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent task, dispatched by
/// `_actor120400KyleMadiganWalkerTask`.
static const TaskFuncTable3 D_actor_120400_80131E30 = { {
    _actor120400SpawnKyleMadiganWalker,
    _actor120400UpdateKyleMadiganWalker,
    _actor120400ExitKyleMadiganWalker,
} };

/// Steps of the parent's walk sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_120400_80131E3C = { {
    _actorMotionFaceTarget,
    _actor120400BeginKyleMadiganWalk,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
static const VECTOR D_actor_120400_80131E4C = { 0, 0, 0x200000, 0 };

static TmdSource _gActor120400KyleMadiganBody;
static TmdSource _gActor120400KyleMadiganHandRight;
static TmdSource _gActor120400KyleMadiganLeft;
static s32       _actor120400StartKyleMadiganWalk(Task* task, s32 msgId, const ActorTransform* placement,
                                                  const ActorMotionWalkAnim* walkAnim);
static s32       _actor120400SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32       _actor120400IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void      _actor120400KyleMadiganHandTask(Task* handTask);
static void      _actor120400KyleMadiganWalkerTask(Task* task);

static TmdBone _gActor120400KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor120400KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor120400KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor120400KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor120400KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor120400KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor120400KyleMadiganBodyPartVerts,
    _gActor120400KyleMadiganBodyVerts,
    _gActor120400KyleMadiganBodyNormals,
    _gActor120400KyleMadiganBodySkeleton,
    _gActor120400KyleMadiganBodyStream,
};

static TmdBone _gActor120400KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor120400KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor120400KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor120400KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor120400KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor120400KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor120400KyleMadiganHandRightPartVerts,
    _gActor120400KyleMadiganHandRightVerts,
    _gActor120400KyleMadiganHandRightNormals,
    _gActor120400KyleMadiganHandRightSkeleton,
    _gActor120400KyleMadiganHandRightStream,
};

static TmdBone _gActor120400KyleMadiganLeftSkeleton[1] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

static u32 _gActor120400KyleMadiganLeftPartVerts[1] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

static SVECTOR _gActor120400KyleMadiganLeftVerts[23] = {
#include "assets/kyle_madigan_left_verts.inc"
};

static SVECTOR _gActor120400KyleMadiganLeftNormals[23] = {
#include "assets/kyle_madigan_left_normals.inc"
};

static u32 _gActor120400KyleMadiganLeftStream[166] = {
#include "assets/kyle_madigan_left_stream.inc"
};

static TmdSource _gActor120400KyleMadiganLeft = {
    0,
    1148,
    0,
    1,
    _gActor120400KyleMadiganLeftPartVerts,
    _gActor120400KyleMadiganLeftVerts,
    _gActor120400KyleMadiganLeftNormals,
    _gActor120400KyleMadiganLeftSkeleton,
    _gActor120400KyleMadiganLeftStream,
};

static AnimationPackedPose _gActor120400Animation0653CBank1[2] = {
#include "assets/actor_120400_animation_0653C_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0653CBank4[32] = {
#include "assets/actor_120400_animation_0653C_bank4.inc"
};

static AnimationRecord _gActor120400Animation0653CRecords[101] = {
#include "assets/actor_120400_animation_0653C_records.inc"
};

static u16 _gActor120400Animation0653CIndices[20] = {
#include "assets/actor_120400_animation_0653C_indices.inc"
};

static AnimationSet _gActor120400Animation0653C = {
    _gActor120400Animation0653CRecords,
    _gActor120400Animation0653CIndices,
    { NULL, _gActor120400Animation0653CBank1, NULL, NULL, _gActor120400Animation0653CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation06CD0Bank1[21] = {
#include "assets/actor_120400_animation_06CD0_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation06CD0Bank4[156] = {
#include "assets/actor_120400_animation_06CD0_bank4.inc"
};

static AnimationRecord _gActor120400Animation06CD0Records[246] = {
#include "assets/actor_120400_animation_06CD0_records.inc"
};

static u16 _gActor120400Animation06CD0Indices[20] = {
#include "assets/actor_120400_animation_06CD0_indices.inc"
};

static AnimationSet _gActor120400Animation06CD0 = {
    _gActor120400Animation06CD0Records,
    _gActor120400Animation06CD0Indices,
    { NULL, _gActor120400Animation06CD0Bank1, NULL, NULL, _gActor120400Animation06CD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation072B4Bank1[2] = {
#include "assets/actor_120400_animation_072B4_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation072B4Bank4[152] = {
#include "assets/actor_120400_animation_072B4_bank4.inc"
};

static AnimationRecord _gActor120400Animation072B4Records[199] = {
#include "assets/actor_120400_animation_072B4_records.inc"
};

static u16 _gActor120400Animation072B4Indices[20] = {
#include "assets/actor_120400_animation_072B4_indices.inc"
};

static AnimationSet _gActor120400Animation072B4 = {
    _gActor120400Animation072B4Records,
    _gActor120400Animation072B4Indices,
    { NULL, _gActor120400Animation072B4Bank1, NULL, NULL, _gActor120400Animation072B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation07828Bank1[2] = {
#include "assets/actor_120400_animation_07828_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation07828Bank4[135] = {
#include "assets/actor_120400_animation_07828_bank4.inc"
};

static AnimationRecord _gActor120400Animation07828Records[188] = {
#include "assets/actor_120400_animation_07828_records.inc"
};

static u16 _gActor120400Animation07828Indices[20] = {
#include "assets/actor_120400_animation_07828_indices.inc"
};

static AnimationSet _gActor120400Animation07828 = {
    _gActor120400Animation07828Records,
    _gActor120400Animation07828Indices,
    { NULL, _gActor120400Animation07828Bank1, NULL, NULL, _gActor120400Animation07828Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation07A00Bank1[3] = {
#include "assets/actor_120400_animation_07A00_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation07A00Bank4[29] = {
#include "assets/actor_120400_animation_07A00_bank4.inc"
};

static AnimationRecord _gActor120400Animation07A00Records[60] = {
#include "assets/actor_120400_animation_07A00_records.inc"
};

static u16 _gActor120400Animation07A00Indices[20] = {
#include "assets/actor_120400_animation_07A00_indices.inc"
};

static AnimationSet _gActor120400Animation07A00 = {
    _gActor120400Animation07A00Records,
    _gActor120400Animation07A00Indices,
    { NULL, _gActor120400Animation07A00Bank1, NULL, NULL, _gActor120400Animation07A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation07E1CBank1[3] = {
#include "assets/actor_120400_animation_07E1C_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation07E1CBank4[80] = {
#include "assets/actor_120400_animation_07E1C_bank4.inc"
};

static AnimationRecord _gActor120400Animation07E1CRecords[154] = {
#include "assets/actor_120400_animation_07E1C_records.inc"
};

static u16 _gActor120400Animation07E1CIndices[20] = {
#include "assets/actor_120400_animation_07E1C_indices.inc"
};

static AnimationSet _gActor120400Animation07E1C = {
    _gActor120400Animation07E1CRecords,
    _gActor120400Animation07E1CIndices,
    { NULL, _gActor120400Animation07E1CBank1, NULL, NULL, _gActor120400Animation07E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation07FD8Bank1[2] = {
#include "assets/actor_120400_animation_07FD8_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation07FD8Bank4[25] = {
#include "assets/actor_120400_animation_07FD8_bank4.inc"
};

static AnimationRecord _gActor120400Animation07FD8Records[60] = {
#include "assets/actor_120400_animation_07FD8_records.inc"
};

static u16 _gActor120400Animation07FD8Indices[20] = {
#include "assets/actor_120400_animation_07FD8_indices.inc"
};

static AnimationSet _gActor120400Animation07FD8 = {
    _gActor120400Animation07FD8Records,
    _gActor120400Animation07FD8Indices,
    { NULL, _gActor120400Animation07FD8Bank1, NULL, NULL, _gActor120400Animation07FD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation081C0Bank1[3] = {
#include "assets/actor_120400_animation_081C0_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation081C0Bank4[33] = {
#include "assets/actor_120400_animation_081C0_bank4.inc"
};

static AnimationRecord _gActor120400Animation081C0Records[60] = {
#include "assets/actor_120400_animation_081C0_records.inc"
};

static u16 _gActor120400Animation081C0Indices[20] = {
#include "assets/actor_120400_animation_081C0_indices.inc"
};

static AnimationSet _gActor120400Animation081C0 = {
    _gActor120400Animation081C0Records,
    _gActor120400Animation081C0Indices,
    { NULL, _gActor120400Animation081C0Bank1, NULL, NULL, _gActor120400Animation081C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0847CBank1[3] = {
#include "assets/actor_120400_animation_0847C_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0847CBank4[39] = {
#include "assets/actor_120400_animation_0847C_bank4.inc"
};

static AnimationRecord _gActor120400Animation0847CRecords[107] = {
#include "assets/actor_120400_animation_0847C_records.inc"
};

static u16 _gActor120400Animation0847CIndices[20] = {
#include "assets/actor_120400_animation_0847C_indices.inc"
};

static AnimationSet _gActor120400Animation0847C = {
    _gActor120400Animation0847CRecords,
    _gActor120400Animation0847CIndices,
    { NULL, _gActor120400Animation0847CBank1, NULL, NULL, _gActor120400Animation0847CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0865CBank1[3] = {
#include "assets/actor_120400_animation_0865C_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0865CBank4[31] = {
#include "assets/actor_120400_animation_0865C_bank4.inc"
};

static AnimationRecord _gActor120400Animation0865CRecords[60] = {
#include "assets/actor_120400_animation_0865C_records.inc"
};

static u16 _gActor120400Animation0865CIndices[20] = {
#include "assets/actor_120400_animation_0865C_indices.inc"
};

static AnimationSet _gActor120400Animation0865C = {
    _gActor120400Animation0865CRecords,
    _gActor120400Animation0865CIndices,
    { NULL, _gActor120400Animation0865CBank1, NULL, NULL, _gActor120400Animation0865CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation08904Bank1[4] = {
#include "assets/actor_120400_animation_08904_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation08904Bank4[52] = {
#include "assets/actor_120400_animation_08904_bank4.inc"
};

static AnimationRecord _gActor120400Animation08904Records[86] = {
#include "assets/actor_120400_animation_08904_records.inc"
};

static u16 _gActor120400Animation08904Indices[20] = {
#include "assets/actor_120400_animation_08904_indices.inc"
};

static AnimationSet _gActor120400Animation08904 = {
    _gActor120400Animation08904Records,
    _gActor120400Animation08904Indices,
    { NULL, _gActor120400Animation08904Bank1, NULL, NULL, _gActor120400Animation08904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation08BE0Bank1[4] = {
#include "assets/actor_120400_animation_08BE0_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation08BE0Bank4[59] = {
#include "assets/actor_120400_animation_08BE0_bank4.inc"
};

static AnimationRecord _gActor120400Animation08BE0Records[92] = {
#include "assets/actor_120400_animation_08BE0_records.inc"
};

static u16 _gActor120400Animation08BE0Indices[20] = {
#include "assets/actor_120400_animation_08BE0_indices.inc"
};

static AnimationSet _gActor120400Animation08BE0 = {
    _gActor120400Animation08BE0Records,
    _gActor120400Animation08BE0Indices,
    { NULL, _gActor120400Animation08BE0Bank1, NULL, NULL, _gActor120400Animation08BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation08EB4Bank1[4] = {
#include "assets/actor_120400_animation_08EB4_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation08EB4Bank4[58] = {
#include "assets/actor_120400_animation_08EB4_bank4.inc"
};

static AnimationRecord _gActor120400Animation08EB4Records[91] = {
#include "assets/actor_120400_animation_08EB4_records.inc"
};

static u16 _gActor120400Animation08EB4Indices[20] = {
#include "assets/actor_120400_animation_08EB4_indices.inc"
};

static AnimationSet _gActor120400Animation08EB4 = {
    _gActor120400Animation08EB4Records,
    _gActor120400Animation08EB4Indices,
    { NULL, _gActor120400Animation08EB4Bank1, NULL, NULL, _gActor120400Animation08EB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation091CCBank1[4] = {
#include "assets/actor_120400_animation_091CC_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation091CCBank4[66] = {
#include "assets/actor_120400_animation_091CC_bank4.inc"
};

static AnimationRecord _gActor120400Animation091CCRecords[100] = {
#include "assets/actor_120400_animation_091CC_records.inc"
};

static u16 _gActor120400Animation091CCIndices[20] = {
#include "assets/actor_120400_animation_091CC_indices.inc"
};

static AnimationSet _gActor120400Animation091CC = {
    _gActor120400Animation091CCRecords,
    _gActor120400Animation091CCIndices,
    { NULL, _gActor120400Animation091CCBank1, NULL, NULL, _gActor120400Animation091CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation097E4Bank1[4] = {
#include "assets/actor_120400_animation_097E4_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation097E4Bank4[147] = {
#include "assets/actor_120400_animation_097E4_bank4.inc"
};

static AnimationRecord _gActor120400Animation097E4Records[211] = {
#include "assets/actor_120400_animation_097E4_records.inc"
};

static u16 _gActor120400Animation097E4Indices[20] = {
#include "assets/actor_120400_animation_097E4_indices.inc"
};

static AnimationSet _gActor120400Animation097E4 = {
    _gActor120400Animation097E4Records,
    _gActor120400Animation097E4Indices,
    { NULL, _gActor120400Animation097E4Bank1, NULL, NULL, _gActor120400Animation097E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0A190Bank1[9] = {
#include "assets/actor_120400_animation_0A190_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0A190Bank4[245] = {
#include "assets/actor_120400_animation_0A190_bank4.inc"
};

static AnimationRecord _gActor120400Animation0A190Records[327] = {
#include "assets/actor_120400_animation_0A190_records.inc"
};

static u16 _gActor120400Animation0A190Indices[20] = {
#include "assets/actor_120400_animation_0A190_indices.inc"
};

static AnimationSet _gActor120400Animation0A190 = {
    _gActor120400Animation0A190Records,
    _gActor120400Animation0A190Indices,
    { NULL, _gActor120400Animation0A190Bank1, NULL, NULL, _gActor120400Animation0A190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0A3B0Bank1[2] = {
#include "assets/actor_120400_animation_0A3B0_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0A3B0Bank4[25] = {
#include "assets/actor_120400_animation_0A3B0_bank4.inc"
};

static AnimationRecord _gActor120400Animation0A3B0Records[85] = {
#include "assets/actor_120400_animation_0A3B0_records.inc"
};

static u16 _gActor120400Animation0A3B0Indices[20] = {
#include "assets/actor_120400_animation_0A3B0_indices.inc"
};

static AnimationSet _gActor120400Animation0A3B0 = {
    _gActor120400Animation0A3B0Records,
    _gActor120400Animation0A3B0Indices,
    { NULL, _gActor120400Animation0A3B0Bank1, NULL, NULL, _gActor120400Animation0A3B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0A720Bank1[6] = {
#include "assets/actor_120400_animation_0A720_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0A720Bank4[73] = {
#include "assets/actor_120400_animation_0A720_bank4.inc"
};

static AnimationRecord _gActor120400Animation0A720Records[109] = {
#include "assets/actor_120400_animation_0A720_records.inc"
};

static u16 _gActor120400Animation0A720Indices[20] = {
#include "assets/actor_120400_animation_0A720_indices.inc"
};

static AnimationSet _gActor120400Animation0A720 = {
    _gActor120400Animation0A720Records,
    _gActor120400Animation0A720Indices,
    { NULL, _gActor120400Animation0A720Bank1, NULL, NULL, _gActor120400Animation0A720Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0A960Bank1[2] = {
#include "assets/actor_120400_animation_0A960_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0A960Bank4[25] = {
#include "assets/actor_120400_animation_0A960_bank4.inc"
};

static AnimationRecord _gActor120400Animation0A960Records[93] = {
#include "assets/actor_120400_animation_0A960_records.inc"
};

static u16 _gActor120400Animation0A960Indices[20] = {
#include "assets/actor_120400_animation_0A960_indices.inc"
};

static AnimationSet _gActor120400Animation0A960 = {
    _gActor120400Animation0A960Records,
    _gActor120400Animation0A960Indices,
    { NULL, _gActor120400Animation0A960Bank1, NULL, NULL, _gActor120400Animation0A960Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0AC50Bank1[5] = {
#include "assets/actor_120400_animation_0AC50_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0AC50Bank4[52] = {
#include "assets/actor_120400_animation_0AC50_bank4.inc"
};

static AnimationRecord _gActor120400Animation0AC50Records[101] = {
#include "assets/actor_120400_animation_0AC50_records.inc"
};

static u16 _gActor120400Animation0AC50Indices[20] = {
#include "assets/actor_120400_animation_0AC50_indices.inc"
};

static AnimationSet _gActor120400Animation0AC50 = {
    _gActor120400Animation0AC50Records,
    _gActor120400Animation0AC50Indices,
    { NULL, _gActor120400Animation0AC50Bank1, NULL, NULL, _gActor120400Animation0AC50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0B110Bank1[8] = {
#include "assets/actor_120400_animation_0B110_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0B110Bank4[93] = {
#include "assets/actor_120400_animation_0B110_bank4.inc"
};

static AnimationRecord _gActor120400Animation0B110Records[167] = {
#include "assets/actor_120400_animation_0B110_records.inc"
};

static u16 _gActor120400Animation0B110Indices[20] = {
#include "assets/actor_120400_animation_0B110_indices.inc"
};

static AnimationSet _gActor120400Animation0B110 = {
    _gActor120400Animation0B110Records,
    _gActor120400Animation0B110Indices,
    { NULL, _gActor120400Animation0B110Bank1, NULL, NULL, _gActor120400Animation0B110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0B544Bank1[2] = {
#include "assets/actor_120400_animation_0B544_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0B544Bank4[98] = {
#include "assets/actor_120400_animation_0B544_bank4.inc"
};

static AnimationRecord _gActor120400Animation0B544Records[145] = {
#include "assets/actor_120400_animation_0B544_records.inc"
};

static u16 _gActor120400Animation0B544Indices[20] = {
#include "assets/actor_120400_animation_0B544_indices.inc"
};

static AnimationSet _gActor120400Animation0B544 = {
    _gActor120400Animation0B544Records,
    _gActor120400Animation0B544Indices,
    { NULL, _gActor120400Animation0B544Bank1, NULL, NULL, _gActor120400Animation0B544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0B9E0Bank1[4] = {
#include "assets/actor_120400_animation_0B9E0_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0B9E0Bank4[105] = {
#include "assets/actor_120400_animation_0B9E0_bank4.inc"
};

static AnimationRecord _gActor120400Animation0B9E0Records[158] = {
#include "assets/actor_120400_animation_0B9E0_records.inc"
};

static u16 _gActor120400Animation0B9E0Indices[20] = {
#include "assets/actor_120400_animation_0B9E0_indices.inc"
};

static AnimationSet _gActor120400Animation0B9E0 = {
    _gActor120400Animation0B9E0Records,
    _gActor120400Animation0B9E0Indices,
    { NULL, _gActor120400Animation0B9E0Bank1, NULL, NULL, _gActor120400Animation0B9E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0BC30Bank1[3] = {
#include "assets/actor_120400_animation_0BC30_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0BC30Bank4[43] = {
#include "assets/actor_120400_animation_0BC30_bank4.inc"
};

static AnimationRecord _gActor120400Animation0BC30Records[76] = {
#include "assets/actor_120400_animation_0BC30_records.inc"
};

static u16 _gActor120400Animation0BC30Indices[20] = {
#include "assets/actor_120400_animation_0BC30_indices.inc"
};

static AnimationSet _gActor120400Animation0BC30 = {
    _gActor120400Animation0BC30Records,
    _gActor120400Animation0BC30Indices,
    { NULL, _gActor120400Animation0BC30Bank1, NULL, NULL, _gActor120400Animation0BC30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0BE80Bank1[3] = {
#include "assets/actor_120400_animation_0BE80_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0BE80Bank4[43] = {
#include "assets/actor_120400_animation_0BE80_bank4.inc"
};

static AnimationRecord _gActor120400Animation0BE80Records[76] = {
#include "assets/actor_120400_animation_0BE80_records.inc"
};

static u16 _gActor120400Animation0BE80Indices[20] = {
#include "assets/actor_120400_animation_0BE80_indices.inc"
};

static AnimationSet _gActor120400Animation0BE80 = {
    _gActor120400Animation0BE80Records,
    _gActor120400Animation0BE80Indices,
    { NULL, _gActor120400Animation0BE80Bank1, NULL, NULL, _gActor120400Animation0BE80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0C278Bank1[3] = {
#include "assets/actor_120400_animation_0C278_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0C278Bank4[86] = {
#include "assets/actor_120400_animation_0C278_bank4.inc"
};

static AnimationRecord _gActor120400Animation0C278Records[139] = {
#include "assets/actor_120400_animation_0C278_records.inc"
};

static u16 _gActor120400Animation0C278Indices[20] = {
#include "assets/actor_120400_animation_0C278_indices.inc"
};

static AnimationSet _gActor120400Animation0C278 = {
    _gActor120400Animation0C278Records,
    _gActor120400Animation0C278Indices,
    { NULL, _gActor120400Animation0C278Bank1, NULL, NULL, _gActor120400Animation0C278Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0C674Bank1[3] = {
#include "assets/actor_120400_animation_0C674_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0C674Bank4[89] = {
#include "assets/actor_120400_animation_0C674_bank4.inc"
};

static AnimationRecord _gActor120400Animation0C674Records[137] = {
#include "assets/actor_120400_animation_0C674_records.inc"
};

static u16 _gActor120400Animation0C674Indices[20] = {
#include "assets/actor_120400_animation_0C674_indices.inc"
};

static AnimationSet _gActor120400Animation0C674 = {
    _gActor120400Animation0C674Records,
    _gActor120400Animation0C674Indices,
    { NULL, _gActor120400Animation0C674Bank1, NULL, NULL, _gActor120400Animation0C674Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120400Animation0C888Bank1[2] = {
#include "assets/actor_120400_animation_0C888_bank1.inc"
};

static AnimationPackedRotation _gActor120400Animation0C888Bank4[22] = {
#include "assets/actor_120400_animation_0C888_bank4.inc"
};

static AnimationRecord _gActor120400Animation0C888Records[85] = {
#include "assets/actor_120400_animation_0C888_records.inc"
};

static u16 _gActor120400Animation0C888Indices[20] = {
#include "assets/actor_120400_animation_0C888_indices.inc"
};

static AnimationSet _gActor120400Animation0C888 = {
    _gActor120400Animation0C888Records,
    _gActor120400Animation0C888Indices,
    { NULL, _gActor120400Animation0C888Bank1, NULL, NULL, _gActor120400Animation0C888Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_120400_8013E6D0[29] = {
    NULL,
    &_gActor120400Animation0653C,
    &_gActor120400Animation08904,
    &_gActor120400Animation08BE0,
    &_gActor120400Animation08EB4,
    &_gActor120400Animation091CC,
    &_gActor120400Animation097E4,
    &_gActor120400Animation0A190,
    &_gActor120400Animation0A3B0,
    &_gActor120400Animation07A00,
    &_gActor120400Animation07E1C,
    &_gActor120400Animation07FD8,
    &_gActor120400Animation081C0,
    &_gActor120400Animation0847C,
    &_gActor120400Animation0865C,
    &_gActor120400Animation07828,
    &_gActor120400Animation06CD0,
    &_gActor120400Animation072B4,
    &_gActor120400Animation0A720,
    &_gActor120400Animation0A960,
    &_gActor120400Animation0AC50,
    &_gActor120400Animation0B110,
    &_gActor120400Animation0B544,
    &_gActor120400Animation0B9E0,
    &_gActor120400Animation0BC30,
    &_gActor120400Animation0BE80,
    &_gActor120400Animation0C278,
    &_gActor120400Animation0C674,
    &_gActor120400Animation0C888,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_120400_8013E6D0,
};

TaskDesc D_actor_120400_8013E748[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor120400KyleMadiganWalkerTask, { .model = &_gActor120400KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor120400KyleMadiganHandTask, { .model = &_gActor120400KyleMadiganLeft } },
    { { { TASK_BODY_TMD, 192 } }, _actor120400KyleMadiganHandTask, { .model = &_gActor120400KyleMadiganHandRight } },
};

TaskMessageEntry D_actor_120400_8013E76C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor120400SetKyleMadiganDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actor120400StartKyleMadiganWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor120400IgnoreKyleMadiganCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Allocates Kyle's scripted-walker work and spawns his two attached hand models.
///
/// Requires the body model, enemy placement index and current loaded area.
/// Allocation failure exits the enemy task. On success initializes absent
/// clip/bank and buffer-release sentinels, clears 16.16 movement carry, and
/// spawns hand slots 1/2 attached to body parts 8/12. Failed child spawns are
/// skipped independently; successful children receive placement texture offsets
/// and both existing buffer halves are rebuilt. Binds the body's borrowed light
/// and colour matrices, messages and exit callback, then advances task state.
static void _actor120400SpawnKyleMadiganWalker(Task* task)
{
    enum {
        ACTOR_120400_KYLE_FREE_NOT_PENDING = -1,
        ACTOR_120400_KYLE_LEFT_HAND_TASK   = 1,
        ACTOR_120400_KYLE_RIGHT_HAND_TASK  = 2,
        ACTOR_120400_KYLE_LEFT_HAND_PART   = 8,
        ACTOR_120400_KYLE_RIGHT_HAND_PART  = 12,
    };

    _Actor120400KyleMadiganWork* work;
    Task*                        childTask;

    work = memCalloc(sizeof(_Actor120400KyleMadiganWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = ACTOR_120400_KYLE_FREE_NOT_PENDING;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    // Spawn independent hand models, borrowing the body task until teardown.
    childTask = taskSpawnFromTable(D_actor_120400_8013E748, ACTOR_120400_KYLE_LEFT_HAND_TASK, ACTOR_120400_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    childTask = taskSpawnFromTable(D_actor_120400_8013E748, ACTOR_120400_KYLE_RIGHT_HAND_TASK, ACTOR_120400_KYLE_RIGHT_HAND_PART, task);
    if (childTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    _actor120400BindKyleMadiganWalkerLighting(task);
    task->msgTable     = D_actor_120400_8013E76C;
    task->exitCallback = _actor120400ExitKyleMadiganWalker;
    task->state       += 1;
}

/// Applies one frame's signed 16.16 velocity and retains the fractional carry.
///
/// Requires live writable work and root coordinate. Invalidates composition
/// even when stationary; the remaining fractions are zero-extended.
static inline void _actor120400IntegrateWalkVelocity(_Actor120400KyleMadiganWork* work, GfxCoord* rootCoord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    rootCoord->coord.t[0]    += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]    += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]    += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Updates Kyle's walk, animation, ground shadow, lighting and delayed buffer release.
///
/// Requires a live TMD body with its twenty-part rig and
/// `_Actor120400KyleMadiganWork`; `walk.motion` must be 0 (idle) or 1 (walking).
/// The body dispatcher runs this only while scene actors are running.
/// Slots 1..19 tick once a clip has been applied. Active-draw exclusion gates
/// the shadow; view readiness gates lighting. The update finding a release
/// counter of zero frees the body primitive buffer, then sets the counter to -1.
static void _actor120400UpdateKyleMadiganWalker(Task* task)
{
    enum {
        ACTOR_120400_KYLE_SHADOW_HALF_SIZE = 0x300, // World-coordinate units
        ACTOR_120400_KYLE_LIGHT_COUNT      = 3,
    };
    TmdObject*                   bodyModel         = task->extra.tmd;
    _Actor120400KyleMadiganWork* work              = task->work;
    TaskFunc                     motionHandlers[2] = { _actor120400IdleKyleMadiganWalk, _actor120400RunKyleMadiganWalkStep };
    VECTOR3                      groundPosition;
    GfxCoord*                    rootCoord;
    s32                          slotIndex;

    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    _actor120400IntegrateWalkVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Use the composed body part for ground projection before refreshing its lighting.
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_120400_KYLE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, ACTOR_120400_KYLE_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

/// Starts Kyle's four-step scripted walk toward a borrowed destination.
///
/// Requires a live TMD body and `_Actor120400KyleMadiganWork`. Copies XYZ
/// position in the root parent's coordinate frame and Euler angles in 4096
/// units per turn; the closing step uses yaw. Optional `walkAnim` supplies
/// bank-0 start and closing clip IDs in 1..28; NULL selects clips 16 and 1. Both borrowed
/// records need only survive this call. Plays the start clip with a five-frame
/// blend when already ticking, otherwise resets slots 1..19. The message ID
/// is ignored. Returns 0; a new request restarts an existing walk.
static s32 _actor120400StartKyleMadiganWalk(Task* task, s32 msgId, const ActorTransform* placement,
                                            const ActorMotionWalkAnim* walkAnim)
{
    enum {
        ACTOR_120400_KYLE_WALK_FACE_TARGET  = 0,
        ACTOR_120400_KYLE_ANIMATION_BANK    = 0,
        ACTOR_120400_KYLE_WALK_START_CLIP   = 0x10,
        ACTOR_120400_KYLE_WALK_CLOSING_CLIP = 1,
        ACTOR_120400_KYLE_WALK_BLEND_FRAMES = 5,
    };
    ActorMotionPlayWork*         animationWork;
    _Actor120400KyleMadiganWork* walkWork;
    AnimationPlayRequest         startRequest;

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = ACTOR_120400_KYLE_WALK_FACE_TARGET;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    startRequest.source.index   = ACTOR_120400_KYLE_ANIMATION_BANK;
    if (walkAnim != NULL) {
        startRequest.animationId   = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        startRequest.animationId   = ACTOR_120400_KYLE_WALK_START_CLIP;
        walkWork->model.nextAnimId = ACTOR_120400_KYLE_WALK_CLOSING_CLIP;
    }
    startRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    startRequest.blendFrames          = ACTOR_120400_KYLE_WALK_BLEND_FRAMES;
    startRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    // Apply the request after latching the destination and closing clip.
    animationWork = task->work;
    _actorMotionApplyAnimationRequest(animationWork, task->extra.tmd, &startRequest);
    return 0;
}

/// Runs a Kyle hand model's attachment, draw-policy update or teardown state.
///
/// Requires a live TMD task with state 0..2. Setup borrows its body task from
/// `spawnArg2.pointer` and its part index from `spawnArg1.value` (8 left,
/// 12 right). The parent's coordinates and lighting storage must outlive the
/// hand's active use. Setup links teardown to the body; later ticks mirror draw and
/// automatic-buffer flags and recover missing buffers when permitted.
static void _actor120400KyleMadiganHandTask(Task* handTask)
{
    TaskFuncTable3 states;

    states = D_actor_120400_80131E24;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Runs Kyle's body setup, update or teardown while scene actors are running.
///
/// Requires a live TMD task with state 0..2. Setup borrows its live `Enemy`
/// spawn argument and allocates the walker work; updates require that work.
/// The actor-control gate covers every state, including setup and teardown.
static void _actor120400KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_120400_80131E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Releases Kyle's enemy tracking and tears down the body and attached tasks.
///
/// Requires the live primary-heap `Enemy` in `spawnArg2.pointer`. The common
/// teardown releases that object, invokes child exits and frees walker work.
/// TMD tasks suppress drawing and may remain alive for deferred body release;
/// the enemy and work pointers are invalid afterwards.
static void _actor120400ExitKyleMadiganWalker(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the walker's light-direction and light-colour matrices to its body model.
///
/// Requires a live TMD body and `_Actor120400KyleMadiganWork`. The model borrows
/// the matrices for active drawing; keep the work live throughout that use.
static void _actor120400BindKyleMadiganWalkerLighting(Task* task)
{
    TmdObject*                   bodyModel;
    _Actor120400KyleMadiganWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves Kyle's motion idle while the body update continues animation and lighting.
static void _actor120400IdleKyleMadiganWalk(Task* task)
{
}

/// Runs the current step of Kyle's scripted walk.
///
/// Requires live body work and `walk.motionStep` in 0..3: face the destination,
/// begin forward motion, check arrival, then turn to the destination yaw.
/// The steps advance the index; the closing turn returns motion to idle.
static void _actor120400RunKyleMadiganWalkStep(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    TaskFuncTable4               handlers;

    work     = task->work;
    handlers = D_actor_120400_80131E3C;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Begins forward motion and advances Kyle's walk to the arrival check.
///
/// Requires a live TMD root and walker work in step 1. The root's Q12 rotation
/// transforms local +Z velocity of 32 units per frame, stored as signed 16.16,
/// into the parent's frame. Seeds all previous-distance components with
/// `ACTOR_WALK_DISTANCE_NONE`, so the first arrival check records X/Z distance.
static void _actor120400BeginKyleMadiganWalk(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    GfxCoord*                    rootCoord;
    VECTOR                       localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = D_actor_120400_80131E4C;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sets Kyle's active-draw and automatic-buffer policy and schedules buffer release.
///
/// Requires a live TMD body and walker work. Modes 0/1 hide/show with automatic
/// buffers enabled; showing requests a missing body buffer. Mode 2 hides,
/// disables automatic buffers and sets the release counter to 2, freeing the
/// body buffer on the third subsequent running update. Mode 3 shows with
/// automatic buffers disabled. Modes 0, 1 and 3 retain a pending release.
/// Other flags remain unchanged; attached hands mirror the policy on their
/// own ticks. Returns 0 for modes 0..3, otherwise 1, even if allocation fails.
/// The message ID and second payload are ignored. `mode` is a full signed
/// word; its counter store narrows to a halfword.
static s32 _actor120400SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_120400_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*                   bodyModel;
    _Actor120400KyleMadiganWork* work;
    s32                          result;

    bodyModel = task->extra.tmd;
    work      = task->work;
    result    = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            bodyModel->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_120400_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Accepts an actor command without changing Kyle's state and returns zero.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; ignores the receiver, message
/// ID and both payload words and retains no borrowed command data.
static s32 _actor120400IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
