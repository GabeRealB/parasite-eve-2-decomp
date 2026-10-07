#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

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
#include "main/gameflag.h"
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

#include "rooms/dryfield_night_garage.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Placement phases carried in the attached model task's reused spawn word.
///
/// Setup preserves part index 8; only later actor commands select phases 1 or 3.
enum {
    ACTOR_135400_CARRIED_PLACEMENT_IDLE       = 0,
    ACTOR_135400_CARRIED_PLACEMENT_DETACH     = 1,
    ACTOR_135400_CARRIED_PLACEMENT_WAIT_EVENT = 2,
    ACTOR_135400_CARRIED_PLACEMENT_SET_DOWN   = 3,
};

/// Gary Douglas's command namespace; the command's stage/area tags are ignored.
enum {
    ACTOR_135400_GARY_DOUGLAS_SHOW_CARRIED      = 0,
    ACTOR_135400_GARY_DOUGLAS_HIDE_CARRIED      = 1,
    ACTOR_135400_GARY_DOUGLAS_TURN_TO_PLAYER    = 2,
    ACTOR_135400_GARY_DOUGLAS_RELEASE_HEAD_TURN = 3,
    ACTOR_135400_GARY_DOUGLAS_DETACH_CARRIED    = 4,
    ACTOR_135400_GARY_DOUGLAS_SET_DOWN_CARRIED  = 5,
};

/// Fourth draw mode: show the model while disabling automatic buffer recovery.
enum { ACTOR_135400_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };

/// Flint's cycle visits clips 1 through 6; -1 means no deferred buffer release.
enum {
    ACTOR_135400_FLINT_FIRST_CYCLE_CLIP     = 1,
    ACTOR_135400_FLINT_CYCLE_CLIP_LIMIT     = 7,
    ACTOR_135400_FLINT_BUFFER_FREE_INACTIVE = -1,
};

/// Work block of Flint, the dog whose model this package carries beside
/// Gary Douglas's.
///
/// Flint's task allocates it zeroed in its spawn state and keeps it at
/// `Task::work` for the task's life. It opens with the two members the
/// nineteen-part play handler runs on (`ActorMotion19PlayWork`); this actor
/// does not walk, so what follows them is its own. The model object borrows
/// `model.light` and `model.color` for as long as the block lives.
///
/// Flint runs a clip cycle of his own, with no request from the room: the
/// tick holds each of clips 1 to 6 for that clip's entry in the package's
/// hold-time table and then plays the next, clip 1 following clip 6. A play
/// request sent from outside changes the clip shown and leaves the cycle's
/// place and timer alone.
typedef struct {
    ActorAnimRig19       rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState      model;         // Clip and bank the rig plays, and the matrices the model is lit with
    AnimationPlayRequest cycleRequest;  // Play request the tick re-sends for each step of the clip cycle: `animationId` is the step in progress (1 to 6; it starts at 2 while clip 1 plays), the other members the bank and blend every step uses
    s32                  freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor135400FlintWork;
STATIC_ASSERT_SIZEOF(_Actor135400FlintWork, 0x498);

/// Work block of Gary Douglas in Dryfield's garage at night.
///
/// The actor's task allocates it zeroed in its spawn state and keeps it at
/// `Task::work` for the task's life. It opens with the two members the
/// twenty-part play handler runs on (`ActorMotionPlayWork`); this actor does
/// not walk, so what follows them is its own: the two child tasks drawing the
/// models attached to the body, and the head turn toward the player. The
/// model object borrows `model.light` and `model.color` for as long as the
/// block lives.
///
/// The spawn stores a child task only when its spawn succeeds, so either
/// pointer can be NULL. The actor command handler checks `carriedTask` before
/// every use; the draw-mode handler reads `headTask` unchecked.
typedef struct {
    ActorAnimRig20  rig;              // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    Task*           headTask;         // Child task drawing the head-and-hat model, which hangs from the body's part 4; it takes the body's tint, and every draw-mode message copies the body's draw flags onto its model
    Task*           carriedTask;      // Child task drawing the model the body carries, which hangs from part 8 until an actor command leaves it where it is or sets it down at a fixed place in the room; actor commands also show and hide it
    s32             turnWeightRising; // Direction `turnWeight` ramps, set by an actor command (0 falls by 0x80 a tick, 1 rises by 0x100)
    s32             turnWeight;       // Weight handed to the per-frame head turn toward the player, 0 to 0xFFF; stepped only while the model is drawn
} _Actor135400GaryDouglasWork;
STATIC_ASSERT_SIZEOF(_Actor135400GaryDouglasWork, 0x4C8);

/// The two placements Gary Douglas can start from, copied as a whole by his
/// task's spawn state, which then places the body at the one the night garage
/// event's progress selects.
typedef struct {
    ActorTransform beforeEvent; // Position and rotation while the night garage event has not played (`GAME_FLAG_NIGHT_GARAGE_PROGRESS` 0)
    ActorTransform afterEvent;  // Position and rotation once it has (any positive progress)
} _Actor135400GaryDouglasPlaces;
STATIC_ASSERT_SIZEOF(_Actor135400GaryDouglasPlaces, 0x30);

/// The actor's two-entry `TaskDesc` table, indexed by `taskSpawnFromTable`:
/// entry 1 is the model-bearing part task `_modelPlacementAttachPartTask`
/// reparents, entry 2 the second part (`_modelPlacementAttachPart`).
extern TaskDesc D_actor_135400_8013A4AC[];

extern TaskMessageEntry D_actor_135400_8013A4D0[5];

/// The three flat lights `_actor135400FlintInitLighting` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

/// The second task's message table: `(message id, handler)` pairs for 0x7D3 /
/// 0x7D4 / 0x7D5, ended by `TASK_MESSAGE_TABLE_END`. `_actor135400FlintSpawn` parks
/// its address in `Task::msgTable`.
extern TaskMessageEntry D_actor_135400_8013F8E4[4];

/// Per-step frame counts of the second task's 0x7D3 animation: eight `s16`
/// entries indexed by `_Actor135400FlintWork::cycleRequest.animationId`.
/// `_actor135400FlintTick` runs the task's `killCountdown` up and, once it
/// passes the entry for the current step, advances that step -- wrapping at 7
/// -- and re-issues the animation. The first and last entries are zero, so
/// neither ever expires.
extern s16 D_actor_135400_8013F8C4[];

/// Animation banks the two 0x7D3 handlers re-seed their slots from, indexed by
/// the request's `field_0`: `gActorMotionAnimBanks` for the main task,
/// `D_actor_135400_8013F8D4` for the second task.
extern AnimationSet*  D_actor_135400_8013A494[5];
extern AnimationSet** gActorMotionAnimBanks[1];
extern AnimationSet*  D_actor_135400_8013F8A8[7];
extern AnimationSet** D_actor_135400_8013F8D4[1];

/// Psy-Q `RotMatrixY`: the angle is a `long`, so a negated angle is passed
/// without re-truncation to 16 bits.

/// Main-executable helper the spawn runs on the flag-clear path, once the
/// actor is placed. Unmatched, so declared here.

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor135400GaryDouglasUpdateCarriedPlacement(Task* task);
static void func_actor_135400_80132064(Task* arg0);
static void _actor135400GaryDouglasTick(Task* task);
static void _actor135400GaryDouglasHeadIdle(Task* unusedTask);
static void _modelPlacementAttachPart(Task* childTask);
static void _actor135400GaryDouglasExit(Task* task);
static void _actor135400GaryDouglasBindLighting(Task* task);
static s32  _actor135400GaryDouglasSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static void _actor135400FlintSpawn(Task* task);
static void _actor135400FlintExit(Task* task);
static void _actor135400FlintInitLighting(Task* task);
static void _actor135400FlintTick(Task* task);
static s32  _actor135400FlintPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor135400FlintSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);

