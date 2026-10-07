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

static void _modelPlacementAttachPartTask(Task* childTask);
static void func_actor_135600_80132234(Task* task);
static s32  _actor135600DrawHeldItemQuad(GfxCoord* itemRoot, s32 lengthScale12);
static void _actor135600UpdateKyleMadiganWalker(Task* task);
static void _actor135600IdleKyleMadiganHand(Task* handTask);
static void _actor135600AttachKyleMadiganHeldItem(Task* itemTask);
static void _actor135600UpdateKyleMadiganHeldItem(Task* itemTask);
static void _actor135600ComposeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation);
static void _actor135600ExitKyleMadiganWalker(Task* task);
static void _actor135600BindKyleMadiganWalkerLighting(Task* task);
static void _actor135600IdleKyleMadiganWalk(Task* task);
static void _actor135600RunKyleMadiganWalkStep(Task* task);
static void _actor135600BeginKyleMadiganWalk(Task* task);
static s32  _actor135600SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32  _actor135600IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void _actor135600KyleMadiganHandTask(Task* handTask);
static void _actor135600KyleMadiganHeldItemTask(Task* itemTask);
static void _actor135600KyleMadiganWalkerTask(Task* task);

/// The held-item quad's Q12 length scale and signed screen-angle transition.
enum {
    ACTOR_135600_HELD_QUAD_FULL_LENGTH   = ONE,
    ACTOR_135600_HELD_QUAD_HALF_LENGTH   = ONE / 2,
    ACTOR_135600_HELD_QUAD_SHORTEN_ANGLE = 0x1F5, // 4096 units per turn
};

/// States of the two part tasks (`D_actor_135600_8013B0C4` entries 1 and 2),
/// dispatched by `_actor135600KyleMadiganHandTask`: attach to the parent, idle,
/// kill.
static const TaskFuncTable3 D_actor_135600_80131E24 = { {
    _modelPlacementAttachPartTask,
    _actor135600IdleKyleMadiganHand,
    taskKill,
} };

/// States of the marker task (entry 3), dispatched by
/// `_actor135600KyleMadiganHeldItemTask`: attach to the parent with an offset, draw the
/// marker, kill.
static const TaskFuncTable3 D_actor_135600_80131E30 = { {
    _actor135600AttachKyleMadiganHeldItem,
    _actor135600UpdateKyleMadiganHeldItem,
    taskKill,
} };

/// States of the actor itself (entry 0), dispatched by
/// `_actor135600KyleMadiganWalkerTask`: setup, per-frame tick, exit.
static const TaskFuncTable3 D_actor_135600_80131E3C = { {
    func_actor_135600_80132234,
    _actor135600UpdateKyleMadiganWalker,
    _actor135600ExitKyleMadiganWalker,
} };

/// Step handlers of the motion sequence, indexed by
/// `KyleMadiganWalkerWork::walk.motionStep`: turn to face `target`, start walking forward,
/// walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_135600_80131E48 = { {
    _actorMotionFaceTarget,
    _actor135600BeginKyleMadiganWalk,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

/// The constant local-space offset `_actor135600BeginKyleMadiganWalk` rotates:
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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor135600KyleMadiganWalkerTask, { .model = &_gActor135600KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor135600KyleMadiganHandTask, { .model = &_gActor135600KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, _actor135600KyleMadiganHandTask, { .model = &_gActor135600KyleMadiganHandRight } },
    { { { TASK_BODY_TMD, 192 } }, _actor135600KyleMadiganHeldItemTask, { .model = &_gActor135600Model06AC4 } },
};

