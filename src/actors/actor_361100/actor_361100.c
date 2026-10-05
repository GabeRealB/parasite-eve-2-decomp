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

/// Script pair handed to `Gp_SpawnScript18` on every even frame of the blink.
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

static void func_actor_361100_80161FF8(Task* arg0);
static void func_actor_361100_80162B18(Task* task);
static void func_actor_361100_80162D28(Task* arg0);
static void func_actor_361100_80162DE4(Task* arg0);
static void func_actor_361100_80162E04(Task* arg0);
static void func_actor_361100_801631C4(Task* task);
static void func_actor_361100_80163410(Task* arg0);
static void func_actor_361100_80163494(Task* arg0);
static void func_actor_361100_801634B4(Task* arg0);

/// `Task::state` handlers `func_actor_361100_80162CBC` dispatches through.
static const TaskFuncTable3 D_actor_361100_80161E24 = {
    {
        func_actor_361100_80162D28,
        func_actor_361100_80162B18,
        func_actor_361100_80162DE4,
    },
};

/// `Task::state` handlers `func_actor_361100_801633A4` dispatches through.
static const TaskFuncTable3 D_actor_361100_80161E30 = {
    {
        func_actor_361100_80163410,
        func_actor_361100_801631C4,
        func_actor_361100_80163494,
    },
};

static AnimationSet _gActor361100Animation0973C;
static AnimationSet _gActor361100Animation09AA8;
static AnimationSet _gActor361100Animation09C88;
static TmdSource    _gActor361100Model06038;
s32                 func_actor_361100_80162F58(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32                 func_actor_361100_80162FF4(Task*, s32, s32, s32);
s32                 func_actor_361100_801630D4(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32                 func_actor_361100_801634D0(Task*, s32, AnimationPlayRequest*, s32);
s32                 func_actor_361100_80163670(Task*, s32, s32, s32);
s32                 func_actor_361100_80163750(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void                func_actor_361100_80162CBC(Task*);
void                func_actor_361100_801633A4(Task*);

extern AnimationPlayRequest D_actor_361100_80165CA0;
extern ActorCommand         D_actor_361100_80165DD4;
extern ActorCommand         D_actor_361100_80165E84;
extern ActorCommand         D_actor_361100_80165F3C;
extern ActorTransform       D_actor_361100_80165D98;
void                        func_actor_361100_8016297C(void);
void                        func_actor_361100_801629D0(s32);
void                        func_actor_361100_80162AEC(s32);

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
void                             func_actor_361100_8016291C(void);
void                             func_actor_361100_8016293C(void);
void                             func_actor_361100_8016295C(void);
void                             func_actor_361100_8016297C(void);
void                             func_actor_361100_8016299C(void);
void                             func_actor_361100_801629D0(s32);
void                             func_actor_361100_80162A24(s32);
void                             func_actor_361100_80162AEC(s32);

void func_actor_361100_80161E3C(Task*);
void func_actor_361100_801627D4(Task*);
void func_actor_361100_80162A54(Task*);

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
    { { { TASK_BODY_NONE, 192 } }, func_actor_361100_801627D4, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016291C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_361100_80165C98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_361100_80165D68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_361100_80165DD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016293C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016299C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_actor_361100_80165D80 } }, { .message = { .pointer = &D_actor_361100_80165DC8 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165DB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_801629D0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_361100_80165D18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_361100_80165EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_361100_80165EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_801629D0 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_80162A24 }, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016295C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_361100_8016297C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_80162AEC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_80162AEC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_801629D0 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_80162AEC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_361100_80162AEC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

TaskDesc D_actor_361100_8016BAE4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_361100_80162CBC, { .model = &_gActor361100Model06038 } };

TaskMessageEntry D_actor_361100_8016BAF0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, func_actor_361100_80162F58 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_361100_80162FF4 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_361100_801630D4 },
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

TaskDesc D_actor_361100_80171BAC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_361100_801633A4, { .model = &_gActor361100AyaBreaBody } };

