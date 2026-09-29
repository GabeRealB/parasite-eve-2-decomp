#include "common.h"
#include "rooms/mine_refuge.h"
#include "mapui/map_shelter.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/acropolis_square.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/area_transitions.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/item_menu.h"
#include "gameplay/object_task.h"
#include "gameplay/room_effects.h"
#include "gameplay/loading.h"
#include "gameplay/world_targets.h"

#include "gameplay/effects.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "gameplay/collision.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_mine_refuge_80182ADC[4];

/// Scratch block `func_mine_refuge_80180710` takes from `G_SCRATCH_HEAD`.
/// `otz`, `flag` and `sx`/`sy` receive the projection of the glow's centre;
/// `rOuter` and `rInner` are its two on-screen radii, derived from that `otz`.
/// Nothing in the function touches the leading bytes.
typedef struct {
    u8  _pad0[8];
    s32 otz;
    s32 flag;
    s32 rOuter;
    s32 rInner;
    u16 sx;
    u16 sy;
} MineRefugeGlowScratch;

STATIC_ASSERT_SIZEOF(MineRefugeGlowScratch, 0x1C);

extern UiObjectDesc D_800611E4;

extern TaskDesc       D_801358D8;

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `Mc_SaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
static const char D_mine_refuge_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
extern u8 D_mine_refuge_80181540[];
extern u8 D_mine_refuge_80181548[];
extern u8 D_mine_refuge_80181554[];
extern u8 D_mine_refuge_80181560[];

/// Row captions of the play-data panel, one per row.
extern u8 D_mine_refuge_80181568[];
extern u8 D_mine_refuge_80181598[];
extern u8 D_mine_refuge_80181570[];
extern u8 D_mine_refuge_80181574[];
extern u8 D_mine_refuge_8018157C[];
extern u8 D_mine_refuge_80181588[];
extern u8 D_mine_refuge_801815A0[];
extern u8 D_mine_refuge_801815A8[];
extern u8 D_mine_refuge_801815B0[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
extern u8 D_mine_refuge_801815B8[];

/// Suffix appended after a percentage.
extern u8 D_mine_refuge_801815C0[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
extern u8 D_mine_refuge_801815C4[];
extern u8 D_mine_refuge_801815F0[];
extern u8 D_mine_refuge_80181614[];
extern u8 D_mine_refuge_80181644[];
extern u8 D_mine_refuge_80181678[];
extern u8 D_mine_refuge_801816AC[];
extern u8 D_mine_refuge_801816E4[];
extern u8 D_mine_refuge_80181718[];
extern u8 D_mine_refuge_80181750[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
extern UiList       D_mine_refuge_8018178C;
extern UiList       D_mine_refuge_801817B4;
extern UiObjectDesc D_mine_refuge_801817D8;
extern UiObjectDesc D_mine_refuge_801817F4;
extern UiObjectDesc D_mine_refuge_80181810;
extern UiList       D_mine_refuge_8018183C;

/// Task table the cutscene and its sound task are spawned from.
extern TaskDesc D_mine_refuge_80181860[];

/// Message table of the room's message task.
extern GpMsgEntry D_mine_refuge_80181884[];

/// Task table of the room's two scripted sequences,
/// `func_mine_refuge_8017FA08` and `func_mine_refuge_8017FDBC`.
extern TaskDesc D_mine_refuge_801818B4[];

/// World-space anchors of the room's per-view glows: `D8` and `E0` are the two
/// drawn in view 2, `E0` again in view 6, and `E8` the one of views 3-5. `E0`
/// is reached both as `D8[1]` (view 2) and by its own name (view 6), and the
/// two forms are different code - indexing emits `D8+8`, naming emits its own
/// `lui` - so it keeps its own declaration.
extern SVECTOR D_mine_refuge_801818E8;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* D_mine_refuge_80182AD4;

/// Task `func_mine_refuge_8017FA08` spawns from `D_801358D8` and waits on;
/// message 0x13F1 is relayed to it while it exists.
extern Task* D_mine_refuge_80182AD8;

/// View saved when `func_mine_refuge_8017FC2C` forces view 6 for its scene,
/// restored when the scene ends.

/// Parameters of the cutscene `func_mine_refuge_8017FE78` starts.
extern RoomCutsceneRec D_mine_refuge_80182AE0;

static void func_mine_refuge_8017F460(Task* task);
static void func_mine_refuge_8017FE78(s32 arg0);
static void func_mine_refuge_8017FF4C(Task* task);
static void func_mine_refuge_8017FFAC(Task* task);

void func_mine_refuge_8017D6E0(UiList *, UiObject *);
void func_mine_refuge_8017DEAC(UiList *, UiObject *);
void func_mine_refuge_8017E8C4(Task *);
void func_mine_refuge_8017ED70(Task *);
void func_mine_refuge_8017EF30(Task *);
void func_mine_refuge_8017F124(UiList *, UiObject *);
void func_mine_refuge_8017F208(UiList *, UiObject *);
void func_mine_refuge_8017F2D0(UiList *, UiObject *);
void func_mine_refuge_8017F398(UiList *, UiObject *);
void func_mine_refuge_8017F49C(Task *);

void func_mine_refuge_8017F49C(Task *);
void func_mine_refuge_8017FB24(Task *);

extern GpGridParams D_mine_refuge_80181BA4[1];
extern GpObj4C D_mine_refuge_80182778[2];
extern GpObj4C D_mine_refuge_80182810[6];
extern GpRoomBoundVec D_mine_refuge_80182A58[8];
extern GpRoomCoordSet D_mine_refuge_80182760[1];
s32 func_mine_refuge_8017FBB4(Task *, s32, s32, s32);
s32 func_mine_refuge_8017FBE8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_mine_refuge_8017FC2C(Task *, s32, s32, GpMessageArg);
s32 func_mine_refuge_8017FCD0(Task *, s32, GpMsg13EF *, GpMessageArg);
s32 func_mine_refuge_8017FD48(Task *, s32, s32, s32);
void func_mine_refuge_8017FA08(Task *);
void func_mine_refuge_8017FDBC(Task *);

u8 D_mine_refuge_80181540[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_mine_refuge_80181548[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_mine_refuge_80181554[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_mine_refuge_80181560[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_mine_refuge_80181568[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_mine_refuge_80181570[4] = {
    87, 111, 110, 0,
};

u8 D_mine_refuge_80181574[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_mine_refuge_8018157C[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_mine_refuge_80181588[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_mine_refuge_80181598[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_mine_refuge_801815A0[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_mine_refuge_801815A8[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_mine_refuge_801815B0[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_mine_refuge_801815B8[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_mine_refuge_801815C0[4] = {
    37, 0, 0, 0,
};

u8 D_mine_refuge_801815C4[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_mine_refuge_801815F0[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_mine_refuge_80181614[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_mine_refuge_80181644[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_mine_refuge_80181678[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_mine_refuge_801816AC[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_mine_refuge_801816E4[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_mine_refuge_80181718[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_mine_refuge_80181750[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_mine_refuge_80181788[1] = {
    func_mine_refuge_8017D6E0,
};

UiList D_mine_refuge_8018178C = { D_mine_refuge_80181788, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_mine_refuge_801817B0[1] = {
    func_mine_refuge_8017DEAC,
};

UiList D_mine_refuge_801817B4 = { D_mine_refuge_801817B0, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_mine_refuge_801817D8 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_mine_refuge_8017ED70, 0 };

UiObjectDesc D_mine_refuge_801817F4 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_mine_refuge_8017EF30, 0 };

UiObjectDesc D_mine_refuge_80181810 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_mine_refuge_8017E8C4, 0 };

UiListItemFunc D_mine_refuge_8018182C[4] = {
    func_mine_refuge_8017F124,
    func_mine_refuge_8017F208,
    func_mine_refuge_8017F2D0,
    func_mine_refuge_8017F398,
};

UiList D_mine_refuge_8018183C = { D_mine_refuge_8018182C, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

void func_mine_refuge_8017F49C(Task *);
void func_mine_refuge_8017FB24(Task *);

TaskDesc D_mine_refuge_80181860[3] = {
    { 0, 32, func_mine_refuge_8017F49C, { .model = NULL } },
    { 0, 32, func_mine_refuge_8017FB24, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_mine_refuge_80181884[6] = {
    { 5102, func_mine_refuge_8017FBE8 },
    { 5105, func_mine_refuge_8017FBB4 },
    { 5103, func_mine_refuge_8017FCD0 },
    { 5104, func_mine_refuge_8017FC2C },
    { 5106, func_mine_refuge_8017FD48 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_mine_refuge_801818B4[3] = {
    { 0, 32, func_mine_refuge_8017FA08, { .model = NULL } },
    { 0, 31, func_mine_refuge_8017FDBC, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

SVECTOR D_mine_refuge_801818D8[2] = {
    { 1910, -1850, 990, 0 },
    { 769, -798, 2640, 0 },
};

SVECTOR D_mine_refuge_801818E8 = { 2731, -1261, 4325, 0 };

GpRoomCoordRec D_mine_refuge_801818F0[1] = {
    { D_mine_refuge_80182760, D_mine_refuge_80182A58 },
};

GpRoomObjRec D_mine_refuge_801818F8[1] = {
    { D_mine_refuge_80181BA4, D_mine_refuge_80182778, D_mine_refuge_80182810, NULL },
};

u8 * D_mine_refuge_80181908[1] = {
    D_8010CAF8,
};

GpViewCountRec D_mine_refuge_8018190C[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_mine_refuge_80181910[1] = {
    { { .words = { 0, 1472, 0, 288 } }, { 0, 0, 0, 0 }, { .words = { 0, 1472, 0, 288 } }, { 0, 0, 0, 0 }, 0x54060002, 0x54060001, 0, 2, 0, 0 },
};

SVECTOR D_mine_refuge_80181948[12] = {
    { 3031, 0, -2755, 0 },
    { 4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { -4096, 0, -37, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 3999, 0, 885, 0 },
    { 3545, 0, -2052, 0 },
    { 4088, 0, 264, 0 },
    { -4096, 0, 0, 0 },
    { -475, 0, 4068, 0 },
    { -2524, 0, -3226, 0 },
};

SVECTOR D_mine_refuge_801819A8[34] = {
    { 780, 50, 3000, 0 },
    { 780, -500, 3000, 0 },
    { 980, -500, 3220, 0 },
    { 980, 50, 3220, 0 },
    { 980, -500, 5050, 0 },
    { 980, 50, 5050, 0 },
    { -200, 0, 5100, 0 },
    { 3200, 0, 5100, 0 },
    { 3200, 0, -200, 0 },
    { -200, 0, -200, 0 },
    { 2370, -3000, 2780, 0 },
    { 2370, 50, 2780, 0 },
    { 2350, 50, 4970, 0 },
    { 2350, -3000, 4970, 0 },
    { 450, 50, 4970, 0 },
    { 450, -3000, 4970, 0 },
    { 450, 50, 3090, 0 },
    { 450, -3000, 3090, 0 },
    { 870, 50, 3090, 0 },
    { 870, -3000, 3090, 0 },
    { 1120, 50, 1960, 0 },
    { 1120, -3000, 1960, 0 },
    { 900, 50, 1580, 0 },
    { 900, -3000, 1580, 0 },
    { 1000, 50, 30, 0 },
    { 1000, -3000, 30, 0 },
    { 2000, 50, 30, 0 },
    { 2000, -3000, 30, 0 },
    { 2000, 50, 1120, 0 },
    { 2000, -3000, 1120, 0 },
    { 2600, 50, 1190, 0 },
    { 2600, -3000, 1190, 0 },
    { 2600, 50, 2600, 0 },
    { 2600, -3000, 2600, 0 },
};

GpGridFace D_mine_refuge_80181AB8[15] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 7, 8, 6, 9 }, 2, 1 },
    { { 11, 12, 10, 13 }, 3, 0 },
    { { 12, 14, 13, 15 }, 4, 0 },
    { { 14, 16, 15, 17 }, 1, 0 },
    { { 16, 18, 17, 19 }, 5, 0 },
    { { 18, 20, 19, 21 }, 6, 0 },
    { { 20, 22, 21, 23 }, 7, 0 },
    { { 22, 24, 23, 25 }, 8, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 26, 28, 27, 29 }, 9, 0 },
    { { 28, 30, 29, 31 }, 10, 0 },
    { { 30, 32, 31, 33 }, 9, 0 },
    { { 32, 11, 33, 10 }, 11, 0 },
};

s16 D_mine_refuge_80181B6C[15] = {
    0,
    1,
    2,
    3,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    -1,
};

s16 D_mine_refuge_80181B8C[7] = {
    0,
    1,
    2,
    3,
    4,
    5,
    -1,
};

s16 * D_mine_refuge_80181B9C[2] = {
    D_mine_refuge_80181B6C,
    D_mine_refuge_80181B8C,
};

GpGridParams D_mine_refuge_80181BA4[1] = {
    { NULL, D_mine_refuge_80181948, D_mine_refuge_801819A8, D_mine_refuge_80181AB8, D_mine_refuge_80181B9C, 200, 200, 1, 2, 4000, 15 },
};

GpViewRec D_mine_refuge_80181BC8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -1500, 8300, -2500 } }, 289 },
    { { { { -3873, 0, -1331 }, { -400, 3906, 1165 }, { 1269, 1232, -3693 } }, { -550, 1610, -4900 } }, 269 },
    { { { { 3915, 0, -1201 }, { -777, 3121, -2535 }, { 915, 2651, 2984 } }, { -1005, 2661, -1831 } }, 246 },
    { { { { 0, 0, -4096 }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { 0, 0, -4096 }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { -1156, 0, 3929 }, { 2912, 2750, 856 }, { -2638, 3035, -776 } }, { -1291, 1474, -2830 } }, 629 },
    { { { { 2270, 0, 3408 }, { 2132, 3195, -1420 }, { -2659, 2562, 1771 } }, { -2075, 1908, -3789 } }, 598 },
};

GpSprtCmd D_mine_refuge_80181CC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_refuge_80181CD4[79] = {
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 32, 487, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, 48, 524, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, 48, 503, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, 88, 549, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 88, 527, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 24, 541, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 24, 541, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 16, 582, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 16, 582, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 8, 620, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 8, 630, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 0, 675, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 0, 686, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -16, 834, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 834, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -8, 753, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -8, 753, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -24, 891, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -24, 891, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -16, 703, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 719, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -16, 675, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 675, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 0, 626, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 0, 653, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 24, 751, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 48, 688, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 696, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -32, 754, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 32, 0, 771, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 761, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 80, 484, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 88, 467, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 442, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 104, 426, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 96, 425, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 96, 449, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 420, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 112, 420, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 8, 593, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -144, 56, 618, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 554, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 56, 564, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -40, 760, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 809, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 971, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 965, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -8, 812, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 1005, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 8, 988, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 803, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -104, 669, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -104, 661, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -88, 693, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -64, 716, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -32, 894, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -56, 696, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -24, 711, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -104, 705, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -96, 783, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -96, 880, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -64, 911, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -88, 916, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 945, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 761, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 997, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -40, 844, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, -40, 786, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 941, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 8, 984, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, -16, 900, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -8, 998, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 786, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -8, 812, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 916, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 24, 838, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 815, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 861, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_refuge_80182300[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 9, 0, 0, { 0, 0 } },
    { 28, 3, 0, 0, { 5, 0 } },
    { 31, 8, 0, 0, { 1, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, 36, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_refuge_80182340[33] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 104, 482, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 80, 513, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 495, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 489, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 447, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -16, 896, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 849, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 0, 850, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 16, 773, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 16, 777, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 32, 724, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 675, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 64, 641, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 88, 600, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 88, 660, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 32, 725, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 48, 684, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 64, 642, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 608, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -8, 883, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, -32, 833, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 817, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -24, 798, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 766, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -16, 795, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -16, 865, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -8, 839, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 772, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -8, 812, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 0, 810, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 0, 807, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_refuge_801825D4[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 1, 0, 0, { 3, 0 } },
    { 6, 15, 0, 0, { 2, 0 } },
    { 21, 12, 0, 0, { 4, 0 } },
    { 33, 0, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_refuge_8018260C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_refuge_8018261C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_refuge_8018262C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_refuge_8018263C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_mine_refuge_8018264C[7] = {
    { { .empty = D_mine_refuge_80181CC4 }, D_mine_refuge_80181CC4, NULL },
    { { .elements = D_mine_refuge_80181CD4 }, D_mine_refuge_80182300, NULL },
    { { .elements = D_mine_refuge_80182340 }, D_mine_refuge_801825D4, NULL },
    { { .empty = D_mine_refuge_8018260C }, D_mine_refuge_8018260C, NULL },
    { { .empty = D_mine_refuge_8018261C }, D_mine_refuge_8018261C, NULL },
    { { .empty = D_mine_refuge_8018262C }, D_mine_refuge_8018262C, NULL },
    { { .empty = D_mine_refuge_8018263C }, D_mine_refuge_8018263C, NULL },
};

GpPointLight D_mine_refuge_801826A0[2] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2128, -1869, 1212 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3342, 2588, { 0, 0 } }, 0, 2200 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1562, -2170, 3664 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 0, 3000 },
};

GpRoomCoordSet D_mine_refuge_80182760[1] = {
    { 0, NULL, 2, D_mine_refuge_801826A0, 0, NULL },
};

GpObj4C D_mine_refuge_80182778[2] = {
    { NULL, NULL, NULL, { 2048, -1600, 2790, 0 }, { { -1888, -1904, 176, 0 }, { 1888, -1904, -176, 0 }, { -1888, 1904, 176, 0 }, { 1888, 1904, -176, 0 } }, { -382, 0, -4086, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 2017, -1568, 2734, 0 }, { { 1792, -1904, -176, 0 }, { -1792, -1904, 176, 0 }, { 1792, 1904, -176, 0 }, { -1792, 1904, 176, 0 } }, { 399, 0, 4075, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 3, 2, 129, 0 },
};

GpObj4C D_mine_refuge_80182810[6] = {
    { NULL, NULL, NULL, { 1504, -48, 208, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 701, 0, 5, 18, 2, 0 },
    { NULL, NULL, NULL, { 2368, -64, 4257, 0 }, { { -448, 0, -880, 0 }, { 448, 0, -880, 0 }, { -448, 0, 880, 0 }, { 448, 0, 880, 0 } }, { 0, 4105, 0, 0 }, { -4091, 0, -201, 0 }, 987, 5, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1024, -64, 4544, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 701, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 1024, -64, 2416, 0 }, { { -576, 0, -576, 0 }, { 576, 0, -576, 0 }, { -576, 0, 576, 0 }, { 576, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 4095, 0, 0, 0 }, 814, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 2592, -64, 1808, 0 }, { { -576, 0, -704, 0 }, { 576, 0, -704, 0 }, { -576, 0, 704, 0 }, { 576, 0, 704, 0 } }, { 0, 4097, 0, 0 }, { -4095, 0, 0, 0 }, 909, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 2464, -64, 2944, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, 2, 11, 0, 130, 0 },
};

GpAreaTmdRec D_mine_refuge_801829D8[2] = {
    { 101, 481, 4, 0, { 0, 0 }, &D_801358D8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_mine_refuge_801829F0[1] = {
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_mine_refuge_80182A00[11] = {
    { NULL, NULL },
    { D_mine_refuge_801829F0, D_mine_refuge_801829D8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpRoomBoundVec D_mine_refuge_80182A58[8] = {
    { 7, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 520, 520, 520, 520 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_mine_refuge_80182A98[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_mine_refuge_80182AA4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_mine_refuge_80182AAC[1] = {
    { 0, 0, 1, 0, D_mine_refuge_80182A98 },
};

GpRoomParamRec * D_mine_refuge_80182AB4[8] = {
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AAC,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
};

Task * D_mine_refuge_80182AD4 = NULL;

Task * D_mine_refuge_80182AD8 = NULL;

u8 D_mine_refuge_80182ADC[4] = {
    0,
    47,
    36,
    50,
};

RoomCutsceneRec D_mine_refuge_80182AE0 = { 0 };

/// Draws one row of the play-data panel, the row picked by
/// `UiList::field_8`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
void func_mine_refuge_8017D6E0(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_mine_refuge_801815C4,
                D_mine_refuge_801815F0,
                D_mine_refuge_80181614,
                D_mine_refuge_80181644,
                D_mine_refuge_80181678,
                D_mine_refuge_801816AC,
                D_mine_refuge_801816E4,
                D_mine_refuge_80181718,
                D_mine_refuge_80181750,
            };

            Ui_SetHolderParam(tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_80181568);
            Text_FormatTime(p, Mc_SaveData[0].state.playTime);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_80181598);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_mine_refuge_801815B8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_80181570);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_mine_refuge_801815B8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_80181574);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_mine_refuge_801815B8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_8018157C);
            if (Mc_SaveData[0].state.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData[0].state.field_6CC * 10000) / (Mc_SaveData[0].state.field_6CC + Mc_SaveData[0].state.field_6CE);
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_mine_refuge_801815C0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = Mc_SaveData[0].state.field_6CC;
            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_80181588);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_mine_refuge_801815C0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar(&(arg1)->panel, arg1->panel.field_1C.s, (s16)arg1->panel.field_1E.u, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_801815A0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_mine_refuge_801815B8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_801815A8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_92C), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_mine_refuge_801815B0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data panel.
static const char D_mine_refuge_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
static const u8 D_mine_refuge_8017D61C[] = "100.0%";

/// Draws one row of an item-usage panel from the `RoomItemUsage` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `barWidths` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
void func_mine_refuge_8017DEAC(UiList* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->panel.field_8 != 5) {
        req.x          = arg1->panel.field_20.u + 0x11 + x;
        textY          = arg1->panel.field_22.u - 6;
        req.y          = textY + y;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        Text_DrawString(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_mine_refuge_8017D61C, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            Text_ItoaPadded(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
        }
        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        Text_Strcat(p, D_mine_refuge_801815C0);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->panel.field_1C.s + 0x80;
    avail = (s16)arg1->panel.field_1E.u - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                     = (POLY_G4*)gGpuPrimCursor;
        tx                       = arg1->panel.field_20.u + barX + 1;
        prim->x2                 = tx;
        prim->x0                 = tx;
        ty                       = arg1->panel.field_22.u;
        gGpuPrimCursor           = prim + 1;
        ty                       = ty + rowY;
        ty                      += 1;
        PRIM_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        PRIM_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        tx                       = (u16)prim->x0 + barW - 1;
        prim->y1                 = ty;
        prim->y0                 = ty;
        ty                      += 8;
        prim->y3                 = ty;
        prim->y2                 = ty;
        prim->x3                 = tx;
        prim->x1                 = tx;
        addPrim(gGpuCurrentOt + arg1->panel.field_14.s + 1, prim);
    }
    one = 1;
    Ui_DrawBeveledRect(&(arg1)->panel, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->panel.field_0.w >> 16) == one) || (arg1->panel.field_0.w == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`Mc_SaveData[0].state.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
