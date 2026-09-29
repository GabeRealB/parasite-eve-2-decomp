#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/item_pickup.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

extern GpImgRec D_actor_141000_8013D72C[2];

extern GpImgRec D_actor_141000_8013D4DC[2];

extern GpImgRec D_actor_141000_8013D28C[2];

typedef struct Actor141000Point {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[4];
} Actor141000Point;

typedef struct Actor141000Proj {
    /* 0x0 */ DVECTOR sxy;
    /* 0x4 */ s32     z;
} Actor141000Proj;
STATIC_ASSERT_SIZEOF(Actor141000Proj, 0x8);

/// Work block of the overlay's walker, allocated zeroed by its spawn routine
/// and kept at `Task::work`: a nineteen-part rig, the model state and walk
/// state, and the texture upload the overlay runs alongside the walk.
typedef struct Actor141000Work {
    ActorAnimRig19   rig;
    ActorModelState  model;
    ActorWalkState   walk;
    /* 0x4C4 */ u16  field_4C4; // source rect the upload countdown at 0x4C6 reloads from
    /* 0x4C6 */ u16  field_4C6; // upload countdown; `func_actor_141000_801335D4` runs it down and an underflow starts the next upload
    /* 0x4C8 */ s8   field_4C8; // variant the 0x7DB handler latches; 0 picks anim 10, non-zero anim 2
    /* 0x4C9 */ s8   field_4C9;
    /* 0x4CA */ s8   field_4CA; // upload step the switch at 0x801335D4 dispatches on
    /* 0x4CB */ byte pad_4CB;
} Actor141000Work;
STATIC_ASSERT_SIZEOF(Actor141000Work, 0x4CC);

/// Work block of the overlay's controller task -- the one whose three `Task`
/// states are `D_actor_141000_80131E30`, which spawns the actor and then drives
/// its model through the four animation states at 0x80131E3C.
///
/// `func_actor_141000_80132C7C` allocates it with `memCalloc(0x10, 0)` and
/// parks it in that task's `Task::work` slot, so the size below is the
/// allocation and not a guess; the slot is not a `TaskIdMap` here.
///
/// `field_0` is armed at 0xFFF by the spawn state, `frames` is the counter the
/// state at 0x80132EF4 masks with 7 to pace the actor's spawns, `scale` is the
/// Z scale the state at 0x80132E24 ramps by 0x100 a frame up to 0x1000, `state`
/// is the index `func_actor_141000_80132D3C` dispatches through, and `ticks` is
/// the per-state frame counter the state at 0x80132EB0 holds for 0x1F frames.
typedef struct Actor141000CtrlWork {
    /* 0x0 */ s32  field_0; // armed at 0xFFF by the spawn state
    /* 0x4 */ byte pad_4[0x4];
    /* 0x8 */ u16  frames;
    /* 0xA */ u16  scale;
    /* 0xC */ u16  state;
    /* 0xE */ u16  ticks;
} Actor141000CtrlWork;
STATIC_ASSERT_SIZEOF(Actor141000CtrlWork, 0x10);

/// Optional start animation for the 0x7DD placement handler: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are anim 10 (or 2
/// once `field_4C8` is latched) and 1.
typedef GpSpawnAnimArg Actor141000SpawnAnim;

/// The rotation table `func_actor_141000_80132FD0` feeds to `RotMatrix`: 0x5A
/// `SVECTOR` axis triples, one per frame of the ramp the controller's state 2
/// climbs, ending at the entry index 0x59 the function clamps to.
extern SVECTOR D_actor_141000_80134228[];

/// The world positions matching `D_actor_141000_80134228`, same 0x5A entries
/// and same index; the function copies the chosen triple into the root
/// coordinate's translation and then drops X by 40.
extern SVECTOR D_actor_141000_801344F8[];

/// Quad index table: sixteen quads, four point/colour indices each.
extern s8 D_actor_141000_801347C8[][4];

/// Base vertex colours, scaled by the controller's `field_0` each frame.
extern u8 D_actor_141000_80134808[][4];

extern SVECTOR D_actor_141000_80134868[2];
extern SVECTOR D_actor_141000_80134878[];
extern SVECTOR D_actor_141000_801348A8[];

/// The descriptor table the controller spawns from: index 1 is the task its
/// spawn state starts and index 2 the model actor `func_actor_141000_80132EF4`
/// spawns later, every eighth frame.
extern TaskDesc D_actor_141000_801348D8[];

/// The texture uploads `func_actor_141000_801335D4` walks, one per value of
/// `Actor141000Work::field_4CA`, and `func_actor_141000_80133FA8` picks from:
/// each a `GpImgRec` whose own `rect` carries the upload size and whose `data`
/// points at the pixel blob.

