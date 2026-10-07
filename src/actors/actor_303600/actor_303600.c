#include "actors/actor_303600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/room_effects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Work block of the package's cutscene controller, the task that starts the
/// cutscene's event script and carries out the cues that script posts.
///
/// The controller allocates it cleared into `Task::work` when it arms, so its
/// size is that allocation's. A script callback posts a cue in `command`, and
/// the controller's next tick carries it out and clears it:
///
/// - 0: none.
/// - 1 to 5: broadcast the same-numbered actor command to the scene's actors;
///   4 also starts a white flash that fades away.
/// - 6 and 7: start a fade to white, fast and slow.
/// - 8: cover the screen in black and start the fading white flash over it.
///
/// Actor command 9, the last the cutscene sends, does not go through
/// `command`: either of two script callbacks broadcasts it directly.
typedef struct {
    Task* player;           // Player task when the controller armed; never read
    u16   command;          // Cue the script posted, carried out and cleared on the controller's next tick (values above)
    s16   field_6;          // Cleared with each posted cue and never read; role unproven
    byte  pad_8[0x4];       // Never accessed
    s16   lastActorCommand; // Actor command last broadcast (1 to 5, or 9); never read
    u16   endCommandSent;   // Actor command 9 has been broadcast (0 not yet, 1 sent), so its two senders send it once between them
} _Actor303600CutsceneWork;
STATIC_ASSERT_SIZEOF(_Actor303600CutsceneWork, 0x10);

/// Height of one shaft segment's model in world units, and so the pitch the
/// segments are stacked at.
#define ACTOR_303600_SHAFT_SEGMENT_HEIGHT 8000

/// Work block of one segment of the package's scrolling shaft: the light
/// matrices its model draws with.
///
/// The segment's task allocates it zeroed into `Task::work`, so its size is
/// that allocation's, and points its model at the two matrices, which the
/// model borrows for as long as the task lives.
typedef struct {
    MATRIX lightMtx;    // Light-direction matrix the model borrows, filled from the package's three flat lights
    MATRIX colorMtx;    // Light-colour matrix the model borrows, filled with `lightMtx`
    byte   pad_40[0x4]; // Never accessed
} _Actor303600ShaftSegmentWork;
STATIC_ASSERT_SIZEOF(_Actor303600ShaftSegmentWork, 0x44);

/// Work block of the package's scrolling shaft, a tube of five identical
/// segments stacked along Y that slides past without end.
///
/// The shaft's task allocates it zeroed into `Task::work`, so its size is that
/// allocation's. The segments hang under the task's own coordinate, so moving
/// that coordinate moves the whole stack. Each frame `scrollSpeed` changes by
/// `scrollAccel` until it passes `scrollSpeedLimit`, and is added to `scrollY`,
/// which wraps by one segment height to stay within half a segment of zero;
/// the segments being identical, the wrap does not show. An actor command
/// sent to the task sets the ramp.
///
/// The speed, its step and its limit are signed 16.16 world units a frame.
typedef struct {
    Task*   segments[5];      // Segment model tasks in order of increasing Y; left NULL from the first failed spawn on, never read
    s32     field_14;         // Never accessed; role unproven
    Fixed16 scrollY;          // Y translation of the stack, within half a segment height of zero
    s32     field_1C;         // Never accessed; role unproven
    s32     field_20;         // Never accessed; role unproven
    s32     field_24;         // Never accessed; role unproven
    s32     scrollSpeed;      // Added to `scrollY` each frame
    s32     field_2C;         // Never accessed; role unproven
    s32     field_30;         // Never accessed; role unproven
    s32     scrollAccel;      // Added to `scrollSpeed` each frame; cleared once the speed passes the limit
    s32     scrollSpeedLimit; // Speed the ramp ends at: above it for a positive `scrollAccel`, below it otherwise
} _Actor303600ShaftWork;
STATIC_ASSERT_SIZEOF(_Actor303600ShaftWork, 0x3C);