TaskMessageEntry D_actor_135600_8013B0F4[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor135600SetKyleMadiganDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor135600IgnoreKyleMadiganCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Draws the orange quad ahead of the held model and returns its signed screen angle.
///
/// `itemRoot` must have an acyclic parent chain ending at `gGfxViewCoord`.
/// `lengthScale12` is a Q12 length multiplier (4096 full length, 2048 half),
/// placing the endpoints at local Z = 10 and 10 + 70 * lengthScale12 / 4096.
/// World positions narrow to signed halfwords. The returned angle has 4096
/// units per turn, measured from screen up toward screen right.
/// Requires room for a `POLY_F4` and `DR_TPAGE` in the current frame arena and
/// an ordering-table tag two entries before the far point's masked depth tag.
/// Emits packets only when the far projection's depth-cue output is nonnegative;
/// its GTE flags are ignored. Packets remain borrowed until the frame is drawn.
static s32 _actor135600DrawHeldItemQuad(GfxCoord* itemRoot, s32 lengthScale12)
{
    enum {
        ACTOR_135600_HELD_QUAD_ORIGIN_Z    = 10,
        ACTOR_135600_HELD_QUAD_FULL_SPAN   = 70,
        ACTOR_135600_HELD_QUAD_PACKET_CODE = 0x2A, // Semi-transparent flat quad
    };
    SVECTOR   nearWorldPoint;
    SVECTOR   farWorldPoint;
    SVECTOR   itemWorldPosition;
    SVECTOR   screenCorners[4];
    GfxMatrix matrix;
    MATRIX*   rotationMatrix;
    long      nearScreenXY;
    long      depthCue;
    long      projectionFlags;
    long      farScreenXY;
    s16       nearScreenY;
    s16       farScreenY;
    s32       screenAngle;
    u16       nearScreenX;
    u16       farScreenX;
    s32       orderingDepth;
    POLY_F4*  quadPacket;
    DR_TPAGE* drawModePacket;
    s32       pairIndex;

    // Project two points on the held model's forward axis into the current view.
    actorRenderComposeCoord(itemRoot);
    rotationMatrix = &matrix.mat;
    _actor135600ComposeWorldTransform(itemRoot, &matrix.mat, &itemWorldPosition);

    nearWorldPoint.vx = 0;
    nearWorldPoint.vy = 0;
    nearWorldPoint.vz = ACTOR_135600_HELD_QUAD_ORIGIN_Z;
    ApplyMatrixSV(&matrix.mat, &nearWorldPoint, &nearWorldPoint);

    farWorldPoint.vx = 0;
    farWorldPoint.vy = 0;
    farWorldPoint.vz = lengthScale12 * ACTOR_135600_HELD_QUAD_FULL_SPAN / ONE + ACTOR_135600_HELD_QUAD_ORIGIN_Z;
    ApplyMatrixSV(&matrix.mat, &farWorldPoint, &farWorldPoint);

    nearWorldPoint.vx += itemWorldPosition.vx;
    nearWorldPoint.vy += itemWorldPosition.vy;
    nearWorldPoint.vz += itemWorldPosition.vz;
    farWorldPoint.vx  += itemWorldPosition.vx;
    farWorldPoint.vy  += itemWorldPosition.vy;
    farWorldPoint.vz  += itemWorldPosition.vz;

    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);

    RotTransPers(&nearWorldPoint, &nearScreenXY, &depthCue, &projectionFlags);
    orderingDepth = RotTransPers(&farWorldPoint, &farScreenXY, &depthCue, &projectionFlags);

    nearScreenX = (u16)nearScreenXY;
    farScreenX  = (u16)farScreenXY;
    nearScreenY = nearScreenXY >> 16;
    farScreenY  = farScreenXY >> 16;
    screenAngle = ratan2((s16)farScreenXY - (s16)nearScreenXY, nearScreenY - farScreenY);

    // Rotate the near vertex pair in screen space; the far pair stays horizontal.
    /// Builds a pure Z rotation in `matrix` (angle in 4096 units per turn).
    ///
    /// Captures the local `matrix` and its `rotationMatrix` alias; writes only
    /// the nine rotation coefficients. Evaluates the angle once. Use as a
    /// standalone statement; this binding is undefined immediately after use.
#define ACTOR_135600_BUILD_HELD_QUAD_ROTATION(angle) \
    {                                                \
        matrix.rotationWords.m00M01       = ONE;     \
        MATRIX_PAIR(&matrix.mat, 0, 2)    = 0;       \
        MATRIX_PAIR(rotationMatrix, 1, 1) = ONE;     \
        MATRIX_PAIR(&matrix.mat, 2, 0)    = 0;       \
        rotationMatrix->m[2][2]           = ONE;     \
        RotMatrixZ((angle), &matrix.mat);            \
    }
    ACTOR_135600_BUILD_HELD_QUAD_ROTATION(screenAngle);
#undef ACTOR_135600_BUILD_HELD_QUAD_ROTATION

    for (pairIndex = 0; pairIndex < (s32)ARRAY_SIZE(screenCorners) / 2; pairIndex++) {
        ApplyMatrixSV(&matrix.mat, &D_actor_135600_8013B060[pairIndex], &screenCorners[pairIndex]);
        screenCorners[pairIndex].vx    += nearScreenX;
        screenCorners[pairIndex].vy    += nearScreenY;
        screenCorners[pairIndex + 2].vx = D_actor_135600_8013B060[pairIndex + 2].vx + farScreenX;
        screenCorners[pairIndex + 2].vy = D_actor_135600_8013B060[pairIndex + 2].vy + farScreenY;
    }

    if (depthCue >= 0) {
        quadPacket     = gGpuPrimCursor;
        gGpuPrimCursor = quadPacket + 1;
        setlen(quadPacket, sizeof(*quadPacket) / sizeof(u32) - 1);
        setcode(quadPacket, ACTOR_135600_HELD_QUAD_PACKET_CODE);
        setRGB0(quadPacket, 0xFF, 0x40, 0);
        quadPacket->x0 = screenCorners[0].vx;
        quadPacket->y0 = screenCorners[0].vy;
        quadPacket->x1 = screenCorners[1].vx;
        quadPacket->y1 = screenCorners[1].vy;
        quadPacket->x2 = screenCorners[2].vx;
        quadPacket->y2 = screenCorners[2].vy;
        quadPacket->x3 = screenCorners[3].vx;
        quadPacket->y3 = screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, quadPacket);

        // addPrim prepends: the draw mode must reach the GPU before the quad.
        drawModePacket = gGpuPrimCursor;
        gGpuPrimCursor = drawModePacket + 1;
        setlen(drawModePacket, sizeof(*drawModePacket) / sizeof(u32) - 1);
        // Quarter-source additive blending, drawing in the display area, no dithering.
        drawModePacket->code[0] = _get_mode(1, 0, getTPage(0, 3, 320, 0));
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, drawModePacket);
    }
    return screenAngle;
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

    spawned = taskSpawnFromTable(D_actor_135600_8013B0C4, 1, 8, task);
    if (spawned != NULL) {
        work->handTasks[1] = spawned;
        actorTintModel(spawned->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned = taskSpawnFromTable(D_actor_135600_8013B0C4, 2, 0xC, task);
    if (spawned != NULL) {
        work->handTasks[0] = spawned;
        actorTintModel(spawned->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned = taskSpawnFromTable(D_actor_135600_8013B0C4, 3, 8, task);
    if (spawned != NULL) {
        work->heldItemTask = spawned;
    }

    _actor135600BindKyleMadiganWalkerLighting(task);

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
    _actorMotionPlayAnim(task, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);

    _actor135600SetKyleMadiganDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);

    task->msgTable     = D_actor_135600_8013B0F4;
    task->exitCallback = _actor135600ExitKyleMadiganWalker;
    task->state       += 1;
}

/// Applies one frame's signed 16.16 walk velocity, retaining the fractional carry.
///
/// `work` and `rootCoord` must be live and writable. Invalidates the root's
/// cached composition even for zero velocity; integer additions retain the
/// target's 32-bit arithmetic and the remaining fractions are zero-extended.
static inline void _actor135600IntegrateWalkVelocity(KyleMadiganWalkerWork* work, GfxCoord* rootCoord)
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

/// Updates Kyle's scripted walk, twenty-slot animation rig, lighting and buffer release.
///
/// Requires a live TMD body and `KyleMadiganWalkerWork`; `walk.motion` is 0
/// (idle) or 1 (walking). A visible body's ground shadow is drawn even while
/// actor updates are frozen. Motion, slots 1..19, view-ready lighting and the
/// buffer-release counter advance only while scene actors are running.
/// The update finding `freeCountdown == 0` frees the body primitive buffer,
/// then decrements the counter to its disabled value of -1.
static void _actor135600UpdateKyleMadiganWalker(Task* task)
{
    enum { ACTOR_135600_KYLE_SHADOW_HALF_SIZE = 0x300 }; // World-coordinate units
    TmdObject*             bodyModel         = task->extra.tmd;
    KyleMadiganWalkerWork* work              = task->work;
    TaskFunc               motionHandlers[2] = { _actor135600IdleKyleMadiganWalk, _actor135600RunKyleMadiganWalkStep };
    VECTOR3                groundPosition;
    GfxCoord*              rootCoord;
    s32                    slotIndex;

    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_135600_KYLE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        motionHandlers[work->walk.motion](task);
        rootCoord = task->extra.tmd->coords;
        _actor135600IntegrateWalkVelocity(work, rootCoord);
        if (work->model.ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationTickSlot(&work->rig.anim, slotIndex);
            }
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                tmdFreePrimitiveBuffer(bodyModel);
            }
            work->freeCountdown--;
        }
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// Runs a Kyle hand model's attachment, idle or teardown state.
///
/// Requires a live TMD task with state 0..2. Setup borrows the parent task
/// from `spawnArg2.pointer` and its part index from `spawnArg1.value` (8 for
/// the left hand, 12 for the right); both parent model and lighting must outlive
/// the attached hand. The body rig drives the hand's inherited transform.
static void _actor135600KyleMadiganHandTask(Task* handTask)
{
    TaskFuncTable3 states;

    states = D_actor_135600_80131E24;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Leaves an attached Kyle hand idle while its parent rig drives the transform.
static void _actor135600IdleKyleMadiganHand(Task* handTask)
{
}

/// Runs Kyle's held model through attachment, quad drawing and teardown.
///
/// Requires a live TMD task with state 0..2; setup's parent task and part-index
/// spawn arguments must remain valid. The attached model and its quad share
/// the parent's transform and lighting lifetime.
static void _actor135600KyleMadiganHeldItemTask(Task* itemTask)
{
    TaskFuncTable3 states;

    states = D_actor_135600_80131E30;
    states.funcs[itemTask->state](itemTask);
}

/// Attaches Kyle's held model to a body part and initializes its quad length.
///
/// Requires live child and parent TMD tasks: `spawnArg2.pointer` borrows the
/// parent, and `spawnArg1.value` is a valid coordinate index (setup supplies 8).
/// Sets the root's local translation to (-150, 80, 0), appends a quarter turn
/// about Y, and borrows the parent's part and lighting matrices until teardown.
/// Joins the parent's child-task ring and advances to state 1. While this
/// callback is active, `killCountdown` stores a Q12 length scale, not a timer.
static void _actor135600AttachKyleMadiganHeldItem(Task* itemTask)
{
    enum { ACTOR_135600_HELD_ITEM_ATTACH_YAW = 0x400 }; // Quarter turn in 4096-angle units
    MATRIX     attachmentRotation;
    MATRIX*    rotationMatrix;
    Task*      parentTask;
    s32        parentPartIndex;
    TmdObject* itemModel;
    TmdObject* parentModel;
    GfxCoord*  itemRoot;
    GfxCoord*  parentPart;

    parentTask      = itemTask->spawnArg2.pointer;
    itemModel       = itemTask->extra.tmd;
    parentPartIndex = itemTask->spawnArg1.value;
    parentModel     = parentTask->extra.tmd;
    itemRoot        = itemModel->coords;
    parentPart      = &parentModel->coords[parentPartIndex];

    itemRoot->coord.t[0] = -0x96;
    itemRoot->coord.t[1] = 0x50;
    itemRoot->coord.t[2] = 0;

    rotationMatrix = &attachmentRotation;
    gfxSetRotIdentity(rotationMatrix);
    RotMatrixY(ACTOR_135600_HELD_ITEM_ATTACH_YAW, rotationMatrix);
    MulMatrix0(&itemRoot->coord, rotationMatrix, &itemRoot->coord);

    // Borrow the body's resources and join its teardown tree.
    itemRoot->parent       = parentPart;
    itemRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    itemModel->lightMtx    = parentModel->lightMtx;
    itemModel->colorMtx    = parentModel->colorMtx;
    itemModel->otOffset    = 0;
    taskReparent(parentTask, itemTask);
    itemTask->killCountdown = ACTOR_135600_HELD_QUAD_FULL_LENGTH;
    itemTask->state        += 1;
}

/// Draws the held-item quad during script events, shortening it at a screen-angle threshold.
///
/// Requires an attached live TMD task. `killCountdown` holds the positive Q12
/// length multiplier: setup supplies 4096, and a signed screen angle at least
/// 501 replaces it with 2048. Neither this callback nor the task scheduler
/// decrements that storage while the held-item callbacks are installed.
/// Outside a script event, or for a nonpositive scale, no quad is drawn.
static void _actor135600UpdateKyleMadiganHeldItem(Task* itemTask)
{
    s16 lengthScale12;

    if (gGameSession->eventState != 0) {
        lengthScale12 = itemTask->killCountdown;
        if (lengthScale12 > 0 && _actor135600DrawHeldItemQuad(itemTask->extra.tmd->coords, lengthScale12) >= ACTOR_135600_HELD_QUAD_SHORTEN_ANGLE) {
            itemTask->killCountdown = ACTOR_135600_HELD_QUAD_HALF_LENGTH;
        }
    }
}

/// Composes a model node's world rotation and halfword translation, excluding the view.
///
/// `node` must be a non-world node in an acyclic chain ending at `gGfxViewCoord`.
/// Outputs must be writable and separate from the input nodes, with the matrix
/// word-aligned. Writes only the
/// Q12 rotation coefficients and the translation vector's xyz halfwords; other
/// output bytes are untouched. Each local translation narrows to a signed
/// halfword before rotation, and each addition narrows again. No cache changes.
static void _actor135600ComposeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation)
{
    SVECTOR localTranslation;

    if (node->parent != &gGfxViewCoord) {
        _actor135600ComposeWorldTransform(node->parent, worldRotation, worldTranslation);
    } else {
        gfxSetRotIdentity(worldRotation);
        worldTranslation->vx = 0;
        worldTranslation->vy = 0;
        worldTranslation->vz = 0;
    }

    // Retain the signed-halfword translation at every level of the hierarchy.
    localTranslation.vx = (u16)node->coord.t[0];
    localTranslation.vy = (u16)node->coord.t[1];
    localTranslation.vz = (u16)node->coord.t[2];
    ApplyMatrixSV(worldRotation, &localTranslation, &localTranslation);
    worldTranslation->vx += localTranslation.vx;
    worldTranslation->vy += localTranslation.vy;
    worldTranslation->vz += localTranslation.vz;
    MulMatrix0(worldRotation, (MATRIX*)&node->coord, worldRotation);
}

/// Runs Kyle's walker through allocation, per-frame update and teardown.
///
/// Requires a live TMD task with state 0..2. Dispatch itself remains active
/// while scene actors are frozen; the update state applies the freeze gate.
static void _actor135600KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_135600_80131E3C;
    states.funcs[task->state](task);
}