/// Animation bank table `func_800B3F84` re-seeds the slots from, indexed by
/// the preset's bank index.
extern GpAnimSet*  D_actor_141000_8013D74C[11];
extern GpAnimSet** D_actor_141000_8013D778[1];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_141000_8013392C`; terminator id 0x7FFFFFFF.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, GpCmdArg*);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, GpXformArg*, Actor141000SpawnAnim*);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor141000MsgEntry;
STATIC_ASSERT_SIZEOF(Actor141000MsgEntry, 8);

extern Actor141000MsgEntry D_actor_141000_8013D788[];

static void func_actor_141000_80132C7C(Task* task);
static void func_actor_141000_80132D3C(Task* task);
static void func_actor_141000_80132E04(Task* task);
static void func_actor_141000_80132E24(Task* arg0);
static void func_actor_141000_80132EB0(Task* arg0);
static void func_actor_141000_80132EF4(Task* arg0);
static void func_actor_141000_80132FC8(Task* arg0);
static s32  func_actor_141000_80132FD0(GfxCoord* arg0, s32 arg1);
static void func_actor_141000_8013308C(GfxCoord* arg0, s32 arg1);
static void func_actor_141000_80133204(Task* task);
static void func_actor_141000_80133260(Task* arg0);
static void func_actor_141000_801332A0(Task* task);
static void func_actor_141000_80133490(Task* arg0);
static void func_actor_141000_801335D4(Task* arg0);
static void func_actor_141000_8013392C(Task* arg0);
static void func_actor_141000_801339BC(Task* arg0);
static void func_actor_141000_801339DC(Task* arg0);
static void func_actor_141000_801339F8(Task* arg0);
static void func_actor_141000_80133A00(Task* arg0);
static void func_actor_141000_80133A68(Task* task);
static void func_actor_141000_80133B28(Task* arg0);
static void func_actor_141000_80133BD8(Task* arg0);
s32         func_actor_141000_80133CD8(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

/// The model actor's attach states: chain under the spawner, then draw the
/// sixteen quads every frame, then `taskKill`. Dispatched by
/// `func_actor_141000_801331AC`.
static const TaskFuncTable3 D_actor_141000_80131E24 = { {
    func_actor_141000_80133204,
    func_actor_141000_80133260,
    taskKill,
} };

/// The controller's three states - spawn, per-frame tick and `taskKill` -
/// dispatched by `func_actor_141000_80132C24`.
static const TaskFuncTable3 D_actor_141000_80131E30 = { {
    func_actor_141000_80132C7C,
    func_actor_141000_80132D3C,
    taskKill,
} };

/// The controller's four animation states, dispatched by
/// `func_actor_141000_80132D3C` through the controller work block's `state`
/// halfword.
static const TaskFuncTable4 D_actor_141000_80131E3C = { {
    func_actor_141000_80132E24,
    func_actor_141000_80132EB0,
    func_actor_141000_80132EF4,
    func_actor_141000_80132FC8,
} };

/// The model actor's three states - spawn, per-frame tick and exit -
/// dispatched by `func_actor_141000_801338C0`.
static const TaskFuncTable3 D_actor_141000_80131E4C = { {
    func_actor_141000_8013392C,
    func_actor_141000_801332A0,
    func_actor_141000_801339BC,
} };

/// The model actor's four main-body states, dispatched by
/// `func_actor_141000_80133A00` through `Actor141000Work::walk.motionStep`.
static const TaskFuncTable4 D_actor_141000_80131E58 = { {
    func_actor_141000_80133A68,
    func_actor_141000_80133B28,
    func_actor_141000_80133490,
    func_actor_141000_80133BD8,
} };

/// The local-space offset the main body's state 1 (`func_actor_141000_80133B28`)
/// rotates into `Actor141000Work::step`: straight ahead along the part's own
/// axis, halved first while `field_4C8` is clear.
static const VECTOR D_actor_141000_80131E68 = { 0, 0, 0x300000 };

extern u32     D_actor_141000_801340E0[1];
extern SVECTOR D_actor_141000_801340E4[9];
extern TmdBone D_actor_141000_801340BC[1];
extern u32     D_actor_141000_8013412C[54];

extern TmdSource D_actor_141000_80134204;
void             func_actor_141000_80132C24(Task*);
void             func_actor_141000_801330C0(Task*);
void             func_actor_141000_801331AC(Task*);

extern GpAnimSet D_actor_141000_8013A2C0;
extern GpAnimSet D_actor_141000_8013A6B8;
extern GpAnimSet D_actor_141000_8013B29C;
extern GpAnimSet D_actor_141000_8013B458;
extern GpAnimSet D_actor_141000_8013B620;
extern GpAnimSet D_actor_141000_8013B8A8;
extern GpAnimSet D_actor_141000_8013C030;
extern GpAnimSet D_actor_141000_8013C268;
extern GpAnimSet D_actor_141000_8013C41C;
extern GpAnimSet D_actor_141000_8013C66C;
extern TmdSource D_actor_141000_8013A0B0;
s32              func_actor_141000_801336DC(Task*, s32, GpXformArg*, Actor141000SpawnAnim*);
s32              func_actor_141000_80133CD8(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_141000_80133E10(Task*, s32, GpXformArg*);
s32              func_actor_141000_80133E8C(Task*, s32, s32);
s32              func_actor_141000_80133F6C(Task*, s32, GpCmdArg*);
s32              func_actor_141000_80133FA8(Task*, s32, s32);
void             func_actor_141000_801338C0(Task*);

TmdBone D_actor_141000_801340BC[1] = {
#include "assets/actor_141000_model_023E4_skeleton.inc"
};

u32 D_actor_141000_801340E0[1] = {
#include "assets/actor_141000_model_023E4_partVerts.inc"
};

SVECTOR D_actor_141000_801340E4[9] = {
#include "assets/actor_141000_model_023E4_verts.inc"
};

u32 D_actor_141000_8013412C[54] = {
#include "assets/actor_141000_model_023E4_stream.inc"
};

TmdSource D_actor_141000_80134204 = {
    0,
    328,
    0,
    1,
    D_actor_141000_801340E0,
    D_actor_141000_801340E4,
    &D_actor_141000_801340E4[9],
    D_actor_141000_801340BC,
    D_actor_141000_8013412C,
};

SVECTOR D_actor_141000_80134228[90] = {
    { 0, 0, 0, 0 },
    { -3, 0, 0, 0 },
    { -6, 0, 0, 0 },
    { -9, 0, 0, 0 },
    { -12, 0, 0, 0 },
    { -15, 0, 0, 0 },
    { -18, 0, 0, 0 },
    { -22, 0, 0, 0 },
    { -25, 0, 0, 0 },
    { -28, 0, 0, 0 },
    { -31, 0, 0, 0 },
    { -34, 0, 0, 0 },
    { -37, 0, 0, 0 },
    { -40, 0, 0, 0 },
    { -44, 0, 0, 0 },
    { -47, 0, 0, 0 },
    { -50, 0, 0, 0 },
    { -53, 0, 0, 0 },
    { -56, 0, 0, 0 },
    { -59, 0, 0, 0 },
    { -62, 0, 0, 0 },
    { -66, 0, 0, 0 },
    { -69, 0, 0, 0 },
    { -72, 0, 0, 0 },
    { -75, 0, 0, 0 },
    { -78, 0, 0, 0 },
    { -81, 0, 0, 0 },
    { -85, 0, 0, 0 },
    { -88, 0, 0, 0 },
    { -91, 0, 0, 0 },
    { -94, 0, 0, 0 },
    { -97, 0, 0, 0 },
    { -100, 0, 0, 0 },
    { -103, 0, 0, 0 },
    { -107, 0, 0, 0 },
    { -110, 0, 0, 0 },
    { -113, 0, 0, 0 },
    { -116, 0, 0, 0 },
    { -119, 0, 0, 0 },
    { -122, 0, 0, 0 },
    { -125, 0, 0, 0 },
    { -129, 0, 0, 0 },
    { -132, 0, 0, 0 },
    { -135, 0, 0, 0 },
    { -138, 0, 0, 0 },
    { -141, 0, 0, 0 },
    { -144, 0, 0, 0 },
    { -148, 0, 0, 0 },
    { -151, 0, 0, 0 },
    { -154, 0, 0, 0 },
    { -157, 0, 0, 0 },
    { -160, 0, 0, 0 },
    { -163, 0, 0, 0 },
    { -166, 0, 0, 0 },
    { -170, 0, 0, 0 },
    { -173, 0, 0, 0 },
    { -176, 0, 0, 0 },
    { -179, 0, 0, 0 },
    { -182, 0, 0, 0 },
    { -185, 0, 0, 0 },
    { -188, 0, 0, 0 },
    { -192, 0, 0, 0 },
    { -195, 0, 0, 0 },
    { -198, 0, 0, 0 },
    { -201, 0, 0, 0 },
    { -204, 0, 0, 0 },
    { -207, 0, 0, 0 },
    { -211, 0, 0, 0 },
    { -214, 0, 0, 0 },
    { -217, 0, 0, 0 },
    { -220, 0, 0, 0 },
    { -223, 0, 0, 0 },
    { -226, 0, 0, 0 },
    { -229, 0, 0, 0 },
    { -233, 0, 0, 0 },
    { -236, 0, 0, 0 },
    { -239, 0, 0, 0 },
    { -242, 0, 0, 0 },
    { -245, 0, 0, 0 },
    { -248, 0, 0, 0 },
    { -251, 0, 0, 0 },
    { -255, 0, 0, 0 },
    { -258, 0, 0, 0 },
    { -261, 0, 0, 0 },
    { -264, 0, 0, 0 },
    { -267, 0, 0, 0 },
    { -270, 0, 0, 0 },
    { -274, 0, 0, 0 },
    { -277, 0, 0, 0 },
    { -280, 0, 0, 0 },
};

SVECTOR D_actor_141000_801344F8[90] = {
    { 1766, -1530, -6406, 0 },
    { 1766, -1520, -6406, 0 },
    { 1766, -1510, -6406, 0 },
    { 1766, -1500, -6406, 0 },
    { 1766, -1491, -6406, 0 },
    { 1766, -1481, -6406, 0 },
    { 1766, -1471, -6406, 0 },
    { 1766, -1462, -6406, 0 },
    { 1766, -1452, -6406, 0 },
    { 1766, -1442, -6406, 0 },
    { 1766, -1432, -6406, 0 },
    { 1766, -1423, -6406, 0 },
    { 1766, -1413, -6406, 0 },
    { 1766, -1403, -6406, 0 },
    { 1766, -1394, -6406, 0 },
    { 1766, -1384, -6406, 0 },
    { 1766, -1374, -6406, 0 },
    { 1766, -1364, -6406, 0 },
    { 1766, -1355, -6406, 0 },
    { 1766, -1345, -6406, 0 },
    { 1766, -1335, -6406, 0 },
    { 1766, -1326, -6406, 0 },
    { 1766, -1316, -6406, 0 },
    { 1766, -1306, -6406, 0 },
    { 1766, -1297, -6406, 0 },
    { 1766, -1287, -6406, 0 },
    { 1766, -1277, -6406, 0 },
    { 1766, -1267, -6406, 0 },
    { 1766, -1258, -6406, 0 },
    { 1766, -1248, -6406, 0 },
    { 1766, -1238, -6406, 0 },
    { 1766, -1229, -6406, 0 },
    { 1766, -1219, -6406, 0 },
    { 1766, -1209, -6406, 0 },
    { 1766, -1199, -6406, 0 },
    { 1766, -1190, -6406, 0 },
    { 1766, -1180, -6406, 0 },
    { 1766, -1170, -6406, 0 },
    { 1766, -1161, -6406, 0 },
    { 1766, -1151, -6406, 0 },
    { 1766, -1141, -6406, 0 },
    { 1766, -1131, -6406, 0 },
    { 1766, -1122, -6406, 0 },
    { 1766, -1112, -6406, 0 },
    { 1766, -1102, -6406, 0 },
    { 1766, -1093, -6406, 0 },
    { 1766, -1083, -6406, 0 },
    { 1766, -1073, -6406, 0 },
    { 1766, -1064, -6406, 0 },
    { 1766, -1054, -6406, 0 },
    { 1766, -1044, -6406, 0 },
    { 1766, -1034, -6406, 0 },
    { 1766, -1025, -6406, 0 },
    { 1766, -1015, -6406, 0 },
    { 1766, -1005, -6406, 0 },
    { 1766, -996, -6406, 0 },
    { 1766, -986, -6406, 0 },
    { 1766, -976, -6406, 0 },
    { 1766, -966, -6406, 0 },
    { 1766, -957, -6406, 0 },
    { 1766, -947, -6406, 0 },
    { 1766, -937, -6406, 0 },
    { 1766, -928, -6406, 0 },
    { 1766, -918, -6406, 0 },
    { 1766, -908, -6406, 0 },
    { 1766, -898, -6406, 0 },
    { 1766, -889, -6406, 0 },
    { 1766, -879, -6406, 0 },
    { 1766, -869, -6406, 0 },
    { 1766, -860, -6406, 0 },
    { 1766, -850, -6406, 0 },
    { 1766, -840, -6406, 0 },
    { 1766, -831, -6406, 0 },
    { 1766, -821, -6406, 0 },
    { 1766, -811, -6406, 0 },
    { 1766, -801, -6406, 0 },
    { 1766, -792, -6406, 0 },
    { 1766, -782, -6406, 0 },
    { 1766, -772, -6406, 0 },
    { 1766, -763, -6406, 0 },
    { 1766, -753, -6406, 0 },
    { 1766, -743, -6406, 0 },
    { 1766, -733, -6406, 0 },
    { 1766, -724, -6406, 0 },
    { 1766, -714, -6406, 0 },
    { 1766, -704, -6406, 0 },
    { 1766, -695, -6406, 0 },
    { 1766, -685, -6406, 0 },
    { 1766, -675, -6406, 0 },
    { 1766, -666, -6406, 0 },
};

s8 D_actor_141000_801347C8[16][4] = {
    { 0, 6, 1, 11 },
    { 0, 6, 5, 7 },
    { 0, 3, 1, 2 },
    { 0, 3, 5, 4 },
    { 6, 9, 7, 8 },
    { 6, 9, 11, 10 },
    { 2, 1, 14, 13 },
    { 3, 2, 15, 14 },
    { 3, 4, 15, 16 },
    { 4, 5, 16, 17 },
    { 5, 7, 17, 19 },
    { 8, 7, 20, 19 },
    { 9, 8, 21, 20 },
    { 9, 10, 21, 22 },
    { 10, 11, 22, 23 },
    { 1, 11, 13, 23 },
};

u8 D_actor_141000_80134808[24][4] = {
    { 200, 176, 160, 0 },
    { 200, 176, 160, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 112, 88, 64, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_141000_80134868[2] = {
    { 0, 0, 0, 0 },
    { 0, 0, 960, 0 },
};

SVECTOR D_actor_141000_80134878[6] = {
    { 0, 0, 0, 0 },
    { 10, 0, 0, 0 },
    { 6, 3, 0, 0 },
    { 0, 6, 0, 0 },
    { -6, 3, 0, 0 },
    { -10, 0, 0, 0 },
};

SVECTOR D_actor_141000_801348A8[6] = {
    { 0, 0, 0, 0 },
    { -10, 0, 0, 0 },
    { -6, -3, 0, 0 },
    { 0, -6, 0, 0 },
    { 6, -3, 0, 0 },
    { 10, 0, 0, 0 },
};

TaskDesc D_actor_141000_801348D8[3] = {
    { 1, 192, func_actor_141000_80132C24, { .model = &D_actor_141000_80134204 } },
    { 2, 192, func_actor_141000_801331AC, { .model = NULL } },
    { 2, 192, func_actor_141000_801330C0, { .model = NULL } },
};

TmdBone D_actor_141000_801348FC[19] = {
#include "assets/actor_141000_model_08290_skeleton.inc"
};

u32 D_actor_141000_80134BA8[19] = {
#include "assets/actor_141000_model_08290_partVerts.inc"
};

SVECTOR D_actor_141000_80134BF4[365] = {
#include "assets/actor_141000_model_08290_verts.inc"
};

SVECTOR D_actor_141000_8013575C[385] = {
#include "assets/actor_141000_model_08290_normals.inc"
};

u32 D_actor_141000_80136364[3923] = {
#include "assets/actor_141000_model_08290_stream.inc"
};

TmdSource D_actor_141000_8013A0B0 = {
    0,
    21760,
    5992,
    19,
    D_actor_141000_80134BA8,
    D_actor_141000_80134BF4,
    D_actor_141000_8013575C,
    D_actor_141000_801348FC,
    D_actor_141000_80136364,
};

AnimationPackedPose D_actor_141000_8013A0D4[2] = {
#include "assets/actor_141000_animation_084A0_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013A0EC[23] = {
#include "assets/actor_141000_animation_084A0_bank4.inc"
};

AnimationRecord D_actor_141000_8013A148[84] = {
#include "assets/actor_141000_animation_084A0_records.inc"
};

u16 D_actor_141000_8013A298[20] = {
#include "assets/actor_141000_animation_084A0_indices.inc"
};

GpAnimSet D_actor_141000_8013A2C0 = {
    D_actor_141000_8013A148,
    D_actor_141000_8013A298,
    { NULL, D_actor_141000_8013A0D4, NULL, NULL, D_actor_141000_8013A0EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013A2E8[7] = {
#include "assets/actor_141000_animation_08898_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013A33C[75] = {
#include "assets/actor_141000_animation_08898_bank4.inc"
};

AnimationRecord D_actor_141000_8013A468[138] = {
#include "assets/actor_141000_animation_08898_records.inc"
};

u16 D_actor_141000_8013A690[20] = {
#include "assets/actor_141000_animation_08898_indices.inc"
};

GpAnimSet D_actor_141000_8013A6B8 = {
    D_actor_141000_8013A468,
    D_actor_141000_8013A690,
    { NULL, D_actor_141000_8013A2E8, NULL, NULL, D_actor_141000_8013A33C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013A6E0[22] = {
#include "assets/actor_141000_animation_0947C_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013A7E8[298] = {
#include "assets/actor_141000_animation_0947C_bank4.inc"
};

AnimationRecord D_actor_141000_8013AC90[377] = {
#include "assets/actor_141000_animation_0947C_records.inc"
};

u16 D_actor_141000_8013B274[20] = {
#include "assets/actor_141000_animation_0947C_indices.inc"
};

GpAnimSet D_actor_141000_8013B29C = {
    D_actor_141000_8013AC90,
    D_actor_141000_8013B274,
    { NULL, D_actor_141000_8013A6E0, NULL, NULL, D_actor_141000_8013A7E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013B2C4[2] = {
#include "assets/actor_141000_animation_09638_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013B2DC[20] = {
#include "assets/actor_141000_animation_09638_bank4.inc"
};

AnimationRecord D_actor_141000_8013B32C[65] = {
#include "assets/actor_141000_animation_09638_records.inc"
};

u16 D_actor_141000_8013B430[20] = {
#include "assets/actor_141000_animation_09638_indices.inc"
};

GpAnimSet D_actor_141000_8013B458 = {
    D_actor_141000_8013B32C,
    D_actor_141000_8013B430,
    { NULL, D_actor_141000_8013B2C4, NULL, NULL, D_actor_141000_8013B2DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013B480[2] = {
#include "assets/actor_141000_animation_09800_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013B498[26] = {
#include "assets/actor_141000_animation_09800_bank4.inc"
};

AnimationRecord D_actor_141000_8013B500[62] = {
#include "assets/actor_141000_animation_09800_records.inc"
};

u16 D_actor_141000_8013B5F8[20] = {
#include "assets/actor_141000_animation_09800_indices.inc"
};

GpAnimSet D_actor_141000_8013B620 = {
    D_actor_141000_8013B500,
    D_actor_141000_8013B5F8,
    { NULL, D_actor_141000_8013B480, NULL, NULL, D_actor_141000_8013B498, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013B648[5] = {
#include "assets/actor_141000_animation_09A88_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013B684[47] = {
#include "assets/actor_141000_animation_09A88_bank4.inc"
};

AnimationRecord D_actor_141000_8013B740[80] = {
#include "assets/actor_141000_animation_09A88_records.inc"
};

u16 D_actor_141000_8013B880[20] = {
#include "assets/actor_141000_animation_09A88_indices.inc"
};

GpAnimSet D_actor_141000_8013B8A8 = {
    D_actor_141000_8013B740,
    D_actor_141000_8013B880,
    { NULL, D_actor_141000_8013B648, NULL, NULL, D_actor_141000_8013B684, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013B8D0[14] = {
#include "assets/actor_141000_animation_0A210_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013B978[155] = {
#include "assets/actor_141000_animation_0A210_bank4.inc"
};

AnimationRecord D_actor_141000_8013BBE4[265] = {
#include "assets/actor_141000_animation_0A210_records.inc"
};

u16 D_actor_141000_8013C008[20] = {
#include "assets/actor_141000_animation_0A210_indices.inc"
};

GpAnimSet D_actor_141000_8013C030 = {
    D_actor_141000_8013BBE4,
    D_actor_141000_8013C008,
    { NULL, D_actor_141000_8013B8D0, NULL, NULL, D_actor_141000_8013B978, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013C058[2] = {
#include "assets/actor_141000_animation_0A448_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013C070[22] = {
#include "assets/actor_141000_animation_0A448_bank4.inc"
};

AnimationRecord D_actor_141000_8013C0C8[94] = {
#include "assets/actor_141000_animation_0A448_records.inc"
};

u16 D_actor_141000_8013C240[20] = {
#include "assets/actor_141000_animation_0A448_indices.inc"
};

GpAnimSet D_actor_141000_8013C268 = {
    D_actor_141000_8013C0C8,
    D_actor_141000_8013C240,
    { NULL, D_actor_141000_8013C058, NULL, NULL, D_actor_141000_8013C070, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013C290[2] = {
#include "assets/actor_141000_animation_0A5FC_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013C2A8[26] = {
#include "assets/actor_141000_animation_0A5FC_bank4.inc"
};

AnimationRecord D_actor_141000_8013C310[57] = {
#include "assets/actor_141000_animation_0A5FC_records.inc"
};

u16 D_actor_141000_8013C3F4[20] = {
#include "assets/actor_141000_animation_0A5FC_indices.inc"
};

GpAnimSet D_actor_141000_8013C41C = {
    D_actor_141000_8013C310,
    D_actor_141000_8013C3F4,
    { NULL, D_actor_141000_8013C290, NULL, NULL, D_actor_141000_8013C2A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_141000_8013C444[2] = {
#include "assets/actor_141000_animation_0A84C_bank1.inc"
};

AnimationPackedRotation D_actor_141000_8013C45C[40] = {
#include "assets/actor_141000_animation_0A84C_bank4.inc"
};

AnimationRecord D_actor_141000_8013C4FC[82] = {
#include "assets/actor_141000_animation_0A84C_records.inc"
};

u16 D_actor_141000_8013C644[20] = {
#include "assets/actor_141000_animation_0A84C_indices.inc"
};

GpAnimSet D_actor_141000_8013C66C = {
    D_actor_141000_8013C4FC,
    D_actor_141000_8013C644,
    { NULL, D_actor_141000_8013C444, NULL, NULL, D_actor_141000_8013C45C, NULL, NULL, NULL },
};

u_long D_actor_141000_8013C694[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD6F,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x6858AD87,
    0x81696969,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x36697FAD,
    0x80573636,
    0x879DAD98,
    0x59494A4B,
    0x53789D4B,
    0x9F9F9F56,
    0x7D569F9F,
    0xED9DD6AD,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x53416F9D,
    0x9F9F9F56,
    0x7D539F9F,
    0x4A4CBF6F,
    0x6FE64959,
    0xAEAED25E,
    0xAEAEAEAE,
    0x9F5CCFD2,
    0xBABF878A,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0xCF9F87C2,
    0xAEAED2D2,
    0xAEAEAEAE,
    0x878ACFD2,
    0x49494A5B,
    0xAED29F87,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875CAEAE,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0xCFEDBFE2,
    0xAEAEAEBB,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x49878ACF,
    0xD29F5949,
    0x107D2AE,
    0x1010201,
    0xAEAE0101,
    0xE6ED5CBB,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEB7D29F,
    0x1010102,
    0x7010102,
    0xCFAEAED2,
    0x5C49498A,
    0x5E5CD2CF,
    0x2040504,
    0x7070202,
    0xCFBBD207,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD29F9DC0,
    0x110707D2,
    0x1020205,
    0x5E5E0405,
    0x599FD2BB,
    0xCF5C5B4A,
    0x9B11537D,
    0x2FA076A,
    0x5E040404,
    0xC59D5C5E,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565C9F,
    0xAE9B9B05,
    0x5110702,
    0xCF5C9856,
    0x87494A6F,
    0x587DCF8A,
    0x4116A11,
    0x5110702,
    0x8A535304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x6A040546,
    0x4010205,
    0x58460511,
    0x86AD5C5C,
    0x5CA6894A,
    0x56A485E,
    0x6A050505,
    0x5811059B,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x11050505,
    0x78CF5348,
    0x894A8791,
    0x53CF8A8C,
    0x9B6A6A46,
    0x56A9B9B,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x11054658,
    0x6A6A9B6A,
    0x5E469B11,
    0x87A97D5E,
    0x768F894A,
    0x53535E5E,
    0x6A6A6A6A,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x6A115653,
    0x53539B6A,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x46467D53,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x46464646,
    0x7F53537D,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_141000_8013CA7C[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_141000_8013C694 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_141000_8013CA9C[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x687F8787,
    0x81686868,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x81817FAD,
    0x98818181,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x69686868,
    0x7F816969,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x817F6F9D,
    0x81696969,
    0x7D988181,
    0x4A4CBF6F,
    0x6FE64959,
    0x69687F5E,
    0x69696969,
    0xD67F8169,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x7F6F6FC2,
    0x69696981,
    0x81816969,
    0x878A7D98,
    0x49494A5B,
    0xD6D69F87,
    0xBBED8787,
    0xEDBBBBBB,
    0x87D68787,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x6F87BFE2,
    0xBBED876F,
    0xD2BBBBBB,
    0xD6875BCF,
    0x49878AD6,
    0x9F9F5949,
    0xAEAED25C,
    0xAEAEAEAE,
    0x5CBBAEAE,
    0xE6ED87ED,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEAE9F9F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875BBBAE,
    0x5C49498A,
    0xAEAED2CF,
    0x2020202,
    0x2020202,
    0xED5CAEAE,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD25C9DC0,
    0x20202D2,
    0x2020202,
    0xAEAE0202,
    0x599FD2BB,
    0xD25C5B4A,
    0x20702AE,
    0x2FA0202,
    0x5C070707,
    0xC59D5CBB,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565CD2,
    0x1070707,
    0x2020202,
    0xCFAEAE5E,
    0x87494A6F,
    0x5C02CF8A,
    0x2070704,
    0x5050702,
    0x8A5E5304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x11040546,
    0x4010205,
    0x5E460404,
    0x86AD5CAE,
    0x5CA6894A,
    0x5045ED2,
    0x6A05056A,
    0x5811056A,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x4040505,
    0x78CF5348,
    0x894A8791,
    0x535E8A8C,
    0x11050446,
    0x5116A6A,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x4054658,
    0x46A9B6A,
    0x5E460504,
    0x87A97D5E,
    0x768F894A,
    0x53535E8A,
    0x5050504,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x5115653,
    0x53040405,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x53535353,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x53464646,
    0x7F535353,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_141000_8013CE84[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_141000_8013CA9C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_141000_8013CEA4[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x91918787,
    0x91919191,
    0xE66F9191,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x767676AD,
    0x76767676,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x7F7F9191,
    0x917F7F7F,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x76766F9D,
    0x7F7F7F7F,
    0x7D76767F,
    0x4A4CBF6F,
    0x78E64959,
    0x7F7F9191,
    0x7F7F7F7F,
    0xD6917F7F,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x76916FC2,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x878A7D76,
    0x49494A5B,
    0x7F91D687,
    0x6969697F,
    0x81696969,
    0x87D6917F,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x9187BFE2,
    0x69817F91,
    0x69696969,
    0x767F7F69,
    0x49878AD6,
    0xD69F5949,
    0x69817F91,
    0x69696969,
    0x91816969,
    0xE6EDD6D6,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0x81919187,
    0x69696969,
    0x69696969,
    0x8776767F,
    0x5C49498A,
    0x7F91D69F,
    0x69696981,
    0x69696969,
    0xD6919181,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0x919D9DC0,
    0x69698191,
    0x69696969,
    0x767F6969,
    0x59878776,
    0xD65C5B4A,
    0x817F7FD6,
    0x81818181,
    0x917F7F81,
    0xC59DD691,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x7F919191,
    0x8181817F,
    0x7F818181,
    0x8787767F,
    0x87494A87,
    0xD2D2ED8A,
    0xD6D6D6ED,
    0xEDD6D6D6,
    0xEDAEAEED,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0xAEED9DC5,
    0xD6EDEDAE,
    0xD6D6D6D6,
    0xD2EDD6D6,
    0x86ADEDD2,
    0x9FA6894A,
    0xAEAEAE5C,
    0xAEAEAEAE,
    0xD2AEAEAE,
    0xCA9D70ED,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0xAED2ED6F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x785CAEAE,
    0x894A8791,
    0xED8A8A8C,
    0xAEAED25C,
    0xD2AEAEAE,
    0xA698EDED,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0xD2EDED8C,
    0xAEAEAEAE,
    0xED5CD2AE,
    0x87A97D76,
    0x768F894A,
    0xA1767676,
    0xEDEDEDED,
    0x5076A1ED,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x8C8C8CA6,
    0xEDEDA18C,
    0xA1EDEDED,
    0x7E987676,
    0xCA4A9D8F,
    0x7F7F7EAB,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_141000_8013D28C[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_141000_8013CEA4 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_141000_8013D2AC[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x45455242,
    0x35353535,
    0x57353529,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x52986052,
    0x57292929,
    0x45457E7F,
    0x52525252,
    0x64525252,
    0x42424264,
    0x735F4242,
    0x87B88689,
    0x8770ADAD,
    0x709D5ABA,
    0x4242607B,
    0x60606464,
    0x60426460,
    0x595B707A,
    0x6C6C6D6D,
    0x6D6C6D6D,
    0x596E6E6E,
    0x607589BA,
    0x79796064,
    0x98606464,
    0x788A8A6F,
    0x8A787878,
    0x8AD5D5D5,
    0x8A8A8A8A,
    0x9187EDED,
    0x79606079,
    0x9A626464,
    0x4552657E,
    0x8083557,
    0x29080808,
    0x67553529,
    0x767D987F,
    0x6060605F,
    0x60606464,
    0x65656464,
    0x29354565,
    0x29290829,
    0x617E5735,
    0x7A7A9898,
    0x6060605F,
    0x60606460,
    0x76927A5F,
    0x657E7BB1,
    0x65656565,
    0xA6AA759A,
    0x609691A6,
    0x60796060,
    0x60646460,
    0x91767A5F,
    0xB1DB9191,
    0x91DBACB1,
    0xAAAAA6AA,
    0x607A8F8C,
    0x60606060,
    0x60646060,
    0x91737762,
    0xD4AAAAAA,
    0xAA8C8CD4,
    0x73918CAA,
    0x60629575,
    0x79796060,
    0x60606079,
    0x76777960,
    0xAC737373,
    0x92927292,
    0x7A959292,
    0x60606062,
    0x93747979,
    0x60797979,
    0x63646464,
    0x65667E62,
    0x64646465,
    0x64656464,
    0x60646464,
    0x93937479,
    0x60797493,
    0x45524264,
    0x29293535,
    0x35353529,
    0x45573535,
    0x60644252,
    0x8D937479,
    0x79749390,
    0x35456460,
    0x9080808,
    0x8090909,
    0x35290808,
    0x64645245,
    0x8D937479,
    0x7493908E,
    0x35526079,
    0x8080829,
    0x9090909,
    0x29080808,
    0x60645257,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x29353545,
    0x8080829,
    0x35080808,
    0x79606545,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x35354545,
    0x52573535,
    0x90937964,
    0xA2D3D7D9,
};

GpImgRec D_actor_141000_8013D4DC[2] = {
    { 0, 0, { 0, 0, 14, 20 }, D_actor_141000_8013D2AC },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_141000_8013D4FC[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x7F7F6042,
    0x7F7F7F7F,
    0x57607F7F,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x6D6D5A89,
    0x6D6D6D6D,
    0x899D5A6D,
    0x52525289,
    0x64525252,
    0x42424264,
    0x6D604242,
    0x40401DD,
    0x4040404,
    0xDD010404,
    0x42646DDD,
    0x60606464,
    0x60426460,
    0xDD6D6065,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0x646DDDF6,
    0x79796064,
    0x65606464,
    0xF6DD6D60,
    0xF7F7F6F6,
    0xF7F7F7F7,
    0xF6F7F7F7,
    0x6DDDF6F6,
    0x79606079,
    0x65626464,
    0xF6F6DD6D,
    0xDDF7F7F6,
    0xDDDDDDDD,
    0xF7F7DDDD,
    0xDDF6F6F6,
    0x6060606D,
    0x60606464,
    0xF6F6DD6D,
    0xDDDDF6F6,
    0x6B6B6BDE,
    0xF6DDDDDE,
    0xDDF6F6F6,
    0x6060606D,
    0x60606460,
    0xF6F6DD6D,
    0x6BDDF6F6,
    0x6B6B6B6B,
    0xF6DD6B6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776460,
    0xF6F6DD6D,
    0xDEDDF6F6,
    0x6B6B6B6B,
    0xF6DDDE6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776060,
    0xF6DD6DD3,
    0xDDF6F6F6,
    0xDEDEDEDE,
    0xF6F6DDDE,
    0x6DDDF6F6,
    0x797764D3,
    0x45776079,
    0xDDD3D335,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xD3D3DDF6,
    0x93776435,
    0x64777779,
    0x77353545,
    0x5A8989D3,
    0x5A5A5A5A,
    0xD389895A,
    0x35356477,
    0x93776445,
    0x77777793,
    0x35456464,
    0x64353535,
    0x64646464,
    0x35356464,
    0x45453535,
    0x8D937764,
    0x77777790,
    0x64647777,
    0x9083545,
    0x8090909,
    0x35290808,
    0x77776464,
    0x8D937477,
    0x7777908E,
    0x77777777,
    0x77777777,
    0x77777777,
    0x77777777,
    0x60777777,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x52525252,
    0x52525252,
    0x52525252,
    0x79606552,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x52525245,
    0x79525252,
    0x90937979,
    0xA2D3D7D9,
};

GpImgRec D_actor_141000_8013D72C[2] = {
    { 0, 0, { 0, 0, 14, 20 }, D_actor_141000_8013D4FC },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

GpAnimSet* D_actor_141000_8013D74C[11] = {
    NULL,
    &D_actor_141000_8013A2C0,
    &D_actor_141000_8013B29C,
    &D_actor_141000_8013B458,
    &D_actor_141000_8013B620,
    &D_actor_141000_8013B8A8,
    &D_actor_141000_8013C030,
    &D_actor_141000_8013C268,
    &D_actor_141000_8013C41C,
    &D_actor_141000_8013C66C,
    &D_actor_141000_8013A6B8,
};

GpAnimSet** D_actor_141000_8013D778[1] = {
    D_actor_141000_8013D74C,
};

TaskDesc D_actor_141000_8013D77C = { 257, 192, func_actor_141000_801338C0, { .model = &D_actor_141000_8013A0B0 } };

Actor141000MsgEntry D_actor_141000_8013D788[7] = {
    { 2003, { .call0 = func_actor_141000_80133CD8 } },
    { 2004, { .call2 = func_actor_141000_80133E10 } },
    { 2005, { .call4 = func_actor_141000_80133E8C } },
    { 2013, { .call3 = func_actor_141000_801336DC } },
    { 2011, { .call1 = func_actor_141000_80133F6C } },
    { 2016, { .call4 = func_actor_141000_80133FA8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

static void func_actor_141000_80131E94(Task* arg0, Actor141000Point* arg1, s32 arg2);
static void func_actor_141000_801323F0(Task* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3);

/// Draws the sixteen gouraud quads, each followed by a semi-transparency
/// tpage change, at `arg2` depth minus 20.
static void func_actor_141000_80131E94(Task* arg0, Actor141000Point* arg1, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad  = D_actor_141000_801347C8[0];
    scale = ((Actor141000CtrlWork*)((Task*)arg0->spawnArg2.pointer)->work)->field_0;
    if (arg0->killCountdown >= 0x800) {
        arg0->killCountdown = 0;
    }
    rsin(arg0->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_actor_141000_80134808[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_actor_141000_80134808[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_actor_141000_80134808[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) - 20, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) - 20, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) - 20, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) - 20, tpage);
        quad += 4;
    }
}

static void func_actor_141000_801323F0(Task* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3)
{
    SVECTOR         a;
    SVECTOR         b;
    MATRIX          rot;
    Actor141000Proj proj[2];
    Task*           parent;
    MATRIX*         mtx;
    SVECTOR*        src;
    s16             t;
    s16             r;
    s32             scale;
    GpMtxWords*     words;
    s32             i;
    u16             f;
    s16             x0;
    s32             y0;
    s16             x1;
    s32             y1;
    s32             dx;
    s32             dy;

    parent = arg0->spawnArg2.pointer;
    mtx    = &parent->extra.tmd->coords->coord;
    f      = ((Actor141000CtrlWork*)parent->work)->field_0;
    a.vx   = D_actor_141000_80134868[0].vx;
    a.vy   = D_actor_141000_80134868[0].vy;
    a.vz   = D_actor_141000_80134868[0].vz;
    src    = &D_actor_141000_80134868[1];
    b.vx   = src->vx;
    b.vy   = src->vy;
    b.vz   = src->vz;
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_rtv0();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_rtv0();
    gte_stsv(&b);
    t     = (double)(s16)f;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&a);
    gte_rtps();
    gte_stsxy(&proj[0].sxy);
    gte_stdp(&proj[0].z);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    gte_ldv0(&b);
    gte_rtps();
    gte_stsxy(&proj[1].sxy);
    gte_stdp(&proj[0].z);
    gte_stflg(arg3);
    gte_stszotz(&proj[1].z);
    dy                           = proj[0].sxy.vy - proj[1].sxy.vy;
    x0                           = proj[0].sxy.vx;
    x1                           = proj[1].sxy.vx;
    dx                           = x1 - x0;
    y0                           = proj[0].sxy.vy;
    y1                           = proj[1].sxy.vy;
    i                            = ratan2(dx, dy);
    scale                        = gDisplayState.screenDistance;
    ((GpMtxWords*)&rot)->m00_m01 = 0x1000;
    ((GpMtxWords*)&rot)->m02_m10 = 0;
    words                        = (GpMtxWords*)&rot;
    words->m11_m12               = 0x1000;
    ((GpMtxWords*)&rot)->m20_m21 = 0;
    words->m22                   = 0x1000;
    RotMatrixZ(i, &rot);
    gte_SetRotMatrix(&rot);
    for (i = 0; i < 6; i++) {
        a.vx = D_actor_141000_80134878[i].vx * scale / *arg2;
        a.vy = D_actor_141000_80134878[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i].field_0 = b.vx + x0;
        arg1[i].field_2 = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = D_actor_141000_801348A8[i].vx * scale / proj[1].z;
        a.vy = D_actor_141000_801348A8[i].vy * scale / proj[1].z;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 6].field_0 = b.vx + x1;
        arg1[i + 6].field_2 = b.vy + y1;
    }
    r = 0x2000 - rsin(arg0->killCountdown);
    for (i = 0; i < 6; i++) {
        a.vx = ((D_actor_141000_80134878[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_actor_141000_80134878[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 12].field_0 = b.vx + x0;
        arg1[i + 12].field_2 = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_actor_141000_801348A8[i].vx * r) >> 12) * scale / proj[1].z;
        a.vy = ((D_actor_141000_801348A8[i].vy * r) >> 12) * scale / proj[1].z;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 18].field_0 = b.vx + x1;
        arg1[i + 18].field_2 = b.vy + y1;
    }
}

void func_actor_141000_80132C24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E30;
    sp.funcs[task->state](task);
}

/// Spawn state of the overlay's controller task: takes the display object's
/// root coordinate, allocates the work block the later states read through
/// `Task::work` and arms it at step 0xFFF, un-parks the model (`field_C` bit
/// 0x80 is the flag that keeps a `TmdObject` out of the coordinate update),
/// republishes that coordinate onto the two scale helpers, spawns the attach
/// task from `D_actor_141000_801348D8` and installs `func_actor_141000_80132E04`
/// as the exit callback before advancing to the per-frame state. A failed allocation kills
/// the task instead of leaving a half-built controller behind.
static void func_actor_141000_80132C7C(Task* task)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    GfxCoord*            coord;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_0 = 0xFFF;
    obj->flags   &= 0xFF7F;
    func_actor_141000_80132FD0(coord, 0);
    func_actor_141000_8013308C(coord, 0);
    Task_SpawnFromTable(&D_actor_141000_801348D8, 1, 0, task);
    task->exitCallback = func_actor_141000_80132E04;
    task->state       += 1;
}

/// Per-frame state of the overlay's controller task: copies the four animation
/// handlers onto the stack and runs the one the controller work block's `state`
/// halfword selects, sign-extended. A pending effect bit spawns the controller's
/// effect through the model's root coordinate, and the session's teardown flag
/// kills the task instead of letting it tick again.
static void func_actor_141000_80132D3C(Task* task)
{
    Actor141000CtrlWork* work;
    TaskFuncTable4       sp;

    work = (Actor141000CtrlWork*)task->work;
    sp   = D_actor_141000_80131E3C;
    sp.funcs[(s16)work->state](task);
    if (gDisplayState.animFrame & 1) {
        Gp_SpawnEff(0x60070, task->extra.tmd->coords, 0x24200, NULL);
    }
    if (gGameSession->viewReady != 0) {
        taskKill(task);
    }
}

/// `Task::exitCallback` the controller's spawn state installs: it only hands
/// the task to `taskKill`.
static void func_actor_141000_80132E04(Task* task)
{
    taskKill(task);
}

/// State 0 of the handler table at 0x80131E3C: ramps the actor's Z scale by
/// 1/16 a frame and, on reaching 1.0, clamps it there and advances the state
/// index `state` the dispatcher at 0x80132D3C walks.
static void func_actor_141000_80132E24(Task* arg0)
{
    Actor141000CtrlWork* work;
    u16                  scale;

    work        = (Actor141000CtrlWork*)arg0->work;
    scale       = work->scale + 0x100;
    work->scale = scale;
    if ((s16)scale >= 0x1000) {
        work->scale = 0x1000;
        work->state = work->state + 1;
    }
    func_actor_141000_80132FD0(arg0->extra.tmd->coords, 0);
    func_actor_141000_8013308C(arg0->extra.tmd->coords, (s16)work->scale);
}

/// State 1 of the handler table at 0x80131E3C: holds for 0x1F frames, then
/// advances the state index `state` the dispatcher at 0x80132D3C walks.
static void func_actor_141000_80132EB0(Task* arg0)
{
    Actor141000CtrlWork* work;
    u16                  ticks;

    work        = (Actor141000CtrlWork*)arg0->work;
    ticks       = work->ticks + 1;
    work->ticks = ticks;
    if ((s16)ticks >= 0x1F) {
        work->state = work->state + 1;
    }
}

/// State 2 of the handler table at 0x80131E3C: drives the model's rotation
/// through `func_actor_141000_80132FD0` and, on the frame that runs the ramp's
/// 0x5A entries out, advances the state index `state` the dispatcher at
/// 0x80132D3C walks. Every eighth frame it spawns another actor from index 2
/// of `D_actor_141000_801348D8` and copies this actor's world position onto
/// the new one.
static void func_actor_141000_80132EF4(Task* arg0)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    Task*                spawned;
    GfxCoord*            src;
    GfxCoord*            dst;
    u16                  frames;

    work         = (Actor141000CtrlWork*)arg0->work;
    obj          = arg0->extra.tmd;
    frames       = work->frames + 1;
    work->frames = frames;

    if (func_actor_141000_80132FD0(obj->coords, (s16)frames) != 0) {
        work->state = work->state + 1;
        return;
    }

    if (!(work->frames & 7)) {
        spawned = Task_SpawnFromTable(D_actor_141000_801348D8, 2, 0, 0);
        if (spawned != NULL) {
            src             = arg0->extra.tmd->coords;
            dst             = spawned->extra.tmd->coords;
            dst->coord.t[0] = src->coord.t[0];
            dst->coord.t[1] = src->coord.t[1];
            dst->coord.t[2] = src->coord.t[2];
        }
    }
}

static void func_actor_141000_80132FC8(Task* arg0)
{
}

/// Drives the model root one frame along the ramp the rotation table at
/// 0x80134228 and its position table at 0x801344F8 hold: splat an identity
/// matrix, let `RotMatrix` replace it with the frame's triple -- entry 0x59
/// once `arg1` runs past the table's 0x5A entries -- copy that entry's
/// position into the root's translation, drop X by 40 and clear `composeStamp`.
/// Returns non-zero on the frame that ran past the table, which is what the
/// state-2 handler at 0x80132EF4 advances `state` on.
static s32 func_actor_141000_80132FD0(GfxCoord* arg0, s32 arg1)
{
    GpMtxWords* words;
    SVECTOR*    pos;
    s32         idx;
    s32         ret;

    if (arg1 < 0x5A) {
        idx = arg1;
        ret = 0;
    } else {
        idx = 0x59;
        ret = 1;
    }
    words          = (GpMtxWords*)&arg0->coord;
    words->m00_m01 = 0x1000;
    words->m02_m10 = 0;
    words->m11_m12 = 0x1000;
    words->m20_m21 = 0;
    words->m22     = 0x1000;
    RotMatrix(&D_actor_141000_80134228[idx], &arg0->coord);
    pos                = &D_actor_141000_801344F8[idx];
    arg0->coord.t[0]   = pos->vx;
    arg0->coord.t[1]   = pos->vy;
    arg0->coord.t[2]   = pos->vz;
    arg0->coord.t[0]  -= 0x28;
    arg0->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}

static void func_actor_141000_8013308C(GfxCoord* arg0, s32 arg1)
{
    VECTOR scale;

    scale.vz = arg1;
    scale.vx = 0x1000;
    scale.vy = 0x1000;
    ScaleMatrix(&arg0->coord, &scale);
}

/// Callback of the model actor the controller's state 2 spawns every eighth
/// frame -- index 2 of `D_actor_141000_801348D8`, pointed at the controller's
/// own position. The first frame splats an identity matrix over the task's root
/// coordinate and clears its `composeStamp`: the rotation part only, so the translation
/// the spawner copied in survives. Every frame then runs the 5-frame countdown
/// in `killCountdown`, spawning effect 0x60070 from that same coordinate
/// (spawn arg 0x14200, no offset vector) each time it completes. The countdown
/// is held while the freeze byte is set, and a room change or a script event
/// taking over kills the task outright.
void func_actor_141000_801330C0(Task* arg0)
{
    GfxCoord*   coord;
    GpMtxWords* words;
    u16         count;

    coord = arg0->extra.tmd->coords;
    if (arg0->state == 0) {
        words               = (GpMtxWords*)&coord->coord;
        words->m00_m01      = 0x1000;
        words->m02_m10      = 0;
        words->m11_m12      = 0x1000;
        words->m20_m21      = 0;
        words->m22          = 0x1000;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state        += 1;
    }
    if (Gp_StateF0.field_4 == 0) {
        count               = arg0->killCountdown + 1;
        arg0->killCountdown = count;
        if ((s16)count >= 5) {
            arg0->killCountdown = 0;
            Gp_SpawnEff(0x60070, coord, 0x14200, NULL);
        }
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->evtSkipped != 0)) {
        taskKill(arg0);
    }
}

void func_actor_141000_801331AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E24;
    sp.funcs[task->state](task);
}

/// Chains this actor's root coordinate under the spawner's root coordinate,
/// hands the task to the spawner with `Task_Reparent`, arms `killCountdown` at
/// 0x7FF and advances the state.
static void func_actor_141000_80133204(Task* task)
{
    task->extra.tmd->coords->parent = ((Task*)task->spawnArg2.pointer)->extra.tmd->coords;
    Task_Reparent((Task*)task->spawnArg2.pointer, task);
    task->killCountdown = 0x7FF;
    task->state        += 1;
}

static void func_actor_141000_80133260(Task* arg0)
{
    Actor141000Point sp10[24];
    s32              spD0;
    s32              spD4;

    func_actor_141000_801323F0(arg0, sp10, &spD0, &spD4);
    func_actor_141000_80131E94(arg0, sp10, spD0);
}

/// Per-frame tick of the model actor: runs the motion handler `walk.motion` selects, steps the 16.16 accumulators
/// by `step` and moves the coordinate by their integer part, ticks the
/// animation slots and ground shadow while visible, runs the texture-upload
/// state, and counts `field_4C9` down to the buffer free.
static void func_actor_141000_801332A0(Task* task)
{
    TmdObject*       ext      = task->extra.tmd;
    Actor141000Work* work     = (Actor141000Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_141000_801339F8, func_actor_141000_80133A00 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[(s16)work->walk.motion](task);
    coord                = task->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    func_actor_141000_801335D4(task);
    if (work->field_4C9 >= 0) {
        if (work->field_4C9 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C9--;
    }
}

/// State 2 of the main-body table `D_actor_141000_80131E58`: the approach
/// test. Once the X/Z distance from the root coordinate to `target` stops
/// shrinking below `limit`, plays anim 0x7D3 with a preset carrying the
/// `model.nextAnimId` byte, clears `step` and advances the state; otherwise records
/// the distance as the new `limit`.
static void func_actor_141000_80133490(Task* arg0)
{
    Actor141000Work*     work;
    GfxCoord*            coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    AnimationPlayRequest preset;

    work  = (Actor141000Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_141000_80133CD8(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Texture-upload state of the enemy actor: runs the countdown at
/// `Actor141000Work::field_4C6` down one a frame while `field_4CA` names the
/// upload in progress, and on the frame it underflows posts that step's image
/// over the 0x19x0x14 rect at 0x10 -- reloading the countdown from `field_4C4`
/// and advancing `field_4CA` for steps 1 and 2, or clearing `field_4CA` and
/// starting over for step 3. Steps 1 and 2 share their whole tail, which is
/// what makes the compiler emit one copy of it that step 1 jumps into; step 3
/// only differs in clearing the step instead of advancing it.
static void func_actor_141000_801335D4(Task* arg0)
{
    Actor141000Work* work;
    RECT             rect;

    work   = (Actor141000Work*)((GameActor*)arg0->work);
    rect.x = 0;
    rect.y = 0x40;
    rect.w = 0x19;
    rect.h = 0x14;

    switch (work->field_4CA) {
        case 1:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013D28C[0], &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 2:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CE84[0], &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 3:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CA7C[0], &rect);
                work->field_4CA = 0;
            }
            break;
    }
}

/// Placement handler: stores the spawn position and rotation, resets the body
/// state, then applies a start preset exactly as `func_actor_141000_80133CD8`
/// does (inlined here). The default anim id is chosen by the `field_4C8`
/// variant; writing it as an if/else into the preset (not a ternary) is what
/// keeps CSE from reusing the earlier constant 1 for the `model.nextAnimId` store.
s32 func_actor_141000_801336DC(Task* task, s32 arg1, GpXformArg* place, Actor141000SpawnAnim* anim)
{
    Actor141000Work*      work;
    Actor141000Work*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                   = (Actor141000Work*)task->work;
    w->walk.motion      = 1;
    w->walk.motionStep  = 0;
    w->walk.target.vx   = place->pos.vx;
    w->walk.target.vy   = place->pos.vy;
    w->walk.target.vz   = place->pos.vz;
    w->walk.rotX        = place->rot.vx;
    w->walk.rotY        = place->rot.vy;
    w->walk.rotZ        = place->rot.vz;
    preset.source.index = 0;
    if (anim != NULL) {
        preset.animationId  = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        if (w->field_4C8 != 0) {
            preset.animationId = 2;
        } else {
            preset.animationId = 0xA;
        }
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor141000Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_141000_8013D778[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Per-frame dispatcher of the model actor: runs its spawn, tick or exit
/// state from `D_actor_141000_80131E4C`, skipping the frame while the global
/// freeze byte is set.
void func_actor_141000_801338C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E4C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit callback `func_actor_141000_801339BC`. An allocation failure ends the task
/// instead of leaving a half-built actor behind.
static void func_actor_141000_8013392C(Task* arg0)
{
    Actor141000Work* work;

    work = memCalloc(sizeof(Actor141000Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work          = (TaskIdMap*)work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->field_4C9     = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    func_actor_141000_801339DC(arg0);

    arg0->msgTable     = D_actor_141000_8013D788;
    arg0->exitCallback = func_actor_141000_801339BC;
    arg0->state++;
}

/// `Task::exitCallback` the model actor's spawn state installs: it only hands
/// the task to `Gp_EnemyTaskExit`.
static void func_actor_141000_801339BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

static void func_actor_141000_801339DC(Task* arg0)
{
    TmdObject*       ext;
    Actor141000Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor141000Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_141000_801339F8(Task* arg0)
{
}

/// Dispatches the actor's four main-body handlers by state.
static void func_actor_141000_80133A00(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor141000Work* work;

    work = (Actor141000Work*)arg0->work;
    sp   = D_actor_141000_80131E58;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// State 0 of the main-body table `D_actor_141000_80131E58`: turns the root
/// part to face `work->walk.target`, the position the placement handler stored.
/// Normalises the offset from the part's own translation, takes its yaw with
/// `ratan2` and rebuilds the local matrix from that yaw alone, then clears
/// `composeStamp` so the world matrix is recomputed and advances the state.
static void func_actor_141000_80133A68(Task* task)
{
    Actor141000Work* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor141000Work*)task->work;
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

/// State 1 of the actor's main-body table `D_actor_141000_80131E58`, the step
/// after the turn-to-face state. Rotates the constant
/// local-space offset `D_actor_141000_80131E68` through the root part's matrix
/// into `work->walk.step`, halving it first while `field_4C8` is clear -- the
/// variant `func_actor_141000_80133F6C` latches through message 0x7DB -- then
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances the state.
static void func_actor_141000_80133B28(Task* arg0)
{
    Actor141000Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = arg0->extra.tmd->coords;
    work  = (Actor141000Work*)arg0->work;

    vec = D_actor_141000_80131E68;
    if (work->field_4C8 == 0) {
        vec.vx >>= 1;
        vec.vy >>= 1;
        vec.vz >>= 1;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx   = 0x7FFF;
    work->walk.limit.vy   = 0x7FFF;
    work->walk.limit.vz   = 0x7FFF;
    work->walk.motionStep = work->walk.motionStep + 1;
}

/// State 3 of the main-body table `D_actor_141000_80131E58`, after the approach
/// test: turns the root part to the placement yaw. Euler-extracts the root coordinate into `vec`, and
/// while the yaw gap to the target `work->walk.rotY` is at least 0x41 it steps
/// `vec.vy` toward it by 0x40 -- the step is taken on an `s32` widening of the
/// extracted yaw, which the common tail adds to -- and otherwise snaps the yaw
/// to the target and plays anim 0x7D3 with a preset carrying the `model.nextAnimId`
/// byte, clearing the two body counters. Either way the root coordinate is
/// rebuilt as the identity matrix rotated by `vec`.
static void func_actor_141000_80133BD8(Task* arg0)
{
    Actor141000Work*     work;
    GpMtxWords*          words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor141000Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                      = work->walk.rotY;
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_141000_80133CD8(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. An unchanged clip skips playback setup.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_141000_80133CD8(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor141000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor141000Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_141000_8013D778[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the model at once. Writes the payload's
/// translation into the root coordinate, keeps its Euler angles in the
/// coordinate's `rot` slot and rebuilds the rotation from them, then clears
/// `composeStamp` so the world matrix is recomputed.
s32 func_actor_141000_80133E10(Task* task, s32 arg1, GpXformArg* args)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 shows the model and clears the 4 flag, 1 hides it, frees the aux
/// buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_4C9`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
s32 func_actor_141000_80133E8C(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor141000Work*)task->work)->field_4C9 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_141000_80133F6C(Task* task, s32 arg1, GpCmdArg* msg)
{
    Actor141000Work* work;

    work = (Actor141000Work*)task->work;
    switch (msg->command) {
        case 1:
            work->field_4C8 = 0;
            break;
        case 2:
            work->field_4C8 = 1;
            break;
    }
    return 0;
}

