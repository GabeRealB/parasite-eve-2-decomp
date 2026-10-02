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
#include "../../shared/model_placement.h"

/// Work block of the actor's second task, the one `func_actor_135400_80132B60`
/// sets up: the `memCalloc(0x498, 0)` result it stores in `Task::work`, which
/// is therefore not a `TaskIdMap` here. The main task's spawn carves a
/// different, 0x4C8-byte `Actor135400MainWork`.
///
/// `rig` and `model` are the model's animation rig and state; the spawn
/// routine points the `TmdObject`'s `lightMtx` / `colorMtx` at the model
/// state's matrices and fills them from the three `D_actor_135400_8013F904`
/// lights. `params` holds the `D_actor_135400_80131EA0` defaults.
typedef struct Actor135400Work {
    ActorAnimRig19       rig;
    ActorModelState      model;
    AnimationPlayRequest params;
    s32                  field_494;
} Actor135400Work;
STATIC_ASSERT_SIZEOF(Actor135400Work, 0x498);

/// Work block of the actor's main task: `func_actor_135400_80132064`
/// allocates it (`memCalloc(0x4C8, 0)`) and parks the 0x7D3 / 0x7D4 / 0x7D5 /
/// 0x7DB handler table `D_actor_135400_8013A4D0` in that task's `msgTable`.
///
/// `rig` and `model` are the model's animation rig and state, whose
/// matrices the model is lit with.
///
/// `field_4B8` / `field_4BC` are the two part tasks the same spawn creates
/// through `Task_SpawnFromTable` (part 1 and part 2); each reparents itself
/// onto this task in its state 0 (`modelPlacementAttachPart` /
/// `func_actor_135400_8013252C`). `headAim` is the on/off latch the 0x7DB
/// handler `func_actor_135400_801328DC` sets and clears (its cases 2 and 3),
/// and `headRate` the 0x000..0xFFF rate the tick ramps toward or away from
/// the slot-3 target and hands `func_800B0928`. Only the fields decompiled
/// bodies reach are described.
typedef struct Actor135400MainWork {
    ActorAnimRig20    rig;
    ActorModelState   model;
    /* 0x4B8 */ Task* field_4B8;
    /* 0x4BC */ Task* field_4BC;
    /* 0x4C0 */ s32   headAim;
    /* 0x4C4 */ s32   headRate;
} Actor135400MainWork;
STATIC_ASSERT_SIZEOF(Actor135400MainWork, 0x4C8);

/// The two placements `func_actor_135400_80132064` starts the actor from. The
/// spawn copies the pair in one go and then hands the branch picked by game
/// flag 0x6C to the 0x7D4 handler `actorMsgPlaceEuler`.
typedef struct Actor135400Places {
    /* 0x00 */ ActorTransform field_0;
    /* 0x18 */ ActorTransform field_18;
} Actor135400Places;
STATIC_ASSERT_SIZEOF(Actor135400Places, 0x30);

/// The actor's two-entry `TaskDesc` table, indexed by `Task_SpawnFromTable`:
/// entry 1 is the model-bearing part task `modelPlacementAttachPart`
/// reparents, entry 2 the second part (`func_actor_135400_8013252C`).
extern TaskDesc D_actor_135400_8013A4AC[];

/// The handler table `func_actor_135400_80132064` parks in `Task::msgTable`:
/// the 0x7D3 / 0x7D4 / 0x7D5 / 0x7DB handlers of the main task.
/// Message entries with the payload signature selected by each message id.
typedef struct {
    s32 id; // Message id; `TASK_MESSAGE_TABLE_END` terminates the table
    union {
        s32                (*animation)(Task*, s32, AnimationPlayRequest*, s32);
        s32                (*placement)(Task*, s32, ActorTransform*, s32);
        TaskMessageHandler mode;
        TaskMessageHandler command;
    } handler; // Callback with the argument views required by that message
} _Actor135400MessageEntry;
STATIC_ASSERT_SIZEOF(_Actor135400MessageEntry, 8);

extern _Actor135400MessageEntry D_actor_135400_8013A4D0[5];

/// The three flat lights `func_actor_135400_80132CB0` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

/// The second task's message table: `(message id, handler)` pairs for 0x7D3 /
/// 0x7D4 / 0x7D5, ended by `TASK_MESSAGE_TABLE_END`. `func_actor_135400_80132B60` parks
/// its address in `Task::msgTable`.
extern _Actor135400MessageEntry D_actor_135400_8013F8E4[4];