TaskMessageEntry D_actor_361100_80171BB8[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_361100_801634D0 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_361100_80163670 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_361100_80163750 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

Task* D_actor_361100_80171BE0;

void func_actor_361100_80162B0C(s32 unused);

/// Runs while `Fs_ChunkOutputSizes[2]` reports a streaming write in flight -- it is `-1`
/// until `Fs_LoadFile` has a chunk, and the mode byte in `gGameSession->location.loc.view`
/// then picks this actor's part in the load: 11 hands the task to
/// `func_actor_361100_80161FF8`, 12 publishes the stream position `D_actor_403600_8016069C`
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
    MATRIX*            mtx;
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
            coord->parent                    = &gGfxViewCoord;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = 0x1CA2;
            coord->coord.t[1]                = 0x712;
            coord->coord.t[2]                = 0x189C;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            arg0->state                     += 1;
        }
        mode = *modePtr;
        if (mode == 11) {
            func_actor_361100_80161FF8(arg0);
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

/// Draws the refraction ripple over screen rows 0x50..0xEF while more than
/// 0x6680 bytes of the 0x18000-byte window past `Fs_ChunkOutputSizes[2]` remain free,
/// building `POLY_FT4` strips downward from the `Fs_ActorLoadBase2` side of it (half
/// the free space further in when `DisplayState::otBuffer` is set). Each row is
/// projected through the transposed view matrix to get its ordering-table
/// depth, and the strip samples the other display buffer (`otBuffer` picks the
/// texture page row and the v offset) displaced by a wave built from `rsin` /
/// `rcos` of two phases seeded from `Task::killCountdown`, which the task
/// advances by 0x20 per call while `gSceneCombatState.actorControl` is clear.
///
/// The row window, fade, clip and mode locals are fixed values in this build,
/// so only the default arm of each mode switch ever runs.
///
/// Matching notes: `xNeg` is read uninitialised by the mode 2/3 arms and
/// `spare` is never assigned; both only exist in the register allocator's view
/// (the first adds one instruction to `z`'s live range, the second is a stack
/// slot the retail frame carries). The `do { } while (0)` around the wave latch
/// raises the loop weight of `wave`'s references so it outranks the two masks
/// for `$t3`, as retail does.
static void func_actor_361100_80161FF8(Task* arg0)
{
    DisplayState*            disp;
    WaterRefractionScratch*  block;
    WaterRefractionScratch** slot;
    POLY_FT4*                prim;
    s32                      left;
    s32                      adj;
    u8*                      ptr;
    s32                      otBuf;
    s32                      mode;
    s32                      shift;
    s32                      ang2;
    s32                      ang;
    s32                      y;
    s32                      yTop;
    s32                      x0;
    s32                      x1;
    s32                      nprims;
    s32                      clip;
    s32                      otOff;
    s32                      fade;
    s32                      scale;
    s32                      xNeg;
    s32                      wave;
    s32                      wave1;
    s32                      baseY;
    s32                      one;
    s32                      dist;
    s32                      z;
    s32                      otz;
    s32                      i;
    s32                      yOff;
    s32                      fadeLen;
    s32                      xMin;
    s32                      xMax;
    s32                      xLeft;
    s32                      xRight;
    s32                      xL;
    s32                      xR;
    s32                      v;
    s32                      edge;
    s32                      sine;
    s32                      cosine;
    u16                      spare;

    left    = 0x18000 - Fs_ChunkOutputSizes[2];
    left   &= -8;
    adj     = left - 0x18000;
    ptr     = (u8*)Fs_ActorLoadBase2 - adj;
    disp    = &gDisplayState;
    otBuf   = disp->otBuffer;
    mode    = 0;
    shift   = mode;
    clip    = 0;
    scale   = 0x1000;
    fade    = 0x1000;
    baseY   = 0x50;
    one     = 1;
    otOff   = 0;
    yOff    = 0;
    fadeLen = 8;
    xMin    = -0xA0;
    xMax    = 0xA0;
    xLeft   = -0xA0;
    xRight  = 0xA0;
    if ((u32)left >= 0x6680U) {
        if (otBuf != 0) {
            ptr += left >> 1;
        }
        prim = (POLY_FT4*)ptr - 1;
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            arg0->killCountdown = (u16)arg0->killCountdown + 0x20;
        }
        ang2                                          = arg0->killCountdown * 2;
        ang                                           = arg0->killCountdown;
        slot                                          = (WaterRefractionScratch**)SCRATCH_HEAD_ADDR;
        SCRATCH_HEAD_AT(slot, WaterRefractionScratch) = SCRATCH_HEAD_AT(slot, WaterRefractionScratch) - 1;
        block                                         = SCRATCH_HEAD_AT(slot, WaterRefractionScratch);
        TransposeMatrix(&gGfxViewCoord.workm, &block->transposedView);
        block->viewTranslation.vx = gGfxViewCoord.workm.t[0];
        block->viewTranslation.vy = gGfxViewCoord.workm.t[1];
        block->viewTranslation.vz = gGfxViewCoord.workm.t[2];
        _gfxRotateSv(&block->transposedView, &block->viewTranslation);
        block->depth        = block->viewTranslation.vy + 0x712;
        block->depth       *= disp->screenDistance;
        block->screenRow.vx = 0;
        block->screenRow.vz = disp->screenDistance;
        gte_SetRotMatrix(&block->transposedView);
        y = 0x50;
        do {
            yTop                = y - 0x78;
            block->screenRow.vy = yTop;
            gte_ldv0(&block->screenRow);
            gte_rtv0();
            x0     = xMin;
            x1     = xMax;
            nprims = 1;
            if (clip > 0) {
                if (y < 8) {
                    x1 = x0 + shift;
                    if (shift <= 0) {
                        x1 = xMax;
                        x0 = x1 + shift;
                    }
                    if (clip < y) {
                        nprims = 2;
                    }
                }
            } else {
                if ((clip < 0) && (-clip < y)) {
                    x0 = xLeft;
                    if (shift > 0) {
                        x1 = x0 + shift;
                    } else {
                        x1 = xRight;
                        x0 = x1 + shift;
                    }
                }
            }
            sine    = rsin(ang2);
            cosine  = rcos(ang + 0x134);
            sine   += 0x2000;
            wave1   = cosine + sine;
            wave1 >>= 9;
            if (fade == 0) {
                wave1 = (wave1 * scale) >> 12;
                do {
                    wave  = wave1;
                    wave1 = wave + 1;
                } while (0);
            } else {
                do {
                    wave  = wave1;
                    wave1 = wave + 1;
                } while (0);
            }
            if (baseY != 1) {
                dist = y - baseY;
                if (dist < fadeLen) {
                    wave1  = wave >> ((fadeLen - dist) >> one);
                    wave1 += one;
                }
            }
            gte_stsv(&block->rotatedRow);
            if (block->rotatedRow.vy > 0) {
                otz   = block->depth / block->rotatedRow.vy;
                otz >>= 2;
            } else {
                otz = 0x3FFF;
            }
            v    = yTop + 0x78 + wave1;
            z    = otz;
            otz  = ((z << gDisplayState.otDepthShift) & 0x3FFF) >> 4;
            otz += otOff;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (mode == 1) {
                xNeg = -0xA0;
                if (y < 0x7D) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    x0 = xNeg;
                    if (y < 0xB3) {
                        nprims = 2;
                    }
                    x1 = -0x59;
                }
            } else if (mode == 2) {
                if (y < 0x83) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    if (y < 0xB7) {
                        nprims = 2;
                        x0     = 0x57;
                    } else {
                        x0 = 0x57;
                    }
                    x1 = 0xA0;
                }
            } else if (mode == 3) {
                nprims = 1;
                if (y < 0x43) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    nprims = 2;
                }
            }
            i = 0;
            if (nprims != 0) {
                do {
                    if (mode == 1) {
                        if (i != 0) {
                            x0 = 0x3C;
                            x1 = 0xA0;
                        }
                    } else if (mode == 2) {
                        if (i == 1) {
                            x0 = xNeg;
                            x1 = -0x69;
                        }
                    } else if (mode == 3) {
                        if (i == 0) {
                            if (y < 0x43) {
                                x0 = xNeg;
                                x1 = 0xA0;
                            } else {
                                x0 = xNeg;
                                x1 = -0x57;
                            }
                        } else {
                            if (y < 0xC1) {
                                x0 = 0x5D;
                                x1 = 0xA0;
                            } else {
                                x0 = 0x2A;
                                x1 = 0xA0;
                            }
                        }
                    } else if (i == 1) {
                        if (y - yOff < fadeLen) {
                            wave1 = wave >> ((fadeLen - (y - yOff)) >> one);
                            v     = yTop + 0x79 + wave1;
                            if (v >= 0xEF) {
                                v = 0x1DC - v;
                            }
                        }
                        edge = shift - 0x140;
                        if (shift <= 0) {
                            edge = shift + 0x140;
                        }
                        x0 = xMin;
                        if (edge > 0) {
                            x1 = edge + x0;
                        } else {
                            x1 = xMax;
                            x0 = edge + x1;
                        }
                    }
                    if (x1 > 0) {
                        prim++;
                        prim->y1    = yTop;
                        prim->y0    = yTop;
                        prim->y3    = yTop + 1;
                        prim->y2    = yTop + 1;
                        prim->tpage = getTPage(2, 0, 0x80, otBuf << 8);
                        xL          = x0;
                        if (x0 < 0) {
                            xL = 0;
                        }
                        prim->x2 = xL;
                        prim->x0 = xL;
                        prim->u2 = xL + 0x20;
                        prim->u0 = xL + 0x20;
                        prim->x3 = x1;
                        prim->x1 = x1;
                        prim->u3 = x1 + 0x20;
                        prim->u1 = x1 + 0x20;
                        prim->v1 = v + (otBuf << 4);
                        prim->v0 = v + (otBuf << 4);
                        prim->v3 = v + (otBuf << 4) + 1;
                        prim->v2 = v + (otBuf << 4) + 1;
                        setlen(prim, 9);
                        setcode(prim, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], prim);
                    }
                    if (x0 <= 0) {
                        prim++;
                        prim->y1    = yTop;
                        prim->y0    = yTop;
                        prim->y2    = (prim->y3 = yTop + 1);
                        prim->tpage = getTPage(2, 0, 0, otBuf << 8);
                        xR          = x1;
                        if (x1 > 0) {
                            xR = 0;
                        }
                        prim->u2 = x0 - 0x60;
                        prim->u0 = x0 - 0x60;
                        prim->u3 = (x0 - 0x60) + (xR - x0);
                        prim->u1 = (x0 - 0x60) + (xR - x0);
                        prim->x2 = x0;
                        prim->x0 = x0;
                        prim->x3 = xR;
                        prim->x1 = xR;
                        prim->v1 = v + (otBuf << 4);
                        prim->v0 = v + (otBuf << 4);
                        prim->v3 = v + (otBuf << 4) + 1;
                        prim->v2 = v + (otBuf << 4) + 1;
                        setlen(prim, 9);
                        setcode(prim, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], prim);
                    }
                    i += 1;
                } while (i < nprims);
            }
            ang2 += 0x1F + (spare >> 16);
            if (z >= 0x301) {
                ang += 0xC5 + (z - 0x300) / 4;
            } else {
                ang += 0xC5;
            }
            y += 1;
        } while (y < 0xF0);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(WaterRefractionScratch));
    }
}