/// State table of the first part task: state 0 reparents it
/// (`_modelPlacementAttachPartTask`), state 1 does nothing and state 2 kills it.
/// Dispatched by `_actor135400GaryDouglasHeadTask`.
static const TaskFuncTable3 D_actor_135400_80131E24 = { {
    _modelPlacementAttachPartTask,
    _actor135400GaryDouglasHeadIdle,
    taskKill,
} };

/// State table of the second part task: state 0 reparents it
/// (`_modelPlacementAttachPart`), state 1 runs its placement phases
/// (`_actor135400GaryDouglasUpdateCarriedPlacement`) and state 2 kills it. Dispatched by
/// `_actor135400GaryDouglasCarriedTask`.
static const TaskFuncTable3 D_actor_135400_80131E30 = { {
    _modelPlacementAttachPart,
    _actor135400GaryDouglasUpdateCarriedPlacement,
    taskKill,
} };

/// State table of the main task: spawn, per-frame tick and exit callback.
/// Dispatched by `_actor135400GaryDouglasTask`.
static const TaskFuncTable3 D_actor_135400_80131E3C = { {
    func_actor_135400_80132064,
    _actor135400GaryDouglasTick,
    _actor135400GaryDouglasExit,
} };

/// The two spawn placements `func_actor_135400_80132064` copies as a whole:
/// the flag-clear branch's first, the other second.
static const _Actor135400GaryDouglasPlaces D_actor_135400_80131E48 = {
    { { 5700, -150, 5900, 0 }, { 1024, 0, -1024, 0 } },
    { { 4700, 0, 5000, 0 }, { 0, -1024, 0, 0 } },
};

static TmdSource _gActor135400FlintBody;
static s32       _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static void      _actor135400FlintTask(Task* task);

static s32  _actor135400GaryDouglasApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void _actor135400GaryDouglasHeadTask(Task* task);
static void _actor135400GaryDouglasCarriedTask(Task* task);
static void _actor135400GaryDouglasTask(Task* task);

static TmdBone _gActor135400GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor135400GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor135400GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor135400GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor135400GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor135400GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor135400GaryDouglasBodyPartVerts,
    _gActor135400GaryDouglasBodyVerts,
    _gActor135400GaryDouglasBodyNormals,
    _gActor135400GaryDouglasBodySkeleton,
    _gActor135400GaryDouglasBodyStream,
};

static TmdBone _gActor135400GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor135400GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor135400GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor135400GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor135400GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor135400GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor135400GaryDouglasHeadHatPartVerts,
    _gActor135400GaryDouglasHeadHatVerts,
    _gActor135400GaryDouglasHeadHatNormals,
    _gActor135400GaryDouglasHeadHatSkeleton,
    _gActor135400GaryDouglasHeadHatStream,
};

static TmdBone _gActor135400Model071ACSkeleton[1] = {
#include "assets/actor_135400_model_071AC_skeleton.inc"
};

