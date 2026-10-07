#include "actors/actor_361100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actor_403600.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Work block of the tentacle drawn with `_gActor361100Model06038`, the first
/// of the package's two scripted models: a tube of ten segments about nine
/// times as long as Aya is tall, whose tip carries four two-jointed flaps.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotion19PlayWork` does, which the
/// play handler of the actor-motion library runs on, and the model object
/// borrows `model.light` and `model.color` for as long as the block lives.
///
/// What follows moves the model in a straight line: each tick adds `velocity`
/// to `carry` and moves the root coordinate by the whole units that makes. A
/// move command sets `velocity` to a displacement divided by a frame count and
/// `moveFrames` to that count; placing the actor stops the move.
///
/// The layout is `_Actor361100AyaBreaWork`'s with the velocity and the carry
/// in each other's place. No code reads or writes the word after `carry`, the
/// fourth word of `velocity`, or `pad_4A3`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    Fixed16         carry[3];      // X, Y and Z displacement not yet applied; only the fractions survive a tick
    byte            pad_48C[0x4];
    VECTOR          velocity;      // Displacement added each tick, in signed 16.16 units; zero while standing
    s16             moveFrames;    // Ticks the move still has to run; the tick finding 0 applies `velocity` once more, clears it and leaves -1 (0 from the spawn, -1 once no move is counting)
    s8              freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4A3[0x1];
} _Actor361100TentacleWork;
STATIC_ASSERT_SIZEOF(_Actor361100TentacleWork, 0x4A4);

/// Work block of Aya Brea's body, the second of the package's two scripted
/// models.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the model's nineteen-part rig and model
/// state, and the model object borrows `model.light` and `model.color` for as
/// long as the block lives.
///
/// What follows moves the model in a straight line: each tick adds `velocity`
/// to `carry` and moves the root coordinate by the whole units that makes. A
/// move command sets `velocity` to a displacement divided by a frame count and
/// `moveFrames` to that count.
///
/// No code reads or writes the fourth word of `velocity`, `pad_49C` or
/// `pad_4A3`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    VECTOR          velocity;      // Displacement added each tick, in signed 16.16 units; zero while standing
    Fixed16         carry[3];      // X, Y and Z displacement not yet applied; only the fractions survive a tick
    byte            pad_49C[0x4];
    s16             moveFrames;    // Ticks the move still has to run; the tick finding 1 applies `velocity` a last time and clears it (0 no move)
    s8              freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4A3[0x1];
} _Actor361100AyaBreaWork;
STATIC_ASSERT_SIZEOF(_Actor361100AyaBreaWork, 0x4A4);

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58[];

/// Script pair handed to `padScriptSpawn` on every even frame of the blink.
extern PadScriptCmd              D_actor_361100_80166AD0[2];
extern PadScriptVibrationSegment D_actor_361100_80166AD8;

extern AnimationSet*  D_actor_361100_8016BAD0[4];
extern AnimationSet** gActorMotionAnimBanks19[1];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_361100_8016BAF0[];
extern AnimationSet*    D_actor_361100_80171B94[5];
extern AnimationSet**   D_actor_361100_80171BA8[1];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_361100_80171BB8[5];

void func_actor_403600_80138C9C(Actor403600Ripple* state);
void func_actor_403600_801353D0(Actor403600Ripple* state, GfxCoord* coord);

static void _actor361100DrawStreamRefraction(Task* task);

/// Draw modes with deferred primitive-buffer release or manual buffer recovery.
///
/// Modes 0 and 1 use `ACTOR_MESSAGE_DRAW_HIDE` and `ACTOR_MESSAGE_DRAW_SHOW`.
/// Releasing stores mode 2 as the countdown: ticks seeing 2, 1, then 0 free it.
enum {
    ACTOR_361100_DRAW_HIDE_RELEASE         = 2,
    ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER = 3,
};

/// No primitive-buffer release pending in either model task.
enum { ACTOR_361100_BUFFER_FREE_IDLE = -1 };

/// One parent-relative coordinate unit in signed 16.16 translation.
enum { ACTOR_361100_TRANSLATION_ONE = 0x10000 };

/// Script-selected head-aim policy and local descriptor slots.
enum {
    ACTOR_361100_HEAD_AIM_RELAX      = 0,
    ACTOR_361100_HEAD_AIM_TRACK      = 1,
    ACTOR_361100_HEAD_AIM_KILL       = -1,
    ACTOR_361100_HEAD_AIM_TASK_INDEX = 0,
    ACTOR_361100_SHAKE_TASK_INDEX    = 1,
};

static void _actor361100TickTentacle(Task* task);
static void _actor361100InitTentacle(Task* task);
static void _actor361100ExitTentacle(Task* task);
static void _actor361100BindTentacleLighting(Task* task);
static void _actor361100TickAyaBrea(Task* task);
static void _actor361100InitAyaBrea(Task* task);
static void _actor361100ExitAyaBrea(Task* task);
static void _actor361100BindAyaBreaLighting(Task* task);

/// `Task::state` handlers `_actor361100TentacleTask` dispatches through.
static const TaskFuncTable3 D_actor_361100_80161E24 = {
    {
        _actor361100InitTentacle,
        _actor361100TickTentacle,
        _actor361100ExitTentacle,
    },
};

/// `Task::state` handlers `_actor361100AyaBreaTask` dispatches through.
static const TaskFuncTable3 D_actor_361100_80161E30 = {
    {
        _actor361100InitAyaBrea,
        _actor361100TickAyaBrea,
        _actor361100ExitAyaBrea,
    },
};

static AnimationSet _gActor361100Animation0973C;
static AnimationSet _gActor361100Animation09AA8;
static AnimationSet _gActor361100Animation09C88;
static TmdSource    _gActor361100Model06038;
static s32          _actor361100PlaceTentacle(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32          _actor361100SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32          _actor361100ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static s32          _actor361100PlayAyaBreaAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg);
static s32          _actor361100SetAyaBreaDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32          _actor361100ApplyAyaBreaCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static void         _actor361100TentacleTask(Task* task);
static void         _actor361100AyaBreaTask(Task* task);

extern AnimationPlayRequest D_actor_361100_80165CA0;
extern ActorCommand         D_actor_361100_80165DD4;
extern ActorCommand         D_actor_361100_80165E84;
extern ActorCommand         D_actor_361100_80165F3C;
extern ActorTransform       D_actor_361100_80165D98;
void                        func_actor_361100_8016297C(void);
static void                 _actor361100SetHeadAimMode(s32 mode);
static void                 _actor361100AddFlowFlags(s32 bits);

extern AnimationPlayRequest      D_actor_361100_80165CB4;
extern AnimationPlayRequest      D_actor_361100_80165CC8;
extern AnimationPlayRequest      D_actor_361100_80165CDC;
extern AnimationPlayRequest      D_actor_361100_80165CF0;
extern AnimationPlayRequest      D_actor_361100_80165D04;
extern AnimationPlayRequest      D_actor_361100_80165D18;
extern AnimationPlayRequest      D_actor_361100_80165D40;
extern AnimationPlayRequest      D_actor_361100_80165D54;
extern AnimationPlayRequest      D_actor_361100_80165DF0;
extern AnimationPlayRequest      D_actor_361100_80165E04;
extern AnimationPlayRequest      D_actor_361100_80165E18;
extern AnimationPlayRequest      D_actor_361100_80165E9C;
extern AnimationPlayRequest      D_actor_361100_80165EB0;
extern AnimationPlayRequest      D_actor_361100_80165EC4;
extern AnimationPlayRequest      D_actor_361100_80165ED8;
extern ActorCommand              D_actor_361100_80165DD0;
extern ActorCommand              D_actor_361100_80165DD8;
extern ActorCommand              D_actor_361100_80165E78;
extern ActorCommand              D_actor_361100_80165E7C;
extern ActorCommand              D_actor_361100_80165E80;
extern AnimationBankCopyRequest  D_actor_361100_80165C98;
extern GameActorMoveAnim         D_actor_361100_80165DC8;
extern PadScriptCmd              D_actor_361100_80166AB8[3];
extern PadScriptVibrationSegment D_actor_361100_80166AC4[3];
extern ActorTransform            D_actor_361100_80165D68;
extern ActorTransform            D_actor_361100_80165D80;
extern ActorTransform            D_actor_361100_80165DB0;
extern ActorTransform            D_actor_361100_80165E2C;
extern ActorTransform            D_actor_361100_80165E5C;
extern ActorTransform            D_actor_361100_80165EEC;
extern ActorTransform            D_actor_361100_80165F04;
extern ActorTransform            D_actor_361100_80165F1C;
static void                      _actor361100StageSceneAudioStart(void);
static void                      _actor361100StartScenePlayback(void);
static void                      _actor361100FinishScene(void);
void                             func_actor_361100_8016297C(void);
static void                      _actor361100SpawnHeadAimTask(void);
static void                      _actor361100SpawnShakeTask(s32 durationTicks);