extern Task*    D_actor_303600_8016E4C0;
extern Task*    D_actor_303600_8016E4C4;
extern TaskDesc D_actor_303600_80162E98[];
extern TaskDesc D_actor_303600_8016E468[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_303600_8016E480[];

/// The overlay's three flat lights, loaded into the model by
/// `func_actor_303600_80162A0C`; one `GsF_LIGHT` (0x10 bytes) each.
extern GsF_LIGHT D_actor_303600_8016E490[3];

/// The cutscene's two script blocks, handed to `evsStartScriptWithSkip` together when the
/// controller below arms the cutscene.
extern EvsCommand D_actor_303600_80162AF0[];
extern EvsCommand D_actor_303600_80162DD8[];

/// Main-executable globals with no module header yet: the attachment wheel being open
/// (`Gp_StateC08.mode`) or a live `gDisplayState.pendingMode` holds the scene, and `gDisplayState.spriteVariant` is the
/// latch state 2 below sets alongside `gMcSaveData`.

static void func_actor_303600_80162850(Task* task);
static void func_actor_303600_80162950(Task* task);
static void func_actor_303600_80162A04(Task* task);
static void func_actor_303600_80162A0C(Task* task);

static TmdSource _gActor303600Model0814C;
s32              func_actor_303600_80162870(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_303600_801628E4(Task*);
void             func_actor_303600_80162A7C(Task*);

void func_actor_303600_80161E60(Task*);
void func_actor_303600_8016216C(Task*);
void func_actor_303600_801622E8(Task*);
void func_actor_303600_801623CC(Task*);
void func_actor_303600_801624B0(void);
void func_actor_303600_8016253C(void);
void func_actor_303600_80162600(s16);
void func_actor_303600_80162620(void);
void func_actor_303600_80162658(void);
void func_actor_303600_80162678(void);
void func_actor_303600_80162698(void);

EvsSceneKey D_actor_303600_80162AE8 = { 6, 12, 11 };

EvsCommand D_actor_303600_80162AF0[31] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_303600_80162AE8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_80162658 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_80162620 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_80162678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_303600_80162600 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_80162698 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_801624B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_303600_80162DD8[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_8016253C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_303600_80162E98[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_303600_8016216C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_303600_801622E8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_303600_801623CC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_303600_80161E60, { .value = 0 } },
};

static TmdBone _gActor303600Model02DD0Skeleton[1] = {
#include "assets/actor_303600_model_02DD0_skeleton.inc"
};

static u32 _gActor303600Model02DD0PartVerts[1] = {
#include "assets/actor_303600_model_02DD0_partVerts.inc"
};

static SVECTOR _gActor303600Model02DD0Verts[458] = {
#include "assets/actor_303600_model_02DD0_verts.inc"
};

static SVECTOR _gActor303600Model02DD0Normals[470] = {
#include "assets/actor_303600_model_02DD0_normals.inc"
};

static u32 _gActor303600Model02DD0Stream[4397] = {
#include "assets/actor_303600_model_02DD0_stream.inc"
};

TmdSource gActor303600Model02DD0 = {
    0,
    30316,
    0,
    1,
    _gActor303600Model02DD0PartVerts,
    _gActor303600Model02DD0Verts,
    _gActor303600Model02DD0Normals,
    _gActor303600Model02DD0Skeleton,
    _gActor303600Model02DD0Stream,
};

static AnimationPackedPose _gActor303600Animation075A0Bank1[5] = {
#include "assets/actor_303600_animation_075A0_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation075A0Bank4[61] = {
#include "assets/actor_303600_animation_075A0_bank4.inc"
};

static AnimationRecord _gActor303600Animation075A0Records[104] = {
#include "assets/actor_303600_animation_075A0_records.inc"
};

static u16 _gActor303600Animation075A0Indices[20] = {
#include "assets/actor_303600_animation_075A0_indices.inc"
};

AnimationSet gActor303600Animation075A0 = {
    _gActor303600Animation075A0Records,
    _gActor303600Animation075A0Indices,
    { NULL, _gActor303600Animation075A0Bank1, NULL, NULL, _gActor303600Animation075A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation077F0Bank1[3] = {
#include "assets/actor_303600_animation_077F0_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation077F0Bank4[29] = {
#include "assets/actor_303600_animation_077F0_bank4.inc"
};

static AnimationRecord _gActor303600Animation077F0Records[90] = {
#include "assets/actor_303600_animation_077F0_records.inc"
};

static u16 _gActor303600Animation077F0Indices[20] = {
#include "assets/actor_303600_animation_077F0_indices.inc"
};

AnimationSet gActor303600Animation077F0 = {
    _gActor303600Animation077F0Records,
    _gActor303600Animation077F0Indices,
    { NULL, _gActor303600Animation077F0Bank1, NULL, NULL, _gActor303600Animation077F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation07C30Bank1[6] = {
#include "assets/actor_303600_animation_07C30_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation07C30Bank4[79] = {
#include "assets/actor_303600_animation_07C30_bank4.inc"
};

static AnimationRecord _gActor303600Animation07C30Records[155] = {
#include "assets/actor_303600_animation_07C30_records.inc"
};

static u16 _gActor303600Animation07C30Indices[20] = {
#include "assets/actor_303600_animation_07C30_indices.inc"
};

AnimationSet gActor303600Animation07C30 = {
    _gActor303600Animation07C30Records,
    _gActor303600Animation07C30Indices,
    { NULL, _gActor303600Animation07C30Bank1, NULL, NULL, _gActor303600Animation07C30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation07E5CBank1[2] = {
#include "assets/actor_303600_animation_07E5C_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation07E5CBank4[26] = {
#include "assets/actor_303600_animation_07E5C_bank4.inc"
};

static AnimationRecord _gActor303600Animation07E5CRecords[87] = {
#include "assets/actor_303600_animation_07E5C_records.inc"
};

static u16 _gActor303600Animation07E5CIndices[20] = {
#include "assets/actor_303600_animation_07E5C_indices.inc"
};

AnimationSet gActor303600Animation07E5C = {
    _gActor303600Animation07E5CRecords,
    _gActor303600Animation07E5CIndices,
    { NULL, _gActor303600Animation07E5CBank1, NULL, NULL, _gActor303600Animation07E5CBank4, NULL, NULL, NULL },
};

static TmdBone _gActor303600Model0814CSkeleton[1] = {
#include "assets/actor_303600_model_0814C_skeleton.inc"
};

static u32 _gActor303600Model0814CPartVerts[1] = {
#include "assets/actor_303600_model_0814C_partVerts.inc"
};

static SVECTOR _gActor303600Model0814CVerts[84] = {
#include "assets/actor_303600_model_0814C_verts.inc"
};

static u32 _gActor303600Model0814CStream[286] = {
#include "assets/actor_303600_model_0814C_stream.inc"
};

static TmdSource _gActor303600Model0814C = {
    0,
    2240,
    0,
    1,
    _gActor303600Model0814CPartVerts,
    _gActor303600Model0814CVerts,
    &_gActor303600Model0814CVerts[84],
    _gActor303600Model0814CSkeleton,
    _gActor303600Model0814CStream,
};

/// The scene figure's turn, one sample per frame; `actor_403600` plays it.
Actor303600RotSample D_actor_303600_8016A408[ACTOR_303600_ROT_SAMPLE_COUNT] = {
#include "assets/actor_303600_motion_085E8.inc"
};

/// The scene's camera path, one key per frame; `actor_403600` plays it and rests
/// on the last key.
Actor303600ViewKey D_actor_303600_8016AEF8[ACTOR_303600_VIEW_KEY_COUNT] = {
#include "assets/actor_303600_path_090D8.inc"
};

TaskDesc D_actor_303600_8016E468[2] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_303600_80162A7C, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_303600_801628E4, { .model = &_gActor303600Model0814C } },
};

TaskMessageEntry D_actor_303600_8016E480[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_303600_80162870 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

GsF_LIGHT D_actor_303600_8016E490[3] = {
    { 0, 4096, 0, 128, 128, 128 },
    { 4096, 0, 0, 128, 128, 128 },
    { 0, 0, 4096, 128, 128, 128 },
};

Task* D_actor_303600_8016E4C0;

Task* D_actor_303600_8016E4C4;

static void func_actor_303600_80161F40(Task* arg0);
static void func_actor_303600_801626C0(Task* task);
static void func_actor_303600_801627B8(Task* task);

/// Entry 3 of `D_actor_303600_80162E98`, spawned by the teardown and by
/// command 8: every frame it covers the screen with an opaque black tile,
/// linked 15 slots below `gGpuCurrentOt`, followed by a draw-mode packet with
/// dithering on. It never ends itself.
void func_actor_303600_80161E60(Task* task)
{
    TILE*     p;
    DR_TPAGE* dr;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 3);
    setcode(p, 0x60);
    p->r0 = 0;
    p->g0 = 0;
    p->b0 = 0;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0xF, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 1, 0);
    addPrim(gGpuCurrentOt - 0xF, dr);
}

/// Command dispatcher the cutscene controller steps while the cutscene is up.
/// Commands 1-5 send the slot-4 task message 0x7DA carrying the session's two id
/// bytes and the command as selector, latching it in the published work block's
/// `lastActorCommand`; 4 then kills the fade in `D_actor_303600_8016E4C4` and spawns
/// `D_actor_303600_80162E98` entry 1. 6 and 7 spawn entry 2, and 8 kills the fade
/// and spawns entries 3 and 1. The command is cleared on the way out.
static void func_actor_303600_80161F40(Task* arg0)
{
    _Actor303600CutsceneWork* work = arg0->work;
    _Actor303600CutsceneWork* w;
    ActorCommand              msg;

    switch (work->command) {
        case 0:
            break;
        case 1:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 1;
            break;
        case 2:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 2;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 2;
            break;
        case 3:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 3;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 3;
            break;
        case 4:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 4;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 4;
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            taskSpawnFromTable(D_actor_303600_80162E98, 1, 4, 0);
            break;
        case 5:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 5;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 5;
            break;
        case 6:
            taskSpawnFromTable(D_actor_303600_80162E98, 2, 8, 0);
            break;
        case 7:
            taskSpawnFromTable(D_actor_303600_80162E98, 2, 4, 0);
            break;
        case 8:
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            taskSpawnFromTable(D_actor_303600_80162E98, 3, 0, 0);
            taskSpawnFromTable(D_actor_303600_80162E98, 1, 4, 0);
            break;
    }
    work->command = 0;
}

/// Cutscene controller for the overlay. State 0 arms it once: it waits while the
/// attachment wheel is open (`Gp_StateC08.mode`) or `gDisplayState.pendingMode` is live, so the state is
/// left where it is and the task returns; otherwise it allocates the
/// `_Actor303600CutsceneWork` block, zeroes it, parks the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task in
/// `player` and publishes itself in `D_actor_303600_8016E4C0` with
/// `D_actor_303600_8016E4C4` cleared, then falls into state 1, which hands the
/// overlay's two cutscene script blocks to `evsStartScriptWithSkip`. State 2 waits for
/// the session's `eventState` to clear -- the cutscene having finished -- and then
/// sets the saved location in `gMcSaveData` to stage 5, area 0x1F, warp 1,
/// room 1, raises the `gDisplayState.spriteVariant` latch, starts the stage-0 type-0x11 task and
/// kills itself; while the cutscene is still up it steps the state machine
/// instead.
void func_actor_303600_8016216C(Task* arg0)
{
    _Actor303600CutsceneWork* work;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_303600_8016E4C0 = arg0;
                D_actor_303600_8016E4C4 = NULL;
            }
            arg0->state += 1;
            /* fallthrough */
        case 1:
            evsStartScriptWithSkip(D_actor_303600_80162AF0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_303600_80162DD8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_NEO_ARK_R31;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
                gDisplayState.spriteVariant                                 = 1;
                taskSpawn(0, 0x11, 0x10, 0);
                taskKill(arg0);
                break;
            }
            func_actor_303600_80161F40(arg0);
            break;
    }
}

/// Fade-out driver: the same eight-byte channel block `func_actor_303600_801623CC`
/// walks up, walked the other way.  State 0 allocates it and fills all three
/// channels with 0xFF; a failed allocation kills the task outright.  State 1
/// draws the overlay tinted `r`/`g`/`r` in mode 1, steps all three channels down
/// by `Task::spawnArg1` -- the fade rate, not a colour -- and once `r` has gone
/// below zero clears `D_actor_303600_8016E4C4` before killing the task.
void func_actor_303600_801622E8(Task* arg0)
{
    ScreenFadeWork* work;
    ScreenFadeWork* alloc;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_ADD);
            work->r -= (u16)arg0->spawnArg1.value;
            work->g -= (u16)arg0->spawnArg1.value;
            work->b -= (u16)arg0->spawnArg1.value;
            if (work->r < 0) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// Fade-in driver: state 0 allocates the eight-byte channel block and clears
/// all three channels; a failed allocation kills the task outright.  State 1
/// runs every frame: it draws the overlay tinted `r`/`g`/`r` in mode 1, steps
/// all three channels by `Task::spawnArg1` -- the fade rate, not a colour -- and
/// once `r` has passed 0x100 clears `D_actor_303600_8016E4C4` before killing the
/// task.  The fade-out counterpart that walks the same block the other way, from
/// 0xFF down past zero, is `func_actor_303600_801622E8`.
void func_actor_303600_801623CC(Task* arg0)
{
    ScreenFadeWork* work;
    ScreenFadeWork* alloc;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_ADD);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if (work->r >= 0x100) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// One-shot announcement of the cutscene: while the work block's
/// `endCommandSent` latch is still clear, hand the slot-4 task the session's two id
/// bytes plus selector 9 as message 0x7DA, record 9 in `lastActorCommand` and
/// raise the latch so the message goes out only once.
void func_actor_303600_801624B0(void)
{
    _Actor303600CutsceneWork* work = D_actor_303600_8016E4C0->work;
    ActorCommand              msg;

    if (work->endCommandSent == 0) {
        msg.context.loc.stage = gGameSession->location.loc.stage;
        msg.context.loc.area  = gGameSession->location.loc.area;
        msg.command           = 9;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        work->lastActorCommand = 9;
        work->endCommandSent   = 1;
    }
}

/// Cutscene teardown: kill the task a previous cutscene left in
/// `D_actor_303600_8016E4C4`, then, while the work block's `endCommandSent`
/// latch is still clear, send the same 0x7DA announcement
/// `func_actor_303600_801624B0` sends and latch selector 9.  Finishes by
/// spawning the overlay's own continuation task -- `D_actor_303600_80162E98`
/// entry 3 -- so this runs exactly once per cutscene.
void func_actor_303600_8016253C(void)
{
    _Actor303600CutsceneWork* work;
    ActorCommand              msg;

    if (D_actor_303600_8016E4C4 != NULL) {
        taskKill(D_actor_303600_8016E4C4);
        D_actor_303600_8016E4C4 = NULL;
    }

    work = D_actor_303600_8016E4C0->work;
    if (work->endCommandSent == 0) {
        msg.context.loc.stage = gGameSession->location.loc.stage;
        msg.context.loc.area  = gGameSession->location.loc.area;
        msg.command           = 9;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        work->lastActorCommand = 9;
        work->endCommandSent   = 1;
    }

    taskSpawnFromTable(D_actor_303600_80162E98, 3, 0, 0);
}

void func_actor_303600_80162600(s16 arg0)
{
    _Actor303600CutsceneWork* work = D_actor_303600_8016E4C0->work;

    work->command = arg0;
    work->field_6 = 0;
}

void func_actor_303600_80162620(void)
{
    roomEffectRequestCancelPe();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Opcode-0x0D callback in the actor's cutscene script: stages the selected
/// scene's deferred audio start through `cdCmdStageSceneAudioStart`.
void func_actor_303600_80162658(void)
{
    cdCmdStageSceneAudioStart();
}

/// Opcode-0x0D callback in the actor's cutscene script: requests the selected
/// scene's playback through `cdCmdEnqueueScenePlayback`.
void func_actor_303600_80162678(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Opcode-0x0D callback in the actor's cutscene script: restores the stream
/// random-number state, then drops the pending replacement CD command through
/// `cdCmdCancelScene`.
void func_actor_303600_80162698(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}

/// Spawn state of the package's scrolling shaft: allocates the work block the
/// later states read through `Task::work`, clears the task's own root
/// coordinate, then spawns the five segment models -- one `taskSpawnFromTable`
/// of `D_actor_303600_8016E468` entry 1 each, parked in `segments` and stacked
/// one segment height apart in Y, centred on the task's coordinate.  The spread
/// reaches the coordinate through the strength-reduced `i * 8000 - 16000`
/// loop.c folds into an accumulator, so its initialiser is scheduled at the
/// loop head beside the hoisted `%hi` of the spawn table.  A failed spawn stops the loop early, a
/// failed allocation kills the task instead of leaving a half-built controller,
/// and the last three statements install the 0x7DB handler table at
/// `Task::msgTable`, the shared kill callback and the next state.
static void func_actor_303600_801626C0(Task* task)
{
    _Actor303600ShaftWork* work;
    GfxCoord*              coord;
    GfxCoord*              childCoord;
    Task*                  child;
    s32                    i;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = work;
    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    for (i = 0; i < (s32)ARRAY_SIZE(work->segments); i++) {
        child = taskSpawnFromTable(D_actor_303600_8016E468, 1, 0, task);
        if (child == NULL) {
            break;
        }
        work->segments[i]      = child;
        childCoord             = child->extra.tmd->coords;
        childCoord->coord.t[1] = i * ACTOR_303600_SHAFT_SEGMENT_HEIGHT - 2 * ACTOR_303600_SHAFT_SEGMENT_HEIGHT;
        childCoord->coord.t[0] = 0;
        childCoord->coord.t[2] = 0;
    }
    task->msgTable     = D_actor_303600_8016E480;
    task->exitCallback = func_actor_303600_80162850;
    task->state       += 1;
}

/// Per-frame motion of the scrolling shaft: ramp `scrollSpeed` by `scrollAccel`
/// toward `scrollSpeedLimit`, drop the ramp once the speed passes the limit in
/// the ramp's own direction, add the speed to `scrollY`, wrap that by one
/// segment height back within half a segment of zero, and publish its integer
/// half as the task coordinate's Y.  The accel is read once for the sum and
/// once for the limit test -- the second read is the branch's own copy of it in
/// the target.
static void func_actor_303600_801627B8(Task* task)
{
    _Actor303600ShaftWork* work  = task->work;
    GfxCoord*              coord = task->extra.tmd->coords;
    s32                    speed;
    s32                    y;
    s32                    passedLimit;

    speed             = work->scrollSpeed + work->scrollAccel;
    work->scrollSpeed = speed;
    if (work->scrollAccel > 0) {
        passedLimit = speed > work->scrollSpeedLimit;
    } else {
        passedLimit = speed < work->scrollSpeedLimit;
    }
    if (passedLimit != 0) {
        work->scrollAccel = 0;
    }
    y                  = work->scrollY.word + work->scrollSpeed;
    work->scrollY.word = y;
    // The segments are identical, so a jump of one segment height is unseen.
    if (y > (ACTOR_303600_SHAFT_SEGMENT_HEIGHT / 2) << 16) {
        work->scrollY.word = y - (ACTOR_303600_SHAFT_SEGMENT_HEIGHT << 16);
    } else if (y < -((ACTOR_303600_SHAFT_SEGMENT_HEIGHT / 2) << 16)) {
        work->scrollY.word = y + (ACTOR_303600_SHAFT_SEGMENT_HEIGHT << 16);
    }
    coord->coord.t[1]   = work->scrollY.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Exit callback the scrolling shaft installs at `Task::exitCallback`, and the
/// third entry of its state table: kills the task.
static void func_actor_303600_80162850(Task* task)
{
    taskKill(task);
}

/// Message 0x7DB handler, listed in `D_actor_303600_8016E480` -- the table
/// `func_actor_303600_801626C0` installs at `Task::msgTable`.  The payload is
/// the borrowed actor command `_sceneBroadcastToPlacedActors` forwards to the scene
/// manager's placed children, so the halfword switched on here is the sender's selector:
/// 0 sets the shaft's scroll speed to 384.0 (16.16 world units a frame) and
/// ramps it by +8.0 a frame toward 768.0, 1 ramps whatever speed it has by
/// -6.0 a frame toward -768.0, and every other selector exits the task
/// through its own `Task::exitCallback`.  `func_actor_303600_801627B8` is what
/// consumes the ramped speed.
s32 func_actor_303600_80162870(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor303600ShaftWork* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            work->scrollSpeed      = 384 << 16;
            work->scrollAccel      = 8 << 16;
            work->scrollSpeedLimit = 768 << 16;
            break;
        case 1:
            work->scrollAccel      = -(6 << 16);
            work->scrollSpeedLimit = -(768 << 16);
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// State table of the scrolling shaft: spawn, per-frame motion and the kill
/// callback. Dispatched by `func_actor_303600_80162A7C`.
static const TaskFuncTable3 D_actor_303600_80161E48 = { {
    func_actor_303600_801626C0,
    func_actor_303600_801627B8,
    func_actor_303600_80162850,
} };

/// State table of the shaft's segment tasks: spawn, an empty per-frame tick and
/// `taskKill`. Dispatched by `func_actor_303600_801628E4`.
static const TaskFuncTable3 D_actor_303600_80161E54 = { {
    func_actor_303600_80162950,
    func_actor_303600_80162A04,
    taskKill,
} };

/// Per-frame dispatcher of the shaft's segment tasks: runs their spawn, tick or
/// exit state from `D_actor_303600_80161E54`, skipping the frame while
/// `gSceneCombatState.actorControl` is set.
void func_actor_303600_801628E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_303600_80161E54;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Builds the actor's light / colour matrix pair, hangs it off the task's
/// `work` slot, and splices this task's model root under its spawn parent's.
static void func_actor_303600_80162950(Task* task)
{
    Task*                         parent      = task->spawnArg2.pointer;
    TmdObject*                    obj         = task->extra.tmd;
    GfxCoord*                     coord       = obj->coords;
    TmdObject*                    parentObj   = parent->extra.tmd;
    GfxCoord*                     parentCoord = parentObj->coords;
    _Actor303600ShaftSegmentWork* work;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }

    task->work          = work;
    coord->parent       = parentCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_303600_80162A0C(task);
    taskReparent(parent, task);
    obj->flags  &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->state += 1;
}

static void func_actor_303600_80162A04(Task* task)
{
}

/// Points the task's model at the light / colour matrix pair in its own work
/// block and loads the overlay's three flat lights into them.
static void func_actor_303600_80162A0C(Task* task)
{
    _Actor303600ShaftSegmentWork* work = task->work;
    TmdObject*                    obj  = task->extra.tmd;
    GsF_LIGHT*                    light;
    s32                           i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_actor_303600_8016E490; i < 3; i++, light++) {
        gfxSetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// Per-frame dispatcher of the scrolling shaft: runs its spawn, motion or exit
/// state from `D_actor_303600_80161E48`, skipping the frame while
/// `gSceneCombatState.actorControl` is set.
void func_actor_303600_80162A7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_303600_80161E48;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}