/// Per-step frame counts of the second task's 0x7D3 animation: eight `s16`
/// entries indexed by `Actor135400Work::params.field_4`.
/// `func_actor_135400_801329B0` runs the task's `killCountdown` up and, once it
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

static void func_actor_135400_80131EB4(Task* task);
static void func_actor_135400_80132064(Task* arg0);
static void func_actor_135400_801322A8(Task* task);
static void func_actor_135400_801324CC(Task* task);
static void func_actor_135400_8013252C(Task* task);
static void func_actor_135400_80132614(Task* arg0);
static void func_actor_135400_80132634(Task* task);
s32         func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3);
static void func_actor_135400_80132B60(Task* arg0);
static void func_actor_135400_80132C90(Task* arg0);
static void func_actor_135400_80132CB0(Task* task);
s32         func_actor_135400_80132D24(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3);
s32         func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3);

/// State table of the first part task: state 0 reparents it
/// (`modelPlacementAttachPart`), state 1 does nothing and state 2 kills it.
/// Dispatched by `func_actor_135400_801323F8`.
static const TaskFuncTable3 D_actor_135400_80131E24 = { {
    modelPlacementAttachPart,
    func_actor_135400_801324CC,
    taskKill,
} };

/// State table of the second part task: state 0 reparents it
/// (`func_actor_135400_8013252C`), state 1 runs its placement phases
/// (`func_actor_135400_80131EB4`) and state 2 kills it. Dispatched by
/// `func_actor_135400_801324D4`.
static const TaskFuncTable3 D_actor_135400_80131E30 = { {
    func_actor_135400_8013252C,
    func_actor_135400_80131EB4,
    taskKill,
} };

/// State table of the main task: spawn, per-frame tick and exit callback.
/// Dispatched by `func_actor_135400_801325A8`.
static const TaskFuncTable3 D_actor_135400_80131E3C = { {
    func_actor_135400_80132064,
    func_actor_135400_801322A8,
    func_actor_135400_80132614,
} };

/// The two spawn placements `func_actor_135400_80132064` copies as a whole:
/// the flag-clear branch's first, the other second.
static const Actor135400Places D_actor_135400_80131E48 = {
    { { 5700, -150, 5900, 0 }, { 1024, 0, -1024, 0 } },
    { { 4700, 0, 5000, 0 }, { 0, -1024, 0, 0 } },
};

static TmdSource _gActor135400FlintBody;
s32              func_actor_135400_80132D24(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_135400_80132E40(Task*, s32, ActorTransform* args, s32);
s32              func_actor_135400_80132EBC(Task*, s32, s32, s32);
void             func_actor_135400_80132AF4(Task*);

s32  func_actor_135400_801327E8(Task*, s32, s32, s32);
s32  func_actor_135400_801328DC(Task*, s32, ActorCommand* msg, s32);
void func_actor_135400_801323F8(Task*);
void func_actor_135400_801324D4(Task*);
void func_actor_135400_801325A8(Task*);

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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_135400_801325A8, { .model = &_gActor135400GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_135400_801323F8, { .model = &_gActor135400GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_135400_801324D4, { .model = &_gActor135400Model071AC } },
};

_Actor135400MessageEntry D_actor_135400_8013A4D0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, { .animation = actorMotionPlayAnim } },
    { ACTOR_MESSAGE_PLACE, { .placement = actorMsgPlaceEuler } },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .mode = func_actor_135400_801327E8 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .command = func_actor_135400_801328DC } },
    { TASK_MESSAGE_TABLE_END, { .animation = NULL } },
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

TaskDesc D_actor_135400_8013F8D8 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_135400_80132AF4, { .model = &_gActor135400FlintBody } };

_Actor135400MessageEntry D_actor_135400_8013F8E4[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, { .animation = func_actor_135400_80132D24 } },
    { ACTOR_MESSAGE_PLACE, { .placement = func_actor_135400_80132E40 } },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .mode = func_actor_135400_80132EBC } },
    { TASK_MESSAGE_TABLE_END, { .animation = NULL } },
};

GsF_LIGHT D_actor_135400_8013F904[3] = {
    { 0, 4096, 0, 96, 96, 96 },
    { 4096, 0, 0, 96, 96, 96 },
    { 0, 0, 4096, 96, 96, 96 },
};

static void func_actor_135400_801329B0(Task* task);

