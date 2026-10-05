#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"
#include "../../shared/model_placement.h"

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// The marker quad's two vertex pairs, in the actor's local frame: `-4/+4`
/// and `-3/+3` along X, all coplanar in Z.
extern SVECTOR D_actor_135600_8013B060[4];

/// Animation bank table the 0x7D3 handler seeds the slot array from, indexed
/// by the preset's bank index.
extern AnimationSet*  D_actor_135600_8013B080[16];
extern AnimationSet** gActorMotionAnimBanks[1];

/// Child task table the setup handler spawns from: entry 0 is the actor
/// itself, entries 1 and 2 the two part models and entry 3 the marker task.
extern TaskDesc D_actor_135600_8013B0C4[];

/// The actor's message table, stored in `Task::msgTable`: 0x7D3, 0x7D4, 0x7D5,
/// 0x7DD and 0x7DB against the handlers below.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_135600_8013B0F4[];

static void func_actor_135600_80132234(Task* task);
static void func_actor_135600_801324D0(Task* task);
static void func_actor_135600_80132AB4(Task* task);
static void func_actor_135600_80132B14(Task* task);
static void func_actor_135600_80132C18(Task* task);
static void func_actor_135600_80132C80(GfxCoord* coord, MATRIX* mtx, SVECTOR* vec);
static void func_actor_135600_80132DBC(Task* task);
static void func_actor_135600_80132DDC(Task* task);
static void func_actor_135600_80132DF8(Task* task);
static void func_actor_135600_80132E00(Task* task);
static void func_actor_135600_80132F28(Task* task);
s32         func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3);

/// States of the two part tasks (`D_actor_135600_8013B0C4` entries 1 and 2),
/// dispatched by `func_actor_135600_801329E0`: attach to the parent, idle,
/// kill.
static const TaskFuncTable3 D_actor_135600_80131E24 = { {
    modelPlacementAttachPart,
    func_actor_135600_80132AB4,
    taskKill,
} };

/// States of the marker task (entry 3), dispatched by
/// `func_actor_135600_80132ABC`: attach to the parent with an offset, draw the
/// marker, kill.
static const TaskFuncTable3 D_actor_135600_80131E30 = { {
    func_actor_135600_80132B14,
    func_actor_135600_80132C18,
    taskKill,
} };

/// States of the actor itself (entry 0), dispatched by
/// `func_actor_135600_80132D64`: setup, per-frame tick, exit.
static const TaskFuncTable3 D_actor_135600_80131E3C = { {
    func_actor_135600_80132234,
    func_actor_135600_801324D0,
    func_actor_135600_80132DBC,
} };

/// Step handlers of the motion sequence, indexed by
/// `KyleMadiganWalkerWork::walk.motionStep`: turn to face `target`, start walking forward,
/// walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_135600_80131E48 = { {
    actorMotionFaceTarget,
    func_actor_135600_80132F28,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset `func_actor_135600_80132F28` rotates:
/// straight ahead along the part's own +Z.
static const VECTOR D_actor_135600_80131E58 = { 0, 0, 0x200000, 0 };

static AnimationSet _gActor135600Animation06F40;
static AnimationSet _gActor135600Animation071F4;
static AnimationSet _gActor135600Animation07588;
static AnimationSet _gActor135600Animation077AC;
static AnimationSet _gActor135600Animation07B08;
static AnimationSet _gActor135600Animation07DE4;
static AnimationSet _gActor135600Animation07FB0;
static AnimationSet _gActor135600Animation0817C;
static AnimationSet _gActor135600Animation08364;
static AnimationSet _gActor135600Animation08644;
static AnimationSet _gActor135600Animation089E4;
static AnimationSet _gActor135600Animation08C00;
static AnimationSet _gActor135600Animation08DF4;
static AnimationSet _gActor135600Animation08FF8;
static AnimationSet _gActor135600Animation09218;
static TmdSource    _gActor135600KyleMadiganBody;
static TmdSource    _gActor135600KyleMadiganHandRight;
static TmdSource    _gActor135600KyleMadiganHandLeft;
static TmdSource    _gActor135600Model06AC4;
s32                 func_actor_135600_80133240(Task*, s32, s32, s32);
s32                 func_actor_135600_8013336C(Task*, s32, s32, s32);
void                func_actor_135600_801329E0(Task*);
void                func_actor_135600_80132ABC(Task*);
void                func_actor_135600_80132D64(Task*);

