#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/dryfield_night_motel_balcony.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Work block of Flint, the dog whose model this package carries beside
/// Gary Douglas's.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the head the nineteen-part actor motion
/// handlers run on (`ActorMotion19WalkWork`), so the room script places Flint,
/// plays his clips and walks him from point to point with the same messages
/// it sends Douglas. The model object borrows `model.light` and `model.color`
/// for as long as the block lives.
///
/// No code reads or writes the bytes of `pad_4C6`.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;          // Scripted walk: destination, per-frame velocity and the step in progress
    s16             freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4C6[0x2];
} _Actor335800FlintWork;
STATIC_ASSERT_SIZEOF(_Actor335800FlintWork, 0x4C8);

/// Values of `_Actor335800GaryDouglasWork::lightState`.
enum {
    ACTOR_335800_GARY_DOUGLAS_LIGHT_DIMMED        = -1, // The matrices are at half strength and no ground shadow is drawn
    ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED = 0,  // The next tick that draws the model halves the matrices and moves to `_DIMMED`
    ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL          = 1   // The matrices are as the room's lights built them, and the ground shadow is drawn
};

/// Work block of Gary Douglas's body, the package's twenty-part scripted
/// walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotionWalkWork` does - the
/// twenty-part rig, the model state and the walk a room script sends the
/// actor on - and the model object borrows `model.light` and `model.color`
/// for as long as the block lives. The two models hung off the body share
/// those matrices.
///
/// What follows `walk` is the package's own: the tasks of the two attached
/// models, which a draw-mode request keeps in step with the body's; the
/// dimming an actor command asks for, which lasts until the next view
/// relights the model; the delayed free of the model's buffers once the
/// model has been hidden; and the animation cues of the previous tick, whose
/// end fires the gun's muzzle flash.
typedef struct {
    ActorAnimRig20  rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;          // Scripted walk: destination, closing rotation, per-frame velocity and the step in progress
    Task*           headTask;      // Child task drawing the head-and-hat model, which hangs from body part 4; NULL if its spawn failed, which the draw-mode handler does not test
    Task*           gunTask;       // Child task drawing the long gun, which hangs from body part 8, where its muzzle flash appears; NULL if its spawn failed, likewise untested
    s16             lightState;    // `ACTOR_335800_GARY_DOUGLAS_LIGHT_*`: `_FULL` from spawn and from every view that comes up, since each relights the model
    s16             freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s32             prevCueFlags;  // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous ticked frame, so cue 2 fires the muzzle flash once, as it ends
} _Actor335800GaryDouglasWork;
STATIC_ASSERT_SIZEOF(_Actor335800GaryDouglasWork, 0x50C);

extern ActorTransform D_actor_335800_80164F80;

extern TaskDesc             D_actor_335800_80164DE0[];
extern AnimationPlayRequest D_actor_335800_80164E7C;

/// The warp-payload table the two dispatchers reach by entry:
/// `func_actor_335800_801621B4` selects an entry of it by index.
extern ActorTransform D_actor_335800_80164EA4[5];
extern EvsCommand     D_actor_335800_80165FC0[];
extern EvsCommand     D_actor_335800_80166098[];

/// Animation bank tables of the parent and the child block.
extern AnimationSet*  D_actor_335800_8016EAC4[5];
extern AnimationSet** gActorMotionAnimBanks[1];
extern AnimationSet*  D_actor_335800_80172E90[2];
extern AnimationSet** D_actor_335800_80172E98[1];