/// Second state handler of the actor's part-2 table (`D_actor_135400_80131E30`,
/// dispatched by `func_actor_135400_801324D4`): a three-phase machine run off
/// `Task::spawnArg1`, the slot the part's state-0 handler read as the part
/// index and sets to 2 for the second part. Phase 1 bakes the part's
/// parent-relative coordinate into world space with `Gp_ComposeParentWorld` and
/// reparents it to `gGfxViewCoord`. Phases 2 and 3 share a body -- 2 only
/// reaches it while the session's `eventState` is clear -- which resets the
/// coordinate to a `-0x38E` yaw (`RotMatrixY`, `RotMatrixY`) at the fixed
/// world position (0x12FE, -0x1B3, 0x157C) and drops the phase back to 0.
static void func_actor_135400_80131EB4(Task* task)
{
    OverlayMat  rot;
    OverlayMat* src;
    SVECTOR     sv;
    GfxCoord*   coord;

    switch (task->spawnArg1.value) {
        case 1:
            coord = task->extra.tmd->coords;
            Gp_ComposeParentWorld(coord, &rot.mat, &sv);
            coord->coord           = rot.mat;
            coord->coord.t[0]      = sv.vx;
            coord->coord.t[1]      = sv.vy;
            coord->coord.t[2]      = sv.vz;
            coord->parent          = &gGfxViewCoord;
            coord->composeStamp    = GRAPHICS_COORD_DIRTY;
            task->spawnArg1.value += 1;
            break;
        case 2:
            if (gGameSession->eventState != 0) {
                break;
            }
        case 3:
            coord             = task->extra.tmd->coords;
            src               = &rot;
            src->ident.m00M01 = ONE;
            src->ident.m02M10 = 0;
            src->ident.m11M12 = ONE;
            src->ident.m20M21 = 0;
            src->ident.m22    = ONE;
            RotMatrixY(-0x38E, &rot.mat);
            coord->coord          = rot.mat;
            coord->coord.t[0]     = 0x12FE;
            coord->coord.t[1]     = -0x1B3;
            coord->coord.t[2]     = 0x157C;
            coord->parent         = &gGfxViewCoord;
            coord->composeStamp   = GRAPHICS_COORD_DIRTY;
            task->spawnArg1.value = 0;
            break;
    }
}

/// The spawn handler of the actor's main task: carves the 0x4C8-byte work
/// block, seeds its two `-1` latches, starts the two part tasks and copies the
/// area record's texture page / CLUT onto part 1's model. It then installs the
/// handler table, the 0x7D5 model mode and the exit callback, and finally hands
/// the 0x7D4 placement and the 0x7D3 animation the game flag 0x6C selects.
static void func_actor_135400_80132064(Task* arg0)
{
    Actor135400MainWork* work;
    Actor135400Places    places;
    AnimationPlayRequest anim[2];
    Task*                spawned;

    places = D_actor_135400_80131E48;
    memset(anim, 0, sizeof(anim));
    anim[0].animationId = 1;
    anim[1].animationId = 4;
    work                = memCalloc(0x4C8, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work         = work;
    work->model.animId = -1;
    work->model.bank   = -1;
    spawned            = Task_SpawnFromTable(D_actor_135400_8013A4AC, 1, 4, arg0);
    if (spawned != NULL) {
        work->field_4B8 = spawned;
        actorTintTask(spawned, (Enemy*)arg0->spawnArg2.pointer);
    }
    spawned = Task_SpawnFromTable(D_actor_135400_8013A4AC, 2, 8, arg0);
    if (spawned != NULL) {
        work->field_4BC = spawned;
    }
    func_actor_135400_80132634(arg0);
    arg0->msgTable = D_actor_135400_8013A4D0;
    func_actor_135400_801327E8(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    if (GameFlag_GetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) <= 0) {
        actorMsgPlaceEuler(arg0, ACTOR_MESSAGE_PLACE, &places.field_0, 0);
        actorMotionPlayAnim(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &anim[0], 0);
        func_dryfield_night_garage_80180414(0);
    } else {
        actorMsgPlaceEuler(arg0, ACTOR_MESSAGE_PLACE, &places.field_18, 0);
        actorMotionPlayAnim(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &anim[1], 0);
    }
    arg0->exitCallback = func_actor_135400_80132614;
    arg0->state       += 1;
}

/// Per-frame tick of the actor's main task: ticks the twenty animation slots
/// once `model.ticking` has latched, and while the model is not hidden (flag 0x80
/// of `TmdObject::flags`) draws its ground shadow from the second
/// part's translation, recomputes that part's world matrix, re-ranks it
/// through `func_800D7A9C`, ramps the head-tracking rate `headRate` and finally
/// turns the head toward the slot-3 skeleton with `func_800B0928`.
static void func_actor_135400_801322A8(Task* task)
{
    Actor135400MainWork* work;
    TmdObject*           ext;
    VECTOR3              pos;
    s32                  i;
    s32                  rate;

    work = (Actor135400MainWork*)task->work;
    ext  = task->extra.tmd;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
        if (work->headAim != 0) {
            rate           = work->headRate + 0x100;
            work->headRate = rate;
            if (rate >= 0x1000) {
                work->headRate = 0xFFF;
            }
        } else {
            rate           = work->headRate - 0x80;
            work->headRate = rate;
            if (rate < 0) {
                work->headRate = 0;
            }
        }
        func_800B0928(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, work->headRate);
    }
}