static u32 _gActor135400Model071ACPartVerts[1] = {
#include "assets/actor_135400_model_071AC_partVerts.inc"
};

static SVECTOR _gActor135400Model071ACVerts[20] = {
#include "assets/actor_135400_model_071AC_verts.inc"
};

static u32 _gActor135400Model071ACStream[106] = {
#include "assets/actor_135400_model_071AC_stream.inc"
};

static TmdSource _gActor135400Model071AC = {
    0,
    800,
    0,
    1,
    _gActor135400Model071ACPartVerts,
    _gActor135400Model071ACVerts,
    &_gActor135400Model071ACVerts[20],
    _gActor135400Model071ACSkeleton,
    _gActor135400Model071ACStream,
};

static AnimationPackedPose _gActor135400Animation07568Bank1[3] = {
#include "assets/actor_135400_animation_07568_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation07568Bank4[18] = {
#include "assets/actor_135400_animation_07568_bank4.inc"
};

static AnimationRecord _gActor135400Animation07568Records[87] = {
#include "assets/actor_135400_animation_07568_records.inc"
};

static u16 _gActor135400Animation07568Indices[20] = {
#include "assets/actor_135400_animation_07568_indices.inc"
};

static AnimationSet _gActor135400Animation07568 = {
    _gActor135400Animation07568Records,
    _gActor135400Animation07568Indices,
    { NULL, _gActor135400Animation07568Bank1, NULL, NULL, _gActor135400Animation07568Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation077B8Bank1[3] = {
#include "assets/actor_135400_animation_077B8_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation077B8Bank4[25] = {
#include "assets/actor_135400_animation_077B8_bank4.inc"
};

static AnimationRecord _gActor135400Animation077B8Records[94] = {
#include "assets/actor_135400_animation_077B8_records.inc"
};

static u16 _gActor135400Animation077B8Indices[20] = {
#include "assets/actor_135400_animation_077B8_indices.inc"
};

static AnimationSet _gActor135400Animation077B8 = {
    _gActor135400Animation077B8Records,
    _gActor135400Animation077B8Indices,
    { NULL, _gActor135400Animation077B8Bank1, NULL, NULL, _gActor135400Animation077B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation084B0Bank1[31] = {
#include "assets/actor_135400_animation_084B0_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation084B0Bank4[309] = {
#include "assets/actor_135400_animation_084B0_bank4.inc"
};

static AnimationRecord _gActor135400Animation084B0Records[408] = {
#include "assets/actor_135400_animation_084B0_records.inc"
};

static u16 _gActor135400Animation084B0Indices[20] = {
#include "assets/actor_135400_animation_084B0_indices.inc"
};

static AnimationSet _gActor135400Animation084B0 = {
    _gActor135400Animation084B0Records,
    _gActor135400Animation084B0Indices,
    { NULL, _gActor135400Animation084B0Bank1, NULL, NULL, _gActor135400Animation084B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation0864CBank1[2] = {
#include "assets/actor_135400_animation_0864C_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0864CBank4[17] = {
#include "assets/actor_135400_animation_0864C_bank4.inc"
};

static AnimationRecord _gActor135400Animation0864CRecords[60] = {
#include "assets/actor_135400_animation_0864C_records.inc"
};

static u16 _gActor135400Animation0864CIndices[20] = {
#include "assets/actor_135400_animation_0864C_indices.inc"
};

static AnimationSet _gActor135400Animation0864C = {
    _gActor135400Animation0864CRecords,
    _gActor135400Animation0864CIndices,
    { NULL, _gActor135400Animation0864CBank1, NULL, NULL, _gActor135400Animation0864CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_135400_8013A494[5] = {
    NULL,
    &_gActor135400Animation07568,
    &_gActor135400Animation077B8,
    &_gActor135400Animation084B0,
    &_gActor135400Animation0864C,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_135400_8013A494,
};

TaskDesc D_actor_135400_8013A4AC[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor135400GaryDouglasTask, { .model = &_gActor135400GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor135400GaryDouglasHeadTask, { .model = &_gActor135400GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, _actor135400GaryDouglasCarriedTask, { .model = &_gActor135400Model071AC } },
};

TaskMessageEntry D_actor_135400_8013A4D0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor135400GaryDouglasSetDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor135400GaryDouglasApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor135400FlintBodySkeleton[19] = {
#include "assets/flint_body_skeleton.inc"
};

static u32 _gActor135400FlintBodyPartVerts[19] = {
#include "assets/flint_body_partVerts.inc"
};

static SVECTOR _gActor135400FlintBodyVerts[238] = {
#include "assets/flint_body_verts.inc"
};

static SVECTOR _gActor135400FlintBodyNormals[238] = {
#include "assets/flint_body_normals.inc"
};

static u32 _gActor135400FlintBodyStream[2784] = {
#include "assets/flint_body_stream.inc"
};

static TmdSource _gActor135400FlintBody = {
    0,
    13636,
    6016,
    19,
    _gActor135400FlintBodyPartVerts,
    _gActor135400FlintBodyVerts,
    _gActor135400FlintBodyNormals,
    _gActor135400FlintBodySkeleton,
    _gActor135400FlintBodyStream,
};

static AnimationPackedPose _gActor135400Animation0C6BCBank1[2] = {
#include "assets/actor_135400_animation_0C6BC_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0C6BCBank4[36] = {
#include "assets/actor_135400_animation_0C6BC_bank4.inc"
};

static AnimationRecord _gActor135400Animation0C6BCRecords[102] = {
#include "assets/actor_135400_animation_0C6BC_records.inc"
};

static u16 _gActor135400Animation0C6BCIndices[20] = {
#include "assets/actor_135400_animation_0C6BC_indices.inc"
};

static AnimationSet _gActor135400Animation0C6BC = {
    _gActor135400Animation0C6BCRecords,
    _gActor135400Animation0C6BCIndices,
    { NULL, _gActor135400Animation0C6BCBank1, NULL, NULL, _gActor135400Animation0C6BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation0C9F0Bank1[5] = {
#include "assets/actor_135400_animation_0C9F0_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0C9F0Bank4[46] = {
#include "assets/actor_135400_animation_0C9F0_bank4.inc"
};

static AnimationRecord _gActor135400Animation0C9F0Records[124] = {
#include "assets/actor_135400_animation_0C9F0_records.inc"
};

static u16 _gActor135400Animation0C9F0Indices[20] = {
#include "assets/actor_135400_animation_0C9F0_indices.inc"
};

static AnimationSet _gActor135400Animation0C9F0 = {
    _gActor135400Animation0C9F0Records,
    _gActor135400Animation0C9F0Indices,
    { NULL, _gActor135400Animation0C9F0Bank1, NULL, NULL, _gActor135400Animation0C9F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation0CFA4Bank1[11] = {
#include "assets/actor_135400_animation_0CFA4_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0CFA4Bank4[134] = {
#include "assets/actor_135400_animation_0CFA4_bank4.inc"
};

static AnimationRecord _gActor135400Animation0CFA4Records[178] = {
#include "assets/actor_135400_animation_0CFA4_records.inc"
};

static u16 _gActor135400Animation0CFA4Indices[20] = {
#include "assets/actor_135400_animation_0CFA4_indices.inc"
};

static AnimationSet _gActor135400Animation0CFA4 = {
    _gActor135400Animation0CFA4Records,
    _gActor135400Animation0CFA4Indices,
    { NULL, _gActor135400Animation0CFA4Bank1, NULL, NULL, _gActor135400Animation0CFA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation0D320Bank1[6] = {
#include "assets/actor_135400_animation_0D320_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0D320Bank4[74] = {
#include "assets/actor_135400_animation_0D320_bank4.inc"
};

static AnimationRecord _gActor135400Animation0D320Records[111] = {
#include "assets/actor_135400_animation_0D320_records.inc"
};

static u16 _gActor135400Animation0D320Indices[20] = {
#include "assets/actor_135400_animation_0D320_indices.inc"
};

static AnimationSet _gActor135400Animation0D320 = {
    _gActor135400Animation0D320Records,
    _gActor135400Animation0D320Indices,
    { NULL, _gActor135400Animation0D320Bank1, NULL, NULL, _gActor135400Animation0D320Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor135400Animation0DA60Bank1[8] = {
#include "assets/actor_135400_animation_0DA60_bank1.inc"
};

static AnimationPackedRotation _gActor135400Animation0DA60Bank4[158] = {
#include "assets/actor_135400_animation_0DA60_bank4.inc"
};

static AnimationRecord _gActor135400Animation0DA60Records[262] = {
#include "assets/actor_135400_animation_0DA60_records.inc"
};

static u16 _gActor135400Animation0DA60Indices[20] = {
#include "assets/actor_135400_animation_0DA60_indices.inc"
};

static AnimationSet _gActor135400Animation0DA60 = {
    _gActor135400Animation0DA60Records,
    _gActor135400Animation0DA60Indices,
    { NULL, _gActor135400Animation0DA60Bank1, NULL, NULL, _gActor135400Animation0DA60Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_135400_8013F8A8[7] = {
    NULL,
    &_gActor135400Animation0C6BC,
    &_gActor135400Animation0D320,
    &_gActor135400Animation0C9F0,
    &_gActor135400Animation0DA60,
    &_gActor135400Animation0C9F0,
    &_gActor135400Animation0CFA4,
};

s16 D_actor_135400_8013F8C4[8] = {
    0,
    700,
    28,
    60,
    110,
    600,
    28,
    0,
};

AnimationSet** D_actor_135400_8013F8D4[1] = {
    D_actor_135400_8013F8A8,
};

TaskDesc D_actor_135400_8013F8D8 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor135400FlintTask, { .model = &_gActor135400FlintBody } };

TaskMessageEntry D_actor_135400_8013F8E4[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor135400FlintPlayAnimation },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor135400FlintSetDrawMode },
    { TASK_MESSAGE_TABLE_END, NULL },
};

GsF_LIGHT D_actor_135400_8013F904[3] = {
    { 0, 4096, 0, 96, 96, 96 },
    { 4096, 0, 0, 96, 96, 96 },
    { 0, 0, 4096, 96, 96, 96 },
};

/// Detaches Gary Douglas's carried model root at its current world placement.
///
/// Requires a live non-world root with acyclic ancestry ending at `gGfxViewCoord`.
/// Borrows separate writable matrix and vector scratch storage for this call;
/// only the Q12 rotation and translation xyz are outputs. Translation uses integer
/// coordinate units and retains signed-halfword truncation at each ancestor.
/// Reparents the root to the world coordinate and invalidates its composed cache.
/// Model lighting and the task teardown parent are retained.
static inline void _actor135400GaryDouglasDetachCarriedRoot(GfxCoord* rootCoord, MATRIX* worldRotation, SVECTOR* worldTranslation)
{
    gfxComposeNodeWorldTransform(rootCoord, worldRotation, worldTranslation);
    rootCoord->coord        = *worldRotation;
    rootCoord->coord.t[0]   = worldTranslation->vx;
    rootCoord->coord.t[1]   = worldTranslation->vy;
    rootCoord->coord.t[2]   = worldTranslation->vz;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Updates the placement of Gary Douglas's carried model after an actor command.
///
/// Requires a live model with coordinate 0. The initial spawn word 8 keeps it
/// attached; phase 1 freezes its current world transform, detaches it and advances
/// to phase 2. Phase 2 waits for eventState to clear, then sets it down; phase 3
/// sets it down immediately. Set-down uses world position (4862, -435, 5500) and
/// yaw -910 in 4096 units per turn, then clears the phase to 0. Other values do
/// nothing. Detachment borrows the world coordinate and retains shared lighting.
static void _actor135400GaryDouglasUpdateCarriedPlacement(Task* task)
{
    enum {
        ACTOR_135400_CARRIED_SET_DOWN_YAW = -910,
        ACTOR_135400_CARRIED_SET_DOWN_X   = 4862,
        ACTOR_135400_CARRIED_SET_DOWN_Y   = -435,
        ACTOR_135400_CARRIED_SET_DOWN_Z   = 5500,
    };
    GfxMatrix worldTransform;
    SVECTOR   worldPosition;
    GfxCoord* rootCoord;

    switch (task->spawnArg1.value) {
        case ACTOR_135400_CARRIED_PLACEMENT_DETACH:
            rootCoord = task->extra.tmd->coords;
            // Freeze the inherited transform before detaching from the body.
            _actor135400GaryDouglasDetachCarriedRoot(rootCoord, &worldTransform.mat, &worldPosition);
            task->spawnArg1.value += 1;
            break;
        case ACTOR_135400_CARRIED_PLACEMENT_WAIT_EVENT:
            if (gGameSession->eventState != 0) {
                break;
            }
            // Fall through once the event has finished.
        case ACTOR_135400_CARRIED_PLACEMENT_SET_DOWN:
            rootCoord = task->extra.tmd->coords;
            gfxSetRotIdentity(&worldTransform.mat);
            RotMatrixY(ACTOR_135400_CARRIED_SET_DOWN_YAW, &worldTransform.mat);
            rootCoord->coord        = worldTransform.mat;
            rootCoord->coord.t[0]   = ACTOR_135400_CARRIED_SET_DOWN_X;
            rootCoord->coord.t[1]   = ACTOR_135400_CARRIED_SET_DOWN_Y;
            rootCoord->coord.t[2]   = ACTOR_135400_CARRIED_SET_DOWN_Z;
            rootCoord->parent       = &gGfxViewCoord;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->spawnArg1.value   = ACTOR_135400_CARRIED_PLACEMENT_IDLE;
            break;
    }
}

/// The spawn handler of the actor's main task: allocates the work
/// block, seeds its two `-1` latches, starts the two part tasks and copies the
/// area record's texture page / CLUT onto part 1's model. It then installs the
/// handler table, the 0x7D5 model mode and the exit callback, and finally hands
/// the 0x7D4 placement and the 0x7D3 animation the game flag 0x6C selects.
static void func_actor_135400_80132064(Task* arg0)
{
    _Actor135400GaryDouglasWork*  work;
    _Actor135400GaryDouglasPlaces places;
    AnimationPlayRequest          anim[2];
    Task*                         spawned;

    places = D_actor_135400_80131E48;
    memset(anim, 0, sizeof(anim));
    anim[0].animationId = 1;
    anim[1].animationId = 4;
    work                = memCalloc(sizeof(_Actor135400GaryDouglasWork), 0);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work         = work;
    work->model.animId = ACTOR_MODEL_STATE_NONE;
    work->model.bank   = ACTOR_MODEL_STATE_NONE;
    spawned            = taskSpawnFromTable(D_actor_135400_8013A4AC, 1, 4, arg0);
    if (spawned != NULL) {
        work->headTask = spawned;
        actorTintTask(spawned, (Enemy*)arg0->spawnArg2.pointer);
    }
    spawned = taskSpawnFromTable(D_actor_135400_8013A4AC, 2, 8, arg0);
    if (spawned != NULL) {
        work->carriedTask = spawned;
    }
    _actor135400GaryDouglasBindLighting(arg0);
    arg0->msgTable = D_actor_135400_8013A4D0;
    _actor135400GaryDouglasSetDrawMode(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) <= 0) {
        actorMsgPlaceEuler(arg0, ACTOR_MESSAGE_PLACE, &places.beforeEvent, 0);
        actorMotionPlayAnim(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &anim[0], 0);
        func_dryfield_night_garage_80180414(0);
    } else {
        actorMsgPlaceEuler(arg0, ACTOR_MESSAGE_PLACE, &places.afterEvent, 0);
        actorMotionPlayAnim(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &anim[1], 0);
    }
    arg0->exitCallback = _actor135400GaryDouglasExit;
    arg0->state       += 1;
}

/// Ramps Gary Douglas's blend weight for head aiming toward the player.
///
/// Requires live work with `turnWeight` in 0..4095, in 1/4096 units. A nonzero
/// `turnWeightRising` adds 256 with a cap of 4095; zero subtracts 128 with a
/// floor of zero. Changes only the weight; the caller applies the head rotation.
static inline void _actor135400GaryDouglasStepHeadTurnWeight(_Actor135400GaryDouglasWork* work)
{
    enum {
        ACTOR_135400_GARY_DOUGLAS_HEAD_TURN_RISE = 0x100,
        ACTOR_135400_GARY_DOUGLAS_HEAD_TURN_FALL = 0x80,
    };
    s32 turnWeight;

    if (work->turnWeightRising != 0) {
        turnWeight       = work->turnWeight + ACTOR_135400_GARY_DOUGLAS_HEAD_TURN_RISE;
        work->turnWeight = turnWeight;
        if (turnWeight >= ONE) {
            work->turnWeight = ONE - 1;
        }
    } else {
        turnWeight       = work->turnWeight - ACTOR_135400_GARY_DOUGLAS_HEAD_TURN_FALL;
        work->turnWeight = turnWeight;
        if (turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
}

/// Ticks Gary Douglas's animation, ground shadow, room lighting and head turn.
///
/// Requires initialized twenty-part playback and a live player model suitable for
/// head aiming. Slots 1..19 tick even while drawing is suppressed; shadow, room
/// lighting and the Q12 head-turn ramp run only while drawn. The shadow samples
/// part 1's cached translation before that coordinate is recomposed. Head limits
/// are 512 yaw and 256 pitch in 4096 units per turn; the weight spans 0..4095.
static void _actor135400GaryDouglasTick(Task* task)
{
    _Actor135400GaryDouglasWork* work;
    TmdObject*                   model;
    VECTOR3                      groundPosition;
    s32                          slotIndex;

    enum {
        ACTOR_135400_GARY_DOUGLAS_SHADOW_HALF_SIZE = 0x300,
        ACTOR_135400_GARY_DOUGLAS_HEAD_MAX_YAW     = 0x200,
        ACTOR_135400_GARY_DOUGLAS_HEAD_MAX_PITCH   = 0x100,
        ACTOR_135400_GARY_DOUGLAS_ROOM_LIGHT_COUNT = 3,
        ACTOR_135400_GARY_DOUGLAS_SHADOW_PART      = 1,
    };

    work  = task->work;
    model = task->extra.tmd;
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[ACTOR_135400_GARY_DOUGLAS_SHADOW_PART].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_135400_GARY_DOUGLAS_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        actorRenderComposeCoord(&task->extra.tmd->coords[ACTOR_135400_GARY_DOUGLAS_SHADOW_PART]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[ACTOR_135400_GARY_DOUGLAS_SHADOW_PART].workm.t, 0, ACTOR_135400_GARY_DOUGLAS_ROOM_LIGHT_COUNT);
        _actor135400GaryDouglasStepHeadTurnWeight(work);
        animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_135400_GARY_DOUGLAS_HEAD_MAX_YAW, ACTOR_135400_GARY_DOUGLAS_HEAD_MAX_PITCH, work->turnWeight);
    }
}

/// Dispatches the head-and-hat task: attach, remain attached, or kill.
///
/// Requires a live TMD task with state 0..2. Attachment borrows the parent body
/// from spawnArg2.pointer and its part-4 index from spawnArg1.value.
static void _actor135400GaryDouglasHeadTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_135400_80131E24;
    handlers.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Leaves the attached head-and-hat model riding its parent without an update.
static void _actor135400GaryDouglasHeadIdle(Task* unusedTask)
{
}

/// Dispatches the carried-model task: attach, update placement, or kill.
///
/// Requires a live TMD task with state 0..2. Setup borrows the parent body from
/// spawnArg2.pointer and part index 8 from spawnArg1.value. Later commands reuse
/// that word for placement phases while task state stays 1.
static void _actor135400GaryDouglasCarriedTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_135400_80131E30;
    handlers.funcs[task->state](task);
}

/// Selects the private setup callback for the carried model's state table.
///
/// The binding is a declared `void(Task*)` function identifier, consumed by
/// the following fragment inclusion.
#define MODEL_PLACEMENT_ATTACH_PART_TASK _modelPlacementAttachPart
#include "../../shared/model_placement_attach_part.inc.c"

/// Dispatches Gary Douglas's spawn, tick or exit state while actor control is running.
///
/// Requires a live body task with state 0..2 and this package's code loaded.
/// Paused or hidden actor control defers every state, including spawn and exit.
/// The selected handler can release the task; no task storage is used afterwards.
static void _actor135400GaryDouglasTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_135400_80131E3C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

/// Releases Gary Douglas through the common enemy-task teardown.
///
/// Installed as both the exit callback and state 2; the work block and owned body
/// resources must remain live at entry and must not be used after teardown.
static void _actor135400GaryDouglasExit(Task* task)
{
    enemyTaskExit(task);
}

/// Binds Gary Douglas's model lighting to the matrices owned by its work block.
///
/// Requires a live TMD body and allocated work. The matrices must remain live
/// through the model's draws; this binds their addresses without filling them.
static void _actor135400GaryDouglasBindLighting(Task* task)
{
    TmdObject*                   model;
    _Actor135400GaryDouglasWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Handles model draw mode for Gary Douglas's body and mirrors its flags to the head.
///
/// Requires allocated work and a successfully spawned headTask with a live model;
/// headTask is dereferenced even for an unsupported mode. Modes 0/1 hide/show with
/// automatic buffer recovery enabled, and 1 also allocates the body buffer. Modes
/// 2/3 hide/show with recovery disabled, and 2 immediately frees the body buffer.
/// The head receives all resulting body flags but keeps its own buffer. Returns
/// 0 for modes 0..3, 1 otherwise; messageId and unusedArg are ignored.
static s32 _actor135400GaryDouglasSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject* bodyModel;
    TmdObject* headModel;
    s32        result;

    bodyModel = task->extra.tmd;
    headModel = ((_Actor135400GaryDouglasWork*)task->work)->headTask->extra.tmd;
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
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdFreePrimitiveBuffer(bodyModel);
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_135400_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    headModel->flags = bodyModel->flags;
    return result;
}

/// Applies Gary Douglas's carried-model visibility, head-turn or placement command.
///
/// Commands 0/1 show/hide the carried model, 2/3 raise/lower the head-turn weight,
/// 4 detaches the carried model at its current transform, and 5 requests immediate
/// set-down and shows it. Placement is applied on the child task's next update.
/// Requires allocated work and a borrowed command live for this call; a missing
/// carried task makes its commands no-ops. Context tags, messageId and unusedArg
/// are ignored. Unknown commands do nothing, and every command returns 0.
static s32 _actor135400GaryDouglasApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor135400GaryDouglasWork* work;
    TmdObject*                   model;

    work = task->work;
    switch (command->command) {
        case ACTOR_135400_GARY_DOUGLAS_SHOW_CARRIED:
            if (work->carriedTask != NULL) {
                model         = work->carriedTask->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_135400_GARY_DOUGLAS_HIDE_CARRIED:
            if (work->carriedTask != NULL) {
                model         = work->carriedTask->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_135400_GARY_DOUGLAS_TURN_TO_PLAYER:
            work->turnWeightRising = 1;
            break;
        case ACTOR_135400_GARY_DOUGLAS_RELEASE_HEAD_TURN:
            work->turnWeightRising = 0;
            break;
        case ACTOR_135400_GARY_DOUGLAS_DETACH_CARRIED:
            if (work->carriedTask != NULL) {
                work->carriedTask->spawnArg1.value = ACTOR_135400_CARRIED_PLACEMENT_DETACH;
            }
            break;
        case ACTOR_135400_GARY_DOUGLAS_SET_DOWN_CARRIED:
            if (work->carriedTask != NULL) {
                work->carriedTask->spawnArg1.value = ACTOR_135400_CARRIED_PLACEMENT_SET_DOWN;
                model                              = work->carriedTask->extra.tmd;
                model->flags                      &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}

/// Ticks Flint's animation and shadow, advances his clip cycle and releases deferred buffers.
///
/// Requires initialized nineteen-part playback and a cycle clip in 1..6. Slots
/// 1..18 and the cycle timer advance even while hidden. The hold timer reuses
/// killCountdown, wraps through a u16 intermediate and compares as s16; a clip
/// advances after its hold time is exceeded. External play messages leave this
/// cycle position and timer intact. A nonnegative freeCountdown decrements each
/// tick; buffers are released on the tick entering with 0, then the latch becomes -1.
static void _actor135400FlintTick(Task* task)
{
    _Actor135400FlintWork* work;
    TmdObject*             model;
    VECTOR3                groundPosition;
    s32                    slotIndex;
    s32                    freeCountdown;
    u16                    holdFrames;

    enum { ACTOR_135400_FLINT_SHADOW_HALF_SIZE = 0x180 };

    work  = task->work;
    model = task->extra.tmd;
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[0].workm), &groundPosition) != 0)) {
        effectDrawGroundShadow(&groundPosition, ACTOR_135400_FLINT_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
    }
    // Keep the original halfword wrap and signed hold-time comparison.
    holdFrames          = task->killCountdown + 1;
    task->killCountdown = holdFrames;
    if (D_actor_135400_8013F8C4[work->cycleRequest.animationId] < (s16)holdFrames) {
        work->cycleRequest.animationId = work->cycleRequest.animationId + 1;
        if (work->cycleRequest.animationId >= ACTOR_135400_FLINT_CYCLE_CLIP_LIMIT) {
            work->cycleRequest.animationId = ACTOR_135400_FLINT_FIRST_CYCLE_CLIP;
        }
        _actor135400FlintPlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &work->cycleRequest, 0);
        task->killCountdown = 0;
    }
    // Free once, when the deferred countdown enters this tick at zero.
    freeCountdown = work->freeCountdown;
    if (freeCountdown >= 0) {
        if (freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
            freeCountdown = work->freeCountdown;
        }
        freeCountdown      -= 1;
        work->freeCountdown = freeCountdown;
    }
}

/// State table of the second task: spawn, per-frame tick and exit callback.
/// Dispatched by `_actor135400FlintTask`.
static const TaskFuncTable3 D_actor_135400_80131E94 = { {
    _actor135400FlintSpawn,
    _actor135400FlintTick,
    _actor135400FlintExit,
} };

/// Dispatches Flint's spawn, tick or exit state while actor control is running.
///
/// Requires state 0..2 and live callback code; the selected handler can tear down
/// the task. Suspended actor control also suspends the cycle and buffer countdown.
static void _actor135400FlintTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_135400_80131E94;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

/// The animation arguments `_actor135400FlintSpawn` copies into
/// `_Actor135400FlintWork::cycleRequest` when the second task is created.
static const AnimationPlayRequest D_actor_135400_80131EA0 = { 0, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Initializes Flint before the night-garage event has progressed.
///
/// Exits if the garage progress flag is positive or work allocation fails.
/// Otherwise owns a zeroed work block, invalidates its initial bank and clip,
/// plays clip 1, and seeds the cycle cursor at 2 with ten-frame blends. Its first
/// timed transition therefore advances to clip 3, after the hold time of entry 2.
/// Shows the body, initializes its borrowed lighting matrices, and installs the
/// message table and teardown callback before advancing to tick state 1.
static void _actor135400FlintSpawn(Task* task)
{
    _Actor135400FlintWork* work;
    AnimationPlayRequest   initialRequest;
    AnimationPlayRequest   cycleRequest;

    memset(&initialRequest, 0, sizeof(initialRequest));
    initialRequest.animationId = ACTOR_135400_FLINT_FIRST_CYCLE_CLIP;
    cycleRequest               = D_actor_135400_80131EA0;
    if ((gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0) || ((work = memCalloc(sizeof(*work), 0)) == NULL)) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = ACTOR_135400_FLINT_BUFFER_FREE_INACTIVE;
    work->cycleRequest  = cycleRequest;
    _actor135400FlintPlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &initialRequest, 0);
    _actor135400FlintSetDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    _actor135400FlintInitLighting(task);
    task->msgTable     = D_actor_135400_8013F8E4;
    task->exitCallback = _actor135400FlintExit;
    task->state       += 1;
}

/// Releases Flint through the common enemy-task teardown.
///
/// Installed as both the exit callback and state 2; the work block and owned body
/// resources must remain live at entry and must not be used after teardown.
static void _actor135400FlintExit(Task* task)
{
    enemyTaskExit(task);
}

/// Binds Flint's work-owned lighting matrices and fills them from three flat lights.
///
/// Requires allocated work and a live TMD model. Its light and colour pointers
/// borrow the work block until teardown; the three light records are read only.
static void _actor135400FlintInitLighting(Task* task)
{
    _Actor135400FlintWork* work  = task->work;
    TmdObject*             model = task->extra.tmd;
    const GsF_LIGHT*       light;
    s32                    lightIndex;

    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
    for (lightIndex = 0, light = D_actor_135400_8013F904; lightIndex < ARRAY_SIZE(D_actor_135400_8013F904); lightIndex++, light++) {
        gfxSetFlatLight(lightIndex, light, &work->model.light, &work->model.color);
    }
}

/// Applies Flint's play request and immediately advances every driven model part.
///
/// Requires live work and a nineteen-part model, bank index 0 and clip ID 1..6.
/// The request is borrowed for this call and may be the work's cycle request,
/// but must not overlap the rig or model state. Playback borrows the work's
/// slot/pose arrays, model coordinates and loaded package clips until teardown.
/// Every request, including a repeated clip, drives slots 1..18; root slot 0 is
/// untouched. Nonzero blend interpolates an already ticking rig, otherwise the
/// slots restart. `blendFrames` is in whole frames (0..2047 keeps blend time
/// nonnegative); collision choice is ignored. Enables subsequent ticking and
/// leaves the cycle cursor, hold timer and deferred buffer release intact.
static inline void _actor135400FlintApplyAnimationRequest(_Actor135400FlintWork* work, TmdObject* model, const AnimationPlayRequest* request)
{
    enum { ACTOR_135400_FLINT_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    // Rebind borrowed playback storage only when the requested bank changes.
    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, D_actor_135400_8013F8D4[work->model.bank], model, work->rig.poses, work->rig.slots);
    }
    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = ACTOR_135400_FLINT_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = ACTOR_135400_FLINT_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    // Apply the new pose immediately before handing playback to the frame tick.
    for (slotIndex = ACTOR_135400_FLINT_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
}

/// Handles Flint's indexed play request, including repeated requests for the same clip.
///
/// Requires allocated work and a live nineteen-part model. Bank 0 and clip IDs
/// 1..6 select this package's bank; the borrowed request must stay live through
/// the call and must not alias playback state. A bank change binds the rig, then
/// slots 1..18 are blended for blendFrames whole frames (normally 0..2047) if
/// already ticking and blend is nonzero, or reset otherwise. Every request ticks those slots once
/// and enables subsequent ticking. Leaves cycle position, timer and deferred
/// release alone. Collision choice, messageId and unusedArg are ignored; returns 0.
static s32 _actor135400FlintPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor135400FlintWork* work;
    TmdObject*             model;

    work  = task->work;
    model = task->extra.tmd;
    _actor135400FlintApplyAnimationRequest(work, model, request);
    return 0;
}

/// Selects the private placement handler for the Flint model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Handles Flint's draw mode, deferring a mode-2 buffer release through the tick.
///
/// Requires live work and a TMD model. Modes 0/1 hide/show with automatic buffer
/// recovery enabled, and 1 also allocates the buffer. Modes 2/3 hide/show with
/// recovery disabled; mode 2 stores a two-tick countdown whose following tick
/// entering at zero releases the buffer. Other modes leave that countdown intact.
/// Returns 0 for modes 0..3, 1 otherwise. messageId and unusedArg are ignored.
static s32 _actor135400FlintSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            // The mode word also supplies the two-tick release delay.
            ((_Actor135400FlintWork*)task->work)->freeCountdown = mode;
            model->flags                                       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_135400_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}