static void func_mine_refuge_8017E2A8(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData[0].state.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData[0].state.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Fills the "Play Data" PE-usage panel's `RoomPeUsage` block from the
/// save's per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData[0].state.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData[0].state.attachUseCounts` is appended and its counter summed. Levels
/// are addressed by page and column, with three slots per page. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void func_mine_refuge_8017E5A4(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;

    for (; i < 12; i++) {
        s32 useCount;

        useCount = Mc_SaveData[0].state.attachUseCounts[i];
        id       = i * 3 + 0xF;
        if (useCount > 0) {
            s32 page;
            s32 column;

            page   = i / 3;
            column = i % 3;
            *p     = id;
            if (Mc_SaveData[0].state.attachLevels[column + page * 3] != 0) {
                *p = id + (Mc_SaveData[0].state.attachLevels[column + page * 3] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData[0].state.attachUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData[0].state.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData[0].state.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData[0].state.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData[0].state.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData[0].state.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Titles of the usage panel: weapons, then Parasite Energy.
static const char D_mine_refuge_8017D624[] = "Weapon Data";
static const char D_mine_refuge_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
void func_mine_refuge_8017E8C4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_mine_refuge_801817B4;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_mine_refuge_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_mine_refuge_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_mine_refuge_801817D8, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_mine_refuge_8017E2A8(list, obj);
        } else {
            func_mine_refuge_8017E5A4(list, obj);
        }
        Ui_InitList(list, &(obj)->panel);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->panel.field_0.w = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_mine_refuge_8017D638[12] = "Telephone\0\x1A\x1C";

/// Task body of the telephone menu. Until the save has a clear or has reached
/// demo scene 1 it spawns `D_800611E4` in place of the list; otherwise it lays
/// out and updates the list. When the first child window finishes, the menu
/// opens the item prompt its selection picks, or closes.
void func_mine_refuge_8017EA78(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    ready         = Mc_SaveData[0].state.demoScene == 1;
    list          = &D_mine_refuge_8018183C;
    one           = 1;
    if (Mc_SaveData[0].state.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->panel.field_0.w = 0;
            obj->panel.field_4  |= 0x80000000;
            task->state          = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.field_0.w = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText(&(obj)->panel, D_mine_refuge_8017D638);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4(obj, task);
        obj->panel.field_0.w = 0;
    }
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim(&(obj)->panel, task);
                    obj->panel.field_0.w = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}

/// Task body of a prompt window: on its first frame it becomes the UI holder and
/// installs `func_mine_refuge_8017F460` as its exit callback; every
/// frame it draws the prompt lines.
void func_mine_refuge_8017ED70(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_mine_refuge_8017F460;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_mine_refuge_8017EDCC(u8* str, s32 decimals)
{
    s32 len = 0;
    s32 i;

    if (decimals > 0) {
        while (*str != 0) {
            str++;
            len++;
        }
        if (len < decimals) {
            decimals = len;
        }
        decimals++;
        for (i = 0; i < decimals; i++) {
            str[1] = str[0];
            str--;
        }
        str[1] = '.';
    }
}

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
static u8* func_mine_refuge_8017EE3C(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        Text_ItoaPadded(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_mine_refuge_801815C0);
    return buf;
}

/// Task body of the play-data panel: on its first frame it spawns
/// `D_mine_refuge_801817D8` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_mine_refuge_8017EF30(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_mine_refuge_8018178C;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_mine_refuge_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_mine_refuge_801817D8, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.bounds.unsignedRect.h += 5;
        list->field_A                     = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
static void func_mine_refuge_8017F020(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s16      y;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim     = (POLY_G4*)gGpuPrimCursor;
        x        = arg0->field_20.u + arg1 + 1;
        prim->x0 = prim->x2      = x;
        y                        = arg0->field_22.u;
        gGpuPrimCursor           = prim + 1;
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setPolyG4(prim);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                        = y + arg2 + 1;
        x                        = prim->x0 + arg3 - 1;
        prim->y0 = prim->y1 = y;
        prim->x1 = prim->x3 = x;
        y                   = y + arg4 - 1;
        prim->y2 = prim->y3 = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14.u + 1, prim);
    }
}

/// The telephone menu's "Save" row: confirmed while the CD is idle, it spawns
/// `D_800611E4` and moves the owning task to state 1.
void func_mine_refuge_8017F124(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181540, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        gDisplayState.gameMode = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->panel.field_0.w = 0;
        obj->field_2E        = 6;
        obj->owner->state    = sel;
    }
}

/// The telephone menu's "Play Data" row: confirmed, it opens
/// `D_mine_refuge_801817F4` and moves the owning task to state 2.
void func_mine_refuge_8017F208(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181548, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_801817F4, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_mine_refuge_80181810` for weapons and moves the owning task to
/// state 2.
void func_mine_refuge_8017F2D0(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181554, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_80181810, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_mine_refuge_80181810` for Parasite Energy and moves the owning
/// task to state 2.
void func_mine_refuge_8017F398(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181560, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_80181810, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
static void func_mine_refuge_8017F460(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_mine_refuge_8017F49C(Task* task)
{
    s32              poll;
    s32              a0;
    s32              a1;
    s32              flag;
    RoomCutsceneRec* script;
    McSaveData*      save;

    script = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_mine_refuge_80182AD4 = NULL;
            Gp_MsgPlayerWeapon(0);
            save = &Mc_SaveData[0];
            if (save->state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (script->field_0 > 0) {
                D_80115694         = save->state.at4.loc.view;
                save->state.at4.loc.view = (u8)script->field_0;
            } else {
                D_80115694 = -script->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (script->field_4 != 0) {
                SndEvt_EnqueueType6(script->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (script->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(script->field_3);
                a0 = script->field_14;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->field_16;
                }
                func_800E6D4C(a0, a1);
            }
            if (script->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_mine_refuge_80182AD4 =
                Task_SpawnFromTable(D_mine_refuge_80181860, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_mine_refuge_80182AD4);
                task->state++;
            } else if (Task_PollKill(D_mine_refuge_80182AD4, &poll) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (script->field_2 == 0) {
                SndEvt_EnqueueType6(script->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (script->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(script->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(script->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (script->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// States of the room's message task, run by `func_mine_refuge_8017FFBC`:
/// install the message table, idle, die.
static const TaskFuncTable3 D_mine_refuge_8017D6A4 = {
    {
        func_mine_refuge_8017FF4C,
        func_mine_refuge_8017FFAC,
        taskKill,
    },
};

void func_mine_refuge_8017FA08(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x166) == 1) {
                GameFlag_SetNibble(0x166, 2);
                Gp_RunCapCmd1(0xF);
            }
            task->state = task->state + 1;
            return;
        case 2:
            SndEvt_EnqueueType6(0x54060007, 0, 0);
            D_mine_refuge_80182AD8 = Task_SpawnFromTable(&D_801358D8, 0, 0, 0);
            task->state            = task->state + 1;
            return;
        case 3:
            if (Task_PollKill(D_mine_refuge_80182AD8, &sp10) != 0) {
                D_mine_refuge_80182AD8 = NULL;
                task->state            = task->state + 1;
            }
            return;
        case 1:
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SndEvt_EnqueueType6(0x54060008, 0, 0);
            taskKill(task);
            break;
    }
}

/// Plays the sound event passed in `spawnArg2` on frames 0 and 0x50 of the
/// task's life and kills the task at frame 0x78.
void func_mine_refuge_8017FB24(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

/// The `0x13F1` message handler of `D_mine_refuge_80181884`: relays the
/// message unchanged to `D_mine_refuge_80182AD8` and returns its answer, or 0
/// while that task does not exist.
s32 func_mine_refuge_8017FBB4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 ret;

    if (D_mine_refuge_80182AD8 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(D_mine_refuge_80182AD8, msgId, arg2, arg3);
    }
    return ret;
}

/// A handler of the room's message table: copies the incoming record onto the
/// outgoing one, hands both to `func_map_shelter_80179A04` and returns 1.
s32 func_mine_refuge_8017FBE8(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_mine_refuge_8017FC2C(Task* task, s32 msgId, s32 arg2, GpMessageArg arg3)
{
    u8 temp_a3;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12B) != 0) {
            func_mine_refuge_8017FE78(0U);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            temp_a3                     = Mc_SaveData[0].state.at4.loc.view;
            Mc_SaveData[0].state.at4.loc.view = 6U;
            D_mine_refuge_80182ADC[0]      = temp_a3;
            SndEvt_EnqueueType6(0x54060003, 0, 0);
            Gp_RunCapCmd(0xD, 0);
            Task_SpawnFromTable(D_mine_refuge_801818B4, 1, 0, 0);
        }
    }
    return 0;
}

s32 func_mine_refuge_8017FCD0(Task* task, s32 msgId, GpMsg13EF * arg2, GpMessageArg arg3)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1) {
        if (GameFlag_GetNibble(0xBB) != temp_s0) {
            GameFlag_SetNibble(0xC4, 0);
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_mine_refuge_801818B4, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

/// The `0x13F2` message handler of `D_mine_refuge_80181884`: arguments 0xC, 0x63
/// and 0x67 each cue a sound (ids 0x5406000C, 0x5406000F and 0x5406000D),
/// centred and at zero depth. Any other argument is ignored.
s32 func_mine_refuge_8017FD48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xC:
            SndEvt_EnqueueType6(0x5406000C, 0, 0);
            break;
        case 0x63:
            SndEvt_EnqueueType6(0x5406000F, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x5406000D, 0, 0);
            break;
    }
    return 0;
}

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            Mc_SaveData[0].state.at4.loc.view = D_mine_refuge_80182ADC[0];
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 1:
            GameFlag_SetNibble(0x12B, 1);
            func_mine_refuge_8017FE78(D_mine_refuge_80182ADC[0]);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// Fills `D_mine_refuge_80182AE0` and spawns the cutscene task with it. A
/// non-zero `arg0` is the view restored when the scene ends, with no opening
/// sound; zero forces view 6 for the scene and opens with sound 0x54060003.
/// Progress nibble 0x155 picks the scene: when it is 0xF, CAP slot 0xE with no
/// file and CAP command 1 afterwards; otherwise CAP slot 1 from file 1 and
/// command 5 afterwards.
static void func_mine_refuge_8017FE78(s32 arg0)
{
    s32 slot;

    if (arg0 != 0) {
        D_mine_refuge_80182AE0.field_4 = 0;
        D_mine_refuge_80182AE0.field_0 = -arg0;
    } else {
        D_mine_refuge_80182AE0.field_0 = 6;
        D_mine_refuge_80182AE0.field_4 = 0x54060003;
    }
    if (GameFlag_GetNibble(0x155) == 0xF) {
        slot                           = 1;
        D_mine_refuge_80182AE0.field_1 = 0xE;
        D_mine_refuge_80182AE0.field_3 = 0;
    } else {
        slot                           = 5;
        D_mine_refuge_80182AE0.field_1 = 1;
        D_mine_refuge_80182AE0.field_3 = 1;
    }
    D_mine_refuge_80182AE0.field_2  = 0;
    D_mine_refuge_80182AE0.field_8  = 0x54060006;
    D_mine_refuge_80182AE0.field_10 = 0x54060004;
    D_mine_refuge_80182AE0.field_C  = 0x54060005;
    Task_SpawnFromTable(D_mine_refuge_80181860, 0, slot, &D_mine_refuge_80182AE0);
}

static void func_mine_refuge_8017FF4C(Task* arg0)
{
    arg0->msgTable = D_mine_refuge_80181884;
    Game_SetPtrSlot(arg0, 7);
    D_mine_refuge_80182AD8 = NULL;
    gStageSceneMusicEntry  = 1;
    arg0->state            = arg0->state + 1;
    D_80115598             = 1;
}

/// Idle state of the room's message task: does nothing.
static void func_mine_refuge_8017FFAC(Task* task)
{
    char pad[0x10];
}

/// Runs the handler for the task's current state, from a local copy of
/// `D_mine_refuge_8017D6A4`.
void func_mine_refuge_8017FFBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_refuge_8017D6A4;
    sp.funcs[task->state](task);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on the projected point (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`).
/// `arg1` also picks the 40-texel-wide texture column `(s16)arg1 * 40`, rows
/// 0..0x27. The sprite's on-screen half-extent is `(s16)arg2 * 39 / otz`, and
/// its grey level alternates between 0x20 and 0x30 with the frame counter.
/// A 0x10-byte scratch block is taken from `G_SCRATCH_HEAD` and returned.
static void func_mine_refuge_80180014(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 32 / otz`. The lit vertex pulses on green and blue at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78`. A 0x18-byte scratch block in the
/// `GpRingScratch` layout is taken from `G_SCRATCH_HEAD` and returned.
static void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    LINE_G3*       line;
    s32            sine;
    s32            pulse;
    s32            radius;
    s32            i;
    s32            t1;
    s32            t2;
    s32            twice;
    u16            sx;
    u16            sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (GpRingScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        sine        = rsin(gDisplayState.animFrame * (s16)arg1);
        radius      = ((s16)arg2 * 32) / block->otz;
        i           = 0;
        pulse       = sine / 34 + 0x78;
        block->step = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->step;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->step;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->step) + (block->step * twice);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->step * t1);
            line->y0 = block->sy - (block->step * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->step * t1);
            line->y2 = block->sy + (block->step * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-wedge disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then four
/// wedges reaching between that radius and an inner one of `(s16)arg2 * 8 /
/// otz`. Only the centre vertex is lit, on green and blue, with a level of
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses; the half-radius
/// copies take that level and every other wedge half of it. The scratch block is returned to
/// `G_SCRATCH_HEAD` on exit.
static void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                 scratch;
    u8*                    head;
    MineRefugeGlowScratch* block;
    POLY_G4*               prim;
    s32                    pulse;
    s32                    color;
    s32                    half;
    s32                    size;
    s32                    ang;
    s32                    t;
    s32                    t2;
    s32                    u;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (MineRefugeGlowScratch*)(*scratch = head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((MineRefugeGlowScratch*)(head - 0x1C))->sx);
    gte_stflg(&((MineRefugeGlowScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((MineRefugeGlowScratch*)(head - 0x1C))->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / block->otz;
        block->rInner = (size * 8) / block->otz;
        color         = pulse / 34 + 0x78;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues three concentric rings of eight
/// gouraud `POLY_G4` wedges around the projected centre. The first ring's
/// radius is `(s16)arg1 * 64 / otz`; each later ring doubles it and halves the
/// centre colour. `arg2` packs three RGB nibbles for the centre vertex, each
/// offset by `(animFrame & 1) << 5` so the glow flickers on alternate frames.
/// Unlike the room's other draws it never returns its 0x10-byte scratch block
/// to `G_SCRATCH_HEAD`.
static void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ring;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (RoomDraw13Scratch*)(*scratch = head - 0x10);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ring          = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) << 5;
        packed        = arg2 << 16;
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            ang = 0;
            do {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
                t        = ang + 0x100;
                prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
                prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
                prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
                t2       = ang + 0x200;
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
                prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
                ang      = t2;
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            } while (ang < 0x1000);
            r              = (u8)r >> 1;
            g              = (u8)g >> 1;
            b              = (u8)b >> 1;
            block->radius *= 2;
            ring++;
        } while (ring < 3);
    }
}

/// Draws the room's glows for whichever view is current. View 2 draws a
/// sprite and a diamond; views 3 and 4/5 draw rings at one anchor, but only
/// while progress nibble 0xC3 is 1; view 6 draws a pulsing disc.
void func_mine_refuge_80181454(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_mine_refuge_80180014(&D_mine_refuge_801818D8[0], 1, 0x300);
            func_mine_refuge_8018029C(&D_mine_refuge_801818D8[1], 0x60, 0x40);
            break;
        case 3:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            func_mine_refuge_80180710(&D_mine_refuge_801818D8[1], 0x60, 0x80);
            break;
    }
}