/// Head-aim state of the actor, run only while `D_801156F9` is clear: a looker
/// task that is missing, or a target task that is, parks the state machine on
/// -1. State 0 allocates the `AnimationHeadAim` record into `Task::work` and
/// seeds its clamps to 0x300 yaw and 0x200 pitch; state 1 ramps its `rate` up
/// toward 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it
/// is not, then hands the record to `func_800B17D4` between the slot-3 task
/// (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`, the skeleton whose head turns) and the
/// `Gp_LookupSlot4(2)` task it turns toward. Every other state kills the task
/// and clears `D_actor_361100_80171BE0`. State 0 reaching a NULL allocation
/// falls out of its own `if` into that kill, rather than into state 1.
void func_actor_361100_801627D4(Task* task)
{
    Task*             looker;
    Task*             target;
    AnimationHeadAim* aim;
    u16               rate;

    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    target = Gp_LookupSlot4(2);
    if (D_801156F9 == 0) {
        if ((looker == NULL) || (target == NULL)) {
            task->state = -1;
        }
        switch (task->state) {
            case 0:
                aim = memCalloc(sizeof(AnimationHeadAim), false);
                if (aim != NULL) {
                    task->work      = aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    task->state++;
                        /* fallthrough */
                    case 1:
                        aim = task->work;
                        if (task->spawnArg1.value != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate > ONE) {
                                aim->rate = ONE;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, target, aim);
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

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_361100_8016291C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_361100_8016293C(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_361100_8016295C(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_361100_8016297C(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_361100_8016299C(void)
{
    D_actor_361100_80171BE0 = taskSpawnFromTable(D_actor_361100_80165C58, 0, 0, 0);
}

void func_actor_361100_801629D0(s32 arg0)
{
    if (D_actor_361100_80171BE0 != NULL) {
        if (arg0 < 2) {
            if (arg0 >= 0) {
                D_actor_361100_80171BE0->spawnArg1.value = arg0;
                return;
            }
        }
        taskKill(D_actor_361100_80171BE0);
        D_actor_361100_80171BE0 = NULL;
    }
}

void func_actor_361100_80162A24(s32 arg0)
{
    taskSpawnFromTable(D_actor_361100_80165C58, 1, arg0, 0);
}

void func_actor_361100_80162A54(Task* arg0)
{
    s32 countdown;

    countdown             = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = countdown;
    if (countdown > 0) {
        displaySetShakeY((countdown & 1) ? 0 : -1);
        Gp_SpawnScript18(D_actor_361100_80166AD0, &D_actor_361100_80166AD8);
    }
    if ((arg0->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
        displaySetShakeY(0);
        taskKill(arg0);
    }
}

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_361100_80162AEC(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void func_actor_361100_80162B0C(s32 unused)
{
    D_actor_361100_80171BE0 = 0;
}

/// Per-frame tick of the tentacle: adds `velocity` to `carry` and moves the
/// root part's local translation by the whole units that makes, keeping the
/// fractions -- runs the `moveFrames` countdown that zeroes `velocity` while it
/// is at 0, ticks the animation slots once `model.ticking` has latched, and
/// while the part is visible rebuilds its world matrix and hands the result to
/// `Gp_UpdateActorColor`. `freeCountdown` counts the model's buffers down to
/// the free.
///
/// The twin of `func_actor_361100_801631C4`, which moves Aya's body the same
/// way from a work block that keeps `velocity` and `carry` in the other order,
/// stops its move a tick earlier and draws a ground shadow under the second
/// part.
static void func_actor_361100_80162B18(Task* task)
{
    TmdObject*                ext  = task->extra.tmd;
    _Actor361100TentacleWork* work = task->work;
    GfxCoord*                 coord;
    VECTOR                    pos;
    s32                       i;

    coord                = ext->coords;
    work->carry[0].word += work->velocity.vx;
    work->carry[1].word += work->velocity.vy;
    work->carry[2].word += work->velocity.vz;
    coord->coord.t[0]   += work->carry[0].halves.integer;
    coord->coord.t[1]   += work->carry[1].halves.integer;
    coord->coord.t[2]   += work->carry[2].halves.integer;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->carry[0].word  = work->carry[0].halves.fraction;
    work->carry[1].word  = work->carry[1].halves.fraction;
    work->carry[2].word  = work->carry[2].halves.fraction;
    if (work->moveFrames >= 0) {
        if (work->moveFrames == 0) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

/// State dispatcher: runs `Task::state` through `D_actor_361100_80161E24` --
/// setup (`func_actor_361100_80162D28`), per-frame tick (`func_actor_361100_80162B18`)
/// and exit (`func_actor_361100_80162DE4`) -- while `gSceneCombatState.actorControl` is clear.
void func_actor_361100_80162CBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_361100_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Spawn callback: allocates the work block into `Task::work`, seeds the
/// three -1 bytes, clears `carry` and arms the spawn
/// argument `Enemy` with the coordinate's root matrix, then enters the
/// `func_actor_361100_80162E04` state with `D_actor_361100_8016BAF0`
/// installed at `Task::msgTable`. The task exits through
/// `func_actor_361100_80162DE4` if the allocation fails.
static void func_actor_361100_80162D28(Task* arg0)
{
    _Actor361100TentacleWork* work;
    GfxCoord*                 coord;
    Enemy*                    enemy;

    coord = arg0->extra.tmd->coords;
    enemy = arg0->spawnArg2.pointer;

    work = memCalloc(sizeof(_Actor361100TentacleWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = -1;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    func_actor_361100_80162E04(arg0);

    arg0->msgTable     = D_actor_361100_8016BAF0;
    arg0->exitCallback = func_actor_361100_80162DE4;
    arg0->state       += 1;
}

static void func_actor_361100_80162DE4(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_361100_80162E04(Task* arg0)
{
    TmdObject*                ext;
    _Actor361100TentacleWork* work;

    work          = arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

#include "../../shared/actor_motion_play19.inc.c"

/// Places the actor at `placement`: drops the opcode's translation straight
/// into the root part's local matrix, stores its Euler angles in the
/// coordinate's own `rot` slot and rebuilds the rotation from them with
/// `RotMatrixZYX`. Clearing `composeStamp` makes `actorRenderComposeCoordChain` recompute the
/// composed matrix from it, and clearing `carry` and `velocity` stops the move
/// in progress.
s32 func_actor_361100_80162F58(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*                 coord;
    _Actor361100TentacleWork* work;

    work                = task->work;
    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;
    work->velocity.vx   = 0;
    work->velocity.vy   = 0;
    work->velocity.vz   = 0;
    return 0;
}

s32 func_actor_361100_80162FF4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    _Actor361100TentacleWork* work;
    TmdObject*                obj;
    s32                       ret;

    obj = task->extra.tmd;
    ret = 0;
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
            work                = task->work;
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

/// Message 0x7DB handler, listed in `D_actor_361100_8016BAF0` -- the table
/// `func_actor_361100_80162D28` installs at `Task::msgTable`, and the twin of
/// `D_actor_361100_80171BB8` where `func_actor_361100_80163750` serves the same
/// id. 0 parks the actor, clearing `velocity`; 1, 2 and 3 start one of three
/// preset moves, setting `velocity` to the move's displacement divided by its
/// frame count and `moveFrames` to that count; every other sub-command exits
/// the task through its own `Task::exitCallback`.
s32 func_actor_361100_801630D4(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor361100TentacleWork* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            break;
        case 1:
            work->velocity.vx = -230 * 0x10000 / 25;
            work->velocity.vy = -7970 * 0x10000 / 25;
            work->velocity.vz = 290 * 0x10000 / 25;
            work->moveFrames  = 25;
            break;
        case 2:
            work->velocity.vx = -1350 * 0x10000 / 26;
            work->velocity.vy = -4430 * 0x10000 / 26;
            work->velocity.vz = 740 * 0x10000 / 26;
            work->moveFrames  = 26;
            break;
        case 3:
            work->velocity.vx = 1350 * 0x10000 / 14;
            work->velocity.vy = 6340 * 0x10000 / 14;
            work->velocity.vz = -4140 * 0x10000 / 14;
            work->moveFrames  = 14;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// Per-frame tick of Aya's body: adds `velocity` to `carry` and moves the root
/// part's local translation by the whole units that makes, keeping the
/// fractions -- runs the `moveFrames` countdown that zeroes `velocity` while it
/// is at 1, ticks the animation slots once `model.ticking` has latched, and
/// while the part is visible draws its ground shadow, rebuilds the second
/// part's world matrix from it and relights the model through `worldCoordSetModelLighting`.
/// `freeCountdown` counts the model's buffers down to the free. Every use of
/// the second part's coordinate (`TmdObject::coords[1]`) is re-read from
/// `task`, not cached.
static void func_actor_361100_801631C4(Task* task)
{
    TmdObject*               ext  = task->extra.tmd;
    _Actor361100AyaBreaWork* work = (_Actor361100AyaBreaWork*)task->work;
    GfxCoord*                coord;
    VECTOR3                  pos;
    s32                      i;

    coord                = ext->coords;
    work->carry[0].word += work->velocity.vx;
    work->carry[1].word += work->velocity.vy;
    work->carry[2].word += work->velocity.vz;
    coord->coord.t[0]   += work->carry[0].halves.integer;
    coord->coord.t[1]   += work->carry[1].halves.integer;
    coord->coord.t[2]   += work->carry[2].halves.integer;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->carry[0].word  = work->carry[0].halves.fraction;
    work->carry[1].word  = work->carry[1].halves.fraction;
    work->carry[2].word  = work->carry[2].halves.fraction;
    if (work->moveFrames > 0) {
        if (work->moveFrames == 1) {
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
        }
        work->moveFrames--;
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(ext, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

/// State dispatcher: runs `Task::state` through `D_actor_361100_80161E30` --
/// setup (`func_actor_361100_80163410`), per-frame tick (`func_actor_361100_801631C4`)
/// and exit (`func_actor_361100_80163494`) -- while `gSceneCombatState.actorControl` is clear.
void func_actor_361100_801633A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_361100_80161E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

static void func_actor_361100_80163410(Task* arg0)
{
    _Actor361100AyaBreaWork* work;

    work = memCalloc(sizeof(_Actor361100AyaBreaWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = -1;
    func_actor_361100_801634B4(arg0);
    arg0->msgTable     = D_actor_361100_80171BB8;
    arg0->exitCallback = func_actor_361100_80163494;
    arg0->state       += 1;
}

static void func_actor_361100_80163494(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_361100_801634B4(Task* arg0)
{
    TmdObject*               ext;
    _Actor361100AyaBreaWork* work;

    ext           = arg0->extra.tmd;
    work          = (_Actor361100AyaBreaWork*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_361100_801634D0(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor361100AyaBreaWork* work;
    TmdObject*               ext;
    s32                      i;

    work = (_Actor361100AyaBreaWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_361100_80171BA8[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
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

#include "../../shared/actor_messages_place_euler.inc.c"

s32 func_actor_361100_80163670(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
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
            obj->flags                                           |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((_Actor361100AyaBreaWork*)task->work)->freeCountdown = mode;
            obj->flags                                           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
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

/// Message 0x7DB handler, listed in `D_actor_361100_80171BB8`, the table the
/// task installs at `Task::msgTable`. 0 parks the actor, clearing `velocity`
/// and `moveFrames`; 1 starts the one preset move, 450 units along +Y over 160
/// frames, setting `velocity` to that displacement divided by the frame count
/// and `moveFrames` to the count; every other sub-command exits the task
/// through its own `Task::exitCallback`.
s32 func_actor_361100_80163750(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor361100AyaBreaWork* work;

    work = (_Actor361100AyaBreaWork*)task->work;
    switch (msg->command) {
        case 0:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            work->moveFrames  = 0;
            break;
        case 1:
            work->velocity.vy = 450 * 0x10000 / 160;
            work->velocity.vx = 0;
            work->velocity.vz = 0;
            work->moveFrames  = 160;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}