void        func_actor_361100_80161E3C(Task*);
static void _actor361100HeadAimTask(Task* task);
void        func_actor_361100_80162A54(Task*);

TaskDesc D_actor_361100_801637C8 = { { { TASK_BODY_COORD, 192 } }, func_actor_361100_80161E3C, { .value = 0 } };

static AnimationPackedPose _gActor361100Animation01C90Bank1[6] = {
#include "assets/actor_361100_animation_01C90_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation01C90Bank4[46] = {
#include "assets/actor_361100_animation_01C90_bank4.inc"
};

static AnimationRecord _gActor361100Animation01C90Records[109] = {
#include "assets/actor_361100_animation_01C90_records.inc"
};

static u16 _gActor361100Animation01C90Indices[20] = {
#include "assets/actor_361100_animation_01C90_indices.inc"
};

static AnimationSet _gActor361100Animation01C90 = {
    _gActor361100Animation01C90Records,
    _gActor361100Animation01C90Indices,
    { NULL, _gActor361100Animation01C90Bank1, NULL, NULL, _gActor361100Animation01C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation01F98Bank1[3] = {
#include "assets/actor_361100_animation_01F98_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation01F98Bank4[68] = {
#include "assets/actor_361100_animation_01F98_bank4.inc"
};

static AnimationRecord _gActor361100Animation01F98Records[97] = {
#include "assets/actor_361100_animation_01F98_records.inc"
};

static u16 _gActor361100Animation01F98Indices[20] = {
#include "assets/actor_361100_animation_01F98_indices.inc"
};

static AnimationSet _gActor361100Animation01F98 = {
    _gActor361100Animation01F98Records,
    _gActor361100Animation01F98Indices,
    { NULL, _gActor361100Animation01F98Bank1, NULL, NULL, _gActor361100Animation01F98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation0227CBank1[4] = {
#include "assets/actor_361100_animation_0227C_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0227CBank4[57] = {
#include "assets/actor_361100_animation_0227C_bank4.inc"
};

static AnimationRecord _gActor361100Animation0227CRecords[96] = {
#include "assets/actor_361100_animation_0227C_records.inc"
};

static u16 _gActor361100Animation0227CIndices[20] = {
#include "assets/actor_361100_animation_0227C_indices.inc"
};

static AnimationSet _gActor361100Animation0227C = {
    _gActor361100Animation0227CRecords,
    _gActor361100Animation0227CIndices,
    { NULL, _gActor361100Animation0227CBank1, NULL, NULL, _gActor361100Animation0227CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation02494Bank1[2] = {
#include "assets/actor_361100_animation_02494_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation02494Bank4[22] = {
#include "assets/actor_361100_animation_02494_bank4.inc"
};

static AnimationRecord _gActor361100Animation02494Records[86] = {
#include "assets/actor_361100_animation_02494_records.inc"
};

static u16 _gActor361100Animation02494Indices[20] = {
#include "assets/actor_361100_animation_02494_indices.inc"
};

static AnimationSet _gActor361100Animation02494 = {
    _gActor361100Animation02494Records,
    _gActor361100Animation02494Indices,
    { NULL, _gActor361100Animation02494Bank1, NULL, NULL, _gActor361100Animation02494Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation02768Bank1[4] = {
#include "assets/actor_361100_animation_02768_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation02768Bank4[47] = {
#include "assets/actor_361100_animation_02768_bank4.inc"
};

static AnimationRecord _gActor361100Animation02768Records[102] = {
#include "assets/actor_361100_animation_02768_records.inc"
};

static u16 _gActor361100Animation02768Indices[20] = {
#include "assets/actor_361100_animation_02768_indices.inc"
};

static AnimationSet _gActor361100Animation02768 = {
    _gActor361100Animation02768Records,
    _gActor361100Animation02768Indices,
    { NULL, _gActor361100Animation02768Bank1, NULL, NULL, _gActor361100Animation02768Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation02A50Bank1[2] = {
#include "assets/actor_361100_animation_02A50_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation02A50Bank4[60] = {
#include "assets/actor_361100_animation_02A50_bank4.inc"
};

static AnimationRecord _gActor361100Animation02A50Records[100] = {
#include "assets/actor_361100_animation_02A50_records.inc"
};

static u16 _gActor361100Animation02A50Indices[20] = {
#include "assets/actor_361100_animation_02A50_indices.inc"
};

static AnimationSet _gActor361100Animation02A50 = {
    _gActor361100Animation02A50Records,
    _gActor361100Animation02A50Indices,
    { NULL, _gActor361100Animation02A50Bank1, NULL, NULL, _gActor361100Animation02A50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation02CA0Bank1[3] = {
#include "assets/actor_361100_animation_02CA0_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation02CA0Bank4[29] = {
#include "assets/actor_361100_animation_02CA0_bank4.inc"
};

static AnimationRecord _gActor361100Animation02CA0Records[90] = {
#include "assets/actor_361100_animation_02CA0_records.inc"
};

static u16 _gActor361100Animation02CA0Indices[20] = {
#include "assets/actor_361100_animation_02CA0_indices.inc"
};

static AnimationSet _gActor361100Animation02CA0 = {
    _gActor361100Animation02CA0Records,
    _gActor361100Animation02CA0Indices,
    { NULL, _gActor361100Animation02CA0Bank1, NULL, NULL, _gActor361100Animation02CA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation035FCBank1[15] = {
#include "assets/actor_361100_animation_035FC_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation035FCBank4[233] = {
#include "assets/actor_361100_animation_035FC_bank4.inc"
};

static AnimationRecord _gActor361100Animation035FCRecords[301] = {
#include "assets/actor_361100_animation_035FC_records.inc"
};

static u16 _gActor361100Animation035FCIndices[20] = {
#include "assets/actor_361100_animation_035FC_indices.inc"
};

static AnimationSet _gActor361100Animation035FC = {
    _gActor361100Animation035FCRecords,
    _gActor361100Animation035FCIndices,
    { NULL, _gActor361100Animation035FCBank1, NULL, NULL, _gActor361100Animation035FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation03E10Bank1[14] = {
#include "assets/actor_361100_animation_03E10_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation03E10Bank4[207] = {
#include "assets/actor_361100_animation_03E10_bank4.inc"
};

static AnimationRecord _gActor361100Animation03E10Records[248] = {
#include "assets/actor_361100_animation_03E10_records.inc"
};

static u16 _gActor361100Animation03E10Indices[20] = {
#include "assets/actor_361100_animation_03E10_indices.inc"
};

static AnimationSet _gActor361100Animation03E10 = {
    _gActor361100Animation03E10Records,
    _gActor361100Animation03E10Indices,
    { NULL, _gActor361100Animation03E10Bank1, NULL, NULL, _gActor361100Animation03E10Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_361100_80165C58[2] = {
    { { { TASK_BODY_NONE, 192 } }, _actor361100HeadAimTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_361100_80162A54, { .value = 0 } },
};

AnimationSet* D_actor_361100_80165C70[10] = {
    NULL,
    &_gActor361100Animation01C90,
    &_gActor361100Animation01F98,
    &_gActor361100Animation0227C,
    &_gActor361100Animation02494,
    &_gActor361100Animation02768,
    &_gActor361100Animation02A50,
    &_gActor361100Animation035FC,
    &_gActor361100Animation02CA0,
    &_gActor361100Animation03E10,
};

AnimationBankCopyRequest D_actor_361100_80165C98 = { { .sets = D_actor_361100_80165C70 }, ARRAY_SIZE(D_actor_361100_80165C70) };

AnimationPlayRequest D_actor_361100_80165CA0 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165CB4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165CC8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165CDC = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165CF0 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165D04 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165D18 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165D2C = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165D40 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165D54 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_361100_80165D68 = { { 0x2D14, -4010, 3190, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_361100_80165D80 = { { 8310, -4010, 6490, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_361100_80165D98 = { { 7760, -4010, 7010, 0 }, { 0, -1480, 0, 0 } };

ActorTransform D_actor_361100_80165DB0 = { { 3836, -8010, 0x2A13, 0 }, { 0, 0, 0, 0 } };

GameActorMoveAnim D_actor_361100_80165DC8 = { 54, 48 };

ActorCommand D_actor_361100_80165DD0 = { { .loc = { 4, 22 } }, 6 };

ActorCommand D_actor_361100_80165DD4 = { { .loc = { 4, 22 } }, 7 };

ActorCommand D_actor_361100_80165DD8 = { { .loc = { 4, 22 } }, 8 };

AnimationPlayRequest D_actor_361100_80165DDC = { 0 };

AnimationPlayRequest D_actor_361100_80165DF0 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165E04 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165E18 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_361100_80165E2C = { { 8200, 9270, 4060, 0 }, { 0, 1536, 0, 0 } };

ActorTransform D_actor_361100_80165E44 = { { 7970, 1300, 4350, 0 }, { 0, 1536, 0, 0 } };

ActorTransform D_actor_361100_80165E5C = { { 4460, 470, 8650, 0 }, { 1024, -341, 0, 0 } };

ActorCommand D_actor_361100_80165E74 = { { .loc = { 4, 22 } }, 0 };

ActorCommand D_actor_361100_80165E78 = { { .loc = { 4, 22 } }, 1 };

ActorCommand D_actor_361100_80165E7C = { { .loc = { 4, 22 } }, 2 };

ActorCommand D_actor_361100_80165E80 = { { .loc = { 4, 22 } }, 3 };

ActorCommand D_actor_361100_80165E84 = { { .loc = { 4, 22 } }, 0xFFFF };

AnimationPlayRequest D_actor_361100_80165E88 = { 0 };

AnimationPlayRequest D_actor_361100_80165E9C = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165EB0 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165EC4 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_361100_80165ED8 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_361100_80165EEC = { { 4440, -0x3250, 6960, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_361100_80165F04 = { { 6870, 1830, 5310, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_361100_80165F1C = { { 7370, 2550, 6228, 0 }, { 371, 885, -450, 0 } };

ActorCommand D_actor_361100_80165F34 = { { .loc = { 4, 22 } }, 0 };

ActorCommand D_actor_361100_80165F38 = { { .loc = { 4, 22 } }, 1 };

ActorCommand D_actor_361100_80165F3C = { { .loc = { 4, 22 } }, 0xFFFF };

EvsSceneKey D_actor_361100_80165F40 = { 6, 11, 21 };

EvsCommand D_actor_361100_80165F48[96] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_361100_80165F40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_361100_80165C98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_361100_80165D68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165DD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100SpawnHeadAimTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_actor_361100_80165D80 } }, { .message = { .pointer = &D_actor_361100_80165DC8 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165DB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165D18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_KILL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165EC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165E2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165DF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165E78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_361100_80166AB8 }, { .vibrationSegments = D_actor_361100_80166AC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165E5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165E04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165E7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165E18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165E80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_361100_80165D98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165D04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165F04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165E9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165F1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165E9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165F38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SpawnShakeTask }, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165D54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165D40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165DD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor361100FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016297C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165E84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_361100_80166848[26] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165DD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_361100_80165D98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016297C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100SetHeadAimMode }, { .value = ACTOR_361100_HEAD_AIM_KILL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor361100AddFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165E84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

PadScriptCmd D_actor_361100_80166AB8[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 11), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_361100_80166AC4[3] = {
    { 3, 255, 15, 1 },
    { 255, 107, 15, 1 },
    { 0, 0, 8, 0 },
};

PadScriptCmd D_actor_361100_80166AD0[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_361100_80166AD8 = { 255, 255, 1, 1 };

static TmdBone _gActor361100Model06038Skeleton[19] = {
#include "assets/actor_361100_model_06038_skeleton.inc"
};

static u32 _gActor361100Model06038PartVerts[19] = {
#include "assets/actor_361100_model_06038_partVerts.inc"
};

static SVECTOR _gActor361100Model06038Verts[258] = {
#include "assets/actor_361100_model_06038_verts.inc"
};

static SVECTOR _gActor361100Model06038Normals[270] = {
#include "assets/actor_361100_model_06038_normals.inc"
};

static u32 _gActor361100Model06038Stream[3353] = {
#include "assets/actor_361100_model_06038_stream.inc"
};

static TmdSource _gActor361100Model06038 = {
    0,
    15104,
    9712,
    19,
    _gActor361100Model06038PartVerts,
    _gActor361100Model06038Verts,
    _gActor361100Model06038Normals,
    _gActor361100Model06038Skeleton,
    _gActor361100Model06038Stream,
};

static AnimationPackedPose _gActor361100Animation0973CBank1[8] = {
#include "assets/actor_361100_animation_0973C_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0973CBank4[35] = {
#include "assets/actor_361100_animation_0973C_bank4.inc"
};

static AnimationRecord _gActor361100Animation0973CRecords[91] = {
#include "assets/actor_361100_animation_0973C_records.inc"
};

static u16 _gActor361100Animation0973CIndices[20] = {
#include "assets/actor_361100_animation_0973C_indices.inc"
};

static AnimationSet _gActor361100Animation0973C = {
    _gActor361100Animation0973CRecords,
    _gActor361100Animation0973CIndices,
    { NULL, _gActor361100Animation0973CBank1, NULL, NULL, _gActor361100Animation0973CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation09AA8Bank1[11] = {
#include "assets/actor_361100_animation_09AA8_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation09AA8Bank4[42] = {
#include "assets/actor_361100_animation_09AA8_bank4.inc"
};

static AnimationRecord _gActor361100Animation09AA8Records[124] = {
#include "assets/actor_361100_animation_09AA8_records.inc"
};

static u16 _gActor361100Animation09AA8Indices[20] = {
#include "assets/actor_361100_animation_09AA8_indices.inc"
};

static AnimationSet _gActor361100Animation09AA8 = {
    _gActor361100Animation09AA8Records,
    _gActor361100Animation09AA8Indices,
    { NULL, _gActor361100Animation09AA8Bank1, NULL, NULL, _gActor361100Animation09AA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation09C88Bank1[6] = {
#include "assets/actor_361100_animation_09C88_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation09C88Bank4[18] = {
#include "assets/actor_361100_animation_09C88_bank4.inc"
};

static AnimationRecord _gActor361100Animation09C88Records[64] = {
#include "assets/actor_361100_animation_09C88_records.inc"
};

static u16 _gActor361100Animation09C88Indices[20] = {
#include "assets/actor_361100_animation_09C88_indices.inc"
};

static AnimationSet _gActor361100Animation09C88 = {
    _gActor361100Animation09C88Records,
    _gActor361100Animation09C88Indices,
    { NULL, _gActor361100Animation09C88Bank1, NULL, NULL, _gActor361100Animation09C88Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_361100_8016BAD0[4] = {
    NULL,
    &_gActor361100Animation0973C,
    &_gActor361100Animation09AA8,
    &_gActor361100Animation09C88,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_361100_8016BAD0,
};

TaskDesc D_actor_361100_8016BAE4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor361100TentacleTask, { .model = &_gActor361100Model06038 } };

TaskMessageEntry D_actor_361100_8016BAF0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, _actor361100PlaceTentacle },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor361100SetTentacleDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor361100ApplyTentacleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor361100AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor361100AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor361100AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor361100AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor361100AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor361100AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor361100AyaBreaBodyPartVerts,
    _gActor361100AyaBreaBodyVerts,
    _gActor361100AyaBreaBodyNormals,
    _gActor361100AyaBreaBodySkeleton,
    _gActor361100AyaBreaBodyStream,
};

static AnimationPackedPose _gActor361100Animation0F84CBank1[2] = {
#include "assets/actor_361100_animation_0F84C_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0F84CBank4[69] = {
#include "assets/actor_361100_animation_0F84C_bank4.inc"
};

static AnimationRecord _gActor361100Animation0F84CRecords[138] = {
#include "assets/actor_361100_animation_0F84C_records.inc"
};

static u16 _gActor361100Animation0F84CIndices[20] = {
#include "assets/actor_361100_animation_0F84C_indices.inc"
};

static AnimationSet _gActor361100Animation0F84C = {
    _gActor361100Animation0F84CRecords,
    _gActor361100Animation0F84CIndices,
    { NULL, _gActor361100Animation0F84CBank1, NULL, NULL, _gActor361100Animation0F84CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation0F9D8Bank1[2] = {
#include "assets/actor_361100_animation_0F9D8_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0F9D8Bank4[16] = {
#include "assets/actor_361100_animation_0F9D8_bank4.inc"
};

static AnimationRecord _gActor361100Animation0F9D8Records[57] = {
#include "assets/actor_361100_animation_0F9D8_records.inc"
};

static u16 _gActor361100Animation0F9D8Indices[20] = {
#include "assets/actor_361100_animation_0F9D8_indices.inc"
};

static AnimationSet _gActor361100Animation0F9D8 = {
    _gActor361100Animation0F9D8Records,
    _gActor361100Animation0F9D8Indices,
    { NULL, _gActor361100Animation0F9D8Bank1, NULL, NULL, _gActor361100Animation0F9D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation0FBA4Bank1[2] = {
#include "assets/actor_361100_animation_0FBA4_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0FBA4Bank4[22] = {
#include "assets/actor_361100_animation_0FBA4_bank4.inc"
};

static AnimationRecord _gActor361100Animation0FBA4Records[67] = {
#include "assets/actor_361100_animation_0FBA4_records.inc"
};

static u16 _gActor361100Animation0FBA4Indices[20] = {
#include "assets/actor_361100_animation_0FBA4_indices.inc"
};

static AnimationSet _gActor361100Animation0FBA4 = {
    _gActor361100Animation0FBA4Records,
    _gActor361100Animation0FBA4Indices,
    { NULL, _gActor361100Animation0FBA4Bank1, NULL, NULL, _gActor361100Animation0FBA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor361100Animation0FD4CBank1[2] = {
#include "assets/actor_361100_animation_0FD4C_bank1.inc"
};

static AnimationPackedRotation _gActor361100Animation0FD4CBank4[23] = {
#include "assets/actor_361100_animation_0FD4C_bank4.inc"
};

static AnimationRecord _gActor361100Animation0FD4CRecords[57] = {
#include "assets/actor_361100_animation_0FD4C_records.inc"
};

static u16 _gActor361100Animation0FD4CIndices[20] = {
#include "assets/actor_361100_animation_0FD4C_indices.inc"
};

static AnimationSet _gActor361100Animation0FD4C = {
    _gActor361100Animation0FD4CRecords,
    _gActor361100Animation0FD4CIndices,
    { NULL, _gActor361100Animation0FD4CBank1, NULL, NULL, _gActor361100Animation0FD4CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_361100_80171B94[5] = {
    NULL,
    &_gActor361100Animation0F84C,
    &_gActor361100Animation0F9D8,
    &_gActor361100Animation0FBA4,
    &_gActor361100Animation0FD4C,
};

AnimationSet** D_actor_361100_80171BA8[1] = {
    D_actor_361100_80171B94,
};

TaskDesc D_actor_361100_80171BAC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor361100AyaBreaTask, { .model = &_gActor361100AyaBreaBody } };

TaskMessageEntry D_actor_361100_80171BB8[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor361100PlayAyaBreaAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor361100SetAyaBreaDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor361100ApplyAyaBreaCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

Task* D_actor_361100_80171BE0;

/// Runs while `Fs_ChunkOutputSizes[2]` reports a streaming write in flight -- it is `-1`
/// until `Fs_LoadFile` has a chunk, and the mode byte in `gGameSession->location.loc.view`
/// then picks this actor's part in the load: 11 hands the task to
/// `_actor361100DrawStreamRefraction`, 12 publishes the stream position `D_actor_403600_8016069C`
/// (half the remaining 0x18000-byte window past the write pointer, times the
/// per-chunk rate) and uploads the coordinate, and 10 exits the task.
///
/// State 0 allocates the `Actor403600Ripple` block into
/// `Task::work`, sets its `emitting` and `shallow` and ticks it 0x1E
/// times, then resets the body's coordinate matrix to identity with the fixed
/// translation (0x1CA2, 0x712, 0x189C) and parks the view coordinate in its
/// `parent` slot. A failed allocation takes the exit call and is *not* branched
/// around: the block pointer is NULL for the rest of the state, as it was in
/// the original.
void func_actor_361100_80161E3C(Task* arg0)
{
    Actor403600Ripple* state;
    GfxCoord*          coord;
    s32                i;
    u8*                writePtr;
    u32                streamLeft;
    u8*                modePtr;
    u8                 mode;

    state   = arg0->work;
    modePtr = &gGameSession->location.loc.view;
    coord   = arg0->extra.coordBody->coord;
    if (Fs_ChunkOutputSizes[2] != -1) {
        streamLeft  = 0x18000 - Fs_ChunkOutputSizes[2];
        streamLeft &= ~7;
        writePtr    = (u8*)Fs_ActorLoadBase2 + Fs_ChunkOutputSizes[2];
        if (arg0->state == 0) {
            state = memCalloc(sizeof(Actor403600Ripple), false);
            if (state == NULL) {
                taskCallExit(arg0);
                i = 0;
            }
            arg0->work      = state;
            state->shallow  = 1;
            state->emitting = 1;
            i               = 0;
            do {
                func_actor_403600_80138C9C(state);
                i += 1;
            } while (i < 0x1E);
            coord->parent = &gGfxViewCoord;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0x1CA2;
            coord->coord.t[1]   = 0x712;
            coord->coord.t[2]   = 0x189C;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state        += 1;
        }
        mode = *modePtr;
        if (mode == 11) {
            _actor361100DrawStreamRefraction(arg0);
            return;
        } else if (mode == 12) {
            D_actor_403600_8016069C = writePtr + (gDisplayState.otBuffer * ((s32)(streamLeft + (streamLeft >> 0x1F)) >> 1));
            func_actor_403600_80138C9C(state);
            func_actor_403600_801353D0(state, coord);
            return;
        } else if (mode == 10) {
            taskCallExit(arg0);
        }
    }
}

/// Prepares screen rays and the depth dividend for the scene's horizontal plane.
///
/// Requires one reserved scratch block and a live display projection. Coordinates
/// use integer world units; rows use centred pixels. View translation narrows
/// to signed halfwords before rotation. Overwrites the GTE rotation.
static inline void _actor361100PrepareRefractionProjection(WaterRefractionScratch* scratch, const DisplayState* display)
{
    enum { ACTOR_361100_REFRACTION_PLANE_Y = 1810 };

    TransposeMatrix(&gGfxViewCoord.workm, &scratch->transposedView);
    scratch->viewTranslation.vx = gGfxViewCoord.workm.t[0];
    scratch->viewTranslation.vy = gGfxViewCoord.workm.t[1];
    scratch->viewTranslation.vz = gGfxViewCoord.workm.t[2];
    _gfxRotateSv(&scratch->transposedView, &scratch->viewTranslation);
    scratch->depth        = scratch->viewTranslation.vy + ACTOR_361100_REFRACTION_PLANE_Y;
    scratch->depth       *= display->screenDistance;
    scratch->screenRow.vx = 0;
    scratch->screenRow.vz = display->screenDistance;
    gte_SetRotMatrix(&scratch->transposedView);
}

/// Redraws the scene's lower screen rows as vertically refracted framebuffer strips.
///
/// Called for view 11 during the chunked actor load. Requires a writable 0x18000-
/// byte streaming window at `Fs_ActorLoadBase2`, a written-byte count in 0..0x18000,
/// and scratch-stack space. The count is rounded up to an eight-byte boundary;
/// only an aligned free tail of at least 0x6680 bytes draws. Each display buffer
/// uses half that tail, with 160 rows split into 320 POLY_FT4 packets (12800 bytes).
/// Packets and the sampled framebuffer must survive GPU consumption and further
/// streaming must not overwrite the selected half while it is still in use.
///
/// Screen positions are pixels centred on (160,120); the horizontal plane offset
/// is 1810 world units. `killCountdown` is a wrapping 16-bit wave phase (4096 units
/// per turn), advanced by 32 while actor control runs. Rotated rays supply depth
/// buckets masked to 0..1023. The clipping configuration is fixed to full width.
/// Borrows task state and streaming storage; restores the scratch cursor and
/// overwrites GTE rotation/arithmetic state.
static void _actor361100DrawStreamRefraction(Task* task)
{
    enum {
        ACTOR_361100_REFRACTION_STREAM_BYTES       = 0x18000,
        ACTOR_361100_REFRACTION_MIN_FREE_BYTES     = 0x6680,
        ACTOR_361100_REFRACTION_TOP_ROW            = 80,
        ACTOR_361100_REFRACTION_BOTTOM_ROW         = 240,
        ACTOR_361100_REFRACTION_HALF_WIDTH         = 160,
        ACTOR_361100_REFRACTION_HALF_HEIGHT        = 120,
        ACTOR_361100_REFRACTION_PHASE_STEP         = 32,
        ACTOR_361100_REFRACTION_SINE_ROW_STEP      = 31,
        ACTOR_361100_REFRACTION_COSINE_ROW_STEP    = 197,
        ACTOR_361100_REFRACTION_COSINE_OFFSET      = 308,
        ACTOR_361100_REFRACTION_WAVE_BIAS          = 2 * ONE,
        ACTOR_361100_REFRACTION_WAVE_PIXEL_SHIFT   = 9,
        ACTOR_361100_REFRACTION_DEPTH_PHASE_START  = 768,
        ACTOR_361100_REFRACTION_FADE_ROWS          = 8,
        ACTOR_361100_REFRACTION_TRIG_SHIFT         = 12,
        ACTOR_361100_REFRACTION_QUAD_WORDS         = 9,
        ACTOR_361100_REFRACTION_RAW_QUAD_CODE      = 0x2D,
        ACTOR_361100_REFRACTION_MAX_DEPTH          = 0x3FFF,
        ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS  = 32,
        ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS   = 96,
        ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW   = 239,
        ACTOR_361100_REFRACTION_TEXTURE_REFLECTION = 476,
    };
    DisplayState*           display;
    WaterRefractionScratch* scratch;
    POLY_FT4*               quad;
    s32                     freeBytes;
    s32                     negativeWrittenBytes;
    u8*                     packetBytes;
    s32                     displayBuffer;
    s32                     clipMode;
    s32                     splitX;
    s32                     sinePhase;
    s32                     cosinePhase;
    s32                     screenY;
    s32                     centeredY;
    s32                     spanLeft;
    s32                     spanRight;
    s32                     spanCount;
    s32                     splitY;
    s32                     bucketOffset;
    s32                     fullWave;
    s32                     waveScale;
    s32                     modeLeftEdge; // Uninitialized only in dormant clip modes; fixed mode 0 never reads it
    s32                     waveDisplacement;
    s32                     fadedDisplacement;
    s32                     fadeStartY;
    s32                     fadeShift;
    s32                     fadeDistance;
    s32                     rowDepth;
    s32                     bucket;
    s32                     spanIndex;
    s32                     splitFadeStartY;
    s32                     fadeRows;
    s32                     baseLeft;
    s32                     baseRight;
    s32                     splitLeft;
    s32                     splitRight;
    s32                     rightPageLeft;
    s32                     leftPageRight;
    s32                     textureY;
    s32                     secondSpanWidth;
    s32                     sineSample;
    s32                     cosineSample;
    u16                     unusedFrameSlot; // Its shifted read emits no load; retained for the matched spill layout

    freeBytes            = ACTOR_361100_REFRACTION_STREAM_BYTES - Fs_ChunkOutputSizes[2];
    freeBytes           &= -8;
    negativeWrittenBytes = freeBytes - ACTOR_361100_REFRACTION_STREAM_BYTES;
    packetBytes          = (u8*)Fs_ActorLoadBase2 - negativeWrittenBytes;
    display              = &gDisplayState;
    displayBuffer        = display->otBuffer;
    clipMode             = 0;
    splitX               = clipMode;
    splitY               = 0;
    waveScale            = ONE;
    fullWave             = ONE;
    fadeStartY           = ACTOR_361100_REFRACTION_TOP_ROW;
    fadeShift            = 1;
    bucketOffset         = 0;
    splitFadeStartY      = 0;
    fadeRows             = ACTOR_361100_REFRACTION_FADE_ROWS;
    baseLeft             = -ACTOR_361100_REFRACTION_HALF_WIDTH;
    baseRight            = ACTOR_361100_REFRACTION_HALF_WIDTH;
    splitLeft            = -ACTOR_361100_REFRACTION_HALF_WIDTH;
    splitRight           = ACTOR_361100_REFRACTION_HALF_WIDTH;
    // The remaining streamed-load tail supplies two independent packet halves.
    if ((u32)freeBytes >= (u32)ACTOR_361100_REFRACTION_MIN_FREE_BYTES) {
        if (displayBuffer != 0) {
            packetBytes += freeBytes >> 1;
        }
        quad = (POLY_FT4*)packetBytes;
        quad--;
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            task->killCountdown = (u16)task->killCountdown + ACTOR_361100_REFRACTION_PHASE_STEP;
        }
        sinePhase   = task->killCountdown * 2;
        cosinePhase = task->killCountdown;
        scratch     = SCRATCH_STACK_RESERVE_BLOCK(WaterRefractionScratch);
        _actor361100PrepareRefractionProjection(scratch, display);
        // Rotate each screen ray, then displace the sampled framebuffer row.
        screenY = ACTOR_361100_REFRACTION_TOP_ROW;
        do {
            centeredY             = screenY - ACTOR_361100_REFRACTION_HALF_HEIGHT;
            scratch->screenRow.vy = centeredY;
            gte_ldv0(&scratch->screenRow);
            gte_rtv0();
            spanLeft  = baseLeft;
            spanRight = baseRight;
            spanCount = 1;
            if (splitY > 0) {
                if (screenY < ACTOR_361100_REFRACTION_FADE_ROWS) {
                    spanRight = spanLeft + splitX;
                    if (splitX <= 0) {
                        spanRight = baseRight;
                        spanLeft  = spanRight + splitX;
                    }
                    if (splitY < screenY) {
                        spanCount = 2;
                    }
                }
            } else {
                if ((splitY < 0) && (-splitY < screenY)) {
                    spanLeft = splitLeft;
                    if (splitX > 0) {
                        spanRight = spanLeft + splitX;
                    } else {
                        spanRight = splitRight;
                        spanLeft  = spanRight + splitX;
                    }
                }
            }
            // Combine Q12 samples, then fade the pixel displacement over the first eight rows.
            sineSample          = rsin(sinePhase);
            cosineSample        = rcos(cosinePhase + ACTOR_361100_REFRACTION_COSINE_OFFSET);
            sineSample         += ACTOR_361100_REFRACTION_WAVE_BIAS;
            fadedDisplacement   = cosineSample + sineSample;
            fadedDisplacement >>= ACTOR_361100_REFRACTION_WAVE_PIXEL_SHIFT;
            // Retain the allocator-visible one-pass wave latches required by this image.
            if (fullWave == 0) {
                fadedDisplacement = (fadedDisplacement * waveScale) >> ACTOR_361100_REFRACTION_TRIG_SHIFT;
                do {
                    waveDisplacement  = fadedDisplacement;
                    fadedDisplacement = waveDisplacement + 1;
                } while (0);
            } else {
                do {
                    waveDisplacement  = fadedDisplacement;
                    fadedDisplacement = waveDisplacement + 1;
                } while (0);
            }
            if (fadeStartY != 1) {
                fadeDistance = screenY - fadeStartY;
                if (fadeDistance < fadeRows) {
                    fadedDisplacement  = waveDisplacement >> ((fadeRows - fadeDistance) >> fadeShift);
                    fadedDisplacement += fadeShift;
                }
            }
            gte_stsv(&scratch->rotatedRow);
            if (scratch->rotatedRow.vy > 0) {
                bucket   = scratch->depth / scratch->rotatedRow.vy;
                bucket >>= 2;
            } else {
                bucket = ACTOR_361100_REFRACTION_MAX_DEPTH;
            }
            textureY = centeredY + ACTOR_361100_REFRACTION_HALF_HEIGHT + fadedDisplacement;
            rowDepth = bucket;
            bucket   = ((rowDepth << gDisplayState.otDepthShift) & ACTOR_361100_REFRACTION_MAX_DEPTH) >> 4;
            bucket  += bucketOffset;
            if (textureY >= ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW) {
                textureY = ACTOR_361100_REFRACTION_TEXTURE_REFLECTION - textureY;
            }
            if (clipMode == 1) {
                modeLeftEdge = -ACTOR_361100_REFRACTION_HALF_WIDTH;
                if (screenY < 0x7D) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    spanLeft = modeLeftEdge;
                    if (screenY < 0xB3) {
                        spanCount = 2;
                    }
                    spanRight = -0x59;
                }
            } else if (clipMode == 2) {
                if (screenY < 0x83) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    if (screenY < 0xB7) {
                        spanCount = 2;
                        spanLeft  = 0x57;
                    } else {
                        spanLeft = 0x57;
                    }
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                }
            } else if (clipMode == 3) {
                spanCount = 1;
                if (screenY < 0x43) {
                    spanLeft  = modeLeftEdge;
                    spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                } else {
                    spanCount = 2;
                }
            }
            spanIndex = 0;
            if (spanCount != 0) {
                do {
                    if (clipMode == 1) {
                        if (spanIndex != 0) {
                            spanLeft  = 0x3C;
                            spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                        }
                    } else if (clipMode == 2) {
                        if (spanIndex == 1) {
                            spanLeft  = modeLeftEdge;
                            spanRight = -0x69;
                        }
                    } else if (clipMode == 3) {
                        if (spanIndex == 0) {
                            if (screenY < 0x43) {
                                spanLeft  = modeLeftEdge;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            } else {
                                spanLeft  = modeLeftEdge;
                                spanRight = -0x57;
                            }
                        } else {
                            if (screenY < 0xC1) {
                                spanLeft  = 0x5D;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            } else {
                                spanLeft  = 0x2A;
                                spanRight = ACTOR_361100_REFRACTION_HALF_WIDTH;
                            }
                        }
                    } else if (spanIndex == 1) {
                        if (screenY - splitFadeStartY < fadeRows) {
                            fadedDisplacement = waveDisplacement >> ((fadeRows - (screenY - splitFadeStartY)) >> fadeShift);
                            textureY          = centeredY + 0x79 + fadedDisplacement;
                            if (textureY >= ACTOR_361100_REFRACTION_TEXTURE_LAST_ROW) {
                                textureY = ACTOR_361100_REFRACTION_TEXTURE_REFLECTION - textureY;
                            }
                        }
                        secondSpanWidth = splitX - 2 * ACTOR_361100_REFRACTION_HALF_WIDTH;
                        if (splitX <= 0) {
                            secondSpanWidth = splitX + 2 * ACTOR_361100_REFRACTION_HALF_WIDTH;
                        }
                        spanLeft = baseLeft;
                        if (secondSpanWidth > 0) {
                            spanRight = secondSpanWidth + spanLeft;
                        } else {
                            spanRight = baseRight;
                            spanLeft  = secondSpanWidth + spanRight;
                        }
                    }
                    // Split at X=0 because each texture page covers one screen half.
                    if (spanRight > 0) {
                        quad++;
                        quad->y1      = centeredY;
                        quad->y0      = centeredY;
                        quad->y3      = centeredY + 1;
                        quad->y2      = centeredY + 1;
                        quad->tpage   = getTPage(2, 0, 0x80, displayBuffer << 8);
                        rightPageLeft = spanLeft;
                        if (spanLeft < 0) {
                            rightPageLeft = 0;
                        }
                        quad->x2 = rightPageLeft;
                        quad->x0 = rightPageLeft;
                        quad->u2 = rightPageLeft + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->u0 = rightPageLeft + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->x3 = spanRight;
                        quad->x1 = spanRight;
                        quad->u3 = spanRight + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->u1 = spanRight + ACTOR_361100_REFRACTION_RIGHT_PAGE_U_BIAS;
                        quad->v1 = textureY + (displayBuffer << 4);
                        quad->v0 = textureY + (displayBuffer << 4);
                        quad->v3 = textureY + (displayBuffer << 4) + 1;
                        quad->v2 = textureY + (displayBuffer << 4) + 1;
                        setlen(quad, ACTOR_361100_REFRACTION_QUAD_WORDS);
                        setcode(quad, ACTOR_361100_REFRACTION_RAW_QUAD_CODE);
                        addPrim(&gGpuCurrentOt[bucket], quad);
                    }
                    if (spanLeft <= 0) {
                        quad++;
                        quad->y1      = centeredY;
                        quad->y0      = centeredY;
                        quad->y2      = (quad->y3 = centeredY + 1);
                        quad->tpage   = getTPage(2, 0, 0, displayBuffer << 8);
                        leftPageRight = spanRight;
                        // Split at X=0 because each texture page covers one screen half.
                        if (spanRight > 0) {
                            leftPageRight = 0;
                        }
                        quad->u2 = spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS;
                        quad->u0 = spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS;
                        quad->u3 = (spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS) + (leftPageRight - spanLeft);
                        quad->u1 = (spanLeft - ACTOR_361100_REFRACTION_LEFT_PAGE_U_BIAS) + (leftPageRight - spanLeft);
                        quad->x2 = spanLeft;
                        quad->x0 = spanLeft;
                        quad->x3 = leftPageRight;
                        quad->x1 = leftPageRight;
                        quad->v1 = textureY + (displayBuffer << 4);
                        quad->v0 = textureY + (displayBuffer << 4);
                        quad->v3 = textureY + (displayBuffer << 4) + 1;
                        quad->v2 = textureY + (displayBuffer << 4) + 1;
                        setlen(quad, ACTOR_361100_REFRACTION_QUAD_WORDS);
                        setcode(quad, ACTOR_361100_REFRACTION_RAW_QUAD_CODE);
                        addPrim(&gGpuCurrentOt[bucket], quad);
                    }
                    spanIndex += 1;
                } while (spanIndex < spanCount);
            }
            sinePhase += ACTOR_361100_REFRACTION_SINE_ROW_STEP + (unusedFrameSlot >> 16);
            if (rowDepth >= ACTOR_361100_REFRACTION_DEPTH_PHASE_START + 1) {
                cosinePhase += ACTOR_361100_REFRACTION_COSINE_ROW_STEP + (rowDepth - ACTOR_361100_REFRACTION_DEPTH_PHASE_START) / 4;
            } else {
                cosinePhase += ACTOR_361100_REFRACTION_COSINE_ROW_STEP;
            }
            screenY += 1;
        } while (screenY < ACTOR_361100_REFRACTION_BOTTOM_ROW);
        SCRATCH_STACK_RELEASE_BLOCK(WaterRefractionScratch);
    }
}

/// Ramps the retained head-aim rate by ONE/16, with halfword truncation before clamping.
///
/// aim is live writable state; nonzero tracking ramps toward ONE, zero toward 0.
static inline void _actor361100RampHeadAimRate(AnimationHeadAim* aim, s32 tracking)
{
    enum { ACTOR_361100_HEAD_AIM_RATE_STEP = ONE / 16 };
    u16 rate;

    if (tracking != 0) {
        rate      = aim->rate + ACTOR_361100_HEAD_AIM_RATE_STEP;
        aim->rate = rate;
        if ((s16)rate > ONE) {
            aim->rate = ONE;
        }
    } else {
        rate      = aim->rate - ACTOR_361100_HEAD_AIM_RATE_STEP;
        aim->rate = rate;
        if ((s16)rate < 0) {
            aim->rate = 0;
        }
    }
}

/// Turns the player's head toward placed scene actor 2 with a ramped Q12 rate.
///
/// Both tasks must own live TMD models with parts 0..4 in root-to-head order.
/// The script freeze gate suspends all state changes. State 0 allocates zeroed
/// AnimationHeadAim work, setting yaw/pitch limits to 768/512 units (4096 per
/// turn), then updates immediately. State 1 changes rate by ONE/16 each tick:
/// nonzero spawnArg1 ramps to ONE, zero ramps to 0. The rate is narrowed to a
/// halfword before a signed clamp. Missing tasks, failed allocation and other
/// states kill the task and clear its handle. taskKill releases owned work.
static void _actor361100HeadAimTask(Task* task)
{
    enum {
        ACTOR_361100_HEAD_AIM_INIT        = 0,
        ACTOR_361100_HEAD_AIM_UPDATE      = 1,
        ACTOR_361100_HEAD_AIM_TARGET      = 2,
        ACTOR_361100_HEAD_AIM_YAW_LIMIT   = 768,
        ACTOR_361100_HEAD_AIM_PITCH_LIMIT = 512,
    };
    Task*             subject;
    Task*             target;
    AnimationHeadAim* aim;

    subject = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    target  = sceneFindPlacedActor(ACTOR_361100_HEAD_AIM_TARGET);
    if (D_801156F9 == 0) {
        if ((subject == NULL) || (target == NULL)) {
            task->state = ACTOR_361100_HEAD_AIM_KILL;
        }
        switch (task->state) {
            case ACTOR_361100_HEAD_AIM_INIT:
                aim = memCalloc(sizeof(AnimationHeadAim), false);
                if (aim != NULL) {
                    task->work      = aim;
                    aim->yawLimit   = ACTOR_361100_HEAD_AIM_YAW_LIMIT;
                    aim->pitchLimit = ACTOR_361100_HEAD_AIM_PITCH_LIMIT;
                    task->state++;
                        /* fallthrough */
                    case ACTOR_361100_HEAD_AIM_UPDATE:
                        aim = task->work;
                        _actor361100RampHeadAimRate(aim, task->spawnArg1.value);
                        animationAimHeadAt(subject, target, aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(task);
                D_actor_361100_80171BE0 = NULL;
                break;
        }
    }
}

/// Stages the selected scene's deferred audio start for this package's script.
///
/// Requires a selected scene and prepared buffers through CD consumption. A
/// missing selection leaves the previous deferred request intact.
static void _actor361100StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues playback of the selected scene for this package's event script.
///
/// Requires prepared playback storage and resident CD queue capacity. The scene
/// descriptor and buffers must remain live until playback completes.
static void _actor361100StartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Ends the selected scene stream and restores its saved random state.
///
/// Requires a successful scene selection. Buffer and CD-request teardown remain
/// with their owners; the script cancels the deferred CD request separately.
static void _actor361100FinishScene(void)
{
    streamFinishScene();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_361100_8016297C(void)
{
    cdCmdCancelScene();
}

/// Spawns and retains this scene's initially relaxed player-head aim task.
///
/// The package and gameplay must remain loaded while it runs. Call once after
/// clearing the handle; an existing task is neither checked nor killed. Failed
/// spawning stores NULL. Task teardown owns any subsequently allocated work.
static void _actor361100SpawnHeadAimTask(void)
{
    D_actor_361100_80171BE0 = taskSpawnFromTable(D_actor_361100_80165C58, ACTOR_361100_HEAD_AIM_TASK_INDEX, ACTOR_361100_HEAD_AIM_RELAX, 0);
}

/// Sets the retained head-aim task's ramp policy, or kills it for any other value.
///
/// Mode 0 relaxes, 1 tracks, and every other signed word kills and clears the
/// handle; the scripts use -1 for teardown. A NULL handle does nothing. The
/// change is applied on the task's next unfrozen update, with work owned by it.
static void _actor361100SetHeadAimMode(s32 mode)
{
    if (D_actor_361100_80171BE0 != NULL) {
        if (mode < ACTOR_361100_HEAD_AIM_TRACK + 1) {
            if (mode >= ACTOR_361100_HEAD_AIM_RELAX) {
                D_actor_361100_80171BE0->spawnArg1.value = mode;
                return;
            }
        }
        taskKill(D_actor_361100_80171BE0);
        D_actor_361100_80171BE0 = NULL;
    }
}

/// Spawns the scene's one-pixel vertical shake and controller-vibration task.
///
/// durationTicks is the signed update countdown (the script uses 120); the child
/// decrements before shaking, giving durationTicks-1 active updates for a positive
/// value, and stops when the event is skipped. Spawning failure is ignored. The
/// descriptor and pad/vibration script storage must survive the child task.
static void _actor361100SpawnShakeTask(s32 durationTicks)
{
    taskSpawnFromTable(D_actor_361100_80165C58, ACTOR_361100_SHAKE_TASK_INDEX, durationTicks, 0);
}

void func_actor_361100_80162A54(Task* arg0)
{
    s32 countdown;

    countdown             = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = countdown;
    if (countdown > 0) {
        displaySetShakeY((countdown & 1) ? 0 : -1);
        padScriptSpawn(D_actor_361100_80166AD0, &D_actor_361100_80166AD8);
    }
    if ((arg0->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
        displaySetShakeY(0);
        taskKill(arg0);
    }
}

/// Adds music-loading and weapon-restoration flags requested by the event script.
///
/// bits is a raw signed callback word; OR assignment retains only its low byte.
/// The scripts set GAME_SESSION_FLOW_SKIP_ENDING_MUSIC and
/// GAME_SESSION_FLOW_SKIP_AREA_MUSIC independently, preserving earlier flags.
static void _actor361100AddFlowFlags(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void actor361100ClearHeadAimTaskHandle(s32 unused)
{
    D_actor_361100_80171BE0 = NULL;
}

/// Applies one signed 16.16 root translation step and retains fractional carry.
///
/// rootCoord and work must be side-effect-free pointer expressions to live,
/// disjoint writable storage. work is either package model's work block;
/// velocity is in the root parent's frame and its fourth word is unused.
/// Both arguments are evaluated repeatedly. The block captures no identifiers,
/// has no return or jump, and is undefined after the two model tick consumers.
#define ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work)      \
    {                                                                 \
        (work)->carry[0].word    += (work)->velocity.vx;              \
        (work)->carry[1].word    += (work)->velocity.vy;              \
        (work)->carry[2].word    += (work)->velocity.vz;              \
        (rootCoord)->coord.t[0]  += (work)->carry[0].halves.integer;  \
        (rootCoord)->coord.t[1]  += (work)->carry[1].halves.integer;  \
        (rootCoord)->coord.t[2]  += (work)->carry[2].halves.integer;  \
        (rootCoord)->composeStamp = GRAPHICS_COORD_DIRTY;             \
        (work)->carry[0].word     = (work)->carry[0].halves.fraction; \
        (work)->carry[1].word     = (work)->carry[1].halves.fraction; \
        (work)->carry[2].word     = (work)->carry[2].halves.fraction; \
    }

/// Advances the tentacle's scripted translation, animation and model lighting.
///
/// Requires the initialized, live model/work and its borrowed Enemy. Translation
/// adds signed 16.16 velocity to the root's parent-relative position, retaining
/// fractional carry. An uninterrupted move armed with N runs for N+1 updating
/// ticks: the tick that finds zero still moves before stopping. Hidden models
/// still move and animate; only lighting is skipped. A pending buffer release
/// runs after movement and drawing preparation, on the tick finding zero.
static void _actor361100TickTentacle(Task* task)
{
    TmdObject*                model = task->extra.tmd;
    _Actor361100TentacleWork* work  = task->work;
    GfxCoord*                 rootCoord;
    VECTOR                    worldPosition;
    s32                       slotIndex;

    rootCoord = model->coords;
    ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work);
    if (work->moveFrames >= 0) {
        if (work->moveFrames == 0) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        worldPosition.vx = rootCoord->workm.t[0];
        worldPosition.vy = rootCoord->workm.t[1];
        worldPosition.vz = rootCoord->workm.t[2];
        worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Dispatches the tentacle's lifecycle while scene actors are running.
///
/// Requires a live TMD task with a borrowed Enemy in spawnArg2 and state 0
/// (initialize), 1 (update) or 2 (exit). Other states are outside the table.
/// Pausing scene actors also pauses initialization and both countdowns.
static void _actor361100TentacleTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_361100_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Allocates the tentacle's owned work and enables its update/message handlers.
///
/// Requires the model's live root coordinate and a borrowed Enemy in spawnArg2.
/// Publishes its local matrix on the Enemy, clears its contact table, and binds
/// model lighting to the work's matrices. Allocation failure exits the enemy
/// task. Success advances state and retains the work until task teardown.
static void _actor361100InitTentacle(Task* task)
{
    _Actor361100TentacleWork* work;
    GfxCoord*                 rootCoord;
    Enemy*                    enemy;

    rootCoord = task->extra.tmd->coords;
    enemy     = task->spawnArg2.pointer;

    work = memCalloc(sizeof(_Actor361100TentacleWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = ACTOR_361100_BUFFER_FREE_IDLE;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;

    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    _actor361100BindTentacleLighting(task);

    task->msgTable     = D_actor_361100_8016BAF0;
    task->exitCallback = _actor361100ExitTentacle;
    task->state       += 1;
}

/// Releases the tentacle's Enemy and starts task/model teardown.
///
/// The live task must borrow an Enemy in spawnArg2. Default teardown owns the
/// work allocation; its lighting matrices remain borrowed by the model.
static void _actor361100ExitTentacle(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the tentacle model's lighting matrices to its owned work.
///
/// Requires initialized work and a live model. The model borrows both matrices
/// until task teardown; later lighting updates populate them.
static void _actor361100BindTentacleLighting(Task* task)
{
    TmdObject*                model;
    _Actor361100TentacleWork* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play19.inc.c"

/// Places the tentacle's root and stops its translation without resetting its timer.
///
/// Requires initialized work, a live root and a readable, word-aligned placement
/// through dispatch. XYZ uses whole units in the root parent's frame; Euler
/// angles use 4096 units per turn. Builds Rz * Ry * Rx and records all three
/// angles. Clears velocity and fractional carry but leaves moveFrames alone.
/// The payload is not retained; msgId and unusedArg are ignored. Returns 0.
static s32 _actor361100PlaceTentacle(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*                 rootCoord;
    _Actor361100TentacleWork* work;

    work                    = task->work;
    rootCoord               = task->extra.tmd->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->carry[0].word     = 0;
    work->carry[1].word     = 0;
    work->carry[2].word     = 0;
    work->velocity.vx       = 0;
    work->velocity.vy       = 0;
    work->velocity.vz       = 0;
    return 0;
}

/// Changes tentacle visibility and primitive-buffer ownership policy.
///
/// Requires a live model and initialized work. Mode 0 hides and enables automatic
/// buffer recovery; 1 shows, requests a buffer and enables recovery; 2 hides,
/// disables recovery and schedules release on the third subsequent updating
/// tick; 3 shows while keeping recovery disabled. Showing does not cancel an
/// earlier release. msgId and unusedArg are ignored. Returns 0 for modes 0..3,
/// or 1 without changing anything for other values.
static s32 _actor361100SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    _Actor361100TentacleWork* work;
    TmdObject*                model;
    s32                       result;

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
        case ACTOR_361100_DRAW_HIDE_RELEASE:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Stops the tentacle, starts one of its three translation legs, or exits it.
///
/// Requires initialized work and a readable ActorCommand through dispatch. Only
/// its command word is read; context, msgId and unusedArg are ignored. Command 0
/// zeros velocity but keeps fractional carry and the countdown. Commands 1..3
/// set signed 16.16 velocity and arm 25, 26 or 14 ticks respectively, retaining
/// carry. Uninterrupted movement applies 26, 27 or 15 steps, including the tick
/// that finds zero. Every other command calls the task's exit handler. The
/// payload is not retained. Returns 0.
static s32 _actor361100ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_361100_TENTACLE_COMMAND_STOP        = 0,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_FIRST  = 1,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_SECOND = 2,
        ACTOR_361100_TENTACLE_COMMAND_MOVE_THIRD  = 3,
        ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES    = 25,
        ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES   = 26,
        ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES    = 14,
    };
    _Actor361100TentacleWork* work;

    work = task->work;
    // Commands replace velocity/countdown but preserve the fractional carry.
    switch (command->command) {
        case ACTOR_361100_TENTACLE_COMMAND_STOP:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_FIRST:
            work->velocity.vx = -230 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->velocity.vy = -7970 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->velocity.vz = 290 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_FIRST_LEG_FRAMES;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_SECOND:
            work->velocity.vx = -1350 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->velocity.vy = -4430 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->velocity.vz = 740 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_SECOND_LEG_FRAMES;
            break;
        case ACTOR_361100_TENTACLE_COMMAND_MOVE_THIRD:
            work->velocity.vx = 1350 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->velocity.vy = 6340 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->velocity.vz = -4140 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            work->moveFrames  = ACTOR_361100_TENTACLE_THIRD_LEG_FRAMES;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// Advances Aya's scripted body translation, animation, ground shadow and lighting.
///
/// Requires initialized work, the live nineteen-part model and room effect
/// state. Signed 16.16 velocity moves the root in its parent's frame, retaining
/// fractional carry. An uninterrupted move armed with N runs for N updating
/// ticks, stopping after the tick that finds one. Hidden models still move and
/// animate. Visible models project the second part's cached world position for
/// a shadow before recomposing that part and updating lighting. Pending buffer
/// release runs last, on the tick finding zero.
static void _actor361100TickAyaBrea(Task* task)
{
    // Ground-shadow half-side, in whole coordinate units before view rotation.
    enum { ACTOR_361100_AYA_SHADOW_HALF_SIZE = 512 };
    TmdObject*               model = task->extra.tmd;
    _Actor361100AyaBreaWork* work  = task->work;
    GfxCoord*                rootCoord;
    VECTOR3                  groundPosition;
    s32                      slotIndex;

    // Integrate whole root displacement and retain only the fractional carry.
    rootCoord = model->coords;
    ACTOR_361100_INTEGRATE_ROOT_TRANSLATION(rootCoord, work);
    if (work->moveFrames > 0) {
        if (work->moveFrames == 1) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        // Sample the cached second-part position before recomposing it.
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_361100_AYA_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

#undef ACTOR_361100_INTEGRATE_ROOT_TRANSLATION

/// Dispatches Aya's body lifecycle while scene actors are running.
///
/// Requires a live TMD task with a borrowed Enemy in spawnArg2 and state 0
/// (initialize), 1 (update) or 2 (exit). Other states are outside the table.
/// Pausing scene actors also pauses initialization and both countdowns.
static void _actor361100AyaBreaTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_361100_80161E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Allocates Aya's owned body work and enables its update/message handlers.
///
/// Requires a live model and a borrowed Enemy in spawnArg2 for teardown.
/// Marks the rig unbound so its first play request installs bank 0, and binds
/// the model's lighting to the work's matrices. Allocation failure exits the
/// enemy task. Success advances state and retains work until task teardown.
static void _actor361100InitAyaBrea(Task* task)
{
    _Actor361100AyaBreaWork* work;

    work = memCalloc(sizeof(_Actor361100AyaBreaWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = ACTOR_361100_BUFFER_FREE_IDLE;
    _actor361100BindAyaBreaLighting(task);
    task->msgTable     = D_actor_361100_80171BB8;
    task->exitCallback = _actor361100ExitAyaBrea;
    task->state       += 1;
}

/// Releases Aya's body Enemy and starts task/model teardown.
///
/// The live task must borrow an Enemy in spawnArg2. Default teardown owns the
/// work allocation; its lighting matrices remain borrowed by the model.
static void _actor361100ExitAyaBrea(Task* task)
{
    enemyTaskExit(task);
}

/// Binds Aya's body model lighting matrices to its owned work.
///
/// Requires initialized work and a live model. The model borrows both matrices
/// until task teardown; later lighting updates populate them.
static void _actor361100BindAyaBreaLighting(Task* task)
{
    TmdObject*               model;
    _Actor361100AyaBreaWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Starts or blends the requested clip on Aya's bound non-root animation slots.
///
/// work must own a bound nineteen-slot rig with loaded set and track data for
/// request.animationId (1..4). All rig, pose-buffer, model and clip storage must
/// remain live during playback. request is borrowed only through this call.
/// Its clip id narrows to the model's signed byte; blendFrames counts whole
/// frames (0..2047). Nonzero blend captures a ticking pose before seeking the
/// new clip; otherwise each driven slot resets. Ticks slots 1..18 immediately
/// and enables subsequent playback. Slot 0 retains separately scripted motion.
static inline void _actor361100StartAyaBreaClip(_Actor361100AyaBreaWork* work, const AnimationPlayRequest* request)
{
    s32 slotIndex;

    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
}

/// Applies an animation request to Aya's body, restarting even the current clip.
///
/// Requires initialized work and a readable request through synchronous dispatch.
/// source.index must be 0 and animationId 1..4; the package's bank data must stay
/// loaded during playback. Stored bank/clip ids narrow to signed bytes. Drives
/// slots 1..18, leaving root motion to the scripted translation. Nonzero blend
/// interpolates an already ticking rig for blendFrames whole frames (0..2047);
/// otherwise slots reset. Ticks the new clip once immediately, then enables
/// future ticks. Collision choice, msgId and unusedArg are ignored. Returns 0.
static s32 _actor361100PlayAyaBreaAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor361100AyaBreaWork* work;
    TmdObject*               model;

    work  = task->work;
    model = task->extra.tmd;
    // A bank switch rebinds the rig; every request restarts or blends its clip.
    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_361100_80171BA8[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    _actor361100StartAyaBreaClip(work, request);
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Changes Aya's body visibility and primitive-buffer ownership policy.
///
/// Requires a live model and initialized work. Mode 0 hides and enables automatic
/// buffer recovery; 1 shows, requests a buffer and enables recovery; 2 hides,
/// disables recovery and schedules release on the third subsequent updating
/// tick; 3 shows while keeping recovery disabled. Showing does not cancel an
/// earlier release. msgId and unusedArg are ignored. Returns 0 for modes 0..3,
/// or 1 without changing anything for other values.
static s32 _actor361100SetAyaBreaDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    _Actor361100AyaBreaWork* work;
    TmdObject*               model;
    s32                      result;

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
        case ACTOR_361100_DRAW_HIDE_RELEASE:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_361100_DRAW_SHOW_EXISTING_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Stops Aya's body, starts its upward translation, or exits it.
///
/// Requires initialized work and a readable ActorCommand through dispatch. Only
/// its command word is read; context, msgId and unusedArg are ignored. Command 0
/// zeros velocity and moveFrames while retaining fractional carry. Command 1
/// adds 450/160 units along the root parent's +Y on each of 160 updating ticks,
/// with velocity truncated to signed 16.16. Every other command calls the
/// task's exit handler. The payload is not retained. Returns 0.
static s32 _actor361100ApplyAyaBreaCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_361100_AYA_COMMAND_STOP = 0,
        ACTOR_361100_AYA_COMMAND_RISE = 1,
        ACTOR_361100_AYA_RISE_FRAMES  = 160,
    };
    _Actor361100AyaBreaWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_361100_AYA_COMMAND_STOP:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            work->moveFrames  = 0;
            break;
        case ACTOR_361100_AYA_COMMAND_RISE:
            work->velocity.vy = 450 * ACTOR_361100_TRANSLATION_ONE / ACTOR_361100_AYA_RISE_FRAMES;
            work->velocity.vx = 0;
            work->velocity.vz = 0;
            work->moveFrames  = ACTOR_361100_AYA_RISE_FRAMES;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}
