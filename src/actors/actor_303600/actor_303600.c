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
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, 0 },
    { 1, 0 },
    { 2, 0 },
    { 2, 0 },
    { 3, 0 },
    { 3, 0 },
    { 4, 0 },
    { 5, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 0 },
    { 8, 0 },
    { 9, 0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 0 },
    { 14, 0 },
    { 16, 0 },
    { 17, 0 },
    { 18, 0 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 24, 0 },
    { 26, 0 },
    { 27, 0 },
    { 29, 0 },
    { 31, 0 },
    { 32, 0 },
    { 34, 0 },
    { 36, 0 },
    { 38, 0 },
    { 40, 0 },
    { 42, 0 },
    { 44, 0 },
    { 46, 0 },
    { 48, 0 },
    { 51, 0 },
    { 53, 0 },
    { 55, 0 },
    { 57, 0 },
    { 60, 0 },
    { 62, 0 },
    { 65, 0 },
    { 67, 0 },
    { 70, 0 },
    { 72, 0 },
    { 75, 0 },
    { 78, 0 },
    { 81, 0 },
    { 83, 0 },
    { 86, 0 },
    { 89, 0 },
    { 92, 0 },
    { 95, 0 },
    { 98, 0 },
    { 101, 0 },
    { 104, 0 },
    { 108, 0 },
    { 111, 0 },
    { 114, 0 },
    { 117, 0 },
    { 121, 0 },
    { 124, 0 },
    { 127, 0 },
    { 131, 0 },
    { 134, 0 },
    { 138, 0 },
    { 142, 0 },
    { 145, 0 },
    { 149, 0 },
    { 153, 0 },
    { 156, 0 },
    { 160, 0 },
    { 164, 0 },
    { 168, 0 },
    { 172, 0 },
    { 176, 0 },
    { 180, 0 },
    { 184, 0 },
    { 188, 0 },
    { 192, 0 },
    { 197, 0 },
    { 201, 0 },
    { 205, 0 },
    { 209, 0 },
    { 214, 0 },
    { 218, 0 },
    { 223, 0 },
    { 227, 0 },
    { 232, 0 },
    { 236, 0 },
    { 241, 0 },
    { 245, 0 },
    { 250, 0 },
    { 255, 0 },
    { 260, 0 },
    { 264, 0 },
    { 269, 0 },
    { 274, 0 },
    { 279, 0 },
    { 284, 0 },
    { 289, 0 },
    { 294, 0 },
    { 299, 0 },
    { 304, 0 },
    { 309, 0 },
    { 315, 0 },
    { 320, 0 },
    { 325, 0 },
    { 330, 0 },
    { 336, 0 },
    { 341, 0 },
    { 347, 0 },
    { 352, 0 },
    { 358, 0 },
    { 363, 0 },
    { 369, 0 },
    { 374, 0 },
    { 380, 0 },
    { 386, 0 },
    { 391, 0 },
    { 397, 0 },
    { 403, 0 },
    { 409, 0 },
    { 415, 0 },
    { 421, 0 },
    { 427, 0 },
    { 433, 0 },
    { 439, 0 },
    { 445, 0 },
    { 451, 0 },
    { 457, 0 },
    { 463, 0 },
    { 469, 0 },
    { 475, 0 },
    { 482, 0 },
    { 488, 0 },
    { 494, 0 },
    { 501, 0 },
    { 507, 0 },
    { 514, 0 },
    { 520, 0 },
    { 527, 0 },
    { 533, 0 },
    { 540, 0 },
    { 546, 0 },
    { 553, 0 },
    { 560, 0 },
    { 566, 0 },
    { 573, 0 },
    { 580, 0 },
    { 587, 0 },
    { 594, 0 },
    { 601, 0 },
    { 607, 0 },
    { 614, 0 },
    { 621, 0 },
    { 628, 0 },
    { 635, 0 },
    { 643, 0 },
    { 650, 0 },
    { 657, 0 },
    { 664, 0 },
    { 671, 0 },
    { 678, 0 },
    { 686, 0 },
    { 693, 0 },
    { 700, 0 },
    { 708, 0 },
    { 715, 0 },
    { 723, 0 },
    { 730, 0 },
    { 737, 0 },
    { 745, 0 },
    { 753, 0 },
    { 760, 0 },
    { 768, 0 },
    { 775, 0 },
    { 783, 0 },
    { 791, 0 },
    { 799, 0 },
    { 806, 0 },
    { 814, 0 },
    { 822, 0 },
    { 830, 0 },
    { 838, 0 },
    { 846, 0 },
    { 853, 0 },
    { 861, 0 },
    { 869, 0 },
    { 877, 0 },
    { 886, 0 },
    { 894, 0 },
    { 902, 0 },
    { 910, 0 },
    { 918, 0 },
    { 926, 0 },
    { 934, 0 },
    { 943, 0 },
    { 951, 0 },
    { 959, 0 },
    { 968, 0 },
    { 976, 0 },
    { 984, 0 },
    { 993, 0 },
    { 1001, 0 },
    { 1010, 0 },
    { 1018, 0 },
    { 1027, 0 },
    { 1035, 0 },
    { 1044, 0 },
    { 1053, 0 },
    { 1061, 0 },
    { 1070, 0 },
    { 1078, 0 },
    { 1087, 0 },
    { 1096, 0 },
    { 1105, 0 },
    { 1113, 0 },
    { 1122, 0 },
    { 1131, 0 },
    { 1140, 0 },
    { 1149, 0 },
    { 1158, 0 },
    { 1167, 0 },
    { 1176, 0 },
    { 1185, 0 },
    { 1194, 0 },
    { 1203, 0 },
    { 1212, 0 },
    { 1221, 0 },
    { 1230, 0 },
    { 1239, 0 },
    { 1248, 0 },
    { 1257, 0 },
    { 1267, 0 },
    { 1276, 0 },
    { 1285, 0 },
    { 1294, 0 },
    { 1304, 0 },
    { 1313, 0 },
    { 1322, 0 },
    { 1332, 0 },
    { 1341, 0 },
    { 1350, 0 },
    { 1360, 0 },
    { 1369, 0 },
    { 1379, 0 },
    { 1388, 0 },
    { 1398, 0 },
    { 1407, 0 },
    { 1417, 0 },
    { 1426, 0 },
    { 1436, 0 },
    { 1446, 0 },
    { 1455, 0 },
    { 1465, 0 },
    { 1475, 0 },
    { 1484, 0 },
    { 1494, 0 },
    { 1504, 0 },
    { 1514, 0 },
    { 1523, 0 },
    { 1533, 0 },
    { 1543, 0 },
    { 1553, 0 },
    { 1563, 0 },
    { 1573, 0 },
    { 1583, 0 },
    { 1592, 0 },
    { 1602, 0 },
    { 1612, 0 },
    { 1622, 0 },
    { 1632, 0 },
    { 1642, 0 },
    { 1652, 0 },
    { 1662, 0 },
    { 1673, 0 },
    { 1683, 0 },
    { 1693, 0 },
    { 1703, 0 },
    { 1713, 0 },
    { 1723, 0 },
    { 1733, 0 },
    { 1744, 0 },
    { 1754, 0 },
    { 1764, 0 },
    { 1774, 0 },
    { 1785, 0 },
    { 1795, 0 },
    { 1805, 0 },
    { 1816, 0 },
    { 1826, 0 },
    { 1836, 0 },
    { 1847, 0 },
    { 1857, 0 },
    { 1867, 0 },
    { 1878, 0 },
    { 1888, 0 },
    { 1899, 0 },
    { 1909, 0 },
    { 1920, 0 },
    { 1930, 0 },
    { 1941, 0 },
    { 1951, 0 },
    { 1962, 0 },
    { 1972, 0 },
    { 1983, 0 },
    { 1993, 0 },
    { 2004, 0 },
    { 2015, 0 },
    { 2025, 0 },
    { 2036, 0 },
    { 2047, 0 },
    { 2057, 0 },
    { 63509, 0 },
    { 63520, 0 },
    { 63530, 0 },
    { 63541, 0 },
    { 63552, 0 },
    { 63563, 0 },
    { 63573, 0 },
    { 63584, 0 },
    { 63595, 0 },
    { 63606, 0 },
    { 63617, 0 },
    { 63627, 0 },
    { 63638, 0 },
    { 63649, 0 },
    { 63660, 0 },
    { 63671, 0 },
    { 63682, 0 },
    { 63693, 0 },
    { 63704, 0 },
    { 63715, 0 },
    { 63726, 0 },
    { 63736, 0 },
    { 63747, 0 },
    { 63758, 0 },
    { 63769, 0 },
    { 63780, 0 },
    { 63791, 0 },
    { 63802, 0 },
    { 63813, 0 },
    { 63825, 0 },
    { 63836, 0 },
    { 63847, 0 },
    { 63858, 0 },
    { 63869, 0 },
    { 63880, 0 },
    { 63891, 0 },
    { 63902, 0 },
    { 63913, 0 },
    { 63924, 0 },
    { 63936, 0 },
    { 63947, 0 },
    { 63958, 0 },
    { 63969, 0 },
    { 63980, 0 },
    { 63991, 0 },
    { 64003, 0 },
    { 64014, 0 },
    { 64025, 0 },
    { 64036, 0 },
    { 64047, 0 },
    { 64059, 0 },
    { 64070, 0 },
    { 64081, 0 },
    { 64092, 0 },
    { 64104, 0 },
    { 64115, 0 },
    { 64126, 0 },
    { 64138, 0 },
    { 64149, 0 },
    { 64160, 0 },
    { 64171, 0 },
    { 64183, 0 },
    { 64194, 0 },
    { 64205, 0 },
    { 64217, 0 },
    { 64228, 0 },
    { 64239, 0 },
    { 64251, 0 },
    { 64262, 0 },
    { 64274, 0 },
    { 64285, 0 },
    { 64296, 0 },
    { 64308, 0 },
    { 64319, 0 },
    { 64330, 0 },
    { 64342, 0 },
    { 64353, 0 },
    { 64365, 0 },
    { 64376, 0 },
    { 64387, 0 },
    { 64399, 0 },
    { 64410, 0 },
    { 64421, 0 },
    { 64433, 0 },
    { 64444, 0 },
    { 64455, 1 },
    { 64466, 2 },
    { 64478, 2 },
    { 64489, 3 },
    { 64500, 5 },
    { 64511, 6 },
    { 64523, 7 },
    { 64534, 9 },
    { 64545, 10 },
    { 64556, 12 },
    { 64567, 14 },
    { 64578, 16 },
    { 64589, 18 },
    { 64601, 20 },
    { 64612, 22 },
    { 64623, 24 },
    { 64634, 27 },
    { 64645, 29 },
    { 64656, 32 },
    { 64667, 35 },
    { 64678, 38 },
    { 64689, 41 },
    { 64700, 44 },
    { 64711, 47 },
    { 64721, 50 },
    { 64732, 53 },
    { 64743, 57 },
    { 64754, 60 },
    { 64765, 64 },
    { 64776, 68 },
    { 64787, 71 },
    { 64797, 75 },
    { 64808, 79 },
    { 64819, 84 },
    { 64830, 88 },
    { 64840, 92 },
    { 64851, 96 },
    { 64862, 101 },
    { 64872, 105 },
    { 64883, 110 },
    { 64893, 115 },
    { 64904, 120 },
    { 64915, 124 },
    { 64925, 129 },
    { 64936, 134 },
    { 64946, 140 },
    { 64957, 145 },
    { 64967, 150 },
    { 64977, 155 },
    { 64988, 161 },
    { 64998, 166 },
    { 65009, 172 },
    { 65019, 178 },
    { 65029, 183 },
    { 65040, 189 },
    { 65050, 195 },
    { 65060, 201 },
    { 65070, 207 },
    { 65081, 213 },
    { 65091, 219 },
    { 65101, 226 },
    { 65111, 232 },
    { 65121, 238 },
    { 65131, 245 },
    { 65141, 251 },
    { 65151, 258 },
    { 65161, 265 },
    { 65171, 271 },
    { 65181, 278 },
    { 65191, 285 },
    { 65201, 292 },
    { 65211, 299 },
    { 65221, 306 },
    { 65231, 313 },
    { 65240, 320 },
    { 65250, 327 },
    { 65260, 334 },
    { 65270, 342 },
    { 65279, 349 },
    { 65289, 356 },
    { 65299, 364 },
    { 65308, 371 },
    { 65318, 379 },
    { 65328, 387 },
    { 65337, 394 },
    { 65347, 402 },
    { 65356, 410 },
    { 65365, 417 },
    { 65375, 425 },
    { 65384, 433 },
    { 65394, 441 },
    { 65403, 449 },
    { 65412, 457 },
    { 65422, 465 },
    { 65431, 473 },
    { 65440, 482 },
    { 65449, 490 },
    { 65458, 498 },
    { 65467, 506 },
    { 65477, 515 },
    { 65486, 523 },
    { 65495, 531 },
    { 65504, 540 },
    { 65513, 548 },
    { 65522, 557 },
    { 65530, 565 },
    { 2, 574 },
    { 11, 582 },
    { 20, 591 },
    { 29, 599 },
    { 37, 608 },
    { 46, 617 },
    { 55, 625 },
    { 64, 634 },
    { 72, 643 },
    { 81, 652 },
    { 89, 661 },
    { 98, 669 },
    { 106, 678 },
    { 115, 687 },
    { 123, 696 },
    { 131, 705 },
    { 140, 714 },
    { 148, 723 },
    { 156, 732 },
    { 165, 741 },
    { 173, 750 },
    { 181, 759 },
    { 189, 768 },
    { 197, 777 },
    { 205, 786 },
    { 213, 795 },
    { 221, 804 },
    { 229, 813 },
    { 237, 822 },
    { 245, 831 },
    { 253, 840 },
    { 261, 850 },
    { 268, 859 },
    { 276, 868 },
    { 284, 877 },
    { 291, 886 },
    { 299, 895 },
    { 307, 904 },
    { 314, 913 },
    { 322, 923 },
    { 329, 932 },
    { 337, 941 },
    { 344, 950 },
    { 351, 959 },
    { 359, 968 },
    { 366, 977 },
    { 373, 986 },
    { 380, 996 },
    { 387, 1005 },
    { 394, 1014 },
    { 401, 1023 },
    { 409, 1032 },
    { 415, 1041 },
    { 422, 1050 },
    { 429, 1059 },
    { 436, 1068 },
    { 443, 1077 },
    { 450, 1086 },
    { 456, 1095 },
    { 463, 1104 },
    { 470, 1113 },
    { 476, 1122 },
    { 483, 1131 },
    { 489, 1140 },
    { 496, 1148 },
    { 502, 1157 },
    { 509, 1166 },
    { 515, 1175 },
    { 521, 1184 },
    { 528, 1192 },
    { 534, 1201 },
    { 540, 1210 },
    { 546, 1218 },
    { 552, 1227 },
    { 558, 1236 },
    { 564, 1244 },
    { 570, 1253 },
    { 576, 1261 },
    { 582, 1270 },
    { 588, 1278 },
    { 594, 1287 },
    { 599, 1295 },
    { 605, 1303 },
    { 611, 1312 },
    { 616, 1320 },
    { 622, 1328 },
    { 627, 1336 },
    { 633, 1345 },
    { 638, 1353 },
    { 643, 1361 },
    { 649, 1369 },
    { 654, 1377 },
    { 659, 1385 },
    { 664, 1393 },
    { 669, 1400 },
    { 674, 1408 },
    { 679, 1416 },
    { 684, 1424 },
    { 689, 1431 },
    { 694, 1439 },
    { 699, 1447 },
    { 704, 1454 },
    { 708, 1462 },
    { 713, 1469 },
    { 718, 1476 },
    { 722, 1484 },
    { 727, 1491 },
    { 731, 1498 },
    { 736, 1505 },
    { 740, 1512 },
    { 744, 1519 },
    { 749, 1526 },
    { 753, 1533 },
    { 757, 1540 },
    { 761, 1547 },
    { 765, 1554 },
    { 769, 1560 },
    { 773, 1567 },
    { 777, 1573 },
    { 781, 1580 },
    { 785, 1586 },
    { 788, 1593 },
    { 792, 1599 },
    { 796, 1605 },
    { 799, 1611 },
    { 803, 1617 },
    { 806, 1623 },
    { 810, 1629 },
    { 813, 1635 },
    { 816, 1641 },
    { 820, 1646 },
    { 823, 1652 },
    { 826, 1657 },
    { 829, 1663 },
    { 832, 1668 },
    { 835, 1674 },
    { 838, 1679 },
    { 841, 1684 },
    { 844, 1689 },
    { 846, 1694 },
    { 849, 1699 },
    { 852, 1704 },
    { 854, 1708 },
    { 857, 1713 },
    { 859, 1718 },
    { 862, 1722 },
    { 864, 1726 },
    { 866, 1731 },
    { 869, 1735 },
    { 871, 1739 },
    { 873, 1743 },
    { 875, 1747 },
    { 877, 1751 },
    { 879, 1755 },
    { 881, 1758 },
    { 883, 1762 },
    { 885, 1765 },
    { 886, 1769 },
    { 888, 1772 },
    { 889, 1775 },
    { 891, 1778 },
    { 893, 1781 },
    { 894, 1784 },
    { 895, 1787 },
    { 897, 1789 },
    { 898, 1792 },
    { 899, 1794 },
    { 900, 1797 },
    { 901, 1799 },
    { 902, 1801 },
    { 903, 1803 },
    { 904, 1805 },
    { 905, 1807 },
    { 906, 1809 },
    { 906, 1810 },
    { 907, 1812 },
    { 907, 1813 },
    { 908, 1814 },
    { 908, 1816 },
    { 909, 1817 },
    { 909, 1817 },
    { 909, 1818 },
    { 910, 1819 },
    { 910, 1819 },
    { 910, 1820 },
};

