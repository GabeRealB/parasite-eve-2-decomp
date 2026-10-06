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

static void func_actor_120400_80131E5C(Task* arg0);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void func_actor_120400_80132050(Task* arg0);
static void func_actor_120400_801327B4(Task* task);
static void func_actor_120400_801327D4(Task* task);
static void func_actor_120400_801327F0(Task* arg0);
static void func_actor_120400_801327F8(Task* task);
static void func_actor_120400_80132920(Task* task);

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `func_actor_120400_8013254C`.
static const TaskFuncTable3 D_actor_120400_80131E24 = { {
    modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent task, dispatched by
/// `func_actor_120400_80132748`.
static const TaskFuncTable3 D_actor_120400_80131E30 = { {
    func_actor_120400_80131E5C,
    func_actor_120400_80132050,
    func_actor_120400_801327B4,
} };

/// Steps of the parent's walk sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_120400_80131E3C = { {
    actorMotionFaceTarget,
    func_actor_120400_80132920,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
static const VECTOR D_actor_120400_80131E4C = { 0, 0, 0x200000, 0 };

static TmdSource _gActor120400KyleMadiganBody;
static TmdSource _gActor120400KyleMadiganHandRight;
static TmdSource _gActor120400KyleMadiganLeft;
s32              func_actor_120400_80132398(Task* task, s32 msgId, ActorTransform* place, ActorMotionWalkAnim*);
s32              func_actor_120400_80132C38(Task*, s32, s32, s32);
s32              func_actor_120400_80132D14(Task*, s32, s32, s32);
void             func_actor_120400_8013254C(Task*);
void             func_actor_120400_80132748(Task*);

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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120400_80132748, { .model = &_gActor120400KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_120400_8013254C, { .model = &_gActor120400KyleMadiganLeft } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_120400_8013254C, { .model = &_gActor120400KyleMadiganHandRight } },
};

TaskMessageEntry D_actor_120400_8013E76C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_120400_80132C38 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_120400_80132398 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_120400_80132D14 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The parent's spawn handler. Allocates the `_Actor120400KyleMadiganWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::texturePageOffset` / `clutRowOffset` loaded with the texture page and CLUT
/// row of the `AreaPlacement` that entry selects, reached through the area key
/// `&gGameSession->location.loc` and indexed by the model id the child's own
/// `spawnArg2` carries at `Enemy::placeKey >> ENEMY_PLACE_INDEX_SHIFT`, and each then has its
/// texture stream processed twice when it has a buffer. The body ends by
/// pointing the parent's model at its light/colour matrices
/// (`func_actor_120400_801327D4`), pointing `msgTable` at the message table and
/// installing `func_actor_120400_801327B4` as its exit callback.
static void func_actor_120400_80131E5C(Task* arg0)
{
    _Actor120400KyleMadiganWork* work;
    GameLocationKey              key;
    GameLocationKey*             sessionKey;
    GameLocationKey*             keyAddr;
    Task*                        spawned;

    work = memCalloc(sizeof(_Actor120400KyleMadiganWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    spawned                  = taskSpawnFromTable(D_actor_120400_8013E748, 1, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        model      = spawned->extra.tmd;
        idx        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        layout                   = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    spawned = taskSpawnFromTable(D_actor_120400_8013E748, 2, 0xC, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        model = spawned->extra.tmd;
        idx   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        // Keep this block's key address separate across the spawn calls.
        sessionKey = (keyAddr = &gGameSession->location.loc);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = keyAddr->room;
        key.view   = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        layout                   = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    func_actor_120400_801327D4(arg0);
    arg0->msgTable     = D_actor_120400_8013E76C;
    arg0->exitCallback = func_actor_120400_801327B4;
    arg0->state       += 1;
}

/// The parent's per-frame update: the motion handler -- entry `walk.motion` of
/// the pair `{func_actor_120400_801327F0, func_actor_120400_801327F8}`, idle or
/// the walk sequence -- runs first, then
/// the three 16.16 words of `walk.carry` take this frame's `velocity`,
/// their integer halves are added onto the root coordinate's translation and
/// the fraction is dropped, and `composeStamp` is cleared so the tree rebuilds. With
/// `model.ticking` set every animation slot is ticked. Unless the model is hidden
/// (bit 0x80 of `TmdObject::flags`), the second coordinate's work matrix
/// feeds `worldCollisionProjectGroundPoint` and a non-zero result draws the ground-effect quad;
/// when `gGameSession->viewReady` is set the same coordinate is flagged stale,
/// updated and re-ranked through `worldCoordSetModelLighting`. The body ends decrementing
/// the `freeCountdown` teardown timer, freeing the model's buffers on the frame it
/// reaches zero.
static void func_actor_120400_80132050(Task* arg0)
{
    TmdObject*                   ext      = arg0->extra.tmd;
    _Actor120400KyleMadiganWork* work     = arg0->work;
    TaskFunc                     funcs[2] = { func_actor_120400_801327F0, func_actor_120400_801327F8 };
    VECTOR3                      pos;
    GfxCoord*                    coord;
    s32                          i;

    funcs[work->walk.motion](arg0);
    coord                     = arg0->extra.tmd->coords;
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    coord->coord.t[0]        += work->walk.carry[0].halves.integer;
    coord->coord.t[1]        += work->walk.carry[1].halves.integer;
    coord->coord.t[2]        += work->walk.carry[2].halves.integer;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
    if (work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        worldCoordSetModelLighting(ext, arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

/// Message 0x7DD handler of the parent: starts the walk sequence toward a
/// placement. The position and rotation are copied into `target` and
/// `walk.targetRot`, `walk.motion` selects the walk and `walk.motionStep` restarts
/// it, and a start preset is built on the stack -- bank id 0, the optional
/// `animationId` and `nextAnimId` (0x10 and 1 when absent), 1, 5 and 1 --
/// and then applied in-line. A changed bank id latches `model.bank` and reseeds
/// the animation through `animationInitContext` with the bank this overlay's
/// `gActorMotionAnimBanks` selects; `model.animId` takes the preset's animation
/// id, and a preset asking for slots while `model.ticking` says the slots are
/// already ticking is pushed onto `animationSeekSlotWithBlend`'s per-slot loop instead of
/// the `animationResetSlot` one, followed by a `animationTickSlot` pass over the
/// same 0x14 slots and `model.ticking` raised. Returns 0 either way.
s32 func_actor_120400_80132398(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    _Actor120400KyleMadiganWork* work;
    _Actor120400KyleMadiganWork* w;
    AnimationPlayRequest         preset;
    AnimationPlayRequest*        msg;
    s32                          i;
    TmdObject*                   ext;

    w                    = task->work;
    w->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    w->walk.motionStep   = 0;
    w->walk.target.vx    = place->pos.vx;
    w->walk.target.vy    = place->pos.vy;
    w->walk.target.vz    = place->pos.vz;
    w->walk.targetRot.vx = place->rot.vx;
    w->walk.targetRot.vy = place->rot.vy;
    w->walk.targetRot.vz = place->rot.vz;
    preset.source.index  = 0;
    if (anim != NULL) {
        preset.animationId  = anim->animationId;
        w->model.nextAnimId = anim->nextAnimId;
    } else {
        preset.animationId  = 0x10;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
        }
    } else {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// State dispatcher of the two child tasks: copies their spawn/tick/teardown
/// table onto the stack and runs the entry `Task::state` selects.
void func_actor_120400_8013254C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// State dispatcher of the parent task: copies its spawn/tick/teardown table
/// onto the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_120400_80132748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback of the parent task, installed by its spawn handler: runs the
/// common enemy teardown.
static void func_actor_120400_801327B4(Task* task)
{
    enemyTaskExit(task);
}

/// Points the parent's model at the light and colour matrices held in its own
/// work block.
static void func_actor_120400_801327D4(Task* task)
{
    TmdObject*                   ext;
    _Actor120400KyleMadiganWork* work;

    ext           = task->extra.tmd;
    work          = task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Motion handler 0 of the parent, idle: does nothing.
static void func_actor_120400_801327F0(Task* arg0)
{
}

/// Motion handler 1 of the parent, the walk sequence: copies the step table
/// onto the stack and runs the entry `walk.motionStep` selects.
static void func_actor_120400_801327F8(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    TaskFuncTable4               handlers;

    work     = task->work;
    handlers = D_actor_120400_80131E3C;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Walk step 1: rotates the constant forward offset `D_actor_120400_80131E4C`
/// through the root part's matrix into `velocity`, seeds `lastDistance` with
/// `ACTOR_WALK_DISTANCE_NONE` and advances the step.
static void func_actor_120400_80132920(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    GfxCoord*                    coord;
    VECTOR                       vec;

    coord = task->extra.tmd->coords;
    work  = task->work;

    vec = D_actor_120400_80131E4C;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message 0x7D5 handler of the parent: shows or hides its model. `mode`
/// drives the `TmdObject` parked in `Task::extra` -- bit 0x80 hides it, bit
/// 0x4 is the one the children copy alongside it:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `tmdAllocPrimitiveBuffer`, drop 0x4
///   mode 2  hide, start the `freeCountdown` countdown to freeing the buffers, raise 0x4
///   mode 3  show, raise 0x4
///
/// Returns 0 for the four known modes and 1 for any other.
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*                   obj;
    _Actor120400KyleMadiganWork* work;
    s32                          ret;

    obj  = task->extra.tmd;
    work = task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message 0x7DB handler of the parent: ignores the message and returns 0.
s32 func_actor_120400_80132D14(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}