/// Image-load handler: picks one of the overlay's texture uploads by `mode`
/// and posts it through `Gp_LoadActorImage` over a scratch `RECT` -- the
/// 0x19x0x14 rect at (0, 0x40) for modes 0-3, the 0xEx0x14 rect at (0xC, 0x60)
/// for 4 and 5. Mode 3 also arms the work block's upload step and countdown
/// source. Unknown modes load nothing and return 0.
s32 func_actor_141000_80133FA8(Task* task, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret = 0;
    switch (mode) {
        case 0:
            img = &D_actor_141000_8013CA7C[0];
            goto small;
        case 1:
            img = &D_actor_141000_8013D28C[0];
            goto small;
        case 2:
            img = &D_actor_141000_8013D4DC[0];
        small:
            rect.y = 0x40;
            rect.w = 0x19;
            rect.x = 0;
            rect.h = 0x14;
            break;
        case 3:
            img                                       = &D_actor_141000_8013D72C[0];
            rect.y                                    = 0x40;
            rect.w                                    = 0x19;
            rect.x                                    = 0;
            rect.h                                    = 0x14;
            ((Actor141000Work*)task->work)->field_4CA = 1;
            ((Actor141000Work*)task->work)->field_4C4 = 1;
            break;
        case 4:
            img = &D_actor_141000_8013D72C[0];
            goto big;
        case 5:
            img = &D_actor_141000_8013D4DC[0];
        big:
            rect.x = 0xC;
            rect.y = 0x60;
            rect.w = 0xE;
            rect.h = 0x14;
            break;
        default:
            img = NULL;
            break;
    }
    if (img != NULL) {
        ret = Gp_LoadActorImage(task, img, &rect);
    }
    return ret;
}