/// The scene's camera path, one key per frame; `actor_403600` plays it and rests
/// on the last key.
Actor303600ViewKey D_actor_303600_8016AEF8[ACTOR_303600_VIEW_KEY_COUNT] = {
    { { -3293, 0, 2435, 1312, 3450, 1775, -2051, 2207, -2774 }, { -1454, 4339, -2396 } },
    { { -3293, 0, 2435, 1312, 3450, 1775, -2051, 2207, -2774 }, { -1454, 4339, -2396 } },
    { { -3293, 0, 2435, 1312, 3450, 1774, -2051, 2207, -2774 }, { -1454, 4338, -2395 } },
    { { -3293, 0, 2435, 1312, 3450, 1774, -2052, 2206, -2774 }, { -1454, 4336, -2394 } },
    { { -3292, 0, 2436, 1312, 3451, 1773, -2052, 2205, -2774 }, { -1454, 4334, -2393 } },
    { { -3292, 0, 2436, 1311, 3451, 1772, -2053, 2204, -2774 }, { -1454, 4331, -2391 } },
    { { -3291, 0, 2437, 1311, 3452, 1770, -2054, 2203, -2774 }, { -1454, 4328, -2389 } },
    { { -3291, 0, 2438, 1310, 3453, 1769, -2056, 2202, -2775 }, { -1454, 4323, -2387 } },
    { { -3290, 0, 2439, 1310, 3454, 1767, -2057, 2200, -2775 }, { -1454, 4319, -2385 } },
    { { -3289, 0, 2440, 1309, 3456, 1765, -2059, 2198, -2775 }, { -1454, 4313, -2382 } },
    { { -3288, 0, 2441, 1308, 3457, 1763, -2061, 2195, -2776 }, { -1454, 4307, -2379 } },
    { { -3287, 0, 2443, 1308, 3459, 1760, -2063, 2193, -2776 }, { -1454, 4300, -2376 } },
    { { -3286, 0, 2444, 1307, 3461, 1757, -2065, 2190, -2777 }, { -1454, 4293, -2372 } },
    { { -3285, 0, 2446, 1306, 3463, 1754, -2068, 2187, -2777 }, { -1454, 4285, -2369 } },
    { { -3284, 0, 2447, 1305, 3465, 1750, -2070, 2183, -2778 }, { -1455, 4277, -2365 } },
    { { -3282, 0, 2449, 1303, 3467, 1747, -2073, 2179, -2779 }, { -1455, 4267, -2361 } },
    { { -3281, 0, 2451, 1302, 3470, 1743, -2077, 2175, -2779 }, { -1456, 4258, -2357 } },
    { { -3279, 0, 2453, 1300, 3473, 1738, -2080, 2171, -2780 }, { -1456, 4247, -2352 } },
    { { -3277, 0, 2456, 1299, 3475, 1734, -2084, 2166, -2781 }, { -1457, 4236, -2348 } },
    { { -3276, 0, 2458, 1297, 3479, 1729, -2088, 2161, -2782 }, { -1458, 4224, -2343 } },
    { { -3274, 0, 2460, 1295, 3482, 1723, -2092, 2156, -2783 }, { -1459, 4212, -2339 } },
    { { -3272, 0, 2463, 1293, 3485, 1718, -2096, 2150, -2785 }, { -1460, 4199, -2334 } },
    { { -3270, 0, 2466, 1291, 3489, 1712, -2101, 2144, -2786 }, { -1461, 4186, -2 } },
    { { -3268, 0, 2469, 1288, 3493, 1705, -2105, 2138, -2787 }, { -1463, 4172, -2324 } },
    { { -3265, 0, 2471, 1286, 3497, 1699, -2110, 2131, -2789 }, { -1465, 4158, -2319 } },
    { { -3263, 0, 2475, 1283, 3502, 1692, -2116, 2124, -2790 }, { -1467, 4143, -2314 } },
    { { -3261, 0, 2478, 1280, 3506, 1685, -2121, 2116, -2792 }, { -1469, 4127, -2310 } },
    { { -3258, 0, 2481, 1277, 3511, 1677, -2127, 2108, -2793 }, { -1471, 4111, -2305 } },
    { { -3256, 0, 2484, 1274, 3516, 1669, -2133, 2100, -2795 }, { -1474, 4094, -2300 } },
    { { -3253, 0, 2488, 1270, 3521, 1661, -2139, 2091, -2797 }, { -1476, 4077, -2295 } },
    { { -3250, 0, 2492, 1267, 3526, 1652, -2145, 2082, -2799 }, { -1479, 4059, -2290 } },
    { { -3247, 0, 2495, 1263, 3532, 1644, -2152, 2073, -2801 }, { -1482, 4041, -2285 } },
    { { -3244, 0, 2499, 1259, 3538, 1634, -2159, 2063, -2803 }, { -1486, 4022, -2280 } },
    { { -3241, 0, 2503, 1255, 3544, 1625, -2166, 2053, -2805 }, { -1489, 4003, -2276 } },
    { { -3238, 0, 2507, 1250, 3550, 1615, -2173, 2042, -2807 }, { -1493, 3983, -2271 } },
    { { -3235, 0, 2511, 1245, 3556, 1605, -2180, 2032, -2809 }, { -1497, 3963, -2266 } },
    { { -3232, 0, 2515, 1241, 3562, 1594, -2188, 2020, -2811 }, { -1501, 3943, -2262 } },
    { { -3229, 0, 2519, 1235, 3569, 1583, -2195, 2008, -2814 }, { -1506, 3922, -2257 } },
    { { -3225, 0, 2524, 1230, 3576, 1572, -2203, 1996, -2816 }, { -1510, 3900, -2253 } },
    { { -3222, 0, 2528, 1225, 3583, 1561, -2212, 1984, -2818 }, { -1515, 3878, -2249 } },
    { { -3218, 0, 2533, 1219, 3590, 1549, -2220, 1971, -2821 }, { -1520, 3856, -2245 } },
    { { -3215, 0, 2537, 1213, 3597, 1537, -2228, 1958, -2823 }, { -1526, 3833, -2240 } },
    { { -3211, 0, 2542, 1207, 3604, 1524, -2237, 1944, -2826 }, { -1531, 3810, -2236 } },
    { { -3207, 0, 2546, 1200, 3612, 1512, -2246, 1930, -2829 }, { -1537, 3787, -2233 } },
    { { -3204, 0, 2551, 1193, 3620, 1499, -2255, 1916, -2831 }, { -1543, 3763, -2229 } },
    { { -3200, 0, 2556, 1186, 3627, 1485, -2264, 1901, -2834 }, { -1549, 3739, -2225 } },
    { { -3196, 0, 2561, 1179, 3635, 1472, -2273, 1886, -2837 }, { -1556, 3714, -2221 } },
    { { -3192, 0, 2566, 1172, 3643, 1458, -2282, 1871, -2839 }, { -1562, 3689, -2218 } },
    { { -3188, 0, 2571, 1164, 3651, 1444, -2292, 1855, -2842 }, { -1569, 3664, -2215 } },
    { { -3184, 0, 2576, 1156, 3659, 1429, -2302, 1839, -2845 }, { -1576, 3639, -2211 } },
    { { -3180, 0, 2581, 1148, 3668, 1414, -2311, 1822, -2848 }, { -1584, 3613, -2208 } },
    { { -3176, 0, 2586, 1140, 3676, 1399, -2321, 1805, -2850 }, { -1591, 3586, -2205 } },
    { { -3171, 0, 2591, 1131, 3685, 1384, -2331, 1788, -2853 }, { -1599, 3560, -2202 } },
    { { -3167, 0, 2596, 1122, 3693, 1369, -2341, 1770, -2856 }, { -1607, 3533, -2200 } },
    { { -3163, 0, 2602, 1113, 3702, 1353, -2351, 1752, -2859 }, { -1615, 3506, -2197 } },
    { { -3158, 0, 2607, 1103, 3710, 1337, -2362, 1734, -2861 }, { -1623, 3479, -2194 } },
    { { -3154, 0, 2612, 1094, 3719, 1321, -2372, 1715, -2864 }, { -1631, 3451, -2192 } },
    { { -3150, 0, 2618, 1084, 3728, 1304, -2382, 1696, -2867 }, { -1640, 3423, -2190 } },
    { { -3145, 0, 2623, 1074, 3736, 1288, -2393, 1677, -2869 }, { -1649, 3395, -2188 } },
    { { -3141, 0, 2628, 1063, 3745, 1271, -2403, 1657, -2872 }, { -1658, 3367, -2185 } },
    { { -3136, 0, 2634, 1053, 3754, 1254, -2414, 1637, -2875 }, { -1667, 3338, -2184 } },
    { { -3132, 0, 2639, 1042, 3763, 1236, -2425, 1617, -2877 }, { -1676, 3310, -2182 } },
    { { -3127, 0, 2644, 1031, 3771, 1219, -2435, 1596, -2880 }, { -1686, 3281, -2180 } },
    { { -3122, 0, 2650, 1019, 3780, 1201, -2446, 1575, -2882 }, { -1695, 3251, -2179 } },
    { { -3118, 0, 2655, 1008, 3789, 1183, -2457, 1554, -2884 }, { -1705, 3222, -2177 } },
    { { -3113, 0, 2661, 996, 3798, 1165, -2467, 1533, -2887 }, { -1715, 3193, -2176 } },
    { { -3108, 0, 2666, 984, 3806, 1147, -2478, 1511, -2889 }, { -1725, 3163, -2175 } },
    { { -3104, 0, 2672, 971, 3815, 1128, -2489, 1489, -2891 }, { -1736, 3133, -2174 } },
    { { -3099, 0, 2677, 959, 3824, 1110, -2499, 1467, -2893 }, { -1746, 3103, -2173 } },
    { { -3094, 0, 2683, 946, 3832, 1091, -2510, 1444, -2895 }, { -1757, 3073, -2172 } },
    { { -3090, 0, 2688, 933, 3841, 1072, -2521, 1422, -2897 }, { -1767, 3042, -2172 } },
    { { -3085, 0, 2693, 920, 3849, 1053, -2531, 1399, -2899 }, { -1778, 3012, -2171 } },
    { { -3080, 0, 2699, 906, 3858, 1034, -2542, 1375, -2901 }, { -1789, 2981, -2171 } },
    { { -3075, 0, 2704, 893, 3866, 1015, -2553, 1352, -2903 }, { -1801, 2951, -2170 } },
    { { -3071, 0, 2710, 879, 3874, 996, -2563, 1328, -2905 }, { -1812, 2920, -2170 } },
    { { -3066, 0, 2715, 865, 3882, 976, -2574, 1305, -2906 }, { -1823, 2889, -2170 } },
    { { -3061, 0, 2720, 850, 3890, 957, -2584, 1281, -2908 }, { -1835, 2858, -2170 } },
    { { -3056, 0, 2726, 836, 3898, 938, -2594, 1256, -2909 }, { -1847, 2827, -2171 } },
    { { -3052, 0, 2731, 821, 3906, 918, -2604, 1232, -2910 }, { -1859, 2796, -2171 } },
    { { -3047, 0, 2736, 807, 3913, 898, -2615, 1208, -2911 }, { -1870, 2765, -2172 } },
    { { -3042, 0, 2742, 792, 3921, 879, -2625, 1183, -2912 }, { -1882, 2733, -2172 } },
    { { -3037, 0, 2747, 777, 3928, 859, -2635, 1158, -2913 }, { -1895, 2702, -2173 } },
    { { -3033, 0, 2752, 762, 3935, 839, -2645, 1133, -2914 }, { -1907, 2671, -2174 } },
    { { -3028, 0, 2757, 746, 3943, 819, -2654, 1108, -2915 }, { -1919, 2639, -2175 } },
    { { -3023, 0, 2762, 731, 3949, 800, -2664, 1083, -2916 }, { -1932, 2608, -2176 } },
    { { -3019, 0, 2768, 715, 3956, 780, -2673, 1058, -2916 }, { -1944, 2577, -2177 } },
    { { -3014, 0, 2773, 699, 3963, 760, -2683, 1033, -2916 }, { -1957, 2545, -2179 } },
    { { -3009, 0, 2778, 683, 3969, 740, -2692, 1008, -2917 }, { -1970, 2514, -2180 } },
    { { -3005, 0, 2783, 667, 3976, 721, -2701, 983, -2917 }, { -1982, 2482, -2182 } },
    { { -3000, 0, 2788, 651, 3982, 701, -2710, 957, -2917 }, { -1995, 2451, -2183 } },
    { { -2995, 0, 2793, 635, 3988, 681, -2719, 932, -2917 }, { -2008, 2420, -2185 } },
    { { -2991, 0, 2797, 619, 3994, 662, -2728, 907, -2917 }, { -2021, 2388, -2187 } },
    { { -2986, 0, 2802, 603, 3999, 642, -2737, 881, -2916 }, { -2035, 2357, -2189 } },
    { { -2982, 0, 2807, 586, 4005, 623, -2745, 856, -2916 }, { -2048, 2326, -2191 } },
    { { -2977, 0, 2812, 570, 4010, 604, -2753, 831, -2915 }, { -2061, 2295, -2194 } },
    { { -2973, 0, 2817, 554, 4015, 584, -2761, 805, -2915 }, { -2074, 2264, -2196 } },
    { { -2969, 0, 2821, 537, 4020, 565, -2769, 780, -2914 }, { -2088, 2233, -2198 } },
    { { -2964, 0, 2826, 521, 4025, 546, -2777, 755, -2913 }, { -2101, 2202, -2201 } },
    { { -2960, 0, 2830, 504, 4030, 527, -2785, 730, -2912 }, { -2114, 2171, -2204 } },
    { { -2956, 0, 2835, 488, 4034, 509, -2793, 705, -2911 }, { -2128, 2140, -2207 } },
    { { -2951, 0, 2839, 471, 4039, 490, -2800, 680, -2910 }, { -2141, 2110, -2210 } },
    { { -2947, 0, 2844, 455, 4043, 471, -2807, 655, -2909 }, { -2155, 2079, -2213 } },
    { { -2943, 0, 2848, 438, 4047, 453, -2814, 630, -2908 }, { -2169, 2049, -2216 } },
    { { -2939, 0, 2852, 422, 4050, 435, -2821, 606, -2906 }, { -2182, 2019, -2219 } },
    { { -2935, 0, 2857, 405, 4054, 417, -2828, 582, -2905 }, { -2196, 1988, -2222 } },
    { { -2930, 0, 2861, 389, 4057, 399, -2834, 557, -2903 }, { -2209, 1958, -2226 } },
    { { -2926, 0, 2865, 373, 4061, 381, -2840, 533, -2901 }, { -2223, 1929, -2229 } },
    { { -2922, 0, 2869, 357, 4064, 363, -2847, 509, -2900 }, { -2237, 1899, -2233 } },
    { { -2918, 0, 2873, 340, 4067, 346, -2853, 486, -2898 }, { -2250, 1869, -2236 } },
    { { -2915, 0, 2877, 324, 4069, 329, -2858, 462, -2896 }, { -2264, 1840, -2240 } },
    { { -2911, 0, 2881, 308, 4072, 312, -2864, 439, -2894 }, { -2278, 1811, -2244 } },
    { { -2907, 0, 2885, 293, 4074, 295, -2870, 416, -2892 }, { -2291, 1782, -2248 } },
    { { -2903, 0, 2888, 277, 4077, 278, -2875, 393, -2890 }, { -2305, 1753, -2252 } },
    { { -2900, 0, 2892, 261, 4079, 262, -2880, 370, -2888 }, { -2319, 1725, -2256 } },
    { { -2896, 0, 2896, 246, 4081, 246, -2885, 347, -2885 }, { -2332, 1697, -2260 } },
    { { -2892, 0, 2899, 230, 4083, 230, -2890, 325, -2883 }, { -2346, 1669, -2264 } },
    { { -2889, 0, 2903, 215, 4084, 214, -2895, 303, -2881 }, { -2359, 1641, -2269 } },
    { { -2885, 0, 2906, 200, 4086, 198, -2899, 282, -2878 }, { -2373, 1613, -2273 } },
    { { -2882, 0, 2910, 185, 4087, 183, -2904, 260, -2876 }, { -2386, 1586, -2278 } },
    { { -2878, 0, 2913, 170, 4088, 168, -2908, 239, -2874 }, { -2400, 1559, -2282 } },
    { { -2875, 0, 2916, 155, 4090, 153, -2912, 218, -2871 }, { -2413, 1532, -2287 } },
    { { -2872, 0, 2920, 141, 4091, 138, -2916, 197, -2868 }, { -2427, 1505, -2291 } },
    { { -2869, 0, 2923, 126, 4092, 124, -2920, 177, -2866 }, { -2440, 1479, -2296 } },
    { { -2865, 0, 2926, 112, 4092, 110, -2924, 157, -2863 }, { -2453, 1453, -2301 } },
    { { -2862, 0, 2929, 98, 4093, 96, -2927, 137, -2861 }, { -2466, 1427, -2306 } },
    { { -2859, 0, 2932, 84, 4094, 82, -2931, 118, -2858 }, { -2479, 1402, -2310 } },
    { { -2856, 0, 2935, 71, 4094, 69, -2934, 99, -2855 }, { -2492, 1376, -2315 } },
    { { -2853, 0, 2938, 57, 4095, 56, -2937, 80, -2853 }, { -2505, 1352, -2320 } },
    { { -2851, 0, 2940, 44, 4095, 43, -2940, 61, -2850 }, { -2518, 1327, -2325 } },
    { { -2848, 0, 2943, 31, 4095, 30, -2943, 43, -2848 }, { -2531, 1303, -2330 } },
    { { -2845, 0, 2946, 18, 4095, 18, -2946, 25, -2845 }, { -2543, 1279, -2335 } },
    { { -2842, 0, 2948, 6, 4095, 5, -2948, 8, -2842 }, { -2556, 1255, -2340 } },
    { { -2840, 0, 2951, -6, 4095, -6, -2951, -8, -2840 }, { -2568, 1232, -2345 } },
    { { -2837, 0, 2953, -18, 4095, -17, -2953, -25, -2837 }, { -2580, 1209, -2350 } },
    { { -2835, 0, 2956, -30, 4095, -29, -2956, -42, -2834 }, { -2593, 1187, -2356 } },
    { { -2832, 0, 2958, -42, 4095, -40, -2958, -58, -2832 }, { -2605, 1165, -2361 } },
    { { -2830, 0, 2960, -53, 4095, -51, -2960, -74, -2829 }, { -2616, 1143, -2366 } },
    { { -2827, 0, 2963, -64, 4095, -61, -2962, -89, -2827 }, { -2628, 1121, -2371 } },
    { { -2825, 0, 2965, -75, 4094, -72, -2964, -104, -2824 }, { -2640, 1100, -2376 } },
    { { -2823, 0, 2967, -86, 4094, -82, -2966, -119, -2822 }, { -2651, 1080, -2381 } },
    { { -2821, 0, 2969, -97, 4093, -92, -2967, -134, -2819 }, { -2663, 1059, -2386 } },
    { { -2819, 0, 2971, -107, 4093, -102, -2969, -148, -2817 }, { -2674, 1040, -2391 } },
    { { -2817, 0, 2973, -117, 4092, -111, -2971, -162, -2814 }, { -2685, 1020, -2396 } },
    { { -2815, 0, 2975, -127, 4092, -120, -2972, -175, -2812 }, { -2696, 1001, -2401 } },
    { { -2813, 0, 2977, -137, 4091, -129, -2974, -188, -2810 }, { -2706, 982, -2406 } },
    { { -2811, 0, 2978, -146, 4091, -138, -2975, -201, -2807 }, { -2717, 964, -2411 } },
    { { -2809, 0, 2980, -155, 4090, -146, -2976, -213, -2805 }, { -2727, 946, -2416 } },
    { { -2807, 0, 2982, -164, 4089, -154, -2977, -225, -2803 }, { -2737, 929, -2421 } },
    { { -2805, 0, 2983, -172, 4089, -162, -2978, -237, -2801 }, { -2747, 912, -2426 } },
    { { -2804, 0, 2985, -181, 4088, -170, -2979, -248, -2799 }, { -2757, 896, -2431 } },
    { { -2802, 0, 2986, -187, 4087, -176, -2981, -257, -2797 }, { -2766, 879, -2436 } },
    { { -2801, 0, 2988, -194, 4087, -181, -2982, -266, -2795 }, { -2776, 863, -2441 } },
    { { -2799, 0, 2989, -200, 4086, -187, -2983, -274, -2793 }, { -2786, 847, -2445 } },
    { { -2798, 0, 2991, -206, 4086, -193, -2984, -283, -2791 }, { -2795, 831, -2451 } },
    { { -2796, 0, 2992, -212, 4085, -198, -2984, -291, -2789 }, { -2805, 815, -2456 } },
    { { -2795, 0, 2993, -218, 4085, -204, -2985, -299, -2787 }, { -2815, 800, -2461 } },
    { { -2793, 0, 2995, -224, 4084, -209, -2986, -307, -2786 }, { -2824, 784, -2466 } },
    { { -2792, 0, 2996, -230, 4083, -215, -2987, -315, -2784 }, { -2834, 768, -2472 } },
    { { -2791, 0, 2997, -236, 4083, -220, -2988, -323, -2782 }, { -2844, 753, -2477 } },
    { { -2789, 0, 2999, -242, 4082, -225, -2989, -331, -2780 }, { -2854, 737, -2482 } },
    { { -2788, 0, 3000, -248, 4081, -230, -2989, -338, -2778 }, { -2864, 722, -2488 } },
    { { -2787, 0, 3001, -253, 4081, -235, -2990, -346, -2777 }, { -2873, 707, -2494 } },
    { { -2785, 0, 3002, -259, 4080, -240, -2991, -353, -2775 }, { -2883, 691, -2499 } },
    { { -2784, 0, 3003, -264, 4080, -245, -2992, -361, -2773 }, { -2893, 676, -2505 } },
    { { -2783, 0, 3004, -270, 4079, -250, -2992, -368, -2772 }, { -2903, 661, -2511 } },
    { { -2782, 0, 3006, -275, 4078, -255, -2993, -375, -2770 }, { -2913, 646, -2517 } },
    { { -2781, 0, 3007, -280, 4078, -259, -2993, -382, -2768 }, { -2923, 631, -2522 } },
    { { -2779, 0, 3008, -286, 4077, -264, -2994, -389, -2767 }, { -2933, 616, -2528 } },
    { { -2778, 0, 3009, -291, 4076, -269, -2995, -396, -2765 }, { -2942, 601, -2534 } },
    { { -2777, 0, 3010, -296, 4076, -273, -2995, -403, -2764 }, { -2952, 587, -2540 } },
    { { -2776, 0, 3011, -301, 4075, -277, -2996, -409, -2762 }, { -2962, 572, -2546 } },
    { { -2775, 0, 3012, -306, 4074, -282, -2996, -416, -2761 }, { -2972, 557, -2552 } },
    { { -2774, 0, 3013, -311, 4074, -286, -2997, -423, -2759 }, { -2982, 543, -2559 } },
    { { -2773, 0, 3014, -316, 4073, -290, -2997, -429, -2757 }, { -2992, 528, -2565 } },
    { { -2772, 0, 3015, -320, 4072, -294, -2998, -435, -2756 }, { -3002, 514, -2571 } },
    { { -2771, 0, 3016, -325, 4072, -299, -2998, -441, -2754 }, { -3012, 499, -2577 } },
    { { -2770, 0, 3017, -330, 4071, -303, -2999, -448, -2753 }, { -3022, 485, -2583 } },
    { { -2769, 0, 3018, -334, 4070, -307, -2999, -454, -2752 }, { -3032, 471, -2590 } },
    { { -2768, 0, 3019, -339, 4070, -311, -2999, -460, -2750 }, { -3042, 456, -2596 } },
    { { -2767, 0, 3019, -343, 4069, -314, -3000, -466, -2749 }, { -3052, 442, -2602 } },
    { { -2766, 0, 3020, -348, 4068, -318, -3000, -471, -2747 }, { -3062, 428, -2609 } },
    { { -2765, 0, 3021, -352, 4068, -322, -3001, -477, -2746 }, { -3071, 414, -2615 } },
    { { -2764, 0, 3022, -356, 4067, -326, -3001, -483, -2744 }, { -3081, 400, -2621 } },
    { { -2763, 0, 3023, -360, 4066, -329, -3001, -488, -2743 }, { -3091, 386, -2628 } },
    { { -2762, 0, 3024, -365, 4066, -333, -3002, -494, -2742 }, { -3101, 372, -2634 } },
    { { -2761, 0, 3025, -369, 4065, -337, -3002, -499, -2740 }, { -3111, 358, -2641 } },
    { { -2760, 0, 3025, -373, 4064, -340, -3002, -505, -2739 }, { -3121, 344, -2647 } },
    { { -2759, 0, 3026, -377, 4064, -344, -3003, -510, -2738 }, { -3131, 331, -2654 } },
    { { -2758, 0, 3027, -381, 4063, -347, -3003, -515, -2736 }, { -3141, 317, -2661 } },
    { { -2758, 0, 3028, -385, 4062, -350, -3003, -521, -2735 }, { -3151, 303, -2667 } },
    { { -2757, 0, 3029, -389, 4062, -354, -3003, -526, -2734 }, { -3161, 290, -2674 } },
    { { -2756, 0, 3029, -392, 4061, -357, -3004, -531, -2733 }, { -3171, 276, -2680 } },
    { { -2755, 0, 3030, -396, 4060, -360, -3004, -536, -2731 }, { -3180, 263, -2687 } },
    { { -2754, 0, 3031, -400, 4060, -363, -3004, -540, -2730 }, { -3190, 249, -2694 } },
    { { -2753, 0, 3032, -404, 4059, -366, -3004, -545, -2729 }, { -3200, 236, -2700 } },
    { { -2753, 0, 3032, -407, 4058, -370, -3005, -550, -2728 }, { -3210, 222, -2707 } },
    { { -2752, 0, 3033, -411, 4058, -373, -3005, -555, -2726 }, { -3220, 209, -2714 } },
    { { -2751, 0, 3034, -414, 4057, -376, -3005, -559, -2725 }, { -3230, 196, -2720 } },
    { { -2750, 0, 3034, -418, 4056, -379, -3005, -564, -2724 }, { -3240, 182, -2727 } },
    { { -2750, 0, 3035, -421, 4056, -382, -3006, -569, -2723 }, { -3249, 169, -2734 } },
    { { -2749, 0, 3036, -425, 4055, -384, -3006, -573, -2722 }, { -3259, 156, -2740 } },
    { { -2748, 0, 3036, -428, 4055, -387, -3006, -577, -2721 }, { -3269, 143, -2747 } },
    { { -2747, 0, 3037, -431, 4054, -390, -3006, -582, -2719 }, { -3279, 130, -2754 } },
    { { -2747, 0, 3038, -435, 4053, -393, -3006, -586, -2718 }, { -3289, 117, -2761 } },
    { { -2746, 0, 3038, -438, 4053, -396, -3007, -590, -2717 }, { -3299, 103, -2768 } },
    { { -2745, 0, 3039, -441, 4052, -398, -3007, -595, -2716 }, { -3308, 91, -2774 } },
    { { -2745, 0, 3040, -444, 4051, -401, -3007, -599, -2715 }, { -3318, 78, -2781 } },
    { { -2744, 0, 3040, -447, 4051, -404, -3007, -603, -2714 }, { -3328, 65, -2788 } },
    { { -2743, 0, 3041, -450, 4050, -406, -3007, -607, -2713 }, { -3338, 52, -2795 } },
    { { -2743, 0, 3041, -453, 4050, -409, -3007, -611, -2712 }, { -3348, 39, -2802 } },
    { { -2742, 0, 3042, -457, 4049, -411, -3007, -615, -2711 }, { -3357, 26, -2809 } },
    { { -2741, 0, 3043, -459, 4048, -414, -3008, -619, -2710 }, { -3367, 13, -2815 } },
    { { -2741, 0, 3043, -462, 4048, -416, -3008, -622, -2709 }, { -3377, 1, -2822 } },
    { { -2740, 0, 3044, -465, 4047, -419, -3008, -626, -2708 }, { -3387, -11, -2829 } },
    { { -2739, 0, 3044, -468, 4047, -421, -3008, -630, -2707 }, { -3396, -24, -2836 } },
    { { -2739, 0, 3045, -471, 4046, -424, -3008, -634, -2706 }, { -3406, -36, -2843 } },
    { { -2738, 0, 3045, -474, 4046, -426, -3008, -637, -2705 }, { -3416, -49, -2850 } },
    { { -2737, 0, 3046, -477, 4045, -428, -3008, -641, -2704 }, { -3426, -62, -2857 } },
    { { -2737, 0, 3047, -479, 4044, -430, -3009, -644, -2703 }, { -3435, -74, -2864 } },
    { { -2736, 0, 3047, -482, 4044, -433, -3009, -648, -2702 }, { -3445, -87, -2871 } },
    { { -2736, 0, 3048, -485, 4043, -435, -3009, -651, -2701 }, { -3455, -99, -2877 } },
    { { -2735, 0, 3048, -487, 4043, -437, -3009, -655, -2700 }, { -3465, -112, -2884 } },
    { { -2734, 0, 3049, -490, 4042, -439, -3009, -658, -2699 }, { -3474, -124, -2891 } },
    { { -2734, 0, 3049, -492, 4042, -441, -3009, -662, -2698 }, { -3484, -136, -2898 } },
    { { -2733, 0, 3050, -495, 4041, -444, -3009, -665, -2697 }, { -3494, -149, -2905 } },
    { { -2733, 0, 3050, -497, 4041, -446, -3009, -668, -2696 }, { -3503, -161, -2912 } },
    { { -2732, 0, 3051, -500, 4040, -448, -3009, -671, -2695 }, { -3513, -173, -2919 } },
    { { -2732, 0, 3051, -502, 4040, -450, -3009, -674, -2694 }, { -3523, -186, -2926 } },
    { { -2731, 0, 3052, -505, 4039, -452, -3010, -678, -2693 }, { -3532, -198, -2933 } },
    { { -2730, 0, 3052, -507, 4038, -454, -3010, -681, -2692 }, { -3542, -210, -2940 } },
    { { -2730, 0, 3053, -509, 4038, -456, -3010, -684, -2692 }, { -3551, -222, -2947 } },
    { { -2729, 0, 3053, -512, 4037, -458, -3010, -687, -2691 }, { -3561, -234, -2954 } },
    { { -2729, 0, 3054, -514, 4037, -459, -3010, -690, -2690 }, { -3571, -246, -2961 } },
    { { -2728, 0, 3054, -516, 4036, -461, -3010, -693, -2689 }, { -3580, -259, -2968 } },
    { { -2728, 0, 3055, -519, 4036, -463, -3010, -695, -2688 }, { -3590, -271, -2975 } },
    { { -2727, 0, 3055, -521, 4035, -465, -3010, -698, -2687 }, { -3599, -283, -2982 } },
    { { -2727, 0, 3055, -523, 4035, -467, -3010, -701, -2687 }, { -3609, -295, -2989 } },
    { { -2726, 0, 3056, -525, 4034, -468, -3010, -704, -2686 }, { -3619, -307, -2996 } },
    { { -2726, 0, 3056, -527, 4034, -470, -3010, -707, -2685 }, { -3628, -319, -3003 } },
    { { -2725, 0, 3057, -529, 4034, -472, -3011, -709, -2684 }, { -3638, -330, -3010 } },
    { { -2725, 0, 3057, -532, 4033, -474, -3011, -712, -2683 }, { -3647, -342, -3017 } },
    { { -2724, 0, 3058, -534, 4033, -475, -3011, -715, -2682 }, { -3657, -354, -3024 } },
    { { -2724, 0, 3058, -536, 4032, -477, -3011, -717, -2682 }, { -3666, -366, -3031 } },
    { { -2723, 0, 3059, -538, 4032, -479, -3011, -720, -2681 }, { -3676, -378, -3038 } },
    { { -2723, 0, 3059, -540, 4031, -480, -3011, -723, -2680 }, { -3685, -390, -3045 } },
    { { -2722, 0, 3059, -542, 4031, -482, -3011, -725, -2679 }, { -3695, -402, -3052 } },
    { { -2722, 0, 3060, -544, 4030, -483, -3011, -728, -2679 }, { -3704, -413, -3059 } },
    { { -2722, 0, 3060, -545, 4030, -485, -3011, -730, -2678 }, { -3714, -425, -3066 } },
    { { -2721, 0, 3061, -547, 4029, -487, -3011, -733, -2677 }, { -3723, -437, -3073 } },
    { { -2721, 0, 3061, -549, 4029, -488, -3011, -735, -2676 }, { -3733, -448, -3080 } },
    { { -2720, 0, 3061, -551, 4028, -490, -3011, -737, -2676 }, { -3742, -460, -3087 } },
    { { -2720, 0, 3062, -553, 4028, -491, -3011, -740, -2675 }, { -3752, -472, -3094 } },
    { { -2719, 0, 3062, -555, 4028, -493, -3011, -742, -2674 }, { -3761, -483, -3101 } },
    { { -2719, 0, 3063, -556, 4027, -494, -3012, -744, -2673 }, { -3771, -495, -3108 } },
    { { -2718, 0, 3063, -558, 4027, -495, -3012, -747, -2673 }, { -3780, -506, -3115 } },
    { { -2718, 0, 3063, -560, 4026, -497, -3012, -749, -2672 }, { -3790, -518, -3122 } },
    { { -2718, 0, 3064, -562, 4026, -498, -3012, -751, -2671 }, { -3799, -530, -3129 } },
    { { -2717, 0, 3064, -563, 4026, -500, -3012, -753, -2671 }, { -3809, -541, -3136 } },
    { { -2717, 0, 3064, -565, 4025, -501, -3012, -755, -2670 }, { -3818, -553, -3143 } },
    { { -2716, 0, 3065, -567, 4025, -502, -3012, -757, -2669 }, { -3827, -564, -3150 } },
    { { -2716, 0, 3065, -568, 4024, -504, -3012, -760, -2669 }, { -3837, -575, -3157 } },
    { { -2715, 0, 3066, -570, 4024, -505, -3012, -762, -2668 }, { -3846, -587, -3164 } },
    { { -2715, 0, 3066, -572, 4024, -506, -3012, -764, -2667 }, { -3856, -598, -3171 } },
    { { -2715, 0, 3066, -573, 4023, -507, -3012, -766, -2667 }, { -3865, -610, -3178 } },
    { { -2714, 0, 3067, -575, 4023, -509, -3012, -768, -2666 }, { -3874, -621, -3185 } },
    { { -2714, 0, 3067, -576, 4022, -510, -3012, -770, -2665 }, { -3884, -632, -3192 } },
    { { -2713, 0, 3067, -578, 4022, -511, -3012, -772, -2665 }, { -3893, -644, -3199 } },
    { { -2713, 0, 3068, -579, 4022, -512, -3012, -774, -2664 }, { -3902, -655, -3206 } },
    { { -2713, 0, 3068, -581, 4021, -514, -3013, -776, -2663 }, { -3912, -666, -3213 } },
    { { -2712, 0, 3068, -582, 4021, -515, -3013, -778, -2663 }, { -3921, -678, -3220 } },
    { { -2712, 0, 3069, -584, 4021, -516, -3013, -779, -2662 }, { -3930, -689, -3227 } },
    { { -2711, 0, 3069, -585, 4020, -517, -3013, -781, -2662 }, { -3940, -700, -3234 } },
    { { -2711, 0, 3069, -587, 4020, -518, -3013, -783, -2661 }, { -3949, -711, -3241 } },
    { { -2711, 0, 3070, -588, 4019, -519, -3013, -785, -2660 }, { -3958, -722, -3248 } },
    { { -2710, 0, 3070, -590, 4019, -520, -3013, -787, -2660 }, { -3968, -734, -3255 } },
    { { -2710, 0, 3070, -591, 4019, -522, -3013, -788, -2659 }, { -3977, -745, -3262 } },
    { { -2710, 0, 3071, -592, 4018, -523, -3013, -790, -2659 }, { -3986, -756, -3269 } },
    { { -2709, 0, 3071, -594, 4018, -524, -3013, -792, -2658 }, { -3996, -767, -3276 } },
    { { -2709, 0, 3071, -595, 4018, -525, -3013, -794, -2657 }, { -4005, -778, -3283 } },
    { { -2708, 0, 3072, -596, 4017, -526, -3013, -795, -2657 }, { -4014, -789, -3291 } },
    { { -2708, 0, 3072, -598, 4017, -527, -3013, -797, -2656 }, { -4024, -800, -3298 } },
    { { -2708, 0, 3072, -599, 4017, -528, -3013, -799, -2656 }, { -4033, -812, -3305 } },
    { { -2707, 0, 3073, -600, 4016, -529, -3013, -800, -2655 }, { -4042, -823, -3312 } },
    { { -2707, 0, 3073, -602, 4016, -530, -3013, -802, -2655 }, { -4051, -834, -3319 } },
    { { -2707, 0, 3073, -603, 4016, -531, -3014, -804, -2654 }, { -4061, -845, -3326 } },
    { { -2706, 0, 3074, -604, 4015, -532, -3014, -805, -2653 }, { -4070, -856, -3333 } },
    { { -2706, 0, 3074, -605, 4015, -533, -3014, -807, -2653 }, { -4079, -867, -3340 } },
    { { -2706, 0, 3074, -607, 4015, -534, -3014, -808, -2652 }, { -4088, -878, -3347 } },
    { { -2705, 0, 3075, -608, 4015, -535, -3014, -810, -2652 }, { -4098, -889, -3354 } },
    { { -2705, 0, 3075, -609, 4014, -536, -3014, -811, -2651 }, { -4107, -900, -3361 } },
    { { -2705, 0, 3075, -610, 4014, -537, -3014, -813, -2651 }, { -4116, -910, -3368 } },
    { { -2704, 0, 3076, -611, 4014, -538, -3014, -814, -2650 }, { -4125, -921, -3375 } },
    { { -2704, 0, 3076, -613, 4013, -538, -3014, -816, -2650 }, { -4134, -932, -3382 } },
    { { -2704, 0, 3076, -614, 4013, -539, -3014, -817, -2649 }, { -4144, -943, -3389 } },
    { { -2703, 0, 3076, -615, 4013, -540, -3014, -819, -2649 }, { -4153, -954, -3396 } },
    { { -2703, 0, 3077, -616, 4012, -541, -3014, -820, -2648 }, { -4162, -965, -3403 } },
    { { -2703, 0, 3077, -617, 4012, -542, -3014, -821, -2648 }, { -4171, -976, -3410 } },
    { { -2702, 0, 3077, -618, 4012, -543, -3014, -823, -2647 }, { -4180, -987, -3417 } },
    { { -2702, 0, 3078, -619, 4012, -544, -3015, -824, -2647 }, { -4190, -997, -3424 } },
    { { -2702, 0, 3078, -620, 4011, -544, -3015, -826, -2646 }, { -4199, -1008, -3431 } },
    { { -2701, 0, 3078, -621, 4011, -545, -3015, -827, -2645 }, { -4208, -1019, -3438 } },
    { { -2701, 0, 3078, -623, 4011, -546, -3015, -828, -2645 }, { -4217, -1030, -3445 } },
    { { -2701, 0, 3079, -624, 4010, -547, -3015, -830, -2644 }, { -4226, -1041, -3452 } },
    { { -2700, 0, 3079, -625, 4010, -548, -3015, -831, -2644 }, { -4236, -1051, -3459 } },
    { { -2700, 0, 3079, -626, 4010, -549, -3015, -832, -2643 }, { -4245, -1062, -3466 } },
    { { -2700, 0, 3080, -627, 4010, -549, -3015, -834, -2643 }, { -4254, -1073, -3473 } },
    { { -2699, 0, 3080, -628, 4009, -550, -3015, -835, -2643 }, { -4263, -1084, -3480 } },
    { { -2699, 0, 3080, -629, 4009, -551, -3015, -836, -2642 }, { -4272, -1094, -3487 } },
    { { -2699, 0, 3080, -630, 4009, -552, -3015, -837, -2642 }, { -4281, -1105, -3494 } },
    { { -2698, 0, 3081, -631, 4009, -552, -3015, -839, -2641 }, { -4291, -1116, -3501 } },
    { { -2698, 0, 3081, -632, 4008, -553, -3015, -840, -2641 }, { -4300, -1127, -3508 } },
    { { -2698, 0, 3081, -633, 4008, -554, -3015, -841, -2640 }, { -4309, -1137, -3515 } },
    { { -2697, 0, 3081, -634, 4008, -555, -3016, -842, -2640 }, { -4318, -1148, -3522 } },
    { { -2697, 0, 3082, -635, 4008, -555, -3016, -843, -2639 }, { -4327, -1159, -3529 } },
    { { -2697, 0, 3082, -636, 4007, -556, -3016, -845, -2639 }, { -4336, -1169, -3536 } },
    { { -2696, 0, 3082, -637, 4007, -557, -3016, -846, -2638 }, { -4345, -1180, -3543 } },
    { { -2696, 0, 3083, -637, 4007, -557, -3016, -847, -2638 }, { -4354, -1191, -3550 } },
    { { -2696, 0, 3083, -638, 4007, -558, -3016, -848, -2637 }, { -4364, -1201, -3557 } },
    { { -2696, 0, 3083, -639, 4006, -559, -3016, -849, -2637 }, { -4373, -1212, -3564 } },
    { { -2695, 0, 3083, -640, 4006, -560, -3016, -850, -2636 }, { -4382, -1222, -3571 } },
    { { -2695, 0, 3084, -641, 4006, -560, -3016, -852, -2636 }, { -4391, -1233, -3578 } },
    { { -2695, 0, 3084, -642, 4006, -561, -3016, -853, -2635 }, { -4400, -1244, -3585 } },
    { { -2694, 0, 3084, -643, 4005, -562, -3016, -854, -2635 }, { -4409, -1254, -3592 } },
    { { -2694, 0, 3084, -644, 4005, -562, -3016, -855, -2635 }, { -4418, -1265, -3599 } },
    { { -2694, 0, 3085, -645, 4005, -563, -3016, -856, -2634 }, { -4427, -1275, -3606 } },
    { { -2693, 0, 3085, -646, 4005, -564, -3017, -857, -2634 }, { -4437, -1286, -3613 } },
    { { -2693, 0, 3085, -646, 4004, -564, -3017, -858, -2633 }, { -4446, -1297, -3620 } },
    { { -2693, 0, 3085, -647, 4004, -565, -3017, -859, -2633 }, { -4455, -1307, -3627 } },
    { { -2693, 0, 3086, -648, 4004, -565, -3017, -860, -2632 }, { -4464, -1318, -3634 } },
    { { -2692, 0, 3086, -649, 4004, -566, -3017, -861, -2632 }, { -4473, -1328, -3641 } },
    { { -2692, 0, 3086, -650, 4004, -567, -3017, -862, -2631 }, { -4482, -1339, -3648 } },
    { { -2692, 0, 3087, -651, 4003, -567, -3017, -864, -2631 }, { -4491, -1349, -3655 } },
    { { -2691, 0, 3087, -652, 4003, -568, -3017, -865, -2631 }, { -4500, -1360, -3662 } },
    { { -2691, 0, 3087, -652, 4003, -569, -3017, -866, -2630 }, { -4509, -1370, -3669 } },
    { { -2691, 0, 3087, -653, 4003, -569, -3017, -867, -2630 }, { -4518, -1381, -3676 } },
    { { -2690, 0, 3088, -654, 4002, -570, -3017, -868, -2629 }, { -4527, -1391, -3683 } },
    { { -2690, 0, 3088, -655, 4002, -570, -3017, -869, -2629 }, { -4537, -1402, -3690 } },
    { { -2690, 0, 3088, -656, 4002, -571, -3018, -870, -2628 }, { -4546, -1412, -3697 } },
    { { -2690, 0, 3088, -656, 4002, -572, -3018, -871, -2628 }, { -4555, -1423, -3704 } },
    { { -2689, 0, 3089, -657, 4002, -572, -3018, -872, -2628 }, { -4564, -1433, -3711 } },
    { { -2689, 0, 3089, -658, 4001, -573, -3018, -873, -2627 }, { -4573, -1444, -3718 } },
    { { -2689, 0, 3089, -659, 4001, -573, -3018, -874, -2627 }, { -4582, -1454, -3725 } },
    { { -2688, 0, 3089, -660, 4001, -574, -3018, -875, -2626 }, { -4591, -1465, -3732 } },
    { { -2688, 0, 3090, -660, 4001, -575, -3018, -876, -2626 }, { -4600, -1475, -3739 } },
    { { -2688, 0, 3090, -661, 4001, -575, -3018, -876, -2625 }, { -4609, -1486, -3746 } },
    { { -2688, 0, 3090, -662, 4000, -576, -3018, -877, -2625 }, { -4618, -1496, -3753 } },
    { { -2687, 0, 3090, -663, 4000, -576, -3018, -878, -2625 }, { -4627, -1507, -3760 } },
    { { -2687, 0, 3091, -663, 4000, -577, -3018, -879, -2624 }, { -4637, -1517, -3767 } },
    { { -2687, 0, 3091, -664, 4000, -577, -3018, -880, -2624 }, { -4646, -1527, -3774 } },
    { { -2686, 0, 3091, -665, 3999, -578, -3019, -881, -2623 }, { -4655, -1538, -3781 } },
    { { -2686, 0, 3091, -666, 3999, -578, -3019, -882, -2623 }, { -4664, -1548, -3788 } },
    { { -2686, 0, 3092, -667, 3999, -579, -3019, -883, -2623 }, { -4673, -1559, -3795 } },
    { { -2686, 0, 3092, -667, 3999, -580, -3019, -884, -2622 }, { -4682, -1569, -3802 } },
    { { -2685, 0, 3092, -668, 3999, -580, -3019, -885, -2622 }, { -4691, -1580, -3809 } },
    { { -2685, 0, 3092, -669, 3998, -581, -3019, -886, -2621 }, { -4700, -1590, -3816 } },
    { { -2685, 0, 3093, -670, 3998, -581, -3019, -887, -2621 }, { -4709, -1601, -3823 } },
    { { -2684, 0, 3093, -670, 3998, -582, -3019, -888, -2620 }, { -4718, -1611, -3830 } },
    { { -2684, 0, 3093, -671, 3998, -582, -3019, -889, -2620 }, { -4727, -1621, -3837 } },
    { { -2684, 0, 3093, -672, 3998, -583, -3019, -890, -2620 }, { -4736, -1632, -3844 } },
    { { -2684, 0, 3094, -673, 3997, -583, -3019, -891, -2619 }, { -4746, -1642, -3851 } },
    { { -2683, 0, 3094, -673, 3997, -584, -3020, -891, -2619 }, { -4755, -1653, -3858 } },
    { { -2683, 0, 3094, -674, 3997, -584, -3020, -892, -2618 }, { -4764, -1663, -3865 } },
    { { -2683, 0, 3094, -675, 3997, -585, -3020, -893, -2618 }, { -4773, -1674, -3872 } },
    { { -2682, 0, 3095, -676, 3997, -586, -3020, -894, -2618 }, { -4782, -1684, -3879 } },
    { { -2682, 0, 3095, -676, 3996, -586, -3020, -895, -2617 }, { -4791, -1694, -3886 } },
    { { -2682, 0, 3095, -677, 3996, -587, -3020, -896, -2617 }, { -4800, -1705, -3893 } },
    { { -2681, 0, 3095, -678, 3996, -587, -3020, -897, -2616 }, { -4809, -1715, -3900 } },
    { { -2681, 0, 3096, -679, 3996, -588, -3020, -898, -2616 }, { -4818, -1726, -3907 } },
    { { -2681, 0, 3096, -679, 3996, -588, -3020, -899, -2615 }, { -4827, -1736, -3914 } },
    { { -2681, 0, 3096, -680, 3995, -589, -3020, -900, -2615 }, { -4837, -1747, -3921 } },
    { { -2680, 0, 3096, -681, 3995, -589, -3021, -900, -2615 }, { -4846, -1757, -3928 } },
    { { -2680, 0, 3097, -681, 3995, -590, -3021, -901, -2614 }, { -4855, -1767, -3935 } },
    { { -2680, 0, 3097, -682, 3995, -590, -3021, -902, -2614 }, { -4864, -1778, -3942 } },
    { { -2679, 0, 3097, -683, 3995, -591, -3021, -903, -2613 }, { -4873, -1788, -3949 } },
    { { -2679, 0, 3097, -684, 3994, -591, -3021, -904, -2613 }, { -4882, -1799, -3956 } },
    { { -2679, 0, 3098, -684, 3994, -592, -3021, -905, -2613 }, { -4891, -1809, -3963 } },
    { { -2679, 0, 3098, -685, 3994, -592, -3021, -906, -2612 }, { -4900, -1820, -3970 } },
    { { -2678, 0, 3098, -686, 3994, -593, -3021, -907, -2612 }, { -4909, -1830, -3977 } },
    { { -2678, 0, 3098, -687, 3994, -593, -3021, -908, -2611 }, { -4919, -1841, -3984 } },
    { { -2678, 0, 3099, -687, 3993, -594, -3021, -909, -2611 }, { -4928, -1851, -3991 } },
    { { -2677, 0, 3099, -688, 3993, -594, -3021, -910, -2610 }, { -4937, -1861, -3998 } },
    { { -2677, 0, 3099, -689, 3993, -595, -3022, -910, -2610 }, { -4946, -1872, -4005 } },
    { { -2677, 0, 3099, -690, 3993, -595, -3022, -911, -2610 }, { -4955, -1882, -4012 } },
    { { -2676, 0, 3100, -690, 3993, -596, -3022, -912, -2609 }, { -4964, -1893, -4019 } },
    { { -2676, 0, 3100, -691, 3992, -597, -3022, -913, -2609 }, { -4973, -1903, -4026 } },
    { { -2676, 0, 3100, -692, 3992, -597, -3022, -914, -2608 }, { -4982, -1914, -4033 } },
    { { -2675, 0, 3101, -693, 3992, -598, -3022, -915, -2608 }, { -4992, -1924, -4040 } },
    { { -2675, 0, 3101, -693, 3992, -598, -3022, -916, -2607 }, { -5001, -1935, -4047 } },
    { { -2675, 0, 3101, -694, 3991, -599, -3022, -917, -2607 }, { -5010, -1945, -4054 } },
    { { -2675, 0, 3101, -695, 3991, -599, -3022, -918, -2606 }, { -5019, -1956, -4061 } },
    { { -2674, 0, 3102, -696, 3991, -600, -3023, -919, -2606 }, { -5028, -1966, -4068 } },
    { { -2674, 0, 3102, -696, 3991, -600, -3023, -920, -2606 }, { -5037, -1977, -4075 } },
    { { -2674, 0, 3102, -697, 3991, -601, -3023, -921, -2605 }, { -5047, -1987, -4082 } },
    { { -2673, 0, 3102, -698, 3990, -601, -3023, -921, -2605 }, { -5056, -1998, -4089 } },
    { { -2673, 0, 3103, -699, 3990, -602, -3023, -922, -2604 }, { -5065, -2009, -4096 } },
    { { -2673, 0, 3103, -699, 3990, -602, -3023, -923, -2604 }, { -5074, -2019, -4103 } },
    { { -2672, 0, 3103, -700, 3990, -603, -3023, -924, -2603 }, { -5083, -2030, -4110 } },
    { { -2672, 0, 3104, -701, 3990, -603, -3023, -925, -2603 }, { -5092, -2040, -4117 } },
    { { -2672, 0, 3104, -702, 3989, -604, -3023, -926, -2602 }, { -5102, -2051, -4123 } },
    { { -2671, 0, 3104, -703, 3989, -605, -3024, -927, -2602 }, { -5111, -2061, -4130 } },
    { { -2671, 0, 3104, -703, 3989, -605, -3024, -928, -2601 }, { -5120, -2072, -4137 } },
    { { -2671, 0, 3105, -704, 3989, -606, -3024, -929, -2601 }, { -5129, -2083, -4144 } },
    { { -2670, 0, 3105, -705, 3988, -606, -3024, -930, -2600 }, { -5138, -2093, -4151 } },
    { { -2670, 0, 3105, -706, 3988, -607, -3024, -931, -2600 }, { -5148, -2104, -4158 } },
    { { -2669, 0, 3106, -707, 3988, -607, -3024, -932, -2599 }, { -5157, -2115, -4165 } },
    { { -2669, 0, 3106, -708, 3988, -608, -3024, -933, -2599 }, { -5166, -2125, -4172 } },
    { { -2669, 0, 3106, -708, 3987, -609, -3024, -934, -2598 }, { -5175, -2136, -4179 } },
    { { -2668, 0, 3107, -709, 3987, -609, -3025, -935, -2598 }, { -5185, -2147, -4185 } },
    { { -2668, 0, 3107, -710, 3987, -610, -3025, -936, -2597 }, { -5194, -2157, -4192 } },
    { { -2668, 0, 3107, -711, 3987, -610, -3025, -937, -2597 }, { -5203, -2168, -4199 } },
    { { -2667, 0, 3108, -712, 3986, -611, -3025, -938, -2596 }, { -5212, -2179, -4206 } },
    { { -2667, 0, 3108, -713, 3986, -611, -3025, -939, -2596 }, { -5222, -2190, -4213 } },
    { { -2666, 0, 3108, -714, 3986, -612, -3025, -940, -2595 }, { -5231, -2200, -4220 } },
    { { -2666, 0, 3109, -714, 3986, -613, -3025, -941, -2595 }, { -5240, -2211, -4226 } },
    { { -2666, 0, 3109, -715, 3985, -613, -3025, -942, -2594 }, { -5249, -2222, -4233 } },
    { { -2665, 0, 3109, -716, 3985, -614, -3026, -944, -2593 }, { -5259, -2233, -4240 } },
    { { -2665, 0, 3110, -717, 3985, -615, -3026, -945, -2593 }, { -5268, -2244, -4247 } },
    { { -2664, 0, 3110, -718, 3985, -615, -3026, -946, -2592 }, { -5277, -2255, -4253 } },
    { { -2664, 0, 3110, -719, 3984, -616, -3026, -947, -2592 }, { -5286, -2266, -4260 } },
    { { -2664, 0, 3111, -720, 3984, -616, -3026, -948, -2591 }, { -5296, -2277, -4267 } },
    { { -2663, 0, 3111, -721, 3984, -617, -3026, -949, -2590 }, { -5305, -2288, -4273 } },
    { { -2663, 0, 3112, -722, 3984, -618, -3027, -950, -2590 }, { -5314, -2299, -4280 } },
    { { -2662, 0, 3112, -723, 3983, -618, -3027, -952, -2589 }, { -5324, -2310, -4287 } },
    { { -2662, 0, 3112, -724, 3983, -619, -3027, -953, -2589 }, { -5333, -2321, -4293 } },
    { { -2661, 0, 3113, -725, 3983, -620, -3027, -954, -2588 }, { -5342, -2332, -4300 } },
    { { -2661, 0, 3113, -726, 3982, -620, -3027, -955, -2587 }, { -5351, -2343, -4306 } },
    { { -2660, 0, 3114, -727, 3982, -621, -3028, -956, -2586 }, { -5361, -2354, -4313 } },
    { { -2660, 0, 3114, -728, 3982, -622, -3028, -958, -2586 }, { -5370, -2365, -4319 } },
    { { -2659, 0, 3115, -729, 3982, -623, -3028, -959, -2585 }, { -5379, -2377, -4325 } },
    { { -2658, 0, 3115, -730, 3981, -623, -3028, -960, -2584 }, { -5389, -2388, -4332 } },
    { { -2658, 0, 3116, -732, 3981, -624, -3028, -962, -2583 }, { -5398, -2399, -4338 } },
    { { -2657, 0, 3116, -733, 3981, -625, -3029, -963, -2583 }, { -5407, -2411, -4344 } },
    { { -2657, 0, 3117, -734, 3980, -626, -3029, -965, -2582 }, { -5416, -2422, -4350 } },
    { { -2656, 0, 3117, -735, 3980, -626, -3029, -966, -2581 }, { -5426, -2434, -4356 } },
    { { -2655, 0, 3118, -736, 3979, -627, -3030, -968, -2580 }, { -5435, -2446, -4362 } },
    { { -2655, 0, 3118, -738, 3979, -628, -3030, -969, -2579 }, { -5444, -2457, -4368 } },
    { { -2654, 0, 3119, -739, 3979, -629, -3030, -971, -2578 }, { -5453, -2469, -4373 } },
    { { -2653, 0, 3120, -741, 3978, -630, -3031, -972, -2577 }, { -5463, -2481, -4379 } },
    { { -2652, 0, 3121, -742, 3978, -631, -3031, -974, -2576 }, { -5472, -2493, -4384 } },
    { { -2651, 0, 3121, -744, 3977, -632, -3031, -976, -2575 }, { -5481, -2506, -4389 } },
    { { -2650, 0, 3122, -745, 3977, -633, -3032, -978, -2573 }, { -5490, -2518, -4393 } },
    { { -2649, 0, 3123, -747, 3976, -634, -3032, -980, -2572 }, { -5499, -2531, -4398 } },
    { { -2648, 0, 3124, -749, 3976, -635, -3033, -982, -2570 }, { -5508, -2544, -4402 } },
    { { -2646, 0, 3125, -751, 3975, -636, -3034, -984, -2569 }, { -5518, -2557, -4406 } },
    { { -2645, 0, 3127, -753, 3975, -637, -3035, -986, -2567 }, { -5527, -2571, -4410 } },
    { { -2643, 0, 3128, -755, 3974, -638, -3036, -989, -2565 }, { -5536, -2585, -4414 } },
    { { -2641, 0, 3130, -758, 3974, -639, -3037, -992, -2562 }, { -5546, -2599, -4416 } },
    { { -2639, 0, 3132, -760, 3973, -641, -3038, -995, -2560 }, { -5555, -2613, -4418 } },
    { { -2635, 0, 3135, -764, 3972, -642, -3040, -998, -2556 }, { -5564, -2629, -4419 } },
    { { -2631, 0, 3138, -769, 3970, -645, -3042, -1004, -2551 }, { -5573, -2644, -4417 } },
    { { -2627, 0, 3142, -776, 3969, -649, -3045, -1011, -2545 }, { -5578, -2659, -4409 } },
    { { -2622, 0, 3146, -781, 3967, -651, -3047, -1017, -2539 }, { -5577, -2666, -4393 } },
    { { -2618, 0, 3149, -785, 3966, -652, -3050, -1021, -2535 }, { -5570, -2665, -4376 } },
    { { -2615, 0, 3152, -787, 3966, -653, -3052, -1023, -2532 }, { -5562, -2661, -4361 } },
    { { -2612, 0, 3154, -789, 3965, -653, -3054, -1024, -2529 }, { -5553, -2654, -4346 } },
    { { -2610, 0, 3156, -790, 3965, -653, -3055, -1025, -2527 }, { -5543, -2647, -4332 } },
    { { -2608, 0, 3158, -791, 3965, -653, -3057, -1026, -2524 }, { -5533, -2639, -4318 } },
    { { -2606, 0, 3159, -792, 3965, -653, -3059, -1027, -2522 }, { -5523, -2630, -4305 } },
    { { -2604, 0, 3161, -793, 3965, -653, -3060, -1027, -2520 }, { -5513, -2621, -4291 } },
    { { -2602, 0, 3163, -793, 3964, -653, -3061, -1027, -2519 }, { -5503, -2612, -4278 } },
    { { -2600, 0, 3164, -794, 3964, -652, -3063, -1028, -2517 }, { -5492, -2602, -4266 } },
    { { -2598, 0, 3165, -794, 3964, -652, -3064, -1028, -2515 }, { -5482, -2592, -4253 } },
    { { -2597, 0, 3167, -795, 3964, -652, -3065, -1028, -2514 }, { -5471, -2582, -4240 } },
    { { -2595, 0, 3168, -795, 3964, -651, -3066, -1028, -2512 }, { -5460, -2572, -4228 } },
    { { -2594, 0, 3169, -796, 3964, -651, -3068, -1028, -2511 }, { -5450, -2561, -4215 } },
    { { -2592, 0, 3170, -796, 3964, -651, -3069, -1028, -2509 }, { -5439, -2550, -4203 } },
    { { -2591, 0, 3172, -796, 3964, -650, -3070, -1028, -2508 }, { -5428, -2540, -4191 } },
    { { -2590, 0, 3173, -796, 3964, -650, -3071, -1028, -2507 }, { -5417, -2529, -4179 } },
    { { -2588, 0, 3174, -796, 3964, -649, -3072, -1028, -2505 }, { -5406, -2517, -4166 } },
    { { -2587, 0, 3175, -797, 3964, -649, -3073, -1028, -2504 }, { -5395, -2506, -4154 } },
    { { -2585, 0, 3176, -797, 3964, -648, -3074, -1027, -2503 }, { -5384, -2495, -4142 } },
    { { -2584, 0, 3177, -797, 3964, -648, -3075, -1027, -2502 }, { -5372, -2483, -4130 } },
    { { -2583, 0, 3178, -797, 3965, -648, -3076, -1027, -2500 }, { -5361, -2472, -4118 } },
    { { -2582, 0, 3179, -797, 3965, -647, -3077, -1027, -2499 }, { -5350, -2460, -4106 } },
    { { -2580, 0, 3180, -797, 3965, -647, -3078, -1026, -2498 }, { -5339, -2449, -4095 } },
    { { -2579, 0, 3181, -797, 3965, -646, -3080, -1026, -2497 }, { -5328, -2437, -4083 } },
    { { -2578, 0, 3182, -797, 3965, -645, -3081, -1026, -2496 }, { -5316, -2425, -4071 } },
    { { -2577, 0, 3183, -797, 3965, -645, -3081, -1025, -2495 }, { -5305, -2413, -4059 } },
    { { -2576, 0, 3184, -797, 3965, -644, -3082, -1025, -2494 }, { -5294, -2401, -4048 } },
    { { -2575, 0, 3185, -797, 3965, -644, -3083, -1024, -2493 }, { -5282, -2389, -4036 } },
    { { -2574, 0, 3186, -796, 3965, -643, -3084, -1024, -2492 }, { -5271, -2377, -4024 } },
    { { -2572, 0, 3187, -796, 3965, -643, -3085, -1024, -2491 }, { -5260, -2365, -4013 } },
    { { -2571, 0, 3187, -796, 3966, -642, -3086, -1023, -2490 }, { -5248, -2353, -4001 } },
    { { -2570, 0, 3188, -796, 3966, -642, -3087, -1023, -2489 }, { -5237, -2341, -3990 } },
    { { -2569, 0, 3189, -796, 3966, -641, -3088, -1022, -2488 }, { -5226, -2329, -3979 } },
    { { -2568, 0, 3190, -796, 3966, -640, -3089, -1022, -2487 }, { -5214, -2317, -3967 } },
    { { -2567, 0, 3191, -795, 3966, -640, -3090, -1021, -2486 }, { -5203, -2304, -3956 } },
    { { -2566, 0, 3192, -795, 3966, -639, -3091, -1020, -2485 }, { -5192, -2292, -3945 } },
    { { -2565, 0, 3192, -795, 3966, -639, -3092, -1020, -2484 }, { -5180, -2280, -3934 } },
    { { -2564, 0, 3193, -795, 3967, -638, -3093, -1019, -2483 }, { -5169, -2268, -3923 } },
    { { -2563, 0, 3194, -794, 3967, -637, -3094, -1019, -2483 }, { -5158, -2256, -3912 } },
    { { -2562, 0, 3195, -794, 3967, -637, -3094, -1018, -2482 }, { -5147, -2244, -3901 } },
    { { -2561, 0, 3196, -794, 3967, -636, -3095, -1018, -2481 }, { -5135, -2231, -3890 } },
    { { -2560, 0, 3196, -794, 3967, -636, -3096, -1017, -2480 }, { -5124, -2219, -3879 } },
    { { -2559, 0, 3197, -793, 3967, -635, -3097, -1016, -2479 }, { -5113, -2207, -3868 } },
    { { -2559, 0, 3198, -793, 3967, -634, -3098, -1016, -2479 }, { -5102, -2195, -3857 } },
    { { -2558, 0, 3198, -793, 3968, -634, -3099, -1015, -2478 }, { -5091, -2183, -3847 } },
    { { -2557, 0, 3199, -792, 3968, -633, -3099, -1014, -2477 }, { -5080, -2171, -3836 } },
    { { -2556, 0, 3200, -792, 3968, -632, -3100, -1014, -2476 }, { -5069, -2159, -3826 } },
    { { -2555, 0, 3201, -792, 3968, -632, -3101, -1013, -2476 }, { -5058, -2147, -3815 } },
    { { -2554, 0, 3201, -791, 3968, -631, -3102, -1012, -2475 }, { -5047, -2135, -3805 } },
    { { -2553, 0, 3202, -791, 3968, -631, -3103, -1012, -2474 }, { -5037, -2123, -3795 } },
    { { -2552, 0, 3203, -790, 3969, -630, -3103, -1011, -2473 }, { -5026, -2111, -3785 } },
    { { -2552, 0, 3203, -790, 3969, -629, -3104, -1010, -2473 }, { -5015, -2099, -3774 } },
    { { -2551, 0, 3204, -790, 3969, -629, -3105, -1010, -2472 }, { -5005, -2088, -3764 } },
    { { -2550, 0, 3204, -789, 3969, -628, -3106, -1009, -2471 }, { -4994, -2076, -3755 } },
    { { -2549, 0, 3205, -789, 3969, -627, -3106, -1008, -2471 }, { -4984, -2064, -3745 } },
    { { -2549, 0, 3206, -789, 3970, -627, -3107, -1007, -2470 }, { -4973, -2053, -3735 } },
    { { -2548, 0, 3206, -788, 3970, -626, -3108, -1007, -2470 }, { -4963, -2041, -3725 } },
    { { -2547, 0, 3207, -788, 3970, -626, -3109, -1006, -2469 }, { -4953, -2030, -3716 } },
    { { -2546, 0, 3207, -787, 3970, -625, -3109, -1005, -2468 }, { -4943, -2019, -3706 } },
    { { -2546, 0, 3208, -787, 3970, -624, -3110, -1005, -2468 }, { -4933, -2007, -3697 } },
    { { -2545, 0, 3209, -786, 3970, -624, -3111, -1004, -2467 }, { -4923, -1996, -3688 } },
    { { -2544, 0, 3209, -786, 3971, -623, -3111, -1003, -2467 }, { -4913, -1985, -3679 } },
    { { -2543, 0, 3210, -786, 3971, -622, -3112, -1002, -2466 }, { -4903, -1974, -3670 } },
    { { -2543, 0, 3210, -785, 3971, -622, -3113, -1002, -2466 }, { -4893, -1963, -3661 } },
    { { -2542, 0, 3211, -785, 3971, -621, -3113, -1001, -2465 }, { -4884, -1953, -3652 } },
    { { -2541, 0, 3211, -784, 3971, -621, -3114, -1000, -2464 }, { -4874, -1942, -3643 } },
    { { -2541, 0, 3212, -784, 3972, -620, -3115, -1000, -2464 }, { -4865, -1931, -3634 } },
    { { -2540, 0, 3212, -783, 3972, -619, -3115, -999, -2463 }, { -4856, -1921, -3626 } },
    { { -2540, 0, 3213, -783, 3972, -619, -3116, -998, -2463 }, { -4846, -1911, -3618 } },
    { { -2539, 0, 3213, -782, 3972, -618, -3116, -997, -2462 }, { -4837, -1901, -3609 } },
    { { -2538, 0, 3214, -782, 3972, -618, -3117, -997, -2462 }, { -4828, -1890, -3601 } },
    { { -2538, 0, 3214, -782, 3972, -617, -3118, -996, -2462 }, { -4820, -1881, -3593 } },
    { { -2537, 0, 3215, -781, 3973, -616, -3118, -995, -2461 }, { -4811, -1871, -3585 } },
    { { -2537, 0, 3215, -781, 3973, -616, -3119, -995, -2461 }, { -4802, -1861, -3577 } },
    { { -2536, 0, 3215, -780, 3973, -615, -3119, -994, -2460 }, { -4794, -1852, -3570 } },
    { { -2536, 0, 3216, -780, 3973, -615, -3120, -993, -2460 }, { -4786, -1842, -3562 } },
    { { -2535, 0, 3216, -779, 3973, -614, -3120, -993, -2459 }, { -4778, -1833, -3555 } },
    { { -2535, 0, 3217, -779, 3973, -614, -3121, -992, -2459 }, { -4770, -1824, -3548 } },
    { { -2534, 0, 3217, -779, 3974, -613, -3121, -991, -2459 }, { -4762, -1815, -3541 } },
    { { -2534, 0, 3218, -778, 3974, -613, -3122, -990, -2458 }, { -4754, -1806, -3534 } },
    { { -2533, 0, 3218, -778, 3974, -612, -3122, -990, -2458 }, { -4746, -1798, -3527 } },
    { { -2533, 0, 3218, -777, 3974, -612, -3123, -989, -2458 }, { -4739, -1789, -3520 } },
    { { -2532, 0, 3219, -777, 3974, -611, -3123, -989, -2457 }, { -4732, -1781, -3514 } },
    { { -2532, 0, 3219, -776, 3974, -611, -3124, -988, -2457 }, { -4725, -1773, -3507 } },
    { { -2531, 0, 3219, -776, 3975, -610, -3124, -987, -2457 }, { -4718, -1765, -3501 } },
    { { -2531, 0, 3220, -776, 3975, -610, -3125, -987, -2456 }, { -4711, -1757, -3495 } },
    { { -2531, 0, 3220, -775, 3975, -609, -3125, -986, -2456 }, { -4704, -1750, -3489 } },
    { { -2530, 0, 3220, -775, 3975, -609, -3126, -986, -2456 }, { -4698, -1742, -3483 } },
    { { -2530, 0, 3221, -774, 3975, -608, -3126, -985, -2455 }, { -4691, -1735, -3478 } },
    { { -2529, 0, 3221, -774, 3975, -608, -3126, -984, -2455 }, { -4685, -1728, -3472 } },
    { { -2529, 0, 3221, -774, 3975, -607, -3127, -984, -2455 }, { -4679, -1721, -3467 } },
    { { -2529, 0, 3221, -773, 3976, -607, -3127, -983, -2455 }, { -4674, -1715, -3462 } },
    { { -2528, 0, 3222, -773, 3976, -607, -3127, -983, -2454 }, { -4668, -1708, -3457 } },
    { { -2528, 0, 3222, -773, 3976, -606, -3128, -982, -2454 }, { -4662, -1702, -3452 } },
    { { -2528, 0, 3222, -772, 3976, -606, -3128, -982, -2454 }, { -4657, -1696, -3447 } },
    { { -2527, 0, 3222, -772, 3976, -605, -3128, -981, -2454 }, { -4652, -1690, -3443 } },
    { { -2527, 0, 3223, -772, 3976, -605, -3129, -981, -2453 }, { -4647, -1685, -3438 } },
    { { -2527, 0, 3223, -771, 3976, -605, -3129, -980, -2453 }, { -4643, -1679, -3434 } },
    { { -2527, 0, 3223, -771, 3976, -604, -3129, -980, -2453 }, { -4638, -1674, -3430 } },
    { { -2526, 0, 3223, -771, 3976, -604, -3130, -980, -2453 }, { -4634, -1669, -3427 } },
    { { -2526, 0, 3223, -771, 3977, -604, -3130, -979, -2453 }, { -4630, -1665, -3423 } },
    { { -2526, 0, 3224, -770, 3977, -604, -3130, -979, -2453 }, { -4626, -1660, -3420 } },
    { { -2526, 0, 3224, -770, 3977, -603, -3130, -979, -2452 }, { -4623, -1656, -3417 } },
    { { -2525, 0, 3224, -770, 3977, -603, -3130, -978, -2452 }, { -4619, -1652, -3414 } },
    { { -2525, 0, 3224, -770, 3977, -603, -3131, -978, -2452 }, { -4616, -1649, -3411 } },
    { { -2525, 0, 3224, -770, 3977, -603, -3131, -978, -2452 }, { -4613, -1645, -3408 } },
    { { -2525, 0, 3224, -769, 3977, -602, -3131, -977, -2452 }, { -4610, -1642, -3406 } },
    { { -2525, 0, 3224, -769, 3977, -602, -3131, -977, -2452 }, { -4608, -1639, -3403 } },
    { { -2525, 0, 3225, -769, 3977, -602, -3131, -977, -2452 }, { -4605, -1637, -3401 } },
    { { -2525, 0, 3225, -769, 3977, -602, -3131, -977, -2452 }, { -4603, -1634, -3400 } },
    { { -2524, 0, 3225, -769, 3977, -602, -3132, -977, -2452 }, { -4601, -1632, -3398 } },
    { { -2524, 0, 3225, -769, 3977, -602, -3132, -976, -2452 }, { -4600, -1630, -3397 } },
    { { -2524, 0, 3225, -769, 3977, -602, -3132, -976, -2451 }, { -4599, -1629, -3395 } },
    { { -2524, 0, 3225, -769, 3977, -602, -3132, -976, -2451 }, { -4597, -1627, -3394 } },
    { { -2524, 0, 3225, -769, 3977, -601, -3132, -976, -2451 }, { -4596, -1626, -3394 } },
    { { -2524, 0, 3225, -768, 3977, -601, -3132, -976, -2451 }, { -4596, -1626, -3393 } },
    { { -2524, 0, 3225, -768, 3977, -601, -3132, -976, -2451 }, { -4595, -1625, -3393 } },
    { { -2524, 0, 3225, -768, 3977, -601, -3132, -976, -2451 }, { -4595, -1625, -3393 } },
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
    Gp_PulseState1C80();
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
/// `CdCmd_CancelReplaceAndActivate`.
void func_actor_303600_80162698(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
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
