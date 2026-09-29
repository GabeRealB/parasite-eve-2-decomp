#include "common.h"
#include "rooms/dryfield_motel_lobby.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/item_menu.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "gameplay/collision.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

extern UiObjectDesc D_800611E4;

/// Labels the four menu-entry handlers `func_dryfield_motel_lobby_8017F094` to
/// `func_dryfield_motel_lobby_8017F308` draw: "Save", "Play Data", "Weapon
/// Data" and "PE Data".
extern u8 D_dryfield_motel_lobby_8017F4F0[];
extern u8 D_dryfield_motel_lobby_8017F4F8[];
extern u8 D_dryfield_motel_lobby_8017F504[];
extern u8 D_dryfield_motel_lobby_8017F510[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_motel_lobby_8017D650` draws.
extern u8 D_dryfield_motel_lobby_8017F518[];
extern u8 D_dryfield_motel_lobby_8017F548[];
extern u8 D_dryfield_motel_lobby_8017F520[];
extern u8 D_dryfield_motel_lobby_8017F524[];
extern u8 D_dryfield_motel_lobby_8017F52C[];
extern u8 D_dryfield_motel_lobby_8017F538[];
extern u8 D_dryfield_motel_lobby_8017F550[];
extern u8 D_dryfield_motel_lobby_8017F558[];
extern u8 D_dryfield_motel_lobby_8017F560[];

/// The " times" suffix appended to that panel's count rows.
extern u8 D_dryfield_motel_lobby_8017F568[];

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_motel_lobby_8017F570[];

/// Help texts of the panel's nine rows, handed to the UI holder for the
/// selected row.
extern u8 D_dryfield_motel_lobby_8017F574[];
extern u8 D_dryfield_motel_lobby_8017F5A0[];
extern u8 D_dryfield_motel_lobby_8017F5C4[];
extern u8 D_dryfield_motel_lobby_8017F5F4[];
extern u8 D_dryfield_motel_lobby_8017F628[];
extern u8 D_dryfield_motel_lobby_8017F65C[];
extern u8 D_dryfield_motel_lobby_8017F694[];
extern u8 D_dryfield_motel_lobby_8017F6C8[];
extern u8 D_dryfield_motel_lobby_8017F700[];

/// The "Play Data" panel's row list.
extern UiList D_dryfield_motel_lobby_8017F73C;

/// The usage panel's row list.
extern UiList D_dryfield_motel_lobby_8017F764;

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F788;

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F7A4;
extern UiObjectDesc D_dryfield_motel_lobby_8017F7C0;

static const char D_dryfield_motel_lobby_8017D638[];

/// The telephone menu's entry list.
extern UiList D_dryfield_motel_lobby_8017F7EC;

/// The room's message table, which `func_dryfield_motel_lobby_8017F44C`
/// installs on the room task.
extern GpMsgEntry D_dryfield_motel_lobby_8017F810[];

static void func_dryfield_motel_lobby_8017E218(UiList* list, UiObject* obj);
static void func_dryfield_motel_lobby_8017E514(UiList* list, UiObject* obj);
static void func_dryfield_motel_lobby_8017F3D0(Task* task);
static void func_dryfield_motel_lobby_8017F44C(Task* task);
static void func_dryfield_motel_lobby_8017F490(Task* task);

extern GpGridParams D_dryfield_motel_lobby_8017FBE4[1];
extern GpObj4C D_dryfield_motel_lobby_80180AEC[4];
extern GpObj4C D_dryfield_motel_lobby_80180C1C[7];
extern GpRoomCoordSet D_dryfield_motel_lobby_80181010[1];
s32 func_dryfield_motel_lobby_8017F40C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_lobby_8017F414(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_dryfield_motel_lobby_8017F43C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_lobby_8017F444(Task *, s32, GpMessageArg, GpMessageArg);
void func_dryfield_motel_lobby_8017D650(UiList *, UiObject *);
void func_dryfield_motel_lobby_8017DE1C(UiList *, UiObject *);
void func_dryfield_motel_lobby_8017E834(Task *);
void func_dryfield_motel_lobby_8017ECE0(Task *);
void func_dryfield_motel_lobby_8017EEA0(Task *);
void func_dryfield_motel_lobby_8017F094(UiList *, UiObject *);
void func_dryfield_motel_lobby_8017F178(UiList *, UiObject *);
void func_dryfield_motel_lobby_8017F240(UiList *, UiObject *);
void func_dryfield_motel_lobby_8017F308(UiList *, UiObject *);

u8 D_dryfield_motel_lobby_8017F4F0[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F4F8[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F504[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_dryfield_motel_lobby_8017F510[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_dryfield_motel_lobby_8017F518[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F520[4] = {
    87, 111, 110, 0,
};

u8 D_dryfield_motel_lobby_8017F524[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_dryfield_motel_lobby_8017F52C[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_dryfield_motel_lobby_8017F538[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F548[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F550[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_dryfield_motel_lobby_8017F558[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_dryfield_motel_lobby_8017F560[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F568[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F570[4] = {
    37, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F574[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F5A0[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F5C4[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F5F4[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F628[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F65C[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_dryfield_motel_lobby_8017F694[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_dryfield_motel_lobby_8017F6C8[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_dryfield_motel_lobby_8017F700[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_dryfield_motel_lobby_8017F738[1] = {
    func_dryfield_motel_lobby_8017D650,
};

UiList D_dryfield_motel_lobby_8017F73C = { D_dryfield_motel_lobby_8017F738, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_dryfield_motel_lobby_8017F760[1] = {
    func_dryfield_motel_lobby_8017DE1C,
};

UiList D_dryfield_motel_lobby_8017F764 = { D_dryfield_motel_lobby_8017F760, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_motel_lobby_8017F788 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_dryfield_motel_lobby_8017ECE0, 0 };

UiObjectDesc D_dryfield_motel_lobby_8017F7A4 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_dryfield_motel_lobby_8017EEA0, 0 };

UiObjectDesc D_dryfield_motel_lobby_8017F7C0 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_dryfield_motel_lobby_8017E834, 0 };

UiListItemFunc D_dryfield_motel_lobby_8017F7DC[4] = {
    func_dryfield_motel_lobby_8017F094,
    func_dryfield_motel_lobby_8017F178,
    func_dryfield_motel_lobby_8017F240,
    func_dryfield_motel_lobby_8017F308,
};

UiList D_dryfield_motel_lobby_8017F7EC = { D_dryfield_motel_lobby_8017F7DC, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

GpMsgEntry D_dryfield_motel_lobby_8017F810[5] = {
    { 5102, func_dryfield_motel_lobby_8017F414 },
    { 5105, func_dryfield_motel_lobby_8017F40C },
    { 5103, func_dryfield_motel_lobby_8017F444 },
    { 5104, func_dryfield_motel_lobby_8017F43C },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_dryfield_motel_lobby_8017F838[1] = {
    { D_dryfield_motel_lobby_8017FBE4, D_dryfield_motel_lobby_80180AEC, D_dryfield_motel_lobby_80180C1C, NULL },
};

u8 * D_dryfield_motel_lobby_8017F848[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_motel_lobby_8017F84C[1] = {
    { { .bytes = { 5, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_lobby_8017F850[1] = {
    { D_dryfield_motel_lobby_80181010, NULL },
};

GpWarpRec D_dryfield_motel_lobby_8017F858[1] = {
    { { .words = { 0, 2940, 0, 316 } }, { 0, 0, 0, 0 }, { .words = { 0, 2940, 0, 316 } }, { 0, 0, 0, 0 }, 0x52110002, 0x52110001, 0, 2, 0, 481 },
};

SVECTOR D_dryfield_motel_lobby_8017F890[9] = {
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 3803, 0, 1521, 0 },
    { -2896, 0, -2896, 0 },
    { 2896, 0, 2896, 0 },
};

SVECTOR D_dryfield_motel_lobby_8017F8D8[42] = {
    { 6000, -3000, 6000, 0 },
    { 6000, 0, 6000, 0 },
    { 0, 0, 6000, 0 },
    { 0, -3000, 6000, 0 },
    { 0, 0, 0, 0 },
    { 0, -3000, 0, 0 },
    { 6000, 0, 0, 0 },
    { 6000, -3000, 0, 0 },
    { 0, -950, 1500, 0 },
    { 0, -950, 6000, 0 },
    { 900, -950, 6000, 0 },
    { 900, -950, 1500, 0 },
    { 1500, -950, 0, 0 },
    { 0, -950, 0, 0 },
    { 900, 0, 6000, 0 },
    { 900, 0, 1500, 0 },
    { 1500, 0, 0, 0 },
    { 5450, -1800, 4400, 0 },
    { 6000, -1800, 4350, 0 },
    { 5500, -1800, 4350, 0 },
    { 5450, 0, 6000, 0 },
    { 5450, -1800, 6000, 0 },
    { 5450, 0, 4400, 0 },
    { 5500, 0, 4350, 0 },
    { 6000, 0, 4350, 0 },
    { 6000, -1800, 6000, 0 },
    { 4300, 0, 5000, 0 },
    { 4300, -1150, 5000, 0 },
    { 3700, -1150, 5000, 0 },
    { 3700, 0, 5000, 0 },
    { 3700, -1150, 2150, 0 },
    { 3700, 0, 2150, 0 },
    { 3900, -1150, 1950, 0 },
    { 3900, 0, 1950, 0 },
    { 6000, -1150, 1950, 0 },
    { 6000, 0, 1950, 0 },
    { 4300, -1150, 2700, 0 },
    { 6000, 0, 2550, 0 },
    { 6000, -1150, 2550, 0 },
    { 4450, -1150, 2550, 0 },
    { 4450, 0, 2550, 0 },
    { 4300, 0, 2700, 0 },
};

GpGridFace D_dryfield_motel_lobby_8017FA28[25] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
    { { 1, 6, 2, 4 }, 4, 1 },
    { { 3, 5, 0, 7 }, 5, 0 },
    { { 9, 10, 8, 11 }, 4, 0 },
    { { 12, 13, 11, 8 }, 4, 0 },
    { { 10, 14, 11, 15 }, 1, 0 },
    { { 16, 12, 15, 11 }, 6, 0 },
    { { 17, 18, 19, 0xFFFF }, 4, 0 },
    { { 21, 17, 20, 22 }, 3, 0 },
    { { 19, 18, 23, 24 }, 0, 0 },
    { { 23, 22, 19, 17 }, 7, 0 },
    { { 17, 21, 18, 25 }, 4, 0 },
    { { 27, 28, 26, 29 }, 2, 0 },
    { { 28, 30, 29, 31 }, 3, 0 },
    { { 32, 33, 30, 31 }, 7, 0 },
    { { 32, 34, 33, 35 }, 0, 0 },
    { { 30, 28, 36, 27 }, 4, 0 },
    { { 38, 39, 37, 40 }, 2, 0 },
    { { 39, 36, 40, 41 }, 8, 0 },
    { { 36, 27, 41, 26 }, 1, 0 },
    { { 36, 39, 30, 32 }, 4, 0 },
    { { 39, 38, 32, 34 }, 4, 0 },
};

s16 D_dryfield_motel_lobby_8017FB54[18] = {
    1,
    2,
    4,
    5,
    6,
    7,
    8,
    9,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    -1,
};

s16 D_dryfield_motel_lobby_8017FB78[11] = {
    0,
    1,
    4,
    5,
    6,
    8,
    15,
    16,
    19,
    22,
    -1,
};

s16 D_dryfield_motel_lobby_8017FB90[19] = {
    2,
    3,
    4,
    5,
    10,
    11,
    12,
    13,
    14,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    -1,
};

s16 D_dryfield_motel_lobby_8017FBB8[14] = {
    0,
    3,
    4,
    5,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    19,
    22,
    -1,
};

s16 * D_dryfield_motel_lobby_8017FBD4[4] = {
    D_dryfield_motel_lobby_8017FB54,
    D_dryfield_motel_lobby_8017FB78,
    D_dryfield_motel_lobby_8017FB90,
    D_dryfield_motel_lobby_8017FBB8,
};

GpGridParams D_dryfield_motel_lobby_8017FBE4[1] = {
    { NULL, D_dryfield_motel_lobby_8017F890, D_dryfield_motel_lobby_8017F8D8, D_dryfield_motel_lobby_8017FA28, D_dryfield_motel_lobby_8017FBD4, 0, 0, 2, 2, 4000, 25 },
};

GpViewRec D_dryfield_motel_lobby_8017FC08[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 0x61A8, -3000 } }, 680 },
    { { { { -3992, 0, -915 }, { -291, 3882, 1272 }, { 867, 1305, -3784 } }, { -1790, 2360, -5690 } }, 230 },
    { { { { 4037, 0, -691 }, { -204, 3912, -1195 }, { 660, 1212, 3856 } }, { -1790, 2360, -340 } }, 230 },
    { { { { 3142, 0, -2627 }, { -1618, 3226, -1936 }, { 2069, 2523, 2475 } }, { -3510, 2960, -1660 } }, 230 },
    { { { { -3394, 0, 2291 }, { 1829, 2466, 2710 }, { -1379, 3270, -2044 } }, { -4690, 1730, -2640 } }, 289 },
};

GpSprtCmd D_dryfield_motel_lobby_8017FCBC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_lobby_8017FCCC[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -24, 1089, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 1167, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -8, 1114, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 1114, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 1064, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 1023, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 950, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 1014, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1107, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -8, 1014, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 8, 1139, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, -16, 1129, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 0, 1146, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 16, 1179, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -32, 1079, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -16, 1090, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 0, 1163, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 16, 1175, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 24, 740, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 1187, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 1112, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1162, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 1175, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 0, 1075, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 24, 1087, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 48, 1087, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 1000, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 24, 1000, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 48, 1025, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 72, 1025, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 0, 920, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 64, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 88, 937, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 842, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 825, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 40, 837, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 64, 862, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 862, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 16, 763, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 32, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 48, 775, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 72, 775, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 775, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 32, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 725, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 725, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 725, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 876, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 32, 906, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 912, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_lobby_801800B4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 32, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_lobby_801800D4[52] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1103, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 0, 986, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 994, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1414, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 1310, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 1230, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 1150, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 687, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 650, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 1041, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 976, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 925, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 791, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 753, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 700, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -24, 1130, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 1237, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 1066, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -8, 1052, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1240, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 0, 1146, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 0, 1033, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 8, 1076, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 1000, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 8, 922, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 8, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, 16, 1008, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, 16, 946, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 875, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 1253, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 24, 1159, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 24, 889, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 104, 32, 846, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 40, 797, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 775, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 48, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 675, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 725, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 56, 700, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 64, 898, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 806, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 64, 662, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 891, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 675, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 96, 770, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -16, 1226, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 24, 825, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 24, 750, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 40, 725, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 678, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 32, 750, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 32, 700, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_lobby_801804E4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_lobby_801804FC[71] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -8, 837, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -8, 818, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 593, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 8, 887, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 8, 730, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 24, 670, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 750, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 612, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 620, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 574, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 558, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -24, 862, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -24, 837, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -24, 837, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 806, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 8, 738, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 24, 680, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 48, 860, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 806, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 8, 850, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 957, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 837, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 16, 708, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 32, 655, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 48, 602, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 72, 775, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 88, 781, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 782, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 32, 655, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 48, 609, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 64, 569, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 64, 606, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 72, 551, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 586, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 72, 551, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 88, 518, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 48, 609, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 64, 569, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 655, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 48, 609, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 72, 700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 40, 631, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 750, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 48, 942, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 962, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 72, 853, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 793, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 530, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 96, 503, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 80, 534, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 96, 503, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 16, 697, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 88, 518, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, 80, 534, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 716, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 48, 606, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 88, 518, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 722, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 32, 655, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 56, 825, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 80, 825, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 8, 725, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 800, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 825, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 8, 750, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 825, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 825, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 0, 887, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 900, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 16, 975, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_lobby_80180A88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_lobby_80180AA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_lobby_80180AB0[5] = {
    { { .empty = D_dryfield_motel_lobby_8017FCBC }, D_dryfield_motel_lobby_8017FCBC, NULL },
    { { .elements = D_dryfield_motel_lobby_8017FCCC }, D_dryfield_motel_lobby_801800B4, NULL },
    { { .elements = D_dryfield_motel_lobby_801800D4 }, D_dryfield_motel_lobby_801804E4, NULL },
    { { .elements = D_dryfield_motel_lobby_801804FC }, D_dryfield_motel_lobby_80180A88, NULL },
    { { .empty = D_dryfield_motel_lobby_80180AA0 }, D_dryfield_motel_lobby_80180AA0, NULL },
};

GpObj4C D_dryfield_motel_lobby_80180AEC[4] = {
    { NULL, NULL, NULL, { 2241, -896, 3011, 0 }, { { -1888, 1024, 0, 0 }, { 1888, 1024, 0, 0 }, { -1888, -1024, 0, 0 }, { 1888, -1024, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2144, -864, 3392, 0 }, { { 1888, 1024, 0, 0 }, { -1888, 1024, 0, 0 }, { 1888, -1024, 0, 0 }, { -1888, -1024, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4000, -896, 5264, 0 }, { { 0, 1024, 848, 0 }, { 0, 1024, -848, 0 }, { 0, -1024, 848, 0 }, { 0, -1024, -848, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 4095, -896, 5248, 0 }, { { 2, 1024, -848, 0 }, { -1, 1024, 848, 0 }, { 2, -1024, -848, 0 }, { -1, -1024, 848, 0 } }, { -4103, 0, -10, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 3, 4, 129, 0 },
};

GpObj4C D_dryfield_motel_lobby_80180C1C[7] = {
    { NULL, NULL, NULL, { 2992, -48, 320, 0 }, { { -848, 0, -256, 0 }, { 848, 0, -256, 0 }, { -848, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 884, 0, 15, 18, 2, 0 },
    { NULL, NULL, NULL, { 5872, -64, 3104, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 523, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 4656, -64, 2280, 0 }, { { -1200, 0, -744, 0 }, { 816, 0, -712, 0 }, { -432, 0, 728, 0 }, { 816, 0, 728, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1408, 2, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 4016, -64, 3456, 0 }, { { -816, 0, -640, 0 }, { 816, 0, -640, 0 }, { -816, 0, 640, 0 }, { 816, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1031, 2, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { 5808, -64, 976, 0 }, { { -320, 0, -880, 0 }, { 320, 0, -880, 0 }, { -320, 0, 880, 0 }, { 320, 0, 880, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 936, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 5312, -64, 4928, 0 }, { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 5888, -64, 3984, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 1, 0, 130, 0 },
};

GpPointLight D_dryfield_motel_lobby_80180E30[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -2500, 1200 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3768, 3768, 3522, { 0, 0 } }, 3000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -1610, 704 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1610, 704 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -1610, 704 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -2500, 5450 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2703, { 0, 0 } }, 3000, 6000 },
};

GpRoomCoordSet D_dryfield_motel_lobby_80181010[1] = {
    { 0, NULL, 5, D_dryfield_motel_lobby_80180E30, 0, NULL },
};

s32 D_dryfield_motel_lobby_80181028[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

GpRoomParamRec D_dryfield_motel_lobby_80181034[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_lobby_8018103C[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_lobby_80181028 },
};

GpRoomParamRec * D_dryfield_motel_lobby_80181044[8] = {
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_8018103C,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
};

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_dryfield_motel_lobby_8017D650(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_motel_lobby_8017F574,
                D_dryfield_motel_lobby_8017F5A0,
                D_dryfield_motel_lobby_8017F5C4,
                D_dryfield_motel_lobby_8017F5F4,
                D_dryfield_motel_lobby_8017F628,
                D_dryfield_motel_lobby_8017F65C,
                D_dryfield_motel_lobby_8017F694,
                D_dryfield_motel_lobby_8017F6C8,
                D_dryfield_motel_lobby_8017F700,
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F518);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F548);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F520);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F524);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F52C);
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
            Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F538);
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
            Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F550);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F558);
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
            Text_DrawString(&req, D_dryfield_motel_lobby_8017F560);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel `func_dryfield_motel_lobby_8017EEA0` draws.
static const char D_dryfield_motel_lobby_8017D610[] = "Play Data";

/// Text drawn in place of a row's percentage once it reaches 100 percent.
static const u8 D_dryfield_motel_lobby_8017D61C[] = "100.0%";

/// Draws one row of the item-usage panel: the item name and icon, its share of
/// all uses as a percentage with two decimals, and a gouraud bar sized from the
/// row's bar width. While the row is selected the item is previewed, and a
/// confirm press opens the item's description.
void func_dryfield_motel_lobby_8017DE1C(UiList* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_motel_lobby_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
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

/// Fills the weapon-usage panel's rows from the save's per-weapon use counters
/// (`Mc_SaveData[0].state.weaponUseCounts`, item ids 0x80-0x9F).
///
/// Every id with a non-empty name (a leading 0 or 0xA marks an unused row) and
/// a non-zero counter is marked seen and appended to `itemIds`, and the
/// counters are summed. The ids are insertion-sorted by use count, most-used
/// first. Each row then gets `percents`, its share of all uses in hundredths of
/// a percent, rounded, and `barWidths`, its counter as a 12-bit fraction of the
/// top row's. The top counter, the total and the scale are halved (and the
/// shift shortened) until the top counter fits in 17 bits, so neither the
/// multiply nor the shift overflows.
static void func_dryfield_motel_lobby_8017E218(UiList* list, UiObject* obj)
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
static void func_dryfield_motel_lobby_8017E514(UiList* list, UiObject* obj)
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

/// Titles of the usage panel `func_dryfield_motel_lobby_8017E834` opens:
/// "Weapon Data" and "PE Data".
static const char D_dryfield_motel_lobby_8017D624[] = "Weapon Data";
static const char D_dryfield_motel_lobby_8017D630[] = "PE Data";

void func_dryfield_motel_lobby_8017E834(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_dryfield_motel_lobby_8017F764;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_dryfield_motel_lobby_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_dryfield_motel_lobby_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F788, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_dryfield_motel_lobby_8017E218(list, obj);
        } else {
            func_dryfield_motel_lobby_8017E514(list, obj);
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

/// The "Telephone" title `func_dryfield_motel_lobby_8017E9E8` draws over its
/// menu once the menu is open. The two bytes after its terminator are not
/// zero, so it stays assembly.
/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_motel_lobby_8017D638[12] = "Telephone\0\xAD\x1A";

void func_dryfield_motel_lobby_8017E9E8(Task* task)
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
    list          = &D_dryfield_motel_lobby_8017F7EC;
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
        Ui_DrawText(&(obj)->panel, D_dryfield_motel_lobby_8017D638);
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

void func_dryfield_motel_lobby_8017ECE0(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_dryfield_motel_lobby_8017F3D0;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at most
/// the string's length) follow it, shifting them and the NUL one byte right.
/// Does nothing when `decimals` is not positive.
static void func_dryfield_motel_lobby_8017ED3C(u8* str, s32 decimals)
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

/// Formats `value`, a percentage scaled by 10^`decimals`, into `buf` and
/// returns `buf`: the integer is printed padded to `decimals + 1` digits when
/// it is below that scale, a '.' is inserted before its last `decimals` digits,
/// and "%" is appended.
static u8* func_dryfield_motel_lobby_8017EDAC(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_motel_lobby_8017F570);
    return buf;
}

void func_dryfield_motel_lobby_8017EEA0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_motel_lobby_8017F73C;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_dryfield_motel_lobby_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F788, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle into the current OT one slot past the panel's
/// draw order, at the panel origin (`field_20`, `field_22`) offset by (`arg1`,
/// `arg2`) and `arg3` by `arg4` in size. The left edge takes colour `arg5`, the
/// right edge `arg6`; nothing is drawn for a zero `arg5` or a width below 2.
static void func_dryfield_motel_lobby_8017EF90(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_dryfield_motel_lobby_8017F094(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F0, prompt->field_1C, 1, 0);
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

void func_dryfield_motel_lobby_8017F178(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7A4, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_dryfield_motel_lobby_8017F240(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F504, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_dryfield_motel_lobby_8017F308(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F510, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

static void func_dryfield_motel_lobby_8017F3D0(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

s32 func_dryfield_motel_lobby_8017F40C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Location-message handler of the room's message table: copies the requested
/// location onto the outgoing record and answers 1.
s32 func_dryfield_motel_lobby_8017F414(Task* task, s32 msgId, GpSaveLoc * src, GpSaveLoc * dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_motel_lobby_8017F43C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_motel_lobby_8017F444(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: installs the room's message table, stores the
/// task in session pointer slot 7 and advances.
static void func_dryfield_motel_lobby_8017F44C(Task* task)
{
    task->msgTable = D_dryfield_motel_lobby_8017F810;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room task: the room has nothing to update.
static void func_dryfield_motel_lobby_8017F490(Task* task)
{
}

/// The room task's three states: set-up, the (empty) per-frame state, and the
/// kill.
static const TaskFuncTable3 D_dryfield_motel_lobby_8017D644 = {
    {
        func_dryfield_motel_lobby_8017F44C,
        func_dryfield_motel_lobby_8017F490,
        taskKill,
    },
};

/// The room task's update: copies the state table and runs the current state.
void func_dryfield_motel_lobby_8017F498(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_lobby_8017D644;
    sp.funcs[task->state](task);
}