/// Releases Kyle's enemy record, child tasks and work, then requests body teardown.
///
/// Requires a live primary-heap `Enemy` in `spawnArg2.pointer`. Both the exit
/// state and exit callback use `enemyTaskExit`'s ownership contract: normal TMD
/// teardown is deferred, or immediate when the display requests it. Neither
/// the task nor the enemy may be accessed again after this call.
static void _actor135600ExitKyleMadiganWalker(Task* task)
{
    enemyTaskExit(task);
}

/// Lends Kyle's work-owned light and colour matrices to his body model.
///
/// Requires a live TMD body and `KyleMadiganWalkerWork`. The work block must
/// outlive all model and attached-child uses of these borrowed pointers.
static void _actor135600BindKyleMadiganWalkerLighting(Task* task)
{
    TmdObject*             bodyModel;
    KyleMadiganWalkerWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves Kyle's motion idle between scripted walks.
static void _actor135600IdleKyleMadiganWalk(Task* task)
{
}

/// Runs the current face, begin-walk, arrival or closing-turn step of Kyle's walk.
///
/// Requires live `KyleMadiganWalkerWork` with `walk.motionStep` in 0..3.
/// The arrival step stops velocity; the closing turn returns motion to idle.
static void _actor135600RunKyleMadiganWalkStep(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = D_actor_135600_80131E48;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Starts Kyle moving along his local +Z axis and advances to the arrival step.
///
/// Requires a live TMD root and `KyleMadiganWalkerWork` in motion step 1.
/// The Q12 root rotation transforms a 32-unit-per-frame 16.16 velocity into
/// the parent's frame. Seeds the previous-distance sentinels so the first
/// arrival check records its X/Z distances, then advances to step 2.
static void _actor135600BeginKyleMadiganWalk(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              rootCoord;
    VECTOR                 localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = D_actor_135600_80131E58;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sets Kyle's body draw mode and propagates the complete flags to both hands and held model.
///
/// Requires live body and all three child TMD tasks in `KyleMadiganWalkerWork`.
/// Modes 0/1 hide/show and enable automatic buffers; showing allocates a body
/// primitive buffer if needed. Mode 2 hides, disables automatic buffers and
/// arms body-buffer release with counter 2. Mode 3 shows with automatic buffers
/// disabled. Modes 0, 1 and 3 leave any pending release unchanged. Other modes retain the
/// body flags; all modes copy those flags to the children. Returns 0 for modes
/// 0..3, otherwise 1. The message ID and second payload are ignored.
/// These flags govern the TMD models; the held-item quad has its own event gate.
static s32 _actor135600SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum {
        ACTOR_135600_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
        ACTOR_135600_KYLE_BUFFER_RELEASE_DELAY       = 2,
    };
    KyleMadiganWalkerWork* work;
    TmdObject*             bodyModel;
    TmdObject*             rightHandModel;
    TmdObject*             leftHandModel;
    TmdObject*             heldItemModel;
    s32                    result;

    work           = task->work;
    bodyModel      = task->extra.tmd;
    leftHandModel  = work->handTasks[1]->extra.tmd;
    rightHandModel = work->handTasks[0]->extra.tmd;
    heldItemModel  = work->heldItemTask->extra.tmd;
    result         = 0;
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
            work->freeCountdown = ACTOR_135600_KYLE_BUFFER_RELEASE_DELAY;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_135600_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    leftHandModel->flags  = bodyModel->flags;
    rightHandModel->flags = bodyModel->flags;
    heldItemModel->flags  = bodyModel->flags;
    return result;
}

/// Accepts an actor command without changing Kyle's state and returns zero.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; ignores the receiver, message
/// ID and both payload words, retaining no borrowed command data.
static s32 _actor135600IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