/// Per-frame dispatcher of the first part task: runs its state from
/// `D_actor_135400_80131E24`.
void func_actor_135400_801323F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// State 1 of the first part task: nothing to do while it rides its parent.
static void func_actor_135400_801324CC(Task* task)
{
}

/// Per-frame dispatcher of the second part task: runs its state from
/// `D_actor_135400_80131E30`.
void func_actor_135400_801324D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E30;
    sp.funcs[task->state](task);
}

/// A second copy, under this file's own name.
#define modelPlacementAttachPart func_actor_135400_8013252C
#include "../../shared/model_placement_attach_part.inc.c"
#undef modelPlacementAttachPart

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_135400_80131E3C`, skipping the frame while `gSceneCombatState.actorControl` is
/// set.
void func_actor_135400_801325A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E3C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// The main task's exit callback, also its state 2: runs the enemy teardown.
static void func_actor_135400_80132614(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the main task's model at the work block's own light and colour
/// matrices, so it draws with the actor's lighting rather than the defaults.
/// The spawn handler runs it once the two part tasks are started.
static void func_actor_135400_80132634(Task* task)
{
    TmdObject*           ext;
    Actor135400MainWork* work;

    ext           = task->extra.tmd;
    work          = (Actor135400MainWork*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// The main task's 0x7D5 handler, a four-way model mode switch on the
/// `TmdObject` in `Task::extra`. Mode 0 hides the model (flag 0x80) and clears
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`; 1 shows it, allocates its buffers and clears
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`; 2 hides it,
/// frees the buffers and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`; 3 shows it and sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`. Anything else
/// returns 1 and leaves the model alone; the handled modes return 0. Either
/// way the resulting flags are copied onto the first part task's model
/// (`Actor135400MainWork::field_4B8`), keeping the pair in step.
s32 func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    TmdObject* obj;
    TmdObject* other;
    s32        ret;

    obj   = task->extra.tmd;
    other = ((Actor135400MainWork*)task->work)->field_4B8->extra.tmd;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_FreeBuffers(obj);
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}

/// The main task's 0x7DB handler. The payload halfword picks one of six
/// actions against the second part task (`Actor135400MainWork::field_4BC`):
/// 0 and 1 show and hide that task's model (flag 0x80), 2 and 3 set
/// and clear `headAim`, 4 hands the part task a `spawnArg1` of 1, and 5 sets
/// that to 3 and then shows the model. Nothing reads the message id.
s32 func_actor_135400_801328DC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    Actor135400MainWork* work;
    TmdObject*           model;

    work = (Actor135400MainWork*)task->work;
    switch (msg->command) {
        case 0:
            if (work->field_4BC != NULL) {
                model         = work->field_4BC->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 1:
            if (work->field_4BC != NULL) {
                model         = work->field_4BC->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 2:
            work->headAim = 1;
            break;
        case 3:
            work->headAim = 0;
            break;
        case 4:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1.value = 1;
            }
            break;
        case 5:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1.value = 3;
                model                            = work->field_4BC->extra.tmd;
                model->flags                    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}

/// Per-frame tick of the second task. Once `func_actor_135400_80132D24` has
/// raised `model.ticking` it ticks slots 1..18 of the work block through
/// `animationTickSlot`; while the model is not hidden (flag 0x80 of
/// `TmdObject::flags`) it draws the ground shadow under the model's
/// root part, as `func_actor_135400_801322A8` does for the main task. It then
/// steps the 0x7D3 animation on the `D_actor_135400_8013F8C4` frame counts, and
/// finally runs `field_494` down -- at zero the model's aux buffers are freed
/// and the countdown carries on to -1, so that free happens once.
static void func_actor_135400_801329B0(Task* task)
{
    Actor135400Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s32              i;
    s32              step;
    u16              count;

    work = (Actor135400Work*)task->work;
    ext  = task->extra.tmd;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[0].workm), &pos) != 0)) {
        Gp_DrawEffGroundQuad(&pos, 0x180, gRoomEffectState->groundShadowShade);
    }
    count               = task->killCountdown + 1;
    task->killCountdown = count;
    if (D_actor_135400_8013F8C4[work->params.animationId] < (s16)count) {
        work->params.animationId = work->params.animationId + 1;
        if (work->params.animationId >= 7) {
            work->params.animationId = 1;
        }
        func_actor_135400_80132D24(task, ACTOR_MESSAGE_PLAY_ANIMATION, &work->params, 0);
        task->killCountdown = 0;
    }
    step = work->field_494;
    if (step >= 0) {
        if (step == 0) {
            Tmd_FreeBuffers(ext);
            step = work->field_494;
        }
        step           -= 1;
        work->field_494 = step;
    }
}

/// State table of the second task: spawn, per-frame tick and exit callback.
/// Dispatched by `func_actor_135400_80132AF4`.
static const TaskFuncTable3 D_actor_135400_80131E94 = { {
    func_actor_135400_80132B60,
    func_actor_135400_801329B0,
    func_actor_135400_80132C90,
} };

/// Per-frame dispatcher of the task `func_actor_135400_80132B60` sets up: runs
/// its spawn, tick or exit state from `D_actor_135400_80131E94`, skipping the
/// frame while `gSceneCombatState.actorControl` is set.
void func_actor_135400_80132AF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E94;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// The animation arguments `func_actor_135400_80132B60` copies into
/// `Actor135400Work::params` when the second task is created.
static const AnimationPlayRequest D_actor_135400_80131EA0 = { 0, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Spawn state of the second task: exits at once when game flag 0x6C is set or
/// the 0x498-byte work block cannot be allocated. Otherwise it seeds the
/// block's latches, stores the `D_actor_135400_80131EA0` defaults, starts
/// animation 1, shows the model through the 0x7D5 handler, loads its flat
/// lights, and installs the message table and exit callback.
static void func_actor_135400_80132B60(Task* arg0)
{
    Actor135400Work*     work;
    AnimationPlayRequest params;
    AnimationPlayRequest spawn;

    memset(&params, 0, sizeof(params));
    params.animationId = 1;
    spawn              = D_actor_135400_80131EA0;
    if ((GameFlag_GetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0) || ((work = memCalloc(0x498, 0)) == NULL)) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work         = work;
    work->model.animId = -1;
    work->model.bank   = -1;
    work->field_494    = -1;
    work->params       = spawn;
    func_actor_135400_80132D24(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &params, 0);
    func_actor_135400_80132EBC(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    func_actor_135400_80132CB0(arg0);
    arg0->msgTable     = D_actor_135400_8013F8E4;
    arg0->exitCallback = func_actor_135400_80132C90;
    arg0->state       += 1;
}

/// The second task's exit callback, also its state 2: runs the enemy teardown.
static void func_actor_135400_80132C90(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the second task's model at the work block's light and colour
/// matrices and fills them from the three `D_actor_135400_8013F904` lights.
static void func_actor_135400_80132CB0(Task* task)
{
    Actor135400Work* work = (Actor135400Work*)task->work;
    TmdObject*       obj  = task->extra.tmd;
    GsF_LIGHT*       light;
    s32              i;

    obj->lightMtx = &work->model.light;
    obj->colorMtx = &work->model.color;
    for (i = 0, light = D_actor_135400_8013F904; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->model.light, &work->model.color);
    }
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_135400_80132D24(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3)
{
    Actor135400Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor135400Work*)task->work;
    ext  = task->extra.tmd;
    if (params->source.index != work->model.bank) {
        work->model.bank = params->source.index;
        animationInitContext(&work->rig.anim, D_actor_135400_8013F8D4[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    work->model.animId = params->animationId;
    if (params->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, params->blendFrames);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            animationResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x13; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// A second copy, under this file's own name.
#define actorMsgPlaceEuler func_actor_135400_80132E40
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

/// The second task's 0x7D5 handler, the same mode switch as
/// `func_actor_135400_801327E8` on this task's model alone. Mode 2 does not
/// free the buffers itself: it stores 2 in `field_494`, and the tick frees
/// them once that countdown reaches zero.
s32 func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (arg2) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Actor135400Work*)task->work)->field_494 = arg2;
            obj->flags                               |= TMD_OBJECT_SKIP_AUTO_BUFFER;
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