/// The two part tasks the parent block spawns, and its message table; both
/// live in this overlay's trailing data.
extern TaskDesc D_actor_335800_8016EADC[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_335800_8016EB00[];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `func_actor_335800_80163AA0`; terminator id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_actor_335800_80172EA8[];

static void _modelPlacementAttachPartTask(Task* childTask);
static void func_actor_335800_80162640(Task* arg0);
static void func_actor_335800_80162844(Task* task);
static void func_actor_335800_80162F08(Task* task);
static void func_actor_335800_80162F7C(Task* arg0);
static void func_actor_335800_80162F9C(Task* arg0);
static void func_actor_335800_80162FF4(Task* arg0);
static void func_actor_335800_80162FFC(Task* task);
static void func_actor_335800_80163124(Task* task);
static void func_actor_335800_80163568(Task* task);
static void func_actor_335800_80163AA0(Task* arg0);
static void func_actor_335800_80163B34(Task* arg0);
static void func_actor_335800_80163B54(Task* arg0);
static void func_actor_335800_80163B70(Task* arg0);
static void func_actor_335800_80163B78(Task* arg0);
static void func_actor_335800_80163BE0(Task* task);
static void func_actor_335800_80163CA0(Task* task);
static void func_actor_335800_80163D20(Task* arg0);

/// Spawn, tick and teardown handlers of the two part tasks the parent block
/// spawns, dispatched by `func_actor_335800_80162E34`.
static const TaskFuncTable3 D_actor_335800_80161E24 = { {
    _modelPlacementAttachPartTask,
    func_actor_335800_80162F08,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent block, dispatched by
/// `func_actor_335800_80162F10`.
static const TaskFuncTable3 D_actor_335800_80161E30 = { {
    func_actor_335800_80162640,
    func_actor_335800_80162844,
    func_actor_335800_80162F7C,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_335800_80161E3C = { {
    actorMotionFaceTarget,
    func_actor_335800_80163124,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset `func_actor_335800_80163124` rotates for
/// the parent block: straight ahead along the part's own +Z.
static const VECTOR D_actor_335800_80161E4C = { 0, 0, 0x200000, 0 };

/// Spawn, tick and teardown handlers of the child block, dispatched by
/// `func_actor_335800_80163A34`.
static const TaskFuncTable3 D_actor_335800_80161E5C = { {
    func_actor_335800_80163AA0,
    func_actor_335800_80163568,
    func_actor_335800_80163B34,
} };

/// Step handlers of the child block's motion sequence, indexed by
/// `ActorWalkState::motionStep`, in the same order as the parent's.
static const TaskFuncTable4 D_actor_335800_80161E68 = { {
    func_actor_335800_80163BE0,
    func_actor_335800_80163CA0,
    actorMotionArrive19,
    func_actor_335800_80163D20,
} };

/// The child block's copy of the forward offset, rotated by
/// `func_actor_335800_80163CA0`.
static const VECTOR D_actor_335800_80161E78 = { 0, 0, 0x200000, 0 };

void func_actor_335800_80161E88(Task*);
void func_actor_335800_80162364(Task*);
void func_actor_335800_801624DC(Task*);
void func_actor_335800_80162588(Task*);

extern ActorTransform           D_actor_335800_80164F80;
extern AnimationPlayRequest     D_actor_335800_80164E54;
extern AnimationPlayRequest     D_actor_335800_80164E90;
extern AnimationPlayRequest     D_actor_335800_80164F44;
extern AnimationPlayRequest     D_actor_335800_80164F58;
extern AnimationPlayRequest     D_actor_335800_80164F6C;
extern ActorCommand             D_actor_335800_80164FC8;
extern AnimationBankCopyRequest D_actor_335800_80164E24;
extern ActorTransform           D_actor_335800_80164EA4[5];
extern ActorTransform           D_actor_335800_80164F98;
extern ActorTransform           D_actor_335800_80164FB0;
void                            func_actor_335800_80162040(void);
void                            func_actor_335800_80162060(void);
void                            func_actor_335800_80162080(void);
void                            func_actor_335800_801620C0(void);
void                            func_actor_335800_801623D8(void);
void                            func_actor_335800_80162408(void);
void                            func_actor_335800_801624B8(s32);
void                            func_actor_335800_80162558(void);

extern AnimationPlayRequest     D_actor_335800_80164E40;
extern AnimationPlayRequest     D_actor_335800_80164E7C;
extern AnimationPlayRequest     D_actor_335800_80164E90;
extern ActorCommand             D_actor_335800_8016502C;
extern ActorCommand             D_actor_335800_80165038;
extern ActorCommand             D_actor_335800_8016503C;
extern ActorCommand             D_actor_335800_80165040;
extern ActorCommand             D_actor_335800_80165044;
extern AnimationBankCopyRequest D_actor_335800_80164E24;
extern EvsSceneKey              D_actor_335800_80165050;
extern EvsSceneKey              D_actor_335800_80165058;
extern ActorTransform           D_actor_335800_80164EA4[5];
s32                             func_actor_335800_8016343C(Task*, s32, s32, s32);
s32                             func_actor_335800_8016354C(Task* task, s32 msgId, ActorCommand* request, s32);
s32                             func_actor_335800_80163880(Task* task, s32 msgId, ActorTransform* place, ActorMotionWalkAnim*);
static s32                      _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
s32                             func_actor_335800_80163FB8(Task*, s32, s32, s32);
s32                             func_actor_335800_80164098(Task*, s32, s32, s32);
void                            func_actor_335800_80162040(void);
void                            func_actor_335800_80162060(void);
void                            func_actor_335800_80162080(void);
void                            func_actor_335800_801620A0(void);
void                            func_actor_335800_801620F0(u8);
void                            func_actor_335800_80162114(void);
void                            func_actor_335800_801621B4(s32);
void                            func_actor_335800_8016224C(void);
void                            func_actor_335800_801622C0(s32);
void                            func_actor_335800_80162408(void);
void                            func_actor_335800_80162428(s8);
void                            func_actor_335800_80162434(s32);
void                            func_actor_335800_80162460(void);
void                            func_actor_335800_80162484(void);
void                            func_actor_335800_801624B8(s32);
void                            func_actor_335800_80162558(void);
void                            func_actor_335800_80162E34(Task*);
void                            func_actor_335800_80162F10(Task*);
void                            func_actor_335800_80163A34(Task*);

static AnimationPackedPose _gActor335800Animation0255CBank1[6] = {
#include "assets/actor_335800_animation_0255C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0255CBank4[46] = {
#include "assets/actor_335800_animation_0255C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0255CRecords[109] = {
#include "assets/actor_335800_animation_0255C_records.inc"
};

static u16 _gActor335800Animation0255CIndices[20] = {
#include "assets/actor_335800_animation_0255C_indices.inc"
};

static AnimationSet _gActor335800Animation0255C = {
    _gActor335800Animation0255CRecords,
    _gActor335800Animation0255CIndices,
    { NULL, _gActor335800Animation0255CBank1, NULL, NULL, _gActor335800Animation0255CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02B78Bank1[12] = {
#include "assets/actor_335800_animation_02B78_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02B78Bank4[146] = {
#include "assets/actor_335800_animation_02B78_bank4.inc"
};

static AnimationRecord _gActor335800Animation02B78Records[189] = {
#include "assets/actor_335800_animation_02B78_records.inc"
};

static u16 _gActor335800Animation02B78Indices[20] = {
#include "assets/actor_335800_animation_02B78_indices.inc"
};

static AnimationSet _gActor335800Animation02B78 = {
    _gActor335800Animation02B78Records,
    _gActor335800Animation02B78Indices,
    { NULL, _gActor335800Animation02B78Bank1, NULL, NULL, _gActor335800Animation02B78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02D8CBank1[2] = {
#include "assets/actor_335800_animation_02D8C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02D8CBank4[24] = {
#include "assets/actor_335800_animation_02D8C_bank4.inc"
};

static AnimationRecord _gActor335800Animation02D8CRecords[83] = {
#include "assets/actor_335800_animation_02D8C_records.inc"
};

static u16 _gActor335800Animation02D8CIndices[20] = {
#include "assets/actor_335800_animation_02D8C_indices.inc"
};

static AnimationSet _gActor335800Animation02D8C = {
    _gActor335800Animation02D8CRecords,
    _gActor335800Animation02D8CIndices,
    { NULL, _gActor335800Animation02D8CBank1, NULL, NULL, _gActor335800Animation02D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation02F98Bank1[2] = {
#include "assets/actor_335800_animation_02F98_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation02F98Bank4[23] = {
#include "assets/actor_335800_animation_02F98_bank4.inc"
};

static AnimationRecord _gActor335800Animation02F98Records[82] = {
#include "assets/actor_335800_animation_02F98_records.inc"
};

static u16 _gActor335800Animation02F98Indices[20] = {
#include "assets/actor_335800_animation_02F98_indices.inc"
};

static AnimationSet _gActor335800Animation02F98 = {
    _gActor335800Animation02F98Records,
    _gActor335800Animation02F98Indices,
    { NULL, _gActor335800Animation02F98Bank1, NULL, NULL, _gActor335800Animation02F98Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_335800_80164DE0[4] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_335800_80161E88, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_335800_801624DC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_335800_80162364, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_335800_80162588, { .value = 0 } },
};

AnimationSet* D_actor_335800_80164E10[5] = {
    NULL,
    &_gActor335800Animation0255C,
    &_gActor335800Animation02B78,
    &_gActor335800Animation02D8C,
    &_gActor335800Animation02F98,
};

AnimationBankCopyRequest D_actor_335800_80164E24 = { { .sets = D_actor_335800_80164E10 }, ARRAY_SIZE(D_actor_335800_80164E10) };

AnimationPlayRequest D_actor_335800_80164E2C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E40 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E54 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E68 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E7C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E90 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_335800_80164EA4[5] = {
    { { -7180, -3200, 4310, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 2047, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 2047, 0, 0 } },
};

AnimationPlayRequest D_actor_335800_80164F1C[2] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_335800_80164F44 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F58 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F6C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80164F80 = { { -4000, 0, 1970, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_335800_80164F98 = { { -7730, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_335800_80164FB0 = { { -6260, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorCommand D_actor_335800_80164FC8 = { { .loc = { 3, 29 } }, 0 };

AnimationPlayRequest D_actor_335800_80164FCC = { 0 };

AnimationPlayRequest D_actor_335800_80164FE0 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164FF4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80165008 = { { -1287, 0, -2750, 0 }, { 0, -1650, 0, 0 } };

ActorCommand D_actor_335800_80165020 = { { .loc = { 3, 29 } }, 1 };

ActorCommand D_actor_335800_80165024 = { { .loc = { 3, 29 } }, 2 };

ActorCommand D_actor_335800_80165028 = { { .loc = { 3, 29 } }, 3 };

ActorCommand D_actor_335800_8016502C = { { .loc = { 3, 29 } }, 0xFFFF };

ActorCommand D_actor_335800_80165030 = { { .loc = { 3, 29 } }, 4 };

ActorCommand D_actor_335800_80165034 = { { .loc = { 3, 29 } }, 5 };

ActorCommand D_actor_335800_80165038 = { { .loc = { 3, 29 } }, 6 };

ActorCommand D_actor_335800_8016503C = { { .loc = { 3, 29 } }, 7 };

ActorCommand D_actor_335800_80165040 = { { .loc = { 3, 29 } }, 8 };

ActorCommand D_actor_335800_80165044 = { { .loc = { 3, 29 } }, 10 };

EvsSceneKey D_actor_335800_80165048 = { 3, 58, 11 };

EvsSceneKey D_actor_335800_80165050 = { 3, 59, 11 };

EvsSceneKey D_actor_335800_80165058 = { 3, 60, 11 };

EvsCommand D_actor_335800_80165060[72] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801624B8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_801623D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162408 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162558 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = D_actor_335800_80164EA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162060 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 49 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165024 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80165008 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164FF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164F80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_801620C0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165030 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80164FC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164F98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_335800_80164FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_335800_80164F6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165720[5] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165798[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_801620A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801624B8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162484 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = D_actor_335800_80164EA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162408 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80165960[21] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162484 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162114 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162558 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162060 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801622C0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801622C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_335800_801620F0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162460 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165B58[26] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016503C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 270 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165040 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801624B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_8016224C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801621B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_335800_80162428 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165DC8[21] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165044 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801621B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x401F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_801624B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_8016224C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_335800_80165FC0[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165050 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165B58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_335800_80166098[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_335800_80164E24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_335800_80162434 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_335800_80165058 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_335800_80165DC8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor335800GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor335800GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor335800GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor335800GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor335800GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor335800GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor335800GaryDouglasBodyPartVerts,
    _gActor335800GaryDouglasBodyVerts,
    _gActor335800GaryDouglasBodyNormals,
    _gActor335800GaryDouglasBodySkeleton,
    _gActor335800GaryDouglasBodyStream,
};

static TmdBone _gActor335800GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor335800GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor335800GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor335800GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor335800GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor335800GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor335800GaryDouglasHeadHatPartVerts,
    _gActor335800GaryDouglasHeadHatVerts,
    _gActor335800GaryDouglasHeadHatNormals,
    _gActor335800GaryDouglasHeadHatSkeleton,
    _gActor335800GaryDouglasHeadHatStream,
};

static TmdBone _gActor335800Actor120300Model082F8Skeleton[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

static u32 _gActor335800Actor120300Model082F8PartVerts[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

static SVECTOR _gActor335800Actor120300Model082F8Verts[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

static SVECTOR _gActor335800Actor120300Model082F8Normals[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

static u32 _gActor335800Actor120300Model082F8Stream[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

static TmdSource _gActor335800Actor120300Model082F8 = {
    0,
    1692,
    0,
    1,
    _gActor335800Actor120300Model082F8PartVerts,
    _gActor335800Actor120300Model082F8Verts,
    _gActor335800Actor120300Model082F8Normals,
    _gActor335800Actor120300Model082F8Skeleton,
    _gActor335800Actor120300Model082F8Stream,
};

static AnimationPackedPose _gActor335800Animation0AB18Bank1[2] = {
#include "assets/actor_335800_animation_0AB18_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0AB18Bank4[32] = {
#include "assets/actor_335800_animation_0AB18_bank4.inc"
};

static AnimationRecord _gActor335800Animation0AB18Records[101] = {
#include "assets/actor_335800_animation_0AB18_records.inc"
};

static u16 _gActor335800Animation0AB18Indices[20] = {
#include "assets/actor_335800_animation_0AB18_indices.inc"
};

static AnimationSet _gActor335800Animation0AB18 = {
    _gActor335800Animation0AB18Records,
    _gActor335800Animation0AB18Indices,
    { NULL, _gActor335800Animation0AB18Bank1, NULL, NULL, _gActor335800Animation0AB18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0B59CBank1[11] = {
#include "assets/actor_335800_animation_0B59C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0B59CBank4[255] = {
#include "assets/actor_335800_animation_0B59C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0B59CRecords[365] = {
#include "assets/actor_335800_animation_0B59C_records.inc"
};

static u16 _gActor335800Animation0B59CIndices[20] = {
#include "assets/actor_335800_animation_0B59C_indices.inc"
};

static AnimationSet _gActor335800Animation0B59C = {
    _gActor335800Animation0B59CRecords,
    _gActor335800Animation0B59CIndices,
    { NULL, _gActor335800Animation0B59CBank1, NULL, NULL, _gActor335800Animation0B59CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0CA48Bank1[34] = {
#include "assets/actor_335800_animation_0CA48_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0CA48Bank4[546] = {
#include "assets/actor_335800_animation_0CA48_bank4.inc"
};

static AnimationRecord _gActor335800Animation0CA48Records[655] = {
#include "assets/actor_335800_animation_0CA48_records.inc"
};

static u16 _gActor335800Animation0CA48Indices[20] = {
#include "assets/actor_335800_animation_0CA48_indices.inc"
};

static AnimationSet _gActor335800Animation0CA48 = {
    _gActor335800Animation0CA48Records,
    _gActor335800Animation0CA48Indices,
    { NULL, _gActor335800Animation0CA48Bank1, NULL, NULL, _gActor335800Animation0CA48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor335800Animation0CC7CBank1[4] = {
#include "assets/actor_335800_animation_0CC7C_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation0CC7CBank4[35] = {
#include "assets/actor_335800_animation_0CC7C_bank4.inc"
};

static AnimationRecord _gActor335800Animation0CC7CRecords[74] = {
#include "assets/actor_335800_animation_0CC7C_records.inc"
};

static u16 _gActor335800Animation0CC7CIndices[20] = {
#include "assets/actor_335800_animation_0CC7C_indices.inc"
};

static AnimationSet _gActor335800Animation0CC7C = {
    _gActor335800Animation0CC7CRecords,
    _gActor335800Animation0CC7CIndices,
    { NULL, _gActor335800Animation0CC7CBank1, NULL, NULL, _gActor335800Animation0CC7CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_335800_8016EAC4[5] = {
    NULL,
    &_gActor335800Animation0AB18,
    &_gActor335800Animation0B59C,
    &_gActor335800Animation0CA48,
    &_gActor335800Animation0CC7C,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_335800_8016EAC4,
};

TaskDesc D_actor_335800_8016EADC[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_335800_80162F10, { .model = &_gActor335800GaryDouglasBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_335800_80162E34, { .model = &_gActor335800GaryDouglasHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_335800_80162E34, { .model = &_gActor335800Actor120300Model082F8 } },
};

TaskMessageEntry D_actor_335800_8016EB00[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_335800_8016343C },
    { ACTOR_MESSAGE_WALK_TO, actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_335800_8016354C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor335800FlintBodySkeleton[19] = {
#include "assets/flint_body_skeleton.inc"
};

static u32 _gActor335800FlintBodyPartVerts[19] = {
#include "assets/flint_body_partVerts.inc"
};

static SVECTOR _gActor335800FlintBodyVerts[238] = {
#include "assets/flint_body_verts.inc"
};

static SVECTOR _gActor335800FlintBodyNormals[238] = {
#include "assets/flint_body_normals.inc"
};

static u32 _gActor335800FlintBodyStream[2784] = {
#include "assets/flint_body_stream.inc"
};

static TmdSource _gActor335800FlintBody = {
    0,
    13636,
    6016,
    19,
    _gActor335800FlintBodyPartVerts,
    _gActor335800FlintBodyVerts,
    _gActor335800FlintBodyNormals,
    _gActor335800FlintBodySkeleton,
    _gActor335800FlintBodyStream,
};

static AnimationPackedPose _gActor335800Animation11048Bank1[7] = {
#include "assets/actor_335800_animation_11048_bank1.inc"
};

static AnimationPackedRotation _gActor335800Animation11048Bank4[128] = {
#include "assets/actor_335800_animation_11048_bank4.inc"
};

static AnimationRecord _gActor335800Animation11048Records[208] = {
#include "assets/actor_335800_animation_11048_records.inc"
};

static u16 _gActor335800Animation11048Indices[20] = {
#include "assets/actor_335800_animation_11048_indices.inc"
};

static AnimationSet _gActor335800Animation11048 = {
    _gActor335800Animation11048Records,
    _gActor335800Animation11048Indices,
    { NULL, _gActor335800Animation11048Bank1, NULL, NULL, _gActor335800Animation11048Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_335800_80172E90[2] = {
    NULL,
    &_gActor335800Animation11048,
};

AnimationSet** D_actor_335800_80172E98[1] = {
    D_actor_335800_80172E90,
};

TaskDesc D_actor_335800_80172E9C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_335800_80163A34, { .model = &_gActor335800FlintBody } };

TaskMessageEntry D_actor_335800_80172EA8[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_335800_80163FB8 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_335800_80163880 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_335800_80164098 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void _actor335800SetView(s32 view);

/// Places the root part 1000 units short of its pose `D_actor_335800_80164F80`
/// and moves it along Z at `killCountdown` (100) per frame; once past the pose
/// height the velocity drops by 6 a frame until it falls below -60, which ends
/// the move. A ground shadow is drawn every frame. The
/// task kills itself once the session's `viewReady` flag is set, or a few
/// frames into state 2.
void func_actor_335800_80161E88(Task* task)
{
    GfxCoord* coord;
    VECTOR3   pos;
    SVECTOR*  rot;

    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case 0:
            coord->coord.t[0]   = D_actor_335800_80164F80.pos.vx;
            coord->coord.t[1]   = D_actor_335800_80164F80.pos.vy;
            coord->coord.t[2]   = D_actor_335800_80164F80.pos.vz - 1000;
            rot                 = &D_actor_335800_80164F80.rot;
            coord->param.rot.vx = rot->vx;
            coord->param.rot.vy = rot->vy;
            coord->param.rot.vz = rot->vz;
            RotMatrix(&coord->param.rot, &coord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->killCountdown = 100;
            task->state++;
        case 1:
            if (D_actor_335800_80164F80.pos.vz < coord->coord.t[2]) {
                task->killCountdown -= 6;
                if (task->killCountdown < -60) {
                    task->killCountdown = 0;
                    task->state++;
                }
            }
            coord->coord.t[2] += task->killCountdown;
            if (gGameSession->viewReady != 0) {
                taskKill(task);
            }
            break;
        case 2:
            if (++task->killCountdown < 4) {
                if (gGameSession->viewReady != 0) {
                    taskKill(task);
                }
            } else {
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
    if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.coordBody->coord->workm), &pos) != 0) {
        effectDrawGroundShadow(&pos, 0x800, gRoomEffectState->groundShadowShade);
    }
}

/// Script callback: queues the replacement overlay load.
void func_actor_335800_80162040(void)
{
    cdCmdStageSceneAudioStart();
}

/// Script callback: queues the overlay load.
void func_actor_335800_80162060(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Script callback: restores the stream random state.
void func_actor_335800_80162080(void)
{
    Gp_RestoreStreamRng();
}

/// Script callback: cancels the pending overlay replacement and activates the
/// loaded one.
void func_actor_335800_801620A0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_335800_801620C0(void)
{
    taskSpawnFromTable(D_actor_335800_80164DE0, 0, 0, 0);
}

void func_actor_335800_801620F0(u8 arg0)
{
    gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = arg0;
    gGameSession->roomObjsDirty                                                                  = 1;
}

void func_actor_335800_80162114(void)
{
    Task*      slot;
    TmdObject* extra;
    GfxCoord*  coord;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot != NULL) {
        extra = slot->extra.tmd;
        coord = extra->coords;
        if ((u32)(coord->coord.t[2] - 0xC53) < 0x96F) {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3E9, &D_actor_335800_80164EA4[2], 0);
        }
        if ((u32)(coord->coord.t[2] - 0x3E9) < 0x86A) {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3E9, &D_actor_335800_80164EA4[1], 0);
        }
    }
}

void func_actor_335800_801621B4(s32 arg0)
{
    Task*     slot;
    GfxCoord* coord;
    s32       lowIdx;
    s32       highIdx;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot != NULL) {
        coord  = slot->extra.tmd->coords;
        lowIdx = 1;
        if (arg0 != 0) {
            highIdx = 2;
        } else {
            lowIdx  = 3;
            highIdx = 4;
        }
        if (coord->coord.t[2] >= 0xC53) {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3E9, &D_actor_335800_80164EA4[highIdx], 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3E9, &D_actor_335800_80164EA4[lowIdx], 0);
        }
    }
}

/// Moves the saved and the live location to `view` and marks the view and
/// the room objects for reloading.
static inline void _actor335800SetView(s32 view)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = view;
    gGameSession->location.loc.view                            = view;
    gGameSession->viewDirty                                    = 1;
    gGameSession->roomObjsDirty                                = 1;
}

void func_actor_335800_8016224C(void)
{
    Task* slot;
    s16   view;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot != NULL) {
        view = 6;
        if (slot->extra.tmd->coords->coord.t[2] >= 0xC53) {
            view = 5;
        }
        _actor335800SetView(view);
    }
}

/// Sets `SpriteBatch::hidden` on two sprite commands of the area's 39th view
/// record: 1 keeps their sprites out of the ordering table and also sets game
/// flag 0x7F's nibble to 1, 0 draws them again.
void func_actor_335800_801622C0(s32 arg0)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteView*      rec;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    rec  = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].areaViews[sess->area - 1];
    switch (arg0) {
        case 0:
            batches           = rec[38].batches;
            batches[2].hidden = 0;
            batches[3].hidden = 0;
            break;
        case 1:
            batches           = rec[38].batches;
            batches[2].hidden = arg0;
            batches[3].hidden = arg0;
            gameFlagSetNibble(GAME_FLAG_07F, 1);
            break;
    }
}

void func_actor_335800_80162364(Task* arg0)
{
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value != 0) {
            evsStartScript(D_actor_335800_80166098, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        } else {
            evsStartScript(D_actor_335800_80165FC0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_actor_335800_801623D8(void)
{
    taskSpawnFromTable(D_dryfield_night_motel_balcony_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    sndEvtRequestMidiStop(gStageSceneMusicEntry, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

void func_actor_335800_801624B8(s32 arg0)
{
    gameFlagSetNibble(GAME_FLAG_STAGE_AMBIENT_MUTED, arg0);
}

void func_actor_335800_801624DC(Task* arg0)
{
    Task* slot;

    if (gGameSession->battleResetPending != 0) {
        slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        Gp_PlayerWeaponId(&D_actor_335800_80164E7C.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(slot, ANIMATION_MESSAGE_PLAY, &D_actor_335800_80164E7C, 0);
        taskKill(arg0);
    }
}

void func_actor_335800_80162558(void)
{
    taskSpawnFromTable(D_actor_335800_80164DE0, 3, 0, 0);
}

void func_actor_335800_80162588(Task* arg0)
{
    s8  var_a0;
    u8  temp_v1;
    s32 count;

    if ((gGameSession->eventState != 0) && (gGameSession->evtSkipped == 0)) {
        temp_v1 = gGameSession->padScriptFlags;
        var_a0  = 0;
        if (temp_v1 & GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE) {
            count  = (u16)arg0->killCountdown;
            var_a0 = count & 1;
        }
        if ((temp_v1 & GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE) && !(arg0->killCountdown & 1)) {
            var_a0 = -1;
        }
        displaySetShakeY(var_a0);
        arg0->killCountdown += 1;
        return;
    }
    displaySetShakeY(0);
    taskKill(arg0);
}

static void func_actor_335800_80162640(Task* arg0)
{
    _Actor335800GaryDouglasWork* work;
    GameLocationKey              key;
    GameLocationKey*             sessionKey;
    GameLocationKey*             keyAddr;
    Task*                        spawned;

    work = memCalloc(sizeof(_Actor335800GaryDouglasWork), false);
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
    spawned                  = taskSpawnFromTable(D_actor_335800_8016EADC, 1, 4, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->headTask = spawned;
        model          = spawned->extra.tmd;
        idx            = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey     = &gGameSession->location.loc;
        key.stage      = sessionKey->stage;
        key.area       = sessionKey->area;
        key.room       = sessionKey->room;
        key.view       = sessionKey->view;
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
    spawned = taskSpawnFromTable(D_actor_335800_8016EADC, 2, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->gunTask = spawned;
        model         = spawned->extra.tmd;
        idx           = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey    = (keyAddr = &gGameSession->location.loc);
        key.stage     = sessionKey->stage;
        key.area      = sessionKey->area;
        key.room      = keyAddr->room;
        key.view      = gGameSession->location.loc.view;
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
    func_actor_335800_80162F9C(arg0);
    arg0->msgTable     = D_actor_335800_8016EB00;
    arg0->exitCallback = func_actor_335800_80162F7C;
    arg0->state       += 1;
}

static void func_actor_335800_80162844(Task* task)
{
    TmdObject*                   ext      = task->extra.tmd;
    _Actor335800GaryDouglasWork* work     = (_Actor335800GaryDouglasWork*)task->work;
    TaskFunc                     funcs[2] = { func_actor_335800_80162FF4, func_actor_335800_80162FFC };
    VECTOR3                      pos;
    GfxCoord*                    coord;
    const AnimationRecord*       rec;
    s32                          i;
    s32                          j;

    funcs[work->walk.motion](task);
    coord                     = task->extra.tmd->coords;
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
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                animationTickSlot(&work->rig.anim, i);
            }
            rec = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                    effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, &task->extra.tmd->coords[8], 0xD, NULL);
                }
                work->prevCueFlags = rec->flags & ANIMATION_RECORD_CUE_MASK;
            }
        }
        if (work->lightState == ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->model.color.m[i][j] >>= 1;
                    work->model.light.m[i][j] >>= 1;
                }
                work->model.color.t[i] >>= 1;
                work->model.light.t[i] >>= 1;
            }
            work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_DIMMED;
        }
        if (work->lightState >= ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL) {
            if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
                effectDrawGroundShadow(&pos, 0x300, gRoomEffectState->groundShadowShade);
            }
        }
    }
    if (gGameSession->viewReady != 0) {
        work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL;
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

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// State dispatcher of the child part tasks: copies the three-handler table
/// onto the stack and runs the entry `Task::state` selects.
void func_actor_335800_80162E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

static void func_actor_335800_80162F08(Task* task)
{
}

/// State dispatcher of the parent block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80162F10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

static void func_actor_335800_80162F7C(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_335800_80162F9C(Task* arg0)
{
    TmdObject*                   ext;
    _Actor335800GaryDouglasWork* work;

    work          = (_Actor335800GaryDouglasWork*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
    worldCoordSetModelLighting(ext, arg0->extra.tmd->coords[1].workm.t, 0, 3);
    work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_FULL;
}

static void func_actor_335800_80162FF4(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
static void func_actor_335800_80162FFC(Task* task)
{
    _Actor335800GaryDouglasWork* work;
    TaskFuncTable4               handlers;

    work     = (_Actor335800GaryDouglasWork*)task->work;
    handlers = D_actor_335800_80161E3C;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1: rotates the constant forward offset `D_actor_335800_80161E4C`
/// through the root part's matrix into `work->walk.velocity`, seeds
/// `walk.lastDistance` with `ACTOR_WALK_DISTANCE_NONE` and advances the step.
static void func_actor_335800_80163124(Task* task)
{
    _Actor335800GaryDouglasWork* work;
    GfxCoord*                    coord;
    VECTOR                       vec;

    coord = task->extra.tmd->coords;
    work  = (_Actor335800GaryDouglasWork*)task->work;

    vec = D_actor_335800_80161E4C;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

s32 func_actor_335800_8016343C(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    _Actor335800GaryDouglasWork* work;
    TmdObject*                   obj;
    TmdObject*                   objA;
    TmdObject*                   objB;
    s32                          ret;

    work = (_Actor335800GaryDouglasWork*)task->work;
    obj  = task->extra.tmd;
    objA = work->headTask->extra.tmd;
    objB = work->gunTask->extra.tmd;
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
    objA->flags = obj->flags;
    objB->flags = obj->flags;
    return ret;
}

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor335800GaryDouglasWork* work;

    work = (_Actor335800GaryDouglasWork*)arg0->work;
    if (request->command == 0) {
        work->lightState = ACTOR_335800_GARY_DOUGLAS_LIGHT_DIM_REQUESTED;
    }
    return 0;
}

/// Per-frame tick of the child block: runs the motion handler `walk.motion`
/// selects, adds the 16.16 velocity `walk.velocity` onto the accumulator `walk.carry`,
/// moves the coordinate by the integer part and keeps only the fraction, then
/// ticks the animation slots, draws the ground shadow and rebuilds the colour
/// matrix while visible, and counts `freeCountdown` down to the buffer free.
static void func_actor_335800_80163568(Task* task)
{
    TmdObject*             ext      = task->extra.tmd;
    _Actor335800FlintWork* work     = (_Actor335800FlintWork*)task->work;
    TaskFunc               funcs[2] = { func_actor_335800_80163B70, func_actor_335800_80163B78 };
    VECTOR3                pos;
    GfxCoord*              coord;
    s32                    i;

    funcs[work->walk.motion](task);
    coord                     = task->extra.tmd->coords;
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
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
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

#include "../../shared/actor_motion_arrive19.inc.c"

/// Placement handler for the child block, the twin of
/// `actorMotionStartWalk`: stores the spawn position and rotation, then
/// applies a start preset exactly as `actorMotionPlayAnim19` does
/// (inlined here).
s32 func_actor_335800_80163880(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    _Actor335800FlintWork* work;
    _Actor335800FlintWork* w;
    AnimationPlayRequest   preset;
    AnimationPlayRequest*  msg;
    s32                    i;
    TmdObject*             ext;

    w                    = (_Actor335800FlintWork*)task->work;
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
        preset.animationId  = 0xD;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (_Actor335800FlintWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        animationInitContext(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
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

/// State dispatcher of the child block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80163A34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E5C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the two -1 bytes,
/// the -1 halfword and three cleared words the work's own init expects,
/// republishes the light and colour matrices onto the display object, then
/// installs the message table and the exit handler. An allocation failure
/// ends the task instead of leaving a half-built actor behind.
static void func_actor_335800_80163AA0(Task* arg0)
{
    _Actor335800FlintWork* work;

    work = memCalloc(sizeof(_Actor335800FlintWork), false);
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

    func_actor_335800_80163B54(arg0);

    arg0->msgTable     = D_actor_335800_80172EA8;
    arg0->exitCallback = func_actor_335800_80163B34;
    arg0->state       += 1;
}

static void func_actor_335800_80163B34(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*             ext;
    _Actor335800FlintWork* work;

    ext           = arg0->extra.tmd;
    work          = (_Actor335800FlintWork*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_335800_80163B70(Task* arg0)
{
}

/// Motion handler 1 of the child block: copies the four-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
static void func_actor_335800_80163B78(Task* arg0)
{
    TaskFuncTable4         handlers;
    _Actor335800FlintWork* work;

    work     = (_Actor335800FlintWork*)arg0->work;
    handlers = D_actor_335800_80161E68;
    handlers.funcs[work->walk.motionStep](arg0);
}

/// State handler 0 of the child block's table `D_actor_335800_80161E68`:
/// turns the root part to face `work->walk.target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances
/// `walk.motionStep` to the next handler.
static void func_actor_335800_80163BE0(Task* task)
{
    _Actor335800FlintWork* work;
    GfxCoord*              coord;
    VECTOR                 delta;
    SVECTOR                dir;
    SVECTOR                rot;

    work  = (_Actor335800FlintWork*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}

/// State handler 1 of the child block's table `D_actor_335800_80161E68`, the
/// step after the turn-to-face handler `func_actor_335800_80163BE0`: rotates
/// the constant forward offset `D_actor_335800_80161E78` through the root
/// part's matrix into `work->walk.velocity`, seeds `walk.lastDistance` with
/// `ACTOR_WALK_DISTANCE_NONE` and advances `walk.motionStep` so the dispatcher
/// `func_actor_335800_80163B78` runs the next handler.
static void func_actor_335800_80163CA0(Task* task)
{
    _Actor335800FlintWork* work;
    GfxCoord*              coord;
    VECTOR                 vec;

    coord = task->extra.tmd->coords;
    work  = (_Actor335800FlintWork*)task->work;

    vec = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

/// State handler 3 of `D_actor_335800_80161E68`, the child block's
/// turn-to-yaw step: Euler-extracts the root coordinate into `vec`, and while
/// the yaw gap to `work->walk.targetRot.vy` is at least 0x41 it steps `vec.vy` toward
/// it by 0x40, taking the step on an `s32` widening of the extracted yaw;
/// otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `model.nextAnimId` byte, clearing the motion index and step.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
static void func_actor_335800_80163D20(Task* arg0)
{
    _Actor335800FlintWork* work;
    GfxRotationWords*      words;
    GfxCoord*              coord;
    SVECTOR                vec;
    AnimationPlayRequest   preset;
    s32                    vy;
    s16                    diff;

    coord = arg0->extra.tmd->coords;
    work  = (_Actor335800FlintWork*)arg0->work;

    gfxExtractSmallestEuler(&vec, &coord->coord);
    diff = (u16)work->walk.targetRot.vy - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                      = work->walk.targetRot.vy;
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }

    words         = (GfxRotationWords*)&coord->coord;
    words->m00M01 = ONE;
    words->m02M10 = 0;
    words->m11M12 = ONE;
    words->m20M21 = 0;
    words->m22    = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Message 0x7D3 handler of the child block, the 19-slot counterpart of
/// `actorMotionPlayAnim`: re-seeds the slot array off bank table
/// `D_actor_335800_80172E98` when the preset's bank index changes, then
/// restarts or resets every slot and ticks them.
s32 actorMotionPlayAnim19(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor335800FlintWork* work;
    TmdObject*             ext;
    s32                    i;

    work = (_Actor335800FlintWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        animationInitContext(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
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

/// Selects the private placement handler for the Flint model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

s32 func_actor_335800_80163FB8(Task* task, s32 arg1, s32 mode, s32 arg3)
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
            obj->flags                                         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((_Actor335800FlintWork*)task->work)->freeCountdown = mode;
            obj->flags                                         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
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

s32 func_actor_335800_80164098(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}