static TmdBone _gActor135600KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor135600KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor135600KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor135600KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor135600KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor135600KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor135600KyleMadiganBodyPartVerts,
    _gActor135600KyleMadiganBodyVerts,
    _gActor135600KyleMadiganBodyNormals,
    _gActor135600KyleMadiganBodySkeleton,
    _gActor135600KyleMadiganBodyStream,
};

static TmdBone _gActor135600KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor135600KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor135600KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor135600KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor135600KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor135600KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor135600KyleMadiganHandRightPartVerts,
    _gActor135600KyleMadiganHandRightVerts,
    _gActor135600KyleMadiganHandRightNormals,
    _gActor135600KyleMadiganHandRightSkeleton,
    _gActor135600KyleMadiganHandRightStream,
};

static TmdBone _gActor135600KyleMadiganHandLeftSkeleton[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

static u32 _gActor135600KyleMadiganHandLeftPartVerts[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

static SVECTOR _gActor135600KyleMadiganHandLeftVerts[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

static SVECTOR _gActor135600KyleMadiganHandLeftNormals[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

static u32 _gActor135600KyleMadiganHandLeftStream[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

static TmdSource _gActor135600KyleMadiganHandLeft = {
    0,
    1328,
    0,
    1,
    _gActor135600KyleMadiganHandLeftPartVerts,
    _gActor135600KyleMadiganHandLeftVerts,
    _gActor135600KyleMadiganHandLeftNormals,
    _gActor135600KyleMadiganHandLeftSkeleton,
    _gActor135600KyleMadiganHandLeftStream,
};

static TmdBone _gActor135600Model06AC4Skeleton[1] = {
#include "assets/actor_135600_model_06AC4_skeleton.inc"
};

static u32 _gActor135600Model06AC4PartVerts[1] = {
#include "assets/actor_135600_model_06AC4_partVerts.inc"
};

static SVECTOR _gActor135600Model06AC4Verts[24] = {
#include "assets/actor_135600_model_06AC4_verts.inc"
};

static u32 _gActor135600Model06AC4Stream[129] = {
#include "assets/actor_135600_model_06AC4_stream.inc"
};

static TmdSource _gActor135600Model06AC4 = {
    0,
    928,
    0,
    1,
    _gActor135600Model06AC4PartVerts,
    _gActor135600Model06AC4Verts,
    &_gActor135600Model06AC4Verts[24],
    _gActor135600Model06AC4Skeleton,
    _gActor135600Model06AC4Stream,
};

static AnimationPackedPose _gActor135600Animation06F40Bank1[2] = {
#include "assets/actor_135600_animation_06F40_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation06F40Bank4[32] = {
#include "assets/actor_135600_animation_06F40_bank4.inc"
};

static AnimationRecord _gActor135600Animation06F40Records[101] = {
#include "assets/actor_135600_animation_06F40_records.inc"
};

static u16 _gActor135600Animation06F40Indices[20] = {
#include "assets/actor_135600_animation_06F40_indices.inc"
};

static AnimationSet _gActor135600Animation06F40 = {
    _gActor135600Animation06F40Records,
    _gActor135600Animation06F40Indices,
    { NULL, _gActor135600Animation06F40Bank1, NULL, NULL, _gActor135600Animation06F40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation071F4Bank1[2] = {
#include "assets/actor_135600_animation_071F4_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation071F4Bank4[39] = {
#include "assets/actor_135600_animation_071F4_bank4.inc"
};

static AnimationRecord _gActor135600Animation071F4Records[108] = {
#include "assets/actor_135600_animation_071F4_records.inc"
};

static u16 _gActor135600Animation071F4Indices[20] = {
#include "assets/actor_135600_animation_071F4_indices.inc"
};

static AnimationSet _gActor135600Animation071F4 = {
    _gActor135600Animation071F4Records,
    _gActor135600Animation071F4Indices,
    { NULL, _gActor135600Animation071F4Bank1, NULL, NULL, _gActor135600Animation071F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation07588Bank1[6] = {
#include "assets/actor_135600_animation_07588_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation07588Bank4[77] = {
#include "assets/actor_135600_animation_07588_bank4.inc"
};

static AnimationRecord _gActor135600Animation07588Records[114] = {
#include "assets/actor_135600_animation_07588_records.inc"
};

static u16 _gActor135600Animation07588Indices[20] = {
#include "assets/actor_135600_animation_07588_indices.inc"
};

static AnimationSet _gActor135600Animation07588 = {
    _gActor135600Animation07588Records,
    _gActor135600Animation07588Indices,
    { NULL, _gActor135600Animation07588Bank1, NULL, NULL, _gActor135600Animation07588Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation077ACBank1[2] = {
#include "assets/actor_135600_animation_077AC_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation077ACBank4[33] = {
#include "assets/actor_135600_animation_077AC_bank4.inc"
};

static AnimationRecord _gActor135600Animation077ACRecords[78] = {
#include "assets/actor_135600_animation_077AC_records.inc"
};

static u16 _gActor135600Animation077ACIndices[20] = {
#include "assets/actor_135600_animation_077AC_indices.inc"
};

static AnimationSet _gActor135600Animation077AC = {
    _gActor135600Animation077ACRecords,
    _gActor135600Animation077ACIndices,
    { NULL, _gActor135600Animation077ACBank1, NULL, NULL, _gActor135600Animation077ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation07B08Bank1[2] = {
#include "assets/actor_135600_animation_07B08_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation07B08Bank4[74] = {
#include "assets/actor_135600_animation_07B08_bank4.inc"
};

static AnimationRecord _gActor135600Animation07B08Records[115] = {
#include "assets/actor_135600_animation_07B08_records.inc"
};

static u16 _gActor135600Animation07B08Indices[20] = {
#include "assets/actor_135600_animation_07B08_indices.inc"
};

static AnimationSet _gActor135600Animation07B08 = {
    _gActor135600Animation07B08Records,
    _gActor135600Animation07B08Indices,
    { NULL, _gActor135600Animation07B08Bank1, NULL, NULL, _gActor135600Animation07B08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation07DE4Bank1[5] = {
#include "assets/actor_135600_animation_07DE4_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation07DE4Bank4[58] = {
#include "assets/actor_135600_animation_07DE4_bank4.inc"
};

static AnimationRecord _gActor135600Animation07DE4Records[90] = {
#include "assets/actor_135600_animation_07DE4_records.inc"
};

static u16 _gActor135600Animation07DE4Indices[20] = {
#include "assets/actor_135600_animation_07DE4_indices.inc"
};

static AnimationSet _gActor135600Animation07DE4 = {
    _gActor135600Animation07DE4Records,
    _gActor135600Animation07DE4Indices,
    { NULL, _gActor135600Animation07DE4Bank1, NULL, NULL, _gActor135600Animation07DE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation07FB0Bank1[2] = {
#include "assets/actor_135600_animation_07FB0_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation07FB0Bank4[29] = {
#include "assets/actor_135600_animation_07FB0_bank4.inc"
};

static AnimationRecord _gActor135600Animation07FB0Records[60] = {
#include "assets/actor_135600_animation_07FB0_records.inc"
};

static u16 _gActor135600Animation07FB0Indices[20] = {
#include "assets/actor_135600_animation_07FB0_indices.inc"
};

static AnimationSet _gActor135600Animation07FB0 = {
    _gActor135600Animation07FB0Records,
    _gActor135600Animation07FB0Indices,
    { NULL, _gActor135600Animation07FB0Bank1, NULL, NULL, _gActor135600Animation07FB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation0817CBank1[2] = {
#include "assets/actor_135600_animation_0817C_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation0817CBank4[29] = {
#include "assets/actor_135600_animation_0817C_bank4.inc"
};

static AnimationRecord _gActor135600Animation0817CRecords[60] = {
#include "assets/actor_135600_animation_0817C_records.inc"
};

static u16 _gActor135600Animation0817CIndices[20] = {
#include "assets/actor_135600_animation_0817C_indices.inc"
};

static AnimationSet _gActor135600Animation0817C = {
    _gActor135600Animation0817CRecords,
    _gActor135600Animation0817CIndices,
    { NULL, _gActor135600Animation0817CBank1, NULL, NULL, _gActor135600Animation0817CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation08364Bank1[3] = {
#include "assets/actor_135600_animation_08364_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation08364Bank4[33] = {
#include "assets/actor_135600_animation_08364_bank4.inc"
};

static AnimationRecord _gActor135600Animation08364Records[60] = {
#include "assets/actor_135600_animation_08364_records.inc"
};

static u16 _gActor135600Animation08364Indices[20] = {
#include "assets/actor_135600_animation_08364_indices.inc"
};

static AnimationSet _gActor135600Animation08364 = {
    _gActor135600Animation08364Records,
    _gActor135600Animation08364Indices,
    { NULL, _gActor135600Animation08364Bank1, NULL, NULL, _gActor135600Animation08364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation08644Bank1[2] = {
#include "assets/actor_135600_animation_08644_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation08644Bank4[54] = {
#include "assets/actor_135600_animation_08644_bank4.inc"
};

static AnimationRecord _gActor135600Animation08644Records[104] = {
#include "assets/actor_135600_animation_08644_records.inc"
};

static u16 _gActor135600Animation08644Indices[20] = {
#include "assets/actor_135600_animation_08644_indices.inc"
};

static AnimationSet _gActor135600Animation08644 = {
    _gActor135600Animation08644Records,
    _gActor135600Animation08644Indices,
    { NULL, _gActor135600Animation08644Bank1, NULL, NULL, _gActor135600Animation08644Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation089E4Bank1[2] = {
#include "assets/actor_135600_animation_089E4_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation089E4Bank4[71] = {
#include "assets/actor_135600_animation_089E4_bank4.inc"
};

static AnimationRecord _gActor135600Animation089E4Records[135] = {
#include "assets/actor_135600_animation_089E4_records.inc"
};

static u16 _gActor135600Animation089E4Indices[20] = {
#include "assets/actor_135600_animation_089E4_indices.inc"
};

static AnimationSet _gActor135600Animation089E4 = {
    _gActor135600Animation089E4Records,
    _gActor135600Animation089E4Indices,
    { NULL, _gActor135600Animation089E4Bank1, NULL, NULL, _gActor135600Animation089E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation08C00Bank1[2] = {
#include "assets/actor_135600_animation_08C00_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation08C00Bank4[23] = {
#include "assets/actor_135600_animation_08C00_bank4.inc"
};

static AnimationRecord _gActor135600Animation08C00Records[86] = {
#include "assets/actor_135600_animation_08C00_records.inc"
};

static u16 _gActor135600Animation08C00Indices[20] = {
#include "assets/actor_135600_animation_08C00_indices.inc"
};

static AnimationSet _gActor135600Animation08C00 = {
    _gActor135600Animation08C00Records,
    _gActor135600Animation08C00Indices,
    { NULL, _gActor135600Animation08C00Bank1, NULL, NULL, _gActor135600Animation08C00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation08DF4Bank1[2] = {
#include "assets/actor_135600_animation_08DF4_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation08DF4Bank4[18] = {
#include "assets/actor_135600_animation_08DF4_bank4.inc"
};

static AnimationRecord _gActor135600Animation08DF4Records[81] = {
#include "assets/actor_135600_animation_08DF4_records.inc"
};

static u16 _gActor135600Animation08DF4Indices[20] = {
#include "assets/actor_135600_animation_08DF4_indices.inc"
};

static AnimationSet _gActor135600Animation08DF4 = {
    _gActor135600Animation08DF4Records,
    _gActor135600Animation08DF4Indices,
    { NULL, _gActor135600Animation08DF4Bank1, NULL, NULL, _gActor135600Animation08DF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation08FF8Bank1[2] = {
#include "assets/actor_135600_animation_08FF8_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation08FF8Bank4[20] = {
#include "assets/actor_135600_animation_08FF8_bank4.inc"
};

static AnimationRecord _gActor135600Animation08FF8Records[83] = {
#include "assets/actor_135600_animation_08FF8_records.inc"
};

static u16 _gActor135600Animation08FF8Indices[20] = {
#include "assets/actor_135600_animation_08FF8_indices.inc"
};

static AnimationSet _gActor135600Animation08FF8 = {
    _gActor135600Animation08FF8Records,
    _gActor135600Animation08FF8Indices,
    { NULL, _gActor135600Animation08FF8Bank1, NULL, NULL, _gActor135600Animation08FF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135600Animation09218Bank1[2] = {
#include "assets/actor_135600_animation_09218_bank1.inc"
};

static AnimationPackedRotation _gActor135600Animation09218Bank4[30] = {
#include "assets/actor_135600_animation_09218_bank4.inc"
};

static AnimationRecord _gActor135600Animation09218Records[80] = {
#include "assets/actor_135600_animation_09218_records.inc"
};

static u16 _gActor135600Animation09218Indices[20] = {
#include "assets/actor_135600_animation_09218_indices.inc"
};

static AnimationSet _gActor135600Animation09218 = {
    _gActor135600Animation09218Records,
    _gActor135600Animation09218Indices,
    { NULL, _gActor135600Animation09218Bank1, NULL, NULL, _gActor135600Animation09218Bank4, NULL, NULL, NULL },
};

SVECTOR D_actor_135600_8013B060[4] = {
    { -4, 0, 0, 0 },
    { 4, 0, 0, 0 },
    { -3, 0, 0, 0 },
    { 3, 0, 0, 0 },
};

AnimationSet* D_actor_135600_8013B080[16] = {
    NULL,
    &_gActor135600Animation06F40,
    &_gActor135600Animation071F4,
    &_gActor135600Animation07588,
    &_gActor135600Animation077AC,
    &_gActor135600Animation07B08,
    &_gActor135600Animation07DE4,
    &_gActor135600Animation07FB0,
    &_gActor135600Animation0817C,
    &_gActor135600Animation08364,
    &_gActor135600Animation08644,
    &_gActor135600Animation089E4,
    &_gActor135600Animation08C00,
    &_gActor135600Animation08DF4,
    &_gActor135600Animation08FF8,
    &_gActor135600Animation09218,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_135600_8013B080,
};

TaskDesc D_actor_135600_8013B0C4[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_135600_80132D64, { .model = &_gActor135600KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_135600_801329E0, { .model = &_gActor135600KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_135600_801329E0, { .model = &_gActor135600KyleMadiganHandRight } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_135600_80132ABC, { .model = &_gActor135600Model06AC4 } },
};

TaskMessageEntry D_actor_135600_8013B0F4[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_135600_80133240 },
    { ACTOR_MESSAGE_WALK_TO, actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_135600_8013336C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static s32 func_actor_135600_80131E68(GfxCoord* coord, s32 arg1);

/// Recomputes `coord`'s world matrix (`actorRenderComposeCoord`), composes its parent
/// chain, then projects two offsets along the part's local Z - the near one 10 units
/// out and the far one `arg1 * 0x46 / 0x1000 + 10`, so the pair opens by 70
/// 4096ths of a unit per tick - and returns the signed `ratan2` of the
/// difference between the two projections, the actor's screen-space angle.
/// The pair is drawn as the quad `D_actor_135600_8013B060` describes: the wide
/// vertex pair rotated about the screen origin by that angle and anchored on
/// the near projection, the narrow pair unrotated on the far one, as a
/// semi-transparent `POLY_F4` followed by its texture page, both linked into
/// the ordering table at the far point's depth. Nothing is drawn when that
/// depth is behind the camera. The marker's draw state passes the countdown it
/// runs on as `arg1`.
static s32 func_actor_135600_80131E68(GfxCoord* coord, s32 arg1)
{
    SVECTOR   v0;
    SVECTOR   v1;
    SVECTOR   pos;
    SVECTOR   quad[4];
    GfxMatrix m;
    MATRIX*   mtx;
    long      sxy0;
    long      p;
    long      flag;
    long      sxy1;
    s16       y0;
    s16       y1;
    s32       rot;
    u16       x0;
    u16       x1;
    s32       depth;
    POLY_F4*  poly;
    DR_TPAGE* tpage;
    s32       i;

    actorRenderComposeCoord(coord);
    mtx = &m.mat;
    func_actor_135600_80132C80(coord, &m.mat, &pos);

    v0.vx = 0;
    v0.vy = 0;
    v0.vz = 0xA;
    ApplyMatrixSV(&m.mat, &v0, &v0);

    v1.vx = 0;
    v1.vy = 0;
    v1.vz = arg1 * 0x46 / 0x1000 + 0xA;
    ApplyMatrixSV(&m.mat, &v1, &v1);

    v0.vx += pos.vx;
    v0.vy += pos.vy;
    v0.vz += pos.vz;
    v1.vx += pos.vx;
    v1.vy += pos.vy;
    v1.vz += pos.vz;

    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);

    RotTransPers(&v0, &sxy0, &p, &flag);
    depth = RotTransPers(&v1, &sxy1, &p, &flag);

    x0  = (u16)sxy0;
    x1  = (u16)sxy1;
    y0  = sxy0 >> 16;
    y1  = sxy1 >> 16;
    rot = ratan2((s16)sxy1 - (s16)sxy0, y0 - y1);

    /* Only the middle diagonal and the last entry go through `mtx`: a store
     * written that way keeps its address in the register `RotMatrixZ` is
     * handed, where the ones naming `m` directly fold to a frame-relative
     * address, and the target has both. */
    m.rotationWords.m00M01    = ONE;
    MATRIX_PAIR(&m.mat, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1)    = 0x1000;
    MATRIX_PAIR(&m.mat, 2, 0) = 0;
    mtx->m[2][2]              = 0x1000;
    RotMatrixZ(rot, &m.mat);

    for (i = 0; i < 2; i++) {
        ApplyMatrixSV(&m.mat, &D_actor_135600_8013B060[i], &quad[i]);
        quad[i].vx    += x0;
        quad[i].vy    += y0;
        quad[i + 2].vx = D_actor_135600_8013B060[i + 2].vx + x1;
        quad[i + 2].vy = D_actor_135600_8013B060[i + 2].vy + y1;
    }

    if (p >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        setRGB0(poly, 0xFF, 0x40, 0);
        poly->x0 = quad[0].vx;
        poly->y0 = quad[0].vy;
        poly->x1 = quad[1].vx;
        poly->y1 = quad[1].vy;
        poly->x2 = quad[2].vx;
        poly->y2 = quad[2].vy;
        poly->x3 = quad[3].vx;
        poly->y3 = quad[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, poly);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, tpage);
    }
    return rot;
}

/// Setup state of the actor (entry 0 of `D_actor_135600_80131E3C`). It
/// allocates the 0x50C-byte work block, seeds the -1 sentinels and spawns
/// entries 1 to 3 of `D_actor_135600_8013B0C4` -- the two part models get the
/// texture page and CLUT of the area record the actor's placement key resolves
/// to, and all three are parked in the work block. It then points the model at
/// the work block's light/colour pair, places it at (0xA6E, 0, 0x5F0) yawed
/// 0x400, applies animation 2, shows it (message 0x7D5 mode 1), publishes the
/// message table `D_actor_135600_8013B0F4`, installs the exit callback and
/// steps to the tick state.
static void func_actor_135600_80132234(Task* task)
{
    KyleMadiganWalkerWork* work;
    Task*                  spawned;
    ActorTransform         args;
    AnimationPlayRequest   preset;

    work = memCalloc(sizeof(KyleMadiganWalkerWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 1, 8, task);
    if (spawned != NULL) {
        work->handTasks[1] = spawned;
        actorTintModel(spawned->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 2, 0xC, task);
    if (spawned != NULL) {
        work->handTasks[0] = spawned;
        actorTintModel(spawned->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 3, 8, task);
    if (spawned != NULL) {
        work->heldItemTask = spawned;
    }

    func_actor_135600_80132DDC(task);

    args.pos.vx = 0xA6E;
    args.pos.vz = 0x5F0;
    args.pos.vy = 0;
    args.rot.vx = 0;
    args.rot.vy = 0x400;
    args.rot.vz = 0;
    actorMsgPlaceEuler(task, ACTOR_MESSAGE_PLACE, &args, 0);

    preset.source.index = 0;
    preset.animationId  = 2;
    preset.blend        = ANIMATION_BLEND_RESET;
    actorMotionPlayAnim(task, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);

    func_actor_135600_80133240(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);

    task->msgTable     = D_actor_135600_8013B0F4;
    task->exitCallback = func_actor_135600_80132DBC;
    task->state       += 1;
}

/// Per-frame tick of the actor (entry 1 of `D_actor_135600_80131E3C`).
/// Draws the ground shadow under the second part unless the model is hidden,
/// then -- only while `gSceneCombatState.actorControl` is clear -- runs the handler `walk.motion`
/// selects, advances the root coordinate by the high halves of the 16.16
/// accumulators fed from `velocity` (re-zeroing each high half), ticks slots 1 to
/// 19 while `model.ticking` is set, rebuilds the second part's coordinate and the
/// actor colour while `gGameSession->viewReady` is set, and counts `freeCountdown`
/// down while non-negative, freeing the model's buffers when it reaches zero.
static void func_actor_135600_801324D0(Task* arg0)
{
    TmdObject*             ext      = arg0->extra.tmd;
    KyleMadiganWalkerWork* work     = arg0->work;
    TaskFunc               funcs[2] = { func_actor_135600_80132DF8, func_actor_135600_80132E00 };
    VECTOR3                pos;
    GfxCoord*              coord;
    s32                    i;

    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
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
            for (i = 1; i < 0x14; i++) {
                animationTickSlot(&work->rig.anim, i);
            }
        }
        if (gGameSession->viewReady != 0) {
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
            func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                tmdFreePrimitiveBuffer(ext);
            }
            work->freeCountdown--;
        }
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// Dispatcher of the two part tasks: runs their state from
/// `D_actor_135600_80131E24`.
void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Tick state of a part task: nothing to do, the parent drives it.
static void func_actor_135600_80132AB4(Task* task)
{
}

/// Dispatcher of the marker task: runs its state from
/// `D_actor_135600_80131E30`.
void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

/// Setup state of the marker task (entry 0 of `D_actor_135600_80131E30`):
/// chains its model root under the parent task's part `spawnArg1`,
/// places the part's coordinate at (-150, 80, 0), turns its rotation by 90
/// degrees about Y, inherits the parent's light and colour matrices, and
/// reparents the task so it is updated with the parent. The kill countdown is
/// set to 0x1000, the value the marker's draw state runs on.
static void func_actor_135600_80132B14(Task* task)
{
    GfxMatrix  m;
    MATRIX*    mtx;
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent      = (Task*)task->spawnArg2.pointer;
    extra       = task->extra.tmd;
    part        = task->spawnArg1.value;
    parentExtra = parent->extra.tmd;
    coord       = extra->coords;
    dest        = &parentExtra->coords[part];

    coord->coord.t[0] = -0x96;
    coord->coord.t[1] = 0x50;
    coord->coord.t[2] = 0;

    mtx                    = &m.mat;
    m.rotationWords.m00M01 = ONE;
    MATRIX_PAIR(mtx, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1) = 0x1000;
    MATRIX_PAIR(mtx, 2, 0) = 0;
    mtx->m[2][2]           = 0x1000;

    RotMatrixY((s16)(0x400), mtx);
    MulMatrix0(&coord->coord, mtx, &coord->coord);

    coord->parent       = dest;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    extra->otOffset     = 0;
    taskReparent(parent, task);
    task->killCountdown = 0x1000;
    task->state        += 1;
}

/// Draw state of the marker task: while `gGameSession->eventState` is set and
/// the kill countdown is still running, draws the marker at the countdown's
/// length, cutting the countdown to 0x800 once the marker's angle reaches
/// 0x1F5.
static void func_actor_135600_80132C18(Task* task)
{
    s16 countdown;

    if (gGameSession->eventState != 0) {
        countdown = task->killCountdown;
        if (countdown > 0 && func_actor_135600_80131E68(task->extra.tmd->coords, countdown) >= 0x1F5) {
            task->killCountdown = 0x800;
        }
    }
}

/// Walks `coord->parent` up to world (`gGfxViewCoord`), composing each node's
/// `coord` rotation into `mtx` and accumulating the rotated translation into
/// `vec`. The world parent initializes `mtx` to identity and `vec` to zero.
/// The same algorithm as gameplay's `Gp_ComposeParentWorld`, but through the
/// library `ApplyMatrixSV` / `MulMatrix0` rather than the GTE macros.
static void func_actor_135600_80132C80(GfxCoord* coord, MATRIX* mtx, SVECTOR* vec)
{
    SVECTOR tmp;
    MATRIX* m;

    if (coord->parent != &gGfxViewCoord) {
        func_actor_135600_80132C80(coord->parent, mtx, vec);
    } else {
        m                    = mtx;
        *(s32*)m             = 0x1000;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = 0x1000;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = 0x1000;
        vec->vx              = 0;
        vec->vy              = 0;
        vec->vz              = 0;
    }

    tmp.vx = (u16)coord->coord.t[0];
    tmp.vy = (u16)coord->coord.t[1];
    tmp.vz = (u16)coord->coord.t[2];
    ApplyMatrixSV(mtx, &tmp, &tmp);
    vec->vx += tmp.vx;
    vec->vy += tmp.vy;
    vec->vz += tmp.vz;
    MulMatrix0(mtx, &coord->coord, mtx);
}

/// Dispatcher of the actor itself: runs its state from
/// `D_actor_135600_80131E3C`.
void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

/// Exit state and exit callback of the actor: the `enemyTaskExit` teardown.
static void func_actor_135600_80132DBC(Task* task)
{
    enemyTaskExit(task);
}

/// Points the model's light and colour matrices at the work block's own pair.
static void func_actor_135600_80132DDC(Task* task)
{
    TmdObject*             ext;
    KyleMadiganWalkerWork* work;

    ext           = task->extra.tmd;
    work          = task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Entry 0 of the tick's handler pair, selected by `walk.motion` while no motion
/// sequence runs: does nothing.
static void func_actor_135600_80132DF8(Task* arg0)
{
}

/// Entry 1 of the tick's handler pair: runs the step of
/// `D_actor_135600_80131E48` that `walk.motionStep` selects.
static void func_actor_135600_80132E00(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = D_actor_135600_80131E48;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1: rotates the forward offset `D_actor_135600_80131E58` through the
/// root part's matrix into `work->walk.velocity`, seeds `walk.lastDistance` with
/// `ACTOR_WALK_DISTANCE_NONE` and advances the step.
static void func_actor_135600_80132F28(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              coord;
    VECTOR                 vec;

    coord = task->extra.tmd->coords;
    work  = task->work;

    vec = D_actor_135600_80131E58;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`, the actor's visibility,
/// switched on the word `mode`. Flag 0x80 hides the model (the tick skips the
/// shadow while it is set). Mode 0 hides the model and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`, 1 shows
/// it, allocates its buffers and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`, 2 hides it, sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` and starts the
/// `freeCountdown` countdown at 2, and 3 shows it while setting `TMD_OBJECT_SKIP_AUTO_BUFFER`. Anything else
/// returns 1 and leaves the flags alone; the handled modes return 0. Either
/// way the resulting flags are copied onto the objects of the three tasks the
/// setup state parked at `handTasks` and `heldItemTask`.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    KyleMadiganWalkerWork* work;
    TmdObject*             obj;
    TmdObject*             objA;
    TmdObject*             objB;
    TmdObject*             objC;
    s32                    ret;

    work = task->work;
    obj  = task->extra.tmd;
    objB = work->handTasks[1]->extra.tmd;
    objA = work->handTasks[0]->extra.tmd;
    objC = work->heldItemTask->extra.tmd;
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
    objB->flags = obj->flags;
    objA->flags = obj->flags;
    objC->flags = obj->flags;
    return ret;
}

/// The 0x7DB entry of `D_actor_135600_8013B0F4`: accepts the message and does
/// nothing with it.
s32 func_actor_135600_8013336C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}
